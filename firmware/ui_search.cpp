// Ricerca (SPEC 5.10): campo con tastiera QWERTY a schermo, ricerche recenti in NVS,
// risultati in quattro schede da una sola richiesta GET /search.

#include <Arduino.h>
#include <Preferences.h>
#include "ui_common.h"
#include "ui_list.h"
#include "ui_views.h"

static const int MAX_CHARS = 64;
static const int RECENT_MAX = 4;
static const int FIELD_TEXT_W = 680 - 52 - 64;  // spazio per il testo nel campo (prima della ×)

// Tastiera (SPEC 4.18): 4 righe; le prime tre cambiano fra lettere e "123"
static const char *const LETTERS[3][10] = {
  { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" },
  { "a", "s", "d", "f", "g", "h", "j", "k", "l", "'" },
  { "z", "x", "c", "v", "b", "n", "m" },
};
static const char *const SYMBOLS[3][10] = {
  { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
  { "-", "/", ":", ";", "(", ")", "&", "@", "\"", "#" },
  { ".", ",", "?", "!", "+", "*", "%" },
};
static const int ROW_KEYS[3] = { 10, 10, 7 };
static const int ROW_X[3] = { 97, 97, 132 };
static const int ROW_Y[4] = { 224, 286, 348, 410 };

enum KeyId { KEY_BACKSPACE = 100, KEY_MODE, KEY_SPACE, KEY_SEARCH };

// Accenti con la pressione lunga sulle vocali
static const char *accents_for(const char *k)
{
  // Opzioni separate da \0, con un \0 in più alla fine
  if (!strcmp(k, "a")) return "\xC3\xA0\0";              // à
  if (!strcmp(k, "e")) return "\xC3\xA8\0\xC3\xA9\0";    // è é
  if (!strcmp(k, "i")) return "\xC3\xAC\0";              // ì
  if (!strcmp(k, "o")) return "\xC3\xB2\0";              // ò
  if (!strcmp(k, "u")) return "\xC3\xB9\0";              // ù
  return nullptr;
}

static int utf8_len(const char *s)
{
  int n = 0;
  for (; *s; s++)
    if ((*s & 0xC0) != 0x80) n++;
  return n;
}

class SearchView : public View {
  enum Mode { KEYBOARD, RESULTS };
  Mode mode = KEYBOARD;
  char text[MAX_CHARS * 2 + 1] = "";  // fino a 64 caratteri, anche accentati
  char query[MAX_CHARS * 2 + 1] = "";
  char recent[RECENT_MAX][MAX_CHARS * 2 + 1];
  bool symbols = false;

  // Tastiera
  lv_obj_t *field_text = nullptr, *field_hint = nullptr, *cursor = nullptr, *clear_btn = nullptr;
  lv_obj_t *keys[3][10] = {}, *key_labels[3][10] = {};
  lv_obj_t *mode_label = nullptr, *search_key = nullptr;
  lv_timer_t *blink = nullptr;
  lv_obj_t *popup = nullptr;  // riquadro degli accenti
  lv_obj_t *popup_keys[2] = {};
  int popup_n = 0, popup_hover = -1;

  // Risultati
  PagedList lists[4];
  int tab = 0;
  int built_tab = -1;  // elenco di cui esistono i widget (tab e mode cambiano prima della ricostruzione)
  uint32_t req = 0;
  bool searched = false, search_failed = false;
  lv_obj_t *chip_icon = nullptr, *chip_spinner = nullptr, *tab_empty = nullptr;

public:
  SearchView()
  {
    load_recent();
    static const char *const NOUNS[] = { "brani", "album", "artisti", "playlist" };
    for (int t = 0; t < 4; t++) {
      PagedList &l = lists[t];
      l.search_type = t;
      l.round_images = t == IT_ARTIST;
      l.track_menu = t == IT_TRACK;
      l.noun_plural = NOUNS[t];
      l.what = "i risultati";
      l.hide_end_note = true;
      l.fetch = [this, t](int offset, const char *) -> uint32_t {
        if (offset == 0) {  // prima pagina: una richiesta sola per tutte le schede
          start_request();
          return 0;
        }
        BrowseRequest r = {};
        r.kind = BR_SEARCH;
        strlcpy(r.arg, query, sizeof(r.arg));
        r.offset = offset;
        r.limit = 10;
        r.search_types = 1 << t;
        return spotify_browse(r);
      };
      l.on_click = [this, t](int i) { open_result(t, i); };
    }
  }
  ~SearchView() override { stop_blink(); }
  Section section() const override { return SEC_SEARCH; }

  void build(lv_obj_t *root) override
  {
    if (mode == KEYBOARD) build_keyboard(root);
    else build_results(root);
  }

  void destroyed() override
  {
    stop_blink();
    field_text = field_hint = cursor = clear_btn = nullptr;
    memset(keys, 0, sizeof(keys));
    memset(key_labels, 0, sizeof(key_labels));
    mode_label = search_key = nullptr;
    popup = nullptr;
    chip_icon = chip_spinner = tab_empty = nullptr;
    if (built_tab >= 0) lists[built_tab].destroyed();
    built_tab = -1;
  }

  void reselect() override
  {
    if (mode == RESULTS) {  // "Cerca" nella barra dai risultati: torna alla tastiera con il testo
      mode = KEYBOARD;
      nav_rebuild();
    }
  }

  bool back() override
  {
    if (popup) {
      close_popup();
      return true;
    }
    if (mode == RESULTS && lists[tab].menu_open()) {
      lists[tab].close_menu();
      return true;
    }
    return false;
  }

  void gesture(lv_dir_t d) override
  {
    if (mode == RESULTS) lists[tab].gesture(d);
  }

  void tick() override
  {
    BrowseResult r;
    if (req && spotify_take_result(req, &r)) {
      req = 0;
      searched = true;
      search_failed = r.code < 200 || r.code >= 300;
      for (int t = 0; t < 4; t++) lists[t].set_result(r);
      spotify_free(r.items);
      spotify_free(r.meta);
      // Scheda iniziale: Brani, oppure la prima che ha risultati
      if (!search_failed && lists[tab].store.count == 0) {
        for (int t = 0; t < 4; t++) {
          if (lists[t].store.count) {
            tab = t;
            break;
          }
        }
      }
      if (mode == RESULTS) nav_rebuild();
      return;
    }
    if (mode == RESULTS && built_tab == tab) {
      PagedList &l = lists[tab];
      l.tick();
      if (l.busy_index >= 0 && !nav_play_busy()) {
        l.busy_index = -1;
        l.refresh();
      }
      bool loading = req || l.loading();
      if (chip_spinner) {
        ui_set_hidden(chip_spinner, !loading);
        ui_set_hidden(chip_icon, loading);
      }
    }
  }

private:
  // ---------- Ricerche recenti (NVS) ----------

  void load_recent()
  {
    Preferences p;
    p.begin("ui", true);
    for (int i = 0; i < RECENT_MAX; i++) {
      char key[4] = { 'r', (char)('0' + i), 0 };
      String s = p.getString(key, "");
      strlcpy(recent[i], s.c_str(), sizeof(recent[i]));
    }
    p.end();
  }

  void save_recent(const char *q)
  {
    // In cima la più recente, senza doppioni (maiuscole/minuscole uguali)
    char list[RECENT_MAX][MAX_CHARS * 2 + 1];
    int n = 0;
    strlcpy(list[n++], q, sizeof(list[0]));
    for (int i = 0; i < RECENT_MAX && n < RECENT_MAX; i++)
      if (recent[i][0] && strcasecmp(recent[i], q) != 0) strlcpy(list[n++], recent[i], sizeof(list[0]));
    Preferences p;
    p.begin("ui", false);
    for (int i = 0; i < RECENT_MAX; i++) {
      strlcpy(recent[i], i < n ? list[i] : "", sizeof(recent[i]));
      char key[4] = { 'r', (char)('0' + i), 0 };
      p.putString(key, recent[i]);
    }
    p.end();
  }

  // ---------- Tastiera ----------

  lv_obj_t *make_key(lv_obj_t *root, int x, int y, int w, lv_color_t bg, lv_color_t pressed, int id)
  {
    lv_obj_t *k = ui_make_touch(root, CX(x), y, w, 54, pressed);
    lv_obj_set_style_radius(k, 8, 0);
    lv_obj_set_style_bg_color(k, bg, 0);
    lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
    lv_obj_set_user_data(k, (void *)(intptr_t)id);
    lv_obj_add_event_cb(k, key_event, LV_EVENT_ALL, this);
    return k;
  }

  void build_keyboard(lv_obj_t *root)
  {
    // Campo di ricerca
    lv_obj_t *field = ui_make_box(root, CX(104), 12, 680, 56, COL_SURFACE, 28);
    lv_obj_t *lens = ui_make_label(field, &font_ic_20, COL_TEXT_2, IC_SEARCH);
    lv_obj_set_pos(lens, 20, 18);
    field_hint = ui_make_label(field, &font_it_20, COL_TEXT_3, "Cerca brani, album, artisti");
    lv_obj_set_pos(field_hint, 52, 15);
    field_text = ui_make_label(field, &font_it_20, COL_TEXT, "");
    lv_obj_set_pos(field_text, 52, 15);
    cursor = ui_make_box(field, 52, 15, 2, 26, COL_ACCENT);
    clear_btn = ui_make_touch(root, CX(724), 12, 56, 56, COL_BG, false);
    lv_obj_t *c = ui_make_box(clear_btn, 12, 12, 32, 32, COL_RAISED, LV_RADIUS_CIRCLE);
    lv_obj_t *x = ui_make_label(c, &font_ic_20, COL_TEXT, IC_CLOSE);
    lv_obj_center(x);
    lv_obj_add_event_cb(clear_btn, [](lv_event_t *e) {
      SearchView *v = (SearchView *)lv_event_get_user_data(e);
      v->text[0] = '\0';
      v->update_field();
    }, LV_EVENT_CLICKED, this);

    // Ricerche recenti, o il suggerimento se non ce ne sono
    if (recent[0][0]) {
      lv_obj_t *l = ui_make_label(root, &font_it_16, COL_TEXT_3, "Ricerche recenti");
      lv_obj_set_pos(l, CX(112), 92);
      int cx = 112;
      for (int i = 0; i < RECENT_MAX && recent[i][0]; i++) {
        lv_point_t sz;
        lv_txt_get_size(&sz, recent[i], &font_it_20, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        int w = min((int)sz.x + 40, 300);
        if (cx + w > 784) break;
        lv_obj_t *chip = ui_make_touch(root, CX(cx), 122, w, 48, COL_RAISED);
        lv_obj_set_style_radius(chip, 24, 0);
        lv_obj_set_style_bg_color(chip, COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_t *t = ui_make_line_label(chip, &font_it_20, COL_TEXT, 20, 11, w - 40);
        ui_set_text_raw(t, recent[i]);
        lv_obj_set_user_data(chip, (void *)(intptr_t)i);
        lv_obj_add_event_cb(chip, [](lv_event_t *e) {
          SearchView *v = (SearchView *)lv_event_get_user_data(e);
          int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
          strlcpy(v->text, v->recent[i], sizeof(v->text));
          v->run_search();
        }, LV_EVENT_CLICKED, this);
        cx += w + 12;
      }
    } else {
      lv_obj_t *l = ui_make_label(root, &font_it_16, COL_TEXT_3,
                                  "Scrivi il nome di un brano, un album, un artista o una playlist.");
      lv_obj_set_pos(l, CX(112), 92);
    }

    // Tasti (SPEC 4.18)
    ui_make_box(root, 0, 208, 712, 272, COL_RAIL);
    for (int r = 0; r < 3; r++) {
      for (int k = 0; k < ROW_KEYS[r]; k++) {
        keys[r][k] = make_key(root, ROW_X[r] + k * 70, ROW_Y[r], 64, COL_PRESSED, COL_KEY_PRESSED, r * 10 + k);
        key_labels[r][k] = ui_make_label(keys[r][k], &font_it_24, COL_TEXT, "");
        lv_obj_center(key_labels[r][k]);
      }
    }
    lv_obj_t *bs = make_key(root, 132 + 7 * 70, ROW_Y[2], 169, COL_RAISED, COL_KEY_PRESSED, KEY_BACKSPACE);
    lv_obj_center(ui_make_label(bs, &font_ic_28, COL_TEXT, IC_BACKSPACE));
    lv_obj_t *m = make_key(root, 97, ROW_Y[3], 134, COL_RAISED, COL_KEY_PRESSED, KEY_MODE);
    mode_label = ui_make_label(m, &font_it_20, COL_TEXT, "");
    lv_obj_center(mode_label);
    lv_obj_t *sp = make_key(root, 237, ROW_Y[3], 344, COL_PRESSED, COL_KEY_PRESSED, KEY_SPACE);
    lv_obj_center(ui_make_label(sp, &font_it_16, COL_TEXT_2, "spazio"));
    search_key = make_key(root, 587, ROW_Y[3], 204, COL_ACCENT, COL_ACCENT_PRESSED, KEY_SEARCH);
    lv_obj_set_style_bg_color(search_key, COL_PRESSED, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(search_key, COL_ON_ACCENT, 0);
    lv_obj_set_style_text_color(search_key, COL_DISABLED_FG, LV_STATE_DISABLED);
    lv_obj_t *sl = lv_label_create(search_key);
    lv_obj_set_style_text_font(sl, &font_it_sb20, 0);
    lv_label_set_text(sl, "Cerca");
    lv_obj_center(sl);

    update_key_labels();
    update_field();
    blink = lv_timer_create([](lv_timer_t *t) {
      SearchView *v = (SearchView *)t->user_data;
      if (v->cursor) ui_set_hidden(v->cursor, !lv_obj_has_flag(v->cursor, LV_OBJ_FLAG_HIDDEN));
    }, 500, this);
  }

  void stop_blink()
  {
    if (blink) {
      lv_timer_del(blink);
      blink = nullptr;
    }
  }

  void update_key_labels()
  {
    for (int r = 0; r < 3; r++)
      for (int k = 0; k < ROW_KEYS[r]; k++) ui_set_text_raw(key_labels[r][k], symbols ? SYMBOLS[r][k] : LETTERS[r][k]);
    ui_set_text_raw(mode_label, symbols ? "abc" : "123");
  }

  void update_field()
  {
    if (!field_text) return;
    // Testo più lungo del campo: si vede la parte finale, preceduta da "…"
    const char *shown = text;
    char buf[sizeof(text) + 4];
    while (lv_txt_get_width(shown, strlen(shown), &font_it_20, 0, LV_TEXT_FLAG_NONE) > FIELD_TEXT_W - 20 && *shown) {
      shown++;
      while ((*shown & 0xC0) == 0x80) shown++;
    }
    if (shown != text) {
      snprintf(buf, sizeof(buf), "\xE2\x80\xA6%s", shown);
      shown = buf;
    }
    ui_set_text_raw(field_text, shown);
    ui_set_hidden(field_hint, text[0] != '\0');
    ui_set_hidden(clear_btn, text[0] == '\0');
    int w = lv_txt_get_width(shown, strlen(shown), &font_it_20, 0, LV_TEXT_FLAG_NONE);
    lv_obj_set_x(cursor, 52 + w + 1);
    ui_set_hidden(cursor, false);
    bool has_text = false;
    for (const char *p = text; *p; p++) has_text |= *p != ' ';
    ui_button_set_enabled(search_key, has_text);
  }

  void insert(const char *s)
  {
    if (utf8_len(text) >= MAX_CHARS) return;
    strlcat(text, s, sizeof(text));
    update_field();
  }

  void backspace()
  {
    int n = strlen(text);
    if (!n) return;
    n--;
    while (n > 0 && (text[n] & 0xC0) == 0x80) n--;
    text[n] = '\0';
    update_field();
  }

  // ---------- Accenti ----------

  void open_popup(lv_obj_t *key, const char *options)
  {
    close_popup();
    popup_n = 0;
    const char *opts[2];
    for (const char *o = options; *o && popup_n < 2; o += strlen(o) + 1) opts[popup_n++] = o;
    lv_obj_t *root = lv_obj_get_parent(key);
    // Strato che chiude il riquadro toccando fuori
    popup = lv_obj_create(root);
    lv_obj_remove_style_all(popup);
    lv_obj_set_size(popup, 712, 480);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(popup, [](lv_event_t *e) { ((SearchView *)lv_event_get_user_data(e))->close_popup(); },
                        LV_EVENT_CLICKED, this);
    int w = popup_n * 64 + (popup_n - 1) * 6 + 12;
    int x = constrain((int)lv_obj_get_x(key) - 6, 0, 712 - w);
    lv_obj_t *box = ui_make_box(popup, x, lv_obj_get_y(key) - 66, w, 66, COL_RAISED, 12);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < popup_n; i++) {
      lv_obj_t *k = ui_make_touch(box, 6 + i * 70, 6, 64, 54, COL_KEY_PRESSED);
      lv_obj_set_style_radius(k, 8, 0);
      lv_obj_set_style_bg_color(k, COL_PRESSED, 0);
      lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(k, COL_KEY_PRESSED, LV_STATE_CHECKED);
      lv_obj_t *l = ui_make_label(k, &font_it_24, COL_TEXT, opts[i]);
      lv_obj_center(l);
      lv_obj_set_user_data(k, (void *)opts[i]);
      lv_obj_add_event_cb(k, [](lv_event_t *e) {
        SearchView *v = (SearchView *)lv_event_get_user_data(e);
        const char *s = (const char *)lv_obj_get_user_data(lv_event_get_current_target(e));
        v->insert(s);
        v->close_popup();
      }, LV_EVENT_CLICKED, this);
      popup_keys[i] = k;
    }
    popup_hover = -1;
  }

  void close_popup()
  {
    if (!popup) return;
    lv_obj_del_async(popup);
    popup = nullptr;
    popup_n = 0;
  }

  // Dito trascinato sul riquadro dopo la pressione lunga: evidenzia l'accento sotto il dito
  int popup_option_at(lv_point_t p)
  {
    for (int i = 0; i < popup_n; i++) {
      lv_area_t a;
      lv_obj_get_coords(popup_keys[i], &a);
      if (p.x >= a.x1 && p.x <= a.x2 && p.y >= a.y1 - 10 && p.y <= a.y2 + 10) return i;
    }
    return -1;
  }

  static void key_event(lv_event_t *e)
  {
    SearchView *v = (SearchView *)lv_event_get_user_data(e);
    lv_obj_t *k = lv_event_get_current_target(e);
    int id = (intptr_t)lv_obj_get_user_data(k);
    lv_event_code_t code = lv_event_get_code(e);
    const char *ch = id < 30 ? (v->symbols ? SYMBOLS[id / 10][id % 10] : LETTERS[id / 10][id % 10]) : nullptr;

    if (id == KEY_BACKSPACE) {
      // ⌫ tenuto premuto ripete ogni 100 ms dopo 500 ms
      if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED || code == LV_EVENT_LONG_PRESSED_REPEAT) v->backspace();
      return;
    }
    if (ch && code == LV_EVENT_LONG_PRESSED && !v->symbols) {
      const char *acc = accents_for(ch);
      if (acc) v->open_popup(k, acc);
      return;
    }
    if (v->popup && v->popup_n && (code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED)) {
      lv_point_t p;
      lv_indev_get_point(lv_indev_get_act(), &p);
      int o = v->popup_option_at(p);
      if (code == LV_EVENT_PRESSING) {
        if (o != v->popup_hover) {
          for (int i = 0; i < v->popup_n; i++) {
            if (i == o) lv_obj_add_state(v->popup_keys[i], LV_STATE_CHECKED);
            else lv_obj_clear_state(v->popup_keys[i], LV_STATE_CHECKED);
          }
          v->popup_hover = o;
        }
      } else if (o >= 0) {
        v->insert((const char *)lv_obj_get_user_data(v->popup_keys[o]));
        v->close_popup();
      }
      return;
    }
    if (code != LV_EVENT_SHORT_CLICKED) return;
    if (ch) v->insert(ch);
    else if (id == KEY_SPACE) {
      if (v->text[0] && v->text[strlen(v->text) - 1] != ' ') v->insert(" ");
    } else if (id == KEY_MODE) {
      v->symbols = !v->symbols;
      v->update_key_labels();
    } else if (id == KEY_SEARCH && !lv_obj_has_state(k, LV_STATE_DISABLED)) {
      v->run_search();
    }
  }

  // ---------- Ricerca ----------

  void run_search()
  {
    // Spazi in testa e in coda tolti
    char *s = text;
    while (*s == ' ') s++;
    memmove(text, s, strlen(s) + 1);
    int n = strlen(text);
    while (n > 0 && text[n - 1] == ' ') text[--n] = '\0';
    if (!text[0]) return;
    strlcpy(query, text, sizeof(query));
    save_recent(query);
    for (auto &l : lists) {
      l.store.clear();
      l.page = 0;
    }
    tab = 0;
    searched = search_failed = false;
    req = 0;
    start_request();
    mode = RESULTS;
    nav_rebuild();
  }

  void start_request()
  {
    if (req || !query[0]) return;
    BrowseRequest r = {};
    r.kind = BR_SEARCH;
    strlcpy(r.arg, query, sizeof(r.arg));
    r.limit = 10;  // massimo 10 nel 2026
    r.search_types = 0x0F;
    req = spotify_browse(r);
  }

  void build_results(lv_obj_t *root)
  {
    // Chip con il testo cercato: riporta alla tastiera
    lv_obj_t *chip = ui_make_touch(root, CX(104), 8, 200, 48, COL_RAISED);
    lv_obj_set_style_radius(chip, 24, 0);
    lv_obj_set_style_bg_color(chip, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    chip_icon = ui_make_label(chip, &font_ic_20, COL_TEXT_2, IC_SEARCH);
    lv_obj_set_pos(chip_icon, 20, 14);
    chip_spinner = ui_make_spinner(chip, 20);
    lv_obj_set_pos(chip_spinner, 20, 14);
    lv_obj_t *t = ui_make_line_label(chip, &font_it_20, COL_TEXT, 48, 11, 136);
    ui_set_text_raw(t, query);
    lv_obj_add_event_cb(chip, [](lv_event_t *e) {
      SearchView *v = (SearchView *)lv_event_get_user_data(e);
      v->mode = KEYBOARD;
      nav_rebuild();
    }, LV_EVENT_CLICKED, this);

    bool all_empty = searched && !search_failed;
    for (auto &l : lists) all_empty &= l.store.count == 0 && !l.store.more;
    if (all_empty) {
      char title[120];
      snprintf(title, sizeof(title), "Nessun risultato per \xC2\xAB%s\xC2\xBB", query);
      ui_state_block(root, 0, 712, IC_SEARCH, COL_TEXT_2, title, "Controlla come l'hai scritto o prova con meno parole.",
                     "Modifica la ricerca", [](lv_event_t *e) {
                       SearchView *v = (SearchView *)lv_event_get_user_data(e);
                       v->mode = KEYBOARD;
                       nav_rebuild();
                     }, this);
      tick();
      return;
    }

    // Schede Brani / Album / Artisti / Playlist (SPEC 4.10)
    static const char *const TABS[] = { "Brani", "Album", "Artisti", "Playlist" };
    for (int i = 0; i < 4; i++) {
      lv_obj_t *tt = ui_make_touch(root, CX(320 + i * 104), 0, 104, 64, COL_BG, false);
      lv_obj_t *l = ui_make_label(tt, &font_it_sb20, i == tab ? COL_TEXT : COL_TEXT_3, TABS[i]);
      lv_obj_center(l);
      if (i == tab) ui_make_box(tt, 24, 55, 56, 3, COL_ACCENT);
      lv_obj_set_user_data(tt, (void *)(intptr_t)i);
      lv_obj_add_event_cb(tt, [](lv_event_t *e) {
        SearchView *v = (SearchView *)lv_event_get_user_data(e);
        int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
        if (i == v->tab) return;
        v->tab = i;
        nav_rebuild();
      }, LV_EVENT_CLICKED, this);
    }
    lists[tab].build(root);
    built_tab = tab;
    lists[tab].refresh();
    // Scheda senza risultati: solo una riga di testo al centro
    if (searched && !search_failed && lists[tab].store.count == 0) {
      static const char *const EMPTY[] = { "Nessun brano", "Nessun album", "Nessun artista", "Nessuna playlist" };
      tab_empty = ui_make_label(root, &font_it_16, COL_TEXT_3, EMPTY[tab]);
      lv_obj_align(tab_empty, LV_ALIGN_TOP_MID, 0, 253);
    }
    tick();
  }

  void open_result(int t, int i)
  {
    const BrowseItem &it = lists[t].store.items[i];
    switch (t) {
      case IT_TRACK: {
        if (nav_play_busy() || !it.uri[0]) return;
        // Il brano parte dentro il suo album, così poi continua con i brani successivi
        char body[200];
        if (it.album_uri[0]) snprintf(body, sizeof(body), "{\"context_uri\":\"%s\",\"offset\":{\"uri\":\"%s\"}}", it.album_uri, it.uri);
        else snprintf(body, sizeof(body), "{\"uris\":[\"%s\"]}", it.uri);
        lists[t].busy_index = i;
        lists[t].refresh();
        nav_play(body, it.title);
        break;
      }
      case IT_ALBUM: nav_push(view_album(it.uri, &it)); break;
      case IT_ARTIST: nav_push(view_artist(it, true)); break;
      default: nav_push(view_playlist(it)); break;
    }
  }
};

View *view_search() { return new SearchView(); }
