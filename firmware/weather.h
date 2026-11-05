// Meteo per l'Orologio: Open-Meteo (gratuito, senza chiave) in HTTP semplice, nessuna sessione TLS.
// Posizione dall'IP (ip-api.com) oppure fissata dal PC con il comando seriale METEO.
#pragma once

#include <stdint.h>
#include <time.h>

static const int WEATHER_HOURS = 24;  // fino a 6 ore di dato vecchio + 11 ore di pagina Meteo (SPEC_METEO §7)
static const int WEATHER_DAYS = 3;    // oggi, domani e dopodomani: con il dato vecchio la mezzanotte può essere passata

struct WeatherHour {
  int8_t hour;   // ora locale 0-23
  int8_t temp;   // °C arrotondati
  uint8_t code;  // codice WMO (0 sereno ... 99 temporale con grandine)
  uint8_t is_day;
  uint8_t rain;  // probabilità di pioggia, %
};

struct WeatherDay {
  int8_t tmax, tmin;
  uint8_t code;
  uint8_t rain;  // probabilità massima di pioggia, %
  int16_t sunrise, sunset;  // minuti dalla mezzanotte, ora locale (-1 se mancano)
  uint8_t uv;    // indice UV massimo, arrotondato
};

// Pollini di Open-Meteo (CAMS, solo Europa): a fine autunno e in inverno tutti a 0
enum Pollen { POLLEN_GRASS, POLLEN_OLIVE, POLLEN_BIRCH, POLLEN_ALDER, POLLEN_MUGWORT, POLLEN_RAGWEED, POLLEN_COUNT };

// Qualità dell'aria: richiesta separata dal meteo, quindi può mancare anche con il meteo presente
struct WeatherAir {
  bool valid;             // almeno una risposta arrivata
  bool failed;            // l'ultima richiesta non è riuscita
  uint8_t aqi;            // indice europeo (EAQI) adesso
  uint8_t aqi_hours[WEATHER_HOURS];  // EAQI dall'ora in corso alla richiesta in poi
  time_t hour0;                      // inizio dell'ora di aqi_hours[0]
  float pm25, pm10, no2, o3;         // µg/m³ adesso
  uint8_t sub[4];                    // sottoindici EAQI degli stessi inquinanti (PM2,5, PM10, NO2, O3)
  float pollen[POLLEN_COUNT];        // grani/m³ adesso
  float pollen_max[POLLEN_COUNT];    // massimo delle prossime 24 ore
  uint32_t updated_ms;
};

struct Weather {
  bool valid;             // almeno una previsione arrivata
  bool failed;            // l'ultima richiesta non è riuscita
  char city[32];
  float temp, feels;      // adesso, °C
  uint8_t code, is_day;   // adesso
  WeatherDay days[WEATHER_DAYS];  // dal giorno della richiesta
  WeatherHour hours[WEATHER_HOURS];  // dall'ora in corso alla richiesta in poi
  time_t hour0;           // inizio dell'ora di hours[0] (ora locale = ora del sistema, fuso intero)
  time_t updated;         // ora dell'ultimo aggiornamento riuscito
  uint32_t updated_ms;    // millis() dell'ultimo aggiornamento riuscito
  WeatherAir air;
  uint32_t version;       // cresce a ogni aggiornamento (anche dell'aria): l'interfaccia ridisegna solo se cambia
};

// Luna calcolata dalla data, senza rete (precisione di qualche ora, basta per la fase)
struct MoonInfo {
  float age;          // giorni dalla luna nuova, 0-29,53
  uint8_t lit;        // percentuale illuminata
  uint8_t phase;      // 0 nuova, 1 falce crescente, 2 primo quarto, 3 gibbosa crescente,
                      // 4 piena, 5 gibbosa calante, 6 ultimo quarto, 7 falce calante
  float to_full, to_new;  // giorni alla prossima luna piena e nuova
};
void moon_info(time_t t, MoonInfo *out);
const char *moon_phase_text(int phase);
// Fascia EAQI min(floor(v/20), 5): i valori al confine vanno nella fascia superiore (SPEC_SOLE_ARIA §5.1).
// Livello dei pollini sul massimo delle 24 ore: 0 assente (anche null), 1 bassa, 2 media, 3 alta.
int air_level(int aqi);
const char *air_level_text(int level);
int pollen_level(Pollen p, float grains);
const char *pollen_name(Pollen p);

// Stato da mostrare (SPEC_METEO §3.4): in arrivo, normale, dato vecchio (2-6 h), errore
enum WeatherState { WX_LOADING, WX_OK, WX_OLD, WX_ERROR };
WeatherState weather_state(const Weather &w);
// Stesse soglie per l'aria (SPEC_SOLE_ARIA §4.5)
WeatherState air_state(const WeatherAir &a);
// Indice in hours[] dell'ora in corso (-1 se fuori dai dati) e del giorno di oggi in days[]
int weather_hour_index(const Weather &w, time_t now);
int weather_day_index(const Weather &w, time_t now);
// Codice WMO -> icona (UTF-8) e testo breve (SPEC_METEO §5)
const char *weather_icon(int code, bool day);
const char *weather_text(int code);
static const char *const WX_ICON_SUN = "\xEF\x86\x85";

// ---------- Un giorno scelto nella pagina a schede (SPEC_HOME_GIORNI §8.7) ----------
// Oggi usa Weather; gli altri giorni (da -7 a +7) si chiedono a Open-Meteo quando servono.

static const int DAY_DAYS = 3;  // il giorno scelto e i due dopo (scheda di destra, Prossimi giorni)

struct DayWeather {
  WeatherHour hours[24];          // dalle 00:00 alle 23:00 del giorno scelto (rain 255 se manca, nel passato)
  WeatherDay days[DAY_DAYS];      // il giorno scelto, +1, +2
  uint16_t rain_mm10[DAY_DAYS];   // pioggia caduta, decimi di mm (precipitation_sum)
};

struct DayAirHour {
  uint8_t aqi;                    // 255 se l'ora non ha dato (fine della previsione)
  uint8_t sub[4];                 // sottoindici PM2,5, PM10, NO2, O3
  float pm25, pm10, no2, o3;
  float pollen[POLLEN_COUNT];
};
struct DayAir {
  DayAirHour hours[24];           // dalle 00:00 alle 23:00
};

enum DayKind { DAY_WEATHER, DAY_AIR };
enum DayStatus { DAY_NONE, DAY_LOADING, DAY_OK, DAY_ERROR };
// Chiede il giorno a "offset" giorni da oggi (non 0). Se è già in memoria e valido (30 minuti) non fa niente,
// a meno di force (Riprova). Le richieste dei giorni passano prima dell'aggiornamento periodico.
void weather_day_request(DayKind kind, int offset, bool force = false);
// Stato del giorno e, se DAY_OK, i dati (out può essere nullptr)
DayStatus weather_day_get(int offset, DayWeather *out);
DayStatus weather_day_get_air(int offset, DayAir *out);
// Uscendo dalla pagina la memoria dei giorni si svuota
void weather_day_clear();

void weather_init();
void weather_get(Weather *out);
// Dal task Spotify, con rete e ora pronte: scarica meteo e aria se è ora (ogni 30 minuti), una richiesta
// per chiamata. true se ha fatto una richiesta.
bool weather_tick();
// Comando seriale "METEO ...": stampa il meteo, fissa la posizione o torna a quella dall'IP, prove degli stati
void weather_serial(const char *args);
