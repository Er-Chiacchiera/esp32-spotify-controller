// Navigazione (fase 6): richieste degli elenchi, comandi con risposta e miniature delle copertine.
// Tutto gira nel task Spotify (core 0); l'interfaccia mette le richieste in coda e ritira i risultati.

#include <Arduino.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <new>
#include "psram_json.h"
#include "spotify_internal.h"

static const int RESULT_SLOTS = 6;
static const uint32_t RESULT_MAX_AGE_MS = 60 * 1000;  // risultati mai ritirati (schermata chiusa)
static const int SMALL_SLOTS = 60;                    // miniature da 64 px: 60 x 8 KB
static const int MID_SLOTS = 10;                      // foto degli artisti nella Home, 80 px: 10 x 13 KB
static const int BIG_SLOTS = 10;                      // pannello dettaglio e riquadri della Home, 150 px: 10 x 45 KB
#define PL_FIELDS "name,uri,duration_ms,artists(name,uri),album(uri,images),show(name),images"
static const size_t THUMB_MAX_JPEG = 256 * 1024;  // copertine caricate dagli utenti: solo 640 px

// Per scorrere con for(:) gli array allocati in PSRAM
template <typename T> struct Span {
  T *p;
  int n;
  T *begin() const { return p; }
  T *end() const { return p + n; }
};

struct Job {
  uint32_t id;
  BrowseRequest req;
};

struct ResultSlot {
  bool used;
  uint32_t stored_ms;
  BrowseResult res;
};

static QueueHandle_t job_queue;
static QueueHandle_t bg_queue;  // pagine chieste in anticipo: dopo le miniature visibili
static bool next_background = false;
static SemaphoreHandle_t work_sem;
static uint32_t next_job_id = 1;
static ResultSlot *results;  // in PSRAM (la RAM interna serve a TLS)

// ---------- Utilità ----------

void spotify_free(void *p) { heap_caps_free(p); }

char *spotify_strdup(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = (char *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  if (p) memcpy(p, s, n);
  return p;
}

void work_signal() { xSemaphoreGive(work_sem); }
void work_wait(uint32_t ms) { xSemaphoreTake(work_sem, pdMS_TO_TICKS(ms)); }

static String url_encode(const char *s)
{
  static const char HEX_CHARS[] = "0123456789ABCDEF";
  String out;
  for (; *s; s++) {
    unsigned char c = *s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += HEX_CHARS[c >> 4];
      out += HEX_CHARS[c & 0xF];
    }
  }
  return out;
}

// Caratteri presenti nei font_it_* (CARATTERI_E_ICONE.md)
static bool cp_in_font(uint32_t c)
{
  if (c >= 0x2010 && c <= 0x2027) {  // Montserrat non ha tutti i segni di questo blocco
    return !(c == 0x2011 || c == 0x2016 || c == 0x2017 || c == 0x201B || c == 0x201F || (c >= 0x2023 && c <= 0x2025) || c == 0x2027);
  }
  return (c >= 0x20 && c <= 0x7E) || (c >= 0xA0 && c <= 0x17F) || c == 0x20AC;
}

static bool cp_is_separator(uint32_t c)
{
  return c == ' ' || c == '-' || c == 0xB7 || c == 0x2013 || c == 0x2014 || c == ',' || c == '/' || c == '|' ||
         c == ':' || c == 0x2022 || c == 0xA0;
}

static bool cp_is_alnum(uint32_t c)
{
  return (c < 0x80 && isalnum(c)) || (c >= 0xC0 && c <= 0x17F && c != 0xD7 && c != 0xF7);
}

// Toglie i caratteri che il font non ha (giapponese, cirillico...), comprime gli spazi e i separatori
// rimasti soli. Se non resta nessuna lettera o cifra scrive il testo di riserva (SPEC §3.5).
static void clean_text(const char *in, char *out, size_t n, const char *fallback = nullptr, uint8_t *flags = nullptr)
{
  static const int MAX_CP = 160;
  uint32_t cps[MAX_CP];
  int len = 0;
  const uint8_t *p = (const uint8_t *)(in ? in : "");
  while (*p && len < MAX_CP) {
    uint32_t c;
    int extra;
    if (*p < 0x80) { c = *p; extra = 0; }
    else if ((*p & 0xE0) == 0xC0) { c = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { c = *p & 0x0F; extra = 2; }
    else if ((*p & 0xF8) == 0xF0) { c = *p & 0x07; extra = 3; }
    else { p++; continue; }
    p++;
    bool bad = false;
    for (int i = 0; i < extra; i++) {
      if ((*p & 0xC0) != 0x80) { bad = true; break; }
      c = (c << 6) | (*p++ & 0x3F);
    }
    if (bad) continue;
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    if (c == 0x2011) c = '-';  // trattino che non va a capo
    if (!cp_in_font(c)) {
      c = ' ';  // al posto delle lettere tolte: separa le parole rimaste
    }
    if (c == ' ' && (len == 0 || cps[len - 1] == ' ')) continue;  // spazi doppi
    cps[len++] = c;
  }
  // Parentesi rimaste vuote: "( )" e "()"
  for (int i = 0; i < len; i++) {
    if (cps[i] != '(' && cps[i] != '[') continue;
    uint32_t close = cps[i] == '(' ? ')' : ']';
    int j = i + 1;
    while (j < len && cps[j] == ' ') j++;
    if (j < len && cps[j] == close) {
      memmove(&cps[i], &cps[j + 1], (len - j - 1) * sizeof(uint32_t));
      len -= j + 1 - i;
      i = -1;  // ricomincio: potrebbero essercene altre annidate
    }
  }
  // Separatori soli all'inizio e alla fine
  int start = 0;
  while (start < len && cp_is_separator(cps[start])) start++;
  while (len > start && cp_is_separator(cps[len - 1])) len--;

  bool any = false;
  for (int i = start; i < len; i++) any |= cp_is_alnum(cps[i]);
  if (!any) {
    strlcpy(out, fallback ? fallback : "", n);
    if (flags && fallback) *flags |= IF_TITLE_FALLBACK;
    return;
  }
  size_t o = 0;
  for (int i = start; i < len; i++) {
    uint32_t c = cps[i];
    uint8_t buf[3];
    int k;
    if (c < 0x80) { buf[0] = c; k = 1; }
    else if (c < 0x800) { buf[0] = 0xC0 | (c >> 6); buf[1] = 0x80 | (c & 0x3F); k = 2; }
    else { buf[0] = 0xE0 | (c >> 12); buf[1] = 0x80 | ((c >> 6) & 0x3F); buf[2] = 0x80 | (c & 0x3F); k = 3; }
    if (o + k >= n) break;  // non spezzo un carattere a metà
    memcpy(out + o, buf, k);
    o += k;
  }
  out[o] = '\0';
}

// Aggiunge " · testo" (o solo testo se dst è vuoto)
static void append_part(char *dst, size_t n, const char *part)
{
  if (!part || !part[0]) return;
  if (dst[0]) strlcat(dst, " \xC2\xB7 ", n);
  strlcat(dst, part, n);
}

// Scelta dell'immagine (SPEC §3.6): fra le misure di Spotify e le riduzioni 1, 1/2, 1/4, 1/8 del decoder
// JPEG prendo quella più vicina al riquadro e non più piccola del riquadro - 8 px.
static void pick_image(JsonArrayConst images, int box, char *url, size_t url_n, uint8_t *scale)
{
  url[0] = '\0';
  *scale = 0;
  int best = INT_MAX, best_w = INT_MAX, fallback_w = -1;
  const char *fallback_url = "";
  for (JsonObjectConst img : images) {
    const char *u = img["url"] | "";
    if (!u[0]) continue;
    int w = img["width"] | 640;  // le copertine caricate dagli utenti non dicono la misura
    for (int s = 0; s <= 3; s++) {
      int d = (w + (1 << s) - 1) >> s;
      // A parità di misura finale la sorgente più piccola (meno da scaricare: 160/2 invece di 640/8)
      if (d >= box - 8 && (d < best || (d == best && w < best_w))) {
        best = d;
        best_w = w;
        strlcpy(url, u, url_n);
        *scale = s;
      }
    }
    if (w > fallback_w) {
      fallback_w = w;
      fallback_url = u;
    }
  }
  if (!url[0] && fallback_url[0]) strlcpy(url, fallback_url, url_n);  // tutte piccole: la più grande
}

static void join_artists(JsonArrayConst artists, char *out, size_t n)
{
  out[0] = '\0';
  for (JsonObjectConst a : artists) {
    char name[64];
    clean_text(a["name"] | "", name, sizeof(name));
    if (!name[0]) continue;
    if (out[0]) strlcat(out, ", ", n);
    strlcat(out, name, n);
  }
}

static const char *device_type_label(const char *t)
{
  static const char *const MAP[][2] = {
    { "Computer", "Computer" }, { "Smartphone", "Smartphone" }, { "Tablet", "Tablet" }, { "Speaker", "Cassa" },
    { "TV", "TV" }, { "AVR", "Amplificatore" }, { "STB", "Decoder TV" }, { "AudioDongle", "Chiavetta audio" },
    { "GameConsole", "Console" }, { "CastVideo", "Chromecast" }, { "CastAudio", "Chromecast audio" },
    { "Automobile", "Auto" }, { "Smartwatch", "Orologio" },
  };
  for (auto &m : MAP)
    if (strcasecmp(t, m[0]) == 0) return m[1];
  return t[0] ? t : "Dispositivo";
}

DeviceIcon spotify_device_icon(const char *t)
{
  if (strcasecmp(t, "Computer") == 0) return DI_COMPUTER;
  if (strcasecmp(t, "Smartphone") == 0) return DI_PHONE;
  if (strcasecmp(t, "Tablet") == 0) return DI_TABLET;
  if (!strcasecmp(t, "Speaker") || !strcasecmp(t, "AVR") || !strcasecmp(t, "AudioDongle") || !strcasecmp(t, "CastAudio"))
    return DI_SPEAKER;
  if (!strcasecmp(t, "TV") || !strcasecmp(t, "CastVideo") || !strcasecmp(t, "STB")) return DI_TV;
  if (strcasecmp(t, "GameConsole") == 0) return DI_GAMEPAD;
  if (strcasecmp(t, "Automobile") == 0) return DI_CAR;
  return DI_OTHER;
}

// ---------- Conversione degli oggetti Spotify in voci ----------

enum TrackSubtitle { SUB_ARTISTS, SUB_ARTISTS_ALBUM };

static void fill_track(BrowseItem &it, JsonObjectConst t, TrackSubtitle sub = SUB_ARTISTS)
{
  it.type = IT_TRACK;
  if (t.isNull()) {  // brano non più disponibile: la riga resta per non sfalsare la numerazione
    strlcpy(it.title, "Brano non disponibile", sizeof(it.title));
    it.flags |= IF_TITLE_FALLBACK;
    return;
  }
  clean_text(t["name"] | "", it.title, sizeof(it.title), "Titolo non latino", &it.flags);
  join_artists(t["artists"], it.subtitle, sizeof(it.subtitle));
  if (!it.subtitle[0]) clean_text(t["show"]["name"] | "", it.subtitle, sizeof(it.subtitle));  // podcast
  if (sub == SUB_ARTISTS_ALBUM) {
    char album[100];
    clean_text(t["album"]["name"] | "", album, sizeof(album));
    append_part(it.subtitle, sizeof(it.subtitle), album);
  }
  strlcpy(it.uri, t["uri"] | "", sizeof(it.uri));
  strlcpy(it.album_uri, t["album"]["uri"] | "", sizeof(it.album_uri));
  strlcpy(it.artist_uri, t["artists"][0]["uri"] | "", sizeof(it.artist_uri));
  clean_text(t["artists"][0]["name"] | "", it.artist_name, sizeof(it.artist_name), "Nome non latino");
  it.duration_ms = t["duration_ms"] | 0;
  it.number = t["track_number"] | 0;
  JsonArrayConst images = t["album"]["images"].isNull() ? t["images"].as<JsonArrayConst>() : t["album"]["images"].as<JsonArrayConst>();
  pick_image(images, 64, it.img, sizeof(it.img), &it.img_scale);
  pick_image(images, 150, it.img_big, sizeof(it.img_big), &it.img_big_scale);
}

enum AlbumSubtitle { SUB_ARTISTS_YEAR, SUB_YEAR };

static void fill_album(BrowseItem &it, JsonObjectConst a, AlbumSubtitle sub = SUB_ARTISTS_YEAR)
{
  it.type = IT_ALBUM;
  clean_text(a["name"] | "", it.title, sizeof(it.title), "Titolo non latino", &it.flags);
  const char *date = a["release_date"] | "";
  it.year = atoi(date);
  char year[8] = "";
  if (it.year) snprintf(year, sizeof(year), "%u", it.year);
  if (sub == SUB_ARTISTS_YEAR) join_artists(a["artists"], it.subtitle, sizeof(it.subtitle));
  append_part(it.subtitle, sizeof(it.subtitle), year);
  strlcpy(it.uri, a["uri"] | "", sizeof(it.uri));
  strlcpy(it.artist_uri, a["artists"][0]["uri"] | "", sizeof(it.artist_uri));
  clean_text(a["artists"][0]["name"] | "", it.artist_name, sizeof(it.artist_name), "Nome non latino");
  it.count = a["total_tracks"] | 0;
  pick_image(a["images"], 64, it.img, sizeof(it.img), &it.img_scale);
  pick_image(a["images"], 150, it.img_big, sizeof(it.img_big), &it.img_big_scale);
}

static void fill_artist(BrowseItem &it, JsonObjectConst a)
{
  it.type = IT_ARTIST;
  clean_text(a["name"] | "", it.title, sizeof(it.title), "Nome non latino", &it.flags);
  int n = 0;
  for (JsonVariantConst g : a["genres"].as<JsonArrayConst>()) {
    if (n++ == 2) break;
    char genre[48];
    clean_text(g | "", genre, sizeof(genre));
    if (!genre[0]) continue;
    if (it.subtitle[0]) strlcat(it.subtitle, ", ", sizeof(it.subtitle));
    strlcat(it.subtitle, genre, sizeof(it.subtitle));
  }
  if (!it.subtitle[0]) strlcpy(it.subtitle, "Artista", sizeof(it.subtitle));
  strlcpy(it.uri, a["uri"] | "", sizeof(it.uri));
  strlcpy(it.artist_uri, it.uri, sizeof(it.artist_uri));
  strlcpy(it.artist_name, it.title, sizeof(it.artist_name));
  pick_image(a["images"], 64, it.img, sizeof(it.img), &it.img_scale);
  pick_image(a["images"], 150, it.img_big, sizeof(it.img_big), &it.img_big_scale);
}

static void fill_playlist(BrowseItem &it, JsonObjectConst p, const char *user_id)
{
  it.type = IT_PLAYLIST;
  clean_text(p["name"] | "", it.title, sizeof(it.title), "Nome non latino", &it.flags);
  clean_text(p["owner"]["display_name"] | (p["owner"]["id"] | ""), it.subtitle, sizeof(it.subtitle), "Nome non latino");
  if (p["collaborative"] | false) {
    it.flags |= IF_COLLABORATIVE;
    append_part(it.subtitle, sizeof(it.subtitle), "collaborativa");
  }
  if (user_id[0] && strcmp(p["owner"]["id"] | "", user_id) == 0) it.flags |= IF_OWNED;
  // 2026: "tracks" è diventato "items"; per le playlist di altri il totale non c'è
  JsonVariantConst total = p["items"]["total"];
  if (total.isNull()) total = p["tracks"]["total"];
  if (total.isNull()) {
    it.flags |= IF_NO_COUNT;
  } else {
    it.count = total.as<int>();
    char n[24];
    snprintf(n, sizeof(n), "%u %s", it.count, it.count == 1 ? "brano" : "brani");
    append_part(it.subtitle, sizeof(it.subtitle), n);
  }
  strlcpy(it.uri, p["uri"] | "", sizeof(it.uri));
  pick_image(p["images"], 64, it.img, sizeof(it.img), &it.img_scale);
  pick_image(p["images"], 150, it.img_big, sizeof(it.img_big), &it.img_big_scale);
}

static void fill_device(BrowseItem &it, JsonObjectConst d)
{
  it.type = IT_DEVICE;
  clean_text(d["name"] | "", it.title, sizeof(it.title), "Nome non latino", &it.flags);
  const char *type = d["type"] | "";
  it.icon = spotify_device_icon(type);
  strlcpy(it.subtitle, device_type_label(type), sizeof(it.subtitle));
  if (d["is_restricted"] | false) {
    it.flags |= IF_RESTRICTED;
    append_part(it.subtitle, sizeof(it.subtitle), "non controllabile da qui");
  } else if ((d["supports_volume"] | false) && !d["volume_percent"].isNull()) {
    char v[24];
    snprintf(v, sizeof(v), "volume %d%%", d["volume_percent"].as<int>());
    append_part(it.subtitle, sizeof(it.subtitle), v);
  }
  if (d["is_active"] | false) it.flags |= IF_ACTIVE;
  strlcpy(it.uri, d["id"] | "", sizeof(it.uri));
}

static BrowseItem *alloc_items(int n)
{
  if (n <= 0) return nullptr;
  return (BrowseItem *)heap_caps_calloc(n, sizeof(BrowseItem), MALLOC_CAP_SPIRAM);
}

// "2026-11-03T14:12:33.123Z" -> secondi Unix (UTC)
static uint32_t parse_iso_utc(const char *s)
{
  int y, mo, d, h, mi, se;
  if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return 0;
  y -= mo <= 2;  // giorni dal 1970 (algoritmo "days from civil")
  int era = y / 400;
  unsigned yoe = y - era * 400;
  unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int32_t days = era * 146097 + (int32_t)doe - 719468;
  return days * 86400 + h * 3600 + mi * 60 + se;
}

// Ascolti recenti raggruppati per contesto, il più recente prima (SPEC 7 §3.1 e §5): una voce per ogni
// playlist/album/artista (o brano, se il contesto manca) con l'ultimo brano ascoltato lì
static void fill_recent(BrowseResult &res, JsonArrayConst arr, const char *user_id)
{
  int n = arr.size();
  res.items = alloc_items(n);
  char(*keys)[72] = (char(*)[72])heap_caps_malloc(max(n, 1) * 72, MALLOC_CAP_SPIRAM);
  if (!res.items || !keys) {
    heap_caps_free(keys);
    return;
  }
  for (JsonVariantConst v : arr) {
    JsonObjectConst t = v["track"];
    if (t.isNull()) continue;
    const char *ctx = v["context"]["uri"] | "";
    const char *type = v["context"]["type"] | "";
    if (strlen(ctx) >= sizeof(BrowseItem::uri) && strcmp(type, "collection") != 0) ctx = "";  // non ci starebbe
    const char *key = ctx[0] ? ctx : (t["uri"] | "");
    bool dup = false;
    for (int j = 0; j < res.count && !dup; j++) dup = strcmp(keys[j], key) == 0;
    if (dup || !key[0]) continue;
    strlcpy(keys[res.count], key, 72);

    BrowseItem &it = res.items[res.count++];
    fill_track(it, t);
    strlcpy(it.track_uri, it.uri, sizeof(it.track_uri));
    it.played_at = parse_iso_utc(v["played_at"] | "");
    char artist[64];
    strlcpy(artist, it.artist_name, sizeof(artist));
    // Regola di riserva: titolo del brano e "Brano · artista"
    snprintf(it.subtitle, sizeof(it.subtitle), "Brano \xC2\xB7 %s", artist);
    if (strcmp(type, "album") == 0 && ctx[0]) {
      it.type = IT_ALBUM;
      it.flags &= ~IF_TITLE_FALLBACK;
      clean_text(t["album"]["name"] | "", it.title, sizeof(it.title), "Titolo non latino", &it.flags);
      char album_artist[64];
      clean_text(t["album"]["artists"][0]["name"] | "", album_artist, sizeof(album_artist), "Nome non latino");
      snprintf(it.subtitle, sizeof(it.subtitle), "Album \xC2\xB7 %s", album_artist);
      strlcpy(it.uri, ctx, sizeof(it.uri));
    } else if (strcmp(type, "collection") == 0 || strstr(ctx, ":collection")) {
      it.type = IT_PLAYLIST;
      it.flags = IF_LIKED;
      strlcpy(it.title, "Brani piaciuti", sizeof(it.title));
      strlcpy(it.subtitle, "Playlist", sizeof(it.subtitle));
      it.uri[0] = '\0';
      it.img[0] = it.img_big[0] = '\0';
    } else if (strcmp(type, "playlist") == 0 && ctx[0]) {
      it.type = IT_PLAYLIST;  // il nome lo mette l'interfaccia, se la playlist è fra quelle dell'utente
      it.flags |= IF_GUESS;
      strlcpy(it.uri, ctx, sizeof(it.uri));
    } else if (strcmp(type, "artist") == 0 && ctx[0]) {
      it.type = IT_ARTIST;
      it.flags &= ~IF_TITLE_FALLBACK;
      const char *name = t["artists"][0]["name"] | "";
      for (JsonObjectConst a : t["artists"].as<JsonArrayConst>())
        if (strcmp(a["uri"] | "", ctx) == 0) name = a["name"] | "";
      clean_text(name, it.title, sizeof(it.title), "Nome non latino", &it.flags);
      strlcpy(it.subtitle, "Artista", sizeof(it.subtitle));
      strlcpy(it.artist_uri, ctx, sizeof(it.artist_uri));
      strlcpy(it.uri, ctx, sizeof(it.uri));
    } else {
      it.type = IT_TRACK;  // radio, playlist di altri non visibili, nessun contesto
      strlcpy(it.uri, ctx[0] ? ctx : it.track_uri, sizeof(it.uri));
    }
  }
  res.total = res.count;
  heap_caps_free(keys);
}

// ---------- Filtri JSON ----------
// ArduinoJson confronta ogni testo con tutti quelli già letti: con risposte lunghe la decodifica
// diventa lentissima (album salvati, 166 KB: 14,6 s). Il filtro scarta già in lettura i campi che
// non si mostrano, così si tengono in memoria poche centinaia di testi.

static void f_images(JsonObject o)
{
  o["images"][0]["url"] = true;
  o["images"][0]["width"] = true;
}

static void f_artists(JsonObject o)
{
  o["artists"][0]["name"] = true;
  o["artists"][0]["uri"] = true;
}

static void f_track(JsonObject o)
{
  o["name"] = true;
  o["uri"] = true;
  o["duration_ms"] = true;
  o["track_number"] = true;
  f_artists(o);
  JsonObject a = o["album"].to<JsonObject>();
  a["uri"] = true;
  a["name"] = true;
  f_images(a);
  o["show"]["name"] = true;  // podcast
  f_images(o);
}

static void f_album(JsonObject o)
{
  o["name"] = true;
  o["uri"] = true;
  o["release_date"] = true;
  o["total_tracks"] = true;
  f_artists(o);
  f_images(o);
}

static void f_artist(JsonObject o)
{
  o["name"] = true;
  o["uri"] = true;
  o["genres"] = true;
  f_images(o);
}

static void f_playlist(JsonObject o)
{
  o["name"] = true;
  o["uri"] = true;
  o["collaborative"] = true;
  o["owner"]["id"] = true;
  o["owner"]["display_name"] = true;
  o["items"]["total"] = true;
  o["tracks"]["total"] = true;
  f_images(o);
}

static void f_device(JsonObject o)
{
  for (const char *k : { "id", "name", "type", "is_active", "is_restricted", "volume_percent", "supports_volume" }) o[k] = true;
}

static void build_filter(BrowseKind kind, JsonDocument &f)
{
  f["error"]["message"] = true;
  f["error"]["reason"] = true;
  JsonObject root = f.as<JsonObject>();
  switch (kind) {
    case BR_PLAYLISTS:
      f["total"] = true;
      f_playlist(f["items"][0].to<JsonObject>());
      break;
    case BR_PLAYLIST_ITEMS:
      f["total"] = true;
      f_track(f["items"][0]["item"].to<JsonObject>());
      f_track(f["items"][0]["track"].to<JsonObject>());
      break;
    case BR_LIKED:
      f["total"] = true;
      f_track(f["items"][0]["track"].to<JsonObject>());
      break;
    case BR_ALBUMS:  // niente "tracks": ogni album salvato porta con sé tutti i suoi brani
      f["total"] = true;
      f_album(f["items"][0]["album"].to<JsonObject>());
      break;
    case BR_ALBUM:
      f_album(root);
      f["tracks"]["total"] = true;
      f_track(f["tracks"]["items"][0].to<JsonObject>());
      break;
    case BR_ALBUM_TRACKS:
      f["total"] = true;
      f_track(f["items"][0].to<JsonObject>());
      break;
    case BR_ARTISTS:
      f["artists"]["total"] = true;
      f["artists"]["cursors"]["after"] = true;
      f_artist(f["artists"]["items"][0].to<JsonObject>());
      break;
    case BR_ARTIST: f_artist(root); break;
    case BR_ARTIST_ALBUMS:
      f["total"] = true;
      f_album(f["items"][0].to<JsonObject>());
      break;
    case BR_QUEUE:
      f_track(f["currently_playing"].to<JsonObject>());
      f_track(f["queue"][0].to<JsonObject>());
      break;
    case BR_DEVICES: f_device(f["devices"][0].to<JsonObject>()); break;
    case BR_SEARCH:
      for (const char *k : { "tracks", "albums", "artists", "playlists" }) f[k]["total"] = true;
      f_track(f["tracks"]["items"][0].to<JsonObject>());
      f_album(f["albums"]["items"][0].to<JsonObject>());
      f_artist(f["artists"]["items"][0].to<JsonObject>());
      f_playlist(f["playlists"]["items"][0].to<JsonObject>());
      break;
    case BR_RECENT: {
      JsonObject item = f["items"][0].to<JsonObject>();
      item["played_at"] = true;
      item["context"]["type"] = true;
      item["context"]["uri"] = true;
      JsonObject t = item["track"].to<JsonObject>();
      f_track(t);
      t["album"]["artists"][0]["name"] = true;
      break;
    }
    case BR_TOP_ARTISTS:
      f["total"] = true;
      f_artist(f["items"][0].to<JsonObject>());
      break;
    case BR_TOP_TRACKS:
      f["total"] = true;
      f_track(f["items"][0].to<JsonObject>());
      break;
    default: break;  // comandi: solo l'errore
  }
}

// ---------- Richieste ----------

uint32_t spotify_browse(const BrowseRequest &req)
{
  Job j;
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  j.id = next_job_id++;
  if (next_job_id == 0) next_job_id = 1;
  xSemaphoreGive(sp_mutex);
  j.req = req;
  if (xQueueSend(next_background ? bg_queue : job_queue, &j, 0) != pdTRUE) {
    if (req.body) heap_caps_free(req.body);
    return 0;
  }
  work_signal();
  return j.id;
}

void spotify_browse_background(bool on) { next_background = on; }

bool spotify_take_result(uint32_t id, BrowseResult *out)
{
  bool found = false;
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  for (auto &r : Span<ResultSlot>{ results, RESULT_SLOTS }) {
    if (r.used && r.res.id == id) {
      *out = r.res;
      r.used = false;
      found = true;
      break;
    }
  }
  xSemaphoreGive(sp_mutex);
  return found;
}

static void store_result(const BrowseResult &res)
{
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  ResultSlot *slot = nullptr;
  uint32_t now = millis();
  for (auto &r : Span<ResultSlot>{ results, RESULT_SLOTS }) {  // risultati mai ritirati: la schermata che li aspettava è stata chiusa
    if (r.used && now - r.stored_ms > RESULT_MAX_AGE_MS) {
      heap_caps_free(r.res.items);
      heap_caps_free(r.res.meta);
      r.used = false;
    }
  }
  for (auto &r : Span<ResultSlot>{ results, RESULT_SLOTS }) {
    if (!r.used) { slot = &r; break; }
  }
  if (!slot) {  // tutti occupati: butto il più vecchio
    slot = &results[0];
    for (auto &r : Span<ResultSlot>{ results, RESULT_SLOTS })
      if (r.stored_ms < slot->stored_ms) slot = &r;
    heap_caps_free(slot->res.items);
    heap_caps_free(slot->res.meta);
  }
  slot->used = true;
  slot->stored_ms = now;
  slot->res = res;
  xSemaphoreGive(sp_mutex);
}

static void run_job(const Job &j, BrowseResult &res)
{
  const BrowseRequest &q = j.req;
  JsonDocument doc(PsramAllocator::instance());
  String path;
  const char *method = "GET";
  int limit = q.limit > 0 ? q.limit : 50;
  SpotifyStatus st;
  spotify_get_status(&st);

  switch (q.kind) {
    case BR_PLAYLISTS: path = "/me/playlists?limit=" + String(limit) + "&offset=" + q.offset; break;
    case BR_PLAYLIST_ITEMS:
      // Solo i campi usati: senza, ogni brano porta con sé l'album intero e i dati di chi l'ha aggiunto
      path = String("/playlists/") + q.arg + "/items?limit=" + limit + "&offset=" + q.offset +
             "&additional_types=track,episode&market=from_token&fields=total,items(item(" PL_FIELDS "),track(" PL_FIELDS "))";
      break;
    case BR_LIKED: path = "/me/tracks?limit=" + String(limit) + "&offset=" + q.offset; break;
    case BR_ALBUMS: path = "/me/albums?limit=" + String(limit) + "&offset=" + q.offset + "&market=from_token"; break;
    case BR_ALBUM: path = String("/albums/") + q.arg + "?market=from_token"; break;
    case BR_ALBUM_TRACKS:
      path = String("/albums/") + q.arg + "/tracks?limit=" + limit + "&offset=" + q.offset + "&market=from_token";
      break;
    case BR_ARTISTS:
      path = "/me/following?type=artist&limit=" + String(limit);
      if (q.arg2[0]) path += String("&after=") + q.arg2;
      break;
    case BR_ARTIST: path = String("/artists/") + q.arg; break;
    case BR_ARTIST_ALBUMS:
      path = String("/artists/") + q.arg + "/albums?include_groups=" + q.arg2 + "&limit=" + min(limit, 10) + "&offset=" + q.offset + "&market=from_token";
      break;
    case BR_QUEUE: path = "/me/player/queue"; break;
    case BR_DEVICES: path = "/me/player/devices"; break;
    case BR_SEARCH: {
      static const char *const NAMES[] = { "track", "album", "artist", "playlist" };
      String types;
      for (int t = 0; t < 4; t++) {
        if (q.search_types & (1 << t)) {
          if (types.length()) types += ',';
          types += NAMES[t];
        }
      }
      path = "/search?q=" + url_encode(q.arg) + "&type=" + types + "&limit=" + min(limit, 10) + "&offset=" + q.offset;  // con market=from_token: 403 "Insufficient client scope"
      break;
    }
    case BR_RECENT: path = "/me/player/recently-played?limit=" + String(limit); break;
    case BR_TOP_ARTISTS:
    case BR_TOP_TRACKS:
      path = String(q.kind == BR_TOP_ARTISTS ? "/me/top/artists" : "/me/top/tracks") + "?time_range=" + q.arg +
             "&limit=" + limit + "&offset=" + q.offset;
      break;
    case ACT_PLAY:
      method = "PUT";
      path = "/me/player/play";
      if (q.arg2[0]) path += String("?device_id=") + q.arg2;
      break;
    case ACT_TRANSFER: method = "PUT"; path = "/me/player"; break;
    case ACT_QUEUE_ADD: method = "POST"; path = "/me/player/queue?uri=" + url_encode(q.arg); break;
  }

  String transfer_body;
  const char *body = q.body;
  if (q.kind == ACT_TRANSFER) {
    transfer_body = String("{\"device_ids\":[\"") + q.arg + "\"],\"play\":true}";
    body = transfer_body.c_str();
  }

  JsonDocument filter(PsramAllocator::instance());
  build_filter(q.kind, filter);
  uint32_t t0 = millis();
  int code = sp_api(method, path, doc, &filter, body);
  // Development Mode 2026: alcuni elenchi potrebbero avere un limite più basso di 50
  if (code == 400 && limit > 10 && strcmp(method, "GET") == 0 && path.indexOf("limit=") >= 0) {
    Serial.printf("Spotify: %s rifiutato con limit=%d, riprovo con 10\n", path.c_str(), limit);
    path.replace("limit=" + String(limit), "limit=10");
    limit = 10;
    code = sp_api(method, path, doc, &filter, body);
  }
  uint32_t t_fill = millis();
  res.code = code;
  res.total = -1;
  strlcpy(res.reason, doc["error"]["reason"] | "", sizeof(res.reason));
  Serial.printf("Spotify: %s %s -> HTTP %d in %lu ms %s\n", method, path.c_str(), code, (unsigned long)(millis() - t0),
                (const char *)(doc["error"]["message"] | ""));
  if (q.kind == ACT_PLAY && body) Serial.printf("Spotify: corpo %.300s\n", body);
  if (code < 200 || code >= 300) return;

  JsonArrayConst arr;
  switch (q.kind) {
    case BR_PLAYLISTS:
    case BR_PLAYLIST_ITEMS:
    case BR_LIKED:
    case BR_ALBUMS:
    case BR_ALBUM_TRACKS:
    case BR_ARTIST_ALBUMS:
    case BR_TOP_ARTISTS:
    case BR_TOP_TRACKS: {
      arr = doc["items"];
      res.total = doc["total"] | (int)arr.size();
      // Classifiche: Spotify dichiara anche migliaia di voci, ma ne dà al massimo 50 (SPEC 7 §3.4)
      if (q.kind == BR_TOP_ARTISTS || q.kind == BR_TOP_TRACKS) res.total = min(res.total, 50);
      res.count = arr.size();
      res.items = alloc_items(res.count);
      if (!res.items) { res.count = 0; break; }
      int i = 0;
      for (JsonVariantConst v : arr) {
        BrowseItem &it = res.items[i];
        switch (q.kind) {
          case BR_PLAYLISTS: fill_playlist(it, v, st.user_id); break;
          case BR_PLAYLIST_ITEMS: {
            JsonObjectConst t = v["item"].isNull() ? v["track"] : v["item"];  // 2026: "track" -> "item"
            fill_track(it, t);
            it.number = q.offset + i + 1;  // posizione nella playlist
            break;
          }
          case BR_LIKED: fill_track(it, v["track"]); break;
          case BR_ALBUMS: fill_album(it, v["album"]); break;
          case BR_ALBUM_TRACKS: fill_track(it, v); break;
          case BR_ARTIST_ALBUMS: fill_album(it, v, SUB_YEAR); break;
          case BR_TOP_ARTISTS:
            fill_artist(it, v);
            it.subtitle[0] = '\0';  // la classifica mostra solo il nome (SPEC 7 §3.4)
            it.number = q.offset + i + 1;
            break;
          case BR_TOP_TRACKS:
            fill_track(it, v);
            it.number = q.offset + i + 1;
            break;
          default: break;
        }
        i++;
      }
      break;
    }
    case BR_ALBUM: {
      res.meta = alloc_items(1);
      if (res.meta) fill_album(*res.meta, doc.as<JsonObjectConst>());
      arr = doc["tracks"]["items"];
      res.total = doc["tracks"]["total"] | (int)arr.size();
      res.count = arr.size();
      res.items = alloc_items(res.count);
      if (!res.items) { res.count = 0; break; }
      int i = 0;
      for (JsonVariantConst v : arr) fill_track(res.items[i++], v);
      break;
    }
    case BR_ARTISTS: {
      arr = doc["artists"]["items"];
      res.total = doc["artists"]["total"] | (int)arr.size();
      strlcpy(res.next, doc["artists"]["cursors"]["after"] | "", sizeof(res.next));
      res.count = arr.size();
      res.items = alloc_items(res.count);
      if (!res.items) { res.count = 0; break; }
      int i = 0;
      for (JsonVariantConst v : arr) fill_artist(res.items[i++], v);
      break;
    }
    case BR_ARTIST:
      res.meta = alloc_items(1);
      if (res.meta) fill_artist(*res.meta, doc.as<JsonObjectConst>());
      break;
    case BR_QUEUE: {
      JsonObjectConst cur = doc["currently_playing"];
      arr = doc["queue"];
      res.count = arr.size() + (cur.isNull() ? 0 : 1);
      res.total = res.count;
      res.items = alloc_items(res.count);
      if (!res.items) { res.count = 0; break; }
      int i = 0;
      if (!cur.isNull()) fill_track(res.items[i++], cur);
      for (JsonVariantConst v : arr) fill_track(res.items[i++], v);
      break;
    }
    case BR_DEVICES: {
      arr = doc["devices"];
      res.count = arr.size();
      res.total = res.count;
      res.items = alloc_items(res.count);
      if (!res.items) { res.count = 0; break; }
      int i = 0;
      // Il dispositivo attivo in cima
      for (JsonVariantConst v : arr)
        if (v["is_active"] | false) fill_device(res.items[i++], v);
      for (JsonVariantConst v : arr)
        if (!(v["is_active"] | false)) fill_device(res.items[i++], v);
      break;
    }
    case BR_SEARCH: {
      static const char *const KEYS[] = { "tracks", "albums", "artists", "playlists" };
      int n = 0;
      for (int t = 0; t < 4; t++) {
        res.search_total[t] = doc[KEYS[t]]["total"] | 0;
        for (JsonVariantConst v : doc[KEYS[t]]["items"].as<JsonArrayConst>())
          if (!v.isNull()) n++;  // la ricerca può restituire voci nulle
      }
      res.items = alloc_items(n);
      if (!res.items) break;
      for (int t = 0; t < 4; t++) {
        for (JsonVariantConst v : doc[KEYS[t]]["items"].as<JsonArrayConst>()) {
          if (v.isNull()) continue;
          BrowseItem &it = res.items[res.count++];
          if (t == IT_TRACK) fill_track(it, v, SUB_ARTISTS_ALBUM);
          else if (t == IT_ALBUM) fill_album(it, v);
          else if (t == IT_ARTIST) fill_artist(it, v);
          else fill_playlist(it, v, st.user_id);
        }
      }
      break;
    }
    case BR_RECENT: fill_recent(res, doc["items"], st.user_id); break;
    default: break;
  }
  Serial.printf("Spotify: %d voci convertite in %lu ms\n", res.count, (unsigned long)(millis() - t_fill));
}

int browse_process_one()
{
  Job j;
  if (xQueueReceive(job_queue, &j, 0) != pdTRUE && (images_pending() || xQueueReceive(bg_queue, &j, 0) != pdTRUE))
    return -1;
  BrowseResult res;
  memset(&res, 0, sizeof(res));
  res.id = j.id;
  res.kind = j.req.kind;
  run_job(j, res);
  if (j.req.body) heap_caps_free(j.req.body);
  store_result(res);
  return j.req.kind;
}

// ---------- Miniature ----------

enum SlotState : uint8_t { SLOT_FREE, SLOT_PENDING, SLOT_LOADING, SLOT_READY, SLOT_FAILED };

struct ImgSlot {
  char url[200];
  uint16_t box;
  uint8_t scale;
  uint8_t state;
  uint32_t seq;  // ordine di richiesta: le in attesa si servono in ordine, si butta la meno recente
  uint16_t *px;
  uint16_t w, h;
};

static ImgSlot *small_slots, *mid_slots, *big_slots;  // in PSRAM
static uint32_t img_seq = 0;
static NetworkClient *img_client = nullptr;
static HTTPClient *img_http = nullptr;
static String img_host;

static ImgSlot *pool_for(int box, int *n)
{
  if (box <= 64) {
    *n = SMALL_SLOTS;
    return small_slots;
  }
  if (box <= 80) {
    *n = MID_SLOTS;
    return mid_slots;
  }
  *n = BIG_SLOTS;
  return big_slots;
}

void spotify_image_request(const char *url, uint8_t scale, int box)
{
  if (!url || !url[0]) return;
  int n;
  ImgSlot *pool = pool_for(box, &n);
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  ImgSlot *victim = nullptr;
  bool found = false;
  for (int i = 0; i < n; i++) {
    ImgSlot &s = pool[i];
    if (s.state != SLOT_FREE && s.box == box && strcmp(s.url, url) == 0) {
      if (s.state == SLOT_READY || s.state == SLOT_FAILED) s.seq = ++img_seq;  // usata di recente
      found = true;
      break;
    }
    if (s.state == SLOT_PENDING || s.state == SLOT_LOADING) continue;
    if (!victim || s.state == SLOT_FREE || (victim->state != SLOT_FREE && s.seq < victim->seq)) victim = &s;
  }
  if (!found && victim) {
    strlcpy(victim->url, url, sizeof(victim->url));
    victim->box = box;
    victim->scale = scale;
    victim->state = SLOT_PENDING;
    victim->seq = ++img_seq;
  }
  xSemaphoreGive(sp_mutex);
  if (!found) work_signal();
}

bool spotify_image_get(const char *url, int box, ThumbImage *out, bool *failed)
{
  if (failed) *failed = false;
  if (!url || !url[0]) {
    if (failed) *failed = true;
    return false;
  }
  int n;
  ImgSlot *pool = pool_for(box, &n);
  bool ok = false;
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  for (int i = 0; i < n; i++) {
    ImgSlot &s = pool[i];
    if (s.state != SLOT_FREE && s.box == box && strcmp(s.url, url) == 0) {
      if (s.state == SLOT_READY) {
        out->pixels = s.px;
        out->w = s.w;
        out->h = s.h;
        ok = true;
      } else if (s.state == SLOT_FAILED && failed) {
        *failed = true;
      }
      break;
    }
  }
  xSemaphoreGive(sp_mutex);
  return ok;
}

void spotify_images_cancel()
{
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  for (auto &s : Span<ImgSlot>{ small_slots, SMALL_SLOTS })
    if (s.state == SLOT_PENDING || s.state == SLOT_LOADING) s.state = SLOT_FREE;
  for (auto &s : Span<ImgSlot>{ big_slots, BIG_SLOTS })
    if (s.state == SLOT_PENDING || s.state == SLOT_LOADING) s.state = SLOT_FREE;
  for (auto &s : Span<ImgSlot>{ mid_slots, MID_SLOTS })
    if (s.state == SLOT_PENDING || s.state == SLOT_LOADING) s.state = SLOT_FREE;
  xSemaphoreGive(sp_mutex);
}

bool images_pending()
{
  bool any = false;
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  for (auto &s : Span<ImgSlot>{ small_slots, SMALL_SLOTS }) any |= s.state == SLOT_PENDING;
  for (auto &s : Span<ImgSlot>{ big_slots, BIG_SLOTS }) any |= s.state == SLOT_PENDING;
  for (auto &s : Span<ImgSlot>{ mid_slots, MID_SLOTS }) any |= s.state == SLOT_PENDING;
  xSemaphoreGive(sp_mutex);
  return any;
}

void image_conn_close()
{
  if (img_http) {
    img_http->end();
    delete img_http;
    img_http = nullptr;
  }
  if (img_client) {
    img_client->stop();
    delete img_client;
    img_client = nullptr;
  }
  img_host = "";
}

// Decodifica con ritaglio al centro del riquadro
static uint16_t *thumb_target;
static int thumb_ox, thumb_oy, thumb_w, thumb_h;

static int thumb_draw(JPEGDRAW *d)
{
  for (int y = 0; y < d->iHeight; y++) {
    int ty = d->y + y - thumb_oy;
    if (ty < 0 || ty >= thumb_h) continue;
    int x0 = max(d->x, thumb_ox), x1 = min(d->x + d->iWidthUsed, thumb_ox + thumb_w);
    if (x1 <= x0) continue;
    memcpy(thumb_target + ty * thumb_w + (x0 - thumb_ox), d->pPixels + y * d->iWidth + (x0 - d->x), (x1 - x0) * 2);
  }
  return 1;
}

// Le copertine sono pubbliche (nessuna credenziale): si scaricano in HTTP semplice, così non serve una
// seconda sessione TLS (~40 KB di RAM interna e ~2 s di handshake a immagine) e la connessione con
// api.spotify.com resta aperta. Connessione tenuta aperta finché non cambia l'host.
bool image_download(const char *url, PsramSink &sink)
{
  String u(url);
  if (u.startsWith("https://")) u = "http://" + u.substring(8);
  int hs = u.indexOf("://");
  int he = u.indexOf('/', hs + 3);
  String host = u.substring(hs + 3, he);
  if (img_client && (host != img_host || !img_client->connected())) image_conn_close();
  if (!img_client) {
    img_client = new NetworkClient();
    img_http = new HTTPClient();
    img_http->setReuse(true);
    img_http->setConnectTimeout(8000);
    img_http->setTimeout(8000);
    img_host = host;
  }
  if (!img_http->begin(*img_client, u)) {
    image_conn_close();
    return false;
  }
  int code = img_http->GET();
  int written = code == 200 ? img_http->writeToStream(&sink) : 0;
  bool ok = code == 200 && written > 0 && !sink.overflow && sink.len > 0;
  if (!ok) {
    Serial.printf("Spotify: miniatura %s: HTTP %d (%s), %u byte%s\n", u.c_str(), code,
                  code > 0 ? "" : HTTPClient::errorToString(code).c_str(), (unsigned)sink.len, sink.overflow ? ", troppo grande" : "");
    image_conn_close();  // dopo un errore si riparte da una connessione nuova
    return false;
  }
  img_http->end();
  return true;
}

bool images_process_one()
{
  // La più vecchia fra quelle in attesa, in qualunque misura
  ImgSlot *slot = nullptr;
  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  for (auto &s : Span<ImgSlot>{ small_slots, SMALL_SLOTS })
    if (s.state == SLOT_PENDING && (!slot || s.seq < slot->seq)) slot = &s;
  for (auto &s : Span<ImgSlot>{ big_slots, BIG_SLOTS })
    if (s.state == SLOT_PENDING && (!slot || s.seq < slot->seq)) slot = &s;
  for (auto &s : Span<ImgSlot>{ mid_slots, MID_SLOTS })
    if (s.state == SLOT_PENDING && (!slot || s.seq < slot->seq)) slot = &s;
  char url[200];
  int box = 0, scale = 0;
  if (slot) {
    slot->state = SLOT_LOADING;
    strlcpy(url, slot->url, sizeof(url));
    box = slot->box;
    scale = slot->scale;
  }
  xSemaphoreGive(sp_mutex);
  if (!slot) return false;

  uint32_t t0 = millis();
  bool ok = false;
  int w = 0, h = 0;
  if (!slot->px) slot->px = (uint16_t *)heap_caps_malloc(box * box * 2, MALLOC_CAP_SPIRAM);
  PsramSink sink(THUMB_MAX_JPEG);
  if (slot->px && image_download(url, sink)) {
    // Decoder in RAM interna solo se ce n'è d'avanzo (in PSRAM contende la memoria con lo schermo)
    bool internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) > sizeof(JPEGDEC) + 24 * 1024;
    void *mem = internal ? heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) : nullptr;
    if (!mem) mem = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_SPIRAM);
    if (mem) {
      JPEGDEC *jpeg = new (mem) JPEGDEC();
      if (jpeg->openRAM(sink.data, sink.len, thumb_draw)) {
        int iw = (jpeg->getWidth() + (1 << scale) - 1) >> scale;
        int ih = (jpeg->getHeight() + (1 << scale) - 1) >> scale;
        w = min(iw, box);
        h = min(ih, box);
        thumb_target = slot->px;
        thumb_ox = (iw - w) / 2;
        thumb_oy = (ih - h) / 2;
        thumb_w = w;
        thumb_h = h;
        jpeg->setPixelType(RGB565_LITTLE_ENDIAN);
        ok = jpeg->decode(0, 0, scale ? (1 << scale) : 0);
        if (!ok) Serial.printf("Spotify: decodifica %dx%d 1/%d fallita, errore JPEG %d\n", jpeg->getWidth(), jpeg->getHeight(), 1 << scale, jpeg->getLastError());
        jpeg->close();
      } else {
        Serial.printf("Spotify: JPEG non leggibile (errore %d, %u byte, inizio %02X %02X)\n", jpeg->getLastError(), (unsigned)sink.len, sink.data[0], sink.data[1]);
      }
      jpeg->~JPEGDEC();
      heap_caps_free(mem);
    }
  }

  xSemaphoreTake(sp_mutex, portMAX_DELAY);
  // Se nel frattempo è stata annullata (cambio pagina) la lascio com'è
  if (slot->state == SLOT_LOADING && strcmp(slot->url, url) == 0) {
    slot->state = ok ? SLOT_READY : SLOT_FAILED;
    slot->w = w;
    slot->h = h;
  }
  xSemaphoreGive(sp_mutex);
  static int logged = 0;
  if (logged < 12 || !ok) {
    logged++;
    Serial.printf("Spotify: miniatura %d px %s in %lu ms (%u byte)\n", box, ok ? "ok" : "FALLITA",
                  (unsigned long)(millis() - t0), (unsigned)sink.len);
  }
  return true;
}

void browse_init()
{
  results = (ResultSlot *)heap_caps_calloc(RESULT_SLOTS, sizeof(ResultSlot), MALLOC_CAP_SPIRAM);
  small_slots = (ImgSlot *)heap_caps_calloc(SMALL_SLOTS, sizeof(ImgSlot), MALLOC_CAP_SPIRAM);
  big_slots = (ImgSlot *)heap_caps_calloc(BIG_SLOTS, sizeof(ImgSlot), MALLOC_CAP_SPIRAM);
  mid_slots = (ImgSlot *)heap_caps_calloc(MID_SLOTS, sizeof(ImgSlot), MALLOC_CAP_SPIRAM);
  job_queue = xQueueCreate(6, sizeof(Job));
  bg_queue = xQueueCreate(4, sizeof(Job));
  work_sem = xSemaphoreCreateBinary();
}
