#include "ui_nav.h"

#include <Arduino.h>
#include "ui.h"
#include "ui_common.h"
#include "ui_list.h"
#include "ui_views.h"

static const int MAX_DEPTH = 7;  // radice + 5 livelli (Home › Più ascoltati › Artista › Album › menu) + Dispositivi
static const uint32_t TOAST_MS = 2500;

static lv_obj_t *panel, *content;
static View *stack[MAX_DEPTH];
static int depth = 0;
static View *built = nullptr;  // la schermata di cui esistono i widget
static bool browsing = false;

// Le operazioni di navigazione partono dagli eventi dei widget che verranno cancellati:
// si eseguono subito dopo, fuori dall'evento (lv_async_call)
enum OpKind { OP_NONE, OP_PUSH, OP_BACK, OP_SECTION, OP_ROOT_VIEW, OP_HOME, OP_REBUILD };
static OpKind op = OP_NONE;
static View *op_view = nullptr;
static Section op_section;

// Barra laterale (SPEC_HOME_GIORNI §4): copertina, play/pausa, Avanti, poi Home della scheda e le sezioni
// di Spotify. La Coda non ha voce: si apre da Now playing.
enum RailItem { RAIL_HOME, RAIL_LISTEN, RAIL_LIBRARY, RAIL_SEARCH, RAIL_DEVICES, RAIL_COUNT };
static lv_obj_t *rail_cover_touch, *rail_play_circle, *rail_play_icon, *rail_next, *rail_next_icon;
static ImageBox rail_cover;
static lv_obj_t *rail_items[RAIL_COUNT], *rail_icons[RAIL_COUNT], *rail_bar;
static lv_obj_t *rail_dot;
static uint32_t play_hold_until = 0;

// Toast
static lv_obj_t *toast, *toast_icon, *toast_label;
static uint32_t toast_until = 0;

// Avvio di un brano in corso
static uint32_t play_req = 0;
static char *play_body = nullptr;
static char *play_fallback = nullptr;
static char play_name[100];

struct PendingAction {
  uint32_t id;
  const char *ok_toast;
};
static PendingAction actions[4];
static uint32_t seen_no_device = 0;

// ---------- Pila ----------

static View *section_root(Section s)
{
  switch (s) {
    case SEC_LIBRARY: return view_library();
    case SEC_SEARCH: return view_search();
    case SEC_QUEUE: return view_queue();
    case SEC_HOME: return view_home();
    default: return view_devices();
  }
}

static void destroy_widgets()
{
  if (built) {
    built->destroyed();
    built = nullptr;
  }
  ui_track_menu_forget();
  lv_obj_clean(content);
}

static void clear_stack()
{
  destroy_widgets();
  while (depth > 0) delete stack[--depth];
}

static void show_top()
{
  destroy_widgets();
  spotify_images_cancel();
  if (depth == 0) return;
  built = stack[depth - 1];
  built->build(content);
}

static void run_op(void *)
{
  OpKind k = op;
  View *v = op_view;
  op = OP_NONE;
  op_view = nullptr;
  switch (k) {
    case OP_PUSH:
      if (depth == MAX_DEPTH) {  // pila piena: tolgo il livello più vecchio dopo la radice
        destroy_widgets();
        delete stack[1];
        memmove(&stack[1], &stack[2], (MAX_DEPTH - 2) * sizeof(View *));
        depth--;
      }
      stack[depth++] = v;
      browsing = true;
      show_top();
      break;
    case OP_BACK:
      if (depth > 1) {
        destroy_widgets();
        delete stack[--depth];
        show_top();
      } else if (depth == 1 && stack[0]->back_goes_home()) {
        clear_stack();
        browsing = false;
      }
      break;
    case OP_SECTION:
      if (browsing && depth == 1 && stack[0]->section() == op_section) {
        stack[0]->reselect();
        break;
      }
      clear_stack();
      stack[depth++] = section_root(op_section);
      browsing = true;
      show_top();
      break;
    case OP_ROOT_VIEW:
      clear_stack();
      stack[depth++] = v;
      browsing = true;
      show_top();
      break;
    case OP_HOME:
      clear_stack();
      browsing = false;
      break;
    case OP_REBUILD:
      show_top();
      break;
    default: break;
  }
}

static void schedule(OpKind k, View *v = nullptr)
{
  if (op != OP_NONE && op_view && op_view != v) delete op_view;  // vince l'ultima richiesta
  bool scheduled = op != OP_NONE;
  op = k;
  op_view = v;
  if (!scheduled) lv_async_call(run_op, nullptr);
}

void nav_push(View *v) { schedule(OP_PUSH, v); }
void nav_open_view(View *root) { schedule(OP_ROOT_VIEW, root); }
void nav_home() { schedule(OP_HOME); }
void nav_rebuild() { schedule(OP_REBUILD); }
int nav_depth() { return depth; }
bool nav_browsing() { return browsing; }
lv_obj_t *nav_panel() { return panel; }

void nav_open_section(Section s)
{
  op_section = s;
  schedule(OP_SECTION);
}

void nav_back()
{
  if (depth > 0 && stack[depth - 1]->back()) return;
  schedule(OP_BACK);
}

// ---------- Toast (SPEC 4.12) ----------

void nav_toast(const char *text, bool error)
{
  ui_set_hidden(toast_icon, !error);
  lv_label_set_text(toast_label, text);
  lv_point_t sz;
  lv_txt_get_size(&sz, text, &font_it_16, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  int w = min(600, (int)sz.x + 48 + (error ? 28 : 0));
  lv_obj_set_size(toast, w, 48);
  lv_obj_set_pos(toast, 444 - w / 2, 408);
  lv_obj_set_style_bg_color(toast, error ? COL_WARN_BG : COL_TEXT, 0);
  lv_obj_set_style_border_width(toast, error ? 1 : 0, 0);
  lv_obj_set_style_text_color(toast_label, error ? COL_TEXT : COL_BG, 0);
  ui_set_hidden(toast, false);
  lv_obj_move_foreground(toast);
  toast_until = millis() + TOAST_MS;
}

static void create_toast()
{
  toast = ui_make_box(lv_layer_top(), 0, 408, 200, 48, COL_TEXT, 24);
  lv_obj_set_style_border_color(toast, COL_WARN, 0);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(toast, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(toast, 8, 0);
  toast_icon = ui_make_label(toast, &font_ic_20, COL_WARN, IC_WARN);
  toast_label = ui_make_label(toast, &font_it_16, COL_BG, "");
  lv_obj_add_event_cb(toast, [](lv_event_t *e) { ui_set_hidden(toast, true); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
}

// ---------- Comandi ----------

bool nav_play_busy() { return play_req != 0; }

void nav_play(const char *body_json, const char *context_name, const char *fallback_json)
{
  if (play_req) return;  // uno alla volta
  BrowseRequest r = {};
  r.kind = ACT_PLAY;
  r.body = spotify_strdup(body_json);
  spotify_free(play_body);
  play_body = spotify_strdup(body_json);
  spotify_free(play_fallback);
  play_fallback = fallback_json ? spotify_strdup(fallback_json) : nullptr;
  strlcpy(play_name, context_name ? context_name : "", sizeof(play_name));
  play_req = spotify_browse(r);
  if (!play_req) nav_toast("Non \xC3\xA8 partito. Riprova.", true);
}

void nav_action(const BrowseRequest &req, const char *ok_toast)
{
  for (auto &a : actions) {
    if (a.id) continue;
    a.id = spotify_browse(req);
    a.ok_toast = ok_toast;
    if (!a.id) nav_toast("Non \xC3\xA8 riuscito. Riprova.", true);
    return;
  }
}

static void check_commands(const PlayerState &p)
{
  BrowseResult r;
  if (play_req && spotify_take_result(play_req, &r)) {
    play_req = 0;
    // 404 senza motivo e corpo di riserva: di solito il brano dell'offset non c'è più (SPEC 7 §9.3)
    bool no_device = strcmp(r.reason, "NO_ACTIVE_DEVICE") == 0 || (r.code == 404 && !play_fallback);
    if (no_device) {
      // Nessun dispositivo: si sceglie dove suonare e il comando riparte da lì (SPEC 5.11, 13b)
      if (browsing) nav_push(view_devices(true, play_body, play_name, false));
      else nav_open_view(view_devices(true, play_body, play_name, true));
    } else if (r.code >= 400 && play_fallback) {
      Serial.printf("Avvio rifiutato (HTTP %d), provo il corpo di riserva\n", r.code);
      BrowseRequest q = {};
      q.kind = ACT_PLAY;
      q.body = play_fallback;  // lo libera il task Spotify
      spotify_free(play_body);
      play_body = spotify_strdup(play_fallback);
      play_fallback = nullptr;
      play_req = spotify_browse(q);
    } else if (r.code < 200 || r.code >= 300) {
      nav_toast("Non \xC3\xA8 partito. Riprova.", true);
    }
  }
  for (auto &a : actions) {
    if (!a.id || !spotify_take_result(a.id, &r)) continue;
    a.id = 0;
    if (r.code >= 200 && r.code < 300) nav_toast(a.ok_toast);
    else if (r.code == 404) nav_toast("Nessun dispositivo attivo", true);
    else nav_toast("Non \xC3\xA8 riuscito. Riprova.", true);
  }
  // Play/pausa, avanti... falliti per "nessun dispositivo attivo" (anche da Now playing)
  if (p.no_device_count != seen_no_device) {
    seen_no_device = p.no_device_count;
    if (!(depth > 0 && stack[depth - 1]->section() == SEC_DEVICES)) {
      if (browsing) nav_push(view_devices(true, nullptr, nullptr, false));
      else nav_open_view(view_devices(true, nullptr, nullptr, true));
    }
  }
}

// ---------- Barra laterale (SPEC_HOME_GIORNI §4) ----------

static void rail_play_cb(lv_event_t *e)
{
  PlayerState p;
  spotify_get_player(&p);
  bool playing = strcmp(lv_label_get_text(rail_play_icon), IC_PAUSE) == 0;
  spotify_command(playing ? CMD_PAUSE : CMD_PLAY);
  ui_set_text_raw(rail_play_icon, playing ? IC_PLAY : IC_PAUSE);
  play_hold_until = millis() + 2000;
}

static const Section RAIL_SECTIONS[RAIL_COUNT] = { SEC_NONE, SEC_HOME, SEC_LIBRARY, SEC_SEARCH, SEC_DEVICES };

static void rail_item_cb(lv_event_t *e)
{
  int i = (intptr_t)lv_event_get_user_data(e);
  if (i == RAIL_HOME) ui_open_home();  // la pila si azzera
  else nav_open_section(RAIL_SECTIONS[i]);
}

static void create_rail(lv_obj_t *parent)
{
  ui_make_box(parent, 0, 0, 88, 480, COL_RAIL);
  // Copertina: Now playing, oppure la Home se non c'è niente in riproduzione
  rail_cover_touch = ui_make_touch(parent, 0, 0, 88, 80, COL_RAIL_PRESSED);
  rail_cover.create(rail_cover_touch, 12, 8, 64, false, IC_NOTE);
  lv_obj_set_style_text_font(rail_cover.placeholder, &font_ic_28, 0);
  lv_obj_add_event_cb(rail_cover_touch, [](lv_event_t *e) {
    PlayerState p;
    spotify_get_player(&p);
    if (p.has_playback) ui_open_player();
    else ui_open_home();
  }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *play = ui_make_touch(parent, 0, 80, 88, 56, COL_RAIL, false);
  rail_play_circle = ui_make_box(play, 18, 2, 52, 52, COL_TEXT, LV_RADIUS_CIRCLE);
  lv_obj_set_style_bg_color(rail_play_circle, COL_TEXT_2, LV_STATE_PRESSED);
  rail_play_icon = ui_make_label(rail_play_circle, &font_ic_28, COL_ON_ACCENT, IC_PLAY);
  lv_obj_center(rail_play_icon);
  lv_obj_add_event_cb(play, rail_play_cb, LV_EVENT_CLICKED, nullptr);
  // Il cerchio si scurisce mentre l'area è premuta
  lv_obj_add_event_cb(play, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) lv_obj_add_state(rail_play_circle, LV_STATE_PRESSED);
    else lv_obj_clear_state(rail_play_circle, LV_STATE_PRESSED);
  }, LV_EVENT_ALL, nullptr);
  // Avanti: si resta dove si è
  rail_next = ui_make_icon_button(parent, 12, 132, IC_NEXT, COL_TEXT, [](lv_event_t *e) {
    spotify_command(CMD_NEXT);
  }, nullptr, &rail_next_icon);
  lv_obj_set_size(rail_next, 64, 64);
  ui_make_box(parent, 28, 196, 32, 1, COL_RAISED);

  // Voci alte 56 da y 200: Home, Ascolti (ex Home di Spotify), Libreria, Cerca, Dispositivi
  static const char *const ICONS[] = { IC_HOME, IC_HEADPHONES, IC_LIBRARY, IC_SEARCH, IC_COMPUTER };
  for (int i = 0; i < RAIL_COUNT; i++) {
    rail_items[i] = ui_make_touch(parent, 0, 200 + i * 56, 88, 56, COL_RAIL_PRESSED);
    rail_icons[i] = ui_make_label(rail_items[i], &font_ic_28, COL_TEXT_2, ICONS[i]);
    lv_obj_center(rail_icons[i]);
    lv_obj_add_event_cb(rail_items[i], rail_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
  rail_dot = ui_make_box(rail_items[RAIL_DEVICES], 52, 12, 10, 10, COL_WARN, LV_RADIUS_CIRCLE);
  rail_bar = ui_make_box(parent, 0, 0, 4, 32, COL_ACCENT);
  lv_obj_add_flag(rail_bar, LV_OBJ_FLAG_HIDDEN);
}

static void update_rail(const PlayerState &p)
{
  // Copertina (box 64 del brano attuale) o segnaposto
  rail_cover.set(p.has_playback ? p.cover_small : "", 0);
  rail_cover.tick();

  bool can_play = p.has_playback;
  ui_set_bg(rail_play_circle, can_play ? COL_TEXT : COL_PRESSED);
  ui_set_text_color(rail_play_icon, can_play ? COL_ON_ACCENT : COL_DISABLED_FG);
  if ((int32_t)(play_hold_until - millis()) <= 0) ui_set_text_raw(rail_play_icon, p.is_playing ? IC_PAUSE : IC_PLAY);
  ui_set_text_color(rail_next_icon, can_play ? COL_TEXT : COL_DISABLED_FG_2);
  if (lv_obj_has_flag(rail_next, LV_OBJ_FLAG_CLICKABLE) != can_play) {
    if (can_play) lv_obj_add_flag(rail_next, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(rail_next, LV_OBJ_FLAG_CLICKABLE);
  }

  // Voce attiva: la sezione da cui è partita la pila (la pagina artista aperta da Ascolti tiene accesi
  // Ascolti, SPEC 7 §2); Dispositivi aperta da un comando fallito accende Dispositivi; Meteo e Impostazioni
  // (SEC_NONE) accendono la Home; la Coda, aperta da Now playing, nessuna voce
  int active = -1;
  if (depth > 0) {
    Section s = stack[depth - 1]->section() == SEC_DEVICES ? SEC_DEVICES : stack[0]->section();
    for (int i = 0; i < RAIL_COUNT; i++)
      if (RAIL_SECTIONS[i] == s) active = i;
  }
  for (int i = 0; i < RAIL_COUNT; i++) {
    if (i == RAIL_DEVICES) continue;
    ui_set_text_color(rail_icons[i], i == active ? COL_TEXT : COL_TEXT_2);
  }
  // Dispositivi: icona del tipo attivo in verde, oppure computer grigio con pallino arancio
  bool device = p.has_playback && p.device[0];
  ui_set_text_raw(rail_icons[RAIL_DEVICES], device ? ui_device_icon(spotify_device_icon(p.device_type)) : IC_COMPUTER);
  ui_set_text_color(rail_icons[RAIL_DEVICES], device ? COL_ACCENT : (active == RAIL_DEVICES ? COL_TEXT : COL_TEXT_2));
  ui_set_hidden(rail_dot, device);

  // Barretta 4x32 a cima della voce + 12
  ui_set_hidden(rail_bar, active < 0);
  if (active >= 0 && lv_obj_get_y(rail_bar) != 212 + active * 56) lv_obj_set_y(rail_bar, 212 + active * 56);
}

// ---------- Creazione e aggiornamento ----------

void nav_create(lv_obj_t *scr)
{
  panel = ui_make_panel(scr);
  lv_obj_set_style_bg_color(panel, COL_BG, 0);
  create_rail(panel);
  content = lv_obj_create(panel);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 88, 0);
  lv_obj_set_size(content, 712, 480);
  lv_obj_set_style_bg_color(content, COL_BG, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  // Swipe sull'area contenuto: a destra = indietro, su/giù = pagine (li gestisce la schermata)
  lv_obj_add_event_cb(content, [](lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT && depth > 1) nav_back();
    else if (depth > 0 && built) built->gesture(dir);
  }, LV_EVENT_GESTURE, nullptr);
  create_toast();

  PlayerState p;
  spotify_get_player(&p);
  seen_no_device = p.no_device_count;
}

void nav_tick()
{
  PlayerState p;
  spotify_get_player(&p);
  check_commands(p);
  if (toast_until && (int32_t)(toast_until - millis()) <= 0) {
    toast_until = 0;
    ui_set_hidden(toast, true);
  }
  if (!browsing) return;
  update_rail(p);
  if (built && op == OP_NONE) built->tick();
}
