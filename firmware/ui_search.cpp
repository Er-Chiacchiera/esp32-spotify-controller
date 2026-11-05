// Ricerca (SPEC 5.10): campo con tastiera QWERTY a schermo, ricerche recenti in NVS,
// risultati in quattro schede da una sola richiesta GET /search.

#include <Arduino.h>
#include <Preferences.h>
#include "ui_common.h"
#include "ui_keyboard.h"
#include "ui_list.h"
#include "ui_views.h"

static const int MAX_CHARS = 64;
static const int RECENT_MAX = 4;
class SearchView : public View {
  enum Mode { KEYBOARD, RESULTS };
  Mode mode = KEYBOARD;
  Keyboard kb;
  char query[MAX_CHARS * 2 + 1] = "";
  char recent[RECENT_MAX][MAX_CHARS * 2 + 1];

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
    kb.on_search = [this] { run_search(); };
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
  Section section() const override { return SEC_SEARCH; }

  void build(lv_obj_t *root) override
  {
    if (mode == KEYBOARD) build_keyboard(root);
    else build_results(root);
  }

  void destroyed() override
  {
    kb.destroyed();
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
    if (kb.back()) return true;
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

  void build_keyboard(lv_obj_t *root)
  {
    kb.build(root, 104, 680, "Cerca brani, album, artisti");
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
          strlcpy(v->kb.text, v->recent[i], sizeof(v->kb.text));
          v->run_search();
        }, LV_EVENT_CLICKED, this);
        cx += w + 12;
      }
    } else {
      lv_obj_t *l = ui_make_label(root, &font_it_16, COL_TEXT_3,
                                  "Scrivi il nome di un brano, un album, un artista o una playlist.");
      lv_obj_set_pos(l, CX(112), 92);
    }
  }

  // ---------- Ricerca ----------

  void run_search()
  {
    kb.trim();
    if (!kb.text[0]) return;
    strlcpy(query, kb.text, sizeof(query));
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
