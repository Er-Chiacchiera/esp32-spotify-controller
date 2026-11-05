// Stato del computer accanto alla scheda: lo manda strumenti/ponte_computer.py in broadcast UDP sulla
// porta 3234 ogni 5 secondi (design/BRIEF_COMPUTER.md). Si riceve dal task di rete.
#pragma once

#include <stdint.h>

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
  uint32_t version;         // cresce a ogni pacchetto
};

static const uint32_t COMPUTER_TIMEOUT_MS = 30000;  // senza pacchetti da 30 s: non raggiungibile

void computer_init();
void computer_poll();  // dal task di rete, con il Wi-Fi collegato
void computer_get(ComputerStatus *out);
bool computer_reachable(const ComputerStatus &c);
// L'avviso più importante (critico prima di attenzione, poi l'ordine di ComputerAlert), -1 se nessuno
int computer_top_alert(const ComputerStatus &c);
// Comando seriale "COMPUTER": stampa l'ultimo stato ricevuto.
// "COMPUTER PROVA <dato> <valore>" cambia un dato in tutti i pacchetti che arrivano (anche senza Mac: finge un
// pacchetto ogni 5 s), "COMPUTER PROVA FINE" toglie le prove. Dati: batteria, stato (carica/ac/batteria),
// minuti, cpu, memoria (% libera), disco (GB liberi), caldo (0/1), cuffie (%, -1 nessuna), fisso.
void computer_serial(const char *args);
