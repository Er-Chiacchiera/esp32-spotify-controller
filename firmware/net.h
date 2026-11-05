// Modulo rete: Wi-Fi, portale di configurazione e ora NTP.
// Gira in un task sul core 0; l'interfaccia legge solo lo stato con net_get_status().
#pragma once

#include <stdint.h>

enum NetState {
  NET_STARTING,    // avvio
  NET_PORTAL,      // nessuna rete salvata: access point + pagina di configurazione
  NET_SAVED,       // credenziali ricevute dal portale, riavvio in corso
  NET_CONNECTING,  // provo a collegarmi alla rete salvata
  NET_CONNECTED,   // collegato
  NET_UPDATING,    // aggiornamento del firmware via Wi-Fi in corso (Spotify si ferma)
};

struct NetStatus {
  NetState state;
  char ssid[33];      // rete salvata (o appena ricevuta dal portale)
  char ip[16];
  int rssi;           // dBm, valido solo se collegato
  char ap_ssid[33];   // rete creata dalla scheda in modalità portale
  char ap_pass[16];
  bool time_ok;       // ora sincronizzata via NTP
  uint32_t connecting_since_ms;
  int ota_percent;    // avanzamento dell'aggiornamento, valido in NET_UPDATING
};

void net_start();
void net_get_status(NetStatus *out);
// Cancella la rete salvata e riavvia in modalità portale (chiamabile dall'interfaccia).
void net_request_forget();
// Salva la password degli aggiornamenti via Wi-Fi (strumenti/aggiorna_wifi.py --prepara) e li riattiva.
bool net_set_ota_password(const char *pass);
