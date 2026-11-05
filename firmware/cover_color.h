// Colore di fondo di Now playing preso dalla copertina (design/colore_copertina/SPEC_COLORE.md §1).
// Senza dipendenze da Arduino: si prova anche sul Mac (strumenti/prova_colore.py).
#pragma once

#include <stdint.h>

static const uint32_t COVER_BG_NONE = 0x121212;  // BG: brano senza copertina

// Tinta più frequente fra i pixel "vivi" della copertina (RGB565 little endian), scurita a luminanza fissa.
// Restituisce 0xRRGGBB; #303030 se la copertina è in bianco e nero, grigia, quasi nera o quasi bianca.
uint32_t cover_color(const uint16_t *pixels, int w, int h);

// Tono derivato di §2.1: il colore mescolato col bianco, k/256 (k 20 SURFACE ... 174 TEXT_2)
uint32_t cover_mix(uint32_t rgb, int k);
