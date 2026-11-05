#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include "net.h"
#include "spotify.h"
#include "lvgl_v8_port.h"
#include "ui_common.h"
#include "ui_list.h"
#include "ui_nav.h"
#include "ui_player.h"
#include "ui_views.h"
#include "ui_weather.h"

const char *const GIORNI[] = { "Domenica", "Luned\xC3\xAC", "Marted\xC3\xAC", "Mercoled\xC3\xAC", "Gioved\xC3\xAC", "Venerd\xC3\xAC", "Sabato" };
const char *const MESI[] = { "gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno",
                             "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre" };

static lv_obj_t *panel_portal, *panel_saved, *panel_connecting, *panel_home, *panel_player, *panel_update;

static lv_obj_t *portal_qr, *portal_net_label, *portal_pass_label;
static lv_obj_t *saved_label;
static lv_obj_t *update_label;
static lv_obj_t *connecting_label, *connecting_hint;
static lv_obj_t *home_time, *home_date, *home_wifi_dot;
static DeviceChip home_chip;
// Riquadro Spotify della Home
static lv_obj_t *sp_tile, *sp_title, *sp_sub, *sp_note, *sp_controls, *sp_play, *sp_play_icon, *sp_idle, *sp_skeleton;
static ImageBox sp_cover;
static uint32_t sp_play_hold_until = 0;

static const uint32_t HOME_AFTER_MS = 2 * 60 * 1000;  // come la luce al 15%
static bool player_open = false;     // Now playing aperto dall'utente
static bool ever_connected = false;  // il Wi-Fi ha funzionato almeno una volta da questa accensione

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

// ---------- Home (SPEC_HOME_GIORNI §3): ora, riquadro Spotify, riquadro Meteo e riquadri piccoli ----------

static void play_cb(lv_event_t *e);

static void create_home(lv_obj_t *scr)
{
  panel_home = ui_make_panel(scr);

  // Chip del dispositivo e icone Ascolti, Libreria, Cerca, Impostazioni (§3.1)
  home_chip.create(panel_home);
  ui_make_top_icons(panel_home, false, nullptr, &home_wifi_dot);

  // Font nativo grande: niente transform_zoom, che con il Wi-Fi attivo faceva glitchare lo schermo.
  // y 79: il carattere ha la riga di 81 px (non 120), così le cifre stanno dove le mette il mockup (ora a 40,60)
  home_time = ui_make_label(panel_home, &font_clock_112, COL_TEXT, "--:--");
  lv_obj_set_pos(home_time, 40, 79);
  home_date = ui_make_line_label(panel_home, &font_it_20, COL_TEXT_2, 44, 184, 340);
  ui_set_text_raw(home_date, "Sincronizzo l'ora...");

  // Riquadro Spotify 400,72 360x152 (§3.3): il tocco fuori dai pulsanti apre Now playing
  sp_tile = ui_make_touch(panel_home, 400, 72, 360, 152, COL_SURFACE_PRESSED);
  lv_obj_set_style_radius(sp_tile, 12, 0);
  lv_obj_set_style_bg_color(sp_tile, COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(sp_tile, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(sp_tile, [](lv_event_t *e) { ui_open_player(); }, LV_EVENT_CLICKED, nullptr);
  sp_cover.create(sp_tile, 16, 16, 64, false, IC_NOTE);
  sp_title = ui_make_line_label(sp_tile, &font_it_sb20, COL_TEXT, 96, 18, 248);
  sp_sub = ui_make_line_label(sp_tile, &font_it_16, COL_TEXT_2, 96, 46, 248);
  sp_note = ui_make_line_label(sp_tile, &font_it_16, COL_TEXT_3, 16, 104, 328);

  // ⏮ ⏯ ⏭ centrati sul riquadro (play a x 180); premuto RAISED, perché PRESSED sul SURFACE non si vede
  sp_controls = lv_obj_create(sp_tile);
  lv_obj_remove_style_all(sp_controls);
  lv_obj_set_size(sp_controls, 360, 64);
  lv_obj_set_pos(sp_controls, 0, 84);
  lv_obj_clear_flag(sp_controls, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *prev = ui_make_icon_button(sp_controls, 84, 0, IC_PREV, COL_TEXT,
                                       [](lv_event_t *e) { spotify_command(CMD_PREVIOUS); });
  lv_obj_set_style_bg_color(lv_obj_get_child(prev, 0), COL_RAISED, 0);
  lv_obj_t *next = ui_make_icon_button(sp_controls, 212, 0, IC_NEXT, COL_TEXT,
                                       [](lv_event_t *e) { spotify_command(CMD_NEXT); });
  lv_obj_set_style_bg_color(lv_obj_get_child(next, 0), COL_RAISED, 0);
  sp_play = ui_make_box(sp_controls, 152, 4, 56, 56, COL_TEXT, LV_RADIUS_CIRCLE);
  lv_obj_add_flag(sp_play, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(sp_play, COL_TEXT_2, LV_STATE_PRESSED);
  sp_play_icon = ui_make_label(sp_play, &font_ic_28, COL_ON_ACCENT, IC_PLAY);
  lv_obj_center(sp_play_icon);
  lv_obj_add_event_cb(sp_play, play_cb, LV_EVENT_CLICKED, nullptr);

  // Niente in riproduzione: Ascolti e Cerca
  sp_idle = lv_obj_create(sp_tile);
  lv_obj_remove_style_all(sp_idle);
  lv_obj_set_size(sp_idle, 360, 48);
  lv_obj_set_pos(sp_idle, 0, 88);
  lv_obj_clear_flag(sp_idle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  ui_make_button(sp_idle, BTN_PRIMARY, 16, 0, 156, 48, IC_HEADPHONES, "Ascolti",
                 [](lv_event_t *e) { nav_open_section(SEC_HOME); });
  ui_make_button(sp_idle, BTN_SECONDARY, 188, 0, 156, 48, IC_SEARCH, "Cerca",
                 [](lv_event_t *e) { nav_open_section(SEC_SEARCH); });

  // In arrivo: scheletro fermo
  sp_skeleton = lv_obj_create(sp_tile);
  lv_obj_remove_style_all(sp_skeleton);
  lv_obj_set_size(sp_skeleton, 360, 104);
  lv_obj_clear_flag(sp_skeleton, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  ui_make_box(sp_skeleton, 16, 16, 64, 64, COL_RAISED);
  ui_make_box(sp_skeleton, 96, 22, 180, 18, COL_RAISED, 8);
  ui_make_box(sp_skeleton, 96, 50, 120, 14, COL_SURFACE_PRESSED, 8);

  home_weather_create(panel_home);
}

static void play_cb(lv_event_t *e)
{
  // Inverto subito l'icona: Spotify conferma con il prossimo aggiornamento
  bool playing = strcmp(lv_label_get_text(sp_play_icon), IC_PAUSE) == 0;
  spotify_command(playing ? CMD_PAUSE : CMD_PLAY);
  ui_set_text_raw(sp_play_icon, playing ? IC_PLAY : IC_PAUSE);
  sp_play_hold_until = millis() + 2000;
}

// ---------- Aggiornamento ----------

static lv_obj_t *shown_panel = nullptr;

static void show_only(lv_obj_t *panel)
{
  if (panel == shown_panel) return;
  shown_panel = panel;
  lv_obj_t *all[] = { panel_portal, panel_saved, panel_connecting, panel_home, panel_player, panel_update, nav_panel() };
  for (lv_obj_t *p : all) ui_set_hidden(p, p != panel);
}

enum SpotifyTile { SPT_LOADING, SPT_TRACK, SPT_IDLE, SPT_OFF };

static void update_spotify_tile(bool wifi, const SpotifyStatus &sp, const PlayerState &p)
{
  SpotifyTile kind;
  const char *title = "Spotify non collegato", *sub = "", *note = "";
  if (!wifi) kind = SPT_OFF, sub = "Wi-Fi non collegato", note = "Riprovo da solo appena torna la rete.";
  else if (sp.state == SP_NO_CREDENTIALS) kind = SPT_OFF, sub = "Login da fare", note = "Lancia lo script di login sul computer.";
  else if (sp.state == SP_LOGIN_REQUIRED) kind = SPT_OFF, sub = "Accesso scaduto", note = "Rifai il login dal computer.";
  else if (sp.state == SP_ERROR) kind = SPT_OFF, sub = "Spotify non risponde", note = "Riprovo tra poco.";
  else if (sp.blocked_until) {
    // Spotify ha fermato l'app per troppe richieste (429): si dice fino a quando
    static char until[48];
    struct tm t;
    localtime_r(&sp.blocked_until, &t);
    snprintf(until, sizeof(until), "Riprovo da solo alle %02d:%02d.", t.tm_hour, t.tm_min);
    kind = SPT_OFF, title = "Spotify in pausa", sub = "Troppe richieste in poco tempo", note = until;
  }
  // Pronto = /me già risposto; con nessun dispositivo attivo lo stato del lettore non cambia mai versione
  else if (sp.state != SP_READY) kind = SPT_LOADING, note = "Mi collego a Spotify\xE2\x80\xA6";
  else if (p.has_playback && p.title[0]) kind = SPT_TRACK, title = p.title, sub = p.artist;
  else kind = SPT_IDLE, title = "Niente in riproduzione", sub = "Scegli cosa ascoltare";

  bool track = kind == SPT_TRACK;
  ui_set_hidden(sp_skeleton, kind != SPT_LOADING);
  ui_set_hidden(sp_title, kind == SPT_LOADING);
  ui_set_hidden(sp_sub, kind == SPT_LOADING);
  ui_set_hidden(sp_note, kind != SPT_LOADING && kind != SPT_OFF);
  ui_set_hidden(sp_controls, !track);
  ui_set_hidden(sp_idle, kind != SPT_IDLE);
  // Solo con un brano il riquadro apre Now playing; negli altri stati rispondono soltanto i pulsanti
  if (lv_obj_has_flag(sp_tile, LV_OBJ_FLAG_CLICKABLE) != track) {
    if (track) lv_obj_add_flag(sp_tile, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(sp_tile, LV_OBJ_FLAG_CLICKABLE);
  }
  ui_set_text_raw(sp_title, title);
  ui_set_text_raw(sp_sub, sub);
  ui_set_text_raw(sp_note, note);
  sp_cover.set(track ? p.cover_small : "", 0);
  sp_cover.tick();
  if (track && (int32_t)(sp_play_hold_until - millis()) <= 0) ui_set_text_raw(sp_play_icon, p.is_playing ? IC_PAUSE : IC_PLAY);
}

static void update_home(const NetStatus &st, const SpotifyStatus &sp)
{
  if (!st.time_ok) {
    ui_set_text_raw(home_time, "--:--");
    ui_set_text_raw(home_date, "Sincronizzo l'ora...");
  } else {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    ui_set_text(home_time, "%02d:%02d", t.tm_hour, t.tm_min);
    ui_set_text(home_date, "%s %d %s", GIORNI[t.tm_wday], t.tm_mday, MESI[t.tm_mon]);
  }
  bool wifi = st.state == NET_CONNECTED;
  ui_set_hidden(home_wifi_dot, wifi);
  PlayerState p;
  spotify_get_player(&p);
  if (sp.state != SP_READY) p.has_playback = false;
  home_chip.update(p);
  update_spotify_tile(wifi, sp, p);
  home_weather_update(true);
}

void ui_open_home()
{
  player_open = false;
  nav_home();
}

void ui_open_player()
{
  player_open = true;
  nav_home();
}

void ui_ask_forget_wifi() { forget_btn_cb(nullptr); }

// Navigazione se aperta, Now playing se l'ha aperto l'utente (resta anche in pausa), altrimenti la Home
static void show_main(const NetStatus &st)
{
  SpotifyStatus sp;
  spotify_get_status(&sp);
  PlayerState p;
  spotify_get_player(&p);
  // Dopo 2 minuti senza tocchi si torna alla Home, tranne Now playing mentre la musica suona (§2)
  if (millis() - lvgl_port_last_touch_ms() >= HOME_AFTER_MS && (nav_browsing() || (player_open && !p.is_playing)))
    ui_open_home();
  if (nav_browsing()) {
    player_open = false;
    show_only(nav_panel());
  } else if (player_open && sp.state == SP_READY && player_panel_update()) {
    show_only(panel_player);
  } else {
    player_open = false;
    show_only(panel_home);
    update_home(st, sp);
  }
  nav_tick();
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
      if (ever_connected) {
        show_main(st);  // Wi-Fi caduto dopo l'avvio: si resta sulla Home, che lo dice (§3.6)
        break;
      }
      show_only(panel_connecting);
      uint32_t secs = (millis() - st.connecting_since_ms) / 1000;
      ui_set_text(connecting_label, "Mi collego a %s...", st.ssid);
      ui_set_hidden(connecting_hint, secs < 20);
      break;
    }
    case NET_CONNECTED:
      ever_connected = true;
      show_main(st);
      break;
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
  create_home(scr);
  panel_player = player_panel_create(scr);
  nav_create(scr);

  lv_timer_create(ui_timer_cb, 100, NULL);
}
