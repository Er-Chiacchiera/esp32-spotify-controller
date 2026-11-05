// Città del meteo (design/citta/SPEC_CITTA.md §10): elenco salvato in NVS, ricerca con il geocoding di Open-Meteo,
// temperatura di adesso di tutte le città con una richiesta sola. Le richieste partono dal task Spotify (cities_tick).
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include "psram_json.h"
#include "spotify_internal.h"
#include "weather.h"

// weather.cpp
bool weather_get_json(const String &url, JsonDocument &doc);
void weather_default_changed();

static const char *NVS_NAMESPACE = "citta";
static const uint32_t NOW_VALID_MS = 30 * 60 * 1000;

static SemaphoreHandle_t mutex;
static City *saved;    // [CITY_MAX] in PSRAM
static int count = 0, def = -1;
static uint32_t version = 1;

// Ricerca
static City *results;  // [CITY_RESULTS] in PSRAM
static int result_count = 0;
static CitySearchState search_state = CS_IDLE;
static char search_text[100];
static volatile bool search_wanted = false;
static uint32_t search_seq = 0;

// Temperature dell'elenco, riconosciute dalle coordinate (le città aggiunte dal comando seriale non hanno id)
struct NowEntry {
  bool used;
  float lat, lon;
  CityNow now;
};
static NowEntry now_list[CITY_MAX + 1];
static uint32_t now_ms = 0;
static volatile bool now_wanted = false;

static void save()
{
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putBytes("list", saved, count * sizeof(City));
  p.putChar("def", def);
  p.end();
}

void cities_init()
{
  mutex = xSemaphoreCreateMutex();
  saved = (City *)heap_caps_calloc(CITY_MAX, sizeof(City), MALLOC_CAP_SPIRAM);
  results = (City *)heap_caps_calloc(CITY_RESULTS, sizeof(City), MALLOC_CAP_SPIRAM);
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  size_t len = p.getBytesLength("list");
  if (len && len % sizeof(City) == 0 && len <= CITY_MAX * sizeof(City)) {
    count = p.getBytes("list", saved, len) / sizeof(City);
    def = p.getChar("def", -1);
  }
  p.end();
  if (def >= count) def = -1;

  // Posizione fissata con il vecchio comando METEO (prima delle città): diventa una città predefinita
  Preferences old;
  old.begin("meteo", false);
  if (old.isKey("lat") && count < CITY_MAX) {
    City c = {};
    c.id = -1;
    c.lat = old.getFloat("lat");
    c.lon = old.getFloat("lon");
    strlcpy(c.name, old.getString("city", "Citt\xC3\xA0").c_str(), sizeof(c.name));
    saved[count] = c;
    def = count++;
    save();
    old.clear();
    Serial.printf("Meteo: posizione fissata %s spostata nell'elenco delle citt\xC3\xA0\n", c.name);
  }
  old.end();
}

int cities_count() { return count; }

bool cities_get(int i, City *out)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = i >= 0 && i < count;
  if (ok) *out = saved[i];
  xSemaphoreGive(mutex);
  return ok;
}

int cities_default() { return def; }
uint32_t cities_version() { return version; }

void cities_set_default(int i)
{
  if (i < -1 || i >= count || i == def) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  def = i;
  version++;
  save();
  xSemaphoreGive(mutex);
  weather_default_changed();
}

int cities_find(int32_t id)
{
  if (id == -1) return -1;
  for (int i = 0; i < count; i++)
    if (saved[i].id == id) return i;
  return -1;
}

int cities_add(const City &c)
{
  int i = cities_find(c.id);
  if (i >= 0) return i;
  if (count >= CITY_MAX) return -1;
  xSemaphoreTake(mutex, portMAX_DELAY);
  i = count;
  saved[count++] = c;
  version++;
  save();
  xSemaphoreGive(mutex);
  now_wanted = true;  // manca la sua temperatura
  return i;
}

void cities_remove(int i)
{
  if (i < 0 || i >= count) return;
  bool was_default = i == def;
  xSemaphoreTake(mutex, portMAX_DELAY);
  memmove(&saved[i], &saved[i + 1], (count - i - 1) * sizeof(City));
  count--;
  if (was_default) def = -1;
  else if (def > i) def--;
  version++;
  save();
  xSemaphoreGive(mutex);
  if (was_default) weather_default_changed();
}

// ---------- Ricerca ----------

void city_search(const char *text)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  strlcpy(search_text, text, sizeof(search_text));
  search_state = CS_LOADING;
  result_count = 0;
  search_seq++;
  xSemaphoreGive(mutex);
  search_wanted = true;
  work_signal();
}

CitySearchState city_search_get(City *out, int *n)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  CitySearchState st = search_state;
  if (n) *n = result_count;
  if (out && st == CS_OK) memcpy(out, results, result_count * sizeof(City));
  xSemaphoreGive(mutex);
  return st;
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

static void fetch_search()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  char text[100];
  strlcpy(text, search_text, sizeof(text));
  uint32_t seq = search_seq;
  xSemaphoreGive(mutex);

  uint32_t t0 = millis();
  String url = "http://geocoding-api.open-meteo.com/v1/search?count=" + String(CITY_RESULTS) +
               "&language=it&format=json&name=" + url_encode(text);
  JsonDocument doc(PsramAllocator::instance());
  bool ok = weather_get_json(url, doc);
  City *found = (City *)heap_caps_calloc(CITY_RESULTS, sizeof(City), MALLOC_CAP_SPIRAM);
  int n = 0;
  if (ok && found) {
    // Nessun risultato: la chiave "results" manca del tutto
    for (JsonObjectConst r : doc["results"].as<JsonArrayConst>()) {
      if (n >= CITY_RESULTS) break;
      City &c = found[n];
      c.id = r["id"] | 0;
      clean_text(r["name"] | "", c.name, sizeof(c.name), "?");
      clean_text(r["admin2"] | "", c.admin2, sizeof(c.admin2));
      clean_text(r["admin1"] | "", c.admin1, sizeof(c.admin1));
      clean_text(r["country"] | "", c.country, sizeof(c.country));
      c.lat = r["latitude"] | NAN;
      c.lon = r["longitude"] | NAN;
      c.population = r["population"] | 0;
      if (!isnan(c.lat) && !isnan(c.lon)) n++;
    }
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (seq == search_seq) {  // nel frattempo nessuna ricerca nuova
    search_state = ok && found ? CS_OK : CS_ERROR;
    result_count = n;
    if (found) memcpy(results, found, n * sizeof(City));
  }
  xSemaphoreGive(mutex);
  heap_caps_free(found);
  Serial.printf("Meteo: ricerca \"%s\" %s, %d risultati in %lu ms, RAM interna %lu KB\n", text, ok ? "riuscita" : "fallita",
                n, (unsigned long)(millis() - t0), (unsigned long)(ESP.getFreeHeap() / 1024));
}

// ---------- Temperature dell'elenco (§10.3) ----------

static NowEntry *now_find(float lat, float lon)
{
  for (auto &e : now_list)
    if (e.used && e.lat == lat && e.lon == lon) return &e;
  return nullptr;
}

void cities_now_request()
{
  bool missing = false;
  AutoLocation al;
  weather_auto_get(&al);
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (al.state == AUTO_OK) missing |= !now_find(al.lat, al.lon);
  for (int i = 0; i < count; i++) missing |= !now_find(saved[i].lat, saved[i].lon);
  xSemaphoreGive(mutex);
  if (missing || !now_ms || millis() - now_ms >= NOW_VALID_MS) {
    now_wanted = true;
    work_signal();
  }
}

bool cities_now_get(int i, CityNow *out)
{
  AutoLocation al;
  if (i < 0) weather_auto_get(&al);
  xSemaphoreTake(mutex, portMAX_DELAY);
  NowEntry *e = i < 0 ? (al.state == AUTO_OK ? now_find(al.lat, al.lon) : nullptr)
                      : i < count ? now_find(saved[i].lat, saved[i].lon) : nullptr;
  bool ok = e && e->now.valid;
  if (ok) *out = e->now;
  xSemaphoreGive(mutex);
  return ok;
}

static void fetch_now()
{
  // Posizione attuale (se stimata) e città salvate, con le coordinate separate da virgole
  NowEntry list[CITY_MAX + 1] = {};
  int n = 0;
  AutoLocation al;
  weather_auto_get(&al);
  if (al.state == AUTO_OK) list[n++] = { true, al.lat, al.lon, {} };
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) list[n++] = { true, saved[i].lat, saved[i].lon, {} };
  xSemaphoreGive(mutex);
  if (!n) return;
  String la, lo;
  for (int i = 0; i < n; i++) {
    la += (i ? "," : "") + String(list[i].lat, 4);
    lo += (i ? "," : "") + String(list[i].lon, 4);
  }
  uint32_t t0 = millis();
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + la + "&longitude=" + lo +
               "&current=temperature_2m,weather_code,is_day&timezone=auto";
  JsonDocument doc(PsramAllocator::instance());
  bool ok = weather_get_json(url, doc);
  if (ok) {
    // Con più città la risposta è un elenco nello stesso ordine; con una sola è l'oggetto e basta
    for (int i = 0; i < n; i++) {
      JsonObjectConst cur = doc.is<JsonArray>() ? doc[i]["current"].as<JsonObjectConst>()
                                                : doc["current"].as<JsonObjectConst>();
      if (!cur["temperature_2m"].is<float>()) continue;
      list[i].now = { true, (int8_t)lroundf(cur["temperature_2m"].as<float>()), cur["weather_code"] | 0,
                      cur["is_day"] | 1 };
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    memcpy(now_list, list, sizeof(now_list));
    now_ms = millis();
    version++;
    xSemaphoreGive(mutex);
  }
  Serial.printf("Meteo: temperature di %d citt\xC3\xA0 %s in %lu ms\n", n, ok ? "arrivate" : "non arrivate",
                (unsigned long)(millis() - t0));
}

bool cities_tick()
{
  if (search_wanted) {
    search_wanted = false;
    fetch_search();
    return true;
  }
  if (now_wanted) {
    now_wanted = false;
    fetch_now();
    return true;
  }
  return false;
}
