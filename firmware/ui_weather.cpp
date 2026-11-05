#include "ui_weather.h"

#include <Arduino.h>
#include <algorithm>
#include <climits>
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

// "10 h 14 min", "9 h 33 min"
static void fmt_daylight(char *out, size_t n, int minutes) { snprintf(out, n, "%d h %d min", minutes / 60, minutes % 60); }

static int daylight(const WeatherDay &d) { return d.sunrise >= 0 && d.sunset > d.sunrise ? d.sunset - d.sunrise : 0; }

// ---------- Home: riquadro Meteo e riquadri piccoli (SPEC_HOME_GIORNI §3.4-§3.5) ----------

// Riquadro Meteo 40,236 720x120: zona Adesso (x 0-312), 4 ore e Domani
static lv_obj_t *tile, *now_main, *now_icon, *now_temp, *now_max, *now_min, *now_drop, *now_rain, *now_cond;
static lv_obj_t *now_warn, *now_updated;
static lv_obj_t *tile_skeleton, *tile_error, *cells_main;
static WxCell cells[5];

// Riquadri piccoli: 3 x 232x96 a x 40, 284, 528, y 368
struct SmallTile {
  lv_obj_t *touch, *icon, *moon, *label, *value, *detail;
  int tab;
};
static lv_obj_t *small_skeleton;
static SmallTile smalls[3];

static void open_weather(lv_event_t *e) { nav_open_view(view_weather()); }
static void open_small(lv_event_t *e) { nav_open_view(view_weather(((SmallTile *)lv_event_get_user_data(e))->tab)); }

static lv_obj_t *make_group(lv_obj_t *parent, int w, int h)
{
  lv_obj_t *g = lv_obj_create(parent);
  lv_obj_remove_style_all(g);
  lv_obj_set_size(g, w, h);
  lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return g;
}

static lv_obj_t *make_tile(lv_obj_t *parent, int x, int y, int w, int h)
{
  lv_obj_t *t = ui_make_touch(parent, x, y, w, h, COL_SURFACE_PRESSED);
  lv_obj_set_style_radius(t, 12, 0);
  lv_obj_set_style_bg_color(t, COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  return t;
}

void home_weather_create(lv_obj_t *panel)
{
  tile = make_tile(panel, 40, 236, 720, 120);
  lv_obj_add_event_cb(tile, open_weather, LV_EVENT_CLICKED, nullptr);
  ui_make_box(tile, 312, 16, 1, 88, COL_RAISED);
  ui_make_box(tile, 632, 24, 1, 72, COL_RAISED);

  // Normale e dato vecchio
  now_main = make_group(tile, 312, 120);
  now_icon = ui_make_label(now_main, &font_wx_56, COL_TEXT, "");
  lv_obj_set_pos(now_icon, 20, 18);  // centrata nella riga di 72 che parte a y 10
  // y 22: il carattere ha la riga di 47 px invece di 72, le cifre come nel mockup
  now_temp = ui_make_label(now_main, &font_temp_64, COL_TEXT, "");
  now_max = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_width(now_max, 88);
  lv_obj_set_style_text_align(now_max, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(now_max, 208, 16);
  now_min = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_width(now_min, 88);
  lv_obj_set_style_text_align(now_min, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(now_min, 208, 38);
  now_drop = ui_make_label(now_main, &font_ic_20, COL_WX_RAIN, IC_DROP);
  now_rain = ui_make_label(now_main, &font_it_16, COL_WX_RAIN, "");
  now_cond = ui_make_line_label(now_main, &font_it_20, COL_TEXT, 20, 86, 276);
  now_warn = ui_make_label(now_main, &font_ic_20, COL_WARN, IC_WARN);
  now_updated = ui_make_label(now_main, &font_it_16, COL_TEXT_2, "");
  cells_main = make_group(tile, 720, 120);
  for (int i = 0; i < 4; i++) cells[i].create(cells_main, 352 + i * 80, 4, 80);
  cells[4].create(cells_main, 676, 4, 88);

  // In arrivo: scheletro fermo
  tile_skeleton = make_group(tile, 720, 120);
  ui_make_box(tile_skeleton, 20, 18, 56, 56, COL_RAISED, LV_RADIUS_CIRCLE);
  ui_make_box(tile_skeleton, 88, 22, 100, 48, COL_RAISED, 8);
  ui_make_box(tile_skeleton, 224, 18, 72, 16, COL_RAISED, 4);
  ui_make_box(tile_skeleton, 224, 40, 72, 16, COL_SURFACE_PRESSED, 4);
  lv_obj_set_pos(ui_make_label(tile_skeleton, &font_it_16, COL_TEXT_3, "Meteo in arrivo\xE2\x80\xA6"), 20, 88);
  for (int i = 0; i < 5; i++) {
    int cx = i < 4 ? 352 + i * 80 : 676;
    ui_make_box(tile_skeleton, cx - 24, 18, 48, 14, COL_RAISED, 4);
    ui_make_box(tile_skeleton, cx - 14, 46, 28, 28, COL_RAISED, LV_RADIUS_CIRCLE);
    ui_make_box(tile_skeleton, cx - 20, 84, 40, 16, COL_SURFACE_PRESSED, 4);
  }

  // Errore: il riquadro resta, vuoto tranne la nuvola spenta e due righe
  tile_error = make_group(tile, 720, 120);
  lv_obj_set_pos(ui_make_label(tile_error, &font_wx_56, COL_DISABLED_FG, weather_icon(3, true)), 20, 32);
  lv_obj_set_pos(ui_make_label(tile_error, &font_it_20, COL_TEXT_2, "Meteo non disponibile"), 100, 34);
  // Dopo un errore weather.cpp riprova ogni 2 minuti: "tra poco" (SPEC_METEO §3.4)
  lv_obj_set_pos(ui_make_label(tile_error, &font_it_16, COL_TEXT_3, "Riprovo tra poco"), 100, 62);

  // Riquadri piccoli: icona 20, etichetta, valore SB20, dettaglio
  for (int i = 0; i < 3; i++) {
    SmallTile &t = smalls[i];
    t.touch = make_tile(panel, 40 + i * 244, 368, 232, 96);
    lv_obj_add_event_cb(t.touch, open_small, LV_EVENT_CLICKED, &t);
    t.icon = ui_make_label(t.touch, &font_ic_20, COL_TEXT, "");
    lv_obj_set_pos(t.icon, 16, 15);
    t.moon = lv_img_create(t.touch);
    lv_obj_set_pos(t.moon, 16, 15);
    t.label = ui_make_label(t.touch, &font_it_16, COL_TEXT_2, "");
    lv_obj_set_pos(t.label, 44, 14);
    t.value = ui_make_line_label(t.touch, &font_it_sb20, COL_TEXT, 16, 40, 200);
    t.detail = ui_make_line_label(t.touch, &font_it_16, COL_TEXT_3, 16, 66, 200);
    t.tab = WT_SUN;
  }
  small_skeleton = make_group(panel, 800, 96);
  lv_obj_set_pos(small_skeleton, 0, 368);
  for (int i = 0; i < 3; i++) {
    lv_obj_t *b = ui_make_box(small_skeleton, 40 + i * 244, 0, 232, 96, COL_SURFACE, 12);
    ui_make_box(b, 16, 16, 100, 16, COL_RAISED, 8);
    ui_make_box(b, 16, 44, 140, 20, COL_RAISED, 8);
    ui_make_box(b, 16, 72, 110, 12, COL_SURFACE_PRESSED, 6);
  }
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
  // Con il dato vecchio una previsione di poche ore fa vale più dell'osservazione di allora (SPEC_METEO §3.4)
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
  int tx = 20 + text_w(&font_wx_56, ic) + 12;
  place(now_temp, tx, 22);
  // Se la temperatura arriva alla colonna Max/Min (in Italia non capita) Max e Min si nascondono
  bool fits = tx + text_w(&font_temp_64, lv_label_get_text(now_temp)) < 296 - 64;

  const WeatherDay &d = w.days[di];
  ui_set_text(now_max, "Max %d" DEG, d.tmax);
  ui_set_text(now_min, "Min %d" DEG, d.tmin);
  ui_set_hidden(now_max, !fits);
  ui_set_hidden(now_min, !fits);
  bool wet = fits && d.rain >= 20;
  ui_set_hidden(now_drop, !wet);
  ui_set_hidden(now_rain, !wet);
  if (wet) {
    ui_set_text(now_rain, "%d%%", d.rain);
    int x = 296 - text_w(&font_it_16, lv_label_get_text(now_rain));
    place(now_rain, x, 60);
    place(now_drop, x - 4 - text_w(&font_ic_20, IC_DROP), 61);
  }
  ui_set_text_raw(now_cond, weather_text(code));

  // Dato vecchio: avviso + ora del dato in basso a destra della zona Adesso, condizione più corta
  bool old = st == WX_OLD;
  ui_set_hidden(now_warn, !old);
  ui_set_hidden(now_updated, !old);
  lv_obj_set_width(now_cond, old ? 180 : 276);
  if (old) {
    char buf[8];
    ui_set_text_raw(now_updated, hhmm(w.updated, buf, sizeof(buf)));
    int x = 296 - text_w(&font_it_16, buf);
    place(now_updated, x, 90);
    place(now_warn, x - 4 - text_w(&font_ic_20, IC_WARN), 91);
  }
}

static void update_cells(const Weather &w, int hi, int di)
{
  char label[8], temp[16];
  for (int k = 0; k < 4; k++) {
    const WeatherHour &h = w.hours[min(hi + 2 * (k + 1), WEATHER_HOURS - 1)];
    snprintf(label, sizeof(label), "%02d:00", h.hour);
    snprintf(temp, sizeof(temp), "%d" DEG, h.temp);
    cells[k].set(label, COL_TEXT_2, h.code, h.is_day, h.rain, temp);
  }
  const WeatherDay &d = w.days[min(di + 1, WEATHER_DAYS - 1)];
  snprintf(temp, sizeof(temp), "%d" DEG "/%d" DEG, d.tmin, d.tmax);
  cells[4].set("Domani", COL_TEXT, d.code, true, d.rain, temp);
}

// Un riquadro piccolo; moon != nullptr al posto dell'icona
static void set_small(SmallTile &t, const char *icon, lv_color_t icon_color, const lv_img_dsc_t *moon, const char *label,
                      const char *value, const char *detail, int tab)
{
  t.tab = tab;
  ui_set_hidden(t.icon, moon != nullptr);
  ui_set_hidden(t.moon, moon == nullptr);
  if (moon) {
    if (lv_img_get_src(t.moon) != moon) lv_img_set_src(t.moon, moon);
  } else {
    ui_set_text_raw(t.icon, icon);
    ui_set_text_color(t.icon, icon_color);
  }
  ui_set_text_raw(t.label, label);
  ui_set_text_raw(t.value, value);
  ui_set_text_raw(t.detail, detail);
}

static void update_smalls(const Weather &w, WeatherState st, int di, bool can_open)
{
  static char shown[160] = "-";
  static bool shown_open = false;

  WeatherState ast = air_state(w.air);
  bool loading = !(w.valid || w.failed) || !(w.air.valid || w.air.failed);
  time_t now = time(nullptr);
  MoonInfo moon;
  moon_info(now, &moon);

  // Prossimo evento del sole: alba di oggi, tramonto di oggi o alba di domani (SPEC_SOLE_ARIA §3.2)
  bool sun = st == WX_OK || st == WX_OLD;
  int ev = -1, kind = 0, light = 0;  // kind: 0 alba, 1 tramonto, 2 alba domani
  if (sun) {
    const WeatherDay &d = w.days[di], &d1 = w.days[min(di + 1, WEATHER_DAYS - 1)];
    int m = minutes_now(now);
    if (m < d.sunrise) ev = d.sunrise, light = daylight(d);
    else if (m < d.sunset) ev = d.sunset, kind = 1, light = daylight(d);
    else ev = d1.sunrise, kind = 2, light = daylight(d1);
    sun = ev >= 0;
  }
  bool air = ast == WX_OK || ast == WX_OLD;
  int level = air ? air_level(w.air.aqi) : 0;
  int pollen = air ? pollen_top(w.air) : 0;

  // Si ridisegna solo se cambia una voce (SPEC_SOLE_ARIA §3.4)
  char sig[160];
  snprintf(sig, sizeof(sig), "%d|%d,%d,%d,%d|%d,%d,%d|%d|%d,%d", loading, sun, kind, ev, light, air, level, w.air.aqi,
           pollen, moon.phase, moon.lit);
  if (!strcmp(sig, shown) && can_open == shown_open) return;
  strlcpy(shown, sig, sizeof(shown));
  shown_open = can_open;

  ui_set_hidden(small_skeleton, !loading);
  int n = 0;
  if (!loading) {
    static const char *const SUN_LABELS[] = { "Alba", "Tramonto", "Alba domani" };
    char value[16], detail[48];
    if (sun) {
      snprintf(value, sizeof(value), "%02d:%02d", ev / 60, ev % 60);
      char l[16];
      fmt_daylight(l, sizeof(l), light);
      snprintf(detail, sizeof(detail), "luce %s", l);
      set_small(smalls[n++], IC_SUN, COL_WX_SUN, nullptr, SUN_LABELS[kind], value, detail, WT_SUN);
    }
    if (air) {
      snprintf(detail, sizeof(detail), "indice %d", w.air.aqi);
      set_small(smalls[n++], IC_WIND, aq_color(level), nullptr, "Aria", air_level_text(level), detail, WT_AIR);
    }
    if (air && pollen >= 2) {
      // I due tipi con più grani al livello più alto: "Olivo, graminacee"
      int a = -1, b = -1;
      for (int p = 0; p < POLLEN_COUNT; p++) {
        if (pollen_level((Pollen)p, w.air.pollen_max[p]) != pollen) continue;
        if (a < 0 || w.air.pollen_max[p] > w.air.pollen_max[a]) b = a, a = p;
        else if (b < 0 || w.air.pollen_max[p] > w.air.pollen_max[b]) b = p;
      }
      strlcpy(detail, pollen_name((Pollen)a), sizeof(detail));
      if (b >= 0) {
        char second[24];
        strlcpy(second, pollen_name((Pollen)b), sizeof(second));
        second[0] = tolower(second[0]);
        strlcat(detail, ", ", sizeof(detail));
        strlcat(detail, second, sizeof(detail));
      }
      set_small(smalls[n++], IC_SPA, pollen_color(pollen), nullptr, "Pollini", pollen >= 3 ? "Alti" : "Medi", detail,
                WT_AIR);
    }
    if (n < 3) {
      snprintf(detail, sizeof(detail), "illuminata al %d%%", moon.lit);
      set_small(smalls[n++], nullptr, COL_TEXT, MOON_20[moon.phase], "Luna", moon_phase_text(moon.phase), detail, WT_SUN);
    }
  }
  for (int i = 0; i < 3; i++) {
    ui_set_hidden(smalls[i].touch, i >= n);
    set_clickable(smalls[i].touch, i < n && can_open);
  }
}

void home_weather_update(bool can_open)
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
  update_smalls(w, st, di, can_open);
  // Si ridisegna all'arrivo di dati nuovi, al cambio di stato e al cambio d'ora
  if (w.version == shown_version && st == shown_state && hi == shown_hi && di == shown_di && can_open == shown_open) return;
  shown_version = w.version;
  shown_state = st;
  shown_hi = hi;
  shown_di = di;
  shown_open = can_open;

  bool data = st == WX_OK || st == WX_OLD;
  ui_set_hidden(now_main, !data);
  ui_set_hidden(cells_main, !data);
  ui_set_hidden(tile_skeleton, st != WX_LOADING);
  ui_set_hidden(tile_error, st != WX_ERROR);
  set_clickable(tile, data && can_open);
  if (!data) return;
  update_now(w, st, hi, di);
  update_cells(w, hi, di);
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

static const char *const UV_TEXT[] = { "basso", "moderato", "alto", "molto alto", "estremo" };
static int uv_level(int uv) { return uv <= 2 ? 0 : uv <= 5 ? 1 : uv <= 7 ? 2 : uv <= 10 ? 3 : 4; }

static time_t add_days(time_t date, int days)
{
  struct tm t;
  localtime_r(&date, &t);
  t.tm_mday += days;
  t.tm_hour = t.tm_min = t.tm_sec = 0;
  t.tm_isdst = -1;
  return mktime(&t);
}

static int days_between(time_t from, time_t to) { return (int)lroundf((day_start(to) - day_start(from)) / 86400.0f); }

// "Oggi", "Domani", "Ieri", altrimenti "Sabato 7" (lower: "sabato 7", nei titoli degli errori)
static void day_name(char *out, size_t n, time_t date, int offset, bool lower = false)
{
  if (offset == 0) strlcpy(out, lower ? "oggi" : "Oggi", n);
  else if (offset == 1) strlcpy(out, lower ? "domani" : "Domani", n);
  else if (offset == -1) strlcpy(out, lower ? "ieri" : "Ieri", n);
  else {
    struct tm t;
    localtime_r(&date, &t);
    snprintf(out, n, "%s %d", GIORNI[t.tm_wday], t.tm_mday);
    if (lower) out[0] = tolower(out[0]);
  }
}

// Riga di barre ferme al posto dei testi (stato "in arrivo" di un giorno, §8.6)
static void skeleton_bars(lv_obj_t *parent, int x, int y, int n)
{
  static const int W[] = { 140, 200, 120, 170 };
  for (int i = 0; i < n; i++)
    ui_make_box(parent, x, y + i * 28, W[i % 4], 14, i ? COL_SURFACE_PRESSED : COL_RAISED, 7);
}

static DayAir *ui_day_air;  // copia dei dati dell'aria di un giorno (1 KB), in PSRAM

class WeatherView : public View {
  int tab;
  time_t sel_date;           // giorno scelto (mezzanotte): resta la stessa data anche passata la mezzanotte
  time_t built_today = 0;
  uint32_t built_version = 0;
  int built_state = -1;
  int built_day_status = -1;
  uint32_t request_at = 0;    // 400 ms dopo l'ultimo cambio di giorno si chiede (§8.7)
  uint32_t loading_since = 0;
  bool timed_out = false;
  lv_obj_t *sun_marker = nullptr;
  int marker_q = -1;
  uint32_t ticks = 0;

  int offset() const { return days_between(time(nullptr), sel_date); }
  DayKind kind() const { return tab == WT_AIR ? DAY_AIR : DAY_WEATHER; }

  void set_day(int off)
  {
    off = constrain(off, -7, 7);
    if (off == offset()) return;
    sel_date = add_days(day_start(time(nullptr)), off);
    timed_out = false;
    nav_rebuild();
  }

  static void tab_cb(lv_event_t *e)
  {
    WeatherView *v = (WeatherView *)lv_event_get_user_data(e);
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i == v->tab) return;
    v->tab = i;  // sul posto e sullo stesso giorno: nessun passo in più nella pila indietro
    v->timed_out = false;
    nav_rebuild();
  }

  static void retry_cb(lv_event_t *e)
  {
    WeatherView *v = (WeatherView *)lv_event_get_user_data(e);
    weather_day_request(v->kind(), v->offset(), true);
    v->loading_since = millis();
    v->timed_out = false;
    nav_rebuild();
  }

  // Selettore ‹ giorno › al posto del testo a destra (§8.1)
  void build_selector(lv_obj_t *root, const Weather &w, bool old, uint32_t old_ms_ago)
  {
    int off = offset();
    lv_obj_t *icon;
    lv_obj_t *prev = ui_make_icon_button(root, CX(512), 0, IC_LEFT, COL_TEXT, [](lv_event_t *e) {
      WeatherView *v = (WeatherView *)lv_event_get_user_data(e);
      v->set_day(v->offset() - 1);
    }, this, &icon);
    if (off <= -7) {
      lv_obj_clear_flag(prev, LV_OBJ_FLAG_CLICKABLE);
      ui_set_text_color(icon, COL_DISABLED_FG_2);
    }
    lv_obj_t *next = ui_make_icon_button(root, CX(712), 0, IC_RIGHT, COL_TEXT, [](lv_event_t *e) {
      WeatherView *v = (WeatherView *)lv_event_get_user_data(e);
      v->set_day(v->offset() + 1);
    }, this, &icon);
    if (off >= 7) {
      lv_obj_clear_flag(next, LV_OBJ_FLAG_CLICKABLE);
      ui_set_text_color(icon, COL_DISABLED_FG_2);
    }
    // Nome su due righe; su un altro giorno il blocco riporta a oggi
    lv_obj_t *name = ui_make_touch(root, CX(576), 4, 136, 56, COL_PRESSED, off != 0);
    lv_obj_set_style_radius(name, 12, 0);
    if (off == 0) lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_add_event_cb(name, [](lv_event_t *e) { ((WeatherView *)lv_event_get_user_data(e))->set_day(0); },
                             LV_EVENT_CLICKED, this);
    char buf[40];
    day_name(buf, sizeof(buf), sel_date, off);
    lv_obj_t *l1 = ui_make_label(name, &font_it_sb20, COL_TEXT, buf);
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, 8);
    if (off != 0) {
      lv_obj_align(ui_make_label(name, &font_it_16, COL_ACCENT, "Torna a oggi"), LV_ALIGN_TOP_MID, 0, 34);
    } else if (old) {
      // Dato vecchio: avviso + ora dell'ultimo dato
      char hm[16], t[8];
      snprintf(hm, sizeof(hm), "alle %s", hhmm(time(nullptr) - old_ms_ago / 1000, t, sizeof(t)));
      int gw = text_w(&font_ic_20, IC_WARN) + 6 + text_w(&font_it_16, hm);
      int x = (136 - gw) / 2;
      label_at(name, &font_ic_20, COL_WARN, x, 35, IC_WARN);
      label_at(name, &font_it_16, COL_TEXT_2, x + text_w(&font_ic_20, IC_WARN) + 6, 34, hm);
    } else if (w.city[0]) {
      char city[40];
      fit_text(&font_it_16, w.city, 128, city, sizeof(city));
      lv_obj_align(ui_make_label(name, &font_it_16, COL_TEXT_3, city), LV_ALIGN_TOP_MID, 0, 34);
    }
  }

  static int sun_x(float hours) { return 20 + (int)lroundf(hours / 24 * 632); }

  // Scheda di un giorno (Oggi/Domani e altri giorni): pioggia in % nel futuro, caduta in mm nel passato (§8.3)
  void day_card(lv_obj_t *root, int k, const char *title, const WeatherDay &d, bool past, int mm10)
  {
    char buf[64];
    lv_obj_t *card = ui_make_box(root, CX(104) + k * 344, 296, 328, 168, COL_SURFACE, 12);
    label_at(card, &font_it_sb20, COL_TEXT, 20, 16, title);
    lv_obj_t *icon = label_at(card, &font_ic_40, COL_TEXT, 20, 60, "");
    set_icon(icon, weather_icon(d.code, true));
    lv_obj_t *cond = ui_make_line_label(card, &font_it_20, COL_TEXT, 80, 56, 228);
    ui_set_text_raw(cond, weather_text(d.code));
    snprintf(buf, sizeof(buf), "Min %d" DEG DOT "Max %d" DEG, d.tmin, d.tmax);
    label_at(card, &font_it_16, COL_TEXT_2, 80, 84, buf);
    label_at(card, &font_ic_20, COL_WX_RAIN, 20, 129, IC_DROP);
    int x = 20 + text_w(&font_ic_20, IC_DROP) + 8;
    if (past) {
      if (mm10 < 1) snprintf(buf, sizeof(buf), "Nessuna pioggia");
      else snprintf(buf, sizeof(buf), "Pioggia caduta %d,%d mm", mm10 / 10, mm10 % 10);
      label_at(card, &font_it_16, COL_TEXT_2, x, 128, buf);
      return;
    }
    // Pioggia sempre mostrata, anche 0%
    label_at(card, &font_it_16, COL_TEXT_2, x, 128, "Pioggia ");
    x += text_w(&font_it_16, "Pioggia ");
    snprintf(buf, sizeof(buf), "%d%%", d.rain);
    label_at(card, &font_it_16, COL_WX_RAIN, x, 128, buf);
  }

  // Blocco stato centrato in 88-800 (§8.6), con il pulsante
  void day_block(lv_obj_t *root, const char *icon, lv_color_t color, const char *title, const char *text,
                 const char *button, lv_event_cb_t cb)
  {
    ui_state_block(root, 0, 712, icon, color, title, text, button, cb, this);
  }

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
    for (int k = 0; k < 2; k++) day_card(root, k, k ? "Domani" : "Oggi", w.days[min(di + k, WEATHER_DAYS - 1)], false, 0);
  }

  // Meteo di un altro giorno: 12 orari ogni 2 ore, il giorno e il seguente (§8.3)
  void build_weather_day(lv_obj_t *root, const DayWeather *d, int off)
  {
    char label[8], temp[8], name[40];
    for (int p = 0; p < 2; p++) {
      lv_obj_t *panel = ui_make_box(root, CX(104), 72 + p * 112, 672, 104, COL_SURFACE, 12);
      for (int i = 0; i < 6; i++) {
        int cx = 56 + i * 112;
        if (!d) {  // in arrivo
          ui_make_box(panel, cx - 24, 14, 48, 14, COL_RAISED, 4);
          ui_make_box(panel, cx - 14, 42, 28, 28, COL_RAISED, LV_RADIUS_CIRCLE);
          ui_make_box(panel, cx - 20, 80, 40, 16, COL_SURFACE_PRESSED, 4);
          continue;
        }
        const WeatherHour &h = d->hours[(p * 6 + i) * 2];
        WxCell cell;
        cell.create(panel, cx, 0, 112);
        snprintf(label, sizeof(label), "%02d:00", h.hour);
        snprintf(temp, sizeof(temp), "%d" DEG, h.temp);
        // Nel passato la percentuale non si mostra: l'icona dice già se ha piovuto
        cell.set(label, COL_TEXT_2, h.code, h.is_day, off < 0 || h.rain == 255 ? 0 : h.rain, temp);
      }
    }
    for (int k = 0; k < 2; k++) {
      if (!d) {
        lv_obj_t *card = ui_make_box(root, CX(104) + k * 344, 296, 328, 168, COL_SURFACE, 12);
        skeleton_bars(card, 20, 18, 4);
        continue;
      }
      day_name(name, sizeof(name), add_days(sel_date, k), off + k);
      day_card(root, k, name, d->days[k], off + k < 0, d->rain_mm10[k]);
    }
  }

  // Sole e luna (§4.3 SPEC_SOLE_ARIA e §8.4): d = giorno scelto e i due dopo (nullptr: in arrivo/errore),
  // today_light = ore di luce di oggi per il confronto (-1 se mancano)
  void build_sun(lv_obj_t *root, const WeatherDay *d, int off, int today_light, int status_msg)
  {
    time_t now = time(nullptr);
    char buf[80], a[16], b[16];
    lv_obj_t *panel = ui_make_box(root, CX(104), 72, 672, 216, COL_SURFACE, 12);
    lv_obj_t *days = ui_make_box(root, CX(104), 296, 328, 168, COL_SURFACE, 12);
    label_at(days, &font_it_sb20, COL_TEXT, 20, 16, "Prossimi giorni");
    // Titolo: "Oggi", oppure la data piena negli altri giorni
    struct tm t;
    localtime_r(&sel_date, &t);
    if (off == 0) strlcpy(buf, "Oggi", sizeof(buf));
    else snprintf(buf, sizeof(buf), "%s %d %s", GIORNI[t.tm_wday], t.tm_mday, MESI[t.tm_mon]);
    label_at(panel, &font_ic_28, COL_WX_SUN, 20, 15, IC_SUN);
    label_at(panel, &font_it_sb20, COL_TEXT, 60, 16, buf);
    if (!d || d[0].sunrise < 0) {
      if (status_msg == 0) {  // in arrivo
        skeleton_bars(panel, 20, 64, 4);
        skeleton_bars(days, 20, 60, 3);
      } else {
        const char *msg = status_msg == 1 ? "Alba e tramonto in arrivo\xE2\x80\xA6" : "Alba e tramonto non disponibili";
        label_at(panel, &font_it_16, COL_TEXT_3, 20, 60, msg);
        label_at(days, &font_it_16, COL_TEXT_3, 20, 60, msg);
        if (status_msg == 3) {  // errore di un altro giorno: Riprova dentro il pannello
          ui_make_button(panel, BTN_SECONDARY, 20, 100, 176, 48, IC_REFRESH, "Riprova", retry_cb, this);
          ui_make_button(days, BTN_SECONDARY, 20, 100, 176, 48, IC_REFRESH, "Riprova", retry_cb, this);
        }
      }
    } else {
      snprintf(buf, sizeof(buf), "Indice UV massimo %d" DOT "%s", d[0].uv, UV_TEXT[uv_level(d[0].uv)]);
      label_right(panel, &font_it_16, COL_TEXT_2, 652, 19, buf);
      static const char *const LABELS[] = { "Alba", "Tramonto", "Ore di luce" };
      for (int i = 0; i < 3; i++) label_at(panel, &font_it_16, COL_TEXT_2, 20 + i * 224, 60, LABELS[i]);
      snprintf(a, sizeof(a), "%02d:%02d", d[0].sunrise / 60, d[0].sunrise % 60);
      label_at(panel, &font_it_sb30, COL_TEXT, 20, 84, a);
      snprintf(a, sizeof(a), "%02d:%02d", d[0].sunset / 60, d[0].sunset % 60);
      label_at(panel, &font_it_sb30, COL_TEXT, 244, 84, a);
      fmt_daylight(a, sizeof(a), daylight(d[0]));
      label_at(panel, &font_it_sb30, COL_TEXT, 468, 84, a);
      // Oggi: rispetto a domani; altri giorni: rispetto a oggi
      int diff = off == 0 ? daylight(d[1]) - daylight(d[0]) : (today_light >= 0 ? daylight(d[0]) - today_light : INT_MIN);
      const char *ref = off == 0 ? "domani" : "di oggi";
      if (diff != INT_MIN) {
        if (diff == 0) snprintf(buf, sizeof(buf), off == 0 ? "come oggi domani" : "come oggi");
        else snprintf(buf, sizeof(buf), "%d min in %s %s", abs(diff), diff < 0 ? "meno" : "pi\xC3\xB9", ref);
        label_at(panel, &font_it_16, COL_TEXT_3, 468, 124, buf);
      }
      // Binario della giornata 00:00-24:00, luce fra alba e tramonto; "adesso" solo oggi, spostato ogni 15 minuti
      ui_make_box(panel, 20, 158, 632, 12, COL_RAISED, 6);
      int x0 = sun_x(d[0].sunrise / 60.0f), x1 = sun_x(d[0].sunset / 60.0f);
      ui_make_box(panel, x0, 158, x1 - x0, 12, COL_WX_SUN, 6);
      static const char *const HOURS[] = { "06:00", "12:00", "18:00" };
      for (int i = 0; i < 3; i++) {
        lv_obj_t *l = label_at(panel, &font_it_16, COL_TEXT_3, sun_x(6 + i * 6) - 30, 178, HOURS[i]);
        lv_obj_set_width(l, 60);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      }
      if (off == 0) {
        marker_q = minutes_now(now) / 15;
        sun_marker = make_marker(panel, sun_x(marker_q / 4.0f), 154);
      }
      // I due giorni dopo quello scelto
      for (int k = 1; k <= 2; k++) {
        const WeatherDay &n = d[k];
        int y = 56 + (k - 1) * 56;
        // "Domani" se lo è, altrimenti il giorno della settimana (con il numero negli altri giorni)
        struct tm wt;
        time_t when = add_days(sel_date, k);
        localtime_r(&when, &wt);
        if (off + k == 1) strlcpy(b, "Domani", sizeof(b));
        else if (off == 0) strlcpy(b, GIORNI[wt.tm_wday], sizeof(b));
        else snprintf(b, sizeof(b), "%s %d", GIORNI[wt.tm_wday], wt.tm_mday);
        label_at(days, &font_it_16, COL_TEXT_2, 20, y, b);
        fmt_daylight(a, sizeof(a), daylight(n));
        snprintf(buf, sizeof(buf), "luce %s", a);
        label_right(days, &font_it_16, COL_TEXT_2, 308, y, buf);
        snprintf(a, sizeof(a), "%02d:%02d", n.sunrise / 60, n.sunrise % 60);
        char e[8];
        snprintf(e, sizeof(e), "%02d:%02d", n.sunset / 60, n.sunset % 60);
        snprintf(buf, sizeof(buf), "%s \xE2\x80\x93 %s", a, e);
        label_at(days, &font_it_20, COL_TEXT, 20, y + 22, buf);
      }
    }

    // Luna: c'è sempre (la calcola la scheda); per un altro giorno a mezzogiorno di quel giorno
    MoonInfo m;
    time_t at = off == 0 ? now : sel_date + 12 * 3600;
    moon_info(at, &m);
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
    int to_full = days_until(at, m.to_full), to_new = days_until(at, m.to_new);
    bool full = m.phase == 0 || to_new == 0 ? true : m.phase == 4 || to_full == 0 ? false : to_full < to_new;
    int dd = full ? to_full : to_new;
    if (off != 0) {  // altri giorni: la data dell'evento
      time_t ev = add_days(sel_date, dd);
      struct tm et;
      localtime_r(&ev, &et);
      snprintf(buf, sizeof(buf), "Luna %s il %d %s", full ? "piena" : "nuova", et.tm_mday, MESI[et.tm_mon]);
    } else if (dd == 1) {
      snprintf(buf, sizeof(buf), "Luna %s domani", full ? "piena" : "nuova");
    } else {
      snprintf(buf, sizeof(buf), "Luna %s tra %d giorni", full ? "piena" : "nuova", dd);
    }
    label_at(card, &font_it_16, COL_TEXT_2, 20, 132, buf);
  }

  // Parte alta del pannello dell'aria: indice, pallino, giudizio, sottotitolo, scala con il segno
  static void air_top(lv_obj_t *panel, int aqi, const char *subtitle)
  {
    char buf[8];
    int level = air_level(aqi);
    snprintf(buf, sizeof(buf), "%d", aqi);
    // y 20: come la temperatura del meteo, il carattere ha la riga di 47 px invece di 72
    label_at(panel, &font_temp_64, COL_TEXT, 20, 20, buf);
    int x = 20 + text_w(&font_temp_64, buf) + 20;
    ui_make_box(panel, x, 16 + 17 - 7, 14, 14, aq_color(level), LV_RADIUS_CIRCLE);
    label_at(panel, &font_it_sb24, COL_TEXT, x + 14 + 10, 16, air_level_text(level));
    label_at(panel, &font_it_16, COL_TEXT_2, x, 50, subtitle);
    for (int i = 0; i < 6; i++) ui_make_box(panel, 420 + i * 39, 36, 36, 8, aq_color(i), 4);
    float pos = 420 + level * 39 + min((aqi - 20.0f * level) / 20.0f, 1.0f) * 36;
    make_marker(panel, (int)lroundf(pos), 30);
    label_at(panel, &font_it_16, COL_TEXT_3, 420, 52, "Buona");
    label_right(panel, &font_it_16, COL_TEXT_3, 652, 52, "Pessima");
    ui_make_box(panel, 20, 96, 632, 1, COL_RAISED);
  }

  // Inquinanti con il pallino del loro sottoindice
  static void pollutants_card(lv_obj_t *root, const float *values, const uint8_t *sub, const char *unit)
  {
    char buf[16];
    lv_obj_t *card = ui_make_box(root, CX(104), 296, 328, 168, COL_SURFACE, 12);
    label_at(card, &font_it_sb20, COL_TEXT, 20, 16, "Inquinanti");
    label_right(card, &font_it_16, COL_TEXT_3, 308, 19, unit);
    static const char *const NAMES[] = { "PM2,5", "PM10", "Biossido di azoto", "Ozono" };
    for (int i = 0; i < 4; i++) {
      int y = 56 + i * 28;
      ui_make_box(card, 20, y + 6, 10, 10, aq_color(air_level(sub[i])), LV_RADIUS_CIRCLE);
      label_at(card, &font_it_16, COL_TEXT_2, 40, y, NAMES[i]);
      fmt_decimal(buf, sizeof(buf), values[i]);
      label_right(card, &font_it_16, COL_TEXT, 308, y, buf);
    }
  }

  // Pollini presenti, dal livello più alto (a parità, dai grani più alti), al massimo 4
  static void pollen_card(lv_obj_t *root, const float *grains, const char *note, const char *none_sub, bool off_season)
  {
    lv_obj_t *card = ui_make_box(root, CX(448), 296, 328, 168, COL_SURFACE, 12);
    label_at(card, &font_it_sb20, COL_TEXT, 20, 16, "Pollini");
    int order[POLLEN_COUNT], n = 0;
    for (int p = 0; p < POLLEN_COUNT; p++)
      if (pollen_level((Pollen)p, grains[p]) >= 1) order[n++] = p;
    std::sort(order, order + n, [&](int x, int y) {
      int lx = pollen_level((Pollen)x, grains[x]), ly = pollen_level((Pollen)y, grains[y]);
      return lx != ly ? lx > ly : grains[x] > grains[y];
    });
    if (n > 0) {
      label_right(card, &font_it_16, COL_TEXT_3, 308, 19, note);
      static const char *const LEVELS[] = { "", "Bassa", "Media", "Alta" };
      for (int i = 0; i < min(n, 4); i++) {
        int p = order[i], lv = pollen_level((Pollen)p, grains[p]);
        int y = 56 + i * 28;
        ui_make_box(card, 20, y + 6, 10, 10, pollen_color(lv), LV_RADIUS_CIRCLE);
        label_at(card, &font_it_16, COL_TEXT_2, 40, y, pollen_name((Pollen)p));
        label_right(card, &font_it_16, COL_TEXT, 308, y, LEVELS[lv]);
      }
      return;
    }
    label_at(card, &font_ic_40, COL_TEXT_3, 20, 60, IC_SPA);
    label_at(card, &font_it_20, COL_TEXT, 80, 56, "Nessun polline");
    label_at(card, &font_it_16, COL_TEXT_2, 80, 84, off_season ? "Fuori stagione" : none_sub);
    if (off_season) label_at(card, &font_it_16, COL_TEXT_3, 20, 128, "I primi arrivano a febbraio.");
  }

  static bool off_season(time_t date)
  {
    struct tm t;
    localtime_r(&date, &t);
    return t.tm_mon >= 9 || t.tm_mon == 0;  // da ottobre a gennaio
  }

  void build_air(lv_obj_t *root, const Weather &w)
  {
    const WeatherAir &a = w.air;
    WeatherState st = air_state(a);
    if (st == WX_LOADING || st == WX_ERROR) {
      bool err = st == WX_ERROR;
      ui_state_block(root, 0, 712, IC_WIND, err ? COL_WARN : COL_TEXT_2,
                     err ? "Qualit\xC3\xA0 dell'aria non disponibile" : "Qualit\xC3\xA0 dell'aria in arrivo\xE2\x80\xA6",
                     // Dopo un errore weather.cpp riprova ogni 2 minuti: "tra poco" (SPEC_SOLE_ARIA §4.5)
                     err ? "Riprovo tra poco. Il meteo e la luna restano aggiornati." : nullptr);
      return;
    }
    char buf[96];
    lv_obj_t *panel = ui_make_box(root, CX(104), 72, 672, 216, COL_SURFACE, 12);
    air_top(panel, a.aqi, "Qualit\xC3\xA0 dell'aria adesso");
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
    const float values[] = { a.pm25, a.pm10, a.no2, a.o3 };
    pollutants_card(root, values, a.sub, "\xC2\xB5g/m\xC2\xB3");
    pollen_card(root, a.pollen_max, "prossime 24 ore", "Nelle prossime 24 ore", off_season(time(nullptr)));
  }

  // Aria di un altro giorno: il massimo del giorno al posto di "adesso", le 24 ore dalle 00:00 (§8.5)
  void build_air_day(lv_obj_t *root, const DayAir *d)
  {
    lv_obj_t *panel = ui_make_box(root, CX(104), 72, 672, 216, COL_SURFACE, 12);
    if (!d) {  // in arrivo
      skeleton_bars(panel, 20, 24, 2);
      for (int i = 0; i < 24; i++) ui_make_box(panel, 24 + i * 26, 160, 20, 22, COL_RAISED, 2);
      lv_obj_t *c1 = ui_make_box(root, CX(104), 296, 328, 168, COL_SURFACE, 12);
      skeleton_bars(c1, 20, 18, 5);
      lv_obj_t *c2 = ui_make_box(root, CX(448), 296, 328, 168, COL_SURFACE, 12);
      skeleton_bars(c2, 20, 18, 4);
      return;
    }
    int vmin = 999, vmax = -1, imin = 0, imax = 0, last = -1;
    for (int h = 0; h < 24; h++) {
      int v = d->hours[h].aqi;
      if (v == 255) continue;
      last = h;
      if (v < vmin) vmin = v, imin = h;
      if (v > vmax) vmax = v, imax = h;
    }
    char buf[96], t[8];
    snprintf(buf, sizeof(buf), "Massimo del giorno, alle %02d:00", imax);
    air_top(panel, max(vmax, 0), buf);
    if (last >= 0 && last < 23) snprintf(buf, sizeof(buf), "Ora per ora" DOT "fino alle %02d:00", last);
    else strlcpy(buf, "Ora per ora", sizeof(buf));
    label_at(panel, &font_it_16, COL_TEXT_2, 20, 108, buf);
    if (vmax >= 0) {
      snprintf(buf, sizeof(buf), "minimo %d alle %02d:00" DOT "massimo %d alle %02d:00", vmin, imin, vmax, imax);
      label_right(panel, &font_it_16, COL_TEXT_3, 652, 108, buf);
    }
    for (int h = 0; h < 24; h++) {
      int v = d->hours[h].aqi;
      if (v == 255) ui_make_box(panel, 24 + h * 26, 178, 20, 4, COL_RAISED);  // ora senza dato
      else {
        int bh = max(4, min(v, 100) * 48 / 100);
        ui_make_box(panel, 24 + h * 26, 182 - bh, 20, bh, aq_color(air_level(v)), 2);
      }
    }
    for (int k = 0; k < 4; k++) {
      snprintf(t, sizeof(t), "%02d:00", k * 6);
      lv_obj_t *l = label_at(panel, &font_it_16, COL_TEXT_3, 24 + k * 6 * 26 + 10 - 30, 186, t);
      lv_obj_set_width(l, 60);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    // Inquinanti all'ora del massimo, pollini sul massimo del giorno
    const DayAirHour &m = d->hours[imax];
    const float values[] = { m.pm25, m.pm10, m.no2, m.o3 };
    snprintf(buf, sizeof(buf), "\xC2\xB5g/m\xC2\xB3" DOT "alle %02d:00", imax);
    pollutants_card(root, values, m.sub, buf);
    float grains[POLLEN_COUNT] = {};
    for (int h = 0; h < 24; h++)
      for (int p = 0; p < POLLEN_COUNT; p++)
        if (d->hours[h].pollen[p] > grains[p]) grains[p] = d->hours[h].pollen[p];
    pollen_card(root, grains, "massimo del giorno", "In questo giorno", off_season(sel_date));
  }

public:
  explicit WeatherView(int t) : tab(t), sel_date(day_start(time(nullptr))) {}
  ~WeatherView() override { weather_day_clear(); }  // uscendo dalla pagina il giorno si dimentica (§8.2)
  Section section() const override { return SEC_NONE; }
  bool back_goes_home() const override { return true; }

  void build(lv_obj_t *root) override
  {
    sun_marker = nullptr;
    ui_make_back_button(root);
    // Schede al posto del titolo (SPEC_SOLE_ARIA §4.1, SPEC 6 §4.10; "Sole e luna" larga 144)
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
    built_today = day_start(time(nullptr));
    int off = offset();
    WeatherState wst = weather_state(w);
    WeatherState ast = air_state(w.air);
    built_state = tab == WT_AIR ? ast : wst;
    bool old = off == 0 && built_state == WX_OLD;
    uint32_t age = tab == WT_AIR ? millis() - w.air.updated_ms : millis() - w.updated_ms;
    build_selector(root, w, old, age);

    built_day_status = -1;
    if (off == 0) {
      request_at = 0;
      if (tab == WT_WEATHER) build_weather(root, w, wst);
      else if (tab == WT_AIR) build_air(root, w);
      else {
        bool data = wst == WX_OK || wst == WX_OLD;
        int di = data ? max(0, weather_day_index(w, time(nullptr))) : 0;
        WeatherDay d3[3];
        for (int k = 0; k < 3; k++) d3[k] = w.days[min(di + k, WEATHER_DAYS - 1)];
        build_sun(root, data ? d3 : nullptr, 0, -1, wst == WX_LOADING ? 1 : 2);
      }
      return;
    }

    char name[40], title[80];
    day_name(name, sizeof(name), sel_date, off, true);
    // Aria oltre i 4 giorni: si dice fin dove arriva la previsione (§8.6)
    if (tab == WT_AIR && off > 4) {
      time_t last = add_days(day_start(time(nullptr)), 4);
      char lname[40], button[48];
      day_name(lname, sizeof(lname), last, 4, true);
      snprintf(title, sizeof(title), "Previsioni dell'aria fino a %s", lname);
      snprintf(button, sizeof(button), "Vai a %s", lname);
      day_block(root, IC_WIND, COL_TEXT_3, title, "La qualit\xC3\xA0 dell'aria si prevede solo per i prossimi 4 giorni.",
                button, [](lv_event_t *e) { ((WeatherView *)lv_event_get_user_data(e))->set_day(4); });
      return;
    }

    DayWeather dw;
    if (!ui_day_air) ui_day_air = (DayAir *)heap_caps_malloc(sizeof(DayAir), MALLOC_CAP_SPIRAM);
    DayStatus ds = tab == WT_AIR ? weather_day_get_air(off, ui_day_air) : weather_day_get(off, &dw);
    built_day_status = ds;
    if (ds == DAY_NONE) request_at = millis() + 400;  // si chiede solo se il giorno resta scelto
    if (ds == DAY_LOADING && !loading_since) loading_since = millis();
    if (ds != DAY_LOADING && ds != DAY_NONE) loading_since = 0;
    bool error = ds == DAY_ERROR || timed_out;
    if (error && tab != WT_SUN) {
      snprintf(title, sizeof(title), tab == WT_AIR ? "Qualit\xC3\xA0 dell'aria di %s non disponibile"
                                                   : "Meteo di %s non disponibile", name);
      ui_state_block(root, 0, 712, IC_WARN, COL_WARN, title, "Controlla la connessione, poi riprova.", "Riprova",
                     retry_cb, this);
      return;
    }
    bool ok = ds == DAY_OK && !error;
    if (tab == WT_WEATHER) build_weather_day(root, ok ? &dw : nullptr, off);
    else if (tab == WT_AIR) build_air_day(root, ok ? ui_day_air : nullptr);
    else {
      int today_light = -1;
      if (wst == WX_OK || wst == WX_OLD) today_light = daylight(w.days[max(0, weather_day_index(w, time(nullptr)))]);
      build_sun(root, ok ? dw.days : nullptr, off, today_light, error ? 3 : 0);
    }
  }

  void gesture(lv_dir_t dir) override
  {
    // Scorrere cambia giorno (a destra il giorno prima, non indietro), senza animazione (§8.2)
    if (dir == LV_DIR_LEFT) set_day(offset() + 1);
    else if (dir == LV_DIR_RIGHT) set_day(offset() - 1);
  }

  void tick() override
  {
    // Richiesta del giorno 400 ms dopo l'ultimo cambio
    if (request_at && (int32_t)(millis() - request_at) >= 0) {
      request_at = 0;
      weather_day_request(kind(), offset());
      loading_since = millis();
    }
    if (++ticks % 5) return;  // due volte al secondo
    time_t now = time(nullptr);
    if (day_start(now) != built_today) {  // mezzanotte: i nomi dei giorni cambiano
      nav_rebuild();
      return;
    }
    if (sun_marker) {
      int q = minutes_now(now) / 15;
      if (q != marker_q) {
        marker_q = q;
        place(sun_marker, sun_x(q / 4.0f) - 10, 154);
      }
    }
    int off = offset();
    if (off != 0) {
      if (built_day_status < 0) return;
      DayStatus ds = tab == WT_AIR ? weather_day_get_air(off, nullptr) : weather_day_get(off, nullptr);
      // Più di 10 s senza risposta: errore
      if (ds == DAY_LOADING && loading_since && !timed_out && millis() - loading_since > 10000) {
        timed_out = true;
        nav_rebuild();
      } else if (ds != built_day_status && ds != DAY_NONE) {
        nav_rebuild();
      }
      return;
    }
    Weather w;
    weather_get(&w);
    if (tab == WT_WEATHER) {
      // La scheda Meteo di oggi resta com'è fino all'uscita (SPEC_METEO §4); si ricostruisce solo se mancavano i dati
      WeatherState st = weather_state(w);
      bool had = built_state == WX_OK || built_state == WX_OLD;
      if (!had && st != built_state) nav_rebuild();
      return;
    }
    // Sole e luna, Aria di oggi: si aggiornano all'arrivo di dati nuovi o al cambio di stato (SPEC_SOLE_ARIA §4.5)
    int st = tab == WT_SUN ? weather_state(w) : air_state(w.air);
    if (w.version != built_version || st != built_state) nav_rebuild();
  }

  void destroyed() override { sun_marker = nullptr; }
};

View *view_weather(int tab) { return new WeatherView(tab); }
