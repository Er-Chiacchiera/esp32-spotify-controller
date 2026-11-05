// Meteo sull'Orologio e pagina Meteo (design/meteo/SPEC_METEO.md). Dati da weather.cpp.
#pragma once

#include <lvgl.h>
#include "ui_nav.h"

// Blocco "adesso" e fascia delle prossime ore dentro il pannello dell'Orologio (coordinate dello schermo)
void clock_weather_create(lv_obj_t *panel);
// Dal timer dell'interfaccia mentre l'Orologio è visibile: ridisegna solo se cambiano dati, stato o ora.
// can_open: il tocco apre la pagina Meteo (serve Spotify collegato, come per tutta la navigazione)
void clock_weather_update(bool can_open);

// Pagina a schede Meteo · Sole e luna · Aria (SPEC_SOLE_ARIA §4)
enum WeatherTab { WT_WEATHER, WT_SUN, WT_AIR };
View *view_weather(int tab = WT_WEATHER);
