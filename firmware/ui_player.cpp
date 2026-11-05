#include "ui_player.h"

#include <Arduino.h>
#include <time.h>
#include "cover_color.h"
#include "ui_common.h"
#include "ui.h"
#include "ui_nav.h"
#include "ui_views.h"

// Colonna destra accanto alla copertina 300x300 (SPEC 5.1)
static const int COL_X = 364;
static const int COL_W = 404;
static const int COVER_X = 32, COVER_Y = 96, COVER_SIDE = 300;

static lv_obj_t *panel;
static DeviceChip chip;
static lv_obj_t *clock_label, *message_label;
static lv_obj_t *cover_placeholder, *cover_note, *cover_img;
static lv_obj_t *title_label, *artist_label, *album_label;
static lv_obj_t *seek_slider, *elapsed_label, *duration_label;
static lv_obj_t *play_icon;
static lv_obj_t *volume_slider, *volume_label, *volume_icon;
static lv_obj_t *top_buttons[5], *prev_button, *next_button;

static lv_img_dsc_t cover_dsc;
static uint32_t shown_player_version = UINT32_MAX;
static uint32_t shown_cover_version = UINT32_MAX;
static PlayerState state;

// Fondo dal colore della copertina (design/colore_copertina/SPEC_COLORE.md)
static uint32_t np_bg = UINT32_MAX;           // fondo applicato
static uint32_t cover_bg = COVER_BG_NONE;     // fondo calcolato dall'ultima copertina
static volatile int32_t forced_bg = -1;       // comando seriale COLORE, -1 se nessuno
static int32_t shown_forced_bg = -1;

static bool seek_dragging = false, volume_dragging = false;
static uint32_t seek_hold_until = 0, volume_hold_until = 0, play_hold_until = 0;

static uint32_t estimated_progress()
{
  uint32_t pos = state.progress_ms;
  if (state.is_playing) pos += millis() - state.progress_at_ms;
  return min(pos, state.duration_ms);
}

// ---------- Pezzi condivisi con l'Orologio ----------

void DeviceChip::create(lv_obj_t *parent)
{
  chip = ui_make_touch(parent, 16, 8, LV_SIZE_CONTENT, 48, COL_RAISED);
  lv_obj_set_style_radius(chip, 24, 0);
  lv_obj_set_style_bg_color(chip, COL_SURFACE, 0);
  lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(chip, 20, 0);
  lv_obj_set_style_max_width(chip, 320, 0);  // con 360 poteva toccare l'ora (SPEC 7 §6)
  lv_obj_set_style_pad_column(chip, 8, 0);
  lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  text_2 = COL_TEXT_2;
  icon = ui_make_label(chip, &font_ic_20, COL_TEXT_2, IC_COMPUTER);
  label = ui_make_label(chip, &font_it_16, COL_TEXT_2, "Nessun dispositivo attivo");
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_max_width(label, 224, 0);
  chevron = ui_make_label(chip, &font_ic_20, COL_TEXT_2, IC_DOWN);
  lv_obj_add_event_cb(chip, [](lv_event_t *e) { nav_open_section(SEC_DEVICES); }, LV_EVENT_CLICKED, nullptr);
}

void DeviceChip::update(const PlayerState &p)
{
  bool active = p.has_playback && p.device[0];
  ui_set_text_raw(icon, active ? ui_device_icon(spotify_device_icon(p.device_type)) : IC_COMPUTER);
  ui_set_text_color(icon, active ? COL_ACCENT : text_2);
  ui_set_text_raw(label, active ? p.device : "Nessun dispositivo attivo");
  ui_set_text_color(label, active ? COL_TEXT : text_2);
}

void DeviceChip::set_colors(lv_color_t surface, lv_color_t pressed, lv_color_t text_2_color)
{
  bool active = lv_color_to32(lv_obj_get_style_text_color(icon, 0)) == lv_color_to32(COL_ACCENT);
  text_2 = text_2_color;
  ui_set_bg(chip, surface);
  lv_obj_set_style_bg_color(chip, pressed, LV_STATE_PRESSED);
  if (!active) ui_set_text_color(icon, text_2);
  if (lv_color_to32(lv_obj_get_style_text_color(label, 0)) != lv_color_to32(COL_TEXT)) ui_set_text_color(label, text_2);
  ui_set_text_color(chevron, text_2);
}

void ui_make_top_icons(lv_obj_t *parent, bool now_playing, lv_obj_t **buttons_out, lv_obj_t **settings_dot_out)
{
  // Now playing: Home 472 · Ascolti 536 · Libreria 600 · Cerca 664 · Coda 728 (SPEC_HOME_GIORNI §5).
  // Home: Ascolti 536 · Libreria 600 · Cerca 664 · Impostazioni 728 (§3.1). SEC_NONE = Home, SECTION_COUNT = Impostazioni
  static const char *const NP_ICONS[] = { IC_HOME, IC_HEADPHONES, IC_LIBRARY, IC_SEARCH, IC_QUEUE };
  static const Section NP_SECTIONS[] = { SEC_NONE, SEC_HOME, SEC_LIBRARY, SEC_SEARCH, SEC_QUEUE };
  static const char *const HOME_ICONS[] = { IC_HEADPHONES, IC_LIBRARY, IC_SEARCH, IC_COG };
  static const Section HOME_SECTIONS[] = { SEC_HOME, SEC_LIBRARY, SEC_SEARCH, SECTION_COUNT };
  int n = now_playing ? 5 : 4;
  for (int i = 0; i < n; i++) {
    Section sec = now_playing ? NP_SECTIONS[i] : HOME_SECTIONS[i];
    lv_obj_t *b = ui_make_icon_button(parent, 792 - (n - i) * 64, 0, now_playing ? NP_ICONS[i] : HOME_ICONS[i], COL_TEXT_2,
                                      [](lv_event_t *e) {
                                        Section s = (Section)(intptr_t)lv_event_get_user_data(e);
                                        if (s == SEC_NONE) ui_open_home();
                                        else if (s == SECTION_COUNT) nav_open_view(view_settings());
                                        else nav_open_section(s);
                                      },
                                      (void *)(intptr_t)sec);
    if (buttons_out) buttons_out[i] = b;
    // Wi-Fi non collegato: pallino arancio sull'ingranaggio (772,14)
    if (!now_playing && i == 3 && settings_dot_out)
      *settings_dot_out = ui_make_box(b, 44, 14, 10, 10, COL_WARN, LV_RADIUS_CIRCLE);
  }
}

// ---------- Eventi ----------

static void play_cb(lv_event_t *e)
{
  // Inverto subito l'icona: Spotify conferma con il prossimo aggiornamento
  bool now_playing = strcmp(lv_label_get_text(play_icon), IC_PAUSE) == 0;
  spotify_command(now_playing ? CMD_PAUSE : CMD_PLAY);
  ui_set_text_raw(play_icon, now_playing ? IC_PLAY : IC_PAUSE);
  play_hold_until = millis() + 2000;
}

static void seek_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  int32_t target = (int64_t)lv_slider_get_value(seek_slider) * state.duration_ms / 1000;
  if (code == LV_EVENT_PRESSED) {
    seek_dragging = true;
  } else if (code == LV_EVENT_VALUE_CHANGED && seek_dragging) {
    char buf[16];
    ui_format_duration(buf, sizeof(buf), target);
    ui_set_text_raw(elapsed_label, buf);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    seek_dragging = false;
    seek_hold_until = millis() + 1500;
    spotify_command(CMD_SEEK, target);
    state.progress_ms = target;  // la barra resta dove l'ho lasciata
    state.progress_at_ms = millis();
  }
}

static void volume_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  int v = lv_slider_get_value(volume_slider);
  if (code == LV_EVENT_PRESSED) {
    volume_dragging = true;
  } else if (code == LV_EVENT_VALUE_CHANGED) {
    ui_set_text(volume_label, "%d", v);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    volume_dragging = false;
    volume_hold_until = millis() + 3000;
    spotify_command(CMD_VOLUME, v);
  }
}

// ---------- Costruzione ----------

// Slider (SPEC 4.16): binario 4 px, pomello 16, area di tocco alta 48
static lv_obj_t *make_slider(lv_obj_t *parent, int x, int y, int w, int max, lv_event_cb_t cb)
{
  lv_obj_t *s = lv_slider_create(parent);
  lv_obj_remove_style_all(s);
  lv_obj_set_pos(s, x, y);
  lv_obj_set_size(s, w, 4);
  lv_slider_set_range(s, 0, max);
  lv_obj_set_ext_click_area(s, 22);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, COL_RAISED, LV_PART_MAIN);
  lv_obj_set_style_radius(s, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, COL_PRESSED, LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, COL_TEXT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s, 2, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_INDICATOR | LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_bg_color(s, COL_TEXT, LV_PART_KNOB);
  lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_pad_all(s, 6, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DISABLED);
  lv_obj_add_event_cb(s, cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(s, cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(s, cb, LV_EVENT_PRESS_LOST, NULL);
  return s;
}

// Colori di Now playing dal fondo (SPEC_COLORE §2): i grigi diventano il fondo mescolato col bianco.
// Con #121212 ridanno i grigi di sempre. Un solo ridisegno: tutto cambia nello stesso giro di LVGL.
static void apply_np_colors(uint32_t bg)
{
  np_bg = bg;
  lv_color_t c_bg = lv_color_hex(bg);
  lv_color_t surface = lv_color_hex(cover_mix(bg, 20)), pressed = lv_color_hex(cover_mix(bg, 26));
  lv_color_t raised = lv_color_hex(cover_mix(bg, 44)), ph_fg = lv_color_hex(cover_mix(bg, 69));
  lv_color_t text_3 = lv_color_hex(cover_mix(bg, 140)), text_2 = lv_color_hex(cover_mix(bg, 174));

  ui_set_bg(panel, c_bg);
  chip.set_colors(surface, raised, text_2);
  ui_set_text_color(clock_label, text_2);
  for (lv_obj_t *b : top_buttons) {
    ui_set_bg(lv_obj_get_child(b, 0), pressed);
    ui_set_text_color(lv_obj_get_child(b, 1), text_2);
  }
  ui_set_bg(cover_placeholder, pressed);
  ui_set_text_color(cover_note, ph_fg);
  ui_set_text_color(artist_label, text_2);
  ui_set_text_color(album_label, text_3);
  lv_obj_t *sliders[] = { seek_slider, volume_slider };
  for (lv_obj_t *s : sliders) {
    ui_set_bg(s, raised);
    lv_obj_set_style_bg_color(s, pressed, LV_PART_MAIN | LV_STATE_DISABLED);
  }
  ui_set_text_color(elapsed_label, text_2);
  ui_set_text_color(duration_label, text_2);
  lv_obj_t *transport[] = { prev_button, next_button };
  for (lv_obj_t *b : transport) ui_set_bg(lv_obj_get_child(b, 0), pressed);
  ui_set_text_color(volume_icon, text_2);
  ui_set_text_color(volume_label, text_2);
}

// Nuovo fondo solo se cambia davvero (un canale di più di 8): evita ridisegni completi inutili
static void set_np_bg(uint32_t bg, bool force)
{
  if (!force && np_bg != UINT32_MAX) {
    bool near = true;
    for (int shift = 0; shift <= 16; shift += 8) {
      if (abs((int)((bg >> shift) & 0xFF) - (int)((np_bg >> shift) & 0xFF)) > 8) near = false;
    }
    if (near) return;
  }
  apply_np_colors(bg);
}

void player_force_bg(int32_t rgb)
{
  forced_bg = rgb;
}

lv_obj_t *player_panel_create(lv_obj_t *scr)
{
  panel = ui_make_panel(scr);

  chip.create(panel);
  clock_label = ui_make_label(panel, &font_it_20, COL_TEXT_2, "");
  lv_obj_set_width(clock_label, 100);
  lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(clock_label, LV_ALIGN_TOP_MID, 0, 19);
  ui_make_top_icons(panel, true, top_buttons);

  // Copertina: segnaposto con la nota finché non arriva l'immagine
  cover_placeholder = ui_make_box(panel, COVER_X, COVER_Y, COVER_SIDE, COVER_SIDE, COL_PRESSED);
  cover_note = ui_make_label(cover_placeholder, &font_ic_40, COL_PLACEHOLDER_FG, IC_NOTE);
  lv_obj_center(cover_note);
  cover_img = lv_img_create(panel);
  lv_obj_add_flag(cover_img, LV_OBJ_FLAG_HIDDEN);

  message_label = ui_make_label(panel, &font_it_16, COL_WARN, "");
  lv_obj_set_pos(message_label, COVER_X, COVER_Y + COVER_SIDE + 14);
  lv_obj_set_width(message_label, COVER_SIDE);
  lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);

  // Titolo fino a 2 righe, poi artisti e album, in colonna
  lv_obj_t *texts = lv_obj_create(panel);
  lv_obj_remove_style_all(texts);
  lv_obj_set_pos(texts, COL_X, COVER_Y);
  lv_obj_set_size(texts, COL_W, 160);
  lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(texts, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  title_label = ui_make_label(texts, &font_it_sb30, COL_TEXT, "");
  lv_obj_set_width(title_label, COL_W);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_pad_row(texts, 4, 0);  // titolo-artisti 6 px (2 di padding sotto), artisti-album 4
  artist_label = ui_make_line_label(texts, &font_it_24, COL_TEXT_2, 0, 0, COL_W);
  lv_obj_set_style_pad_top(artist_label, 2, 0);
  lv_obj_set_height(artist_label, lv_font_get_line_height(&font_it_24) + 2);
  album_label = ui_make_line_label(texts, &font_it_16, COL_TEXT_3, 0, 0, COL_W);

  // Barra di avanzamento (si può trascinare per spostarsi nel brano)
  seek_slider = make_slider(panel, COL_X, 276, COL_W, 1000, seek_cb);
  elapsed_label = ui_make_label(panel, &font_it_16, COL_TEXT_2, "0:00");
  lv_obj_set_pos(elapsed_label, COL_X, 290);
  duration_label = ui_make_label(panel, &font_it_16, COL_TEXT_2, "0:00");
  lv_obj_align(duration_label, LV_ALIGN_TOP_RIGHT, -(800 - 768), 290);

  // Tasti
  prev_button = ui_make_icon_button(panel, 430, 332, IC_PREV, COL_TEXT, [](lv_event_t *e) { spotify_command(CMD_PREVIOUS); },
                                    nullptr, nullptr, 56);
  next_button = ui_make_icon_button(panel, 638, 332, IC_NEXT, COL_TEXT, [](lv_event_t *e) { spotify_command(CMD_NEXT); },
                                    nullptr, nullptr, 56);
  // Play bianco con icona nera (SPEC_COLORE §2.2): il verde stonava su metà delle copertine
  lv_obj_t *play = ui_make_touch(panel, 522, 320, 88, 88, COL_TEXT_2);
  lv_obj_set_style_radius(play, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(play, COL_TEXT, 0);
  lv_obj_set_style_bg_opa(play, LV_OPA_COVER, 0);
  play_icon = ui_make_label(play, &font_ic_40, COL_ON_ACCENT, IC_PLAY);
  lv_obj_center(play_icon);
  lv_obj_add_event_cb(play, play_cb, LV_EVENT_CLICKED, NULL);

  // Volume
  volume_icon = ui_make_label(panel, &font_ic_20, COL_TEXT_2, IC_VOLUME);
  lv_obj_set_pos(volume_icon, COL_X, 434);
  volume_slider = make_slider(panel, 404, 442, 308, 100, volume_cb);
  volume_label = ui_make_label(panel, &font_it_16, COL_TEXT_2, "");
  lv_obj_set_width(volume_label, 48);
  lv_obj_set_style_text_align(volume_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(volume_label, 768 - 48, 433);

  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  apply_np_colors(COVER_BG_NONE);
  return panel;
}

// ---------- Aggiornamento ----------

static void update_title(const char *title)
{
  if (strcmp(lv_label_get_text(title_label), title) == 0) return;
  // Altezza su misura: 1 o 2 righe, così artista e album salgono quando il titolo è corto
  lv_point_t size;
  lv_txt_get_size(&size, title, &font_it_sb30, 0, 0, COL_W, LV_TEXT_FLAG_NONE);
  lv_coord_t line = lv_font_get_line_height(&font_it_sb30);
  lv_obj_set_height(title_label, size.y > line ? 2 * line : line);
  lv_label_set_text(title_label, title);
}

static void update_cover()
{
  CoverImage c;
  spotify_get_cover(&c);
  if (forced_bg != shown_forced_bg) {
    shown_forced_bg = forced_bg;
    set_np_bg(forced_bg >= 0 ? (uint32_t)forced_bg : cover_bg, true);
  }
  if (c.version == shown_cover_version) return;
  shown_cover_version = c.version;
  // Il colore nuovo nello stesso aggiornamento dello schermo dell'immagine (SPEC_COLORE §4)
  cover_bg = c.bg_rgb;
  if (forced_bg < 0) set_np_bg(cover_bg, false);
  if (c.pixels) {
    cover_dsc.header.always_zero = 0;
    cover_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565 come lo schermo
    cover_dsc.header.w = c.width;
    cover_dsc.header.h = c.height;
    cover_dsc.data_size = c.width * c.height * 2;
    cover_dsc.data = (const uint8_t *)c.pixels;
    lv_img_cache_invalidate_src(&cover_dsc);
    lv_img_set_src(cover_img, &cover_dsc);
    lv_obj_set_pos(cover_img, COVER_X + (COVER_SIDE - c.width) / 2, COVER_Y + (COVER_SIDE - c.height) / 2);
    lv_obj_invalidate(cover_img);
    ui_set_hidden(cover_img, false);
  } else {
    ui_set_hidden(cover_img, true);
  }
  spotify_cover_shown(c.version);
}

bool player_panel_update()
{
  PlayerState s;
  spotify_get_player(&s);
  if (!s.has_playback) return false;

  if (s.version != shown_player_version) {
    shown_player_version = s.version;
    // Durante il trascinamento della barra tengo la posizione scelta dall'utente
    bool keep_progress = seek_dragging || (int32_t)(seek_hold_until - millis()) > 0;
    uint32_t p_ms = state.progress_ms, p_at = state.progress_at_ms;
    state = s;
    if (keep_progress) {
      state.progress_ms = p_ms;
      state.progress_at_ms = p_at;
    }

    update_title(s.title);
    ui_set_text_raw(artist_label, s.artist);
    ui_set_text_raw(album_label, s.album);
    chip.update(s);
    ui_set_text_raw(message_label, s.message);
    if ((int32_t)(play_hold_until - millis()) <= 0) ui_set_text_raw(play_icon, s.is_playing ? IC_PAUSE : IC_PLAY);

    // Volume non regolabile sul dispositivo: slider spento e "—"
    bool vol = s.volume >= 0;
    if (lv_obj_has_state(volume_slider, LV_STATE_DISABLED) == vol) {
      if (vol) lv_obj_clear_state(volume_slider, LV_STATE_DISABLED);
      else lv_obj_add_state(volume_slider, LV_STATE_DISABLED);
    }
    if (!vol) {
      ui_set_text_raw(volume_label, "\xE2\x80\x94");
    } else if (!volume_dragging && (int32_t)(volume_hold_until - millis()) <= 0 && lv_slider_get_value(volume_slider) != s.volume) {
      lv_slider_set_value(volume_slider, s.volume, LV_ANIM_OFF);
      ui_set_text(volume_label, "%d", s.volume);
    }
    char buf[16];
    ui_format_duration(buf, sizeof(buf), s.duration_ms);
    ui_set_text_raw(duration_label, buf);
  }

  // Barra e tempo calcolati in locale tra un aggiornamento e l'altro
  if (!seek_dragging) {
    uint32_t pos = estimated_progress();
    int v = state.duration_ms ? (int64_t)pos * 1000 / state.duration_ms : 0;
    if (lv_slider_get_value(seek_slider) != v) lv_slider_set_value(seek_slider, v, LV_ANIM_OFF);
    char buf[16];
    ui_format_duration(buf, sizeof(buf), pos);
    ui_set_text_raw(elapsed_label, buf);
  }

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  ui_set_text(clock_label, "%02d:%02d", t.tm_hour, t.tm_min);

  update_cover();
  return true;
}
