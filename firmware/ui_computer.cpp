#include "ui_computer.h"

#include <Arduino.h>
#include <time.h>
#include "ui_common.h"
#include "ui_list.h"
#include "ui_nav.h"

// Batteria da piena a vuota secondo il livello (§3): dall'88%, dal 63%, dal 38%, dal 13%, sotto
static const char *const BATTERY_ICONS[5] = { "\xEF\x89\x80", "\xEF\x89\x81", "\xEF\x89\x82", "\xEF\x89\x83", "\xEF\x89\x84" };

static const char *battery_icon(int pct)
{
  return BATTERY_ICONS[pct >= 88 ? 0 : pct >= 63 ? 1 : pct >= 38 ? 2 : pct >= 13 ? 3 : 4];
}

static lv_color_t level_color(uint8_t level, lv_color_t normal)
{
  return level == AL_CRITICAL ? COL_AQ_5 : level == AL_WARN ? COL_WARN : normal;
}

// "3 h 12 min", "40 min"
static void fmt_minutes(char *out, size_t n, int minutes)
{
  if (minutes >= 60) snprintf(out, n, "%d h %d min", minutes / 60, minutes % 60);
  else snprintf(out, n, "%d min", minutes);
}

// GB con la virgola: un decimale sotto i 100, intero sopra
static void fmt_gb(char *out, size_t n, float gb)
{
  if (gb < 100) snprintf(out, n, "%.1f", gb);
  else snprintf(out, n, "%.0f", gb);
  char *dot = strchr(out, '.');
  if (dot) *dot = ',';
}

// "Acceso da 2 giorni e 3 ore", "Acceso da 5 ore", "Acceso da 40 min"
static void fmt_uptime(char *out, size_t n, uint32_t s)
{
  uint32_t days = s / 86400, hours = s % 86400 / 3600;
  if (days > 0) {
    int k = snprintf(out, n, "Acceso da %lu %s", (unsigned long)days, days == 1 ? "giorno" : "giorni");
    if (hours > 0) snprintf(out + k, n - k, " e %lu %s", (unsigned long)hours, hours == 1 ? "ora" : "ore");
  } else if (hours > 0) {
    snprintf(out, n, "Acceso da %lu %s", (unsigned long)hours, hours == 1 ? "ora" : "ore");
  } else {
    snprintf(out, n, "Acceso da %lu min", (unsigned long)(s / 60));
  }
}

// "alle 18:40" se è di oggi, "ieri alle 18:40", "il 3 novembre" (§7)
static void fmt_last_seen(char *out, size_t n, const ComputerStatus &c)
{
  time_t now = time(nullptr);
  uint32_t ago = (millis() - c.at_ms) / 1000;
  if (now < 1700000000) {  // orologio non ancora sincronizzato
    snprintf(out, n, "%lu min fa", (unsigned long)(ago / 60));
    return;
  }
  time_t at = now - ago;
  struct tm t, today;
  localtime_r(&at, &t);
  localtime_r(&now, &today);
  time_t yest = now - 86400;
  struct tm y;
  localtime_r(&yest, &y);
  if (t.tm_yday == today.tm_yday && t.tm_year == today.tm_year) snprintf(out, n, "alle %02d:%02d", t.tm_hour, t.tm_min);
  else if (t.tm_yday == y.tm_yday && t.tm_year == y.tm_year) snprintf(out, n, "ieri alle %02d:%02d", t.tm_hour, t.tm_min);
  else snprintf(out, n, "il %d %s", t.tm_mday, MESI[t.tm_mon]);
}

bool computer_tile(const ComputerStatus &c, ComputerTile *out)
{
  if (!computer_reachable(c)) return false;
  char t[24];
  int top = computer_top_alert(c);
  out->alert = top >= 0;
  out->color = top >= 0 ? level_color(c.alert[top], COL_TEXT) : COL_TEXT;
  switch (top) {
    case CA_BATTERY:
      out->icon = battery_icon(c.battery);
      snprintf(out->value, sizeof(out->value), "Batteria %d%%", c.battery);
      if (c.remaining_min >= 0) {
        fmt_minutes(t, sizeof(t), c.remaining_min);
        snprintf(out->detail, sizeof(out->detail), "%s rimanenti", t);
      } else {
        strlcpy(out->detail, "collega l'alimentatore", sizeof(out->detail));
      }
      return true;
    case CA_THERMAL:
      out->icon = IC_THERMOMETER;
      strlcpy(out->value, "Fa caldo", sizeof(out->value));
      strlcpy(out->detail, "il Mac rallenta", sizeof(out->detail));
      return true;
    case CA_DISK:
      out->icon = IC_HDD;
      strlcpy(out->value, "Disco quasi pieno", sizeof(out->value));
      fmt_gb(t, sizeof(t), c.disk_free_gb);
      snprintf(out->detail, sizeof(out->detail), "%s GB liberi", t);
      return true;
    case CA_MEMORY:
      out->icon = IC_MEMORY;
      strlcpy(out->value, "Memoria quasi piena", sizeof(out->value));
      snprintf(out->detail, sizeof(out->detail), "%d%% libera", c.mem_free);
      return true;
    case CA_CPU:
      out->icon = IC_MICROCHIP;
      strlcpy(out->value, "Processore al massimo", sizeof(out->value));
      snprintf(out->detail, sizeof(out->detail), "da %lu min", (unsigned long)max(1UL, (unsigned long)c.cpu_high_s / 60));
      return true;
    case CA_HEADPHONES:
      out->icon = IC_HEADPHONES;
      snprintf(out->value, sizeof(out->value), "Cuffie %d%%", c.headphones_battery);
      strlcpy(out->detail, c.headphones, sizeof(out->detail));
      return true;
  }

  // Normale: la batteria, oppure "Tutto in ordine" per un computer fisso
  if (c.power == PW_NONE) {
    out->icon = IC_COMPUTER;
    strlcpy(out->value, "Tutto in ordine", sizeof(out->value));
    fmt_gb(t, sizeof(t), c.disk_free_gb);
    snprintf(out->detail, sizeof(out->detail), "disco %s GB liberi", t);
    return true;
  }
  bool full = c.power == PW_AC && c.battery >= 100;
  out->icon = full ? BATTERY_ICONS[0] : battery_icon(c.battery);
  snprintf(out->value, sizeof(out->value), "Batteria %d%%", c.battery);
  if (c.power == PW_CHARGING && c.remaining_min >= 0) {
    fmt_minutes(t, sizeof(t), c.remaining_min);
    snprintf(out->detail, sizeof(out->detail), "piena tra %s", t);
  } else if (c.power == PW_BATTERY && c.remaining_min >= 0) {
    fmt_minutes(t, sizeof(t), c.remaining_min);
    snprintf(out->detail, sizeof(out->detail), "%s rimanenti", t);
  } else {
    strlcpy(out->detail, c.power == PW_CHARGING ? "in carica" : c.power == PW_BATTERY ? "a batteria"
                         : full                 ? "carica completa"
                                                : "alimentatore collegato",
            sizeof(out->detail));
  }
  return true;
}

void computer_settings_text(const ComputerStatus &c, char *out, size_t n)
{
  if (!c.ever) {
    strlcpy(out, "Nessun computer collegato", n);
  } else if (computer_reachable(c)) {
    snprintf(out, n, "%s \xC2\xB7 collegato", c.name);
  } else {
    char when[32];
    fmt_last_seen(when, sizeof(when), c);
    snprintf(out, n, "%s \xC2\xB7 ultimo dato %s", c.name, when);
  }
}

// ---------- Pagina Computer (§6) ----------

// Barra con binario RAISED; il pieno cambia larghezza e colore solo se serve
struct Bar {
  lv_obj_t *fill = nullptr;
  int w = 0;
  void create(lv_obj_t *parent, int x, int y, int width, int h)
  {
    w = width;
    lv_obj_t *track = ui_make_box(parent, x, y, width, h, COL_RAISED, h / 2);
    fill = ui_make_box(track, 0, 0, 0, h, COL_TEXT_2, h / 2);
  }
  void set(int pct, lv_color_t color)
  {
    int fw = constrain(pct, 0, 100) * w / 100;
    if (lv_obj_get_width(fill) != fw) lv_obj_set_width(fill, fw);
    ui_set_hidden(fill, fw == 0);
    ui_set_bg(fill, color);
  }
};

// Scheda 216x140 di processore, memoria o disco (§6.2)
struct Card {
  lv_obj_t *icon, *value, *detail;
  Bar bar;
  void create(lv_obj_t *root, int x, const char *icon_text, const char *label)
  {
    lv_obj_t *box = ui_make_box(root, x, 216, 216, 140, COL_SURFACE, 12);
    icon = ui_make_label(box, &font_ic_20, COL_TEXT_2, icon_text);
    lv_obj_set_pos(icon, 16, 15);
    lv_point_t sz;
    lv_txt_get_size(&sz, icon_text, &font_ic_20, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_pos(ui_make_label(box, &font_it_16, COL_TEXT_2, label), 16 + sz.x + 8, 14);
    value = ui_make_label(box, &font_it_sb30, COL_TEXT, "");
    lv_obj_set_pos(value, 16, 44);
    bar.create(box, 16, 92, 184, 8);
    detail = ui_make_line_label(box, &font_it_16, COL_TEXT_3, 16, 108, 184);
  }
  void set(uint8_t level, int bar_pct)
  {
    ui_set_text_color(icon, level_color(level, COL_TEXT_2));
    ui_set_text_color(detail, level ? COL_TEXT_2 : COL_TEXT_3);
    bar.set(bar_pct, level_color(level, COL_TEXT_2));
  }
};

// Icona + etichetta nella prima riga di un pannello: l'etichetta parte gap px dopo l'icona
static lv_obj_t *icon_title(lv_obj_t *box, const lv_font_t *icon_font, const char *icon_text, int gap,
                            const lv_font_t *font, lv_color_t color, const char *title, int title_y)
{
  int x = icon_font == &font_ic_28 ? 20 : 16;
  lv_obj_t *icon = ui_make_label(box, icon_font, COL_TEXT_2, icon_text);
  lv_obj_set_pos(icon, x, 15);
  lv_point_t sz;
  lv_txt_get_size(&sz, icon_text, icon_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_obj_set_pos(ui_make_label(box, font, color, title), x + sz.x + gap, title_y);
  return icon;
}

enum ComputerMode { CM_NEVER, CM_UNREACHABLE, CM_DATA };

class ComputerView : public View {
  int mode = -1;
  bool fixed = false, has_headphones = false;
  uint32_t shown_version = 0;
  // Batteria
  lv_obj_t *bat_icon = nullptr, *bat_estimate, *bat_pct, *bat_state;
  Bar bat_bar;
  Card cpu, mem, disk;
  // Cuffie e sistema
  lv_obj_t *hp_name = nullptr, *hp_pct, *sys_icon, *sys_uptime, *sys_temp, *sys_dot;
  Bar hp_bar;

  static int mode_of(const ComputerStatus &c) { return !c.ever ? CM_NEVER : computer_reachable(c) ? CM_DATA : CM_UNREACHABLE; }

  // Stato in alto a destra, allineato a x 776: pallino 10 o avviso 20, 8 px, testo
  void make_status(lv_obj_t *root, bool connected, const char *text)
  {
    lv_obj_t *label = ui_make_label(root, &font_it_16, connected ? COL_TEXT_3 : COL_TEXT_2, text);
    lv_point_t sz;
    lv_txt_get_size(&sz, text, &font_it_16, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int x = CX(776) - sz.x;
    lv_obj_set_pos(label, x, 21);
    if (connected) {
      ui_make_box(root, x - 8 - 10, 21 + (sz.y - 10) / 2, 10, 10, COL_AQ_2, LV_RADIUS_CIRCLE);
    } else {
      lv_obj_t *warn = ui_make_label(root, &font_ic_20, COL_WARN, IC_WARN);
      lv_obj_set_pos(warn, x - 8 - 20, 21 + (sz.y - 20) / 2);
    }
  }

  void build_battery(lv_obj_t *root)
  {
    lv_obj_t *box = ui_make_box(root, CX(104), 72, 672, 136, COL_SURFACE, 12);
    if (fixed) {
      icon_title(box, &font_ic_28, IC_PLUG, 12, &font_it_sb20, COL_TEXT, "Alimentazione", 17);
      lv_obj_set_pos(ui_make_label(box, &font_it_20, COL_TEXT, "Senza batteria"), 20, 60);
      lv_obj_set_pos(ui_make_label(box, &font_it_16, COL_TEXT_2, "Computer fisso, collegato alla corrente."), 20, 88);
      return;
    }
    bat_icon = icon_title(box, &font_ic_28, BATTERY_ICONS[0], 12, &font_it_sb20, COL_TEXT, "Batteria", 17);
    bat_estimate = ui_make_label(box, &font_it_16, COL_TEXT_2, "");
    lv_obj_set_width(bat_estimate, 300);
    lv_obj_set_style_text_align(bat_estimate, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(bat_estimate, 652 - 300, 19);
    bat_pct = ui_make_label(box, &font_temp_64, COL_TEXT, "");
    lv_obj_set_pos(bat_pct, 20, 64);  // 52 della SPEC + 12: le cifre cadono dove sono nel mockup
    bat_bar.create(box, 216, 80, 436, 12);
    bat_state = ui_make_label(box, &font_it_16, COL_TEXT_2, "");
    lv_obj_set_pos(bat_state, 216, 100);
  }

  void build_bottom(lv_obj_t *root)
  {
    lv_obj_t *hp = ui_make_box(root, CX(104), 364, 332, 100, COL_SURFACE, 12);
    icon_title(hp, &font_ic_20, IC_HEADPHONES, 8, &font_it_16, COL_TEXT_2, "Cuffie", 14);
    if (has_headphones) {
      hp_name = ui_make_line_label(hp, &font_it_sb20, COL_TEXT, 16, 44, 220);
      hp_pct = ui_make_label(hp, &font_it_sb20, COL_TEXT, "");
      lv_obj_set_width(hp_pct, 80);
      lv_obj_set_style_text_align(hp_pct, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(hp_pct, 316 - 80, 44);
      hp_bar.create(hp, 16, 80, 300, 8);
    } else {
      lv_obj_set_pos(ui_make_label(hp, &font_it_20, COL_TEXT_2, "Nessuna cuffia"), 16, 44);
      lv_obj_set_pos(ui_make_label(hp, &font_it_16, COL_TEXT_3, "Le AirPods mostrano qui la batteria."), 16, 72);
    }

    lv_obj_t *sys = ui_make_box(root, CX(444), 364, 332, 100, COL_SURFACE, 12);
    sys_icon = icon_title(sys, &font_ic_20, IC_THERMOMETER, 8, &font_it_16, COL_TEXT_2, "Sistema", 14);
    sys_uptime = ui_make_line_label(sys, &font_it_sb20, COL_TEXT, 16, 44, 300);
    sys_dot = ui_make_box(sys, 16, 72 + 5, 10, 10, COL_WARN, LV_RADIUS_CIRCLE);
    sys_temp = ui_make_label(sys, &font_it_16, COL_TEXT_3, "");
  }

  void refresh(const ComputerStatus &c)
  {
    char t[48], m[24];
    if (bat_icon) {
      uint8_t lv = c.alert[CA_BATTERY];
      bool full = c.power == PW_AC && c.battery >= 100;
      ui_set_text_raw(bat_icon, full ? BATTERY_ICONS[0] : battery_icon(c.battery));
      ui_set_text_color(bat_icon, level_color(lv, COL_TEXT_2));
      t[0] = '\0';
      if (c.remaining_min >= 0 && (c.power == PW_BATTERY || c.power == PW_CHARGING)) {
        fmt_minutes(m, sizeof(m), c.remaining_min);
        snprintf(t, sizeof(t), c.power == PW_BATTERY ? "%s rimanenti" : "piena tra %s", m);
      }
      ui_set_text_raw(bat_estimate, t);
      ui_set_text(bat_pct, "%d%%", c.battery);
      bat_bar.set(c.battery, lv ? level_color(lv, COL_TEXT_2) : c.power == PW_CHARGING ? COL_AQ_2 : COL_TEXT_2);
      ui_set_text_raw(bat_state, c.power == PW_CHARGING ? "In carica"
                                 : c.power == PW_BATTERY ? "A batteria"
                                 : full                  ? "Carica completa"
                                                         : "Alimentatore collegato \xC2\xB7 non in carica");
    }

    ui_set_text(cpu.value, "%d%%", c.cpu);
    if (c.alert[CA_CPU]) ui_set_text(cpu.detail, "al massimo da %lu min", (unsigned long)max(1UL, (unsigned long)c.cpu_high_s / 60));
    else ui_set_text_raw(cpu.detail, "in uso");
    cpu.set(c.alert[CA_CPU], c.cpu);

    int used = c.mem_free >= 0 ? 100 - c.mem_free : 0;
    ui_set_text(mem.value, "%d%%", used);
    ui_set_text(mem.detail, "in uso \xC2\xB7 %d%% libera", c.mem_free);
    mem.set(c.alert[CA_MEMORY], used);

    fmt_gb(m, sizeof(m), c.disk_free_gb);
    ui_set_text(disk.value, "%s GB", m);
    ui_set_text(disk.detail, "liberi su %.0f GB", c.disk_total_gb);
    disk.set(c.alert[CA_DISK], c.disk_total_gb > 0 ? (int)(100 - c.disk_free_gb * 100 / c.disk_total_gb + 0.5f) : 0);

    if (hp_name) {
      ui_set_text_raw(hp_name, c.headphones);
      ui_set_text(hp_pct, "%d%%", c.headphones_battery);
      hp_bar.set(c.headphones_battery, level_color(c.alert[CA_HEADPHONES], COL_TEXT_2));
    }

    fmt_uptime(t, sizeof(t), c.uptime_s);
    ui_set_text_raw(sys_uptime, t);
    ui_set_text_color(sys_icon, c.thermal ? COL_WARN : COL_TEXT_2);
    ui_set_hidden(sys_dot, !c.thermal);
    ui_set_text_raw(sys_temp, c.thermal ? "Rallenta per il caldo" : "Temperatura normale");
    ui_set_text_color(sys_temp, c.thermal ? COL_TEXT : COL_TEXT_3);
    lv_obj_set_pos(sys_temp, c.thermal ? 34 : 16, 72);
  }

public:
  Section section() const override { return SEC_NONE; }
  bool back_goes_home() const override { return true; }

  void build(lv_obj_t *root) override
  {
    ComputerStatus c;
    computer_get(&c);
    mode = mode_of(c);
    fixed = c.power == PW_NONE;
    has_headphones = c.headphones[0] && c.headphones_battery >= 0;
    shown_version = c.version;

    ui_make_back_button(root);
    lv_obj_t *title = ui_make_line_label(root, &font_it_sb24, COL_TEXT, CX(156), 17, 364);
    ui_set_text_raw(title, mode == CM_NEVER ? "Computer" : c.name);

    if (mode == CM_NEVER) {
      ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, "Nessun computer collegato",
                     "Avvia sul computer il programma che manda i dati alla scheda. Computer e scheda devono essere "
                     "sulla stessa rete Wi-Fi.");
      return;
    }
    if (mode == CM_UNREACHABLE) {
      char when[32], status[48], title_text[96], text[160];
      fmt_last_seen(when, sizeof(when), c);
      snprintf(status, sizeof(status), "ultimo dato %s", when);
      make_status(root, false, status);
      snprintf(title_text, sizeof(title_text), "%s non raggiungibile", c.name);
      snprintf(text, sizeof(text), "Ultimo dato %s. Il computer \xC3\xA8 spento, in stop o su un'altra rete Wi-Fi.", when);
      ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, title_text, text);
      return;
    }

    make_status(root, true, "collegato");
    build_battery(root);
    cpu.create(root, CX(104), IC_MICROCHIP, "Processore");
    mem.create(root, CX(332), IC_MEMORY, "Memoria");
    disk.create(root, CX(560), IC_HDD, "Disco");
    build_bottom(root);
    refresh(c);
  }

  void tick() override
  {
    ComputerStatus c;
    computer_get(&c);
    // Cambio di stato o di forma della pagina (fisso, cuffie): si ricostruisce; altrimenti solo i valori
    bool hp = c.headphones[0] && c.headphones_battery >= 0;
    int md = mode_of(c);
    if (md != mode || (md == CM_DATA && (fixed != (c.power == PW_NONE) || hp != has_headphones))) {
      nav_rebuild();
      return;
    }
    if (md == CM_DATA && c.version != shown_version) {
      shown_version = c.version;
      refresh(c);
    }
  }

  void destroyed() override
  {
    bat_icon = nullptr;
    hp_name = nullptr;
  }
};

View *view_computer() { return new ComputerView(); }
