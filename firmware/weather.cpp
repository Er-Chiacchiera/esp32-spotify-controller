#include "weather.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include "psram_json.h"
#include "spotify_internal.h"

// Ogni 30 minuti (Open-Meteo aggiorna i modelli ogni ora); dopo un errore si riprova fra 2 minuti
static const uint32_t REFRESH_MS = 30 * 60 * 1000;
static const uint32_t RETRY_MS = 2 * 60 * 1000;
static const size_t MAX_RESPONSE = 12 * 1024;  // la previsione occupa ~1,4 KB, un giorno scelto ~3 KB
static const char *NVS_NAMESPACE = "meteo";

static SemaphoreHandle_t mutex;
static Weather weather;

// Posizione: fissata dal PC (NVS) oppure trovata dall'IP a ogni avvio. Usate solo dal task Spotify,
// tranne location_reset che arriva dal comando seriale.
static bool have_location = false;
static float lat, lon;
static char city[32], country[4];
static bool name_pending = false;  // nome della città ancora in inglese
static void localize_city();
static uint32_t next_ms = 0, air_next_ms = 0;
static volatile bool location_reset = true;
static volatile uint32_t age_offset_ms = 0;  // METEO ETA: finge che l'ultimo aggiornamento sia più vecchio
static volatile uint32_t air_age_offset_ms = 0;  // METEO ARIA ETA
static volatile int32_t moon_offset_s = 0;       // METEO LUNA

// ---------- Giorni scelti nella pagina a schede (SPEC_HOME_GIORNI §8.7) ----------

static const int DAY_SLOTS = 6;                      // ultimi 6 giorni per tipo
static const uint32_t DAY_VALID_MS = 30 * 60 * 1000;
struct DaySlot {
  time_t date;       // mezzanotte locale del giorno: resta giusta anche se passa la mezzanotte con la pagina aperta
  DayStatus status;
  uint32_t at_ms;    // ultima richiesta o risposta
  bool wanted;       // da scaricare
};
static DaySlot wslots[DAY_SLOTS], aslots[DAY_SLOTS];
static DayWeather *wdata;  // [DAY_SLOTS] in PSRAM
static DayAir *adata;
static volatile bool day_pending = false;

void weather_init()
{
  mutex = xSemaphoreCreateMutex();
  memset(&weather, 0, sizeof(weather));
  wdata = (DayWeather *)heap_caps_calloc(DAY_SLOTS, sizeof(DayWeather), MALLOC_CAP_SPIRAM);
  adata = (DayAir *)heap_caps_calloc(DAY_SLOTS, sizeof(DayAir), MALLOC_CAP_SPIRAM);
}



void weather_get(Weather *out)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  *out = weather;
  xSemaphoreGive(mutex);
  // METEO ETA: come se l'ultimo aggiornamento (e quindi la prima ora dei dati) fosse più indietro
  uint32_t off = age_offset_ms;
  out->updated_ms -= off;
  out->updated -= off / 1000;
  out->hour0 -= off / 1000;
  out->air.updated_ms -= air_age_offset_ms;
}

static bool get_json(const String &url, JsonDocument &doc)
{
  PsramSink sink(MAX_RESPONSE);
  bool ok = image_download(url.c_str(), sink);
  image_conn_close();  // ogni 30 minuti: non vale la pena tenere aperta la connessione (e la sua RAM)
  if (!ok) return false;
  DeserializationError err = deserializeJson(doc, (const char *)sink.data, sink.len);
  if (err) Serial.printf("Meteo: JSON non valido (%s)\n", err.c_str());
  return !err;
}

static String url_encode(const char *s)
{
  String out;
  for (; *s; s++) {
    uint8_t c = *s;
    if (isalnum(c) || c == '-' || c == '.' || c == '_') {
      out += (char)c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", c);
      out += hex;
    }
  }
  return out;
}

static bool find_location()
{
  name_pending = false;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  bool fixed = prefs.isKey("lat");
  if (fixed) {
    lat = prefs.getFloat("lat");
    lon = prefs.getFloat("lon");
    strlcpy(city, prefs.getString("city", "").c_str(), sizeof(city));
  }
  prefs.end();
  if (fixed) {
    Serial.printf("Meteo: posizione fissata %s (%.4f, %.4f)\n", city, lat, lon);
    return true;
  }

  // Posizione approssimata dall'IP (servizio gratuito solo in HTTP), poi il nome in italiano dal geocoding
  // di Open-Meteo ("Naples" -> "Napoli")
  JsonDocument doc(PsramAllocator::instance());
  if (!get_json("http://ip-api.com/json/?fields=status,city,countryCode,lat,lon", doc) || doc["status"] != "success") {
    Serial.println("Meteo: posizione dall'IP non trovata");
    return false;
  }
  lat = doc["lat"];
  lon = doc["lon"];
  strlcpy(city, doc["city"] | "", sizeof(city));
  strlcpy(country, doc["countryCode"] | "", sizeof(country));
  Serial.printf("Meteo: posizione dall'IP %s (%.4f, %.4f)\n", city, lat, lon);
  name_pending = true;
  localize_city();
  return true;
}

// Nome in italiano; se la richiesta fallisce resta quello in inglese e si riprova dopo la prossima previsione
static void localize_city()
{
  JsonDocument doc(PsramAllocator::instance());
  String url = "http://geocoding-api.open-meteo.com/v1/search?count=1&language=it&format=json&name=" + url_encode(city) +
               "&countryCode=" + country;
  if (!get_json(url, doc)) return;
  name_pending = false;
  if (doc["results"][0]["name"].is<const char *>()) {
    strlcpy(city, doc["results"][0]["name"].as<const char *>(), sizeof(city));
    Serial.printf("Meteo: nome in italiano %s\n", city);
  }
}

static int hour_of(const char *iso)  // "2026-11-04T16:00"
{
  return iso && strlen(iso) >= 13 ? atoi(iso + 11) : -1;
}

static int16_t minutes_of(const char *iso)  // "2026-11-04T06:46" -> 406
{
  return iso && strlen(iso) >= 16 ? atoi(iso + 11) * 60 + atoi(iso + 14) : -1;
}

// Inizio dell'ora di una serie oraria che parte dall'ora in corso nel fuso della posizione (timezone=auto);
// la scheda usa il fuso italiano, quindi per l'Italia coincide con l'ora di sistema
static time_t series_hour0(time_t now, int first_hour)
{
  time_t h0 = now - now % 3600;
  struct tm t;
  localtime_r(&h0, &t);
  int shift = (first_hour - t.tm_hour + 36) % 24 - 12;  // se il server parte da un'altra ora
  return h0 + shift * 3600;
}

static bool fetch_forecast()
{
  char url[400];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,weather_code,is_day"
           "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
           "&daily=temperature_2m_max,temperature_2m_min,weather_code,precipitation_probability_max,sunrise,sunset,"
           "uv_index_max"
           "&forecast_hours=%d&forecast_days=%d&timezone=auto",
           lat, lon, WEATHER_HOURS, WEATHER_DAYS);
  uint32_t t0 = millis();
  JsonDocument doc(PsramAllocator::instance());
  if (!get_json(url, doc) || !doc["current"]["temperature_2m"].is<float>()) {
    Serial.println("Meteo: previsione non arrivata");
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.failed = true;
    weather.version++;
    xSemaphoreGive(mutex);
    return false;
  }

  Weather w = {};
  w.valid = true;
  strlcpy(w.city, city, sizeof(w.city));
  JsonObject cur = doc["current"];
  w.temp = cur["temperature_2m"];
  w.feels = cur["apparent_temperature"];
  w.code = cur["weather_code"];
  w.is_day = cur["is_day"];
  JsonObject daily = doc["daily"];
  for (int d = 0; d < WEATHER_DAYS; d++) {
    w.days[d].tmax = (int8_t)lroundf(daily["temperature_2m_max"][d] | 0.0f);
    w.days[d].tmin = (int8_t)lroundf(daily["temperature_2m_min"][d] | 0.0f);
    w.days[d].code = daily["weather_code"][d] | 0;
    w.days[d].rain = daily["precipitation_probability_max"][d] | 0;
    w.days[d].sunrise = minutes_of(daily["sunrise"][d].as<const char *>());
    w.days[d].sunset = minutes_of(daily["sunset"][d].as<const char *>());
    w.days[d].uv = (uint8_t)lroundf(daily["uv_index_max"][d] | 0.0f);
  }
  JsonObject hourly = doc["hourly"];
  for (int i = 0; i < WEATHER_HOURS; i++) {
    WeatherHour &h = w.hours[i];
    h.hour = hour_of(hourly["time"][i].as<const char *>());
    h.temp = (int8_t)lroundf(hourly["temperature_2m"][i] | 0.0f);
    h.code = hourly["weather_code"][i] | 0;
    h.is_day = hourly["is_day"][i] | 0;
    h.rain = hourly["precipitation_probability"][i] | 0;
  }
  time_t now = time(nullptr);
  w.hour0 = series_hour0(now, w.hours[0].hour);
  w.updated = now;
  w.updated_ms = millis();
  age_offset_ms = 0;

  xSemaphoreTake(mutex, portMAX_DELAY);
  w.air = weather.air;
  w.version = weather.version + 1;
  weather = w;
  xSemaphoreGive(mutex);
  Serial.printf("Meteo: %s %.1f °C (percepiti %.1f), codice %d, oggi %d/%d °C, in %lu ms\n", w.city, w.temp, w.feels,
                w.code, w.days[0].tmin, w.days[0].tmax, (unsigned long)(millis() - t0));
  return true;
}

// Nello stesso ordine di enum Pollen
static const char *const POLLEN_KEYS[POLLEN_COUNT] = {"grass_pollen",   "olive_pollen",   "birch_pollen",
                                                      "alder_pollen",   "mugwort_pollen", "ragweed_pollen"};

static float value_or_nan(JsonVariantConst v)  // pollini fuori dall'Europa: null
{
  return v.is<float>() ? v.as<float>() : NAN;
}

static bool fetch_air()
{
  String pollens;
  for (const char *k : POLLEN_KEYS) pollens += String(",") + k;
  char url[600];
  snprintf(url, sizeof(url),
           "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
           "&current=european_aqi,pm2_5,pm10,nitrogen_dioxide,ozone,european_aqi_pm2_5,european_aqi_pm10,"
           "european_aqi_nitrogen_dioxide,european_aqi_ozone%s&hourly=european_aqi%s"
           "&forecast_hours=%d&timezone=auto",
           lat, lon, pollens.c_str(), pollens.c_str(), WEATHER_HOURS);
  uint32_t t0 = millis();
  JsonDocument doc(PsramAllocator::instance());
  if (!get_json(url, doc) || !doc["current"]["european_aqi"].is<int>()) {
    Serial.println("Meteo: qualità dell'aria non arrivata");
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.air.failed = true;
    weather.version++;
    xSemaphoreGive(mutex);
    return false;
  }

  WeatherAir a = {};
  a.valid = true;
  JsonObject cur = doc["current"];
  a.aqi = cur["european_aqi"];
  a.pm25 = value_or_nan(cur["pm2_5"]);
  a.pm10 = value_or_nan(cur["pm10"]);
  a.no2 = value_or_nan(cur["nitrogen_dioxide"]);
  a.o3 = value_or_nan(cur["ozone"]);
  a.sub[0] = cur["european_aqi_pm2_5"] | 0;
  a.sub[1] = cur["european_aqi_pm10"] | 0;
  a.sub[2] = cur["european_aqi_nitrogen_dioxide"] | 0;
  a.sub[3] = cur["european_aqi_ozone"] | 0;
  JsonObject hourly = doc["hourly"];
  for (int i = 0; i < WEATHER_HOURS; i++) a.aqi_hours[i] = hourly["european_aqi"][i] | 0;
  for (int p = 0; p < POLLEN_COUNT; p++) {
    a.pollen[p] = value_or_nan(cur[POLLEN_KEYS[p]]);
    a.pollen_max[p] = a.pollen[p];
    for (JsonVariantConst v : hourly[POLLEN_KEYS[p]].as<JsonArrayConst>())
      if (v.is<float>() && !(v.as<float>() <= a.pollen_max[p])) a.pollen_max[p] = v.as<float>();  // anche se era NAN
  }
  a.hour0 = series_hour0(time(nullptr), hour_of(hourly["time"][0].as<const char *>()));
  a.updated_ms = millis();
  air_age_offset_ms = 0;

  xSemaphoreTake(mutex, portMAX_DELAY);
  weather.air = a;
  weather.version++;
  xSemaphoreGive(mutex);
  Serial.printf("Meteo: aria EAQI %d (%s), PM2,5 %.1f, PM10 %.1f, graminacee %.1f, olivo %.1f, in %lu ms\n", a.aqi,
                air_level_text(air_level(a.aqi)), a.pm25, a.pm10, a.pollen_max[POLLEN_GRASS],
                a.pollen_max[POLLEN_OLIVE], (unsigned long)(millis() - t0));
  return true;
}

int air_level(int aqi)
{
  return min(aqi / 20, 5);
}

const char *air_level_text(int level)
{
  static const char *const TEXT[] = {"Buona", "Discreta", "Moderata", "Scadente", "Molto scadente", "Pessima"};
  return level >= 0 && level <= 5 ? TEXT[level] : "";
}

// Classi POLLnet (SPEC_SOLE_ARIA §5.2, soglie di media e alta)
int pollen_level(Pollen p, float grains)
{
  static const float LV_MEDIUM[POLLEN_COUNT] = {10, 5, 16, 16, 5, 5}, LV_HIGH[POLLEN_COUNT] = {30, 25, 50, 50, 25, 25};
  if (isnan(grains) || grains < 0.1f) return 0;
  return grains >= LV_HIGH[p] ? 3 : grains >= LV_MEDIUM[p] ? 2 : 1;
}

const char *pollen_name(Pollen p)
{
  static const char *const NAME[POLLEN_COUNT] = {"Graminacee", "Olivo", "Betulla", "Ontano", "Artemisia", "Ambrosia"};
  return p >= 0 && p < POLLEN_COUNT ? NAME[p] : "";
}

void moon_info(time_t t, MoonInfo *out)
{
  t += moon_offset_s;
  static const double SYNODIC = 29.530588853;
  static const time_t NEW_MOON = 947182440;  // luna nuova di riferimento: 6 gennaio 2000, 18:14 UTC
  double age = fmod((double)(t - NEW_MOON) / 86400.0, SYNODIC);
  if (age < 0) age += SYNODIC;
  out->age = age;
  out->lit = (uint8_t)lround((1 - cos(2 * M_PI * age / SYNODIC)) * 50);
  out->phase = (uint8_t)((int)lround(age / SYNODIC * 8) % 8);
  out->to_full = fmod(SYNODIC / 2 - age + SYNODIC, SYNODIC);
  out->to_new = SYNODIC - age;
}

const char *moon_phase_text(int phase)
{
  static const char *const TEXT[] = {"Luna nuova",  "Falce crescente",  "Primo quarto",  "Gibbosa crescente",
                                     "Luna piena", "Gibbosa calante", "Ultimo quarto", "Falce calante"};
  return phase >= 0 && phase < 8 ? TEXT[phase] : "";
}

WeatherState air_state(const WeatherAir &a)
{
  if (!a.valid) return a.failed ? WX_ERROR : WX_LOADING;
  uint32_t age = millis() - a.updated_ms;
  if (age > 6 * 3600 * 1000UL) return WX_ERROR;
  return age > 2 * 3600 * 1000UL ? WX_OLD : WX_OK;
}

WeatherState weather_state(const Weather &w)
{
  if (!w.valid) return w.failed ? WX_ERROR : WX_LOADING;
  uint32_t age = millis() - w.updated_ms;
  if (age > 6 * 3600 * 1000UL) return WX_ERROR;
  return age > 2 * 3600 * 1000UL ? WX_OLD : WX_OK;
}

int weather_hour_index(const Weather &w, time_t now)
{
  long i = (long)(now - w.hour0) / 3600;
  return i >= 0 && i < WEATHER_HOURS ? (int)i : -1;
}

static time_t midnight(time_t t)
{
  struct tm tm;
  localtime_r(&t, &tm);
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

int weather_day_index(const Weather &w, time_t now)
{
  long d = lroundf((midnight(now) - midnight(w.hour0)) / 86400.0f);  // 23 o 25 ore al cambio dell'ora legale
  return d >= 0 && d < WEATHER_DAYS ? (int)d : -1;
}

// SPEC_METEO §5
const char *weather_icon(int code, bool day)
{
  static const char *SUN = "\xEF\x86\x85", *MOON = "\xEF\x86\x86", *CLOUD_SUN = "\xEF\x9B\x84",
                    *CLOUD_MOON = "\xEF\x9B\x83", *CLOUD = "\xEF\x83\x82", *SMOG = "\xEF\x9D\x9F",
                    *RAIN = "\xEF\x9C\xBD", *HEAVY = "\xEF\x9D\x80", *SUN_RAIN = "\xEF\x9D\x83",
                    *MOON_RAIN = "\xEF\x9C\xBC", *SNOW = "\xEF\x8B\x9C", *BOLT = "\xEF\x83\xA7";
  switch (code) {
    case 0: return day ? SUN : MOON;
    case 1:
    case 2: return day ? CLOUD_SUN : CLOUD_MOON;
    case 45:
    case 48: return SMOG;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
    case 61: return RAIN;
    case 63:
    case 65:
    case 66:
    case 67:
    case 82: return HEAVY;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86: return SNOW;
    case 80:
    case 81: return day ? SUN_RAIN : MOON_RAIN;
    case 95:
    case 96:
    case 99: return BOLT;
    default: return CLOUD;  // 3 e codici sconosciuti
  }
}

const char *weather_text(int code)
{
  switch (code) {
    case 0: return "Sereno";
    case 1: return "Quasi sereno";
    case 2: return "Poco nuvoloso";
    case 3: return "Coperto";
    case 45:
    case 48: return "Nebbia";
    case 51: return "Pioviggine debole";
    case 53: return "Pioviggine";
    case 55: return "Pioviggine intensa";
    case 56:
    case 57: return "Pioviggine gelata";
    case 61: return "Pioggia debole";
    case 63: return "Pioggia";
    case 65: return "Pioggia forte";
    case 66:
    case 67: return "Pioggia gelata";
    case 71: return "Neve debole";
    case 73: return "Neve";
    case 75: return "Neve forte";
    case 77: return "Nevischio";
    case 80: return "Rovesci deboli";
    case 81: return "Rovesci";
    case 82: return "Rovesci forti";
    case 85: return "Rovesci di neve";
    case 86: return "Forti rovesci di neve";
    case 95: return "Temporale";
    case 96:
    case 99: return "Temporale con grandine";
    default: return "";
  }
}

static time_t date_of(int offset)
{
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  t.tm_mday += offset;
  t.tm_hour = t.tm_min = t.tm_sec = 0;
  t.tm_isdst = -1;
  return mktime(&t);
}

static void ymd(time_t date, int add_days, char *out, size_t n)
{
  struct tm t;
  localtime_r(&date, &t);
  t.tm_mday += add_days;
  t.tm_hour = 12;  // mezzogiorno: nessun problema al cambio dell'ora legale
  t.tm_isdst = -1;
  time_t d = mktime(&t);
  localtime_r(&d, &t);
  snprintf(out, n, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

static int find_slot(DaySlot *slots, time_t date)
{
  for (int i = 0; i < DAY_SLOTS; i++)
    if (slots[i].status != DAY_NONE && slots[i].date == date) return i;
  return -1;
}

void weather_day_request(DayKind kind, int offset, bool force)
{
  DaySlot *slots = kind == DAY_WEATHER ? wslots : aslots;
  time_t date = date_of(offset);
  xSemaphoreTake(mutex, portMAX_DELAY);
  int i = find_slot(slots, date);
  bool fresh = i >= 0 && slots[i].status == DAY_OK && millis() - slots[i].at_ms < DAY_VALID_MS;
  if (i >= 0 && !force && (fresh || slots[i].status == DAY_LOADING)) {
    xSemaphoreGive(mutex);
    return;
  }
  if (i < 0) {  // posto libero, o quello usato meno di recente
    i = 0;
    for (int k = 0; k < DAY_SLOTS; k++) {
      if (slots[k].status == DAY_NONE) {
        i = k;
        break;
      }
      if ((int32_t)(slots[k].at_ms - slots[i].at_ms) < 0) i = k;
    }
  }
  slots[i] = { date, DAY_LOADING, millis(), true };
  day_pending = true;
  xSemaphoreGive(mutex);
  work_signal();
}

static DayStatus day_get(DaySlot *slots, int offset, void *out, const void *data, size_t size)
{
  time_t date = date_of(offset);
  xSemaphoreTake(mutex, portMAX_DELAY);
  int i = find_slot(slots, date);
  DayStatus st = i < 0 ? DAY_NONE : slots[i].status;
  if (st == DAY_OK && millis() - slots[i].at_ms >= DAY_VALID_MS) st = DAY_NONE;  // scaduto: va richiesto
  if (st == DAY_OK && out) memcpy(out, (const uint8_t *)data + i * size, size);
  xSemaphoreGive(mutex);
  return st;
}

DayStatus weather_day_get(int offset, DayWeather *out) { return day_get(wslots, offset, out, wdata, sizeof(DayWeather)); }
DayStatus weather_day_get_air(int offset, DayAir *out) { return day_get(aslots, offset, out, adata, sizeof(DayAir)); }

void weather_day_clear()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < DAY_SLOTS; i++) wslots[i].status = aslots[i].status = DAY_NONE;
  day_pending = false;
  xSemaphoreGive(mutex);
}

static void day_done(DaySlot &slot, time_t date, bool ok)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  // Se nel frattempo il posto è stato preso da un altro giorno, la risposta si butta
  if (slot.date == date && slot.status == DAY_LOADING) {
    slot.status = ok ? DAY_OK : DAY_ERROR;
    slot.at_ms = millis();
  }
  xSemaphoreGive(mutex);
}

static bool fetch_day_weather(int i, time_t date)
{
  char d0[12], d2[12], url[512];
  ymd(date, 0, d0, sizeof(d0));
  ymd(date, DAY_DAYS - 1, d2, sizeof(d2));
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=auto&start_date=%s&end_date=%s"
           "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,precipitation_sum,"
           "sunrise,sunset,uv_index_max",
           lat, lon, d0, d2);
  uint32_t t0 = millis();
  JsonDocument doc(PsramAllocator::instance());
  bool ok = get_json(url, doc) && doc["hourly"]["temperature_2m"].is<JsonArray>();
  if (ok) {
    DayWeather d = {};
    JsonObject hourly = doc["hourly"];
    for (int h = 0; h < 24; h++) {
      WeatherHour &x = d.hours[h];
      x.hour = h;
      x.temp = (int8_t)lroundf(hourly["temperature_2m"][h] | 0.0f);
      x.code = hourly["weather_code"][h] | 0;
      x.is_day = hourly["is_day"][h] | 0;
      x.rain = hourly["precipitation_probability"][h] | 255;  // nel passato può mancare
    }
    JsonObject daily = doc["daily"];
    for (int k = 0; k < DAY_DAYS; k++) {
      WeatherDay &x = d.days[k];
      x.tmax = (int8_t)lroundf(daily["temperature_2m_max"][k] | 0.0f);
      x.tmin = (int8_t)lroundf(daily["temperature_2m_min"][k] | 0.0f);
      x.code = daily["weather_code"][k] | 0;
      x.rain = daily["precipitation_probability_max"][k] | 0;
      x.sunrise = minutes_of(daily["sunrise"][k].as<const char *>());
      x.sunset = minutes_of(daily["sunset"][k].as<const char *>());
      x.uv = (uint8_t)lroundf(daily["uv_index_max"][k] | 0.0f);
      d.rain_mm10[k] = (uint16_t)lroundf((daily["precipitation_sum"][k] | 0.0f) * 10);
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (wslots[i].date == date) wdata[i] = d;
    xSemaphoreGive(mutex);
  }
  day_done(wslots[i], date, ok);
  Serial.printf("Meteo: giorno %s %s in %lu ms\n", d0, ok ? "arrivato" : "non arrivato", (unsigned long)(millis() - t0));
  return ok;
}

static bool fetch_day_air(int i, time_t date)
{
  char d0[12], url[700];
  ymd(date, 0, d0, sizeof(d0));
  String pollens;
  for (const char *k : POLLEN_KEYS) pollens += String(",") + k;
  snprintf(url, sizeof(url),
           "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f&timezone=auto"
           "&start_date=%s&end_date=%s&hourly=european_aqi,pm2_5,pm10,nitrogen_dioxide,ozone,european_aqi_pm2_5,"
           "european_aqi_pm10,european_aqi_nitrogen_dioxide,european_aqi_ozone%s",
           lat, lon, d0, d0, pollens.c_str());
  uint32_t t0 = millis();
  JsonDocument doc(PsramAllocator::instance());
  bool ok = get_json(url, doc) && doc["hourly"]["european_aqi"].is<JsonArray>();
  if (ok) {
    static const char *const SUB[4] = { "european_aqi_pm2_5", "european_aqi_pm10", "european_aqi_nitrogen_dioxide",
                                        "european_aqi_ozone" };
    JsonObject hourly = doc["hourly"];
    DayAir *a = (DayAir *)heap_caps_calloc(1, sizeof(DayAir), MALLOC_CAP_SPIRAM);
    ok = a != nullptr;
    for (int h = 0; ok && h < 24; h++) {
      DayAirHour &x = a->hours[h];
      JsonVariantConst v = hourly["european_aqi"][h];
      x.aqi = v.is<int>() ? (uint8_t)min(v.as<int>(), 254) : 255;
      for (int k = 0; k < 4; k++) x.sub[k] = hourly[SUB[k]][h] | 0;
      x.pm25 = value_or_nan(hourly["pm2_5"][h]);
      x.pm10 = value_or_nan(hourly["pm10"][h]);
      x.no2 = value_or_nan(hourly["nitrogen_dioxide"][h]);
      x.o3 = value_or_nan(hourly["ozone"][h]);
      for (int p = 0; p < POLLEN_COUNT; p++) x.pollen[p] = hourly[POLLEN_KEYS[p]][h] | 0.0f;  // null come 0
    }
    if (ok) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      if (aslots[i].date == date) adata[i] = *a;
      xSemaphoreGive(mutex);
      heap_caps_free(a);
    }
  }
  day_done(aslots[i], date, ok);
  Serial.printf("Meteo: aria del %s %s in %lu ms\n", d0, ok ? "arrivata" : "non arrivata", (unsigned long)(millis() - t0));
  return ok;
}

// Un giorno chiesto dalla pagina, il più recente per primo. true se ha fatto una richiesta.
static bool day_tick()
{
  if (!day_pending) return false;
  int best = -1;
  bool air = false;
  time_t date = 0;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int k = 0; k < 2 * DAY_SLOTS; k++) {
    DaySlot &s = k < DAY_SLOTS ? wslots[k] : aslots[k - DAY_SLOTS];
    if (!s.wanted || s.status != DAY_LOADING) continue;
    if (best < 0 || (int32_t)(s.at_ms - (air ? aslots[best] : wslots[best]).at_ms) > 0) {
      best = k % DAY_SLOTS;
      air = k >= DAY_SLOTS;
    }
  }
  if (best >= 0) {
    DaySlot &s = air ? aslots[best] : wslots[best];
    s.wanted = false;
    date = s.date;
  } else {
    day_pending = false;
  }
  xSemaphoreGive(mutex);
  if (best < 0) return false;
  if (!have_location) {
    day_done(air ? aslots[best] : wslots[best], date, false);
    return true;
  }
  if (air) fetch_day_air(best, date);
  else fetch_day_weather(best, date);
  return true;
}

bool weather_tick()
{
  if (day_tick()) return true;
  if (location_reset) {
    location_reset = false;
    have_location = false;
    next_ms = air_next_ms = millis();
  }
  if ((int32_t)(millis() - next_ms) >= 0) {
    if (!have_location) have_location = find_location();
    else if (name_pending) localize_city();
    if (!have_location) {
      // Senza posizione non parte nessuna richiesta: meteo e aria passano comunque a "non disponibile"
      // invece di restare "in arrivo" (SPEC_SOLE_ARIA §3.3)
      xSemaphoreTake(mutex, portMAX_DELAY);
      weather.failed = weather.air.failed = true;
      weather.version++;
      xSemaphoreGive(mutex);
    }
    bool ok = have_location && fetch_forecast();
    next_ms = millis() + (ok ? REFRESH_MS : RETRY_MS);
    return true;
  }
  // L'aria alla chiamata dopo il meteo, con il suo ritmo: se manca si riprova solo lei
  if (!have_location || (int32_t)(millis() - air_next_ms) < 0) return false;
  air_next_ms = millis() + (fetch_air() ? REFRESH_MS : RETRY_MS);
  return true;
}

// METEO                      stampa il meteo
// METEO <lat> <lon> <città>  fissa la posizione (es. METEO 40.8522 14.2681 Napoli)
// METEO AUTO                 torna alla posizione dall'IP
// Prove degli stati (SPEC_METEO §9.6), fino al prossimo aggiornamento:
// METEO ETA <s>              l'ultimo aggiornamento risale a s secondi fa (7200-21600 dato vecchio, oltre errore)
// METEO ARRIVO | ERRORE      come all'accensione / senza nessun dato; riprova fra 60 s
// METEO PROVA <codice> <°C> <giorno 0/1>   condizione e temperatura finte su adesso, ore e giorni
// Aria (SPEC_SOLE_ARIA §10.6-7), fino alla prossima risposta:
// METEO ARIA ETA <s> | ARIA ARRIVO | ARIA ERRORE | ARIA <indice>   dato vecchio, stati, indice finto (24 ore a onda)
// METEO POLLINI <tipo 0-5> <grani>   grani finti di un tipo (0 graminacee ... 5 ambrosia)
// METEO LUNA <giorni>                sposta la luna di tanti giorni (0 torna a oggi)
void weather_serial(const char *args)
{
  while (*args == ' ') args++;
  float a, b;
  int n = 0, code, day;
  if (sscanf(args, "ARIA ETA %f", &a) == 1) {
    air_age_offset_ms = (uint32_t)(a * 1000);
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.version++;
    xSemaphoreGive(mutex);
    Serial.printf("METEO_OK aria di %.0f s fa\n", a);
  } else if (!strcmp(args, "ARIA ARRIVO") || !strcmp(args, "ARIA ERRORE")) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.air.valid = false;
    weather.air.failed = args[5] == 'E';
    weather.version++;
    xSemaphoreGive(mutex);
    air_next_ms = millis() + 60000;
    Serial.println("METEO_OK");
  } else if (sscanf(args, "ARIA %d", &code) == 1) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    WeatherAir &w = weather.air;
    w.valid = true;
    w.updated_ms = millis();
    w.hour0 = time(nullptr) - time(nullptr) % 3600;
    w.aqi = code;
    for (int i = 0; i < WEATHER_HOURS; i++)
      w.aqi_hours[i] = (uint8_t)constrain(lroundf(code * (0.75f + 0.35f * cosf(i * 2 * (float)M_PI / 24))), 0, 255);
    w.aqi_hours[0] = code;
    w.pm25 = code * 0.55f;
    w.pm10 = code * 0.62f;
    w.no2 = code * 0.97f;
    w.o3 = 2;
    w.sub[0] = code;
    w.sub[1] = code * 3 / 4;
    w.sub[2] = code / 2;
    w.sub[3] = 5;
    weather.version++;
    xSemaphoreGive(mutex);
    Serial.println("METEO_OK");
  } else if (sscanf(args, "POLLINI %d %f", &code, &a) == 2 && code >= 0 && code < POLLEN_COUNT) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.air.pollen[code] = weather.air.pollen_max[code] = a;
    weather.version++;
    xSemaphoreGive(mutex);
    Serial.println("METEO_OK");
  } else if (sscanf(args, "LUNA %f", &a) == 1) {
    moon_offset_s = (int32_t)(a * 86400);
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.version++;
    xSemaphoreGive(mutex);
    Serial.println("METEO_OK");
  } else if (sscanf(args, "ETA %f", &a) == 1) {
    age_offset_ms = (uint32_t)(a * 1000);
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.version++;
    xSemaphoreGive(mutex);
    Serial.printf("METEO_OK dato di %.0f s fa\n", a);
  } else if (!strcmp(args, "ARRIVO") || !strcmp(args, "ERRORE")) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    weather.valid = false;
    weather.failed = args[0] == 'E';
    weather.version++;
    xSemaphoreGive(mutex);
    next_ms = millis() + 60000;
    Serial.println("METEO_OK");
  } else if (sscanf(args, "PROVA %d %f %d", &code, &a, &day) == 3) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool wet = code >= 51;
    weather.temp = weather.feels = a;
    weather.feels -= 1;
    weather.code = code;
    weather.is_day = day;
    for (int i = 0; i < WEATHER_HOURS; i++) {
      weather.hours[i].code = code;
      weather.hours[i].temp = (int8_t)lroundf(a - i / 3);
      weather.hours[i].rain = wet ? 40 + i * 2 : 0;
    }
    for (auto &d : weather.days) {
      d.code = code;
      d.tmin = (int8_t)lroundf(a - 6);
      d.tmax = (int8_t)lroundf(a + 2);
      d.rain = wet ? 90 : 0;
    }
    weather.version++;
    xSemaphoreGive(mutex);
    Serial.println("METEO_OK");
  } else if (!strcmp(args, "AUTO")) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    location_reset = true;
    Serial.println("METEO_OK posizione dall'IP");
  } else if (sscanf(args, "%f %f %n", &a, &b, &n) == 2 && n > 0 && args[n]) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putFloat("lat", a);
    prefs.putFloat("lon", b);
    prefs.putString("city", args + n);
    prefs.end();
    location_reset = true;
    Serial.printf("METEO_OK posizione fissata: %s\n", args + n);
  } else {
    Weather w;
    weather_get(&w);
    if (!w.valid) {
      Serial.println("METEO nessuna previsione ancora");
      return;
    }
    Serial.printf("METEO %s: %.1f °C (percepiti %.1f), codice %d, %s, aggiornato %lu s fa\n", w.city, w.temp, w.feels,
                  w.code, w.is_day ? "giorno" : "notte", (unsigned long)((millis() - w.updated_ms) / 1000));
    for (int d = 0; d < WEATHER_DAYS; d++)
      Serial.printf("  giorno +%d: %d/%d °C, codice %d, pioggia %d%%, alba %02d:%02d, tramonto %02d:%02d, UV %d\n", d,
                    w.days[d].tmin, w.days[d].tmax, w.days[d].code, w.days[d].rain, w.days[d].sunrise / 60,
                    w.days[d].sunrise % 60, w.days[d].sunset / 60, w.days[d].sunset % 60, w.days[d].uv);
    for (int i = 0; i < WEATHER_HOURS; i++)
      Serial.printf("  %02d:00 %d °C codice %d %s pioggia %d%%, aria %d\n", w.hours[i].hour, w.hours[i].temp,
                    w.hours[i].code, w.hours[i].is_day ? "giorno" : "notte", w.hours[i].rain, w.air.aqi_hours[i]);
    MoonInfo m;
    moon_info(time(nullptr), &m);
    Serial.printf("  luna: %s, %d%% illuminata, età %.1f giorni, piena fra %.1f, nuova fra %.1f\n",
                  moon_phase_text(m.phase), m.lit, m.age, m.to_full, m.to_new);
    const WeatherAir &a = w.air;
    if (!a.valid) {
      Serial.printf("  aria: %s\n", a.failed ? "non arrivata" : "non ancora chiesta");
      return;
    }
    Serial.printf("  aria: EAQI %d %s, PM2,5 %.1f, PM10 %.1f, NO2 %.1f, O3 %.1f, aggiornata %lu s fa\n", a.aqi,
                  air_level_text(air_level(a.aqi)), a.pm25, a.pm10, a.no2, a.o3,
                  (unsigned long)((millis() - a.updated_ms) / 1000));
    for (int p = 0; p < POLLEN_COUNT; p++)
      Serial.printf("  %s: adesso %.1f, massimo 24 h %.1f, livello %d\n", pollen_name((Pollen)p), a.pollen[p],
                    a.pollen_max[p], pollen_level((Pollen)p, a.pollen_max[p]));
  }
}
