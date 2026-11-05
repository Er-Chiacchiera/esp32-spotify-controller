#include "spotify.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include "cover_color.h"
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <new>
#include "net.h"
#include "psram_json.h"
#include "spotify_internal.h"
#include "weather.h"

static const char *NVS_NAMESPACE = "spotify";
static const char *TOKEN_URL = "https://accounts.spotify.com/api/token";
static const char *API_BASE = "https://api.spotify.com/v1";
static const uint32_t RENEW_MARGIN_MS = 5 * 60 * 1000;  // rinnovo 5 minuti prima della scadenza
static const uint32_t POLL_MS = 1500;                   // stato della riproduzione
static const uint32_t POLL_AFTER_CMD_MS = 400;          // dopo un comando Spotify ci mette un attimo ad aggiornarsi
static const int COVER_MAX_SIDE = 320;                  // si usa la copertina da 300 px
static const size_t COVER_MAX_JPEG = 256 * 1024;

SemaphoreHandle_t sp_mutex;
#define mutex sp_mutex
static SpotifyStatus status;
static PlayerState player;
static CoverImage cover;
static volatile uint32_t cover_shown_version = 0;
static volatile bool creds_changed = false;

struct CmdMsg {
  SpotifyCommand cmd;
  int value;
};
static QueueHandle_t cmd_queue;

// Usati solo dal task Spotify
static String client_id, refresh_token, access_token;
static uint32_t access_expires_at_ms = 0;
// Connessione tenuta aperta verso api.spotify.com. Oggetti ricreati da zero dopo ogni errore:
// riusare lo stesso NetworkClientSecure dopo una caduta del Wi-Fi faceva fallire tutte le richieste (-1).
static NetworkClientSecure *api_client = nullptr;
static HTTPClient *api_http = nullptr;
static String cover_url_wanted, cover_url_loaded;
static uint16_t *cover_bufs[2];
static int cover_next_buf = 0;

enum TokenResult { TOKEN_OK, TOKEN_INVALID_GRANT, TOKEN_ERROR };

// Certificati verificati con il bundle di autorità già incluso nel sistema (mbedTLS).
// Da arduino-esp32 3.3 c'è useBuiltinCACertBundle(); nella 3.2 (SDK high_perf) va indicato a mano.
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void use_ca_bundle(NetworkClientSecure &client) { client.useBuiltinCACertBundle(); }
#else
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");
void use_ca_bundle(NetworkClientSecure &client)
{
  static bool logged = false;
  if (!logged) {
    logged = true;
    Serial.printf("Spotify: bundle certificati %u byte\n", (unsigned)(x509_crt_bundle_end - x509_crt_bundle_start));
  }
  client.setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
}
#endif

// ---------- Stato condiviso ----------

static void set_state(SpotifyState s, const char *error = nullptr)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  status.state = s;
  strlcpy(status.error, error ? error : "", sizeof(status.error));
  xSemaphoreGive(mutex);
}

static void set_player_message(const char *msg)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (strcmp(player.message, msg) != 0) {
    strlcpy(player.message, msg, sizeof(player.message));
    player.version++;
  }
  xSemaphoreGive(mutex);
}

void spotify_get_status(SpotifyStatus *out)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  *out = status;
  xSemaphoreGive(mutex);
}

void spotify_get_player(PlayerState *out)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  *out = player;
  xSemaphoreGive(mutex);
}

void spotify_get_cover(CoverImage *out)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  *out = cover;
  xSemaphoreGive(mutex);
}

void spotify_cover_shown(uint32_t version)
{
  cover_shown_version = version;
}

void spotify_command(SpotifyCommand cmd, int value)
{
  CmdMsg m = { cmd, value };
  xQueueSend(cmd_queue, &m, 0);
  work_signal();
}

bool spotify_set_credentials(const char *cid, const char *refresh)
{
  if (strlen(cid) != 32 || strlen(refresh) < 20) return false;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("client_id", cid);
  prefs.putString("refresh", refresh);
  prefs.end();
  creds_changed = true;
  return true;
}

static void load_credentials()
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  client_id = prefs.getString("client_id", "");
  refresh_token = prefs.getString("refresh", "");
  prefs.end();
  access_token = "";
  access_expires_at_ms = 0;
}

// ---------- HTTPS ----------

static String url_encode(const String &s)
{
  static const char HEX_CHARS[] = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += HEX_CHARS[(c >> 4) & 0xF];
      out += HEX_CHARS[c & 0xF];
    }
  }
  return out;
}

// Chiude la connessione con api.spotify.com: ogni sessione TLS occupa ~40 KB di RAM interna,
// quindi prima di aprirne un'altra (token, copertine) si chiude questa.
void api_close()
{
  if (api_http) {
    api_http->end();
    delete api_http;
    api_http = nullptr;
  }
  if (api_client) {
    api_client->stop();
    delete api_client;
    api_client = nullptr;
  }
}

// Richiesta HTTPS singola con certificati verificati (bundle integrato). Restituisce il codice HTTP (<0 se errore di rete).
static int https_request(const char *method, const String &url, const String &body, const char *content_type,
                         JsonDocument &doc)
{
  NetworkClientSecure client;
  use_ca_bundle(client);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return -1;
  if (content_type) http.addHeader("Content-Type", content_type);

  int code = http.sendRequest(method, body);
  if (code > 0) {
    String payload = http.getString();
    doc.clear();
    if (payload.length() > 0) deserializeJson(doc, payload);
  } else {
    char tls_err[100] = "";
    client.lastError(tls_err, sizeof(tls_err));
    Serial.printf("Spotify: %s %s fallita: %s (TLS: %s)\n", method, url.c_str(), HTTPClient::errorToString(code).c_str(),
                  tls_err);
  }
  http.end();
  return code;
}

// Richiesta alla Web API sulla connessione tenuta aperta.
static int api_request(const char *method, const String &path, JsonDocument &doc, JsonDocument *filter = nullptr,
                       uint32_t *retry_after_s = nullptr, const char *body = nullptr)
{
  if (!api_client) {
    api_client = new NetworkClientSecure();
    use_ca_bundle(*api_client);
    api_http = new HTTPClient();
    api_http->setReuse(true);
    api_http->setConnectTimeout(10000);
    api_http->setTimeout(10000);
  }
  HTTPClient &http = *api_http;
  bool reused = api_client->connected();  // misura dei tempi: connessione già aperta o nuova (handshake TLS)
  uint32_t t_start = millis();
  if (!http.begin(*api_client, String(API_BASE) + path)) {
    api_close();
    return -1;
  }
  http.addHeader("Authorization", "Bearer " + access_token);
  size_t body_len = body ? strlen(body) : 0;
  // HTTPClient non manda Content-Length con corpo vuoto, ma Spotify lo vuole su PUT/POST (altrimenti 411)
  if (body_len) http.addHeader("Content-Type", "application/json");
  else if (strcmp(method, "GET") != 0) http.addHeader("Content-Length", "0");
  const char *keys[] = { "Retry-After" };
  http.collectHeaders(keys, 1);

  int code = http.sendRequest(method, (uint8_t *)body, body_len);
  doc.clear();
  if (code > 0) {
    // 204 e risposte vuote: niente Content-Length, e getString() su una connessione tenuta aperta
    // aspetterebbe la chiusura da parte di Spotify (minuti) bloccando tutto il task
    String payload;
    uint32_t t_body = millis();
    if (code != 204 && code != 304 && http.getSize() != 0) payload = http.getString();
    uint32_t t_read = millis();
    if (payload.length() > 0) {
      if (filter) deserializeJson(doc, payload, DeserializationOption::Filter(*filter));
      else deserializeJson(doc, payload);
    }
    if (strstr(path.c_str(), "/me/player?") == nullptr)  // il controllo ogni 1,5 s non riempie il registro
      Serial.printf("Spotify: tempi %s: connessione %s, attesa risposta %lu ms, lettura %u KB %lu ms, JSON %lu ms\n",
                    path.substring(0, 40).c_str(), reused ? "riusata" : "NUOVA", (unsigned long)(t_body - t_start),
                    (unsigned)(payload.length() / 1024), (unsigned long)(t_read - t_body), (unsigned long)(millis() - t_read));
    if (retry_after_s && http.hasHeader("Retry-After")) *retry_after_s = http.header("Retry-After").toInt();
    http.end();
  } else {
    char tls_err[100] = "";
    api_client->lastError(tls_err, sizeof(tls_err));
    Serial.printf("Spotify: %s %s fallita: %s (TLS: %s), RAM interna %lu KB\n", method, path.c_str(),
                  HTTPClient::errorToString(code).c_str(), tls_err, (unsigned long)(ESP.getFreeHeap() / 1024));
    api_close();
  }
  return code;
}

static TokenResult renew_access_token();

int sp_api(const char *method, const String &path, JsonDocument &doc, JsonDocument *filter, const char *body)
{
  int code = api_request(method, path, doc, filter, nullptr, body);
  if (code == 401 && renew_access_token() == TOKEN_OK) code = api_request(method, path, doc, filter, nullptr, body);
  return code;
}

static TokenResult renew_access_token()
{
  api_close();
  JsonDocument doc(PsramAllocator::instance());
  String body = "grant_type=refresh_token&refresh_token=" + url_encode(refresh_token) + "&client_id=" + client_id;
  int code = https_request("POST", TOKEN_URL, body, "application/x-www-form-urlencoded", doc);

  if (code == 200 && doc["access_token"].is<const char *>()) {
    access_token = doc["access_token"].as<const char *>();
    uint32_t expires_in = doc["expires_in"] | 3600;
    access_expires_at_ms = millis() + expires_in * 1000;
    // Con PKCE Spotify può dare un refresh token nuovo: va salvato, il vecchio smette di funzionare
    const char *new_refresh = doc["refresh_token"] | "";
    if (new_refresh[0] && refresh_token != new_refresh) {
      refresh_token = new_refresh;
      Preferences prefs;
      prefs.begin(NVS_NAMESPACE, false);
      prefs.putString("refresh", refresh_token);
      prefs.end();
      Serial.println("Spotify: refresh token aggiornato in NVS");
    }
    Serial.printf("Spotify: token rinnovato, scade tra %lu min. RAM interna libera %lu KB (blocco max %lu KB)\n",
                  (unsigned long)(expires_in / 60), (unsigned long)(ESP.getFreeHeap() / 1024),
                  (unsigned long)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
    return TOKEN_OK;
  }

  const char *err = doc["error"] | "";
  Serial.printf("Spotify: rinnovo token fallito (HTTP %d) %s %s\n", code, err, (const char *)(doc["error_description"] | ""));
  if (code == 400 && strcmp(err, "invalid_grant") == 0) return TOKEN_INVALID_GRANT;
  return TOKEN_ERROR;
}

// Restituisce il codice HTTP di GET /me
static int fetch_profile()
{
  JsonDocument doc(PsramAllocator::instance());
  int code = api_request("GET", "/me", doc);
  if (code == 200) {
    const char *name = doc["display_name"] | "";
    const char *id = doc["id"] | "";
    xSemaphoreTake(mutex, portMAX_DELAY);
    strlcpy(status.display_name, name[0] ? name : id, sizeof(status.display_name));
    strlcpy(status.user_id, id, sizeof(status.user_id));
    xSemaphoreGive(mutex);
    Serial.printf("Spotify: GET /me ok, utente \"%s\"\n", name[0] ? name : id);
  } else {
    Serial.printf("Spotify: GET /me HTTP %d %s\n", code, (const char *)(doc["error"]["message"] | ""));
  }
  return code;
}

// ---------- Stato della riproduzione ----------

// Posizione attuale stimata in locale dall'ultimo stato ricevuto
static uint32_t estimated_progress(const PlayerState &p)
{
  uint32_t pos = p.progress_ms;
  if (p.is_playing) pos += millis() - p.progress_at_ms;
  return min(pos, p.duration_ms);
}

static int poll_player(uint32_t *retry_after_s)
{
  JsonDocument filter;
  filter["is_playing"] = true;
  filter["progress_ms"] = true;
  filter["device"]["name"] = true;
  filter["device"]["volume_percent"] = true;
  filter["device"]["supports_volume"] = true;
  filter["device"]["type"] = true;
  JsonObject item = filter["item"].to<JsonObject>();
  item["id"] = true;
  item["uri"] = true;
  item["name"] = true;
  item["duration_ms"] = true;
  item["artists"][0]["name"] = true;
  item["album"]["name"] = true;
  item["album"]["images"][0]["url"] = true;
  item["album"]["images"][0]["width"] = true;
  item["show"]["name"] = true;  // podcast
  item["images"][0]["url"] = true;
  item["images"][0]["width"] = true;
  filter["context"]["uri"] = true;

  JsonDocument doc(PsramAllocator::instance());
  uint32_t t0 = millis();
  int code = api_request("GET", "/me/player?additional_types=episode", doc, &filter, retry_after_s);
  static uint32_t polls = 0;
  if (++polls % 40 == 0) {  // circa una volta al minuto: segno di vita e memoria
    Serial.printf("Spotify: %lu controlli, ultimo HTTP %d in %lu ms, RAM interna libera %lu KB (minimo %lu KB), stack libero %lu byte\n", (unsigned long)polls,
                  code, (unsigned long)(millis() - t0), (unsigned long)(ESP.getFreeHeap() / 1024),
                  (unsigned long)(ESP.getMinFreeHeap() / 1024), (unsigned long)uxTaskGetStackHighWaterMark(NULL));
  }

  if (code == 204) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (player.has_playback) {
      player.has_playback = false;
      player.is_playing = false;
      player.version++;
    }
    xSemaphoreGive(mutex);
    // La copertina resta: se la riproduzione riprende sullo stesso brano non va riscaricata
    return code;
  }
  if (code != 200) return code;

  JsonObject it = doc["item"];
  JsonArray images = it["album"]["images"].isNull() ? it["images"].as<JsonArray>() : it["album"]["images"].as<JsonArray>();
  // Copertina più vicina a 300 px (Spotify dà 640/300/64)
  const char *best_url = "";
  int best_diff = 100000;
  for (JsonObject img : images) {
    int diff = abs((int)(img["width"] | 0) - 300);
    if (diff < best_diff) {
      best_diff = diff;
      best_url = img["url"] | "";
    }
  }
  cover_url_wanted = best_url;
  // Miniatura per la barra laterale: la più piccola non sotto i 56 px (Spotify dà 64)
  const char *small_url = "";
  int small_w = 100000;
  for (JsonObject img : images) {
    int w = img["width"] | 640;
    if (w >= 56 && w < small_w) {
      small_w = w;
      small_url = img["url"] | "";
    }
  }

  String artists;
  for (JsonObject a : it["artists"].as<JsonArray>()) {
    if (artists.length()) artists += ", ";
    artists += (const char *)(a["name"] | "");
  }
  if (artists.isEmpty()) artists = (const char *)(it["show"]["name"] | "");

  xSemaphoreTake(mutex, portMAX_DELAY);
  bool track_changed = strcmp(player.track_id, it["id"] | "") != 0;
  player.has_playback = true;
  player.is_playing = doc["is_playing"] | false;
  player.progress_ms = doc["progress_ms"] | 0;
  player.progress_at_ms = t0 + (millis() - t0) / 2;  // a metà richiesta: compensa il tempo di rete
  player.duration_ms = it["duration_ms"] | 0;
  strlcpy(player.track_id, it["id"] | "", sizeof(player.track_id));
  strlcpy(player.track_uri, it["uri"] | "", sizeof(player.track_uri));
  strlcpy(player.context_uri, doc["context"]["uri"] | "", sizeof(player.context_uri));
  strlcpy(player.cover_small, small_url, sizeof(player.cover_small));
  strlcpy(player.device_type, doc["device"]["type"] | "", sizeof(player.device_type));
  strlcpy(player.title, it["name"] | "", sizeof(player.title));
  strlcpy(player.artist, artists.c_str(), sizeof(player.artist));
  strlcpy(player.album, it["album"]["name"] | "", sizeof(player.album));
  strlcpy(player.device, doc["device"]["name"] | "", sizeof(player.device));
  player.volume = (doc["device"]["supports_volume"] | false) ? (int)(doc["device"]["volume_percent"] | 0) : -1;
  player.message[0] = '\0';
  player.version++;
  xSemaphoreGive(mutex);

  if (track_changed) Serial.printf("Spotify: ora suona \"%s\" - %s\n", player.title, player.artist);
  return code;
}

static int run_command(const CmdMsg &m)
{
  const char *method = "PUT";
  String path;
  const char *name = "";
  // Aggiorno subito lo stato locale, così lo schermo risponde senza aspettare Spotify
  xSemaphoreTake(mutex, portMAX_DELAY);
  uint32_t now_pos = estimated_progress(player);
  switch (m.cmd) {
    case CMD_PLAY:
      path = "/me/player/play";
      name = "play";
      player.progress_ms = now_pos;
      player.progress_at_ms = millis();
      player.is_playing = true;
      break;
    case CMD_PAUSE:
      path = "/me/player/pause";
      name = "pausa";
      player.progress_ms = now_pos;
      player.progress_at_ms = millis();
      player.is_playing = false;
      break;
    case CMD_NEXT:
      method = "POST";
      path = "/me/player/next";
      name = "avanti";
      break;
    case CMD_PREVIOUS:
      method = "POST";
      path = "/me/player/previous";
      name = "indietro";
      break;
    case CMD_VOLUME:
      path = "/me/player/volume?volume_percent=" + String(constrain(m.value, 0, 100));
      name = "volume";
      player.volume = constrain(m.value, 0, 100);
      break;
    case CMD_SEEK:
      path = "/me/player/seek?position_ms=" + String(max(m.value, 0));
      name = "posizione";
      player.progress_ms = max(m.value, 0);
      player.progress_at_ms = millis();
      break;
  }
  player.version++;
  xSemaphoreGive(mutex);

  JsonDocument doc(PsramAllocator::instance());
  int code = api_request(method, path, doc);
  if (code == 401 && renew_access_token() == TOKEN_OK) code = api_request(method, path, doc);
  Serial.printf("Spotify: comando %s -> HTTP %d %s\n", name, code, (const char *)(doc["error"]["message"] | ""));

  if (code == 404) {
    set_player_message("Nessun dispositivo attivo: apri Spotify sul PC o sul telefono");
    xSemaphoreTake(mutex, portMAX_DELAY);
    player.no_device_count++;
    player.last_cmd_was_play = m.cmd == CMD_PLAY;
    if (m.cmd == CMD_PLAY) player.is_playing = false;
    player.version++;
    xSemaphoreGive(mutex);
  }
  else if (code == 403) set_player_message("Comando non disponibile in questo momento");
  else if (code == 200 || code == 202 || code == 204) set_player_message("");
  return code;
}

// ---------- Copertina ----------

static uint16_t *decode_target;
static int decode_w, decode_h;

static int jpeg_draw(JPEGDRAW *d)
{
  int w = min(d->iWidthUsed, decode_w - d->x);
  for (int y = 0; y < d->iHeight && d->y + y < decode_h; y++) {
    if (w > 0) memcpy(decode_target + (d->y + y) * decode_w + d->x, d->pPixels + y * d->iWidth, w * 2);
  }
  return 1;
}

static void publish_cover(const uint16_t *pixels, int w, int h, uint32_t bg_rgb = COVER_BG_NONE)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  cover.pixels = pixels;
  cover.width = w;
  cover.height = h;
  cover.bg_rgb = bg_rgb;
  cover.version++;
  xSemaphoreGive(mutex);
}

// Colori di fondo già calcolati, con chiave l'URL della copertina (SPEC_COLORE §4): ultimi 16
static uint32_t color_cache_key[16], color_cache_rgb[16];
static int color_cache_next = 0;

static uint32_t url_hash(const String &url)
{
  uint32_t h = 2166136261u;  // FNV-1a
  for (size_t i = 0; i < url.length(); i++) h = (h ^ (uint8_t)url[i]) * 16777619u;
  return h ? h : 1;
}

static uint32_t cover_bg_for(const String &url, const uint16_t *pixels, int w, int h)
{
  uint32_t key = url_hash(url);
  for (int i = 0; i < 16; i++) {
    if (color_cache_key[i] == key) return color_cache_rgb[i];
  }
  uint32_t t0 = micros();
  uint32_t rgb = cover_color(pixels, w, h);
  color_cache_key[color_cache_next] = key;
  color_cache_rgb[color_cache_next] = rgb;
  color_cache_next = (color_cache_next + 1) % 16;
  Serial.printf("Spotify: colore di fondo #%06lX in %lu us\n", (unsigned long)rgb, (unsigned long)(micros() - t0));
  return rgb;
}

static void load_cover(const String &url)
{
  if (url.isEmpty()) {
    publish_cover(nullptr, 0, 0);
    return;
  }
  uint32_t t0 = millis();
  PsramSink sink(COVER_MAX_JPEG);
  // In HTTP semplice come le miniature (spotify_browse.cpp): niente seconda sessione TLS, e la
  // connessione con api.spotify.com resta aperta (prima ogni cambio di brano la chiudeva)
  if (!image_download(url.c_str(), sink)) {
    publish_cover(nullptr, 0, 0);
    return;
  }

  // Il decoder (~18 KB) sta in RAM interna solo durante la decodifica: in PSRAM il suo lavorio
  // contende la memoria con lo schermo e fa glitchare. Se con la sessione TLS aperta non c'è un
  // blocco abbastanza grande, chiudo la sessione (si riaprirà alla prossima richiesta).
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < sizeof(JPEGDEC) + 4096) api_close();
  bool internal = true;
  void *mem = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!mem) {
    internal = false;
    mem = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_SPIRAM);
  }
  if (!mem) {
    publish_cover(nullptr, 0, 0);
    return;
  }
  JPEGDEC *jpeg = new (mem) JPEGDEC();
  struct Cleanup {
    JPEGDEC *j;
    ~Cleanup() { j->~JPEGDEC(); heap_caps_free(j); }
  } cleanup{ jpeg };

  if (!jpeg->openRAM(sink.data, sink.len, jpeg_draw)) {
    Serial.printf("Spotify: copertina non leggibile (errore %d)\n", jpeg->getLastError());
    publish_cover(nullptr, 0, 0);
    return;
  }
  int scale = 0, w = jpeg->getWidth(), h = jpeg->getHeight();
  while ((w > COVER_MAX_SIDE || h > COVER_MAX_SIDE) && scale < 3) {  // 1/2, 1/4, 1/8
    scale++;
    w = (w + 1) / 2;
    h = (h + 1) / 2;
  }
  if (w > COVER_MAX_SIDE || h > COVER_MAX_SIDE) {
    jpeg->close();
    publish_cover(nullptr, 0, 0);
    return;
  }
  decode_target = cover_bufs[cover_next_buf];
  decode_w = w;
  decode_h = h;
  memset(decode_target, 0, w * h * 2);
  jpeg->setPixelType(RGB565_LITTLE_ENDIAN);
  int ok = jpeg->decode(0, 0, scale ? (1 << scale) : 0);  // JPEG_SCALE_HALF=2, QUARTER=4, EIGHTH=8
  jpeg->close();
  if (!ok) {
    Serial.printf("Spotify: decodifica copertina fallita (errore %d)\n", jpeg->getLastError());
    publish_cover(nullptr, 0, 0);
    return;
  }
  publish_cover(decode_target, w, h, cover_bg_for(url, decode_target, w, h));
  cover_next_buf ^= 1;
  Serial.printf("Spotify: copertina %dx%d (%u KB) in %lu ms, decoder %s (%u byte)\n", w, h, (unsigned)(sink.len / 1024),
                (unsigned long)(millis() - t0), internal ? "in RAM interna" : "in PSRAM", (unsigned)sizeof(JPEGDEC));
}

// ---------- Task ----------

static void spotify_task(void *arg)
{
  load_credentials();
  uint32_t backoff_s = 5;
  bool profile_ok = false;
  uint32_t next_poll_ms = 0;
  bool net_was_ok = false;

  while (true) {
    if (creds_changed) {
      creds_changed = false;
      api_close();
      load_credentials();
      profile_ok = false;
      backoff_s = 5;
      set_state(SP_CONNECTING);
      Serial.println("Spotify: nuove credenziali ricevute");
    }

    if (refresh_token.isEmpty() || client_id.isEmpty()) {
      set_state(SP_NO_CREDENTIALS);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    NetStatus net;
    net_get_status(&net);
    bool net_ok = net.state == NET_CONNECTED && net.time_ok;
    if (!net_ok) {
      if (net_was_ok) api_close();
      net_was_ok = false;
      set_state(SP_WAITING_NETWORK);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    net_was_ok = true;

    SpotifyStatus st;
    spotify_get_status(&st);
    if (st.state == SP_LOGIN_REQUIRED) {  // resto fermo finché non arriva un nuovo token
      weather_tick();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (access_token.isEmpty() || (int32_t)(access_expires_at_ms - millis()) < (int32_t)RENEW_MARGIN_MS) {
      if (st.state != SP_READY) set_state(SP_CONNECTING);
      TokenResult r = renew_access_token();
      if (r == TOKEN_INVALID_GRANT) {
        set_state(SP_LOGIN_REQUIRED);
        continue;
      }
      if (r == TOKEN_ERROR) {
        set_state(SP_ERROR, "rinnovo token non riuscito");
        weather_tick();  // il meteo dell'Orologio non dipende da Spotify
        vTaskDelay(pdMS_TO_TICKS(backoff_s * 1000));
        backoff_s = min<uint32_t>(backoff_s * 2, 120);
        continue;
      }
    }

    if (!profile_ok) {
      int code = fetch_profile();
      if (code == 200) {
        profile_ok = true;
        backoff_s = 5;
        set_state(SP_READY);
      } else if (code == 401) {
        access_token = "";  // token non più valido: rinnovo al giro dopo
        continue;
      } else {
        char err[64];
        snprintf(err, sizeof(err), "errore %d, riprovo", code);
        set_state(SP_ERROR, err);
        weather_tick();
        vTaskDelay(pdMS_TO_TICKS(backoff_s * 1000));
        backoff_s = min<uint32_t>(backoff_s * 2, 120);
        continue;
      }
    } else if (st.state != SP_READY) {
      // Rete tornata dopo una caduta o token rinnovato dopo un errore: il profilo c'è già, si riparte
      // senza rifare GET /me (prima lo stato restava "mi collego..." con tutto che funzionava)
      backoff_s = 5;
      set_state(SP_READY);
      Serial.println("Spotify: di nuovo collegato");
    }

    // Comandi dall'interfaccia: hanno la precedenza su tutto il resto
    CmdMsg m;
    if (xQueueReceive(cmd_queue, &m, 0)) {
      if (run_command(m) == 401) access_token = "";
      next_poll_ms = millis() + POLL_AFTER_CMD_MS;
      continue;
    }
    // Poi le richieste delle schermate (elenchi, avvio di un brano)
    int job = browse_process_one();
    if (job >= 0) {
      if (job >= ACT_PLAY) next_poll_ms = millis() + POLL_AFTER_CMD_MS;
      continue;
    }

    // Miniature nel tempo libero fra un controllo dello stato e l'altro
    int32_t poll_in = (int32_t)(next_poll_ms - millis());
    if (poll_in > 0) {
      if (images_process_one()) continue;
      if (weather_tick()) continue;
      if (uxQueueMessagesWaiting(cmd_queue) == 0) work_wait(min<int32_t>(poll_in, 1000));
      continue;
    }

    {
      uint32_t retry_after_s = 0;
      int code = poll_player(&retry_after_s);
      next_poll_ms = millis() + POLL_MS;
      if (code == 200 || code == 204) {
        set_player_message("");
      } else if (code == 401) {
        access_token = "";
      } else if (code == 429) {
        Serial.printf("Spotify: troppe richieste, aspetto %lu s\n", (unsigned long)retry_after_s);
        next_poll_ms = millis() + max<uint32_t>(retry_after_s, 2) * 1000;
      } else {
        Serial.printf("Spotify: stato riproduzione HTTP %d\n", code);
        set_player_message("Connessione a Spotify instabile, riprovo...");
        next_poll_ms = millis() + 3000;
      }

      // Copertina nuova: solo quando l'interfaccia ha già mostrato l'ultima, così il buffer libero non è sullo schermo
      if (cover_url_wanted != cover_url_loaded && cover_shown_version == cover.version) {
        cover_url_loaded = cover_url_wanted;
        load_cover(cover_url_wanted);
      }

      xSemaphoreTake(mutex, portMAX_DELAY);
      int32_t left = (int32_t)(access_expires_at_ms - millis());
      status.token_expires_in_s = left > 0 ? left / 1000 : 0;
      xSemaphoreGive(mutex);
    }
  }
}

void spotify_start()
{
  mutex = xSemaphoreCreateMutex();
  cmd_queue = xQueueCreate(8, sizeof(CmdMsg));
  browse_init();
  weather_init();
  memset(&status, 0, sizeof(status));
  memset(&player, 0, sizeof(player));
  memset(&cover, 0, sizeof(cover));
  cover.bg_rgb = COVER_BG_NONE;
  status.state = SP_NO_CREDENTIALS;
  player.volume = -1;

  for (int i = 0; i < 2; i++) {
    cover_bufs[i] = (uint16_t *)heap_caps_malloc(COVER_MAX_SIDE * COVER_MAX_SIDE * 2, MALLOC_CAP_SPIRAM);
  }

  // Core 0 come la rete. Stack: al massimo ~5 KB usati (log "stack libero" ogni 40 controlli).
  // Priorità 0, come IDLE0: quando i dati TLS arrivano a pezzi HTTPClient::getString() gira in un ciclo
  // con delay(0), che non cede la CPU a task di priorità più bassa; con priorità 2 IDLE0 non girava per
  // 5 s e il task watchdog riavviava la scheda ("task_wdt: IDLE0 (CPU 0)"). Alla pari si alternano.
  xTaskCreatePinnedToCore(spotify_task, "spotify", 14 * 1024, NULL, tskIDLE_PRIORITY, NULL, 0);
}
