// Memoria di LVGL: gli oggetti (piccoli, letti di rado durante il disegno) vanno in PSRAM, così la
// RAM interna resta per Wi-Fi e TLS; i buffer temporanei del disegno (maschere degli angoli, righe
// di testo: di solito più grandi) restano in RAM interna, dove leggerli non contende la memoria con lo schermo.
#pragma once

#include <esp_heap_caps.h>

#define LV_PSRAM_SMALL_MAX 384  // byte: sotto questa misura in PSRAM

static inline uint32_t lv_psram_caps(size_t size)
{
  return size <= LV_PSRAM_SMALL_MAX ? MALLOC_CAP_SPIRAM : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static inline void *lv_psram_malloc(size_t size)
{
  void *p = heap_caps_malloc(size, lv_psram_caps(size));
  return p ? p : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

static inline void *lv_psram_realloc(void *ptr, size_t size)
{
  void *p = heap_caps_realloc(ptr, size, lv_psram_caps(size));
  return p ? p : heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
}

static inline void lv_psram_free(void *ptr) { heap_caps_free(ptr); }
