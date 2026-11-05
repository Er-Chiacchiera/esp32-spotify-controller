#include "ui_keyboard.h"

#include <Arduino.h>
#include "ui_common.h"
#include "ui_nav.h"

// Tastiera (SPEC 4.18): 4 righe; le prime tre cambiano fra lettere e "123"
static const char *const LETTERS[3][10] = {
  { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" },
  { "a", "s", "d", "f", "g", "h", "j", "k", "l", "'" },
  { "z", "x", "c", "v", "b", "n", "m" },
};
static const char *const SYMBOLS[3][10] = {
  { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
  { "-", "/", ":", ";", "(", ")", "&", "@", "\"", "#" },
  { ".", ",", "?", "!", "+", "*", "%" },
};
static const int ROW_KEYS[3] = { 10, 10, 7 };
static const int ROW_X[3] = { 97, 97, 132 };
static const int ROW_Y[4] = { 224, 286, 348, 410 };

enum KeyId { KEY_BACKSPACE = 100, KEY_MODE, KEY_SPACE, KEY_SEARCH };

// Accenti con la pressione lunga sulle vocali
static const char *accents_for(const char *k)
{
  // Opzioni separate da \0, con un \0 in più alla fine
  if (!strcmp(k, "a")) return "\xC3\xA0\0";              // à
  if (!strcmp(k, "e")) return "\xC3\xA8\0\xC3\xA9\0";    // è é
  if (!strcmp(k, "i")) return "\xC3\xAC\0";              // ì
  if (!strcmp(k, "o")) return "\xC3\xB2\0";              // ò
  if (!strcmp(k, "u")) return "\xC3\xB9\0";              // ù
  return nullptr;
}

static int utf8_len(const char *s)
{
  int n = 0;
  for (; *s; s++)
    if ((*s & 0xC0) != 0x80) n++;
  return n;
}

lv_obj_t *Keyboard::make_key(lv_obj_t *root, int x, int y, int w, lv_color_t bg, lv_color_t pressed, int id)
{
  lv_obj_t *k = ui_make_touch(root, CX(x), y, w, 54, pressed);
  lv_obj_set_style_radius(k, 8, 0);
  lv_obj_set_style_bg_color(k, bg, 0);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
  lv_obj_set_user_data(k, (void *)(intptr_t)id);
  lv_obj_add_event_cb(k, key_event, LV_EVENT_ALL, this);
  return k;
}

void Keyboard::build(lv_obj_t *root, int x, int w, const char *placeholder)
{
  text_w = w - 52 - 64;  // prima della ×
  // Campo
  lv_obj_t *field = ui_make_box(root, CX(x), 12, w, 56, COL_SURFACE, 28);
  lv_obj_t *lens = ui_make_label(field, &font_ic_20, COL_TEXT_2, IC_SEARCH);
  lv_obj_set_pos(lens, 20, 18);
  field_hint = ui_make_label(field, &font_it_20, COL_TEXT_3, placeholder);
  lv_obj_set_pos(field_hint, 52, 15);
  field_text = ui_make_label(field, &font_it_20, COL_TEXT, "");
  lv_obj_set_pos(field_text, 52, 15);
  cursor = ui_make_box(field, 52, 15, 2, 26, COL_ACCENT);
  clear_btn = ui_make_touch(root, CX(x + w - 60), 12, 56, 56, COL_BG, false);
  lv_obj_t *c = ui_make_box(clear_btn, 12, 12, 32, 32, COL_RAISED, LV_RADIUS_CIRCLE);
  lv_obj_t *xl = ui_make_label(c, &font_ic_20, COL_TEXT, IC_CLOSE);
  lv_obj_center(xl);
  lv_obj_add_event_cb(clear_btn, [](lv_event_t *e) {
    Keyboard *k = (Keyboard *)lv_event_get_user_data(e);
    k->text[0] = '\0';
    k->update_field();
  }, LV_EVENT_CLICKED, this);

  // Tasti (SPEC 4.18)
  ui_make_box(root, 0, 208, 712, 272, COL_RAIL);
  for (int r = 0; r < 3; r++) {
    for (int k = 0; k < ROW_KEYS[r]; k++) {
      keys[r][k] = make_key(root, ROW_X[r] + k * 70, ROW_Y[r], 64, COL_PRESSED, COL_KEY_PRESSED, r * 10 + k);
      key_labels[r][k] = ui_make_label(keys[r][k], &font_it_24, COL_TEXT, "");
      lv_obj_center(key_labels[r][k]);
    }
  }
  lv_obj_t *bs = make_key(root, 132 + 7 * 70, ROW_Y[2], 169, COL_RAISED, COL_KEY_PRESSED, KEY_BACKSPACE);
  lv_obj_center(ui_make_label(bs, &font_ic_28, COL_TEXT, IC_BACKSPACE));
  lv_obj_t *m = make_key(root, 97, ROW_Y[3], 134, COL_RAISED, COL_KEY_PRESSED, KEY_MODE);
  mode_label = ui_make_label(m, &font_it_20, COL_TEXT, "");
  lv_obj_center(mode_label);
  lv_obj_t *sp = make_key(root, 237, ROW_Y[3], 344, COL_PRESSED, COL_KEY_PRESSED, KEY_SPACE);
  lv_obj_center(ui_make_label(sp, &font_it_16, COL_TEXT_2, "spazio"));
  search_key = make_key(root, 587, ROW_Y[3], 204, COL_ACCENT, COL_ACCENT_PRESSED, KEY_SEARCH);
  lv_obj_set_style_bg_color(search_key, COL_PRESSED, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(search_key, COL_ON_ACCENT, 0);
  lv_obj_set_style_text_color(search_key, COL_DISABLED_FG, LV_STATE_DISABLED);
  lv_obj_t *sl = lv_label_create(search_key);
  lv_obj_set_style_text_font(sl, &font_it_sb20, 0);
  lv_label_set_text(sl, "Cerca");
  lv_obj_center(sl);

  update_key_labels();
  update_field();
  blink = lv_timer_create([](lv_timer_t *t) {
    Keyboard *k = (Keyboard *)t->user_data;
    if (k->cursor) ui_set_hidden(k->cursor, !lv_obj_has_flag(k->cursor, LV_OBJ_FLAG_HIDDEN));
  }, 500, this);
}

void Keyboard::destroyed()
{
  stop_blink();
  field_text = field_hint = cursor = clear_btn = nullptr;
  memset(keys, 0, sizeof(keys));
  memset(key_labels, 0, sizeof(key_labels));
  mode_label = search_key = nullptr;
  popup = nullptr;
  popup_n = 0;
}

bool Keyboard::back()
{
  if (!popup) return false;
  close_popup();
  return true;
}

void Keyboard::trim()
{
  char *s = text;
  while (*s == ' ') s++;
  memmove(text, s, strlen(s) + 1);
  int n = strlen(text);
  while (n > 0 && text[n - 1] == ' ') text[--n] = '\0';
}

void Keyboard::stop_blink()
{
  if (blink) {
    lv_timer_del(blink);
    blink = nullptr;
  }
}

void Keyboard::update_key_labels()
{
  for (int r = 0; r < 3; r++)
    for (int k = 0; k < ROW_KEYS[r]; k++) ui_set_text_raw(key_labels[r][k], symbols ? SYMBOLS[r][k] : LETTERS[r][k]);
  ui_set_text_raw(mode_label, symbols ? "abc" : "123");
}

void Keyboard::update_field()
{
  if (!field_text) return;
  // Testo più lungo del campo: si vede la parte finale, preceduta da "…"
  const char *shown = text;
  char buf[sizeof(text) + 4];
  while (lv_txt_get_width(shown, strlen(shown), &font_it_20, 0, LV_TEXT_FLAG_NONE) > text_w - 20 && *shown) {
    shown++;
    while ((*shown & 0xC0) == 0x80) shown++;
  }
  if (shown != text) {
    snprintf(buf, sizeof(buf), "\xE2\x80\xA6%s", shown);
    shown = buf;
  }
  ui_set_text_raw(field_text, shown);
  ui_set_hidden(field_hint, text[0] != '\0');
  ui_set_hidden(clear_btn, text[0] == '\0');
  int w = lv_txt_get_width(shown, strlen(shown), &font_it_20, 0, LV_TEXT_FLAG_NONE);
  lv_obj_set_x(cursor, 52 + w + 1);
  ui_set_hidden(cursor, false);
  int chars = 0;  // caratteri senza gli spazi
  for (const char *p = text; *p; p++) chars += *p != ' ' && (*p & 0xC0) != 0x80;
  ui_button_set_enabled(search_key, chars >= min_chars);
}

void Keyboard::insert(const char *s)
{
  if (utf8_len(text) >= max_chars) return;
  strlcat(text, s, sizeof(text));
  update_field();
}

void Keyboard::backspace()
{
  int n = strlen(text);
  if (!n) return;
  n--;
  while (n > 0 && (text[n] & 0xC0) == 0x80) n--;
  text[n] = '\0';
  update_field();
}

// ---------- Accenti ----------

void Keyboard::open_popup(lv_obj_t *key, const char *options)
{
  close_popup();
  popup_n = 0;
  const char *opts[2];
  for (const char *o = options; *o && popup_n < 2; o += strlen(o) + 1) opts[popup_n++] = o;
  lv_obj_t *root = lv_obj_get_parent(key);
  // Strato che chiude il riquadro toccando fuori
  popup = lv_obj_create(root);
  lv_obj_remove_style_all(popup);
  lv_obj_set_size(popup, 712, 480);
  lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(popup, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(popup, [](lv_event_t *e) { ((Keyboard *)lv_event_get_user_data(e))->close_popup(); },
                      LV_EVENT_CLICKED, this);
  int w = popup_n * 64 + (popup_n - 1) * 6 + 12;
  int x = constrain((int)lv_obj_get_x(key) - 6, 0, 712 - w);
  lv_obj_t *box = ui_make_box(popup, x, lv_obj_get_y(key) - 66, w, 66, COL_RAISED, 12);
  lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < popup_n; i++) {
    lv_obj_t *k = ui_make_touch(box, 6 + i * 70, 6, 64, 54, COL_KEY_PRESSED);
    lv_obj_set_style_radius(k, 8, 0);
    lv_obj_set_style_bg_color(k, COL_PRESSED, 0);
    lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(k, COL_KEY_PRESSED, LV_STATE_CHECKED);
    lv_obj_t *l = ui_make_label(k, &font_it_24, COL_TEXT, opts[i]);
    lv_obj_center(l);
    lv_obj_set_user_data(k, (void *)opts[i]);
    lv_obj_add_event_cb(k, [](lv_event_t *e) {
      Keyboard *kb = (Keyboard *)lv_event_get_user_data(e);
      const char *s = (const char *)lv_obj_get_user_data(lv_event_get_current_target(e));
      kb->insert(s);
      kb->close_popup();
    }, LV_EVENT_CLICKED, this);
    popup_keys[i] = k;
  }
  popup_hover = -1;
}

void Keyboard::close_popup()
{
  if (!popup) return;
  lv_obj_del_async(popup);
  popup = nullptr;
  popup_n = 0;
}

// Dito trascinato sul riquadro dopo la pressione lunga: evidenzia l'accento sotto il dito
int Keyboard::popup_option_at(lv_point_t p)
{
  for (int i = 0; i < popup_n; i++) {
    lv_area_t a;
    lv_obj_get_coords(popup_keys[i], &a);
    if (p.x >= a.x1 && p.x <= a.x2 && p.y >= a.y1 - 10 && p.y <= a.y2 + 10) return i;
  }
  return -1;
}

void Keyboard::key_event(lv_event_t *e)
{
  Keyboard *v = (Keyboard *)lv_event_get_user_data(e);
  lv_obj_t *k = lv_event_get_current_target(e);
  int id = (intptr_t)lv_obj_get_user_data(k);
  lv_event_code_t code = lv_event_get_code(e);
  const char *ch = id < 30 ? (v->symbols ? SYMBOLS[id / 10][id % 10] : LETTERS[id / 10][id % 10]) : nullptr;

  if (id == KEY_BACKSPACE) {
    // ⌫ tenuto premuto ripete ogni 100 ms dopo 500 ms
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED || code == LV_EVENT_LONG_PRESSED_REPEAT) v->backspace();
    return;
  }
  if (ch && code == LV_EVENT_LONG_PRESSED && !v->symbols) {
    const char *acc = accents_for(ch);
    if (acc) v->open_popup(k, acc);
    return;
  }
  if (v->popup && v->popup_n && (code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED)) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);
    int o = v->popup_option_at(p);
    if (code == LV_EVENT_PRESSING) {
      if (o != v->popup_hover) {
        for (int i = 0; i < v->popup_n; i++) {
          if (i == o) lv_obj_add_state(v->popup_keys[i], LV_STATE_CHECKED);
          else lv_obj_clear_state(v->popup_keys[i], LV_STATE_CHECKED);
        }
        v->popup_hover = o;
      }
    } else if (o >= 0) {
      v->insert((const char *)lv_obj_get_user_data(v->popup_keys[o]));
      v->close_popup();
    }
    return;
  }
  if (code != LV_EVENT_SHORT_CLICKED) return;
  if (ch) v->insert(ch);
  else if (id == KEY_SPACE) {
    if (v->text[0] && v->text[strlen(v->text) - 1] != ' ') v->insert(" ");
  } else if (id == KEY_MODE) {
    v->symbols = !v->symbols;
    v->update_key_labels();
  } else if (id == KEY_SEARCH && !lv_obj_has_state(k, LV_STATE_DISABLED)) {
    if (v->on_search) v->on_search();
  }
}
