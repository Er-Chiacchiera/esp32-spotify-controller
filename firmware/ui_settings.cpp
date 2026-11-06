// Impostazioni (SPEC_HOME_GIORNI §6, SPEC_COMPUTER §4, SPEC_CITTA §3): righe 712x80 da y 64, una per argomento. Barra con la Home accesa.
#include <Arduino.h>
#include <esp_app_desc.h>
#include "computer.h"
#include "net.h"
#include "weather.h"
#include "ui.h"
#include "ui_cities.h"
#include "ui_common.h"
#include "ui_computer.h"
#include "ui_list.h"
#include "ui_views.h"

class SettingsView : public View {
  Header header;
  lv_obj_t *wifi_sub = nullptr, *computer_sub = nullptr, *cities_sub = nullptr, *info_sub = nullptr;
  lv_obj_t *pair_btn = nullptr, *pair_label = nullptr;
  uint32_t ticks = 0;

  lv_obj_t *make_row(lv_obj_t *root, int i, const char *icon, const char *title, lv_event_cb_t cb = nullptr)
  {
    lv_obj_t *row;
    if (cb) {  // tutta la riga si tocca (premuta: fondo PRESSED)
      row = ui_make_touch(root, 0, 64 + i * 80, 712, 80, COL_PRESSED);
      lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, nullptr);
    } else {
      row = ui_make_box(root, 0, 64 + i * 80, 712, 80, COL_BG);
    }
    lv_obj_t *circle = ui_make_box(row, 16, 12, 56, 56, COL_SURFACE, LV_RADIUS_CIRCLE);
    lv_obj_center(ui_make_label(circle, &font_ic_28, COL_TEXT_2, icon));
    lv_obj_set_pos(ui_make_label(row, &font_it_sb20, COL_TEXT, title), 88, 15);
    return row;
  }

  void refresh()
  {
    NetStatus st;
    net_get_status(&st);
    if (st.state == NET_CONNECTED) {
      // Oltre -65 dBm buono, fra -65 e -78 discreto, sotto debole
      const char *q = st.rssi > -65 ? "buono" : st.rssi >= -78 ? "discreto" : "debole";
      ui_set_text(wifi_sub, "%s \xC2\xB7 segnale %s", st.ssid, q);
    } else {
      ui_set_text_raw(wifi_sub, "Non collegato");
    }
    ComputerStatus c;
    computer_get(&c);
    char text[96];
    computer_settings_text(c, text, sizeof(text));
    ui_set_text_raw(computer_sub, text);
    // "Abbina" / "Abbina di nuovo" solo con il Mac raggiungibile e il programma nuovo (SPEC_CONTROLLO_MAC §7)
    bool pairable = computer_reachable(c) && c.can_command;
    ui_set_hidden(pair_btn, !pairable);
    lv_obj_set_width(computer_sub, pairable ? 410 : 600);
    if (pairable) ui_set_text_raw(pair_label, c.paired ? "Abbina di nuovo" : "Abbina");
    // "Posizione attuale (Roma) · 4 città salvate", "Milano · 4 città salvate", "Posizione attuale · non trovata"
    AutoLocation al;
    weather_auto_get(&al);
    int n = cities_count(), d = cities_default();
    char def[80], saved[32] = "";
    City city;
    if (d >= 0 && cities_get(d, &city)) strlcpy(def, city.name, sizeof(def));
    else if (al.state == AUTO_OK) snprintf(def, sizeof(def), "Posizione attuale (%s)", al.city);
    else if (al.state == AUTO_FAILED) strlcpy(def, "Posizione attuale \xC2\xB7 non trovata", sizeof(def));
    else strlcpy(def, "Posizione attuale", sizeof(def));
    if (n) snprintf(saved, sizeof(saved), " \xC2\xB7 %d %s", n, n == 1 ? "citt\xC3\xA0 salvata" : "citt\xC3\xA0 salvate");
    ui_set_text(cities_sub, "%s%s", def, saved);
    char sha[17];
    esp_app_get_elf_sha256(sha, sizeof(sha));
    if (st.state == NET_CONNECTED) ui_set_text(info_sub, "Firmware %s \xC2\xB7 IP %s", sha, st.ip);
    else ui_set_text(info_sub, "Firmware %s", sha);
  }

public:
  Section section() const override { return SEC_NONE; }
  bool back_goes_home() const override { return true; }

  void build(lv_obj_t *root) override
  {
    header = Header();
    header.create(root, true, "Impostazioni");
    lv_obj_t *wifi = make_row(root, 0, IC_WIFI, "Wi-Fi");
    wifi_sub = ui_make_line_label(wifi, &font_it_16, COL_TEXT_2, 88, 43, 410);
    lv_obj_t *btn = ui_make_button(wifi, BTN_SECONDARY, 512, 16, 176, 48, nullptr, "Cambia Wi-Fi",
                                   [](lv_event_t *e) { ui_ask_forget_wifi(); });
    lv_obj_set_style_border_color(btn, COL_PLACEHOLDER_FG, 0);
    lv_obj_t *computer = make_row(root, 1, IC_LAPTOP, "Computer", [](lv_event_t *e) { nav_push(view_computer()); });
    computer_sub = ui_make_line_label(computer, &font_it_16, COL_TEXT_2, 88, 43, 600);
    pair_btn = ui_make_button(computer, BTN_SECONDARY, 512, 16, 176, 48, nullptr, "Abbina",
                              [](lv_event_t *e) {
                                ComputerStatus c;
                                computer_get(&c);
                                nav_push(view_computer(CT_COMMANDS, c.paired));  // "Abbina di nuovo" ha anche Annulla
                              });
    lv_obj_set_style_border_color(pair_btn, COL_PLACEHOLDER_FG, 0);
    pair_label = lv_obj_get_child(pair_btn, 0);
    lv_obj_t *cities = make_row(root, 2, IC_MARKER, "Citt\xC3\xA0 del meteo",
                                [](lv_event_t *e) { nav_push(view_cities(false, nav_depth())); });
    cities_sub = ui_make_line_label(cities, &font_it_16, COL_TEXT_2, 88, 43, 600);
    lv_obj_t *info = make_row(root, 3, IC_INFO, "Informazioni");
    info_sub = ui_make_line_label(info, &font_it_16, COL_TEXT_2, 88, 43, 600);
    refresh();
  }

  void tick() override
  {
    if (++ticks % 20 == 0) refresh();  // ogni 2 secondi: segnale e indirizzo possono cambiare
  }

  void destroyed() override
  {
    header = Header();
    wifi_sub = computer_sub = cities_sub = info_sub = pair_btn = pair_label = nullptr;
  }
};

View *view_settings() { return new SettingsView(); }
