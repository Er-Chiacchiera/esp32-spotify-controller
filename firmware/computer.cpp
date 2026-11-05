#include "computer.h"

#include <Arduino.h>
#include <NetworkUdp.h>
#include "psram_json.h"

static const uint16_t PORT = 3234;

static SemaphoreHandle_t mutex;
static ComputerStatus status;
static NetworkUDP udp;
static bool listening = false;

void computer_init()
{
  mutex = xSemaphoreCreateMutex();
  memset(&status, 0, sizeof(status));
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

// Un pacchetto (vero o finto) diventa lo stato visibile. Chiamata con il mutex preso.
static void publish(ComputerStatus c)
{
  apply_overrides(c);
  c.ever = true;
  c.at_ms = millis();
  compute_alerts(c, status);
  c.version = status.version + 1;
  status = c;
}

// "PC1 {json}": nome, batteria, stato, minuti, processore, memoria, disco, acceso da, caldo, cuffie
void computer_poll()
{
  if (!listening) listening = udp.begin(PORT);
  if (!listening) return;
  int len = udp.parsePacket();
  if (len <= 0) {
    // Con una prova attiva e il Mac assente si finge un pacchetto ogni 5 s
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (any_override() && millis() - real_ms > 6000 && millis() - fake_ms >= 5000) {
      fake_ms = millis();
      if (!raw.ever) {
        raw.ever = true;
        strlcpy(raw.name, "Computer di prova", sizeof(raw.name));
        raw.battery = 80, raw.power = PW_AC, raw.remaining_min = -1, raw.cpu = 20, raw.mem_free = 50;
        raw.disk_free_gb = 100, raw.disk_total_gb = 500, raw.uptime_s = 3600, raw.headphones_battery = -1;
      }
      raw.uptime_s += 5;
      publish(raw);
    }
    xSemaphoreGive(mutex);
    return;
  }
  char buf[512];
  int n = udp.read(buf, sizeof(buf) - 1);
  udp.clear();
  if (n < 5 || strncmp(buf, "PC1 ", 4) != 0) return;
  buf[n] = '\0';
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

  xSemaphoreTake(mutex, portMAX_DELAY);
  raw = c;
  real_ms = millis();
  publish(c);
  xSemaphoreGive(mutex);
}

void computer_serial(const char *args)
{
  while (*args == ' ') args++;
  if (!strncmp(args, "PROVA", 5)) {
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
  Serial.printf("  RAM interna libera %lu KB (minimo %lu KB)\n", (unsigned long)(ESP.getFreeHeap() / 1024),
                (unsigned long)(ESP.getMinFreeHeap() / 1024));
}
