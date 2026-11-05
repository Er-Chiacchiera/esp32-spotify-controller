// Fase 1: verifica che la scheda risponda sul seriale e che la PSRAM (8 MB) sia attiva.

void setup() {
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  Serial.println();
  Serial.println("=== Controller Spotify - Fase 1 ===");
  Serial.printf("Chip: %s rev %d, %d core, %lu MHz\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Flash: %lu MB\n", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));

  if (psramFound()) {
    Serial.printf("PSRAM: %.2f MB totali, %.2f MB liberi\n",
                  ESP.getPsramSize() / 1048576.0, ESP.getFreePsram() / 1048576.0);
  } else {
    Serial.println("PSRAM: NON TROVATA (controlla l'impostazione PSRAM=opi)");
  }
  Serial.printf("RAM interna libera: %lu KB\n", (unsigned long)(ESP.getFreeHeap() / 1024));

  delay(5000);
}
