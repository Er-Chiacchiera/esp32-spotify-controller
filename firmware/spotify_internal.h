// Pezzi condivisi fra spotify.cpp (token, stato, comandi) e spotify_browse.cpp (elenchi, miniature).
// Usati solo dal task Spotify sul core 0, tranne sp_mutex e le funzioni chiamate dall'interfaccia.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NetworkClientSecure.h>
#include "spotify.h"

extern SemaphoreHandle_t sp_mutex;  // protegge lo stato condiviso con l'interfaccia

void use_ca_bundle(NetworkClientSecure &client);
// Richiesta alla Web API (connessione tenuta aperta, rinnovo del token su 401). body: JSON o nullptr
int sp_api(const char *method, const String &path, JsonDocument &doc, JsonDocument *filter = nullptr,
           const char *body = nullptr);
void api_close();

// spotify_browse.cpp
void browse_init();
int browse_process_one();     // esegue una richiesta in coda: restituisce il suo BrowseKind, -1 se non c'era niente
bool images_pending();
bool images_process_one();    // scarica e decodifica una miniatura: false se non c'era niente da fare
void image_conn_close();      // chiude la connessione verso il server delle immagini
class PsramSink;
// Scarica un'immagine pubblica (copertine di Spotify) in HTTP semplice, connessione tenuta aperta
bool image_download(const char *url, PsramSink &sink);
void work_signal();           // sveglia il task Spotify (nuovo comando, richiesta o miniatura)
void work_wait(uint32_t ms);

// Raccoglie il corpo della risposta in PSRAM (gestisce anche le risposte "chunked")
class PsramSink : public Stream {
public:
  uint8_t *data = nullptr;
  size_t len = 0, cap = 0;
  bool overflow = false;
  explicit PsramSink(size_t capacity) : cap(capacity) { data = (uint8_t *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM); }
  ~PsramSink() { heap_caps_free(data); }
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *buf, size_t n) override
  {
    if (!data || len + n > cap) {
      overflow = true;
      return 0;
    }
    memcpy(data + len, buf, n);
    len += n;
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

