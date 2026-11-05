#include "ui_weather.h"

#include <Arduino.h>
#include <algorithm>
#include <math.h>
#include <time.h>
#include "ui_common.h"
#include "ui_list.h"
#include "weather.h"

#define DEG "\xC2\xB0"
#define DOT " \xC2\xB7 "

// Le 8 fasi della luna a 64 e 20 px (moon_images.c, generato da strumenti/genera_luna.py)
extern "C" {
extern const lv_img_dsc_t *const MOON_64[8];
extern const lv_img_dsc_t *const MOON_20[8];
}

// Colore della fascia EAQI (SPEC_SOLE_ARIA §5.1) e del livello dei pollini (§5.2)
static lv_color_t aq_color(int level)
{
  static const uint32_t AQ_HEX[] = { 0x4CC3D9, 0x5CC48A, 0xE8CF4A, 0xF2873A, 0xE5484D, 0xB76FE3 };
  return lv_color_hex(AQ_HEX[constrain(level, 0, 5)]);
}

static lv_color_t pollen_color(int level) { return level >= 3 ? COL_POL_HIGH : level == 2 ? COL_POL_MID : COL_POL_LOW; }

// Livello più alto dei pollini nelle prossime 24 ore (0 se nessuno)
static int pollen_top(const WeatherAir &a)
{
  int top = 0;
  for (int p = 0; p < POLLEN_COUNT; p++) top = max(top, pollen_level((Pollen)p, a.pollen_max[p]));
  return top;
}

static int minutes_now(time_t now)
{
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour * 60 + t.tm_min;
}

static int text_w(const lv_font_t *font, const char *text)
{
  lv_point_t p;
  lv_txt_get_size(&p, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return p.x;
}

// Spostare un oggetto lo ridisegna: solo se la posizione cambia davvero
static void place(lv_obj_t *obj, int x, int y)
{
  if (lv_obj_get_style_x(obj, 0) != x || lv_obj_get_style_y(obj, 0) != y) lv_obj_set_pos(obj, x, y);
}

// Testo accorciato con "…" finché non sta in max_w. La troncatura di LVGL lascia lo spazio vuoto
// a destra dell'etichetta, mentre la percepita deve seguire la città subito (§3.2)
static void fit_text(const lv_font_t *font, const char *src, int max_w, char *out, size_t n)
{
  strlcpy(out, src, n);
  if (text_w(font, out) <= max_w) return;
  size_t len = strlen(out);
  while (len > 0) {
    do len--;
    while (len > 0 && (out[len] & 0xC0) == 0x80);  // non tagliare a metà un carattere UTF-8
    while (len > 0 && out[len - 1] == ' ') len--;
    if (len + 4 > n) continue;
    memcpy(out + len, "\xE2\x80\xA6", 4);
    if (text_w(font, out) <= max_w) return;
  }
  out[0] = 0;
}

// Solo il sole è giallo, tutte le altre icone bianche (SPEC_METEO §5)
static void set_icon(lv_obj_t *label, const char *icon)
{
  ui_set_text_raw(label, icon);
  ui_set_text_color(label, strcmp(icon, WX_ICON_SUN) == 0 ? COL_WX_SUN : COL_TEXT);
}

// "21°", "-4°": arrotondato all'intero, mai "-0"
static long deg(float t) { return lroundf(t); }

static const char *hhmm(time_t t, char *out, size_t n)
{
  struct tm tm;
  localtime_r(&t, &tm);
  snprintf(out, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return out;
}

// Cella delle ore (fascia §3.3 e pagina §4): etichetta, icona con la pioggia accanto, temperatura.
// Coordinate relative al pannello che la contiene, top = bordo alto del pannello.
struct WxCell {
  lv_obj_t *label, *icon, *rain, *temp;
  int cx, top;

  void create(lv_obj_t *parent, int center_x, int y, int w)
  {
    cx = center_x;
    top = y;
    label = ui_make_label(parent, &font_it_16, COL_TEXT_2, "");
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, cx - w / 2, top + 10);
    icon = ui_make_label(parent, &font_ic_28, COL_TEXT, "");
    rain = ui_make_label(parent, &font_it_16, COL_WX_RAIN, "");
    temp = ui_make_label(parent, &font_it_20, COL_TEXT, "");
    lv_obj_set_width(temp, w);
    lv_obj_set_style_text_align(temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(temp, cx - w / 2, top + 72);
  }

  void set(const char *label_text, lv_color_t label_color, int code, bool day, int rain_pct, const char *temp_text)
  {
    ui_set_text_raw(label, label_text);
    ui_set_text_color(label, label_color);
    const char *ic = weather_icon(code, day);
    set_icon(icon, ic);
    bool wet = rain_pct >= 20;
    ui_set_hidden(rain, !wet);
    if (wet) ui_set_text(rain, "%d%%", rain_pct);
    // Il gruppo icona + percentuale è centrato sulla larghezza vera dell'icona (la nuvola è 1,25 volte più larga)
    int iw = text_w(&font_ic_28, ic);
    int gw = iw + (wet ? 6 + text_w(&font_it_16, lv_label_get_text(rain)) : 0);
    int x = cx - gw / 2;
    place(icon, x, top + 38);
    place(rain, x + iw + 6, top + 38 + 4);  // centrata sull'icona: righe di 29 e 22 px
    ui_set_text_raw(temp, temp_text);
  }
};

// ---------- Orologio (§3) ----------

// Blocco "adesso" (area di tocco 440,72 336x156): figli in coordinate relative
static lv_obj_t *now_box, *now_main, *now_icon, *now_temp, *now_max, *now_min, *now_drop, *now_rain, *now_cond;
static lv_obj_t *now_city, *now_feels, *now_warn, *now_updated;
static lv_obj_t *now_skeleton, *now_error;
// Fascia 40,248 720x104
static lv_obj_t *band, *band_main, *band_skeleton;
static WxCell band_cells[6];

// Riga 40,356 720x48 con tre posti da 240 (SPEC_SOLE_ARIA §3.2)
struct RowSlot {
  lv_obj_t *touch, *icon, *moon, *word, *value;
  int tab;
};
static lv_obj_t *row, *row_skeleton;
static RowSlot row_slots[3];

static void open_weather(lv_event_t *e) { nav_open_view(view_weather()); }
static void open_slot(lv_event_t *e) { nav_open_view(view_weather(((RowSlot *)lv_event_get_user_data(e))->tab)); }

static lv_obj_t *make_group(lv_obj_t *parent, int w, int h)
{
  lv_obj_t *g = lv_obj_create(parent);
  lv_obj_remove_style_all(g);
  lv_obj_set_size(g, w, h);
  lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return g;
}

void clock_weather_create(lv_obj_t *panel)
{
  now_box = ui_make_touch(panel, 440, 72, 336, 156, COL_PRESSED);
  lv_obj_set_style_radius(now_box, 12, 0);
  lv_obj_add_event_cb(now_box, open_weather, LV_EVENT_CLICKED, nullptr);

  // Normale e dato vecchio (§3.2)
  now_main = make_group(now_box, 336, 156);
  now_icon = ui_make_label(now_main, &font_wx_56, COL_TEXT, "");
  lv_obj_set_pos(now_icon, 16, 16);
  // y 20: il carattere ha la riga di 47 px, le cifre allineate a quelle del mockup (y 93 sullo schermo)
  now_temp = ui_make_label(now_main, &font_temp_64, COL_TEXT, "");
  now_max = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_width(now_max, 88);
  lv_obj_set_style_text_align(now_max, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(now_max, 232, 12);
  now_min = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_width(now_min, 88);
  lv_obj_set_style_text_align(now_min, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(now_min, 232, 34);
  now_drop = ui_make_label(now_main, &font_ic_20, COL_WX_RAIN, IC_DROP);
  now_rain = ui_make_label(now_main, &font_it_16, COL_WX_RAIN, "");
  now_cond = ui_make_line_label(now_main, &font_it_20, COL_TEXT, 16, 88, 304);
  now_city = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_pos(now_city, 16, 118);
  now_feels = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  now_warn = ui_make_label(now_main, &font_ic_20, COL_WARN, IC_WARN);
  lv_obj_set_pos(now_warn, 16, 119);
  now_updated = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_pos(now_updated, 16 + text_w(&font_ic_20, IC_WARN) + 8, 118);

  // In arrivo (§3.4): scheletro fisso
  now_skeleton = make_group(now_box, 336, 156);
  ui_make_box(now_skeleton, 16, 16, 56, 56, COL_SURFACE, LV_RADIUS_CIRCLE);
  ui_make_box(now_skeleton, 88, 20, 112, 48, COL_SURFACE, 8);
  ui_make_box(now_skeleton, 248, 16, 72, 16, COL_SURFACE, 4);
  ui_make_box(now_skeleton, 248, 38, 72, 16, COL_SKELETON_2, 4);
  ui_make_box(now_skeleton, 16, 92, 160, 20, COL_SURFACE, 4);
  lv_obj_set_pos(ui_make_label(now_skeleton, &font_it_16, COL_TEXT_3, "Meteo in arrivo\xE2\x80\xA6"), 16, 118);

  // Errore
  now_error = make_group(now_box, 336, 156);
  lv_obj_set_pos(ui_make_label(now_error, &font_wx_56, COL_DISABLED_FG, weather_icon(3, true)), 16, 16);
  lv_obj_set_pos(ui_make_label(now_error, &font_it_20, COL_TEXT_2, "Meteo non disponibile"), 16, 88);
  // Dopo un errore weather.cpp riprova ogni 2 minuti: "tra poco" (SPEC_METEO §3.4)
  lv_obj_set_pos(ui_make_label(now_error, &font_it_16, COL_TEXT_3, "Riprovo tra poco"), 16, 118);

  // Fascia (§3.3)
  band = ui_make_touch(panel, 40, 248, 720, 104, COL_SURFACE_PRESSED);
  lv_obj_set_style_radius(band, 12, 0);
  lv_obj_set_style_bg_color(band, COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(band, open_weather, LV_EVENT_CLICKED, nullptr);
  ui_make_box(band, 600, 16, 1, 72, COL_RAISED);
  band_main = make_group(band, 720, 104);
  for (int i = 0; i < 6; i++) band_cells[i].create(band_main, 60 + i * 120, 0, 120);
  band_skeleton = make_group(band, 720, 104);
  for (int i = 0; i < 6; i++) {
    int cx = 60 + i * 120;
    ui_make_box(band_skeleton, cx - 24, 14, 48, 14, COL_RAISED, 4);
    ui_make_box(band_skeleton, cx - 14, 40, 28, 28, COL_RAISED, LV_RADIUS_CIRCLE);
    ui_make_box(band_skeleton, cx - 20, 76, 40, 16, COL_SURFACE_PRESSED, 4);
  }

  // Riga sole, aria, pollini, luna: ogni voce è icona 20 + 8 px + parola (TEXT_2) + valore (TEXT), centrata
  row = make_group(panel, 720, 48);
  lv_obj_set_pos(row, 40, 356);
  for (int i = 0; i < 3; i++) {
    RowSlot &s = row_slots[i];
    s.touch = ui_make_touch(row, i * 240, 0, 240, 48, COL_PRESSED);
    lv_obj_set_style_radius(s.touch, 12, 0);
    lv_obj_add_event_cb(s.touch, open_slot, LV_EVENT_CLICKED, &s);
    s.icon = ui_make_label(s.touch, &font_ic_20, COL_TEXT, "");
    s.moon = lv_img_create(s.touch);
    s.word = ui_make_label(s.touch, &font_it_20, COL_TEXT_2, "");
    s.value = ui_make_label(s.touch, &font_it_20, COL_TEXT, "");
    s.tab = WT_SUN;
  }
  // In arrivo (§3.3): tre barre ferme
  row_skeleton = make_group(row, 720, 48);
  for (int i = 0; i < 3; i++) ui_make_box(row_skeleton, i * 240 + 50, 16, 140, 16, COL_SURFACE, 8);
}

static void set_clickable(lv_obj_t *obj, bool on)
{
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) == on) return;
  if (on) lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  else {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_state(obj, LV_STATE_PRESSED);
  }
}

static void update_now(const Weather &w, WeatherState st, int hi, int di)
{
  // Con il dato vecchio una previsione di poche ore fa vale più dell'osservazione di allora (§3.4)
  float t = w.temp;
  int code = w.code;
  bool day = w.is_day;
  if (st == WX_OLD) {
    t = w.hours[hi].temp;
    code = w.hours[hi].code;
    day = w.hours[hi].is_day;
  }
  const char *ic = weather_icon(code, day);
  set_icon(now_icon, ic);
  ui_set_text(now_temp, "%ld" DEG, deg(t));
  place(now_temp, 16 + text_w(&font_wx_56, ic) + 16, 20);

  const WeatherDay &d = w.days[di];
  ui_set_text(now_max, "Max %d" DEG, d.tmax);
  ui_set_text(now_min, "Min %d" DEG, d.tmin);
  bool wet = d.rain >= 20;
  ui_set_hidden(now_drop, !wet);
  ui_set_hidden(now_rain, !wet);
  if (wet) {
    ui_set_text(now_rain, "%d%%", d.rain);
    int x = 320 - text_w(&font_it_16, lv_label_get_text(now_rain));
    place(now_rain, x, 56);
    place(now_drop, x - 4 - text_w(&font_ic_20, IC_DROP), 57);
  }
  ui_set_text_raw(now_cond, weather_text(code));

  bool old = st == WX_OLD;
  ui_set_hidden(now_warn, !old);
  ui_set_hidden(now_updated, !old);
  ui_set_hidden(now_city, old || !w.city[0]);
  ui_set_hidden(now_feels, old);
  if (old) {
    char buf[8];
    ui_set_text(now_updated, "Aggiornato alle %s", hhmm(w.updated, buf, sizeof(buf)));
  } else if (w.city[0]) {
    // Si tronca la città, mai la percepita (§3.2)
    char city[40];
    fit_text(&font_it_16, w.city, 180, city, sizeof(city));
    ui_set_text_raw(now_city, city);
    int cw = text_w(&font_it_16, city);
    ui_set_text(now_feels, " \xC2\xB7 percepiti %ld" DEG, deg(w.feels));
    place(now_feels, 16 + cw, 118);
  } else {
    ui_set_text(now_feels, "Percepiti %ld" DEG, deg(w.feels));
    place(now_feels, 16, 118);
  }
}

static void update_band(const Weather &w, int hi, int di)
{
  char label[8], temp[16];
  for (int k = 0; k < 5; k++) {
    const WeatherHour &h = w.hours[min(hi + 2 * (k + 1), WEATHER_HOURS - 1)];
    snprintf(label, sizeof(label), "%02d:00", h.hour);
    snprintf(temp, sizeof(temp), "%d" DEG, h.temp);
    band_cells[k].set(label, COL_TEXT_2, h.code, h.is_day, h.rain, temp);
  }
  const WeatherDay &d = w.days[min(di + 1, WEATHER_DAYS - 1)];
  snprintf(temp, sizeof(temp), "%d" DEG "/%d" DEG, d.tmin, d.tmax);
  band_cells[5].set("Domani", COL_TEXT, d.code, true, d.rain, temp);
}

// Una voce della riga; moon != nullptr al posto dell'icona, word nullptr per la sola luna
static void set_slot(RowSlot &s, const char *icon, lv_color_t icon_color, const lv_img_dsc_t *moon, const char *word,
                     const char *value, int tab)
{
  s.tab = tab;
  ui_set_hidden(s.icon, moon != nullptr);
  ui_set_hidden(s.moon, moon == nullptr);
  ui_set_hidden(s.word, word == nullptr);
  int iw = 20;
  if (moon) {
    if (lv_img_get_src(s.moon) != moon) lv_img_set_src(s.moon, moon);
  } else {
    ui_set_text_raw(s.icon, icon);
    ui_set_text_color(s.icon, icon_color);
    iw = text_w(&font_ic_20, icon);
  }
  int ww = 0;
  if (word) {
    ui_set_text_raw(s.word, word);
    ww = text_w(&font_it_20, word) + text_w(&font_it_20, " ");
  }
  ui_set_text_raw(s.value, value);
  int x = (240 - (iw + 8 + ww + text_w(&font_it_20, value))) / 2;
  // Righe: testo 26 px, icona 21, immagine 20, centrate nei 48 del posto
  place(s.icon, x, 13);
  place(s.moon, x, 14);
  place(s.word, x + iw + 8, 11);
  place(s.value, x + iw + 8 + ww, 11);
}

static void update_row(const Weather &w, WeatherState st, int di, bool can_open)
{
  static char shown[160] = "-";
  static bool shown_open = false;

  WeatherState ast = air_state(w.air);
  bool loading = !(w.valid || w.failed) || !(w.air.valid || w.air.failed);
  time_t now = time(nullptr);
  MoonInfo moon;
  moon_info(now, &moon);

  // Prossimo evento del sole: alba di oggi, tramonto di oggi o alba di domani (§3.2)
  bool sun = st == WX_OK || st == WX_OLD;
  bool sunset = false;
  int ev = -1;
  if (sun) {
    const WeatherDay &d = w.days[di];
    int m = minutes_now(now);
    if (m < d.sunrise) ev = d.sunrise;
    else if (m < d.sunset) ev = d.sunset, sunset = true;
    else ev = w.days[min(di + 1, WEATHER_DAYS - 1)].sunrise;
    sun = ev >= 0;
  }
  bool air = ast == WX_OK || ast == WX_OLD;
  int level = air ? air_level(w.air.aqi) : 0;
  int pollen = air ? pollen_top(w.air) : 0;

  // Si ridisegna solo se cambia una voce (§3.4)
  char sig[160];
  snprintf(sig, sizeof(sig), "%d|%d,%d,%d|%d,%d|%d|%d", loading, sun, sunset, ev, air, level, pollen, moon.phase);
  if (!strcmp(sig, shown) && can_open == shown_open) return;
  strlcpy(shown, sig, sizeof(shown));
  shown_open = can_open;

  ui_set_hidden(row_skeleton, !loading);
  int n = 0;
  if (!loading) {
    static const char *const AIR_WORDS[] = { "buona", "discreta", "moderata", "scadente", "molto scadente", "pessima" };
    char hm[8];
    if (sun) {
      snprintf(hm, sizeof(hm), "%02d:%02d", ev / 60, ev % 60);
      set_slot(row_slots[n++], IC_SUN, COL_WX_SUN, nullptr, sunset ? "Tramonto" : "Alba", hm, WT_SUN);
    }
    if (air) set_slot(row_slots[n++], IC_WIND, aq_color(level), nullptr, "Aria", AIR_WORDS[level], WT_AIR);
    if (air && pollen >= 2)
      set_slot(row_slots[n++], IC_SPA, pollen_color(pollen), nullptr, "Pollini", pollen >= 3 ? "alti" : "medi", WT_AIR);
    if (n < 3) set_slot(row_slots[n++], nullptr, COL_TEXT, MOON_20[moon.phase], nullptr, moon_phase_text(moon.phase), WT_SUN);
  }
  for (int i = 0; i < 3; i++) {
    ui_set_hidden(row_slots[i].touch, i >= n);
    set_clickable(row_slots[i].touch, i < n && can_open);
  }
}

void clock_weather_update(bool can_open)
{
  static uint32_t shown_version = UINT32_MAX;
  static int shown_state = -1, shown_hi = -2, shown_di = -2;
  static bool shown_open = false;

  Weather w;
  weather_get(&w);
  WeatherState st = weather_state(w);
  time_t now = time(nullptr);
  int hi = 0, di = 0;
  if (w.valid) {
    hi = max(0, weather_hour_index(w, now));
    di = max(0, weather_day_index(w, now));
  }
  update_row(w, st, di, can_open);
  // Si ridisegna all'arrivo di dati nuovi, al cambio di stato e al cambio d'ora (§9.4)
  if (w.version == shown_version && st == shown_state && hi == shown_hi && di == shown_di && can_open == shown_open) return;
  shown_version = w.version;
  shown_state = st;
  shown_hi = hi;
  shown_di = di;
  shown_open = can_open;

  bool data = st == WX_OK || st == WX_OLD;
  ui_set_hidden(now_main, !data);
  ui_set_hidden(now_skeleton, st != WX_LOADING);
  ui_set_hidden(now_error, st != WX_ERROR);
  ui_set_hidden(band, st == WX_ERROR);
  ui_set_hidden(band_main, !data);
  ui_set_hidden(band_skeleton, data);
  set_clickable(now_box, data && can_open);
  set_clickable(band, data && can_open);
  if (!data) return;
  update_now(w, st, hi, di);
  update_band(w, hi, di);
}

// ---------- Pagina a schede Meteo · Sole e luna · Aria (SPEC_SOLE_ARIA §4, SPEC_METEO §4) ----------

static lv_obj_t *label_at(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int x, int y, const char *text)
{
  lv_obj_t *l = ui_make_label(parent, font, color, text);
  lv_obj_set_pos(l, x, y);
  return l;
}

// Etichetta allineata a destra a x_right
static lv_obj_t *label_right(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int x_right, int y,
                             const char *text)
{
  return label_at(parent, font, color, x_right - text_w(font, text), y, text);
}

// Cerchio "adesso" (barra del sole, scala dell'aria): 20 px TEXT con bordo 3 SURFACE che lo stacca dal fondo
static lv_obj_t *make_marker(lv_obj_t *parent, int cx, int y)
{
  lv_obj_t *m = ui_make_box(parent, cx - 10, y, 20, 20, COL_TEXT, LV_RADIUS_CIRCLE);
  lv_obj_set_style_border_width(m, 3, 0);
  lv_obj_set_style_border_color(m, COL_SURFACE, 0);
  lv_obj_set_style_border_opa(m, LV_OPA_COVER, 0);
  return m;
}

// "10 h 14 min", "9 h 33 min"
static void fmt_daylight(char *out, size_t n, int minutes) { snprintf(out, n, "%d h %d min", minutes / 60, minutes % 60); }

static int daylight(const WeatherDay &d) { return d.sunrise >= 0 && d.sunset > d.sunrise ? d.sunset - d.sunrise : 0; }

// "36,1"
static void fmt_decimal(char *out, size_t n, float v)
{
  if (isnan(v)) {
    strlcpy(out, "\xE2\x80\x93", n);
    return;
  }
  long t = lroundf(v * 10);
  snprintf(out, n, "%ld,%ld", t / 10, labs(t % 10));
}

static time_t day_start(time_t t)
{
  struct tm tm;
  localtime_r(&t, &tm);
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

static int days_until(time_t now, float days)  // giorni di calendario fra oggi e un evento fra "days" giorni
{
  return (int)lroundf((day_start(now + (time_t)(days * 86400)) - day_start(now)) / 86400.0f);
}

class WeatherView : public View {
  int tab;
  uint32_t built_version = 0;
  int built_state = -1;
  lv_obj_t *sun_marker = nullptr;
  int marker_q = -1;
  uint32_t ticks = 0;

  static void tab_cb(lv_event_t *e)
  {
    WeatherView *v = (WeatherView *)lv_event_get_user_data(e);
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i == v->tab) return;
    v->tab = i;  // sul posto: nessun passo in più nella pila indietro (§2)
    nav_rebuild();
  }

  // Testo in alto a destra, allineato a x 776, L max 264, con l'avviso prima se il dato è vecchio (§4.1)
  void header_info(lv_obj_t *root, const char *text, bool warn)
  {
    lv_obj_t *info = ui_make_label(root, &font_it_16, COL_TEXT_3, text);
    lv_label_set_long_mode(info, LV_LABEL_LONG_DOT);
    int iw = min(264, text_w(&font_it_16, text));
    lv_obj_set_width(info, iw);
    lv_obj_set_pos(info, CX(776) - iw, 21);
    if (warn) label_at(root, &font_ic_20, COL_WARN, CX(776) - iw - 8 - text_w(&font_ic_20, IC_WARN), 22, IC_WARN);
  }

  static int sun_x(float hours) { return 20 + (int)lroundf(hours / 24 * 632); }

  void build_weather(lv_obj_t *root, const Weather &w, WeatherState st)
  {
    if (st != WX_OK && st != WX_OLD) {
      // Scheda raggiunta da un'altra senza dati del meteo: blocco stato come per l'aria (non previsto dalla SPEC)
      bool err = st == WX_ERROR;
      ui_state_block(root, 0, 712, weather_icon(3, true), err ? COL_WARN : COL_TEXT_2,
                     err ? "Meteo non disponibile" : "Meteo in arrivo\xE2\x80\xA6", err ? "Riprovo tra poco." : nullptr);
      return;
    }
    time_t now = time(nullptr);
    int hi = max(0, weather_hour_index(w, now));
    int di = max(0, weather_day_index(w, now));
    char buf[80], hm[8];
    hhmm(w.updated, hm, sizeof(hm));
    if (w.city[0]) snprintf(buf, sizeof(buf), "%s" DOT "aggiornato alle %s", w.city, hm);
    else snprintf(buf, sizeof(buf), "Aggiornato alle %s", hm);
    header_info(root, buf, st == WX_OLD);

    // 12 ore dall'ora in corso, in due pannelli da 6
    char label[8], temp[8];
    for (int p = 0; p < 2; p++) {
      lv_obj_t *panel = ui_make_box(root, CX(104), 72 + p * 112, 672, 104, COL_SURFACE, 12);
      for (int i = 0; i < 6; i++) {
        const WeatherHour &h = w.hours[min(hi + p * 6 + i, WEATHER_HOURS - 1)];
        WxCell cell;
        cell.create(panel, 56 + i * 112, 0, 112);
        snprintf(label, sizeof(label), "%02d:00", h.hour);
        snprintf(temp, sizeof(temp), "%d" DEG, h.temp);
        cell.set(label, COL_TEXT_2, h.code, h.is_day, h.rain, temp);
      }
    }

    // Oggi e Domani
    for (int k = 0; k < 2; k++) {
      const WeatherDay &d = w.days[min(di + k, WEATHER_DAYS - 1)];
      lv_obj_t *card = ui_make_box(root, CX(104) + k * 344, 296, 328, 168, COL_SURFACE, 12);
      label_at(card, &font_it_sb20, COL_TEXT, 20, 16, k ? "Domani" : "Oggi");
      lv_obj_t *icon = label_at(card, &font_ic_40, COL_TEXT, 20, 60, "");
      set_icon(icon, weather_icon(d.code, true));
      lv_obj_t *cond = ui_make_line_label(card, &font_it_20, COL_TEXT, 80, 56, 228);
      ui_set_text_raw(cond, weather_text(d.code));
      snprintf(buf, sizeof(buf), "Min %d" DEG DOT "Max %d" DEG, d.tmin, d.tmax);
      label_at(card, &font_it_16, COL_TEXT_2, 80, 84, buf);
      // Pioggia sempre mostrata, anche 0%
      label_at(card, &font_ic_20, COL_WX_RAIN, 20, 129, IC_DROP);
      int x = 20 + text_w(&font_ic_20, IC_DROP) + 8;
      label_at(card, &font_it_16, COL_TEXT_2, x, 128, "Pioggia ");
      x += text_w(&font_it_16, "Pioggia ");
      snprintf(buf, sizeof(buf), "%d%%", d.rain);
      label_at(card, &font_it_16, COL_WX_RAIN, x, 128, buf);
    }
  }

  void build_sun(lv_obj_t *root, const Weather &w, WeatherState st)
  {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char buf[80], a[16], b[16];
    snprintf(buf, sizeof(buf), "%s %d %s", GIORNI[t.tm_wday], t.tm_mday, MESI[t.tm_mon]);
    header_info(root, buf, false);

    lv_obj_t *panel = ui_make_box(root, CX(104), 72, 672, 216, COL_SURFACE, 12);
    lv_obj_t *days = ui_make_box(root, CX(104), 296, 328, 168, COL_SURFACE, 12);
    label_at(days, &font_it_sb20, COL_TEXT, 20, 16, "Prossimi giorni");
    bool data = st == WX_OK || st == WX_OLD;
    int di = data ? max(0, weather_day_index(w, now)) : 0;
    if (!data || w.days[di].sunrise < 0) {
      // Meteo in arrivo o in errore: solo i titoli e una riga (§4.3)
      label_at(panel, &font_it_sb20, COL_TEXT, 60, 16, "Oggi");
      label_at(panel, &font_ic_28, COL_WX_SUN, 20, 15, IC_SUN);
      const char *msg = st == WX_LOADING ? "Alba e tramonto in arrivo\xE2\x80\xA6" : "Alba e tramonto non disponibili";
      label_at(panel, &font_it_16, COL_TEXT_3, 20, 60, msg);
      label_at(days, &font_it_16, COL_TEXT_3, 20, 60, msg);
    } else {
      const WeatherDay &d = w.days[di];
      label_at(panel, &font_ic_28, COL_WX_SUN, 20, 15, IC_SUN);
      label_at(panel, &font_it_sb20, COL_TEXT, 60, 16, "Oggi");
      static const char *const UV[] = { "basso", "moderato", "alto", "molto alto", "estremo" };
      int uvl = d.uv <= 2 ? 0 : d.uv <= 5 ? 1 : d.uv <= 7 ? 2 : d.uv <= 10 ? 3 : 4;
      snprintf(buf, sizeof(buf), "Indice UV massimo %d" DOT "%s", d.uv, UV[uvl]);
      label_right(panel, &font_it_16, COL_TEXT_2, 652, 19, buf);
      static const char *const LABELS[] = { "Alba", "Tramonto", "Ore di luce" };
      for (int i = 0; i < 3; i++) label_at(panel, &font_it_16, COL_TEXT_2, 20 + i * 224, 60, LABELS[i]);
      snprintf(a, sizeof(a), "%02d:%02d", d.sunrise / 60, d.sunrise % 60);
      label_at(panel, &font_it_sb30, COL_TEXT, 20, 84, a);
      snprintf(a, sizeof(a), "%02d:%02d", d.sunset / 60, d.sunset % 60);
      label_at(panel, &font_it_sb30, COL_TEXT, 244, 84, a);
      fmt_daylight(a, sizeof(a), daylight(d));
      label_at(panel, &font_it_sb30, COL_TEXT, 468, 84, a);
      if (di + 1 < WEATHER_DAYS) {
        int diff = daylight(w.days[di + 1]) - daylight(d);
        if (diff == 0) snprintf(buf, sizeof(buf), "come oggi domani");
        else snprintf(buf, sizeof(buf), "%d min in %s domani", abs(diff), diff < 0 ? "meno" : "pi\xC3\xB9");
        label_at(panel, &font_it_16, COL_TEXT_3, 468, 124, buf);
      }
      // Binario della giornata 00:00-24:00, luce fra alba e tramonto, "adesso" spostato ogni 15 minuti
      ui_make_box(panel, 20, 158, 632, 12, COL_RAISED, 6);
      int x0 = sun_x(d.sunrise / 60.0f), x1 = sun_x(d.sunset / 60.0f);
      ui_make_box(panel, x0, 158, x1 - x0, 12, COL_WX_SUN, 6);
      static const char *const HOURS[] = { "06:00", "12:00", "18:00" };
      for (int i = 0; i < 3; i++) {
        lv_obj_t *l = label_at(panel, &font_it_16, COL_TEXT_3, sun_x(6 + i * 6) - 30, 178, HOURS[i]);
        lv_obj_set_width(l, 60);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      }
      marker_q = minutes_now(now) / 15;
      sun_marker = make_marker(panel, sun_x(marker_q / 4.0f), 154);

      // Domani e dopodomani
      for (int k = 1; k <= 2; k++) {
        const WeatherDay &n = w.days[min(di + k, WEATHER_DAYS - 1)];
        int y = 56 + (k - 1) * 56;
        time_t when = now + k * 86400;
        struct tm wt;
        localtime_r(&when, &wt);
        label_at(days, &font_it_16, COL_TEXT_2, 20, y, k == 1 ? "Domani" : GIORNI[wt.tm_wday]);
        fmt_daylight(a, sizeof(a), daylight(n));
        snprintf(buf, sizeof(buf), "luce %s", a);
        label_right(days, &font_it_16, COL_TEXT_2, 308, y, buf);
        snprintf(a, sizeof(a), "%02d:%02d", n.sunrise / 60, n.sunrise % 60);
        snprintf(b, sizeof(b), "%02d:%02d", n.sunset / 60, n.sunset % 60);
        snprintf(buf, sizeof(buf), "%s \xE2\x80\x93 %s", a, b);
        label_at(days, &font_it_20, COL_TEXT, 20, y + 22, buf);
      }
    }

    // Luna: c'è sempre (la calcola la scheda)
    MoonInfo m;
    moon_info(now, &m);
    lv_obj_t *card = ui_make_box(root, CX(448), 296, 328, 168, COL_SURFACE, 12);
    label_at(card, &font_it_sb20, COL_TEXT, 20, 16, "Luna");
    lv_obj_t *img = lv_img_create(card);
    lv_img_set_src(img, MOON_64[m.phase]);
    lv_obj_set_pos(img, 20, 56);
    lv_obj_t *ph = ui_make_line_label(card, &font_it_20, COL_TEXT, 100, 62, 208);
    ui_set_text_raw(ph, moon_phase_text(m.phase));
    snprintf(buf, sizeof(buf), "Illuminata al %d%%", m.lit);
    label_at(card, &font_it_16, COL_TEXT_2, 100, 90, buf);
    // L'evento più vicino fra luna nuova e piena; nel giorno di una delle due si parla dell'altra
    int to_full = days_until(now, m.to_full), to_new = days_until(now, m.to_new);
    bool full = m.phase == 0 || to_new == 0 ? true : m.phase == 4 || to_full == 0 ? false : to_full < to_new;
    int dd = full ? to_full : to_new;
    if (dd == 1) snprintf(buf, sizeof(buf), "Luna %s domani", full ? "piena" : "nuova");
    else snprintf(buf, sizeof(buf), "Luna %s tra %d giorni", full ? "piena" : "nuova", dd);
    label_at(card, &font_it_16, COL_TEXT_2, 20, 132, buf);
  }

  void build_air(lv_obj_t *root, const Weather &w)
  {
    const WeatherAir &a = w.air;
    WeatherState st = air_state(a);
    if (st == WX_LOADING || st == WX_ERROR) {
      bool err = st == WX_ERROR;
      ui_state_block(root, 0, 712, IC_WIND, err ? COL_WARN : COL_TEXT_2,
                     err ? "Qualit\xC3\xA0 dell'aria non disponibile" : "Qualit\xC3\xA0 dell'aria in arrivo\xE2\x80\xA6",
                     // Dopo un errore weather.cpp riprova ogni 2 minuti: "tra poco" (§4.5)
                     err ? "Riprovo tra poco. Il meteo e la luna restano aggiornati." : nullptr);
      return;
    }
    char buf[96], hm[8];
    time_t updated = time(nullptr) - (millis() - a.updated_ms) / 1000;
    hhmm(updated, hm, sizeof(hm));
    if (w.city[0]) snprintf(buf, sizeof(buf), "%s" DOT "aggiornato alle %s", w.city, hm);
    else snprintf(buf, sizeof(buf), "Aggiornato alle %s", hm);
    header_info(root, buf, st == WX_OLD);

    // Adesso + 24 ore
    lv_obj_t *panel = ui_make_box(root, CX(104), 72, 672, 216, COL_SURFACE, 12);
    int level = air_level(a.aqi);
    snprintf(buf, sizeof(buf), "%d", a.aqi);
    // y 20: come la temperatura del meteo, il carattere ha la riga di 47 px invece di 72
    label_at(panel, &font_temp_64, COL_TEXT, 20, 20, buf);
    int x = 20 + text_w(&font_temp_64, buf) + 20;
    ui_make_box(panel, x, 16 + 17 - 7, 14, 14, aq_color(level), LV_RADIUS_CIRCLE);
    label_at(panel, &font_it_sb24, COL_TEXT, x + 14 + 10, 16, air_level_text(level));
    label_at(panel, &font_it_16, COL_TEXT_2, x, 50, "Qualit\xC3\xA0 dell'aria adesso");
    for (int i = 0; i < 6; i++) ui_make_box(panel, 420 + i * 39, 36, 36, 8, aq_color(i), 4);
    float pos = 420 + level * 39 + min((a.aqi - 20.0f * level) / 20.0f, 1.0f) * 36;
    make_marker(panel, (int)lroundf(pos), 30);
    label_at(panel, &font_it_16, COL_TEXT_3, 420, 52, "Buona");
    label_right(panel, &font_it_16, COL_TEXT_3, 652, 52, "Pessima");
    ui_make_box(panel, 20, 96, 632, 1, COL_RAISED);
    label_at(panel, &font_it_16, COL_TEXT_2, 20, 108, "Prossime 24 ore");

    // Barre dall'ora in corso; con il dato vecchio le ultime mancano
    int first = max(0, (int)((time(nullptr) - a.hour0) / 3600));
    int vmin = 999, vmax = -1, imin = 0, imax = 0;
    for (int i = 0; i < 24 && first + i < WEATHER_HOURS; i++) {
      int v = a.aqi_hours[first + i];
      if (v < vmin) vmin = v, imin = i;
      if (v > vmax) vmax = v, imax = i;
      int h = max(4, min(v, 100) * 48 / 100);
      ui_make_box(panel, 24 + i * 26, 182 - h, 20, h, aq_color(air_level(v)), 2);
    }
    char t1[8], t2[8];
    if (vmax >= 0) {
      hhmm(a.hour0 + (first + imin) * 3600, t1, sizeof(t1));
      hhmm(a.hour0 + (first + imax) * 3600, t2, sizeof(t2));
      snprintf(buf, sizeof(buf), "minimo %d alle %s" DOT "massimo %d alle %s", vmin, t1, vmax, t2);
      label_right(panel, &font_it_16, COL_TEXT_3, 652, 108, buf);
    }
    for (int k = 0; k < 4; k++) {
      int i = k * 6;
      if (k) hhmm(a.hour0 + (first + i) * 3600, t1, sizeof(t1));
      lv_obj_t *l = label_at(panel, &font_it_16, COL_TEXT_3, 24 + i * 26 + 10 - 30, 186, k ? t1 : "Ora");
      lv_obj_set_width(l, 60);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }

    // Inquinanti, con il pallino del loro sottoindice
    lv_obj_t *card = ui_make_box(root, CX(104), 296, 328, 168, COL_SURFACE, 12);
    label_at(card, &font_it_sb20, COL_TEXT, 20, 16, "Inquinanti");
    label_right(card, &font_it_16, COL_TEXT_3, 308, 19, "\xC2\xB5g/m\xC2\xB3");
    static const char *const NAMES[] = { "PM2,5", "PM10", "Biossido di azoto", "Ozono" };
    const float values[] = { a.pm25, a.pm10, a.no2, a.o3 };
    for (int i = 0; i < 4; i++) {
      int y = 56 + i * 28;
      ui_make_box(card, 20, y + 6, 10, 10, aq_color(air_level(a.sub[i])), LV_RADIUS_CIRCLE);
      label_at(card, &font_it_16, COL_TEXT_2, 40, y, NAMES[i]);
      fmt_decimal(buf, sizeof(buf), values[i]);
      label_right(card, &font_it_16, COL_TEXT, 308, y, buf);
    }

    // Pollini: i tipi presenti, dal livello più alto (a parità, dai grani più alti), al massimo 4
    card = ui_make_box(root, CX(448), 296, 328, 168, COL_SURFACE, 12);
    label_at(card, &font_it_sb20, COL_TEXT, 20, 16, "Pollini");
    int order[POLLEN_COUNT], n = 0;
    for (int p = 0; p < POLLEN_COUNT; p++)
      if (pollen_level((Pollen)p, a.pollen_max[p]) >= 1) order[n++] = p;
    std::sort(order, order + n, [&](int x, int y) {
      int lx = pollen_level((Pollen)x, a.pollen_max[x]), ly = pollen_level((Pollen)y, a.pollen_max[y]);
      return lx != ly ? lx > ly : a.pollen_max[x] > a.pollen_max[y];
    });
    if (n > 0) {
      label_right(card, &font_it_16, COL_TEXT_3, 308, 19, "prossime 24 ore");
      static const char *const LEVELS[] = { "", "Bassa", "Media", "Alta" };
      for (int i = 0; i < min(n, 4); i++) {
        int p = order[i], lv = pollen_level((Pollen)p, a.pollen_max[p]);
        int y = 56 + i * 28;
        ui_make_box(card, 20, y + 6, 10, 10, pollen_color(lv), LV_RADIUS_CIRCLE);
        label_at(card, &font_it_16, COL_TEXT_2, 40, y, pollen_name((Pollen)p));
        label_right(card, &font_it_16, COL_TEXT, 308, y, LEVELS[lv]);
      }
    } else {
      struct tm t;
      time_t now = time(nullptr);
      localtime_r(&now, &t);
      bool off_season = t.tm_mon >= 9 || t.tm_mon == 0;  // da ottobre a gennaio
      label_at(card, &font_ic_40, COL_TEXT_3, 20, 60, IC_SPA);
      label_at(card, &font_it_20, COL_TEXT, 80, 56, "Nessun polline");
      label_at(card, &font_it_16, COL_TEXT_2, 80, 84, off_season ? "Fuori stagione" : "Nelle prossime 24 ore");
      if (off_season) label_at(card, &font_it_16, COL_TEXT_3, 20, 128, "I primi arrivano a febbraio.");
    }
  }

public:
  explicit WeatherView(int t) : tab(t) {}
  Section section() const override { return SEC_NONE; }
  bool back_goes_home() const override { return true; }

  void build(lv_obj_t *root) override
  {
    sun_marker = nullptr;
    ui_make_back_button(root);
    // Schede al posto del titolo (§4.1, SPEC 6 §4.10; "Sole e luna" larga 144)
    static const char *const TABS[] = { "Meteo", "Sole e luna", "Aria" };
    static const int TX[] = { 152, 256, 400 }, TW[] = { 104, 144, 104 };
    for (int i = 0; i < 3; i++) {
      lv_obj_t *t = ui_make_touch(root, CX(TX[i]), 0, TW[i], 64, COL_BG, false);
      lv_obj_t *l = ui_make_label(t, &font_it_sb20, i == tab ? COL_TEXT : COL_TEXT_3, TABS[i]);
      lv_obj_set_style_text_color(l, COL_TEXT, LV_STATE_PRESSED);
      lv_obj_center(l);
      lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
      if (i == tab) ui_make_box(t, (TW[i] - 56) / 2, 55, 56, 3, COL_ACCENT);
      lv_obj_add_event_cb(t, tab_cb, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(t, (void *)(intptr_t)i);
    }

    Weather w;
    weather_get(&w);
    built_version = w.version;
    if (tab == WT_WEATHER) {
      built_state = weather_state(w);
      build_weather(root, w, (WeatherState)built_state);
    } else if (tab == WT_SUN) {
      built_state = weather_state(w);
      build_sun(root, w, (WeatherState)built_state);
    } else {
      built_state = air_state(w.air);
      build_air(root, w);
    }
  }

  void tick() override
  {
    if (++ticks % 10) return;  // una volta al secondo
    if (sun_marker) {
      int q = minutes_now(time(nullptr)) / 15;
      if (q != marker_q) {
        marker_q = q;
        place(sun_marker, sun_x(q / 4.0f) - 10, 154);
      }
    }
    Weather w;
    weather_get(&w);
    if (tab == WT_WEATHER) {
      // La scheda Meteo resta com'è fino all'uscita (SPEC_METEO §4); si ricostruisce solo se mancavano i dati
      WeatherState st = weather_state(w);
      bool had = built_state == WX_OK || built_state == WX_OLD;
      if (!had && st != built_state) nav_rebuild();
      return;
    }
    // Sole e luna, Aria: si aggiornano all'arrivo di dati nuovi o al cambio di stato (§4.5)
    int st = tab == WT_SUN ? weather_state(w) : air_state(w.air);
    if (w.version != built_version || st != built_state) nav_rebuild();
  }

  void destroyed() override { sun_marker = nullptr; }
};

View *view_weather(int tab) { return new WeatherView(tab); }
