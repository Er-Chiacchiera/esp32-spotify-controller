// Meteo della Home e pagina a schede Meteo · Sole e luna · Aria (SPEC_METEO, SPEC_SOLE_ARIA, SPEC_HOME_GIORNI).
#pragma once

#include <lvgl.h>
#include "ui_nav.h"

// Home (SPEC_HOME_GIORNI §3.4-§3.5): riquadro Meteo e i tre riquadri piccoli (sole, aria, pollini, luna)
// dentro il pannello della Home, in coordinate dello schermo
void home_weather_create(lv_obj_t *panel);
// Dal timer dell'interfaccia mentre la Home è visibile: ridisegna solo se cambiano dati, stato o ora
void home_weather_update(bool can_open);

// Pagina a schede Meteo · Sole e luna · Aria (SPEC_SOLE_ARIA §4)
enum WeatherTab { WT_WEATHER, WT_SUN, WT_AIR };
View *view_weather(int tab = WT_WEATHER);
