// Interfaccia LVGL. Va chiamata con il mutex LVGL preso (lvgl_port_lock).
#pragma once

void ui_create();
// Home della scheda (SPEC_HOME_GIORNI §3) e Now playing: si aprono solo a richiesta, non più da soli
// quando parte o si ferma la musica. Azzerano la pila della navigazione.
void ui_open_home();
void ui_open_player();
// "Cambia Wi-Fi": chiede conferma, dimentica la rete e riavvia nel portale (Impostazioni, Mi collego)
void ui_ask_forget_wifi();
