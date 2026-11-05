#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include "net.h"
#include "spotify.h"
#include "ui_common.h"
#include "ui_nav.h"
#include "ui_player.h"
#include "ui_views.h"
#include "ui_weather.h"

const char *const GIORNI[] = { "Domenica", "Luned\xC3\xAC", "Marted\xC3\xAC", "Mercoled\xC3\xAC", "Gioved\xC3\xAC", "Venerd\xC3\xAC", "Sabato" };
const char *const MESI[] = { "gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno",
                             "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre" };

static lv_obj_t *panel_portal, *panel_saved, *panel_connecting, *panel_clock, *panel_player, *panel_update;

static lv_obj_t *portal_qr, *portal_net_label, *portal_pass_label;
static lv_obj_t *saved_label;
static lv_obj_t *update_label;
static lv_obj_t *connecting_label, *connecting_hint;
static lv_obj_t *clock_time, *clock_date, *clock_status, *clock_spotify, *clock_wifi;
static lv_obj_t *clock_buttons, *clock_icons, *clock_queue_icon, *clock_queue_btn;
static DeviceChip clock_chip;

static char shown_ap_pass[16];

// ---------- Cambia Wi-Fi ----------

static void forget_msgbox_cb(lv_event_t *e)
{
  lv_obj_t *mbox = lv_event_get_current_target(e);
  const char *btn = lv_msgbox_get_active_btn_text(mbox);
  if (btn && strcmp(btn, "Conferma") == 0) {
    net_request_forget();
    lv_label_set_text(lv_msgbox_get_text(mbox), "Riavvio in corso...");
    return;
  }
  lv_msgbox_close(mbox);
}

static void forget_btn_cb(lv_event_t *e)
{
  static const char *btns[] = { "Annulla", "Conferma", "" };
  lv_obj_t *mbox = lv_msgbox_create(NULL, "Cambia Wi-Fi",
                                    "La scheda dimentica la rete salvata e si riavvia "
                                    "in modalità configurazione.",
                                    btns, false);
  lv_obj_set_width(mbox, 460);
  lv_obj_set_style_text_font(mbox, &font_it_20, 0);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, forget_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void make_forget_button(lv_obj_t *parent, int x, int y)
{
  lv_obj_t *btn = ui_make_button(parent, BTN_SECONDARY, x, y, 176, 48, nullptr, "Cambia Wi-Fi", forget_btn_cb);
  lv_obj_set_style_border_color(btn, COL_PLACEHOLDER_FG, 0);
}

// ---------- Pannelli ----------

static void create_portal(lv_obj_t *scr)
{
  panel_portal = ui_make_panel(scr);

  lv_obj_t *title = ui_make_label(panel_portal, &font_it_sb30, COL_TEXT, "Configura il Wi-Fi");
  lv_obj_set_pos(title, 40, 30);

  lv_obj_t *s1 = ui_make_label(panel_portal, &font_it_20, COL_TEXT,
                            "1. Inquadra il QR code con la fotocamera\n    del telefono, oppure collegati a mano:");
  lv_obj_set_pos(s1, 40, 95);

  portal_net_label = ui_make_label(panel_portal, &font_it_20, COL_TEXT_2, "");
  lv_obj_set_pos(portal_net_label, 70, 160);
  portal_pass_label = ui_make_label(panel_portal, &font_it_24, COL_ACCENT, "");
  lv_obj_set_pos(portal_pass_label, 70, 192);

  lv_obj_t *s2 = ui_make_label(panel_portal, &font_it_20, COL_TEXT,
                            "2. Si apre una pagina: scegli la rete di casa\n    e scrivi la sua password\n\n"
                            "3. Premi \"Salva e collega\"");
  lv_obj_set_pos(s2, 40, 250);

  lv_obj_t *hint = ui_make_label(panel_portal, &font_it_16, COL_TEXT_2,
                              "Se la pagina non si apre da sola, apri il browser\nsu http://192.168.4.1");
  lv_obj_set_pos(hint, 40, 400);

  // QR su riquadro bianco: serve un bordo chiaro perché la fotocamera lo legga
  lv_obj_t *qr_bg = lv_obj_create(panel_portal);
  lv_obj_set_size(qr_bg, 290, 290);
  lv_obj_align(qr_bg, LV_ALIGN_RIGHT_MID, -40, 0);
  lv_obj_set_style_bg_color(qr_bg, lv_color_white(), 0);
  lv_obj_set_style_radius(qr_bg, 12, 0);
  lv_obj_set_style_border_width(qr_bg, 0, 0);
  lv_obj_clear_flag(qr_bg, LV_OBJ_FLAG_SCROLLABLE);
  portal_qr = lv_qrcode_create(qr_bg, 250, lv_color_black(), lv_color_white());
  lv_obj_center(portal_qr);
}

static void create_saved(lv_obj_t *scr)
{
  panel_saved = ui_make_panel(scr);
  saved_label = ui_make_label(panel_saved, &font_it_sb30, COL_TEXT, "");
  lv_obj_set_style_text_align(saved_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(saved_label);
}

// Aggiornamento via Wi-Fi: stesso stile della schermata "Rete salvata"
static void create_update(lv_obj_t *scr)
{
  panel_update = ui_make_panel(scr);
  update_label = ui_make_label(panel_update, &font_it_sb30, COL_TEXT, "");
  lv_obj_set_style_text_align(update_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(update_label, LV_ALIGN_CENTER, 0, -20);
  lv_obj_t *hint = ui_make_label(panel_update, &font_it_20, COL_TEXT_2, "Non spegnere la scheda: si riavvia da sola.");
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 40);
}

static void create_connecting(lv_obj_t *scr)
{
  panel_connecting = ui_make_panel(scr);
  lv_obj_t *spinner = lv_spinner_create(panel_connecting, 1000, 60);
  lv_obj_set_size(spinner, 90, 90);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -70);
  lv_obj_set_style_arc_color(spinner, COL_ACCENT, LV_PART_INDICATOR);

  connecting_label = ui_make_label(panel_connecting, &font_it_sb30, COL_TEXT, "");
  lv_obj_align(connecting_label, LV_ALIGN_CENTER, 0, 30);

  connecting_hint = ui_make_label(panel_connecting, &font_it_20, COL_TEXT_2,
                               "Ci sta mettendo tanto: controlla che il router sia acceso.\n"
                               "Se hai cambiato password, premi \"Cambia Wi-Fi\".");
  lv_obj_set_style_text_align(connecting_hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(connecting_hint, LV_ALIGN_CENTER, 0, 100);
  lv_obj_add_flag(connecting_hint, LV_OBJ_FLAG_HIDDEN);

  make_forget_button(panel_connecting, 600, 420);
}

// Orologio: SPEC 6 §5.2 e SPEC 7 §3.5, rivisto in due colonne con il meteo (SPEC_METEO §3)
static void create_clock(lv_obj_t *scr)
{
  panel_clock = ui_make_panel(scr);

  // Chip del dispositivo e icone in alto: solo con Spotify collegato
  clock_icons = lv_obj_create(panel_clock);
  lv_obj_remove_style_all(clock_icons);
  lv_obj_set_size(clock_icons, 800, 64);
  lv_obj_clear_flag(clock_icons, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  clock_chip.create(clock_icons);
  ui_make_top_icons(clock_icons, &clock_queue_icon, &clock_queue_btn);
  // Coda disabilitata: non c'è niente in riproduzione
  ui_set_text_color(clock_queue_icon, COL_DISABLED_FG_2);
  lv_obj_clear_flag(clock_queue_btn, LV_OBJ_FLAG_CLICKABLE);

  // Font nativo grande: niente transform_zoom, che con il Wi-Fi attivo faceva glitchare lo schermo.
  // y 91: il carattere ha la riga di 81 px (non 120), così le cifre partono a y 92 come nel mockup
  clock_time = ui_make_label(panel_clock, &font_clock_112, COL_TEXT, "--:--");
  lv_obj_set_pos(clock_time, 40, 91);

  clock_date = ui_make_line_label(panel_clock, &font_it_20, COL_TEXT_2, 44, 196, 380);
  ui_set_text_raw(clock_date, "Sincronizzo l'ora...");

  clock_weather_create(panel_clock);

  clock_buttons = lv_obj_create(panel_clock);
  lv_obj_remove_style_all(clock_buttons);
  lv_obj_set_size(clock_buttons, 420, 48);
  lv_obj_set_pos(clock_buttons, 0, 412);  // SPEC_SOLE_ARIA §3.1: 20 px più in basso per la riga nuova
  lv_obj_clear_flag(clock_buttons, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  ui_make_button(clock_buttons, BTN_PRIMARY, 40, 0, 176, 48, IC_HOME, "Home",
                 [](lv_event_t *e) { nav_open_section(SEC_HOME); });
  ui_make_button(clock_buttons, BTN_SECONDARY, 232, 0, 176, 48, IC_SEARCH, "Cerca",
                 [](lv_event_t *e) { nav_open_section(SEC_SEARCH); });

  // Spotify non collegato: al posto dei pulsanti, che servono solo con Spotify (il saluto non c'è più)
  clock_spotify = ui_make_label(panel_clock, &font_it_16, COL_TEXT_2, "");
  lv_label_set_long_mode(clock_spotify, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(clock_spotify, 340);
  lv_obj_set_pos(clock_spotify, 40, 414);

  // Wi-Fi: icona + nome allineati a destra fino a x 568 (posizioni in update_clock)
  clock_wifi = ui_make_label(panel_clock, &font_ic_20, COL_TEXT_3, IC_WIFI);
  clock_status = ui_make_label(panel_clock, &font_it_16, COL_TEXT_3, "");
  lv_label_set_long_mode(clock_status, LV_LABEL_LONG_DOT);

  make_forget_button(panel_clock, 584, 412);
}

// ---------- Aggiornamento ----------

static lv_obj_t *shown_panel = nullptr;

static void show_only(lv_obj_t *panel)
{
  if (panel == shown_panel) return;
  shown_panel = panel;
  lv_obj_t *all[] = { panel_portal, panel_saved, panel_connecting, panel_clock, panel_player, panel_update, nav_panel() };
  for (lv_obj_t *p : all) ui_set_hidden(p, p != panel);
}

static void update_clock(const NetStatus &st, const SpotifyStatus &sp)
{
  if (!st.time_ok) {
    ui_set_text_raw(clock_time, "--:--");
    ui_set_text_raw(clock_date, "Sincronizzo l'ora...");
  } else {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    ui_set_text(clock_time, "%02d:%02d", t.tm_hour, t.tm_min);
    ui_set_text(clock_date, "%s %d %s", GIORNI[t.tm_wday], t.tm_mday, MESI[t.tm_mon]);
  }
  if (strcmp(lv_label_get_text(clock_status), st.ssid) != 0) {
    lv_label_set_text(clock_status, st.ssid);
    lv_point_t sz;
    lv_txt_get_size(&sz, st.ssid, &font_it_16, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int w = min(144, (int)sz.x);
    lv_obj_set_size(clock_status, w, 22);
    lv_obj_set_pos(clock_status, 568 - w, 425);
    lv_txt_get_size(&sz, IC_WIFI, &font_ic_20, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_pos(clock_wifi, 568 - w - 8 - sz.x, 426);
  }

  bool ready = sp.state == SP_READY;
  ui_set_hidden(clock_icons, !ready);
  ui_set_hidden(clock_buttons, !ready);
  ui_set_hidden(clock_spotify, ready);
  clock_weather_update(ready);
  if (ready) {
    PlayerState p;
    spotify_get_player(&p);
    clock_chip.update(p);
  }

  lv_color_t col = COL_TEXT_2;
  switch (sp.state) {
    case SP_NO_CREDENTIALS:
      ui_set_text_raw(clock_spotify, "Spotify da collegare: lancia lo script di login sul computer");
      col = COL_TEXT_2;
      break;
    case SP_WAITING_NETWORK:
    case SP_CONNECTING:
      ui_set_text_raw(clock_spotify, "Spotify: mi collego...");
      col = COL_TEXT_2;
      break;
    case SP_READY:
      break;
    case SP_LOGIN_REQUIRED:
      ui_set_text_raw(clock_spotify, "Spotify: accesso scaduto, rifai il login dal computer");
      col = COL_WARN;
      break;
    case SP_ERROR:
      ui_set_text(clock_spotify, "Spotify: %s", sp.error);
      col = COL_WARN;
      break;
  }
  ui_set_text_color(clock_spotify, col);
}

static void ui_timer_cb(lv_timer_t *t)
{
  NetStatus st;
  net_get_status(&st);

  switch (st.state) {
    case NET_PORTAL:
      show_only(panel_portal);
      if (strcmp(shown_ap_pass, st.ap_pass) != 0) {
        strlcpy(shown_ap_pass, st.ap_pass, sizeof(shown_ap_pass));
        lv_label_set_text_fmt(portal_net_label, "Rete:  %s", st.ap_ssid);
        lv_label_set_text_fmt(portal_pass_label, "Password:  %s", st.ap_pass);
        char qr[96];
        snprintf(qr, sizeof(qr), "WIFI:T:WPA;S:%s;P:%s;;", st.ap_ssid, st.ap_pass);
        lv_qrcode_update(portal_qr, qr, strlen(qr));
      }
      break;
    case NET_SAVED:
      show_only(panel_saved);
      ui_set_text(saved_label, "Rete salvata: %s\nRiavvio in corso...", st.ssid);
      break;
    case NET_UPDATING:
      show_only(panel_update);
      ui_set_text(update_label, "Aggiornamento in corso... %d%%", st.ota_percent);
      break;
    case NET_CONNECTING: {
      show_only(panel_connecting);
      uint32_t secs = (millis() - st.connecting_since_ms) / 1000;
      ui_set_text(connecting_label, "Mi collego a %s...", st.ssid);
      ui_set_hidden(connecting_hint, secs < 20);
      break;
    }
    case NET_CONNECTED: {
      SpotifyStatus sp;
      spotify_get_status(&sp);
      // Navigazione (fase 6) se aperta; altrimenti il player con qualcosa in riproduzione (anche in pausa),
      // oppure l'orologio
      if (sp.state == SP_READY && nav_browsing()) {
        show_only(nav_panel());
      } else if (sp.state == SP_READY && player_panel_update()) {
        show_only(panel_player);
      } else {
        show_only(panel_clock);
        update_clock(st, sp);
      }
      if (sp.state == SP_READY) nav_tick();
      break;
    }
    default:
      show_only(nullptr);
      break;
  }
}

void ui_create()
{
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  create_portal(scr);
  create_saved(scr);
  create_connecting(scr);
  create_update(scr);
  create_clock(scr);
  panel_player = player_panel_create(scr);
  nav_create(scr);

  lv_timer_create(ui_timer_cb, 100, NULL);
}
