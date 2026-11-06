// Controller Spotify - firmware principale.
// Fase 3: Wi-Fi (portale al primo avvio, credenziali in NVS) e ora via NTP.
// Fase 4: login Spotify (refresh token in NVS via seriale, rinnovo automatico, GET /me).
// Aggiornamenti via Wi-Fi: ArduinoOTA in net.cpp, invio con strumenti/aggiorna_wifi.py.
// Rete e Spotify sul core 0 (net.cpp, spotify.cpp), LVGL sul core 1 (ui.cpp + lvgl_v8_port).

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"
#include "net.h"
#include "spotify.h"
#include "ui.h"
#include "ui_player.h"
#include "weather.h"
#include "computer.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

static Board *board;

// Dopo un aggiornamento via Wi-Fi il firmware nuovo lo conferma net.cpp quando è collegato da 30 s:
// se prima si blocca o si riavvia, il bootloader torna alla versione precedente.
extern "C" bool verifyRollbackLater() { return true; }

// Cattura dello schermo per strumenti/screenshot.py: copia il framebuffer, lo comprime a run-length
// (blocchi da 3 byte: ripetizioni 1-255, pixel RGB565 little endian) e lo manda dalla seriale.
// Il checksum serve al PC per scartare catture mescolate a log di altri task.
static void send_screenshot()
{
  LCD *lcd = board->getLCD();
  const int w = lcd->getFrameWidth(), h = lcd->getFrameHeight();
  const size_t n = (size_t)w * h;
  uint16_t *copy = (uint16_t *)heap_caps_malloc(n * 2, MALLOC_CAP_SPIRAM);
  uint8_t *rle = (uint8_t *)heap_caps_malloc(n * 3, MALLOC_CAP_SPIRAM);
  if (!copy || !rle) {
    Serial.println("SCREENSHOT_ERR memoria");
    heap_caps_free(copy);
    heap_caps_free(rle);
    return;
  }
  lvgl_port_lock(-1);  // niente disegni a metà durante la copia
  memcpy(copy, lcd->getFrameBufferByIndex(0), n * 2);
  lvgl_port_unlock();

  size_t len = 0;
  for (size_t i = 0; i < n;) {
    uint16_t px = copy[i];
    uint8_t run = 1;
    while (i + run < n && run < 255 && copy[i + run] == px) run++;
    rle[len++] = run;
    rle[len++] = px & 0xFF;
    rle[len++] = px >> 8;
    i += run;
  }
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++) sum += rle[i];

  Serial.printf("\nSCREENSHOT_BEGIN %d %d %u %lu\n", w, h, (unsigned)len, (unsigned long)sum);
  for (size_t off = 0; off < len; off += 1024) Serial.write(rle + off, min((size_t)1024, len - off));
  Serial.flush();
  Serial.println("\nSCREENSHOT_END");
  heap_caps_free(copy);
  heap_caps_free(rle);
}

// Comandi dal PC, una riga per comando:
//   SPOTIFY_SET <client_id> <refresh_token>   (strumenti/spotify_login.py)
//   SCREENSHOT                                (strumenti/screenshot.py)
//   TAP x y | LONG x y | SWIPE x1 y1 x2 y2    tocco simulato, per provare l'interfaccia dal PC
//   INATTIVO s                                come se l'ultimo tocco fosse s secondi fa (luce dello schermo)
//   COLORE RRGGBB | COLORE AUTO               fondo di Now playing fisso, per le prove (SPEC_COLORE §6)
//   METEO | METEO <lat> <lon> <città> | METEO AUTO   meteo dell'Orologio: stampa, posizione fissa, posizione dall'IP
//   COMPUTER                                  stato del computer ricevuto da strumenti/ponte_computer.py
//   COMPUTER PROVA <dato> <valore> | FINE     valori finti del computer, per le prove (computer.h)
//   OTA_SET <password>                        password degli aggiornamenti via Wi-Fi (strumenti/aggiorna_wifi.py)
//   LUCE                                      luce dello schermo e da dove viene
static int full_brightness();
static const char *light_source = "";
static void handle_serial_line(String &line)
{
  line.trim();
  int a, b, c, d;
  if (line == "SCREENSHOT") {
    send_screenshot();
  } else if (sscanf(line.c_str(), "TAP %d %d", &a, &b) == 2) {
    lvgl_port_simulate_touch(a, b, a, b, 80);
  } else if (sscanf(line.c_str(), "LONG %d %d", &a, &b) == 2) {
    lvgl_port_simulate_touch(a, b, a, b, 800);
  } else if (sscanf(line.c_str(), "SWIPE %d %d %d %d", &a, &b, &c, &d) == 4) {
    lvgl_port_simulate_touch(a, b, c, d, 250);
  } else if (sscanf(line.c_str(), "INATTIVO %d", &a) == 1) {
    lvgl_port_set_idle((uint32_t)a * 1000);
  } else if (line == "COLORE AUTO") {
    player_force_bg(-1);
  } else if (line.startsWith("COLORE ") && line.length() == 13) {
    player_force_bg((int32_t)strtoul(line.c_str() + 7, nullptr, 16));
  } else if (line == "COMPUTER" || line.startsWith("COMPUTER ")) {
    computer_serial(line.c_str() + 8);
  } else if (line == "METEO" || line.startsWith("METEO ")) {
    weather_serial(line.c_str() + 5);
  } else if (line == "LUCE") {
    Serial.printf("Luce piena: %d%% (%s)\n", full_brightness(), light_source);
  } else if (line.startsWith("OTA_SET ")) {
    Serial.println(net_set_ota_password(line.c_str() + 8) ? "OTA_OK" : "OTA_ERR password da 8 a 64 caratteri");
  } else if (line.startsWith("SPOTIFY_SET ")) {
    int sp = line.indexOf(' ', 12);
    String cid = sp > 0 ? line.substring(12, sp) : "";
    String token = sp > 0 ? line.substring(sp + 1) : "";
    if (spotify_set_credentials(cid.c_str(), token.c_str())) {
      Serial.printf("SPOTIFY_OK (token di %d caratteri salvato)\n", token.length());
    } else {
      Serial.println("SPOTIFY_ERR formato non valido");
    }
  }
}

static void poll_serial()
{
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handle_serial_line(line);
      line = "";
    } else if (line.length() < 600) {
      line += c;
    }
  }
}

// Il touch GT911 non ha pin di reset/INT collegati alla scheda: sceglie l'indirizzo I2C (0x5D o 0x14)
// da solo all'accensione, e dopo un caricamento del firmware si risveglia spesso su 0x14.
// La libreria lo cerca solo su 0x5D, quindi all'avvio chiedo al GT911 dove risponde.
static void i2c_bus_clear(int sda, int scl)
{
  // Sblocco standard del bus I2C nel caso SDA sia rimasto basso: 9 impulsi di clock e uno STOP
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(10);
    digitalWrite(scl, HIGH);
    delayMicroseconds(10);
  }
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, LOW);
  delayMicroseconds(10);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  digitalWrite(sda, HIGH);
  delayMicroseconds(10);
  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
}

// Risposta del GT911 a un indirizzo.
// I2C "a mano" sui GPIO: Wire usa il driver I2C nuovo, che non può convivere con quello della libreria display.
static bool i2c_probe(int sda, int scl, uint8_t addr)
{
  auto line = [](int pin, int level) { digitalWrite(pin, level); delayMicroseconds(5); };
  pinMode(sda, OUTPUT_OPEN_DRAIN | PULLUP);
  pinMode(scl, OUTPUT_OPEN_DRAIN | PULLUP);
  line(sda, HIGH);
  line(scl, HIGH);
  line(sda, LOW);  // START
  line(scl, LOW);
  uint8_t byte = addr << 1;  // scrittura
  for (int i = 7; i >= 0; i--) {
    line(sda, (byte >> i) & 1);
    line(scl, HIGH);
    line(scl, LOW);
  }
  line(sda, HIGH);  // rilascio SDA per l'ACK
  line(scl, HIGH);
  bool ack = digitalRead(sda) == LOW;
  line(scl, LOW);
  line(sda, LOW);  // STOP
  line(scl, HIGH);
  line(sda, HIGH);
  return ack;
}

// Restituisce l'indirizzo a cui risponde il GT911, 0 se non risponde
static uint8_t find_touch_address(int sda, int scl)
{
  i2c_bus_clear(sda, scl);
  uint8_t found = 0;
  for (uint8_t addr : { 0x5D, 0x14 }) {
    if (i2c_probe(sda, scl, addr)) {
      found = addr;
      break;
    }
  }
  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
  Serial.printf("Touch GT911: %s0x%02X\n", found ? "indirizzo " : "non risponde ", found);
  return found;
}

void setup()
{
  Serial.setRxBufferSize(1024);  // il comando con il token è più lungo dei 256 byte predefiniti
  Serial.begin(115200);
  delay(500);
  Serial.println("=== Controller Spotify ===");
  Serial.printf("Firmware del %s %s, partizione %s\n", __DATE__, __TIME__, esp_ota_get_running_partition()->label);

  uint8_t touch_addr = find_touch_address(19, 20);  // SDA, SCL del GT911 (BOARD_VIEWE_UEDX80480050E_WB_A.h)

  board = new Board();
  board->init();
  if (touch_addr) static_cast<BusI2C *>(board->getTouch()->getBus())->configI2C_Address(touch_addr);
  // Con il Wi-Fi attivo la PSRAM è contesa e il bounce buffer non si riempie in tempo: sullo schermo
  // compaiono pezzi delle righe 10-20 più in alto (visibili sotto testi e copertine chiare).
  // Pixel clock 20 -> 16 MHz (~36 Hz). NON scendere sotto: a 12 MHz l'ST7262 va in autotest
  // (schermo a tinta unita che cicla rosso/verde/blu). Bounce buffer 10 -> 20 (fase 3) -> 30 righe (fase 5,
  // glitch sotto le copertine chiare): costa 2 x 48 KB di RAM interna.
  auto bus = static_cast<BusRGB *>(board->getLCD()->getBus());
  bus->configRGB_FreqHz(16 * 1000 * 1000);
  bus->configRGB_BounceBufferSize(board->getLCD()->getFrameWidth() * 30);
  if (!board->begin()) {
    // Di solito è il touch che non risponde: riprovo con calma invece di riavviare a raffica
    Serial.println("Avvio display/touch non riuscito, riprovo tra 2 secondi");
    delay(2000);
    ESP.restart();
  }
  lvgl_port_init(board->getLCD(), board->getTouch());

  computer_init();
  net_start();
  spotify_start();

  uint32_t before_ui = ESP.getFreeHeap();
  lvgl_port_lock(-1);
  ui_create();
  lvgl_port_unlock();
  Serial.printf("Interfaccia creata: %lu KB di RAM interna usati\n", (unsigned long)((before_ui - ESP.getFreeHeap()) / 1024));

  Serial.printf("Pronto. PSRAM libera: %.2f MB, RAM interna libera: %lu KB\n",
                ESP.getFreePsram() / 1048576.0, (unsigned long)(ESP.getFreeHeap() / 1024));
}

// Luce dello schermo: piena, poi bassa dopo 2 minuti senza tocchi, spenta dopo altri 10 (anche con la
// musica). Un tocco la riporta piena; a schermo spento quel tocco non preme niente.
static const uint32_t DIM_AFTER_MS = 2 * 60 * 1000, OFF_AFTER_MS = 12 * 60 * 1000;
static const int BRIGHT_FULL = 100, BRIGHT_DIM = 15;

// La luce "piena" segue lo schermo del Mac (che di solito si regola da solo sulla luce della stanza),
// mai sotto il 10%. Senza il dato del Mac da più di un'ora: metà luce dal tramonto all'alba, piena di giorno.
static const uint32_t MAC_LIGHT_KEEP_MS = 60 * 60 * 1000;
static const int BRIGHT_MIN = 10, BRIGHT_NIGHT = 50;

static int night_brightness()
{
  time_t now = time(nullptr);
  if (now < 1700000000) return BRIGHT_FULL;  // ora non ancora arrivata
  Weather *w = (Weather *)heap_caps_malloc(sizeof(Weather), MALLOC_CAP_SPIRAM);
  if (!w) return BRIGHT_FULL;
  weather_get(w);
  int level = BRIGHT_FULL;
  int di = w->valid ? weather_day_index(*w, now) : -1;
  if (di >= 0 && w->days[di].sunrise >= 0 && w->days[di].sunset > w->days[di].sunrise) {
    struct tm t;
    localtime_r(&now, &t);
    int m = t.tm_hour * 60 + t.tm_min;
    if (m < w->days[di].sunrise || m >= w->days[di].sunset) level = BRIGHT_NIGHT;
  }
  heap_caps_free(w);
  return level;
}

static int full_brightness()
{
  static int mac_light = -1, sun_level = BRIGHT_FULL;
  static uint32_t mac_light_ms = 0, checked_ms = 0, sun_ms = 0;
  static bool first = true;
  if (first || millis() - checked_ms >= 1000) {
    ComputerStatus *c = (ComputerStatus *)heap_caps_malloc(sizeof(ComputerStatus), MALLOC_CAP_SPIRAM);
    if (c) {
      computer_get(c);
      if (computer_reachable(*c) && c->light >= 0) mac_light = c->light, mac_light_ms = millis();
      heap_caps_free(c);
    }
    checked_ms = millis();
  }
  if (first || millis() - sun_ms >= 60 * 1000) sun_level = night_brightness(), sun_ms = millis();
  first = false;
  if (mac_light >= 0 && millis() - mac_light_ms < MAC_LIGHT_KEEP_MS) {
    light_source = "dallo schermo del Mac";
    return max(BRIGHT_MIN, (int)mac_light);
  }
  light_source = sun_level == BRIGHT_NIGHT ? "sera, senza Mac" : "giorno o meteo assente, senza Mac";
  return sun_level;
}

static void update_backlight()
{
  static int current = -1;
  Backlight *bl = board->getBacklight();
  if (!bl) return;
  uint32_t idle = millis() - lvgl_port_last_touch_ms();
  int full = full_brightness();
  int level = idle < DIM_AFTER_MS ? full : (idle < OFF_AFTER_MS ? min(BRIGHT_DIM, full) : 0);
  if (level == current) return;
  bl->setBrightness(level);
  lvgl_port_swallow_touch(level == 0);
  Serial.printf("Schermo: luce %d%%\n", level);
  current = level;
}

void loop()
{
  poll_serial();
  update_backlight();
  delay(20);
}
