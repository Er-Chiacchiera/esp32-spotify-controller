// Client Spotify: refresh token in NVS, rinnovo automatico del token di accesso, GET /me,
// stato della riproduzione (polling), comandi e copertina.
// Gira in un task sul core 0; l'interfaccia legge lo stato con spotify_get_status() / spotify_get_player()
// e manda i comandi con spotify_command(), che li mette in coda.
#pragma once

#include <stdint.h>
#include <time.h>

enum SpotifyState {
  SP_NO_CREDENTIALS,  // manca il refresh token: lanciare strumenti/spotify_login.py
  SP_WAITING_NETWORK, // aspetto Wi-Fi e ora (serve per verificare i certificati)
  SP_CONNECTING,      // rinnovo il token / chiedo il profilo
  SP_READY,           // token valido e profilo letto
  SP_LOGIN_REQUIRED,  // refresh token revocato o scaduto: rifare il login
  SP_ERROR,           // errore temporaneo, riprovo
};

struct SpotifyStatus {
  SpotifyState state;
  char display_name[64];
  char user_id[64];
  char error[64];
  uint32_t token_expires_in_s;  // secondi al prossimo scadere del token di accesso
  // Spotify ha fermato l'app per troppe richieste (HTTP 429, in Development Mode anche per ore): fino a
  // blocked_until (ora del sistema) nessuna richiesta parte. 0 se non c'è blocco.
  time_t blocked_until;
};

struct PlayerState {
  bool has_playback;        // false: nessun dispositivo attivo (HTTP 204)
  bool is_playing;
  uint32_t progress_ms;     // posizione all'istante progress_at_ms
  uint32_t progress_at_ms;  // millis() a cui si riferisce progress_ms (la barra si calcola in locale)
  uint32_t duration_ms;
  char track_id[24];
  char track_uri[48];       // spotify:track:... (per evidenziare il brano negli elenchi)
  char context_uri[72];     // playlist/album/artista da cui suona (Home: riquadro in riproduzione)
  char cover_small[72];     // URL della copertina da 64 px (barra laterale)
  char title[128];
  char artist[128];         // artisti separati da virgola
  char album[128];
  char device[64];
  char device_type[16];     // "Computer", "Smartphone", ... (icona del chip e della barra)
  int volume;               // 0-100, -1 se il dispositivo non lo permette
  char message[96];         // avviso per l'utente (es. nessun dispositivo), vuoto se tutto ok
  uint32_t version;         // cambia a ogni aggiornamento
  uint32_t no_device_count; // cresce a ogni comando fallito per "nessun dispositivo attivo" (404)
  bool last_cmd_was_play;   // il comando fallito era play: si può far ripartire sul dispositivo scelto
};

// Copertina decodificata in RGB565 nella PSRAM. pixels resta valido finché l'interfaccia
// non conferma di aver mostrato una versione più nuova (spotify_cover_shown).
struct CoverImage {
  const uint16_t *pixels;  // nullptr se il brano non ha copertina
  int width, height;
  uint32_t bg_rgb;         // fondo di Now playing calcolato dalla copertina (cover_color.h), 0x121212 senza
  uint32_t version;
};

enum SpotifyCommand { CMD_PLAY, CMD_PAUSE, CMD_NEXT, CMD_PREVIOUS, CMD_VOLUME, CMD_SEEK };

void spotify_start();
void spotify_get_status(SpotifyStatus *out);
void spotify_get_player(PlayerState *out);
void spotify_get_cover(CoverImage *out);
void spotify_cover_shown(uint32_t version);
// value: volume 0-100 per CMD_VOLUME, posizione in ms per CMD_SEEK
void spotify_command(SpotifyCommand cmd, int value = 0);
// Chiamata dal comando seriale SPOTIFY_SET: salva in NVS e riparte con le nuove credenziali.
bool spotify_set_credentials(const char *client_id, const char *refresh_token);

// ---------- Navigazione (fase 6): elenchi, ricerca, comandi con risposta ----------
// L'interfaccia chiede un elenco con spotify_browse() e ritira il risultato con spotify_take_result():
// le voci arrivano già "pulite" (caratteri che il font non ha tolti) in un blocco di PSRAM che da
// quel momento appartiene all'interfaccia (liberarlo con spotify_free).

enum BrowseKind : uint8_t {
  BR_PLAYLISTS,       // GET /me/playlists
  BR_PLAYLIST_ITEMS,  // GET /playlists/{arg}/items
  BR_LIKED,           // GET /me/tracks
  BR_ALBUMS,          // GET /me/albums
  BR_ALBUM,           // GET /albums/{arg}: meta + primi 50 brani
  BR_ALBUM_TRACKS,    // GET /albums/{arg}/tracks
  BR_ARTISTS,         // GET /me/following?type=artist (cursore in arg2)
  BR_ARTIST,          // GET /artists/{arg}: solo meta
  BR_ARTIST_ALBUMS,   // GET /artists/{arg}/albums?include_groups=arg2
  BR_QUEUE,           // GET /me/player/queue
  BR_DEVICES,         // GET /me/player/devices
  BR_SEARCH,          // GET /search?q=arg (search_types: bit per tipo, vedi ItemType)
  BR_RECENT,          // GET /me/player/recently-played: ascolti raggruppati per contesto (fase 7)
  BR_TOP_ARTISTS,     // GET /me/top/artists?time_range=arg
  BR_TOP_TRACKS,      // GET /me/top/tracks?time_range=arg
  ACT_PLAY,           // PUT /me/player/play[?device_id=arg2], corpo JSON in body
  ACT_TRANSFER,       // PUT /me/player {"device_ids":[arg],"play":true}
  ACT_QUEUE_ADD,      // POST /me/player/queue?uri=arg
};

enum ItemType : uint8_t { IT_TRACK, IT_ALBUM, IT_ARTIST, IT_PLAYLIST, IT_DEVICE };

enum ItemFlag : uint8_t {
  IF_TITLE_FALLBACK = 1,  // il titolo era tutto in lettere non latine: mostrato il testo di riserva
  IF_COLLABORATIVE = 2,
  IF_OWNED = 4,           // playlist dell'utente
  IF_ACTIVE = 8,          // dispositivo attivo
  IF_RESTRICTED = 16,     // dispositivo non controllabile
  IF_NO_COUNT = 32,       // playlist di altri: Spotify non dice quanti brani ha
  IF_LIKED = 64,          // recenti: dai Brani piaciuti (uri vuoto, il context si fa con l'id utente)
  IF_GUESS = 128,         // recenti: playlist di cui non si sa il nome, titolo e immagine sono del brano
};

struct BrowseItem {
  char title[100];
  char subtitle[100];
  char uri[48];          // spotify:track:... ; per i dispositivi il loro id
  char img[200];         // immagine per il riquadro da 64 (i mosaici delle playlist hanno indirizzi lunghi)
  char img_big[200];     // immagine per il riquadro da 150
  char album_uri[40];    // brani: album (menu "Vai all'album")
  char artist_uri[40];   // brani e album: primo artista
  char artist_name[64];
  uint32_t duration_ms;
  uint16_t number;       // numero di traccia
  uint16_t count;        // album e playlist: numero di brani
  uint16_t year;
  uint8_t img_scale, img_big_scale;  // riduzione JPEG da usare: 0 = 1/1, 1 = 1/2, 2 = 1/4, 3 = 1/8
  uint8_t type, flags;
  uint8_t icon;          // dispositivi: DeviceIcon
  // Recenti (BR_RECENT): una voce per contesto. uri = contesto (o il brano se non c'è), track_uri =
  // ultimo brano ascoltato lì (offset per riprendere), played_at = ora dell'ascolto (secondi Unix)
  char track_uri[48];
  uint32_t played_at;
};

enum DeviceIcon : uint8_t { DI_COMPUTER, DI_PHONE, DI_TABLET, DI_SPEAKER, DI_TV, DI_GAMEPAD, DI_CAR, DI_OTHER };
DeviceIcon spotify_device_icon(const char *type);

struct BrowseRequest {
  BrowseKind kind;
  char arg[72];        // id, uri o testo della ricerca
  char arg2[48];       // cursore, gruppi album, device_id
  int offset;
  int limit;
  uint8_t search_types;  // BR_SEARCH: bit (1 << ItemType)
  char *body;          // ACT_PLAY: corpo JSON in PSRAM (lo libera il task Spotify), oppure nullptr
};

struct BrowseResult {
  uint32_t id;
  BrowseKind kind;
  int code;              // codice HTTP, <0 errore di rete
  char reason[24];       // es. NO_ACTIVE_DEVICE
  int total;             // voci totali secondo Spotify (-1 se non noto)
  int search_total[4];   // BR_SEARCH: totale per tipo (IT_TRACK..IT_PLAYLIST)
  int count;
  BrowseItem *items;     // PSRAM, appartiene a chi ritira il risultato
  BrowseItem *meta;      // BR_ALBUM / BR_ARTIST: l'album o l'artista
  char next[40];         // BR_ARTISTS: cursore della pagina dopo
};

// Restituisce l'id della richiesta (0 se la coda è piena)
uint32_t spotify_browse(const BrowseRequest &req);
// Le richieste fatte con on=true (pagine chieste in anticipo) passano dopo le miniature in attesa
void spotify_browse_background(bool on);
// true quando il risultato è pronto: out->items e out->meta vanno liberati con spotify_free
bool spotify_take_result(uint32_t id, BrowseResult *out);
void spotify_free(void *p);
char *spotify_strdup(const char *s);  // copia in PSRAM (corpo di ACT_PLAY)

// ---------- Miniature ----------
// Cache in PSRAM di immagini già decodificate in RGB565 e ritagliate al riquadro (64 o 150 px).
// spotify_image_request() è idempotente: si può richiamare a ogni giro finché l'immagine non è pronta.
struct ThumbImage {
  const uint16_t *pixels;  // w x h pixel RGB565 (al massimo box x box): l'interfaccia la centra nel riquadro
  int w, h;
};
void spotify_image_request(const char *url, uint8_t scale, int box);
// true se pronta (out valido), false se in attesa o fallita; *failed = true se non arriverà
bool spotify_image_get(const char *url, int box, ThumbImage *out, bool *failed = nullptr);
// Annulla le miniature in attesa (cambio pagina o schermata)
void spotify_images_cancel();
