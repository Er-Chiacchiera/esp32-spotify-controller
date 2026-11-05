// Città del meteo (SPEC_CITTA): elenco e ricerca. from_weather: aperto dalla pagina a schede, che sta al livello
// back_depth della pila (ci si torna con la città scelta); altrimenti la scelta apre una pagina a schede nuova.
#pragma once

#include "ui_nav.h"

View *view_cities(bool from_weather, int back_depth);
View *view_city_search(bool from_weather, int back_depth);
