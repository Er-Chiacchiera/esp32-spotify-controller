// Interfaccia LVGL. Va chiamata con il mutex LVGL preso (lvgl_port_lock).
// Nota: i font Montserrat di LVGL non hanno le lettere accentate, quindi i testi a schermo ne fanno a meno.
#pragma once

void ui_create();
