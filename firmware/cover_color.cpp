#include "cover_color.h"

#include <math.h>

// Parametri della ricetta (SPEC_COLORE.md §1.2): cambiarli solo qui, dopo le prove sul dispositivo
static const int CC_STEP = 6;            // passo dei campioni
static const int CC_MIN_MAX = 40;        // sotto: quasi nero, scartato
static const int CC_MIN_C = 24;          // sotto: grigio, scartato
static const int CC_MIN_SAT = 30;        // % di saturazione HSV minima per essere "vivo"
static const int CC_MIN_VIVID = 5;       // % di campioni vivi sotto cui la copertina è acromatica (grigio)
static const float CC_S_MIN = 0.30f, CC_S_MAX = 0.60f;  // saturazione del fondo
static const int CC_L_START = 30;        // tetto di luminosità HSL, in centesimi
static const float CC_Y = 0.032f;        // luminanza del fondo: non superare 0,040 (SPEC §2.3)

static float lin(int c)
{
  float v = c / 255.0f;
  return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

// Tinta (0-360) e saturazione HSL di un colore a 8 bit per canale (anche medie non intere)
static void rgb_to_hs(float r, float g, float b, float *h, float *s)
{
  r /= 255.0f;
  g /= 255.0f;
  b /= 255.0f;
  float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn, l = (mx + mn) / 2;
  *h = 0;
  *s = 0;
  if (d <= 0) return;
  *s = d / (1 - fabsf(2 * l - 1));
  if (mx == r) *h = 60 * fmodf((g - b) / d, 6);
  else if (mx == g) *h = 60 * ((b - r) / d + 2);
  else *h = 60 * ((r - g) / d + 4);
  if (*h < 0) *h += 360;
}

static uint32_t hsl_to_rgb(float h, float s, float l)
{
  float c = (1 - fabsf(2 * l - 1)) * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = l - c / 2;
  float r, g, b;
  if (h < 60) r = c, g = x, b = 0;
  else if (h < 120) r = x, g = c, b = 0;
  else if (h < 180) r = 0, g = c, b = x;
  else if (h < 240) r = 0, g = x, b = c;
  else if (h < 300) r = x, g = 0, b = c;
  else r = c, g = 0, b = x;
  auto to8 = [](float v) { return (uint32_t)floorf(v * 255 + 0.5f); };
  return to8(r + m) << 16 | to8(g + m) << 8 | to8(b + m);
}

uint32_t cover_color(const uint16_t *pixels, int w, int h)
{
  uint32_t cnt[36] = { 0 }, sr[36] = { 0 }, sg[36] = { 0 }, sb[36] = { 0 };
  int n = 0, vivid = 0;
  for (int y = CC_STEP / 2; y < h; y += CC_STEP) {
    for (int x = CC_STEP / 2; x < w; x += CC_STEP) {
      uint16_t p = pixels[y * w + x];
      // Espansione a 8 bit senza replicare i bit bassi (come i valori di prova della SPEC)
      int r = (p >> 8) & 0xF8, g = (p >> 3) & 0xFC, b = (p << 3) & 0xF8;
      n++;
      int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
      int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
      int c = mx - mn;
      if (mx < CC_MIN_MAX || c < CC_MIN_C || c * 100 < mx * CC_MIN_SAT) continue;
      float hue, sat;
      rgb_to_hs(r, g, b, &hue, &sat);
      int k = (int)(hue / 10);
      if (k > 35) k = 35;
      cnt[k]++;
      sr[k] += r;
      sg[k] += g;
      sb[k] += b;
      vivid++;
    }
  }

  float hue = 0, sat = 0;
  if (n > 0 && vivid * 100 >= n * CC_MIN_VIVID) {
    // Tinta vincente: il contenitore con più campioni, contando anche i due vicini (36 = giro completo)
    int best = 0;
    uint32_t best_score = 0;
    for (int k = 0; k < 36; k++) {
      uint32_t score = cnt[(k + 35) % 36] + 2 * cnt[k] + cnt[(k + 1) % 36];
      if (score > best_score) {
        best_score = score;
        best = k;
      }
    }
    uint32_t R = 0, G = 0, B = 0, N = 0;
    for (int i = -1; i <= 1; i++) {
      int k = (best + 36 + i) % 36;
      R += sr[k];
      G += sg[k];
      B += sb[k];
      N += cnt[k];
    }
    rgb_to_hs((float)R / N, (float)G / N, (float)B / N, &hue, &sat);
    sat = fminf(CC_S_MAX, fmaxf(CC_S_MIN, sat));
  }

  // Scurisco a luminanza fissa: tutte le tinte sembrano scure allo stesso modo
  uint32_t rgb = 0;
  for (int l = CC_L_START; l >= 6; l--) {
    rgb = hsl_to_rgb(hue, sat, l / 100.0f);
    float y = 0.2126f * lin(rgb >> 16) + 0.7152f * lin((rgb >> 8) & 0xFF) + 0.0722f * lin(rgb & 0xFF);
    if (y <= CC_Y) break;
  }
  return rgb;
}

uint32_t cover_mix(uint32_t rgb, int k)
{
  uint32_t out = 0;
  for (int shift = 16; shift >= 0; shift -= 8) {
    int v = (rgb >> shift) & 0xFF;
    out |= (uint32_t)((v * 256 + (255 - v) * k + 128) / 256) << shift;
  }
  return out;
}
