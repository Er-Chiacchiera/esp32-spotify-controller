// Stato del computer (design/computer/SPEC_COMPUTER.md): riquadro piccolo della Home, pagina Computer,
// riga di Impostazioni. I dati arrivano da computer.cpp.
#pragma once

#include <lvgl.h>
#include <stddef.h>
#include "computer.h"

class View;

// Contenuto del riquadro piccolo (§3): l'avviso più importante, oppure la batteria
struct ComputerTile {
  const char *icon;
  lv_color_t color;
  char value[40], detail[48];
  bool alert;
};
// false se il computer non è raggiungibile: niente riquadro
bool computer_tile(const ComputerStatus &c, ComputerTile *out);

// "Valerio's MacBook Pro · collegato", "… · ultimo dato alle 18:40", "Nessun computer collegato"
void computer_settings_text(const ComputerStatus &c, char *out, size_t n);

// Pagina Computer a due schede (SPEC_CONTROLLO_MAC §3.1): 0 Stato, 1 Comandi. repair: codice di un abbinamento
// nuovo anche se il Mac è già abbinato ("Abbina di nuovo" di Impostazioni, §7)
enum ComputerTab { CT_STATUS, CT_COMMANDS };
View *view_computer(int tab = CT_STATUS, bool repair = false);
// Pulsante del Mac nella Home (§4): false se non c'è; altrimenti icona e colore della spia
bool computer_home_button(const ComputerStatus &c, const char **icon, lv_color_t *color);
