// Schermata "now playing" (SPEC 5.1): chip del dispositivo, icone in alto, copertina, titolo,
// barra, tasti e volume. Più il chip del dispositivo e le icone in alto condivisi con l'Orologio.
#pragma once

#include <lvgl.h>
#include "spotify.h"

lv_obj_t *player_panel_create(lv_obj_t *scr);
// Aggiorna la schermata; restituisce false se non c'è niente in riproduzione (va mostrato l'orologio).
bool player_panel_update();
// Prova dal PC (comando seriale COLORE RRGGBB): fondo di Now playing fisso; -1 torna al colore della copertina.
void player_force_bg(int32_t rgb);

// Chip del dispositivo (SPEC 4.15): tocco = Dispositivi
struct DeviceChip {
  lv_obj_t *chip = nullptr, *icon = nullptr, *label = nullptr, *chevron = nullptr;
  lv_color_t text_2;  // grigio di icona e nome senza dispositivo: TEXT_2, in Now playing NP_TEXT_2
  void create(lv_obj_t *parent);
  void update(const PlayerState &p);
  void set_colors(lv_color_t surface, lv_color_t pressed, lv_color_t text_2);  // Now playing (SPEC_COLORE §2.2)
};

// Home / Libreria / Cerca / Coda in alto a destra (536, 600, 664, 728). buttons_out: i 4 pulsanti
// (figlio 0 il cerchio premuto, figlio 1 l'icona), per ricolorarli in Now playing
void ui_make_top_icons(lv_obj_t *parent, lv_obj_t **queue_icon_out = nullptr, lv_obj_t **queue_btn_out = nullptr,
                       lv_obj_t **buttons_out = nullptr);
