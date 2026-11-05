// Allocatore ArduinoJson che mette i documenti JSON in PSRAM (la RAM interna serve a Wi-Fi, TLS e display).
#pragma once

#include <ArduinoJson.h>
#include <esp_heap_caps.h>

struct PsramAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
  void deallocate(void *ptr) override { heap_caps_free(ptr); }
  void *reallocate(void *ptr, size_t new_size) override { return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM); }

  static PsramAllocator *instance()
  {
    static PsramAllocator alloc;
    return &alloc;
  }
};
