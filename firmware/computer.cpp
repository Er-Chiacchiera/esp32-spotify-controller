#include "computer.h"

#include <Arduino.h>
#include <NetworkUdp.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include "psram_json.h"

static const uint16_t PORT = 3234;
static const uint32_t REPLY_TIMEOUT_MS = 2000;  // comando senza risposta: non riuscito (§5)
static const uint32_t VOLUME_EVERY_MS = 150;    // mentre si trascina il cursore, al massimo 6-7 comandi al secondo
static const uint32_t CODE_VALID_S = 600;       // il codice di abbinamento vale 10 minuti (§6)
static const int CODE_ATTEMPTS = 5;             // dopo 5 tentativi sbagliati la scheda ne fa uno nuovo

static SemaphoreHandle_t mutex;
static ComputerStatus status;
static NetworkUDP udp;
static bool listening = false;

// ---------- Chiave e firme ----------
// Il Mac abbinato ha una chiave di 32 byte, uguale qui (NVS, namespace "mac") e sul Mac. Ogni comando e ogni
// risposta portano in fondo i primi 16 byte (in esadecimale) di HMAC-SHA256(chiave, testo).

static uint8_t key[32];
static bool has_key = false;
static char key_mac_id[12];  // il Mac a cui appartiene la chiave
static char key_kid[9];      // impronta della chiave: primi 4 byte di SHA-256, in esadecimale

static void hmac(const uint8_t *k, size_t klen, const char *msg, size_t len, uint8_t out[32])
{
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), k, klen, (const uint8_t *)msg, len, out);
}

static void to_hex(const uint8_t *in, size_t n, char *out)
{
  for (size_t i = 0; i < n; i++) sprintf(out + i * 2, "%02x", in[i]);
  out[n * 2] = '\0';
}

static bool from_hex(const char *in, uint8_t *out, size_t n)
{
  if (strlen(in) != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    unsigned v;
    if (sscanf(in + i * 2, "%2x", &v) != 1) return false;
    out[i] = v;
  }
  return true;
}

// Firma di 32 caratteri esadecimali
static void sign(const uint8_t *k, size_t klen, const char *msg, size_t len, char out[33])
{
  uint8_t h[32];
  hmac(k, klen, msg, len, h);
  to_hex(h, 16, out);
}

static void key_fingerprint()
{
  uint8_t h[32];
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, sizeof(key), h);
  to_hex(h, 4, key_kid);
}

static void key_load()
{
  Preferences p;
  if (!p.begin("mac", true)) return;
  has_key = p.getBytes("k", key, sizeof(key)) == sizeof(key);
  p.getString("id", key_mac_id, sizeof(key_mac_id));
  p.end();
  if (has_key) key_fingerprint();
}

static void key_save(const uint8_t *k, const char *mac_id)
{
  memcpy(key, k, sizeof(key));
  strlcpy(key_mac_id, mac_id, sizeof(key_mac_id));
  has_key = true;
  key_fingerprint();
  Preferences p;
  p.begin("mac", false);
  p.putBytes("k", key, sizeof(key));
  p.putString("id", key_mac_id);
  p.end();
}

static void key_forget()
{
  has_key = false;
  key_mac_id[0] = key_kid[0] = '\0';
  Preferences p;
  p.begin("mac", false);
  p.clear();
  p.end();
}

void computer_init()
{
  mutex = xSemaphoreCreateMutex();
  memset(&status, 0, sizeof(status));
  status.volume = -1, status.light = -1;
  key_load();
}

void computer_get(ComputerStatus *out)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  *out = status;
  xSemaphoreGive(mutex);
}

bool computer_reachable(const ComputerStatus &c) { return c.ever && millis() - c.at_ms < COMPUTER_TIMEOUT_MS; }

int computer_top_alert(const ComputerStatus &c)
{
  int best = -1;
  for (int i = 0; i < CA_COUNT; i++)
    if (c.alert[i] != AL_NONE && (best < 0 || c.alert[i] > c.alert[best])) best = i;
  return best;
}

// Prove dal comando seriale: valori che sostituiscono quelli dei pacchetti
enum { OV_BATTERY, OV_POWER, OV_MINUTES, OV_CPU, OV_MEMORY, OV_DISK, OV_THERMAL, OV_HEADPHONES, OV_FIXED, OV_COUNT };
static const char *const OV_NAMES[OV_COUNT] = { "batteria", "stato", "minuti", "cpu", "memoria", "disco", "caldo", "cuffie", "fisso" };
static bool ov_set[OV_COUNT];
static float ov_val[OV_COUNT];
static ComputerStatus raw;  // ultimo pacchetto ricevuto, prima delle prove
static uint32_t real_ms, fake_ms;

static bool any_override()
{
  for (bool b : ov_set)
    if (b) return true;
  return false;
}

static void apply_overrides(ComputerStatus &c)
{
  if (ov_set[OV_BATTERY]) c.battery = ov_val[OV_BATTERY];
  if (ov_set[OV_POWER]) c.power = ov_val[OV_POWER];
  if (ov_set[OV_MINUTES]) c.remaining_min = ov_val[OV_MINUTES];
  if (ov_set[OV_CPU]) c.cpu = ov_val[OV_CPU];
  if (ov_set[OV_MEMORY]) c.mem_free = ov_val[OV_MEMORY];
  if (ov_set[OV_DISK]) c.disk_free_gb = ov_val[OV_DISK];
  if (ov_set[OV_THERMAL]) c.thermal = ov_val[OV_THERMAL] != 0;
  if (ov_set[OV_HEADPHONES]) {
    c.headphones_battery = ov_val[OV_HEADPHONES];
    if (c.headphones_battery < 0) c.headphones[0] = '\0';
    else if (!c.headphones[0]) strlcpy(c.headphones, "AirPods Pro", sizeof(c.headphones));
  }
  if (ov_set[OV_FIXED]) c.battery = -1;
  if (c.battery < 0) c.power = PW_NONE;
  else if (c.power == PW_NONE) c.power = PW_AC;
}

// "≤ soglia" (batteria, cuffie): l'avviso esce solo oltre la soglia più il margine
static uint8_t low_level(int v, uint8_t cur, int warn, int crit, int margin)
{
  if (v < 0) return AL_NONE;
  if (v <= crit || (cur == AL_CRITICAL && v <= crit + margin)) return AL_CRITICAL;
  if (v <= warn || (cur >= AL_WARN && v <= warn + margin)) return AL_WARN;
  return AL_NONE;
}

// Da quando vale una condizione (0 se non vale): serve per "almeno 30 s" e "almeno 60 s"
static uint32_t since(bool cond, uint32_t from, uint32_t now) { return cond ? (from ? from : max(now, 1UL)) : 0; }

// Avvisi del §5.2 con margini e tempi; prev è lo stato precedente. Chiamata con il mutex preso.
static void compute_alerts(ComputerStatus &c, const ComputerStatus &prev)
{
  static uint32_t mem_warn_since, mem_crit_since, cpu_high_since, cpu_low_since;
  uint32_t now = c.at_ms;
  uint8_t old[CA_COUNT];
  memcpy(old, prev.alert, sizeof(old));
  if (!prev.ever || now - prev.at_ms >= COMPUTER_TIMEOUT_MS) {  // dopo un buco si riparte da capo
    memset(old, 0, sizeof(old));
    mem_warn_since = mem_crit_since = cpu_high_since = cpu_low_since = 0;
  }

  c.alert[CA_BATTERY] = c.power == PW_BATTERY ? low_level(c.battery, old[CA_BATTERY], 20, 10, 3) : AL_NONE;
  c.alert[CA_THERMAL] = c.thermal ? AL_WARN : AL_NONE;
  uint8_t d = old[CA_DISK];
  float gb = c.disk_free_gb;
  c.alert[CA_DISK] = c.disk_total_gb <= 0                          ? AL_NONE
                     : gb < 10 || (d == AL_CRITICAL && gb < 12)    ? AL_CRITICAL
                     : gb < 20 || (d >= AL_WARN && gb < 22)        ? AL_WARN
                                                                   : AL_NONE;

  // Memoria: sotto la soglia per almeno 30 s; esce sopra la soglia + 5 punti
  uint8_t m = old[CA_MEMORY];
  int mem = c.mem_free;
  bool warn = mem >= 0 && mem < (m >= AL_WARN ? 25 : 20);
  bool crit = mem >= 0 && mem < (m == AL_CRITICAL ? 15 : 10);
  mem_warn_since = since(warn, mem_warn_since, now);
  mem_crit_since = since(crit, mem_crit_since, now);
  c.alert[CA_MEMORY] = crit && (m == AL_CRITICAL || now - mem_crit_since >= 30000)  ? AL_CRITICAL
                       : warn && (m >= AL_WARN || now - mem_warn_since >= 30000)   ? AL_WARN
                                                                                   : AL_NONE;

  // Processore: dal 90% per almeno 60 s; esce sotto l'80% per 30 s
  if (old[CA_CPU] == AL_NONE) {
    cpu_high_since = since(c.cpu >= 90, cpu_high_since, now);
    cpu_low_since = 0;
    c.alert[CA_CPU] = cpu_high_since && now - cpu_high_since >= 60000 ? AL_WARN : AL_NONE;
  } else {
    cpu_low_since = since(c.cpu < 80, cpu_low_since, now);
    c.alert[CA_CPU] = cpu_low_since && now - cpu_low_since >= 30000 ? AL_NONE : AL_WARN;
    if (c.alert[CA_CPU] == AL_NONE) cpu_high_since = 0;
  }
  c.cpu_high_s = c.alert[CA_CPU] != AL_NONE ? (now - cpu_high_since) / 1000 : 0;

  c.alert[CA_HEADPHONES] = low_level(c.headphones_battery, old[CA_HEADPHONES], 20, 10, 3);
}

static char mac_id[12], mac_kid[9];  // dall'ultimo pacchetto: chi è il Mac e quale chiave ha
static IPAddress mac_ip;
static uint16_t mac_port;
static uint32_t asleep_ms;

// Abbinato: la chiave è di questo Mac e il Mac dice di avere la stessa
static bool is_paired() { return has_key && mac_id[0] && !strcmp(key_mac_id, mac_id) && !strcmp(key_kid, mac_kid); }

// Un pacchetto (vero o finto) diventa lo stato visibile; i dati dei comandi e dell'abbinamento sono della
// scheda e restano. Chiamata con il mutex preso.
static void publish(ComputerStatus c)
{
  apply_overrides(c);
  c.ever = true;
  c.at_ms = millis();
  compute_alerts(c, status);
  c.version = status.version + 1;
  c.paired = c.can_command && is_paired();
  // Dopo lo stop il Mac può mandare ancora un pacchetto: "in stop" finisce solo con quelli dopo 8 s
  c.asleep = status.asleep && millis() - asleep_ms < 8000;
  c.cmd_pending = status.cmd_pending;
  c.cmd_failed = status.cmd_failed;
  c.cmd_done = status.cmd_done;
  c.cmd_done_kind = status.cmd_done_kind;
  c.pairing = status.pairing;
  memcpy(c.code, status.code, sizeof(c.code));
  c.code_until = status.code_until;
  c.code_renewed = status.code_renewed;
  c.pair_done = status.pair_done;
  status = c;
}

static void changed() { status.version++; }

// ---------- Comandi (§3) ----------

static const char *const CMD_NAMES[MC_COUNT] = { "vol", "mute", "mic", "awake", "screen", "sleep" };
static int cmd_want[MC_COUNT];       // valore da mandare
static bool cmd_dirty[MC_COUNT];     // da mandare al prossimo giro del task di rete
static uint32_t cmd_seq[MC_COUNT];   // numero dell'ultimo invio, per riconoscere la risposta
static uint32_t cmd_sent_ms[MC_COUNT];
static uint32_t next_seq = 1;

void computer_command(MacCommand cmd, int value)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  cmd_want[cmd] = value;
  cmd_dirty[cmd] = true;
  if (cmd != MC_VOLUME) status.cmd_pending |= 1 << cmd;  // il cursore non ha lo spinner
  changed();
  xSemaphoreGive(mutex);
}

// "MC1 {"s":12,"t":1762430000,"c":"vol","v":63} <firma>" al Mac abbinato. Chiamata con il mutex preso.
static void send_command(int cmd)
{
  char json[96], sig[33], pkt[140];
  uint32_t seq = next_seq++;
  int n = snprintf(json, sizeof(json), "{\"s\":%lu,\"t\":%lu,\"c\":\"%s\",\"v\":%d}", (unsigned long)seq,
                   (unsigned long)time(nullptr), CMD_NAMES[cmd], cmd_want[cmd]);
  sign(key, sizeof(key), json, n, sig);
  int len = snprintf(pkt, sizeof(pkt), "MC1 %s %s", json, sig);
  udp.beginPacket(mac_ip, mac_port);
  udp.write((const uint8_t *)pkt, len);
  udp.endPacket();
  cmd_seq[cmd] = seq;
  cmd_sent_ms[cmd] = millis();
  cmd_dirty[cmd] = false;
}

// Il broadcast dello stato a volte non passa (router della fibra, 6/11/2026): senza stato da 12 s si richiama il
// Mac all'ultimo indirizzo, ogni 10 s, e lui manda lo stato diretto. Chiamata con il mutex preso.
static uint32_t ping_ms;
static bool wifi_ok;  // il task di rete dice se il Wi-Fi c'è

static void ping_tick()
{
  uint32_t now = millis();
  if (!mac_port || now - real_ms < 12000 || now - ping_ms < 10000) return;
  ping_ms = now;
  udp.beginPacket(mac_ip, mac_port);
  udp.write((const uint8_t *)"PB1", 3);
  udp.endPacket();
}

static void commands_tick()
{
  uint32_t now = millis();
  for (int i = 0; i < MC_COUNT; i++) {
    bool waiting = cmd_seq[i] && cmd_sent_ms[i];
    if (cmd_dirty[i]) {
      if (!wifi_ok || !status.paired || !computer_reachable(status)) {  // niente Mac a cui mandarlo
        cmd_dirty[i] = false;
        status.cmd_pending &= ~(1 << i);
        status.cmd_failed++;
        changed();
      } else if (i != MC_VOLUME || !waiting || now - cmd_sent_ms[i] >= VOLUME_EVERY_MS) {
        send_command(i);
      }
    } else if (waiting && now - cmd_sent_ms[i] >= REPLY_TIMEOUT_MS) {
      cmd_sent_ms[i] = 0;
      status.cmd_pending &= ~(1 << i);
      status.cmd_failed++;
      changed();
    }
  }
}

// Valori del Mac nei pacchetti di stato e nelle risposte
static void read_mac_values(ComputerStatus &c, JsonDocument &doc)
{
  c.volume = doc["vo"] | -1;
  c.out_muted = (doc["mu"] | 0) != 0;
  c.mic_muted = (doc["mi"] | 0) != 0;
  c.awake = (doc["aw"] | 0) != 0;
  strlcpy(c.output, doc["on"] | "", sizeof(c.output));
  strlcpy(c.input, doc["inn"] | "", sizeof(c.input));
}

// Separa "<json> <firma>" e controlla la firma con la chiave data
static bool split_signed(char *body, const uint8_t *k, size_t klen, char **json_out)
{
  *json_out = body;
  char *sp = strrchr(body, ' ');
  if (!sp || strlen(sp + 1) != 32) return false;
  *sp = '\0';
  char sig[33];
  sign(k, klen, body, sp - body, sig);
  return !strcmp(sig, sp + 1);
}

// "MR1 {"s":12,"ok":1,"vo":63,...} <firma>": risposta del Mac a un comando
static void handle_reply(char *body)
{
  char *json;
  if (!has_key || !split_signed(body, key, sizeof(key), &json)) return;
  JsonDocument doc(PsramAllocator::instance());
  if (deserializeJson(doc, json)) return;
  uint32_t seq = doc["s"] | 0;
  bool ok = (doc["ok"] | 0) != 0;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < MC_COUNT; i++) {
    if (cmd_seq[i] != seq || !cmd_sent_ms[i]) continue;
    cmd_sent_ms[i] = 0;
    if (!cmd_dirty[i]) status.cmd_pending &= ~(1 << i);
    if (!ok) {
      status.cmd_failed++;
    } else {
      status.cmd_done++;
      status.cmd_done_kind = i;
      if (i == MC_SLEEP) {
        status.asleep = true;
        asleep_ms = millis();
      }
    }
  }
  if (ok) read_mac_values(status, doc);
  changed();
  xSemaphoreGive(mutex);
}

// ---------- Abbinamento (§6) ----------
// Il Mac manda in broadcast "PA1 {"id":..,"n":<nonce>,"nm":<nome>} <firma col codice>". Se il codice è giusto la
// scheda crea la chiave e risponde "PA2 {"n":<nonce>,"k":<chiave cifrata>} <firma con la chiave>": la chiave
// viaggia mescolata con HMAC-SHA256(codice, "chiave" + nonce), che conosce solo chi ha il codice.

static int code_attempts;

static void new_code()
{
  snprintf(status.code, sizeof(status.code), "%06lu", (unsigned long)(esp_random() % 1000000));
  time_t now = time(nullptr);
  status.code_until = now > 1700000000 ? now + CODE_VALID_S : 0;
  status.pairing = true;
  code_attempts = 0;
  changed();
}

static uint32_t code_from_ms;

void computer_pair_open(bool fresh)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (fresh || !status.pairing) {
    new_code();
    code_from_ms = millis();
  }
  xSemaphoreGive(mutex);
}

void computer_pair_close()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  status.pairing = false;
  changed();
  xSemaphoreGive(mutex);
}

static void pair_reply(const char *pkt)
{
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write((const uint8_t *)pkt, strlen(pkt));
  udp.endPacket();
}

static void handle_pair(char *body)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (!status.pairing) {
    xSemaphoreGive(mutex);
    return;
  }
  char code[7];
  memcpy(code, status.code, sizeof(code));
  xSemaphoreGive(mutex);

  char *json;
  bool good = split_signed(body, (const uint8_t *)code, 6, &json);
  JsonDocument doc(PsramAllocator::instance());
  if (deserializeJson(doc, json)) return;
  const char *nonce = doc["n"] | "";
  const char *id = doc["id"] | "";
  if (!nonce[0] || !id[0] || strlen(id) >= sizeof(key_mac_id)) return;
  if (!good) {
    char pkt[96];
    snprintf(pkt, sizeof(pkt), "PA2 {\"n\":\"%.32s\",\"e\":\"codice\"}", nonce);
    pair_reply(pkt);
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (++code_attempts >= CODE_ATTEMPTS) {
      new_code();
      status.code_renewed++;
    }
    xSemaphoreGive(mutex);
    return;
  }

  uint8_t k[32], pad[32];
  esp_fill_random(k, sizeof(k));
  char msg[48];
  int n = snprintf(msg, sizeof(msg), "chiave%s", nonce);
  hmac((const uint8_t *)code, 6, msg, n, pad);
  for (int i = 0; i < 32; i++) pad[i] ^= k[i];
  char khex[65], reply[160], sig[33], pkt[200];
  to_hex(pad, 32, khex);
  int len = snprintf(reply, sizeof(reply), "{\"n\":\"%.32s\",\"k\":\"%s\"}", nonce, khex);
  sign(k, sizeof(k), reply, len, sig);
  snprintf(pkt, sizeof(pkt), "PA2 %s %s", reply, sig);
  pair_reply(pkt);

  xSemaphoreTake(mutex, portMAX_DELAY);
  key_save(k, id);
  status.pairing = false;
  status.pair_done++;
  // Il Mac lo conferma con il prossimo pacchetto di stato (stessa impronta della chiave)
  strlcpy(mac_kid, key_kid, sizeof(mac_kid));
  status.paired = status.can_command && is_paired();
  changed();
  xSemaphoreGive(mutex);
  Serial.printf("COMPUTER abbinato a %s\n", (const char *)(doc["nm"] | id));
}

static void pairing_tick()
{
  if (!status.pairing) return;
  time_t now = time(nullptr);
  bool expired = status.code_until ? now > status.code_until : millis() - code_from_ms > CODE_VALID_S * 1000;
  if (expired) {
    new_code();
    code_from_ms = millis();
    status.code_renewed++;
  }
}

// "PC1 {json}": nome, batteria, stato, minuti, processore, memoria, disco, acceso da, caldo, cuffie; dalla
// versione 2 anche i valori dei comandi. "MR1" risposta a un comando, "PA1" richiesta di abbinamento.
static void handle_packet();

void computer_poll(bool connected)
{
  // Anche senza Wi-Fi: i comandi in attesa devono finire "non riusciti", altrimenti lo spinner gira per sempre
  wifi_ok = connected;
  xSemaphoreTake(mutex, portMAX_DELAY);
  commands_tick();
  pairing_tick();
  xSemaphoreGive(mutex);
  if (!connected) return;
  if (!listening) listening = udp.begin(PORT);
  if (!listening) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  ping_tick();
  xSemaphoreGive(mutex);
  // Fino a 4 pacchetti per giro: le risposte ai comandi non aspettano i 100 ms del task
  for (int i = 0; i < 4 && udp.parsePacket() > 0; i++) handle_packet();
  // Con una prova attiva e il Mac assente si finge un pacchetto ogni 5 s
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (any_override() && millis() - real_ms > 6000 && millis() - fake_ms >= 5000) {
    fake_ms = millis();
    if (!raw.ever) {
      raw.ever = true;
      strlcpy(raw.name, "Computer di prova", sizeof(raw.name));
      raw.battery = 80, raw.power = PW_AC, raw.remaining_min = -1, raw.cpu = 20, raw.mem_free = 50;
      raw.disk_free_gb = 100, raw.disk_total_gb = 500, raw.uptime_s = 3600, raw.headphones_battery = -1;
      raw.volume = -1, raw.light = -1;
    }
    raw.uptime_s += 5;
    publish(raw);
  }
  xSemaphoreGive(mutex);
}

static void handle_packet()
{
  char buf[768];
  int n = udp.read(buf, sizeof(buf) - 1);
  udp.clear();
  if (n < 5) return;
  buf[n] = '\0';
  if (!strncmp(buf, "MR1 ", 4)) return handle_reply(buf + 4);
  if (!strncmp(buf, "PA1 ", 4)) return handle_pair(buf + 4);
  if (strncmp(buf, "PC1 ", 4) != 0) return;
  JsonDocument doc(PsramAllocator::instance());
  if (deserializeJson(doc, buf + 4)) return;

  ComputerStatus c = {};
  c.ever = true;
  strlcpy(c.name, doc["n"] | "", sizeof(c.name));
  c.battery = doc["b"] | -1;
  const char *p = doc["p"] | "none";
  c.power = !strcmp(p, "charging") ? PW_CHARGING : !strcmp(p, "ac") ? PW_AC : !strcmp(p, "battery") ? PW_BATTERY : PW_NONE;
  if (c.battery < 0) c.power = PW_NONE;
  c.remaining_min = doc["r"] | -1;
  c.cpu = doc["c"] | 0;
  c.mem_free = doc["m"] | -1;
  c.disk_free_gb = doc["df"] | 0.0f;
  c.disk_total_gb = doc["dt"] | 0.0f;
  c.uptime_s = doc["u"] | 0;
  c.thermal = (doc["t"] | 0) != 0;
  strlcpy(c.headphones, doc["hn"] | "", sizeof(c.headphones));
  c.headphones_battery = doc["hb"] | -1;
  c.light = doc["lu"] | -1;
  c.can_command = (doc["v"] | 1) >= 2;
  c.volume = -1;
  if (c.can_command) read_mac_values(c, doc);

  xSemaphoreTake(mutex, portMAX_DELAY);
  strlcpy(mac_id, doc["id"] | "", sizeof(mac_id));
  strlcpy(mac_kid, doc["kid"] | "", sizeof(mac_kid));
  mac_ip = udp.remoteIP();
  mac_port = udp.remotePort();
  raw = c;
  real_ms = millis();
  publish(c);
  xSemaphoreGive(mutex);
}

void computer_serial(const char *args)
{
  while (*args == ' ') args++;
  if (!strncmp(args, "ABBINA", 6)) {
    computer_pair_open(false);
  } else if (!strncmp(args, "DIMENTICA", 9)) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    key_forget();
    status.paired = false;
    changed();
    xSemaphoreGive(mutex);
  } else if (!strncmp(args, "COMANDO", 7)) {
    static const char *const NAMES[MC_COUNT] = { "volume", "muto", "microfono", "sveglio", "schermo", "stop" };
    char name[16] = "";
    int value = 0;
    sscanf(args + 7, "%15s %d", name, &value);
    int k = 0;
    while (k < MC_COUNT && strcmp(name, NAMES[k])) k++;
    if (k == MC_COUNT) {
      Serial.println("COMPUTER COMANDO: volume <0-100>, muto <0/1>, microfono <0/1>, sveglio <0/1>, schermo, stop");
      return;
    }
    computer_command((MacCommand)k, value);
  } else if (!strncmp(args, "PROVA", 5)) {
    char key[16] = "", val[16] = "";
    sscanf(args + 5, "%15s %15s", key, val);
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!strcmp(key, "FINE")) {
      memset(ov_set, 0, sizeof(ov_set));
    } else {
      int k = 0;
      while (k < OV_COUNT && strcmp(key, OV_NAMES[k])) k++;
      if (k == OV_COUNT) {
        xSemaphoreGive(mutex);
        Serial.println("COMPUTER PROVA: dati batteria, stato, minuti, cpu, memoria, disco, caldo, cuffie, fisso, FINE");
        return;
      }
      ov_set[k] = true;
      ov_val[k] = k == OV_POWER ? (!strcmp(val, "carica") ? PW_CHARGING : !strcmp(val, "batteria") ? PW_BATTERY : PW_AC)
                  : k == OV_FIXED ? 1 : atof(val);
    }
    if (raw.ever) publish(raw);  // si vede subito, senza aspettare il pacchetto
    xSemaphoreGive(mutex);
  }
  ComputerStatus c;
  computer_get(&c);
  if (!c.ever) {
    Serial.println("COMPUTER nessun dato: avvia strumenti/ponte_computer.py sul computer");
    return;
  }
  static const char *const POWER[] = { "senza batteria", "in carica", "collegato", "a batteria" };
  Serial.printf("COMPUTER %s: %s, ultimo dato %lu s fa\n", c.name, computer_reachable(c) ? "raggiungibile" : "non raggiungibile",
                (unsigned long)((millis() - c.at_ms) / 1000));
  Serial.printf("  batteria %d%% %s, %d min; processore %d%%, memoria libera %d%%, disco %.1f/%.0f GB\n", c.battery,
                POWER[c.power], c.remaining_min, c.cpu, c.mem_free, c.disk_free_gb, c.disk_total_gb);
  Serial.printf("  acceso da %lu h, caldo %d, cuffie \"%s\" %d%%\n", (unsigned long)(c.uptime_s / 3600), c.thermal,
                c.headphones, c.headphones_battery);
  static const char *const ALERTS[] = { "batteria", "caldo", "disco", "memoria", "processore", "cuffie" };
  Serial.print("  avvisi:");
  for (int i = 0; i < CA_COUNT; i++)
    if (c.alert[i]) Serial.printf(" %s %s", ALERTS[i], c.alert[i] == AL_CRITICAL ? "critico" : "attenzione");
  if (computer_top_alert(c) < 0) Serial.print(" nessuno");
  Serial.println(any_override() ? " (valori di prova attivi)" : "");
  if (!c.can_command) Serial.println("  comandi: il programma sul Mac non li sa ricevere (versione 1)");
  else
    Serial.printf("  comandi: %s; volume %d%s, uscita \"%s\", microfono %s (\"%s\"), tieni sveglio %d%s\n",
                  c.paired ? "abbinato" : "non abbinato", c.volume, c.out_muted ? " muto" : "", c.output,
                  c.mic_muted ? "muto" : "acceso", c.input, c.awake, c.asleep ? ", in stop" : "");
  Serial.printf("  in corso 0x%02x, riusciti %u, non riusciti %u\n", c.cmd_pending, c.cmd_done, c.cmd_failed);
  if (c.pairing) Serial.printf("  codice di abbinamento %s\n", c.code);
  Serial.printf("  RAM interna libera %lu KB (minimo %lu KB)\n", (unsigned long)(ESP.getFreeHeap() / 1024),
                (unsigned long)(ESP.getMinFreeHeap() / 1024));
}
