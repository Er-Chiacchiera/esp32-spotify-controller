// Campo di testo con la tastiera QWERTY a schermo (SPEC 6 §4.18 e §5.10): accenti con la pressione lunga
// sulle vocali, "123", ⌫ che ripete, cursore che lampeggia. Usato da Cerca e da Cerca una città.
#pragma once

#include <functional>
#include <lvgl.h>

static const int KB_MAX_BYTES = 64 * 2 + 1;  // fino a 64 caratteri, anche accentati

struct Keyboard {
  char text[KB_MAX_BYTES] = "";
  int max_chars = 64;
  int min_chars = 1;              // il tasto Cerca si accende da tanti caratteri (spazi esclusi)
  std::function<void()> on_search;

  ~Keyboard() { stop_blink(); }
  // Campo a x (coordinate dello schermo), y 12, largo w; tasti in fondo all'area contenuto
  void build(lv_obj_t *root, int x, int w, const char *placeholder);
  void destroyed();
  bool back();          // chiude il riquadro degli accenti, se aperto
  void trim();          // toglie gli spazi in testa e in coda

private:
  lv_obj_t *field_text = nullptr, *field_hint = nullptr, *cursor = nullptr, *clear_btn = nullptr;
  lv_obj_t *keys[3][10] = {}, *key_labels[3][10] = {};
  lv_obj_t *mode_label = nullptr, *search_key = nullptr;
  lv_timer_t *blink = nullptr;
  lv_obj_t *popup = nullptr;  // riquadro degli accenti
  lv_obj_t *popup_keys[2] = {};
  int popup_n = 0, popup_hover = -1;
  int text_w = 0;             // spazio per il testo nel campo
  bool symbols = false;

  lv_obj_t *make_key(lv_obj_t *root, int x, int y, int w, lv_color_t bg, lv_color_t pressed, int id);
  void stop_blink();
  void update_key_labels();
  void update_field();
  void insert(const char *s);
  void backspace();
  void open_popup(lv_obj_t *key, const char *options);
  void close_popup();
  int popup_option_at(lv_point_t p);
  static void key_event(lv_event_t *e);
};
