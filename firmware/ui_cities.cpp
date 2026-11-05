// Città del meteo (design/citta/SPEC_CITTA.md): elenco con la Posizione attuale e le città salvate (§4),
// Cerca una città e i risultati (§5, §6). Scegliere una città apre il suo meteo nella pagina a schede (§2).
#include "ui_cities.h"

#include <Arduino.h>
#include "ui_common.h"
#include "ui_keyboard.h"
#include "ui_list.h"
#include "ui_weather.h"
#include "weather.h"

static const int ROWS = 5;

static int text_w(const lv_font_t *font, const char *text)
{
  return lv_txt_get_width(text, strlen(text), font, 0, LV_TEXT_FLAG_NONE);
}

// Colonna pagine a 736,64 (SPEC 6 §4.5)
struct Pager {
  lv_obj_t *up = nullptr, *down = nullptr;
  int *page = nullptr;

  static lv_obj_t *button(lv_obj_t *root, int y, const char *icon, bool on)
  {
    lv_obj_t *t = ui_make_touch(root, CX(736), y, 64, 80, COL_BG, false);
    lv_obj_t *c = ui_make_box(t, 8, 16, 48, 48, on ? COL_SURFACE : COL_DISABLED_BG, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_color(c, COL_RAISED, LV_STATE_PRESSED);
    lv_obj_center(ui_make_label(c, &font_ic_28, on ? COL_TEXT : COL_DISABLED_FG_2, icon));
    if (!on) lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);
    return t;
  }

  // Nessuna colonna se basta una pagina
  void build(lv_obj_t *root, int *page_var, int pages)
  {
    page = page_var;
    if (pages <= 1) return;
    up = button(root, 64, IC_UP, *page > 0);
    down = button(root, 64 + ROWS * 80 - 80, IC_DOWN, *page + 1 < pages);
    lv_obj_add_event_cb(up, [](lv_event_t *e) {
      (*(int *)lv_event_get_user_data(e))--;
      nav_rebuild();
    }, LV_EVENT_CLICKED, page);
    lv_obj_add_event_cb(down, [](lv_event_t *e) {
      (*(int *)lv_event_get_user_data(e))++;
      nav_rebuild();
    }, LV_EVENT_CLICKED, page);
    lv_obj_t *counter = ui_make_label(root, &font_it_16, COL_TEXT_3, "");
    ui_set_text(counter, "%d/%d", *page + 1, pages);
    lv_obj_set_width(counter, 64);
    lv_obj_set_style_text_align(counter, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(counter, CX(736), 64 + ROWS * 40 - 11);
  }
};

// Riga 648x80 con il cerchio dell'icona, nome e testo sotto (SPEC 6 §4.3); restituisce l'area di tocco
static lv_obj_t *make_row(lv_obj_t *root, int k, int touch_w, const char *icon, lv_color_t icon_color, const char *name,
                          const char *sub, int text_w_max, bool touchable)
{
  int y = 64 + k * 80;
  lv_obj_t *row = ui_make_touch(root, 0, y, touch_w, 80, COL_PRESSED, touchable);
  if (!touchable) lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *circle = ui_make_box(row, 16, 12, 56, 56, COL_SURFACE, LV_RADIUS_CIRCLE);
  lv_obj_center(ui_make_label(circle, &font_ic_28, icon_color, icon));
  ui_set_text_raw(ui_make_line_label(row, &font_it_sb20, COL_TEXT, 96, 15, text_w_max), name);
  ui_set_text_raw(ui_make_line_label(row, &font_it_16, COL_TEXT_2, 96, 43, text_w_max), sub);
  return row;
}

// Dove porta la scelta di una città (§2): nella pagina a schede da cui si è venuti, oppure in una nuova
// sopra l'elenco. La predefinita non è "al volo".
static void open_city(bool from_weather, int back_depth, const WeatherPlace *p)
{
  weather_place_set(p);
  if (from_weather) nav_pop_to(back_depth);
  else nav_pop_to(back_depth + 1, view_weather(WT_WEATHER));
}

static void place_of(const City &c, WeatherPlace *p)
{
  p->on = true;
  p->lat = c.lat;
  p->lon = c.lon;
  strlcpy(p->name, c.name, sizeof(p->name));
}

// ---------- Città del meteo (§4) ----------

class CitiesView : public View {
  bool from_weather;
  int back_depth;  // livello della pila da cui si è aperto l'elenco
  bool editing = false;
  int page = 0;
  uint32_t built_version = 0;
  AutoState built_auto = AUTO_UNKNOWN;
  Pager pager;

  struct RowRef {
    CitiesView *v;
    int i;  // -1 Posizione attuale
  } refs[ROWS];

  // "Lombardia, Italia"; con due città dello stesso nome e regione la provincia al posto della regione
  static void city_sub(const City &c, char *out, size_t n)
  {
    const char *region = c.admin1;
    for (int k = 0; k < cities_count(); k++) {
      City o;
      cities_get(k, &o);
      if (o.id != c.id && !strcmp(o.name, c.name) && !strcmp(o.admin1, c.admin1) && c.admin2[0]) region = c.admin2;
    }
    if (region[0] && c.country[0]) snprintf(out, n, "%s, %s", region, c.country);
    else strlcpy(out, region[0] ? region : c.country, n);
  }

  void build_header(lv_obj_t *root)
  {
    ui_make_back_button(root);
    lv_obj_t *title = ui_make_line_label(root, &font_it_sb24, COL_TEXT, CX(156), 17, 260);
    ui_set_text_raw(title, "Citt\xC3\xA0 del meteo");
    if (editing) {
      ui_make_button(root, BTN_PRIMARY, CX(584), 8, 152, 48, nullptr, "Fine", [](lv_event_t *e) {
        CitiesView *v = (CitiesView *)lv_event_get_user_data(e);
        v->editing = false;
        nav_rebuild();
      }, this);
      return;
    }
    if (cities_count() > 0) {
      lv_obj_t *edit = ui_make_button(root, BTN_SECONDARY, CX(432), 8, 136, 48, nullptr, "Modifica", [](lv_event_t *e) {
        CitiesView *v = (CitiesView *)lv_event_get_user_data(e);
        v->editing = true;
        nav_rebuild();
      }, this);
      lv_obj_set_style_border_color(edit, COL_PLACEHOLDER_FG, 0);
    }
    lv_obj_t *add = ui_make_button(root, BTN_PRIMARY, CX(584), 8, 152, 48, IC_PLUS, "Aggiungi", [](lv_event_t *e) {
      CitiesView *v = (CitiesView *)lv_event_get_user_data(e);
      nav_push(view_city_search(v->from_weather, v->back_depth));
    }, this);
    ui_button_set_enabled(add, cities_count() < CITY_MAX);
  }

  // Icona del tempo + temperatura, allineate a destra a x 568 della riga
  static void now_group(lv_obj_t *row, const CityNow &n)
  {
    char t[8];
    snprintf(t, sizeof(t), "%d\xC2\xB0", n.temp);
    const char *ic = weather_icon(n.code, n.is_day);
    int x = 568 - text_w(&font_it_20, t);
    lv_obj_set_pos(ui_make_label(row, &font_it_20, COL_TEXT, t), x, 28);
    lv_obj_set_pos(ui_make_label(row, &font_ic_28, COL_TEXT, ic), x - 8 - text_w(&font_ic_28, ic), 26);
  }

  void build_row(lv_obj_t *root, int k, int i, const AutoLocation &al)
  {
    char name[64], sub[120], place[100];
    bool is_def = cities_default() == i;
    const char *icon = IC_MARKER;
    lv_color_t icon_color = COL_TEXT_2;
    bool touchable = !editing;
    if (i < 0) {
      icon = IC_LOCATION;
      strlcpy(name, "Posizione attuale", sizeof(name));
      if (al.state == AUTO_OK) snprintf(place, sizeof(place), "%s, stimata dalla rete", al.city);
      else if (al.state == AUTO_FAILED) {
        // Dopo un errore weather.cpp riprova ogni 2 minuti: "tra poco"
        strlcpy(place, "Non trovata \xC2\xB7 riprovo tra poco", sizeof(place));
        icon_color = COL_DISABLED_FG;
        touchable = false;
      } else {
        strlcpy(place, "Cerco la posizione\xE2\x80\xA6", sizeof(place));
        touchable = false;
      }
    } else {
      City c;
      cities_get(i, &c);
      strlcpy(name, c.name, sizeof(name));
      city_sub(c, place, sizeof(place));
    }
    if (is_def) snprintf(sub, sizeof(sub), "Predefinita \xC2\xB7 %s", place);
    else strlcpy(sub, place, sizeof(sub));

    lv_obj_t *row = make_row(root, k, 576, icon, icon_color, name, sub, 368, touchable);
    refs[k] = { this, i };
    if (touchable) {
      lv_obj_add_event_cb(row, [](lv_event_t *e) {
        RowRef *r = (RowRef *)lv_event_get_user_data(e);
        r->v->choose(r->i);
      }, LV_EVENT_CLICKED, &refs[k]);
    }
    if (editing) {
      if (i < 0) return;
      // Cestino: cerchio 48 RAISED sempre visibile, premuto KEY_PRESSED; tocco esteso a 64x64
      lv_obj_t *trash = ui_make_touch(root, 576 + 8, 64 + k * 80 + 16, 48, 48, COL_KEY_PRESSED);
      lv_obj_set_style_radius(trash, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(trash, COL_RAISED, 0);
      lv_obj_set_style_bg_opa(trash, LV_OPA_COVER, 0);
      lv_obj_set_ext_click_area(trash, 8);
      lv_obj_center(ui_make_label(trash, &font_ic_28, COL_TEXT, IC_TRASH));
      lv_obj_add_event_cb(trash, [](lv_event_t *e) {
        RowRef *r = (RowRef *)lv_event_get_user_data(e);
        r->v->remove(r->i);
      }, LV_EVENT_CLICKED, &refs[k]);
      return;
    }
    // Riga 648 di larghezza: la stella sta fuori dall'area di tocco (576), nella stessa riga dello schermo
    CityNow n;
    if (cities_now_get(i, &n) && (i >= 0 || al.state == AUTO_OK)) now_group(row, n);
    lv_obj_t *icon_obj;
    lv_obj_t *star = ui_make_icon_button(root, 576, 64 + k * 80 + 8, IC_STAR, is_def ? COL_ACCENT : COL_PLACEHOLDER_FG,
                                         [](lv_event_t *e) {
                                           RowRef *r = (RowRef *)lv_event_get_user_data(e);
                                           r->v->make_default(r->i);
                                         }, &refs[k], &icon_obj);
    if (is_def) lv_obj_clear_flag(star, LV_OBJ_FLAG_CLICKABLE);
  }

  void choose(int i)
  {
    if (i == cities_default()) {
      open_city(from_weather, back_depth, nullptr);
      return;
    }
    WeatherPlace p = {};
    if (i < 0) {
      AutoLocation al;
      weather_auto_get(&al);
      if (al.state != AUTO_OK) return;
      p.on = true;
      p.lat = al.lat;
      p.lon = al.lon;
      strlcpy(p.name, al.city, sizeof(p.name));
    } else {
      City c;
      if (!cities_get(i, &c)) return;
      place_of(c, &p);
    }
    open_city(from_weather, back_depth, &p);
  }

  void make_default(int i)
  {
    cities_set_default(i);
    nav_rebuild();
  }

  void remove(int i)
  {
    City c;
    if (!cities_get(i, &c)) return;
    bool was_default = i == cities_default();
    cities_remove(i);
    char msg[120];
    if (was_default) snprintf(msg, sizeof(msg), "%s tolta. Ora vale la posizione attuale", c.name);
    else snprintf(msg, sizeof(msg), "%s tolta dall'elenco", c.name);
    nav_toast(msg);
    if (cities_count() == 0) editing = false;
    nav_rebuild();
  }

public:
  CitiesView(bool from_weather, int back_depth) : from_weather(from_weather), back_depth(back_depth) {}
  Section section() const override { return SEC_NONE; }
  bool back_goes_home() const override { return true; }

  bool back() override
  {
    if (!editing) return false;
    editing = false;
    nav_rebuild();
    return true;
  }

  void build(lv_obj_t *root) override
  {
    weather_auto_request();
    cities_now_request();
    AutoLocation al;
    weather_auto_get(&al);
    built_auto = al.state;
    built_version = cities_version();
    int total = 1 + cities_count();
    int pages = (total + ROWS - 1) / ROWS;
    page = constrain(page, 0, pages - 1);
    build_header(root);
    int first = page * ROWS, shown = min(ROWS, total - first);
    for (int k = 0; k < shown; k++) build_row(root, k, first + k - 1, al);
    pager.build(root, &page, pages);
    // Elenco pieno: nota sotto l'ultima riga dell'ultima pagina (con 9 righe ne ha 4)
    if (cities_count() >= CITY_MAX && !editing && page == pages - 1 && shown < ROWS) {
      lv_obj_t *note = ui_make_label(root, &font_it_16, COL_TEXT_3,
                                     "Hai salvato 8 citt\xC3\xA0, il massimo. Per aggiungerne un'altra togline una con "
                                     "Modifica.");
      lv_obj_set_width(note, 600);
      lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
      lv_obj_set_pos(note, CX(112), 64 + shown * 80 + 20);
    }
  }

  void gesture(lv_dir_t d) override
  {
    int pages = (1 + cities_count() + ROWS - 1) / ROWS;
    if (d == LV_DIR_TOP && page + 1 < pages) page++;
    else if (d == LV_DIR_BOTTOM && page > 0) page--;
    else return;
    nav_rebuild();
  }

  void tick() override
  {
    // Temperature arrivate, posizione stimata: si ridisegna (pagina ferma, costa poco)
    AutoLocation al;
    weather_auto_get(&al);
    if (cities_version() != built_version || al.state != built_auto) {
      if (al.state != built_auto && al.state == AUTO_OK) cities_now_request();
      nav_rebuild();
    }
  }
};

View *view_cities(bool from_weather, int back_depth) { return new CitiesView(from_weather, back_depth); }

// ---------- Cerca una città e risultati (§5, §6) ----------

// "1,4 mln", "93.200 ab.", "421 ab."
static void fmt_population(char *out, size_t n, uint32_t p)
{
  if (!p) out[0] = '\0';
  else if (p >= 1000000) {
    unsigned long tenths = (p + 50000) / 100000;  // un decimale, arrotondato
    snprintf(out, n, "%lu,%lu mln", tenths / 10, tenths % 10);
  }
  else if (p >= 1000) snprintf(out, n, "%lu.%03lu ab.", (unsigned long)(p / 1000), (unsigned long)(p % 1000));
  else snprintf(out, n, "%lu ab.", (unsigned long)p);
}

class CitySearchView : public View {
  enum Mode { KEYBOARD, RESULTS };
  Mode mode = KEYBOARD;
  bool from_weather;
  int back_depth;
  Keyboard kb;
  char query[100] = "";
  City *results = nullptr;  // [CITY_RESULTS] in PSRAM
  int count = 0, page = 0;
  int built_state = -1;
  uint32_t searched_at = 0;
  bool timed_out = false;
  Pager pager;

  struct RowRef {
    CitySearchView *v;
    int i;
  } refs[ROWS];

  CitySearchState state()
  {
    CitySearchState st = city_search_get(nullptr, nullptr);
    return st == CS_LOADING && timed_out ? CS_ERROR : st;
  }

  void run_search()
  {
    kb.trim();
    if (!kb.text[0]) return;
    strlcpy(query, kb.text, sizeof(query));
    city_search(query);
    searched_at = millis();
    timed_out = false;
    count = page = 0;
    mode = RESULTS;
    nav_rebuild();
  }

  void to_keyboard()
  {
    mode = KEYBOARD;
    nav_rebuild();
  }

  void build_keyboard(lv_obj_t *root)
  {
    lv_obj_set_y(ui_make_back_button(root), 8);  // centrata sul campo
    kb.build(root, 152, 632, "Cerca una citt\xC3\xA0");
    lv_obj_t *hint = ui_make_label(root, &font_it_16, COL_TEXT_3,
                                   "Scrivi il nome della citt\xC3\xA0 e tocca Cerca.\nSe ci sono pi\xC3\xB9 posti con lo "
                                   "stesso nome, vedrai provincia e regione.");
    lv_obj_set_width(hint, 580);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, CX(172), 92);
  }

  void build_results(lv_obj_t *root)
  {
    ui_make_back_button(root);
    // Chip con il testo cercato: riporta alla tastiera
    int tw = min(text_w(&font_it_20, query), 360 - 68);
    lv_obj_t *chip = ui_make_touch(root, CX(152), 8, tw + 68, 48, COL_RAISED);
    lv_obj_set_style_radius(chip, 24, 0);
    lv_obj_set_style_bg_color(chip, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_pos(ui_make_label(chip, &font_ic_20, COL_TEXT_2, IC_SEARCH), 20, 14);
    lv_obj_t *t = ui_make_line_label(chip, &font_it_20, COL_TEXT, 48, 11, tw);
    ui_set_text_raw(t, query);
    lv_obj_add_event_cb(chip, [](lv_event_t *e) { ((CitySearchView *)lv_event_get_user_data(e))->to_keyboard(); },
                        LV_EVENT_CLICKED, this);

    CitySearchState st = state();
    built_state = st;
    if (st == CS_LOADING) {
      lv_obj_t *sp = ui_make_spinner(root, 28);
      lv_obj_set_pos(sp, CX(692), 18);
      static const int W1[] = { 160, 200, 140, 180, 150 }, W2[] = { 260, 220, 280, 240, 200 };
      for (int k = 0; k < ROWS; k++) {
        int y = 64 + k * 80;
        ui_make_box(root, 16, y + 12, 56, 56, COL_SURFACE, LV_RADIUS_CIRCLE);
        ui_make_box(root, 88, y + 20, W1[k], 16, COL_SURFACE, 8);
        ui_make_box(root, 88, y + 48, W2[k], 12, COL_SKELETON_2, 6);
      }
      return;
    }
    if (st == CS_ERROR) {
      ui_state_block(root, 0, 712, IC_WARN, COL_WARN, "Non riesco a cercare le citt\xC3\xA0", "Controlla il Wi-Fi e riprova.",
                     "Riprova", [](lv_event_t *e) {
                       CitySearchView *v = (CitySearchView *)lv_event_get_user_data(e);
                       city_search(v->query);
                       v->searched_at = millis();
                       v->timed_out = false;
                       nav_rebuild();
                     }, this);
      return;
    }
    if (!results) results = (City *)heap_caps_calloc(CITY_RESULTS, sizeof(City), MALLOC_CAP_SPIRAM);
    if (!results) return;
    city_search_get(results, &count);
    if (count == 0) {
      char title[160];
      snprintf(title, sizeof(title), "Nessuna citt\xC3\xA0 trovata per \xC2\xAB%s\xC2\xBB", query);
      ui_state_block(root, 0, 712, IC_MARKER, COL_TEXT_2, title,
                     "Controlla come l'hai scritto. Con 2 lettere cerco solo il nome esatto.", "Modifica la ricerca",
                     [](lv_event_t *e) { ((CitySearchView *)lv_event_get_user_data(e))->to_keyboard(); }, this);
      return;
    }
    lv_obj_t *counter = ui_make_label(root, &font_it_16, COL_TEXT_3, "");
    ui_set_text(counter, count == 1 ? "1 risultato" : "%d risultati", count);
    lv_obj_set_pos(counter, CX(720) - text_w(&font_it_16, lv_label_get_text(counter)), 21);

    int pages = (count + ROWS - 1) / ROWS;
    page = constrain(page, 0, pages - 1);
    for (int k = 0; k < ROWS && page * ROWS + k < count; k++) {
      int i = page * ROWS + k;
      const City &c = results[i];
      // Provincia · regione · nazione, saltando quelle che mancano o ripetono la precedente
      char sub[160] = "", right[24];
      const char *parts[] = { c.admin2, c.admin1, c.country };
      const char *prev = c.name;
      for (const char *p : parts) {
        if (!p[0] || !strcmp(p, prev)) continue;
        if (sub[0]) strlcat(sub, " \xC2\xB7 ", sizeof(sub));
        strlcat(sub, p, sizeof(sub));
        prev = p;
      }
      lv_obj_t *row = make_row(root, k, 648, IC_MARKER, COL_TEXT_2, c.name, sub, 424, true);
      if (cities_find(c.id) >= 0) strlcpy(right, "In elenco", sizeof(right));
      else fmt_population(right, sizeof(right), c.population);
      lv_obj_set_pos(ui_make_label(row, &font_it_16, COL_TEXT_3, right), 632 - text_w(&font_it_16, right), 29);
      refs[k] = { this, i };
      lv_obj_add_event_cb(row, [](lv_event_t *e) {
        RowRef *r = (RowRef *)lv_event_get_user_data(e);
        r->v->choose(r->i);
      }, LV_EVENT_CLICKED, &refs[k]);
    }
    pager.build(root, &page, pages);
  }

  // La città entra nell'elenco (non diventa predefinita) e si apre il suo meteo
  void choose(int i)
  {
    const City &c = results[i];
    int idx = cities_add(c);
    if (idx >= 0 && idx == cities_default()) {
      open_city(from_weather, back_depth, nullptr);
      return;
    }
    WeatherPlace p;
    place_of(c, &p);
    open_city(from_weather, back_depth, &p);
  }

public:
  CitySearchView(bool from_weather, int back_depth) : from_weather(from_weather), back_depth(back_depth)
  {
    kb.min_chars = 2;
    kb.max_chars = 48;
    kb.on_search = [this] { run_search(); };
  }
  ~CitySearchView() override { heap_caps_free(results); }
  Section section() const override { return SEC_NONE; }

  void build(lv_obj_t *root) override
  {
    if (mode == KEYBOARD) build_keyboard(root);
    else build_results(root);
  }

  void destroyed() override { kb.destroyed(); }
  bool back() override { return kb.back(); }

  void gesture(lv_dir_t d) override
  {
    if (mode != RESULTS || built_state != CS_OK) return;
    int pages = (count + ROWS - 1) / ROWS;
    if (d == LV_DIR_TOP && page + 1 < pages) page++;
    else if (d == LV_DIR_BOTTOM && page > 0) page--;
    else return;
    nav_rebuild();
  }

  void tick() override
  {
    if (mode != RESULTS) return;
    // Più di 10 s senza risposta: errore (§9)
    if (built_state == CS_LOADING && !timed_out && millis() - searched_at > 10000) timed_out = true;
    if (state() != built_state) nav_rebuild();
  }
};

View *view_city_search(bool from_weather, int back_depth) { return new CitySearchView(from_weather, back_depth); }
