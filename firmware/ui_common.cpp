#include "ui_common.h"

#include <Arduino.h>
#include <stdarg.h>
#include "spotify.h"

lv_obj_t *ui_make_panel(lv_obj_t *scr)
{
  lv_obj_t *p = lv_obj_create(scr);
  lv_obj_remove_style_all(p);
  lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  return p;
}

void ui_set_text_raw(lv_obj_t *label, const char *text)
{
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void ui_set_text(lv_obj_t *label, const char *fmt, ...)
{
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  ui_set_text_raw(label, buf);
}

void ui_set_hidden(lv_obj_t *obj, bool hidden)
{
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_text_color(lv_obj_t *obj, lv_color_t color)
{
  if (lv_color_to32(lv_obj_get_style_text_color(obj, 0)) != lv_color_to32(color)) lv_obj_set_style_text_color(obj, color, 0);
}

void ui_set_bg(lv_obj_t *obj, lv_color_t color)
{
  if (lv_color_to32(lv_obj_get_style_bg_color(obj, 0)) != lv_color_to32(color)) lv_obj_set_style_bg_color(obj, color, 0);
}

lv_obj_t *ui_make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_label_set_text(l, text);
  return l;
}

lv_obj_t *ui_make_line_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int x, int y, int w)
{
  lv_obj_t *l = ui_make_label(parent, font, color, "");
  lv_obj_set_pos(l, x, y);
  lv_obj_set_size(l, w, lv_font_get_line_height(font));
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  return l;
}

lv_obj_t *ui_make_box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg, int radius)
{
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  if (radius) lv_obj_set_style_radius(b, radius, 0);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return b;
}

lv_obj_t *ui_make_touch(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t pressed, bool has_pressed)
{
  lv_obj_t *t = lv_obj_create(parent);
  lv_obj_remove_style_all(t);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_size(t, w, h);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
  if (has_pressed) {
    lv_obj_set_style_bg_color(t, pressed, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, LV_STATE_PRESSED);
  }
  return t;
}

lv_obj_t *ui_make_icon_button(lv_obj_t *parent, int x, int y, const char *icon, lv_color_t color, lv_event_cb_t cb,
                              void *user, lv_obj_t **icon_out, int circle)
{
  lv_obj_t *t = ui_make_touch(parent, x, y, 64, 64, COL_PRESSED, false);
  // Cerchio grigio dietro l'icona solo mentre è premuto
  lv_obj_t *c = ui_make_box(t, (64 - circle) / 2, (64 - circle) / 2, circle, circle, COL_PRESSED, LV_RADIUS_CIRCLE);
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_t *l = ui_make_label(t, &font_ic_28, color, icon);
  lv_obj_center(l);
  // Lo stato "premuto" del contenitore passa al cerchio
  lv_obj_add_event_cb(
    t,
    [](lv_event_t *e) {
      lv_obj_t *circle = (lv_obj_t *)lv_event_get_user_data(e);
      if (lv_event_get_code(e) == LV_EVENT_PRESSED) lv_obj_add_state(circle, LV_STATE_PRESSED);
      else lv_obj_clear_state(circle, LV_STATE_PRESSED);
    },
    LV_EVENT_PRESSED, c);
  lv_obj_add_event_cb(
    t, [](lv_event_t *e) { lv_obj_clear_state((lv_obj_t *)lv_event_get_user_data(e), LV_STATE_PRESSED); },
    LV_EVENT_RELEASED, c);
  lv_obj_add_event_cb(
    t, [](lv_event_t *e) { lv_obj_clear_state((lv_obj_t *)lv_event_get_user_data(e), LV_STATE_PRESSED); },
    LV_EVENT_PRESS_LOST, c);
  if (cb) lv_obj_add_event_cb(t, cb, LV_EVENT_CLICKED, user);
  if (icon_out) *icon_out = l;
  return t;
}

lv_obj_t *ui_make_spinner(lv_obj_t *parent, int size, bool on_accent)
{
  lv_obj_t *s = lv_spinner_create(parent, 1000, 90);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, size, size);
  int w = size >= 28 ? 4 : 3;
  lv_obj_set_style_arc_width(s, w, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s, w, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s, on_accent ? lv_color_hex(0x0E5C2A) : COL_PRESSED, LV_PART_MAIN);
  lv_obj_set_style_arc_color(s, on_accent ? COL_ON_ACCENT : COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(s, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
  return s;
}

lv_obj_t *ui_make_button(lv_obj_t *parent, ButtonKind kind, int x, int y, int w, int h, const char *icon,
                         const char *text, lv_event_cb_t cb, void *user)
{
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, h / 2, 0);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  if (kind == BTN_PRIMARY) {
    lv_obj_set_style_bg_color(b, COL_ACCENT, 0);
    lv_obj_set_style_bg_color(b, COL_ACCENT_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(b, COL_ON_ACCENT, 0);
    lv_obj_set_style_bg_color(b, COL_PRESSED, LV_STATE_DISABLED);
  } else {
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(b, COL_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, COL_TEXT, 0);
    lv_obj_set_style_border_color(b, COL_DISABLED_FG, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(b, COL_TEXT, 0);
  }
  lv_obj_set_style_text_color(b, COL_DISABLED_FG, LV_STATE_DISABLED);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(b, 8, 0);
  if (icon) {
    lv_obj_t *i = lv_label_create(b);
    lv_obj_set_style_text_font(i, &font_ic_20, 0);
    lv_label_set_text(i, icon);
  }
  if (text) {
    lv_obj_t *t = lv_label_create(b);
    lv_obj_set_style_text_font(t, &font_it_sb20, 0);
    lv_label_set_text(t, text);
  }
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

void ui_button_set_enabled(lv_obj_t *btn, bool enabled)
{
  if (lv_obj_has_state(btn, LV_STATE_DISABLED) == !enabled) return;
  if (enabled) lv_obj_clear_state(btn, LV_STATE_DISABLED);
  else lv_obj_add_state(btn, LV_STATE_DISABLED);
}

bool ui_click_is_gesture()
{
  lv_indev_t *in = lv_indev_get_act();
  return in && in->proc.types.pointer.gesture_sent;
}

void ui_format_duration(char *out, size_t n, uint32_t ms)
{
  uint32_t s = ms / 1000;
  if (s >= 3600)
    snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
  else snprintf(out, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

const char *ui_device_icon(int icon)
{
  switch (icon) {
    case DI_COMPUTER: return IC_COMPUTER;
    case DI_PHONE: return IC_PHONE;
    case DI_TABLET: return IC_TABLET;
    case DI_SPEAKER: return IC_VOLUME;
    case DI_TV: return IC_TV;
    case DI_GAMEPAD: return IC_GAMEPAD;
    case DI_CAR: return IC_CAR;
    default: return IC_NOTE;
  }
}
