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
    snprintf(out, n, c.can_command && !c.paired ? "%s \xC2\xB7 collegato, da abbinare" : "%s \xC2\xB7 collegato", c.name);
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
// Scheda Comandi (SPEC_CONTROLLO_MAC §5): se capitano più cose insieme vince la prima
enum CommandsMode { CMD_NEVER, CMD_UNREACHABLE, CMD_ASLEEP, CMD_OLD, CMD_PAIR, CMD_PAIRED_OK, CMD_READY };

bool computer_home_button(const ComputerStatus &c, const char **icon, lv_color_t *color)
{
  if (!computer_reachable(c) || !c.can_command || !c.paired || c.asleep) return false;
  if (c.mic_muted) *icon = IC_MIC_SLASH, *color = COL_WARN;
  else if (c.out_muted) *icon = IC_VOLUME_MUTE, *color = COL_TEXT_2;
  else *icon = IC_LAPTOP, *color = COL_TEXT_2;
  return true;
}

static int text_width(const char *text, const lv_font_t *font)
{
  lv_point_t sz;
  lv_txt_get_size(&sz, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return sz.x;
}

// Cerchio 56 con icona 28 e lo spinner 20 al posto dell'icona mentre un comando è in corso
struct Circle {
  lv_obj_t *box, *icon, *spin;
  void create(lv_obj_t *parent, int x, int y, const char *icon_text)
  {
    box = ui_make_box(parent, x, y, 56, 56, COL_RAISED, LV_RADIUS_CIRCLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    icon = ui_make_label(box, &font_ic_28, COL_TEXT, icon_text);
    lv_obj_center(icon);
    spin = nullptr;
  }
  // Fondo, icona e colore; pending mostra lo spinner (sul colore se il fondo non è RAISED)
  void set(lv_color_t bg, const char *icon_text, lv_color_t fg, bool pending)
  {
    ui_set_bg(box, bg);
    ui_set_text_raw(icon, icon_text);
    ui_set_text_color(icon, fg);
    ui_set_hidden(icon, pending);
    if (pending && !spin) {
      bool on_color = lv_color_to32(bg) != lv_color_to32(COL_RAISED);
      spin = ui_make_spinner(box, 20, on_color);
      if (lv_color_to32(bg) == lv_color_to32(COL_WARN)) lv_obj_set_style_arc_color(spin, lv_color_hex(0x804C00), LV_PART_MAIN);
      lv_obj_center(spin);
    } else if (!pending && spin) {
      lv_obj_del(spin);
      spin = nullptr;
    }
  }
};

// Interruttore 332x140 (§3.3): tutta la scheda è un pulsante
struct Toggle {
  lv_obj_t *card, *state, *detail;
  Circle circle;
  void create(lv_obj_t *root, int x, const char *icon, const char *title, lv_event_cb_t cb, void *user)
  {
    card = ui_make_touch(root, CX(x), 216, 332, 140, COL_SURFACE_PRESSED);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    // Bordo sempre largo 1 (i figli contano dal bordo), visibile solo sulla scheda arancio; le posizioni sono
    // quelle della SPEC meno 1
    lv_obj_set_style_border_color(card, COL_WARN, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, LV_STATE_PRESSED);  // premuta: il bordo sparisce
    lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, user);
    circle.create(card, 15, 15, icon);
    lv_obj_t *t = ui_make_label(card, &font_it_sb20, COL_TEXT, title);
    lv_obj_set_pos(t, 87, 18);
    state = ui_make_label(card, &font_it_16, COL_TEXT_2, "");
    lv_obj_set_pos(state, 87, 46);
    detail = ui_make_label(card, &font_it_16, COL_TEXT_3, "");
    lv_obj_set_pos(detail, 15, 87);
    lv_obj_set_width(detail, 300);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
  }
  void set(bool warn, lv_color_t circle_bg, const char *icon, lv_color_t icon_color, bool on, const char *state_text,
           const char *detail_text, bool pending)
  {
    lv_obj_set_style_bg_color(card, warn ? COL_WARN_BG : COL_SURFACE, 0);
    lv_obj_set_style_border_opa(card, warn ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    circle.set(circle_bg, icon, icon_color, pending);
    ui_set_text_raw(state, state_text);
    ui_set_text_color(state, on ? COL_TEXT : COL_TEXT_2);
    ui_set_text_raw(detail, detail_text);
    ui_set_text_color(detail, warn ? COL_TEXT_2 : COL_TEXT_3);
  }
};

class ComputerView : public View {
  int tab;
  bool repair;  // codice di un abbinamento nuovo anche se già abbinato
  int mode = -1;
  bool header_reachable = false, header_asleep = false;
  bool fixed = false, has_headphones = false;
  uint32_t shown_version = 0;
  uint16_t seen_failed = 0, seen_done = 0, seen_renewed = 0, seen_pair_done = 0;
  uint32_t ok_until = 0;  // fine della schermata "abbinamento riuscito"
  // Batteria
  lv_obj_t *bat_icon = nullptr, *bat_estimate, *bat_pct, *bat_state;
  Bar bat_bar;
  Card cpu, mem, disk;
  // Cuffie e sistema
  lv_obj_t *hp_name = nullptr, *hp_pct, *sys_icon, *sys_uptime, *sys_temp, *sys_dot;
  Bar hp_bar;
  // Comandi (§3)
  lv_obj_t *vol_out = nullptr, *slider, *vol_value;
  Circle mute;
  Toggle mic, awake;
  lv_obj_t *actions, *confirm, *confirm_q, *confirm_text, *confirm_btn, *confirm_label, *confirm_spin;
  Circle confirm_circle;
  int confirm_kind = -1;     // MC_SCREEN o MC_SLEEP con la conferma aperta
  uint32_t confirm_at = 0;   // apertura della conferma: il pulsante verde risponde dopo 0,5 s
  bool confirm_sent = false;
  bool dragging = false;
  uint32_t hold_until = 0;   // dopo il rilascio del cursore i valori del Mac aspettano la risposta
  // Abbinamento (§6)
  lv_obj_t *code_a = nullptr, *code_b, *code_until;

  static int status_mode(const ComputerStatus &c) { return !c.ever ? CM_NEVER : computer_reachable(c) ? CM_DATA : CM_UNREACHABLE; }

  int commands_mode(const ComputerStatus &c) const
  {
    if (!c.ever) return CMD_NEVER;
    if (!computer_reachable(c)) return CMD_UNREACHABLE;
    if (c.asleep) return CMD_ASLEEP;
    if (!c.can_command) return CMD_OLD;
    if (ok_until) return CMD_PAIRED_OK;
    if (!c.paired || repair) return CMD_PAIR;
    return CMD_READY;
  }

  int mode_of(const ComputerStatus &c) const { return tab == CT_STATUS ? status_mode(c) : commands_mode(c); }

  // ---------- Intestazione (§3.1) ----------

  static void tab_cb(lv_event_t *e)
  {
    ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i == v->tab) return;
    v->tab = i;  // sul posto: nessun passo in più nella pila indietro
    nav_rebuild();
  }

  // A destra, allineato a x 776: pallino AQ_2 + nome del Mac, oppure avviso + ultimo dato; niente dopo lo stop
  void build_header(lv_obj_t *root, const ComputerStatus &c)
  {
    ui_make_back_button(root);
    static const char *const TABS[] = { "Stato", "Comandi" };
    static const int TX[] = { 152, 256 }, TW[] = { 104, 128 };
    for (int i = 0; i < 2; i++) {
      lv_obj_t *t = ui_make_touch(root, CX(TX[i]), 0, TW[i], 64, COL_BG, false);
      lv_obj_t *l = ui_make_label(t, &font_it_sb20, i == tab ? COL_TEXT : COL_TEXT_3, TABS[i]);
      lv_obj_set_style_text_color(l, COL_TEXT, LV_STATE_PRESSED);
      lv_obj_center(l);
      lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
      if (i == tab) ui_make_box(t, (TW[i] - 56) / 2, 55, 56, 3, COL_ACCENT);
      lv_obj_add_event_cb(t, tab_cb, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(t, (void *)(intptr_t)i);
    }

    header_reachable = computer_reachable(c);
    header_asleep = c.asleep;
    if (!c.ever || (header_reachable && c.asleep)) return;
    char text[64];
    if (header_reachable) {
      strlcpy(text, c.name, sizeof(text));
    } else {
      char when[32];
      fmt_last_seen(when, sizeof(when), c);
      snprintf(text, sizeof(text), "ultimo dato %s", when);
    }
    lv_obj_t *label = ui_make_label(root, &font_it_16, header_reachable ? COL_TEXT_3 : COL_TEXT_2, text);
    int w = min(text_width(text, &font_it_16), 368 - 18);
    lv_obj_set_width(label, w);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    int x = CX(776) - w, h = lv_font_get_line_height(&font_it_16);
    lv_obj_set_pos(label, x, 21);
    if (header_reachable) {
      ui_make_box(root, x - 8 - 10, 21 + (h - 10) / 2, 10, 10, COL_AQ_2, LV_RADIUS_CIRCLE);
    } else {
      lv_obj_t *warn = ui_make_label(root, &font_ic_20, COL_WARN, IC_WARN);
      lv_obj_set_pos(warn, x - 8 - 20, 21 + (h - 20) / 2);
    }
  }

  // ---------- Scheda Stato (SPEC_COMPUTER §6) ----------

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

  void refresh_status(const ComputerStatus &c)
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

  void build_status(lv_obj_t *root, const ComputerStatus &c)
  {
    if (mode == CM_NEVER) {
      ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, "Nessun computer collegato",
                     "Avvia sul computer il programma che manda i dati alla scheda. Computer e scheda devono essere "
                     "sulla stessa rete Wi-Fi.");
      return;
    }
    if (mode == CM_UNREACHABLE) {
      char when[32], title_text[96], text[160];
      fmt_last_seen(when, sizeof(when), c);
      snprintf(title_text, sizeof(title_text), "%s non raggiungibile", c.name);
      snprintf(text, sizeof(text), "Ultimo dato %s. Il computer \xC3\xA8 spento, in stop o su un'altra rete Wi-Fi.", when);
      ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, title_text, text);
      return;
    }
    build_battery(root);
    cpu.create(root, CX(104), IC_MICROCHIP, "Processore");
    mem.create(root, CX(332), IC_MEMORY, "Memoria");
    disk.create(root, CX(560), IC_HDD, "Disco");
    build_bottom(root);
    refresh_status(c);
  }

  // ---------- Scheda Comandi (§3) ----------

  static void mute_cb(lv_event_t *e)
  {
    ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
    ComputerStatus c;
    computer_get(&c);
    if (c.cmd_pending & (1 << MC_MUTE)) return;
    computer_command(MC_MUTE, !c.out_muted);
    v->refresh_commands(c, true);
  }

  static void slider_cb(lv_event_t *e)
  {
    ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    int value = lv_slider_get_value(v->slider);
    if (code == LV_EVENT_PRESSED) {
      v->dragging = true;
      v->hold_until = millis() + 1500;
      computer_command(MC_VOLUME, value);  // anche un tocco senza trascinare sposta il volume
    } else if (code == LV_EVENT_VALUE_CHANGED && v->dragging) {
      computer_command(MC_VOLUME, value);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      v->dragging = false;
      v->hold_until = millis() + 1500;
      computer_command(MC_VOLUME, value);  // l'ultimo valore parte sempre
    } else {
      return;
    }
    v->show_volume(value, false);
  }

  static void toggle_cb(lv_event_t *e)
  {
    ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
    bool is_mic = lv_event_get_current_target(e) == v->mic.card;
    MacCommand cmd = is_mic ? MC_MIC : MC_AWAKE;
    ComputerStatus c;
    computer_get(&c);
    if (c.cmd_pending & (1 << cmd)) return;  // un tocco alla volta
    computer_command(cmd, is_mic ? !c.mic_muted : !c.awake);
    computer_get(&c);
    v->refresh_commands(c, true);
  }

  static void action_cb(lv_event_t *e)
  {
    ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
    v->open_confirm((intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e)));
  }

  static void confirm_cb(lv_event_t *e)
  {
    ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
    if (v->confirm_sent || millis() - v->confirm_at < 500) return;  // un doppio tocco veloce non conferma
    v->confirm_sent = true;
    computer_command((MacCommand)v->confirm_kind);
    ui_set_hidden(v->confirm_label, true);
    v->confirm_spin = ui_make_spinner(v->confirm_btn, 20, true);
  }

  void show_volume(int value, bool muted)
  {
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_color_t fill = muted ? COL_DISABLED_FG : COL_TEXT;
    lv_obj_set_style_bg_color(slider, fill, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, fill, LV_PART_KNOB);
    if (muted) {
      ui_set_text_raw(vol_value, "Muto");
      lv_obj_set_style_text_font(vol_value, &font_it_sb20, 0);
      ui_set_text_color(vol_value, COL_TEXT_2);
      lv_obj_set_y(vol_value, 79);
    } else {
      ui_set_text(vol_value, "%d", value);
      lv_obj_set_style_text_font(vol_value, &font_it_sb30, 0);
      ui_set_text_color(vol_value, COL_TEXT);
      lv_obj_set_y(vol_value, 73);
    }
  }

  // Pannello Volume del Mac 104,72 672x136 (§3.2)
  void build_volume(lv_obj_t *root)
  {
    lv_obj_t *box = ui_make_box(root, CX(104), 72, 672, 136, COL_SURFACE, 12);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);  // il dito vicino al cursore non tocca altro
    lv_obj_t *icon = ui_make_label(box, &font_ic_28, COL_TEXT, IC_LAPTOP);
    lv_obj_set_pos(icon, 20, 15);
    lv_obj_set_pos(ui_make_label(box, &font_it_sb20, COL_TEXT, "Volume del Mac"), 20 + text_width(IC_LAPTOP, &font_ic_28) + 12, 17);
    vol_out = ui_make_line_label(box, &font_it_16, COL_TEXT_2, 652 - 300, 19, 300);
    lv_obj_set_style_text_align(vol_out, LV_TEXT_ALIGN_RIGHT, 0);

    // Pulsante muto: il cerchio stesso, premuto KEY_PRESSED
    mute.create(box, 20, 64, IC_VOLUME);
    lv_obj_add_flag(mute.box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(mute.box, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_bg_color(mute.box, COL_KEY_PRESSED, LV_STATE_PRESSED);
    lv_obj_add_event_cb(mute.box, mute_cb, LV_EVENT_CLICKED, this);

    slider = lv_slider_create(box);
    lv_obj_remove_style_all(slider);
    lv_obj_set_pos(slider, 96, 88);
    lv_obj_set_size(slider, 472, 8);
    lv_slider_set_range(slider, 0, 100);
    lv_obj_set_ext_click_area(slider, 24);  // tocco da 64 a 120, come la SPEC
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, COL_RAISED, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 10, LV_PART_KNOB);  // pomello 28 sul binario da 8
    for (lv_event_code_t code : { LV_EVENT_PRESSED, LV_EVENT_VALUE_CHANGED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST })
      lv_obj_add_event_cb(slider, slider_cb, code, this);

    vol_value = ui_make_label(box, &font_it_sb30, COL_TEXT, "");
    lv_obj_set_width(vol_value, 64);
    lv_obj_set_style_text_align(vol_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(vol_value, 652 - 64, 73);
  }

  // Azioni 332x100 (§3.4) e conferma 672x100 al loro posto
  void build_actions(lv_obj_t *root)
  {
    actions = lv_obj_create(root);
    lv_obj_remove_style_all(actions);
    lv_obj_set_pos(actions, CX(104), 364);
    lv_obj_set_size(actions, 672, 100);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    static const char *const ICONS[] = { IC_COMPUTER, IC_MOON };
    static const char *const TITLES[] = { "Spegni lo schermo", "Metti in stop" };
    static const char *const DETAILS[] = { "Lo riaccendi dal Mac", "Si sveglia solo dal Mac" };
    for (int i = 0; i < 2; i++) {
      lv_obj_t *card = ui_make_touch(actions, i * 340, 0, 332, 100, COL_SURFACE_PRESSED);
      lv_obj_set_style_radius(card, 12, 0);
      lv_obj_set_style_bg_color(card, COL_SURFACE, 0);
      lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
      lv_obj_set_user_data(card, (void *)(intptr_t)(i == 0 ? MC_SCREEN : MC_SLEEP));
      lv_obj_add_event_cb(card, action_cb, LV_EVENT_CLICKED, this);
      Circle circle;
      circle.create(card, 16, 22, ICONS[i]);
      lv_obj_set_pos(ui_make_label(card, &font_it_sb20, COL_TEXT, TITLES[i]), 88, 24);
      lv_obj_t *d = ui_make_line_label(card, &font_it_16, COL_TEXT_3, 88, 52, 228);
      ui_set_text_raw(d, DETAILS[i]);
    }

    confirm = ui_make_box(root, CX(104), 364, 672, 100, COL_SURFACE, 12);
    lv_obj_set_style_border_width(confirm, 1, 0);
    lv_obj_set_style_border_color(confirm, COL_PLACEHOLDER_FG, 0);
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_CLICKABLE);
    confirm_circle.create(confirm, 15, 21, IC_MOON);  // 16,22 meno il bordo
    confirm_q = ui_make_line_label(confirm, &font_it_sb20, COL_TEXT, 87, 23, 264);
    confirm_text = ui_make_line_label(confirm, &font_it_16, COL_TEXT_2, 87, 51, 264);
    lv_obj_t *cancel = ui_make_button(confirm, BTN_SECONDARY, 351, 21, 136, 56, nullptr, "Annulla",
                                      [](lv_event_t *e) { ((ComputerView *)lv_event_get_user_data(e))->close_confirm(); }, this);
    lv_obj_set_style_border_color(cancel, COL_PLACEHOLDER_FG, 0);
    confirm_btn = ui_make_button(confirm, BTN_PRIMARY, 499, 21, 156, 56, nullptr, "Spegni", confirm_cb, this);
    confirm_label = lv_obj_get_child(confirm_btn, 0);
    confirm_spin = nullptr;
    ui_set_hidden(confirm, true);
  }

  void open_confirm(int kind)
  {
    confirm_kind = kind;
    confirm_at = millis();
    confirm_sent = false;
    bool sleep = kind == MC_SLEEP;
    confirm_circle.set(COL_RAISED, sleep ? IC_MOON : IC_COMPUTER, COL_TEXT, false);
    ui_set_text_raw(confirm_q, sleep ? "Mettere in stop il Mac?" : "Spegnere lo schermo?");
    ui_set_text_raw(confirm_text, sleep ? "Si sveglia solo dal Mac." : "Lo riaccendi dal Mac.");
    lv_label_set_text(confirm_label, sleep ? "Metti in stop" : "Spegni");
    ui_set_hidden(confirm_label, false);
    ui_set_hidden(actions, true);
    ui_set_hidden(confirm, false);
  }

  void close_confirm()
  {
    if (confirm_kind < 0) return;
    confirm_kind = -1;
    if (confirm_spin) lv_obj_del(confirm_spin);
    confirm_spin = nullptr;
    ui_set_hidden(confirm, true);
    ui_set_hidden(actions, false);
  }

  void build_commands_ready(lv_obj_t *root, const ComputerStatus &c)
  {
    build_volume(root);
    mic.create(root, 104, IC_MIC, "Microfono", toggle_cb, this);
    awake.create(root, 444, IC_COFFEE, "Tieni sveglio", toggle_cb, this);
    build_actions(root);
    refresh_commands(c, true);
  }

  // force: ridisegna anche i valori del Mac (dopo un tocco l'interruttore mostra subito lo spinner)
  void refresh_commands(const ComputerStatus &c, bool force)
  {
    if (!vol_out) return;
    ui_set_text_raw(vol_out, c.output);
    bool hold = dragging || millis() < hold_until;
    if (!hold && c.volume >= 0) show_volume(c.volume, c.out_muted);
    bool mute_pending = c.cmd_pending & (1 << MC_MUTE);
    bool muted = hold ? false : c.out_muted;  // il cursore toccato toglie il muto
    mute.set(muted ? COL_TEXT : COL_RAISED, muted ? IC_VOLUME_MUTE : IC_VOLUME, muted ? COL_ON_ACCENT : COL_TEXT, mute_pending);

    bool mic_pending = c.cmd_pending & (1 << MC_MIC);
    if (c.mic_muted) {
      mic.set(true, COL_WARN, IC_MIC_SLASH, COL_ON_ACCENT, true, "Muto",
              "Nessuno ti sente, anche se Teams mostra il microfono acceso.", mic_pending);
    } else {
      mic.set(false, COL_RAISED, IC_MIC, COL_TEXT, false, "Acceso", c.input, mic_pending);
    }
    bool awake_pending = c.cmd_pending & (1 << MC_AWAKE);
    if (c.awake) {
      awake.set(false, COL_ACCENT, IC_COFFEE, COL_ON_ACCENT, true, "Acceso", "Il Mac non va in stop e non spegne lo schermo.",
                awake_pending);
    } else {
      awake.set(false, COL_RAISED, IC_COFFEE, COL_TEXT, false, "Spento", "Il Mac va in stop da solo, come sempre.",
                awake_pending);
    }
  }

  // ---------- Abbinamento (§6) ----------

  void show_code(const ComputerStatus &c)
  {
    if (!code_a) return;
    char a[4], b[4];
    memcpy(a, c.code, 3), a[3] = '\0';
    memcpy(b, c.code + 3, 3), b[3] = '\0';
    lv_label_set_text(code_a, a);
    lv_label_set_text(code_b, b);
    int wa = text_width(a, &font_temp_64), wb = text_width(b, &font_temp_64);
    int x = (320 - wa - 32 - wb) / 2;
    lv_obj_align(code_a, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_align(code_b, LV_ALIGN_LEFT_MID, x + wa + 32, 0);
    if (c.code_until) {
      struct tm t;
      localtime_r(&c.code_until, &t);
      ui_set_text(code_until, "Valido fino alle %02d:%02d", t.tm_hour, t.tm_min);
    } else {
      ui_set_text_raw(code_until, "Valido 10 minuti");
    }
  }

  static lv_obj_t *center_label(lv_obj_t *root, const lv_font_t *font, lv_color_t color, const char *text, int y, int w)
  {
    lv_obj_t *l = ui_make_label(root, font, color, text);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(l, (712 - w) / 2, y);
    return l;
  }

  void build_pair(lv_obj_t *root, const ComputerStatus &c)
  {
    computer_pair_open(false);
    ComputerStatus now;
    computer_get(&now);
    seen_renewed = now.code_renewed;
    seen_pair_done = now.pair_done;
    lv_obj_t *circle = ui_make_box(root, 316, 80, 80, 80, COL_SURFACE, LV_RADIUS_CIRCLE);
    lv_obj_center(ui_make_label(circle, &font_ic_40, COL_TEXT_2, IC_LINK));
    center_label(root, &font_it_sb20, COL_TEXT, "Abbina la scheda al Mac", 176, 664);
    center_label(root, &font_it_16, COL_TEXT_2,
                 "Per comandare il Mac da qui, scrivi questo codice sul Mac quando il programma della scheda te lo "
                 "chiede.",
                 208, 520);
    lv_obj_t *box = ui_make_box(root, CX(284), 264, 320, 88, COL_SURFACE, 12);
    code_a = ui_make_label(box, &font_temp_64, COL_TEXT, "");
    code_b = ui_make_label(box, &font_temp_64, COL_TEXT, "");
    code_until = center_label(root, &font_it_16, COL_TEXT_3, "", 362, 664);
    show_code(now);

    lv_event_cb_t renew = [](lv_event_t *e) {
      computer_pair_open(true);
      ComputerView *v = (ComputerView *)lv_event_get_user_data(e);
      ComputerStatus s;
      computer_get(&s);
      v->seen_renewed = s.code_renewed;
      v->show_code(s);
    };
    if (repair) {
      lv_obj_t *cancel = ui_make_button(root, BTN_SECONDARY, CX(258), 396, 160, 56, nullptr, "Annulla",
                                        [](lv_event_t *e) { nav_back(); });
      lv_obj_set_style_border_color(cancel, COL_PLACEHOLDER_FG, 0);
    }
    lv_obj_t *fresh = ui_make_button(root, BTN_SECONDARY, CX(repair ? 430 : 344), 396, 200, 56, nullptr, "Nuovo codice",
                                     renew, this);
    lv_obj_set_style_border_color(fresh, COL_PLACEHOLDER_FG, 0);
  }

  void build_paired_ok(lv_obj_t *root, const ComputerStatus &c)
  {
    char title[80];
    snprintf(title, sizeof(title), "Abbinata a %s", c.name);
    lv_obj_t *block = ui_state_block(root, 0, 712, IC_CHECK, COL_ON_ACCENT, title, "Da adesso puoi comandare il Mac da qui.");
    ui_set_bg(lv_obj_get_child(block, 0), COL_ACCENT);
    lv_obj_update_layout(block);
    lv_obj_t *col = lv_obj_get_child(block, 1);
    lv_obj_t *text = lv_obj_get_child(col, 1);
    int y = lv_obj_get_y(col) + lv_obj_get_y(text) + lv_obj_get_height(text) + 24;
    ui_make_button(root, BTN_PRIMARY, (712 - 200) / 2, y, 200, 56, nullptr, "Vai ai comandi", [](lv_event_t *e) {
      ((ComputerView *)lv_event_get_user_data(e))->ok_until = 0;
      nav_rebuild();
    }, this);
  }

  void build_commands(lv_obj_t *root, const ComputerStatus &c)
  {
    char title[96];
    switch (mode) {
      case CMD_NEVER:
        ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, "Nessun computer collegato",
                       "Avvia sul computer il programma che manda i dati alla scheda. Computer e scheda devono essere "
                       "sulla stessa rete Wi-Fi.");
        return;
      case CMD_UNREACHABLE: {
        char when[32], text[160];
        fmt_last_seen(when, sizeof(when), c);
        snprintf(title, sizeof(title), "%s non raggiungibile", c.name);
        snprintf(text, sizeof(text), "Ultimo dato %s. I comandi tornano quando il Mac \xC3\xA8 acceso e sulla stessa rete Wi-Fi.",
                 when);
        ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, title, text);
        return;
      }
      case CMD_ASLEEP:
        snprintf(title, sizeof(title), "%s \xC3\xA8 in stop", c.name);
        ui_state_block(root, 0, 712, IC_MOON, COL_TEXT_3, title,
                       "Si sveglia solo dal Mac. I comandi tornano quando si riaccende.");
        return;
      case CMD_OLD:
        ui_state_block(root, 0, 712, IC_LAPTOP, COL_TEXT_3, "Programma del Mac da aggiornare",
                       "La versione sul Mac manda i dati ma non riceve comandi. Aggiornala dal computer: i comandi "
                       "compaiono da soli.");
        return;
      case CMD_PAIR: build_pair(root, c); return;
      case CMD_PAIRED_OK: build_paired_ok(root, c); return;
      default: build_commands_ready(root, c); return;
    }
  }

public:
  ComputerView(int t, bool r) : tab(t), repair(r) {}
  ~ComputerView() override { computer_pair_close(); }  // uscendo dalla pagina il codice non vale più
  Section section() const override { return SEC_NONE; }
  bool back_goes_home() const override { return true; }

  void build(lv_obj_t *root) override
  {
    ComputerStatus c;
    computer_get(&c);
    if (ok_until && millis() > ok_until) ok_until = 0;
    mode = mode_of(c);
    if (mode != CMD_PAIR || tab != CT_COMMANDS) computer_pair_close();
    fixed = c.power == PW_NONE;
    has_headphones = c.headphones[0] && c.headphones_battery >= 0;
    shown_version = c.version;
    seen_failed = c.cmd_failed;
    seen_done = c.cmd_done;
    seen_pair_done = c.pair_done;
    seen_renewed = c.code_renewed;
    confirm_kind = -1;
    dragging = false;

    build_header(root, c);
    if (tab == CT_STATUS) build_status(root, c);
    else build_commands(root, c);
  }

  void tick() override
  {
    ComputerStatus c;
    computer_get(&c);
    // Abbinamento appena riuscito: 3 s di conferma, poi i comandi (§5)
    if (c.pair_done != seen_pair_done) {
      seen_pair_done = c.pair_done;
      if (tab == CT_COMMANDS && mode == CMD_PAIR) {
        repair = false;
        ok_until = millis() + 3000;
      }
    }
    if (ok_until && millis() > ok_until) ok_until = 0;

    // Cambio di stato o di forma della pagina: si ricostruisce; altrimenti solo i valori
    bool hp = c.headphones[0] && c.headphones_battery >= 0;
    int md = mode_of(c);
    bool reshape = md != mode || computer_reachable(c) != header_reachable || (c.asleep != header_asleep);
    if (tab == CT_STATUS && md == CM_DATA) reshape |= fixed != (c.power == PW_NONE) || hp != has_headphones;
    if (reshape && !dragging) {
      nav_rebuild();
      return;
    }

    if (c.cmd_failed != seen_failed) {
      seen_failed = c.cmd_failed;
      nav_toast("Il Mac non ha risposto. Riprova.", true);
      close_confirm();
    }
    if (c.cmd_done != seen_done) {
      seen_done = c.cmd_done;
      if (c.cmd_done_kind == MC_SCREEN) {
        nav_toast("Schermo del Mac spento");
        close_confirm();
      }
    }
    if (tab == CT_COMMANDS && mode == CMD_PAIR && c.code_renewed != seen_renewed) {
      seen_renewed = c.code_renewed;
      nav_toast("Codice scaduto: ne ho fatto uno nuovo.", true);
      show_code(c);
    }
    // La conferma si chiude da sola dopo 5 s senza tocchi
    if (confirm_kind >= 0 && !confirm_sent && millis() - confirm_at > 5000) close_confirm();

    if (c.version != shown_version) {
      shown_version = c.version;
      if (tab == CT_STATUS && md == CM_DATA) refresh_status(c);
      if (tab == CT_COMMANDS && md == CMD_READY) refresh_commands(c, false);
    } else if (tab == CT_COMMANDS && md == CMD_READY && !dragging && hold_until && millis() >= hold_until) {
      hold_until = 0;  // finita l'attesa dopo il cursore: si torna ai valori del Mac
      refresh_commands(c, false);
    }
  }

  void destroyed() override
  {
    bat_icon = nullptr;
    hp_name = nullptr;
    vol_out = nullptr;
    code_a = nullptr;
    confirm_spin = nullptr;
    mute.spin = mic.circle.spin = awake.circle.spin = confirm_circle.spin = nullptr;
  }
};

View *view_computer(int tab, bool repair) { return new ComputerView(tab, repair); }
