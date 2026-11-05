#include "ui_list.h"

#include <Arduino.h>
#include "ui_common.h"
#include "ui_nav.h"
#include "ui_views.h"

static const uint32_t LIST_CACHE_MS = 5 * 60 * 1000;  // tornando indietro l'elenco resta valido 5 minuti

// Ultimi elenchi chiusi (Playlist, Album, ...): riaprendoli entro 5 minuti non si riscaricano
struct CachedList {
  char key[56];
  ItemStore store;
};
static CachedList list_cache[10];

static void store_move(ItemStore &dst, ItemStore &src)
{
  dst.clear();
  dst = src;
  src.items = nullptr;
  src.clear();
}

static const char *placeholder_for(uint8_t type)
{
  switch (type) {
    case IT_ALBUM: return IC_DISC;
    case IT_ARTIST: return IC_USER;
    default: return IC_NOTE;
  }
}

// ---------- ImageBox ----------

void ImageBox::create(lv_obj_t *parent, int x, int y, int sz, bool round, const char *placeholder_icon)
{
  size = sz;
  url[0] = '\0';
  shown = false;
  box = ui_make_box(parent, x, y, sz, sz, COL_PRESSED, round ? LV_RADIUS_CIRCLE : 0);
  if (round) lv_obj_set_style_clip_corner(box, true, 0);
  placeholder = ui_make_label(box, sz >= 150 ? &font_ic_40 : &font_ic_20, COL_PLACEHOLDER_FG,
                              placeholder_icon ? placeholder_icon : IC_NOTE);
  lv_obj_center(placeholder);
  img = lv_img_create(box);
  lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  memset(&dsc, 0, sizeof(dsc));
}

void ImageBox::set(const char *u, uint8_t s, const char *placeholder_icon)
{
  if (!box) return;
  if (placeholder_icon) ui_set_text_raw(placeholder, placeholder_icon);
  if (strcmp(url, u ? u : "") == 0) return;
  strlcpy(url, u ? u : "", sizeof(url));
  scale = s;
  shown = false;
  ui_set_hidden(img, true);
  ui_set_hidden(placeholder, false);
  if (url[0]) spotify_image_request(url, scale, size);
}

void ImageBox::tick()
{
  if (!box || shown || !url[0]) return;
  ThumbImage t;
  bool failed;
  if (!spotify_image_get(url, size, &t, &failed)) {
    // Annullata da un cambio pagina mentre era in attesa: la richiedo di nuovo (se c'è già non succede niente)
    if (!failed) spotify_image_request(url, scale, size);
    return;
  }
  dsc.header.always_zero = 0;
  dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565 come lo schermo
  dsc.header.w = t.w;
  dsc.header.h = t.h;
  dsc.data_size = t.w * t.h * 2;
  dsc.data = (const uint8_t *)t.pixels;
  lv_img_set_src(img, &dsc);
  lv_obj_set_pos(img, (size - t.w) / 2, (size - t.h) / 2);
  lv_obj_invalidate(img);
  ui_set_hidden(img, false);
  ui_set_hidden(placeholder, true);
  shown = true;
}

// ---------- Intestazione, freccia, blocco stato ----------

lv_obj_t *ui_make_back_button(lv_obj_t *root)
{
  return ui_make_icon_button(root, 0, 0, IC_LEFT, COL_TEXT, [](lv_event_t *e) { nav_back(); });
}

void Header::create(lv_obj_t *root, bool with_back, const char *title_text)
{
  if (with_back) back = ui_make_back_button(root);
  int x = with_back ? CX(156) : CX(112);
  title = ui_make_line_label(root, &font_it_sb24, COL_TEXT, x, 17, CX(540) - x);
  ui_set_text_raw(title, title_text);
  info = ui_make_label(root, &font_it_16, COL_TEXT_3, "");
  lv_obj_align(info, LV_ALIGN_TOP_RIGHT, -(800 - 720), 21);
  spinner = ui_make_spinner(root, 28);
  lv_obj_set_pos(spinner, CX(692), 18);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
}

void Header::set_loading(bool loading)
{
  if (!spinner) return;
  ui_set_hidden(spinner, !loading);
  ui_set_hidden(info, loading);
}

void ui_label_lines(lv_obj_t *label, const char *text, int width, int max_lines)
{
  const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
  lv_point_t size;
  lv_txt_get_size(&size, text, font, 0, 0, width, LV_TEXT_FLAG_NONE);
  lv_coord_t line = lv_font_get_line_height(font);
  int lines = constrain((size.y + line / 2) / line, 1, max_lines);
  lv_obj_set_size(label, width, lines * line);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  ui_set_text_raw(label, text);
}

lv_obj_t *ui_state_block(lv_obj_t *parent, int x0, int w, const char *icon, lv_color_t icon_color, const char *title,
                         const char *text, const char *button, lv_event_cb_t cb, void *user)
{
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_set_pos(c, x0, 0);
  lv_obj_set_size(c, w, 480);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  int dy = button ? 0 : 8;  // senza pulsante il blocco scende di 8 px
  lv_obj_t *circle = ui_make_box(c, (w - 80) / 2, 112 + dy, 80, 80, COL_SURFACE, LV_RADIUS_CIRCLE);
  lv_obj_t *ic = ui_make_label(circle, &font_ic_40, icon_color, icon);
  lv_obj_center(ic);

  // Titolo, testo e pulsante in colonna: se il titolo va su due righe il resto scende
  lv_obj_t *col = lv_obj_create(c);
  lv_obj_remove_style_all(col);
  lv_obj_set_pos(col, 24, 216 + dy);
  lv_obj_set_size(col, w - 48, 480 - 216 - dy);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 8, 0);
  lv_obj_t *t = ui_make_label(col, &font_it_sb20, COL_TEXT, title);
  lv_obj_set_width(t, w - 48);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  if (text) {
    lv_obj_t *d = ui_make_label(col, &font_it_16, COL_TEXT_2, text);
    lv_obj_set_width(d, w - 48);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
  }
  if (button) {
    lv_point_t sz;
    lv_txt_get_size(&sz, button, &font_it_sb20, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int bw = max(176, (int)sz.x + 56);
    lv_obj_t *gap = lv_obj_create(col);  // 8 + 8 + 8 = 24 px sotto il testo
    lv_obj_remove_style_all(gap);
    lv_obj_set_size(gap, 1, 8);
    ui_make_button(col, BTN_SECONDARY, 0, 0, bw, 56, nullptr, button, cb, user);
  }
  return c;
}

// ---------- Pannello dettaglio ----------

void DetailPanel::create(lv_obj_t *root, const char *context_text, bool round, const char *placeholder_icon,
                         lv_event_cb_t play_cb, void *user)
{
  bg = ui_make_box(root, 0, 0, 232, 480, COL_PANEL);
  ui_make_back_button(root);
  context = ui_make_line_label(root, &font_it_16, COL_TEXT_2, CX(152), 21, 160);
  ui_set_text_raw(context, context_text);
  image.create(root, CX(112), 72, 150, round, placeholder_icon);

  lv_obj_t *col = lv_obj_create(root);
  lv_obj_remove_style_all(col);
  lv_obj_set_pos(col, CX(112), 234);
  lv_obj_set_size(col, 184, 160);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 6, 0);
  name = ui_make_label(col, &font_it_sb24, COL_TEXT, "");
  data = ui_make_label(col, &font_it_16, COL_TEXT_2, "");
  ui_label_lines(name, "", 184, 2);
  ui_label_lines(data, "", 184, 2);

  play = ui_make_button(root, BTN_PRIMARY, CX(112), 400, 184, 56, IC_PLAY, "Riproduci", play_cb, user);
}

void DetailPanel::set(const BrowseItem &it, const char *data_text)
{
  if (!name) return;
  ui_label_lines(name, it.title, 184, 2);
  ui_set_text_color(name, (it.flags & IF_TITLE_FALLBACK) ? COL_TEXT_3 : COL_TEXT);
  ui_label_lines(data, data_text, 184, 2);
  image.set(it.img_big, it.img_big_scale);
}

// ---------- ItemStore ----------

void ItemStore::clear()
{
  heap_caps_free(items);
  items = nullptr;
  count = cap = 0;
  total = -1;
  more = false;
  cursor[0] = '\0';
  loaded_ms = 0;
}

void ItemStore::append(const BrowseItem *src, int n)
{
  if (n <= 0) return;
  if (count + n > cap) {
    int ncap = max(cap * 2, count + n);
    BrowseItem *p = (BrowseItem *)heap_caps_realloc(items, ncap * sizeof(BrowseItem), MALLOC_CAP_SPIRAM);
    if (!p) return;
    items = p;
    cap = ncap;
  }
  memcpy(items + count, src, n * sizeof(BrowseItem));
  count += n;
}

// ---------- PagedList ----------

struct PagedList::Row {
  PagedList *owner;
  int slot;
  lv_obj_t *touch, *num, *title, *sub, *right, *right_icon, *spinner, *circle, *circle_icon;
  ImageBox img;
};

struct PagedList::Widgets {
  lv_obj_t *parent;
  Row rows[5];
  lv_obj_t *up, *up_circle, *up_icon, *down, *down_circle, *down_icon, *counter;
  lv_obj_t *skeleton, *block, *end_note;
};

static lv_obj_t *make_pager_button(lv_obj_t *parent, int x, int y, const char *icon, lv_obj_t **circle, lv_obj_t **ic)
{
  lv_obj_t *t = ui_make_touch(parent, x, y, 64, 80, COL_BG, false);
  *circle = ui_make_box(t, 8, 16, 48, 48, COL_SURFACE, LV_RADIUS_CIRCLE);
  lv_obj_set_style_bg_color(*circle, COL_RAISED, LV_STATE_PRESSED);
  *ic = ui_make_label(*circle, &font_ic_28, COL_TEXT, icon);
  lv_obj_center(*ic);
  return t;
}

void PagedList::build(lv_obj_t *parent)
{
  // Elenco chiuso da poco: lo riprendo dalla memoria prima che refresh() chieda la prima pagina
  if (cache_key[0] && !store.loaded_ms) {
    for (auto &c : list_cache) {
      if (strcmp(c.key, cache_key) != 0) continue;
      if (c.store.loaded_ms && millis() - c.store.loaded_ms < LIST_CACHE_MS) store_move(store, c.store);
      else c.store.clear();
      c.key[0] = '\0';
    }
  }
  wd = new Widgets();
  memset(wd, 0, sizeof(Widgets));
  wd->parent = parent;
  bool media = style == ROW_MEDIA || style == ROW_DETAIL_MEDIA;
  int text_x = style == ROW_NUMBERED ? 48 : (style == ROW_RANK ? 128 : 96);

  for (int i = 0; i < rows; i++) {
    Row &r = wd->rows[i];
    r.owner = this;
    r.slot = i;
    r.touch = ui_make_touch(parent, x, y + i * 80, w, 80, COL_PRESSED, clickable);
    lv_obj_set_style_bg_color(r.touch, COL_PRESSED, LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(r.touch, LV_OPA_COVER, LV_STATE_CHECKED);
    if (!clickable) lv_obj_clear_flag(r.touch, LV_OBJ_FLAG_CLICKABLE);
    if (media) {
      r.img.create(r.touch, 16, 8, 64, round_images, IC_NOTE);
    } else if (style == ROW_RANK) {
      r.img.create(r.touch, 48, 8, 64, round_images, IC_NOTE);
    } else if (style == ROW_DEVICE) {
      r.circle = ui_make_box(r.touch, 16, 12, 56, 56, COL_SURFACE, LV_RADIUS_CIRCLE);
      r.circle_icon = ui_make_label(r.circle, &font_ic_28, COL_TEXT_2, IC_COMPUTER);
      lv_obj_center(r.circle_icon);
    }
    if (style == ROW_NUMBERED || style == ROW_RANK) {
      r.num = ui_make_label(r.touch, &font_it_16, COL_TEXT_3, "");
      lv_obj_set_pos(r.num, 0, 29);
      lv_obj_set_width(r.num, 48);
      lv_obj_set_style_text_align(r.num, LV_TEXT_ALIGN_CENTER, 0);
    }
    r.title = ui_make_line_label(r.touch, &font_it_sb20, COL_TEXT, text_x, 15, w - text_x - 24);
    r.sub = ui_make_line_label(r.touch, &font_it_16, COL_TEXT_2, text_x, 43, w - text_x - 24);
    r.right = ui_make_label(r.touch, &font_it_16, COL_TEXT_3, "");
    lv_obj_set_width(r.right, 64);
    lv_obj_set_style_text_align(r.right, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(r.right, w - 16 - 64, 29);
    r.right_icon = ui_make_label(r.touch, &font_ic_20, COL_ACCENT, IC_VOLUME);
    lv_obj_add_flag(r.right_icon, LV_OBJ_FLAG_HIDDEN);
    r.spinner = ui_make_spinner(r.touch, 20);
    lv_obj_set_pos(r.spinner, w - 16 - 20, 30);
    lv_obj_add_flag(r.spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(r.touch, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(r.touch, row_event, LV_EVENT_SHORT_CLICKED, &r);
    if (track_menu || on_long) lv_obj_add_event_cb(r.touch, row_event, LV_EVENT_LONG_PRESSED, &r);
  }

  // Colonna pagine (SPEC 4.5)
  int px = x + w;
  wd->up = make_pager_button(parent, px, y, IC_UP, &wd->up_circle, &wd->up_icon);
  wd->down = make_pager_button(parent, px, y + rows * 80 - 80, IC_DOWN, &wd->down_circle, &wd->down_icon);
  lv_obj_add_event_cb(wd->up, pager_event, LV_EVENT_CLICKED, (void *)this);
  lv_obj_add_event_cb(wd->down, pager_event, LV_EVENT_CLICKED, (void *)this);
  wd->counter = ui_make_label(parent, &font_it_16, COL_TEXT_3, "");
  lv_obj_set_width(wd->counter, 64);
  lv_obj_set_style_text_align(wd->counter, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(wd->counter, px, y + rows * 40 - 11);

  // Scheletro statico (SPEC 4.8)
  static const int BAR1[] = { 240, 300, 200, 260, 220 }, BAR2[] = { 160, 120, 150, 180, 140 };
  wd->skeleton = lv_obj_create(parent);
  lv_obj_remove_style_all(wd->skeleton);
  lv_obj_set_pos(wd->skeleton, x, y);
  lv_obj_set_size(wd->skeleton, w, rows * 80);
  lv_obj_clear_flag(wd->skeleton, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < rows; i++) {
    int bx = text_x;
    if (style != ROW_NUMBERED)
      ui_make_box(wd->skeleton, style == ROW_RANK ? 48 : 16, i * 80 + 8, 64, 64, COL_SURFACE,
                  style == ROW_DEVICE || round_images ? LV_RADIUS_CIRCLE : 0);
    ui_make_box(wd->skeleton, bx, i * 80 + 20, min(BAR1[i], w - bx - 24), 16, COL_SURFACE, 8);
    ui_make_box(wd->skeleton, bx, i * 80 + 48, BAR2[i], 12, COL_SKELETON_2, 8);
  }
  lv_obj_add_flag(wd->skeleton, LV_OBJ_FLAG_HIDDEN);

  wd->end_note = ui_make_label(parent, &font_it_16, COL_TEXT_3, "");
  lv_obj_set_width(wd->end_note, w);
  lv_obj_set_style_text_align(wd->end_note, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(wd->end_note, LV_OBJ_FLAG_HIDDEN);
  shown_player_version = UINT32_MAX;
  refresh();
}

void PagedList::destroyed()
{
  delete wd;
  wd = nullptr;
  menu_index = -1;
}

int PagedList::known_total() const { return store.total >= 0 ? store.total : store.count; }

int PagedList::page_count() const
{
  int n = known_total();
  return max(1, (n + rows - 1) / rows);
}

const ItemStore *list_cache_peek(const char *key)
{
  for (auto &c : list_cache)
    if (strcmp(c.key, key) == 0 && c.store.loaded_ms && millis() - c.store.loaded_ms < LIST_CACHE_MS) return &c.store;
  return nullptr;
}

void list_cache_put(const char *key, ItemStore &store)
{
  CachedList *slot = &list_cache[0];
  for (auto &c : list_cache) {
    if (strcmp(c.key, key) == 0) { slot = &c; break; }
    if (c.store.loaded_ms < slot->store.loaded_ms) slot = &c;  // la più vecchia (o una libera)
  }
  strlcpy(slot->key, key, sizeof(slot->key));
  store_move(slot->store, store);
}

PagedList::~PagedList()
{
  if (!cache_key[0] || !store.loaded_ms || error || !store.count) return;
  list_cache_put(cache_key, store);
}

void PagedList::start()
{
  if (store.loaded_ms && millis() - store.loaded_ms < LIST_CACHE_MS && !error) {
    refresh();
    return;
  }
  store.clear();
  page = 0;
  error = false;
  request_more();
  refresh();
}

void PagedList::reload()
{
  close_menu();
  store.clear();
  error = false;
  page = 0;
  req = 0;
  request_more();
  refresh();
}

void PagedList::request_more(bool background)
{
  if (req || !fetch) return;
  spotify_browse_background(background);
  req = fetch(store.count, store.cursor);
  spotify_browse_background(false);
}

void PagedList::set_result(const BrowseResult &r)
{
  if (r.code < 200 || r.code >= 300) {
    error = true;
    error_code = r.code;
  } else {
    error = false;
    if (search_type >= 0) {
      for (int i = 0; i < r.count; i++)
        if (r.items[i].type == search_type) store.append(&r.items[i], 1);
      store.total = -1;
      store.more = store.count < r.search_total[search_type];
    } else {
      store.append(r.items, r.count);
      store.total = r.total;
      // Artisti: si va avanti solo con il cursore; se manca, non ce ne sono altri
      strlcpy(store.cursor, r.next, sizeof(store.cursor));
    }
    store.loaded_ms = millis();
  }
  refresh();
}

void PagedList::set_page(int p)
{
  if (p < 0) return;
  bool can_go = p < page_count() || (search_type >= 0 && store.more && p == page_count());
  if (!can_go || p == page) return;
  close_menu();
  page = p;
  spotify_images_cancel();  // le miniature della pagina lasciata non servono più
  refresh();
}

bool PagedList::gesture(lv_dir_t dir)
{
  if (dir == LV_DIR_TOP) set_page(page + 1);
  else if (dir == LV_DIR_BOTTOM) set_page(page - 1);
  else return false;
  return true;
}

void PagedList::render_row(Row &r, int index)
{
  const BrowseItem &it = store.items[index];
  bool restricted = it.flags & IF_RESTRICTED;
  bool playing = is_playing ? is_playing(it) : it.type == IT_TRACK && playing_uri[0] && strcmp(it.uri, playing_uri) == 0;
  bool busy = index == busy_index;
  bool with_duration = right_text || (it.type == IT_TRACK && style != ROW_DETAIL_MEDIA);

  ui_set_hidden(r.touch, false);
  ui_set_text_raw(r.title, it.title);
  lv_color_t tc = COL_TEXT;
  if (it.flags & IF_TITLE_FALLBACK) tc = COL_TEXT_3;
  if (playing) tc = COL_ACCENT;
  if (restricted) tc = COL_DISABLED_FG;
  ui_set_text_color(r.title, tc);
  int text_x = style == ROW_NUMBERED ? 48 : (style == ROW_RANK ? 128 : 96);
  int right_margin = style == ROW_NUMBERED ? 88 : (with_duration ? 96 : 24);
  if (style == ROW_DETAIL_MEDIA) right_margin = 16;
  if (lv_obj_get_style_width(r.title, 0) != w - text_x - right_margin) {
    lv_obj_set_width(r.title, w - text_x - right_margin);
    lv_obj_set_width(r.sub, w - text_x - right_margin);
  }
  ui_set_text_raw(r.sub, busy && style == ROW_DEVICE ? "Sposto la musica qui\xE2\x80\xA6" : it.subtitle);
  ui_set_text_color(r.sub, restricted ? COL_DISABLED_FG : COL_TEXT_2);
  if (style == ROW_RANK) {  // senza sottotitolo (artisti) il titolo sta al centro della riga
    int ty = it.subtitle[0] ? 15 : 27;
    if (lv_obj_get_y(r.title) != ty) lv_obj_set_y(r.title, ty);
    ui_set_hidden(r.sub, !it.subtitle[0]);
  }

  if (style == ROW_MEDIA || style == ROW_DETAIL_MEDIA || style == ROW_RANK) {
    if (it.flags & IF_LIKED) r.img.set("", 0, IC_HEART);  // Brani piaciuti: cuore fisso (SPEC 7 §3.1)
    else r.img.set(it.img, it.img_scale, placeholder_for(it.type));
    ui_set_text_color(r.img.placeholder, (it.flags & IF_LIKED) ? COL_ACCENT : COL_PLACEHOLDER_FG);
  }
  if (r.num) {
    if (playing && style == ROW_NUMBERED) {
      ui_set_text_raw(r.num, IC_VOLUME);
      lv_obj_set_style_text_font(r.num, &font_ic_20, 0);
      ui_set_text_color(r.num, COL_ACCENT);
    } else {
      lv_obj_set_style_text_font(r.num, &font_it_16, 0);
      ui_set_text(r.num, "%d", it.number ? it.number : index + 1);
      ui_set_text_color(r.num, COL_TEXT_3);
    }
  }
  if (r.circle) {
    bool active = it.flags & IF_ACTIVE;
    ui_set_bg(r.circle, busy ? COL_RAISED : (active ? COL_ACCENT : COL_SURFACE));
    ui_set_text_raw(r.circle_icon, ui_device_icon(it.icon));
    ui_set_text_color(r.circle_icon, active && !busy ? COL_ON_ACCENT : (restricted ? COL_DISABLED_FG : COL_TEXT_2));
  }

  // Elemento a destra: durata, icona (brano in riproduzione, dispositivo attivo) o spinner
  char dur[16] = "";
  if (right_text) right_text(it, dur, sizeof(dur));
  else if (with_duration) ui_format_duration(dur, sizeof(dur), it.duration_ms);
  bool icon = false;
  if (style == ROW_DEVICE && (it.flags & IF_ACTIVE)) {
    ui_set_text_raw(r.right_icon, IC_CHECK);
    icon = true;
  } else if (playing && (style == ROW_MEDIA || style == ROW_RANK)) {
    ui_set_text_raw(r.right_icon, IC_VOLUME);
    icon = true;
  }
  if (icon) lv_obj_align(r.right_icon, LV_ALIGN_TOP_RIGHT, -16, 30);
  ui_set_hidden(r.spinner, !busy);
  ui_set_hidden(r.right_icon, busy || !icon);
  ui_set_hidden(r.right, busy || icon || !dur[0]);
  ui_set_text_raw(r.right, dur);

  bool can_click = clickable && !restricted;
  if (lv_obj_has_flag(r.touch, LV_OBJ_FLAG_CLICKABLE) != can_click) {
    if (can_click) lv_obj_add_flag(r.touch, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(r.touch, LV_OBJ_FLAG_CLICKABLE);
  }
  bool checked = busy || index == menu_index;
  if (lv_obj_has_state(r.touch, LV_STATE_CHECKED) != checked) {
    if (checked) lv_obj_add_state(r.touch, LV_STATE_CHECKED);
    else lv_obj_clear_state(r.touch, LV_STATE_CHECKED);
  }
  if (on_row_extra) on_row_extra(r.touch, index);
}

void PagedList::refresh()
{
  if (!wd) return;
  if (wd->block) {
    lv_obj_del(wd->block);
    wd->block = nullptr;
  }
  int first = page * rows;
  bool all_loaded = search_type >= 0 ? !store.more : (store.total >= 0 && store.count >= store.total);
  bool empty = store.loaded_ms && store.count == 0 && all_loaded && !error;
  bool have_rows = first < store.count;
  bool show_error = !have_rows && error;
  bool show_skeleton = !have_rows && !error && !empty;
  if (show_skeleton && !all_loaded) request_more();
  if (show_skeleton && all_loaded) show_skeleton = false;  // pagina oltre la fine (non dovrebbe succedere)

  int shown = 0;
  for (int i = 0; i < rows; i++) {
    Row &r = wd->rows[i];
    int idx = first + i;
    if (have_rows && idx < store.count) {
      render_row(r, idx);
      shown++;
    } else {
      ui_set_hidden(r.touch, true);
    }
  }
  // Pagina caricata solo in parte, oppure si è arrivati alla penultima pagina caricata: chiedo il
  // blocco dopo in anticipo, così la pagina successiva è già pronta quando ci si arriva
  if (have_rows && !all_loaded && (shown < rows || (page + 2) * rows >= store.count))
    request_more(shown == rows);

  ui_set_hidden(wd->skeleton, !show_skeleton);

  if (show_error) {
    char title[96];
    snprintf(title, sizeof(title), "Non riesco a caricare %s", what);
    wd->block = ui_state_block(wd->parent, x, w + (x + w + 64 <= 712 ? 64 : 0), IC_WARN, COL_WARN, title,
                               "Controlla il Wi-Fi e riprova.", "Riprova", retry_event, this);
  } else if (empty && empty_title) {
    wd->block = ui_state_block(wd->parent, x, w + (x + w + 64 <= 712 ? 64 : 0), empty_icon ? empty_icon : IC_NOTE,
                               COL_TEXT_2, empty_title, empty_text, empty_button, empty_cb, empty_user);
  }

  // Colonna pagine: nascosta se basta una pagina o se si sta caricando la prima
  int pages = page_count();
  bool open_more = search_type >= 0 && store.more;
  bool pager = (pages > 1 || open_more) && !(store.count == 0);
  ui_set_hidden(wd->up, !pager);
  ui_set_hidden(wd->down, !pager);
  ui_set_hidden(wd->counter, !pager);
  if (pager) {
    bool up_on = page > 0, down_on = page + 1 < pages || open_more;
    ui_set_bg(wd->up_circle, up_on ? COL_SURFACE : COL_DISABLED_BG);
    ui_set_text_color(wd->up_icon, up_on ? COL_TEXT : COL_DISABLED_FG_2);
    ui_set_bg(wd->down_circle, down_on ? COL_SURFACE : COL_DISABLED_BG);
    ui_set_text_color(wd->down_icon, down_on ? COL_TEXT : COL_DISABLED_FG_2);
    if (up_on) lv_obj_add_flag(wd->up, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(wd->up, LV_OBJ_FLAG_CLICKABLE);
    if (down_on) lv_obj_add_flag(wd->down, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(wd->down, LV_OBJ_FLAG_CLICKABLE);
    ui_set_text(wd->counter, "%d/%d%s", page + 1, max(pages, page + 1), open_more ? "+" : "");
  }

  // "Fine dell'elenco · 27 artisti" sotto l'ultima riga dell'ultima pagina
  bool end = have_rows && all_loaded && shown < rows && page > 0 && !hide_end_note;
  ui_set_hidden(wd->end_note, !end);
  if (end) {
    ui_set_text(wd->end_note, "Fine dell'elenco \xC2\xB7 %d %s", store.count, noun_plural);
    lv_obj_set_pos(wd->end_note, x, y + shown * 80 + 24);
  }
}

void PagedList::tick()
{
  if (req) {
    BrowseResult r;
    if (spotify_take_result(req, &r)) {
      req = 0;
      if (on_result) on_result(r);
      set_result(r);
      spotify_free(r.items);
      spotify_free(r.meta);
    }
  }
  if (!wd) return;
  for (int i = 0; i < rows; i++)
    if (!lv_obj_has_flag(wd->rows[i].touch, LV_OBJ_FLAG_HIDDEN)) wd->rows[i].img.tick();

  // Brano in riproduzione evidenziato: ridisegno solo quando cambia
  PlayerState p;
  spotify_get_player(&p);
  if (p.version != shown_player_version) {
    shown_player_version = p.version;
    const char *uri = p.has_playback ? p.track_uri : "";
    if (strcmp(uri, playing_uri) != 0) {
      strlcpy(playing_uri, uri, sizeof(playing_uri));
      refresh();
    }
  }
}

void PagedList::row_event(lv_event_t *e)
{
  Row *r = (Row *)lv_event_get_user_data(e);
  PagedList *l = r->owner;
  int index = l->page * l->rows + r->slot;
  if (index >= l->store.count) return;
  if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
    if (l->on_long) l->on_long(index);
    else if (l->store.items[index].type == IT_TRACK && l->store.items[index].uri[0]) l->open_menu(index);
    return;
  }
  if (ui_click_is_gesture()) return;
  if (l->on_click) l->on_click(index);
}

void PagedList::pager_event(lv_event_t *e)
{
  PagedList *l = (PagedList *)lv_event_get_user_data(e);
  lv_obj_t *t = lv_event_get_target(e);
  l->set_page(l->page + (t == l->wd->up ? -1 : 1));
}

void PagedList::retry_event(lv_event_t *e)
{
  PagedList *l = (PagedList *)lv_event_get_user_data(e);
  l->error = false;
  l->request_more();
  l->refresh();
}

// ---------- Menu del brano (SPEC 4.11) ----------

enum MenuAction { MENU_QUEUE, MENU_ALBUM, MENU_ARTIST };

static lv_obj_t *menu_obj = nullptr;
static BrowseItem menu_item;  // copia: la schermata può sparire mentre il menu è aperto
static std::function<void()> menu_closed;

static void menu_event(lv_event_t *e)
{
  MenuAction action = (MenuAction)(intptr_t)lv_event_get_user_data(e);
  BrowseItem it = menu_item;
  ui_track_menu_close();
  if (action == MENU_QUEUE) {
    BrowseRequest r = {};
    r.kind = ACT_QUEUE_ADD;
    strlcpy(r.arg, it.track_uri[0] ? it.track_uri : it.uri, sizeof(r.arg));  // recenti: uri è il contesto
    nav_action(r, "Aggiunto alla coda");
  } else if (action == MENU_ALBUM) {
    nav_push(view_album(it.album_uri, nullptr));
  } else {
    BrowseItem a = {};
    a.type = IT_ARTIST;
    strlcpy(a.uri, it.artist_uri, sizeof(a.uri));
    strlcpy(a.title, it.artist_name, sizeof(a.title));
    nav_push(view_artist(a, false));
  }
}

void ui_track_menu_open(lv_obj_t *parent, const BrowseItem &it, int y, std::function<void()> on_close)
{
  ui_track_menu_close();
  menu_item = it;
  menu_closed = on_close;
  // Strato che chiude il menu toccando fuori, senza oscurare lo sfondo
  menu_obj = lv_obj_create(parent);
  lv_obj_remove_style_all(menu_obj);
  lv_obj_set_size(menu_obj, 712, 480);
  lv_obj_clear_flag(menu_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(menu_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(menu_obj, [](lv_event_t *e) { ui_track_menu_close(); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *p = ui_make_box(menu_obj, CX(400), y, 320, 235, COL_PRESSED, 12);
  lv_obj_set_style_border_width(p, 1, 0);
  lv_obj_set_style_border_color(p, COL_RAISED, 0);
  lv_obj_set_style_clip_corner(p, true, 0);
  lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);  // i tocchi sul pannello non chiudono il menu
  lv_obj_t *t = ui_make_line_label(p, &font_it_sb20, COL_TEXT, 19, 9, 280);
  ui_set_text_raw(t, it.title);
  lv_obj_t *a = ui_make_line_label(p, &font_it_16, COL_TEXT_2, 19, 35, 280);
  ui_set_text_raw(a, it.artist_name);
  ui_make_box(p, 0, 63, 318, 1, COL_RAISED);

  static const char *const ICONS[] = { IC_PLUS, IC_DISC, IC_USER };
  static const char *const TEXTS[] = { "Aggiungi alla coda", "Vai all'album", "Vai all'artista" };
  for (int i = 0; i < 3; i++) {
    bool enabled = i == MENU_QUEUE || (i == MENU_ALBUM ? it.album_uri[0] : it.artist_uri[0]);
    lv_obj_t *row = ui_make_touch(p, 0, 64 + i * 56, 318, 56, COL_RAISED, enabled);
    lv_obj_t *ic = ui_make_label(row, &font_ic_20, enabled ? COL_TEXT_2 : COL_DISABLED_FG, ICONS[i]);
    lv_obj_set_pos(ic, 19, 17);
    lv_obj_t *tx = ui_make_label(row, &font_it_20, enabled ? COL_TEXT : COL_DISABLED_FG, TEXTS[i]);
    lv_obj_set_pos(tx, 55, 15);
    if (enabled) lv_obj_add_event_cb(row, menu_event, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    else lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
  }
}

void ui_track_menu_close()
{
  if (!menu_obj) return;
  lv_obj_del_async(menu_obj);
  menu_obj = nullptr;
  std::function<void()> cb = menu_closed;
  menu_closed = nullptr;
  if (cb) cb();
}

void ui_track_menu_forget()
{
  menu_obj = nullptr;
  menu_closed = nullptr;
}

bool ui_track_menu_is_open() { return menu_obj != nullptr; }

void PagedList::open_menu(int index)
{
  if (!wd) return;
  int slot = index - page * rows;
  int row_top = y + slot * 80;
  int my = slot < 2 ? row_top + 80 + 8 : row_top - 8 - 235;
  ui_track_menu_open(wd->parent, store.items[index], my, [this]() {
    menu_index = -1;
    refresh();
  });
  menu_index = index;
  refresh();
}

void PagedList::close_menu()
{
  if (menu_index >= 0) ui_track_menu_close();
}
