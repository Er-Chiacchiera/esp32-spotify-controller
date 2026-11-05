#include "net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <NetworkUdp.h>
#include <esp_ota_ops.h>
#include <esp_app_desc.h>
#include <time.h>

// Fuso orario Italia con ora legale automatica
static const char *TZ_ITALIA = "CET-1CEST,M3.5.0,M10.5.0/3";
static const char *NVS_NAMESPACE = "wifi";
static const char *HOSTNAME = "controller-spotify";
static const uint16_t INFO_PORT = 3233;  // risponde a "CHI?": lo script di aggiornamento trova così la scheda

static SemaphoreHandle_t status_mutex;
static NetStatus status;
static volatile bool forget_requested = false;
static volatile bool ota_reload = true;  // (ri)avviare gli aggiornamenti via Wi-Fi: al primo collegamento e con una password nuova

static WebServer server(80);
static DNSServer dns;
static String scan_options;  // <option> della pagina, preparate prima di aprire l'access point
static volatile bool portal_saved = false;

static void set_state(NetState s)
{
  xSemaphoreTake(status_mutex, portMAX_DELAY);
  status.state = s;
  xSemaphoreGive(status_mutex);
}

void net_get_status(NetStatus *out)
{
  xSemaphoreTake(status_mutex, portMAX_DELAY);
  *out = status;
  xSemaphoreGive(status_mutex);
}

void net_request_forget()
{
  forget_requested = true;
}

static void handle_forget()
{
  if (!forget_requested) return;
  Serial.println("Rete dimenticata, riavvio in modalità configurazione");
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  delay(300);
  ESP.restart();
}

// ---------- Portale di configurazione ----------

static String html_escape(const String &s)
{
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static const char PAGE_HEAD[] = R"(<!doctype html><html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Controller Spotify</title><style>
body{font-family:-apple-system,system-ui,sans-serif;background:#121212;color:#fff;margin:0;padding:24px}
h1{font-size:22px;margin:0 0 16px}label{display:block;margin:16px 0 6px;color:#b3b3b3}
select,input{width:100%;box-sizing:border-box;font-size:17px;padding:12px;border-radius:8px;border:1px solid #333;background:#222;color:#fff}
button{margin-top:24px;width:100%;font-size:18px;padding:14px;border:0;border-radius:24px;background:#1db954;color:#000;font-weight:600}
p{color:#b3b3b3;line-height:1.4}</style></head><body>)";

static void send_root()
{
  String page = PAGE_HEAD;
  page += "<h1>Wi-Fi del controller</h1>"
          "<form method='POST' action='/save'>"
          "<label>Rete</label><select name='ssid'>";
  page += scan_options;
  page += "</select>"
          "<label>Oppure scrivi il nome della rete (se non è in elenco)</label>"
          "<input name='ssid_manual' autocapitalize='none' autocorrect='off'>"
          "<label>Password</label>"
          "<input name='pass' type='password' autocapitalize='none' autocorrect='off'>"
          "<button type='submit'>Salva e collega</button></form>"
          "<p>La password resta salvata solo nella scheda.</p></body></html>";
  server.send(200, "text/html; charset=utf-8", page);
}

static void send_save()
{
  String ssid = server.arg("ssid_manual");
  ssid.trim();
  if (ssid.isEmpty()) ssid = server.arg("ssid");
  String pass = server.arg("pass");

  String err;
  if (ssid.isEmpty() || ssid.length() > 32) err = "Nome della rete mancante o troppo lungo.";
  else if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) err = "La password Wi-Fi deve avere da 8 a 63 caratteri.";

  String page = PAGE_HEAD;
  if (!err.isEmpty()) {
    page += "<h1>Errore</h1><p>" + err + "</p><p><a style='color:#1db954' href='/'>Torna indietro</a></p></body></html>";
    server.send(400, "text/html; charset=utf-8", page);
    return;
  }

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.printf("Rete salvata: %s\n", ssid.c_str());

  xSemaphoreTake(status_mutex, portMAX_DELAY);
  strlcpy(status.ssid, ssid.c_str(), sizeof(status.ssid));
  status.state = NET_SAVED;
  xSemaphoreGive(status_mutex);

  page += "<h1>Salvato</h1><p>La scheda si riavvia e si collega a <b>" + html_escape(ssid) +
          "</b>.</p><p>Puoi chiudere questa pagina e tornare al Wi-Fi di casa.</p></body></html>";
  server.send(200, "text/html; charset=utf-8", page);
  portal_saved = true;
}

// Qualsiasi altro indirizzo (anche i controlli "captive" di iPhone e Android) porta alla pagina.
static void send_redirect()
{
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

static void scan_networks()
{
  Serial.println("Cerco le reti Wi-Fi...");
  int n = WiFi.scanNetworks();
  scan_options = "";
  // Ordino per segnale ed elimino i doppioni (stessa rete su più ripetitori)
  int idx[64];
  int count = 0;
  for (int i = 0; i < n && count < 64; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;
    bool dup = false;
    for (int j = 0; j < count; j++) {
      if (WiFi.SSID(idx[j]) == WiFi.SSID(i)) { dup = true; break; }
    }
    if (!dup) idx[count++] = i;
  }
  for (int a = 0; a < count; a++) {
    for (int b = a + 1; b < count; b++) {
      if (WiFi.RSSI(idx[b]) > WiFi.RSSI(idx[a])) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    }
  }
  for (int k = 0; k < count; k++) {
    String name = html_escape(WiFi.SSID(idx[k]));
    scan_options += "<option value=\"" + name + "\">" + name + " (" + String(WiFi.RSSI(idx[k])) + " dBm)</option>";
  }
  if (count == 0) scan_options = "<option value=''>Nessuna rete trovata</option>";
  WiFi.scanDelete();
  Serial.printf("Trovate %d reti\n", count);
}

static void run_portal()
{
  // Password casuale senza caratteri ambigui (0/O, 1/l/I)
  static const char ALPHABET[] = "abcdefghijkmnpqrstuvwxyz23456789";
  char pass[9];
  for (int i = 0; i < 8; i++) pass[i] = ALPHABET[esp_random() % (sizeof(ALPHABET) - 1)];
  pass[8] = '\0';
  WiFi.mode(WIFI_AP_STA);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ap_ssid[33];
  snprintf(ap_ssid, sizeof(ap_ssid), "Controller-Spotify-%02X%02X", mac[4], mac[5]);

  scan_networks();
  WiFi.softAP(ap_ssid, pass);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, send_root);
  server.on("/save", HTTP_POST, send_save);
  server.onNotFound(send_redirect);
  server.begin();

  xSemaphoreTake(status_mutex, portMAX_DELAY);
  strlcpy(status.ap_ssid, ap_ssid, sizeof(status.ap_ssid));
  strlcpy(status.ap_pass, pass, sizeof(status.ap_pass));
  status.state = NET_PORTAL;
  xSemaphoreGive(status_mutex);
  Serial.printf("Portale attivo: rete %s, indirizzo %s\n", ap_ssid, WiFi.softAPIP().toString().c_str());

  uint32_t saved_at = 0;
  while (true) {
    dns.processNextRequest();
    server.handleClient();
    if (portal_saved && saved_at == 0) saved_at = millis();
    // Aspetto un attimo così il telefono riceve la pagina "Salvato"
    if (saved_at && millis() - saved_at > 2000) {
      Serial.println("Riavvio con la nuova rete");
      delay(200);
      ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ---------- Aggiornamenti via Wi-Fi (strumenti/aggiorna_wifi.py) ----------
// ArduinoOTA: il Mac chiede l'aggiornamento sulla porta UDP 3232 con la password, poi la scheda scarica
// il firmware dal Mac e lo scrive nell'altra metà della flash (app0/app1). Senza password resta spento.
// Niente mDNS (RAM): lo script trova la scheda chiedendo "CHI?" in broadcast sulla porta 3233.

static NetworkUDP info_udp;

bool net_set_ota_password(const char *pass)
{
  size_t n = strlen(pass);
  if (n < 8 || n > 64) return false;
  Preferences prefs;
  prefs.begin("ota", false);
  prefs.putString("pass", pass);
  prefs.end();
  ota_reload = true;
  return true;
}

static void ota_set_percent(int pct)
{
  xSemaphoreTake(status_mutex, portMAX_DELAY);
  status.state = NET_UPDATING;
  status.ota_percent = pct;
  xSemaphoreGive(status_mutex);
}

static void ota_setup()
{
  static bool callbacks_set = false;
  if (!callbacks_set) {
    callbacks_set = true;
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setMdnsEnabled(false);
    info_udp.begin(INFO_PORT);
    ArduinoOTA.onStart([]() {
      Serial.println("Aggiornamento via Wi-Fi: inizio");
      ota_set_percent(0);
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
      static int last = -1;
      int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
      if (pct == last) return;
      last = pct;
      ota_set_percent(pct);
      if (pct % 10 == 0) Serial.printf("Aggiornamento via Wi-Fi: %d%%\n", pct);
    });
    ArduinoOTA.onEnd([]() { Serial.println("Aggiornamento via Wi-Fi: completato, riavvio"); });
    // Dopo un errore il ciclo di run_station rimette lo stato a "collegato" e Spotify riparte
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("Aggiornamento via Wi-Fi: errore %d\n", (int)e); });
  }

  Preferences prefs;
  prefs.begin("ota", true);
  String pass = prefs.getString("pass", "");
  prefs.end();
  ArduinoOTA.end();
  if (pass.isEmpty()) {
    Serial.println("Aggiornamenti via Wi-Fi spenti: manca la password (strumenti/aggiorna_wifi.py --prepara)");
    return;
  }
  ArduinoOTA.setPassword(pass.c_str());
  ArduinoOTA.begin();
  Serial.printf("Aggiornamenti via Wi-Fi attivi (%s)\n", WiFi.localIP().toString().c_str());
}

static bool firmware_pending()
{
  esp_ota_img_states_t state;
  return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

// Risposta a "CHI?": nome, impronta del firmware (primi 9 caratteri dello SHA dell'ELF, come nel .bin),
// partizione, confermato, secondi accesa.
// Lo script la usa per trovare la scheda e per controllare che dopo il riavvio giri davvero il firmware nuovo.
static void info_handle()
{
  if (!info_udp.parsePacket()) return;
  char req[8] = {0};
  info_udp.read(req, sizeof(req) - 1);
  if (strncmp(req, "CHI?", 4) == 0) {
    char sha[17];
    esp_app_get_elf_sha256(sha, sizeof(sha));
    char reply[96];
    snprintf(reply, sizeof(reply), "%s %s %s %d %lu", HOSTNAME, sha, esp_ota_get_running_partition()->label,
             firmware_pending() ? 0 : 1, (unsigned long)(millis() / 1000));
    info_udp.beginPacket(info_udp.remoteIP(), info_udp.remotePort());
    info_udp.write((const uint8_t *)reply, strlen(reply));
    info_udp.endPacket();
  }
  info_udp.clear();
}

// Ritorno automatico alla versione precedente: dopo un aggiornamento via Wi-Fi il bootloader avvia il firmware
// nuovo "in prova". Se si riavvia prima della conferma (crash, blocco), torna da solo a quello vecchio.
// Confermo quando il nuovo ha retto 30 secondi collegato al Wi-Fi, cioè può ricevere un altro aggiornamento.
// (verifyRollbackLater() in firmware.ino impedisce al core Arduino di confermarlo subito all'avvio)
static void ota_confirm_if_pending()
{
  if (!firmware_pending()) return;
  esp_ota_mark_app_valid_cancel_rollback();
  Serial.printf("Firmware nuovo confermato (%s)\n", esp_ota_get_running_partition()->label);
}

// ---------- Collegamento alla rete salvata ----------

static void run_station(const String &ssid, const String &pass)
{
  xSemaphoreTake(status_mutex, portMAX_DELAY);
  strlcpy(status.ssid, ssid.c_str(), sizeof(status.ssid));
  status.state = NET_CONNECTING;
  status.connecting_since_ms = millis();
  xSemaphoreGive(status_mutex);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // niente risparmio energetico: meno ritardi e connessione più stabile (la scheda è alimentata dal cavo)
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("Mi collego a %s...\n", ssid.c_str());

  bool ntp_started = false;
  bool was_connected = false;
  bool ota_confirmed = false;
  while (true) {
    handle_forget();
    bool connected = WiFi.status() == WL_CONNECTED;

    if (connected && !was_connected) {
      Serial.printf("Collegato. IP %s, segnale %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      IPAddress resolved;
      uint32_t t_dns = millis();
      bool dns_ok = WiFi.hostByName("api.spotify.com", resolved);
      Serial.printf("DNS %s: api.spotify.com -> %s in %lu ms (server %s)\n", dns_ok ? "ok" : "FALLITO",
                    resolved.toString().c_str(), (unsigned long)(millis() - t_dns), WiFi.dnsIP().toString().c_str());
      if (!ntp_started) {
        configTzTime(TZ_ITALIA, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        ntp_started = true;
      }
    } else if (!connected && was_connected) {
      Serial.println("Wi-Fi perso, riprovo...");
      xSemaphoreTake(status_mutex, portMAX_DELAY);
      status.connecting_since_ms = millis();
      xSemaphoreGive(status_mutex);
    }
    was_connected = connected;

    bool time_ok = time(nullptr) > 1700000000;  // dopo il 2023: l'NTP ha risposto
    xSemaphoreTake(status_mutex, portMAX_DELAY);
    if (time_ok && !status.time_ok) {
      struct tm t;
      time_t now = time(nullptr);
      localtime_r(&now, &t);
      Serial.printf("Ora sincronizzata: %02d/%02d/%04d %02d:%02d:%02d\n", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
                    t.tm_hour, t.tm_min, t.tm_sec);
    }
    status.time_ok = time_ok;
    status.state = connected ? NET_CONNECTED : NET_CONNECTING;
    if (connected) {
      strlcpy(status.ip, WiFi.localIP().toString().c_str(), sizeof(status.ip));
      status.rssi = WiFi.RSSI();
    }
    xSemaphoreGive(status_mutex);

    if (connected && ota_reload) {
      ota_reload = false;
      ota_setup();
    }
    if (connected && !ota_confirmed && millis() > 30000) {
      ota_confirmed = true;
      ota_confirm_if_pending();
    }
    // La richiesta di aggiornamento va raccolta in fretta: 5 controlli da 100 ms. Durante l'aggiornamento
    // handle() non ritorna fino alla fine (o al riavvio).
    for (int i = 0; i < 5; i++) {
      if (connected) {
        ArduinoOTA.handle();
        info_handle();
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

static void net_task(void *arg)
{
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.isEmpty()) {
    Serial.println("Nessuna rete salvata: avvio il portale di configurazione");
    run_portal();
  } else {
    run_station(ssid, pass);
  }
}

void net_start()
{
  status_mutex = xSemaphoreCreateMutex();
  memset(&status, 0, sizeof(status));
  status.state = NET_STARTING;
  // Rete sul core 0, LVGL resta sul core 1
  xTaskCreatePinnedToCore(net_task, "net", 8192, NULL, 3, NULL, 0);
}
