// Pezzi comuni dell'interfaccia: colori, caratteri, icone e funzioni per scritte e pulsanti.
// Misure e colori da design/fase6/SPEC_FASE6.md.
#pragma once

#include <lvgl.h>

// Colori (SPEC §3.1)
#define COL_BG lv_color_hex(0x121212)
#define COL_RAIL lv_color_hex(0x000000)
#define COL_RAIL_PRESSED lv_color_hex(0x1F1F1F)
#define COL_PANEL lv_color_hex(0x181818)
#define COL_SURFACE lv_color_hex(0x242424)
#define COL_SKELETON_2 lv_color_hex(0x1E1E1E)
#define COL_PRESSED lv_color_hex(0x2A2A2A)
#define COL_SURFACE_PRESSED lv_color_hex(0x333333)
#define COL_RAISED lv_color_hex(0x3A3A3A)
#define COL_KEY_PRESSED lv_color_hex(0x5A5A5A)
#define COL_DISABLED_FG lv_color_hex(0x5A5A5A)
#define COL_DISABLED_FG_2 lv_color_hex(0x3A3A3A)
#define COL_DISABLED_BG lv_color_hex(0x1A1A1A)
#define COL_PLACEHOLDER_FG lv_color_hex(0x535353)
#define COL_TEXT lv_color_hex(0xFFFFFF)
#define COL_TEXT_2 lv_color_hex(0xB3B3B3)
#define COL_TEXT_3 lv_color_hex(0x8A8A8A)
#define COL_ACCENT lv_color_hex(0x1DB954)
#define COL_ACCENT_PRESSED lv_color_hex(0x169C46)
#define COL_ON_ACCENT lv_color_hex(0x000000)
#define COL_WARN lv_color_hex(0xFF9800)
#define COL_WARN_BG lv_color_hex(0x33240D)
// Solo meteo (SPEC_METEO §5): icona del sole e probabilità di pioggia
#define COL_WX_SUN lv_color_hex(0xFFC24B)
#define COL_WX_RAIN lv_color_hex(0x6CB4F0)
// Solo colori di segnale, mai di testo (SPEC_SOLE_ARIA §5): fasce dell'aria (EAQI), livelli dei pollini, luna
#define COL_AQ_1 lv_color_hex(0x4CC3D9)
#define COL_AQ_2 lv_color_hex(0x5CC48A)
#define COL_AQ_3 lv_color_hex(0xE8CF4A)
#define COL_AQ_4 lv_color_hex(0xF2873A)
#define COL_AQ_5 lv_color_hex(0xE5484D)
#define COL_AQ_6 lv_color_hex(0xB76FE3)
#define COL_POL_LOW COL_AQ_2
#define COL_POL_MID COL_AQ_3
#define COL_POL_HIGH COL_AQ_5
// MOON_LIT #D6D6D6 e MOON_DARK #3A3A3A sono già nelle immagini (strumenti/genera_luna.py)

// Montserrat Medium (con simboli LVGL) e SemiBold, Latin-1 + Latin Extended-A + virgolette e trattini
LV_FONT_DECLARE(font_it_16);
LV_FONT_DECLARE(font_it_20);
LV_FONT_DECLARE(font_it_24);
LV_FONT_DECLARE(font_it_sb20);
LV_FONT_DECLARE(font_it_sb24);
LV_FONT_DECLARE(font_it_sb30);
LV_FONT_DECLARE(font_clock_112);  // solo cifre e ":"
LV_FONT_DECLARE(font_temp_64);    // temperatura del meteo: solo "-", cifre e "°"
LV_FONT_DECLARE(font_wx_56);      // le 12 icone del meteo (SPEC_METEO §6)
// Icone Font Awesome 5 (CARATTERI_E_ICONE.md §2)
LV_FONT_DECLARE(font_ic_20);
LV_FONT_DECLARE(font_ic_28);
LV_FONT_DECLARE(font_ic_40);

#define IC_PLAY "\xEF\x81\x8B"
#define IC_PAUSE "\xEF\x81\x8C"
#define IC_PREV "\xEF\x81\x88"
#define IC_NEXT "\xEF\x81\x91"
#define IC_VOLUME "\xEF\x80\xA8"
#define IC_SEARCH "\xEF\x80\x82"
#define IC_LIBRARY "\xEF\x80\xAD"
#define IC_QUEUE "\xEF\x80\x8B"
#define IC_NOTE "\xEF\x80\x81"
#define IC_HEART "\xEF\x80\x84"
#define IC_DISC "\xEF\x94\x9F"
#define IC_USER "\xEF\x80\x87"
#define IC_LEFT "\xEF\x81\x93"
#define IC_UP "\xEF\x81\xB7"
#define IC_DOWN "\xEF\x81\xB8"
#define IC_PLUS "\xEF\x81\xA7"
#define IC_CLOSE "\xEF\x80\x8D"
#define IC_CHECK "\xEF\x80\x8C"
#define IC_REFRESH "\xEF\x80\xA1"
#define IC_WARN "\xEF\x81\xB1"
#define IC_BACKSPACE "\xEF\x95\x9A"
#define IC_COMPUTER "\xEF\x84\x88"
#define IC_PHONE "\xEF\x8F\x8D"
#define IC_TABLET "\xEF\x8F\xBA"
#define IC_TV "\xEF\x89\xAC"
#define IC_GAMEPAD "\xEF\x84\x9B"
#define IC_CAR "\xEF\x86\xB9"
#define IC_WIFI "\xEF\x87\xAB"
#define IC_HOME "\xEF\x80\x95"
#define IC_RIGHT "\xEF\x81\x94"
#define IC_DROP "\xEF\x81\x83"  // goccia della pioggia, solo font_ic_20
#define IC_SUN "\xEF\x86\x85"   // font_ic_20/28/40
#define IC_WIND "\xEF\x9C\xAE"  // aria, font_ic_20/40
#define IC_SPA "\xEF\x96\xBB"   // pollini, font_ic_20/40
#define IC_HEADPHONES "\xEF\x80\xA5"  // Ascolti (Home di Spotify della fase 7), font_ic_20/28
#define IC_COG "\xEF\x80\x93"         // Impostazioni, font_ic_28
#define IC_INFO "\xEF\x81\x9A"        // info-circle, font_ic_28
// Stato del computer (SPEC_COMPUTER §8)
#define IC_LAPTOP "\xEF\x84\x89"       // font_ic_20/28/40
#define IC_BATTERY_FULL "\xEF\x89\x80"  // F240-F244 da piena a vuota, font_ic_20/28
#define IC_MICROCHIP "\xEF\x8B\x9B"    // font_ic_20
#define IC_MEMORY "\xEF\x94\xB8"       // font_ic_20
#define IC_HDD "\xEF\x82\xA0"          // font_ic_20
#define IC_THERMOMETER "\xEF\x8B\x89"  // thermometer-half, font_ic_20
#define IC_PLUG "\xEF\x87\xA6"         // font_ic_28
// Città del meteo (SPEC_CITTA §11)
#define IC_MARKER "\xEF\x8F\x85"        // map-marker-alt, font_ic_20/28/40
#define IC_LOCATION "\xEF\x84\xA4"      // location-arrow, font_ic_28
#define IC_STAR "\xEF\x80\x85"          // font_ic_28
#define IC_TRASH "\xEF\x87\xB8"         // font_ic_28

// "Domenica"… e "gennaio"… (ui.cpp)
extern const char *const GIORNI[7];
extern const char *const MESI[12];

lv_obj_t *ui_make_panel(lv_obj_t *scr);
lv_obj_t *ui_make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
// Etichetta di una riga con i puntini finali se non ci sta
lv_obj_t *ui_make_line_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int x, int y, int w);
// Ridisegnare costa (e il disegno passa dalla PSRAM, la stessa memoria da cui legge lo schermo):
// le scritte aggiornate dai timer cambiano solo se il testo è davvero diverso.
void ui_set_text(lv_obj_t *label, const char *fmt, ...);
void ui_set_text_raw(lv_obj_t *label, const char *text);
void ui_set_hidden(lv_obj_t *obj, bool hidden);
void ui_set_text_color(lv_obj_t *obj, lv_color_t color);
void ui_set_bg(lv_obj_t *obj, lv_color_t color);

// Rettangolo pieno senza stile del tema (niente transizioni, niente bordi)
lv_obj_t *ui_make_box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg, int radius = 0);
// Area toccabile: trasparente, con il colore "premuto" indicato (LV_OPA_TRANSP se nessuno)
lv_obj_t *ui_make_touch(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t pressed, bool has_pressed = true);
// Pulsante icona 64x64 con cerchio 48 PRESSED quando premuto (SPEC 4.7 ibtn)
lv_obj_t *ui_make_icon_button(lv_obj_t *parent, int x, int y, const char *icon, lv_color_t color, lv_event_cb_t cb,
                              void *user = nullptr, lv_obj_t **icon_out = nullptr, int circle = 48);
lv_obj_t *ui_make_spinner(lv_obj_t *parent, int size, bool on_accent = false);

enum ButtonKind { BTN_PRIMARY, BTN_SECONDARY };
// Pulsante con testo SB20 e icona 20 facoltativa (SPEC 4.7). Restituisce l'oggetto da posizionare.
lv_obj_t *ui_make_button(lv_obj_t *parent, ButtonKind kind, int x, int y, int w, int h, const char *icon,
                         const char *text, lv_event_cb_t cb, void *user = nullptr);
void ui_button_set_enabled(lv_obj_t *btn, bool enabled);

// I tocchi che finiscono con uno swipe non devono valere come clic sulla riga sotto il dito
bool ui_click_is_gesture();

// "3:42", "1:02:05"
void ui_format_duration(char *out, size_t n, uint32_t ms);
const char *ui_device_icon(int device_icon);
