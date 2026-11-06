// Stato del computer accanto alla scheda: lo manda strumenti/ponte_computer.py in broadcast UDP sulla
// porta 3234 ogni 5 secondi (design/BRIEF_COMPUTER.md). Si riceve dal task di rete.
#pragma once

#include <stdint.h>
#include <time.h>

enum ComputerPower { PW_NONE, PW_CHARGING, PW_AC, PW_BATTERY };  // senza batteria, in carica, collegato, a batteria

// Avvisi in ordine di importanza (SPEC_COMPUTER §5.2): a pari livello vince il primo
enum ComputerAlert { CA_BATTERY, CA_THERMAL, CA_DISK, CA_MEMORY, CA_CPU, CA_HEADPHONES, CA_COUNT };
enum AlertLevel { AL_NONE, AL_WARN, AL_CRITICAL };

struct ComputerStatus {
  bool ever;              // almeno un pacchetto da questa accensione
  uint32_t at_ms;         // millis() dell'ultimo pacchetto
  char name[48];
  int8_t battery;         // %, -1 senza batteria
  uint8_t power;          // ComputerPower
  int16_t remaining_min;  // a batteria: autonomia; in carica: fino al 100%; -1 se il Mac non lo sa ancora
  int8_t cpu;             // % in uso
  int8_t mem_free;        // % libera, -1 se manca
  float disk_free_gb, disk_total_gb;
  uint32_t uptime_s;
  bool thermal;           // il computer rallenta per il caldo
  char headphones[40];    // cuffie Bluetooth collegate che dicono la batteria, "" se nessuna
  int8_t headphones_battery;
  uint8_t alert[CA_COUNT];  // AlertLevel, con i margini e i tempi del §5.2
  uint32_t cpu_high_s;      // da quanti secondi il processore è al massimo (con l'avviso acceso)
  uint32_t version;         // cresce a ogni pacchetto e a ogni cambio dei comandi

  // Comandi del Mac (design/controllo_mac/SPEC_CONTROLLO_MAC.md), dal programma del Mac versione 2
  bool can_command;         // il programma sul Mac sa ricevere comandi
  bool paired;              // la scheda e questo Mac hanno la stessa chiave
  int8_t volume;            // volume del Mac 0-100, -1 se manca
  bool out_muted, mic_muted, awake;
  char output[40], input[40];  // "MacBook Pro Speakers", "MacBook Pro Microphone"
  bool asleep;              // messo in stop dalla scheda e non ancora risvegliato
  uint8_t cmd_pending;      // un bit per MacCommand: mandato, risposta non ancora arrivata
  uint16_t cmd_failed;      // cresce a ogni comando senza risposta entro 2 s
  uint16_t cmd_done;        // cresce a ogni risposta positiva; cmd_done_kind dice di quale comando
  uint8_t cmd_done_kind;
  // Abbinamento (§6)
  bool pairing;             // c'è un codice valido
  char code[7];
  time_t code_until;        // fine della validità del codice (0 se l'ora non è sincronizzata)
  uint16_t code_renewed;    // cresce quando la scheda cambia il codice da sola (scaduto, 5 tentativi sbagliati)
  uint16_t pair_done;       // cresce a ogni abbinamento riuscito
};

enum MacCommand { MC_VOLUME, MC_MUTE, MC_MIC, MC_AWAKE, MC_SCREEN, MC_SLEEP, MC_COUNT };

static const uint32_t COMPUTER_TIMEOUT_MS = 30000;  // senza pacchetti da 30 s: non raggiungibile

void computer_init();
void computer_poll(bool connected);  // dal task di rete ogni 100 ms, anche senza Wi-Fi (scadenza dei comandi)
void computer_get(ComputerStatus *out);
bool computer_reachable(const ComputerStatus &c);
// L'avviso più importante (critico prima di attenzione, poi l'ordine di ComputerAlert), -1 se nessuno
int computer_top_alert(const ComputerStatus &c);
// Comandi al Mac abbinato (dal task dell'interfaccia). Volume: valore 0-100, mandato al massimo ogni 150 ms
// (vale l'ultimo). Muto, microfono, tieni sveglio: 1 acceso/muto, 0 spento. Schermo e stop: senza valore.
void computer_command(MacCommand cmd, int value = 0);
// Abbinamento: apre un codice nuovo (o tiene quello valido se fresh è false); close lo chiude.
// L'abbinamento vecchio resta valido finché un Mac non scrive il codice nuovo.
void computer_pair_open(bool fresh);
void computer_pair_close();
// Comando seriale "COMPUTER": stampa l'ultimo stato ricevuto.
// "COMPUTER PROVA <dato> <valore>" cambia un dato in tutti i pacchetti che arrivano (anche senza Mac: finge un
// pacchetto ogni 5 s), "COMPUTER PROVA FINE" toglie le prove. Dati: batteria, stato (carica/ac/batteria),
// minuti, cpu, memoria (% libera), disco (GB liberi), caldo (0/1), cuffie (%, -1 nessuna), fisso.
// "COMPUTER ABBINA" apre un codice e lo stampa, "COMPUTER DIMENTICA" cancella l'abbinamento,
// "COMPUTER COMANDO <volume|muto|microfono|sveglio|schermo|stop> [valore]" manda un comando al Mac.
void computer_serial(const char *args);
