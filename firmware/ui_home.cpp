// Fase 7: Home personale (design/fase7/SPEC_FASE7.md). Una pagina con due vetrine (ascoltati di recente
// raggruppati per contesto, artisti più ascoltati a 4 settimane), l'elenco completo dei recenti e
// "Più ascoltati" con schede Artisti/Brani e selettore del periodo.

#include <Arduino.h>
#include <time.h>
#include "ui_common.h"
#include "ui_list.h"
#include "ui_nav.h"
#include "ui_views.h"

static const uint32_t HOME_CACHE_MS = 5 * 60 * 1000;   // tornando alla Home entro 5 minuti tutto è già pronto
static const uint32_t RECENT_REFRESH_MS = 60 * 1000;   // poi i recenti si rinfrescano in silenzio
static const char *const RANGES[] = { "short_term", "medium_term", "long_term" };
// ArduinoJson cerca ogni testo in un elenco che cresce con la risposta: 50 ascolti si decodificano in
// ~0,9 s, 20 in molto meno. La Home ne chiede 20, "Tutti" i 50 (SPEC 7 §9.1); le classifiche 20 alla volta.
static const int HOME_RECENT_LIMIT = 20, ALL_RECENT_LIMIT = 50, TOP_LIMIT = 20;

// Dati della Home: restano fra un'apertura e l'altra
static ItemStore home_recent;   // gruppi dei recenti
static bool home_recent_full = false;  // home_recent viene dai 50 ascolti di "Tutti" (non dai 20 della Home)
static ItemStore home_artists;  // artisti più ascoltati, 4 settimane

static uint32_t browse(BrowseKind kind, const char *arg, int offset, int limit)
{
  BrowseRequest r = {};
  r.kind = kind;
  strlcpy(r.arg, arg ? arg : "", sizeof(r.arg));
  r.offset = offset;
  r.limit = limit;
  return spotify_browse(r);
}

static bool fresh(const ItemStore &s, uint32_t max_ms) { return s.loaded_ms && millis() - s.loaded_ms < max_ms; }

static void store_set(ItemStore &s, const BrowseItem *items, int n, int total)
{
  s.clear();
  s.append(items, n);
  s.total = total;
  s.loaded_ms = millis();
}

static void top_key(char *out, size_t n, int tab, int range) { snprintf(out, n, "top:%d:%d", tab, range); }

// ---------- Recenti: nome, riproduzione, tocchi ----------

// Le playlist arrivano senza nome: lo prendo dall'elenco delle playlist dell'utente, se è in memoria.
// Restituisce quante delle prime `first` voci restano senza nome.
static int resolve_names(BrowseItem *items, int n, int first)
{
  const ItemStore *pl = list_cache_peek("playlists");
  int missing = 0;
  for (int i = 0; i < n; i++) {
    BrowseItem &it = items[i];
    if (!(it.flags & IF_GUESS)) continue;
    const BrowseItem *found = nullptr;
    for (int k = 0; pl && k < pl->count && !found; k++)
      if (strcmp(pl->items[k].uri, it.uri) == 0) found = &pl->items[k];
    if (!found) {
      if (i < first) missing++;
      continue;
    }
    strlcpy(it.title, found->title, sizeof(it.title));
    strlcpy(it.subtitle, "Playlist", sizeof(it.subtitle));
    if (found->img[0]) {
      strlcpy(it.img, found->img, sizeof(it.img));
      it.img_scale = found->img_scale;
      strlcpy(it.img_big, found->img_big, sizeof(it.img_big));
      it.img_big_scale = found->img_big_scale;
    }
    it.flags = (it.flags & ~(IF_GUESS | IF_TITLE_FALLBACK)) | (found->flags & (IF_OWNED | IF_COLLABORATIVE | IF_TITLE_FALLBACK));
  }
  return missing;
}

static bool is_track_uri(const char *uri) { return strncmp(uri, "spotify:track:", 14) == 0; }

static bool recent_is_playing(const BrowseItem &it, const PlayerState &p)
{
  if (!p.has_playback) return false;
  if (it.flags & IF_LIKED) return strstr(p.context_uri, ":collection") != nullptr;
  if (is_track_uri(it.uri)) return !p.context_uri[0] && strcmp(p.track_uri, it.uri) == 0;
  return strcmp(p.context_uri, it.uri) == 0;
}

// Album del brano a partire dal brano stesso. Nel 2026 Spotify risponde 204 a {"uris":[...]} ma
// poi non suona niente (provato dalla scheda, 3 novembre): i brani senza contesto partono così.
static void play_in_album(const BrowseItem &it, const char *track_uri)
{
  char body[200];
  if (it.album_uri[0]) {
    snprintf(body, sizeof(body), "{\"context_uri\":\"%s\",\"offset\":{\"uri\":\"%s\"}}", it.album_uri, track_uri);
  } else {
    snprintf(body, sizeof(body), "{\"uris\":[\"%s\"]}", track_uri);
  }
  nav_play(body, it.title);
}

// Riprende il contesto dal brano ascoltato per ultimo (SPEC 7 §3.1). Se Spotify rifiuta l'offset
// (brano tolto dalla playlist) si riprova senza; i Brani piaciuti ripiegano sull'album del brano.
static void recent_play(const BrowseItem &it)
{
  char ctx[96] = "";
  if (it.flags & IF_LIKED) {
    SpotifyStatus st;
    spotify_get_status(&st);
    snprintf(ctx, sizeof(ctx), "spotify:user:%s:collection", st.user_id);
  } else if (!is_track_uri(it.uri)) {
    strlcpy(ctx, it.uri, sizeof(ctx));
  }
  if (!ctx[0]) {
    play_in_album(it, it.track_uri);
    return;
  }
  char body[256], fallback[256];
  snprintf(body, sizeof(body), "{\"context_uri\":\"%s\",\"offset\":{\"uri\":\"%s\"}}", ctx, it.track_uri);
  if ((it.flags & IF_LIKED) && it.album_uri[0])
    snprintf(fallback, sizeof(fallback), "{\"context_uri\":\"%s\",\"offset\":{\"uri\":\"%s\"}}", it.album_uri, it.track_uri);
  else snprintf(fallback, sizeof(fallback), "{\"context_uri\":\"%s\"}", ctx);
  nav_play(body, it.title, fallback);
}

// Pressione lunga: dettaglio del contesto; per i brani senza contesto noto, il menu del brano
static void recent_long(const BrowseItem &it, std::function<void()> track_menu)
{
  if (it.flags & IF_LIKED) {
    nav_push(view_liked());
  } else if (it.type == IT_ALBUM) {
    nav_push(view_album(it.uri, nullptr));
  } else if (it.type == IT_ARTIST) {
    BrowseItem a = {};
    a.type = IT_ARTIST;
    strlcpy(a.uri, it.uri, sizeof(a.uri));
    strlcpy(a.title, it.title, sizeof(a.title));
    nav_push(view_artist(a, false));
  } else if (it.type == IT_PLAYLIST && !(it.flags & IF_GUESS)) {
    const ItemStore *pl = list_cache_peek("playlists");
    for (int k = 0; pl && k < pl->count; k++) {
      if (strcmp(pl->items[k].uri, it.uri) == 0) {
        nav_push(view_playlist(pl->items[k]));
        return;
      }
    }
    BrowseItem p = it;
    p.type = IT_PLAYLIST;
    nav_push(view_playlist(p));
  } else {
    track_menu();
  }
}

// "adesso", "12 min", "5 h", "ieri", "2 nov" (SPEC 7 §3.3)
static void time_ago(uint32_t t, char *out, size_t n)
{
  time_t now = time(nullptr);
  out[0] = '\0';
  if (!t || now < 1700000000) return;  // ora non ancora sincronizzata
  long d = max<long>(0, (long)(now - t));
  if (d < 60) {
    strlcpy(out, "adesso", n);
    return;
  }
  if (d < 3600) {
    snprintf(out, n, "%ld min", d / 60);
    return;
  }
  if (d < 86400) {
    snprintf(out, n, "%ld h", d / 3600);
    return;
  }
  time_t tt = t, yesterday = now - 86400;
  struct tm a, y;
  localtime_r(&tt, &a);
  localtime_r(&yesterday, &y);
  if (a.tm_year == y.tm_year && a.tm_yday == y.tm_yday) {
    strlcpy(out, "ieri", n);
    return;
  }
  static const char *const MONTHS[] = { "gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic" };
  snprintf(out, n, "%d %s", a.tm_mday, MONTHS[a.tm_mon]);
}

// ---------- Home (3.1, 3.2) ----------

// "Tutti ›" (4.1): area 96x56 allineata a destra a x 776
static lv_obj_t *make_all_link(lv_obj_t *root, int y, lv_event_cb_t cb)
{
  lv_obj_t *t = ui_make_touch(root, CX(680), y, 96, 56, COL_PRESSED);
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_text_color(t, COL_TEXT_2, 0);
  lv_obj_set_style_text_color(t, COL_TEXT, LV_STATE_PRESSED);
  lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(t, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(t, 4, 0);
  lv_obj_t *l = lv_label_create(t);
  lv_obj_set_style_text_font(l, &font_it_16, 0);
  lv_label_set_text(l, "Tutti");
  lv_obj_t *c = lv_label_create(t);
  lv_obj_set_style_text_font(c, &font_ic_20, 0);
  lv_label_set_text(c, IC_RIGHT);
  lv_obj_add_event_cb(t, cb, LV_EVENT_CLICKED, nullptr);
  return t;
}

class HomeView : public View {
  static const int TILES = 4, CELLS = 5;
  struct Tile {
    lv_obj_t *touch, *title, *sub, *busy_row;
    ImageBox img;
  };
  struct Cell {
    lv_obj_t *touch, *name;
    ImageBox img;
  };
  lv_obj_t *root = nullptr;
  lv_obj_t *title1, *title2, *range2, *all1, *all2, *spin1, *spin2;
  lv_obj_t *skel1, *skel2, *empty1, *empty2, *err2, *block = nullptr;
  Tile tiles[TILES];
  Cell cells[CELLS];

  uint32_t req_recent = 0, req_playlists = 0, req_artists = 0;
  bool silent = false;            // recenti rinfrescati senza spinner (dati vecchi già sullo schermo)
  bool recent_error = false, artists_error = false;
  ItemStore incoming;             // recenti arrivati che aspettano i nomi delle playlist (R2)
  int busy_tile = -1;
  uint32_t shown_player_version = UINT32_MAX;
  uint32_t build_ms = 0;          // registro dei tempi (obiettivo della fase: tutto entro 3 s)
  bool logged_recent = false, logged_artists = false, logged_images = false;
  PlayerState player;

public:
  Section section() const override { return SEC_HOME; }

  void build(lv_obj_t *r) override
  {
    root = r;
    block = nullptr;
    title1 = ui_make_label(root, &font_it_sb20, COL_TEXT, "Ascoltati di recente");
    lv_obj_set_pos(title1, CX(112), 15);
    all1 = make_all_link(root, 0, [](lv_event_t *e) { nav_push(view_recent()); });
    spin1 = ui_make_spinner(root, 28);
    lv_obj_set_pos(spin1, CX(740), 14);

    title2 = ui_make_label(root, &font_it_sb20, COL_TEXT, "Artisti pi\xC3\xB9 ascoltati");
    lv_obj_set_pos(title2, CX(112), 297);
    range2 = ui_make_label(root, &font_it_16, COL_TEXT_3, "\xC2\xB7 4 settimane");
    lv_obj_update_layout(title2);
    lv_obj_set_pos(range2, CX(112) + lv_obj_get_width(title2) + 8, 301);
    all2 = make_all_link(root, 282, [](lv_event_t *e) { nav_push(view_top()); });
    spin2 = ui_make_spinner(root, 28);
    lv_obj_set_pos(spin2, CX(740), 296);

    // Scheletro statico (01b)
    static const int TX[] = { 112, 283, 454, 626 };
    static const int BAR1[] = { 110, 124, 96, 116 }, BAR2[] = { 72, 96, 110, 84 };
    skel1 = make_layer();
    for (int i = 0; i < TILES; i++) {
      ui_make_box(skel1, CX(TX[i]), 56, 150, 150, COL_SURFACE, 8);
      ui_make_box(skel1, CX(TX[i]), 218, BAR1[i], 16, COL_SURFACE, 8);
      ui_make_box(skel1, CX(TX[i]), 244, BAR2[i], 12, COL_SKELETON_2, 8);
    }
    static const int AX[] = { 112, 245, 378, 511, 644 };
    skel2 = make_layer();
    for (int i = 0; i < CELLS; i++) {
      ui_make_box(skel2, CX(AX[i]) + 26, 338, 80, 80, COL_SURFACE, LV_RADIUS_CIRCLE);
      ui_make_box(skel2, CX(AX[i]) + 26, 434, 80, 14, COL_SURFACE, 7);
    }

    // Riquadri (4.2): il fondo premuto è 8 px più grande per lato
    for (int i = 0; i < TILES; i++) {
      Tile &t = tiles[i];
      t.touch = ui_make_touch(root, CX(TX[i]) - 8, 48, 166, 218, COL_PRESSED);
      lv_obj_set_style_radius(t.touch, 8, 0);
      lv_obj_set_style_bg_color(t.touch, COL_PRESSED, LV_STATE_CHECKED);
      lv_obj_set_style_bg_opa(t.touch, LV_OPA_COVER, LV_STATE_CHECKED);
      t.img.create(t.touch, 8, 8, 150, false, IC_NOTE);
      t.title = ui_make_line_label(t.touch, &font_it_16, COL_TEXT, 8, 166, 150);
      t.sub = ui_make_line_label(t.touch, &font_it_16, COL_TEXT_3, 8, 188, 150);
      // "Avvio…" con lo spinner al posto del sottotitolo
      t.busy_row = lv_obj_create(t.touch);
      lv_obj_remove_style_all(t.busy_row);
      lv_obj_set_pos(t.busy_row, 8, 186);
      lv_obj_set_size(t.busy_row, 150, 24);
      lv_obj_clear_flag(t.busy_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_flex_flow(t.busy_row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(t.busy_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(t.busy_row, 8, 0);
      ui_make_spinner(t.busy_row, 20);
      ui_make_label(t.busy_row, &font_it_16, COL_TEXT_3, "Avvio\xE2\x80\xA6");
      lv_obj_add_flag(t.busy_row, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_event_cb(t.touch, tile_event, LV_EVENT_SHORT_CLICKED, this);
      lv_obj_add_event_cb(t.touch, tile_event, LV_EVENT_LONG_PRESSED, this);
      lv_obj_set_user_data(t.touch, (void *)(intptr_t)i);
      lv_obj_add_flag(t.touch, LV_OBJ_FLAG_HIDDEN);
    }
    // Celle artista (4.3)
    for (int i = 0; i < CELLS; i++) {
      Cell &c = cells[i];
      c.touch = ui_make_touch(root, CX(AX[i]), 334, 132, 124, COL_PRESSED);
      lv_obj_set_style_radius(c.touch, 8, 0);
      c.img.create(c.touch, 26, 4, 80, true, IC_USER);
      lv_obj_set_style_text_font(c.img.placeholder, &font_ic_40, 0);
      c.name = ui_make_line_label(c.touch, &font_it_16, COL_TEXT, 4, 92, 124);
      lv_obj_set_style_text_align(c.name, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_add_event_cb(c.touch, cell_event, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(c.touch, (void *)(intptr_t)i);
      lv_obj_add_flag(c.touch, LV_OBJ_FLAG_HIDDEN);
    }

    empty1 = ui_make_label(root, &font_it_16, COL_TEXT_3, "Qui compariranno le playlist e gli album che ascolti.");
    lv_obj_set_pos(empty1, CX(112), 120);
    empty2 = ui_make_label(root, &font_it_16, COL_TEXT_3, "Spotify calcola i tuoi artisti dopo qualche giorno di ascolti.");
    lv_obj_set_pos(empty2, CX(112), 350);
    err2 = make_layer();
    lv_obj_t *et = ui_make_label(err2, &font_it_16, COL_TEXT_2, "Non riesco a caricare gli artisti.");
    lv_obj_set_pos(et, CX(112), 350);
    ui_make_button(err2, BTN_SECONDARY, CX(112), 384, 160, 48, nullptr, "Riprova",
                   [](lv_event_t *e) { ((HomeView *)lv_event_get_user_data(e))->start_artists(); }, this);

    // Dati in memoria: compaiono subito; i recenti più vecchi di un minuto si rinfrescano in silenzio
    if (!fresh(home_recent, HOME_CACHE_MS)) {
      if (!req_recent) start_recent(false);
    } else {
      if (!req_recent && millis() - home_recent.loaded_ms > RECENT_REFRESH_MS) start_recent(true);
      if (!fresh(home_artists, HOME_CACHE_MS) && !req_artists) start_artists();
    }
    shown_player_version = UINT32_MAX;
    build_ms = millis();
    logged_recent = logged_artists = logged_images = false;
    render();
  }

  void destroyed() override
  {
    root = block = nullptr;
    for (auto &t : tiles) t = Tile();
    for (auto &c : cells) c = Cell();
  }

  void tick() override
  {
    BrowseResult r;
    if (req_recent && spotify_take_result(req_recent, &r)) {
      req_recent = 0;
      on_recent(r);
      free_result(r);
    }
    if (req_playlists && spotify_take_result(req_playlists, &r)) {
      req_playlists = 0;
      if (r.code >= 200 && r.code < 300) {
        ItemStore pl;
        store_set(pl, r.items, r.count, r.total);
        list_cache_put("playlists", pl);  // la usa anche la schermata Playlist
      }
      free_result(r);
      commit_incoming();  // senza nomi le playlist mostrano il brano
    }
    if (req_artists && spotify_take_result(req_artists, &r)) {
      req_artists = 0;
      artists_error = r.code < 200 || r.code >= 300;
      if (!artists_error) {
        store_set(home_artists, r.items, r.count, r.total);
        ItemStore copy;  // "Più ascoltati · Artisti · 4 settimane" la trova già pronta
        store_set(copy, r.items, r.count, r.total);
        char key[16];
        top_key(key, sizeof(key), 0, 0);
        list_cache_put(key, copy);
      }
      free_result(r);
      render();
    }
    if (!root) return;
    if (busy_tile >= 0 && !nav_play_busy()) {
      busy_tile = -1;
      render();
    }
    spotify_get_player(&player);
    if (player.version != shown_player_version) {
      shown_player_version = player.version;
      render_playing();
    }
    for (auto &t : tiles)
      if (!lv_obj_has_flag(t.touch, LV_OBJ_FLAG_HIDDEN)) t.img.tick();
    for (auto &c : cells)
      if (!lv_obj_has_flag(c.touch, LV_OBJ_FLAG_HIDDEN)) c.img.tick();
    log_times();
  }

private:
  void log_times()
  {
    bool recent = !lv_obj_has_flag(tiles[0].touch, LV_OBJ_FLAG_HIDDEN);
    bool artists = !lv_obj_has_flag(cells[0].touch, LV_OBJ_FLAG_HIDDEN);
    if (recent && !logged_recent) {
      logged_recent = true;
      Serial.printf("Home: recenti sullo schermo in %lu ms\n", (unsigned long)(millis() - build_ms));
    }
    if (artists && !logged_artists) {
      logged_artists = true;
      Serial.printf("Home: artisti sullo schermo in %lu ms\n", (unsigned long)(millis() - build_ms));
    }
    if (recent && artists && !logged_images) {
      bool all = true;
      for (auto &t : tiles)
        if (!lv_obj_has_flag(t.touch, LV_OBJ_FLAG_HIDDEN) && t.img.url[0] && !t.img.shown) all = false;
      for (auto &c : cells)
        if (!lv_obj_has_flag(c.touch, LV_OBJ_FLAG_HIDDEN) && c.img.url[0] && !c.img.shown) all = false;
      if (all) {
        logged_images = true;
        Serial.printf("Home: tutte le immagini in %lu ms\n", (unsigned long)(millis() - build_ms));
      }
    }
  }

  lv_obj_t *make_layer()
  {
    lv_obj_t *l = lv_obj_create(root);
    lv_obj_remove_style_all(l);
    lv_obj_set_size(l, 712, 480);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
  }

  static void free_result(BrowseResult &r)
  {
    spotify_free(r.items);
    spotify_free(r.meta);
  }

  void start_recent(bool quiet)
  {
    silent = quiet;
    recent_error = false;
    req_recent = browse(BR_RECENT, "", 0, HOME_RECENT_LIMIT);
    render();
  }

  void start_artists()
  {
    artists_error = false;
    req_artists = browse(BR_TOP_ARTISTS, RANGES[0], 0, TOP_LIMIT);
    render();
  }

  void on_recent(BrowseResult &r)
  {
    bool ok = r.code >= 200 && r.code < 300;
    if (!ok) {
      if (!silent) recent_error = true;
    } else {
      store_set(incoming, r.items, r.count, r.total);
      // Nomi delle playlist: se ne manca qualcuno fra i primi 4 e l'elenco non è in memoria, lo chiedo
      // prima di mostrare i testi, così i titoli non cambiano sotto gli occhi (SPEC 7 §5, R2)
      if (resolve_names(incoming.items, incoming.count, TILES) > 0 && !list_cache_peek("playlists")) {
        req_playlists = browse(BR_PLAYLISTS, "", 0, 50);
        if (req_playlists) return;
      }
      commit_incoming();
    }
    if (!fresh(home_artists, HOME_CACHE_MS) && !req_artists) start_artists();
    render();
  }

  void commit_incoming()
  {
    if (!incoming.loaded_ms) return;
    resolve_names(incoming.items, incoming.count, TILES);
    store_set(home_recent, incoming.items, incoming.count, incoming.total);
    home_recent_full = false;
    incoming.clear();
    if (!fresh(home_artists, HOME_CACHE_MS) && !req_artists) start_artists();
    render();
  }

  void render()
  {
    if (!root) return;
    bool have_recent = home_recent.loaded_ms != 0;
    bool have_artists = home_artists.loaded_ms != 0;
    bool loading_recent = (req_recent || req_playlists) && !silent;

    // Errore di tutta la Home: recenti falliti e niente in memoria (01d)
    bool page_error = recent_error && !have_recent;
    // Home vuota: recenti e artisti caricati ed entrambi vuoti (01e)
    bool page_empty = have_recent && have_artists && home_recent.count == 0 && home_artists.count == 0;
    if (block) {
      lv_obj_del(block);
      block = nullptr;
    }
    if (page_error) {
      block = ui_state_block(root, 0, 712, IC_WARN, COL_WARN, "Non riesco a caricare la Home", "Controlla il Wi-Fi e riprova.",
                             "Riprova", [](lv_event_t *e) {
                               ((HomeView *)lv_event_get_user_data(e))->start_recent(false);
                             }, this);
    } else if (page_empty) {
      block = make_empty_block();
    }
    bool page = !page_error && !page_empty;
    for (lv_obj_t *o : { title1, title2, range2 }) ui_set_hidden(o, !page);

    // Vetrina 1
    bool show_recent = page && have_recent;
    bool skeleton1 = page && !have_recent;
    ui_set_hidden(skel1, !skeleton1);
    ui_set_hidden(spin1, !(page && loading_recent));
    ui_set_hidden(all1, !(show_recent && !loading_recent && home_recent.count > 0));
    ui_set_hidden(empty1, !(show_recent && home_recent.count == 0));
    for (int i = 0; i < TILES; i++) {
      Tile &t = tiles[i];
      bool vis = show_recent && i < home_recent.count;
      ui_set_hidden(t.touch, !vis);
      if (!vis) continue;
      const BrowseItem &it = home_recent.items[i];
      ui_set_text_raw(t.title, it.title);
      ui_set_text_raw(t.sub, it.subtitle);
      if (it.flags & IF_LIKED) t.img.set("", 0, IC_HEART);
      else t.img.set(it.img_big, it.img_big_scale, it.type == IT_ALBUM ? IC_DISC : (it.type == IT_ARTIST ? IC_USER : IC_NOTE));
      ui_set_text_color(t.img.placeholder, (it.flags & IF_LIKED) ? COL_ACCENT : COL_PLACEHOLDER_FG);
      bool busy = i == busy_tile;
      ui_set_hidden(t.sub, busy);
      ui_set_hidden(t.busy_row, !busy);
      if (lv_obj_has_state(t.touch, LV_STATE_CHECKED) != busy) {
        if (busy) lv_obj_add_state(t.touch, LV_STATE_CHECKED);
        else lv_obj_clear_state(t.touch, LV_STATE_CHECKED);
      }
    }

    // Vetrina 2
    bool show_artists = page && have_artists;
    bool err = page && artists_error && !have_artists;
    ui_set_hidden(skel2, !(page && !have_artists && !err));
    ui_set_hidden(spin2, !(page && req_artists));
    ui_set_hidden(all2, !(show_artists && !req_artists && home_artists.count > 0));
    ui_set_hidden(empty2, !(show_artists && home_artists.count == 0));
    ui_set_hidden(err2, !err);
    for (int i = 0; i < CELLS; i++) {
      Cell &c = cells[i];
      bool vis = show_artists && i < home_artists.count;
      ui_set_hidden(c.touch, !vis);
      if (!vis) continue;
      const BrowseItem &it = home_artists.items[i];
      ui_set_text_raw(c.name, it.title);
      c.img.set(it.img, it.img_scale, IC_USER);  // 160/2 = 80 (SPEC 7 §4.3)
    }
    render_playing();
  }

  // Titolo verde per il contesto in riproduzione
  void render_playing()
  {
    if (!root) return;
    for (int i = 0; i < TILES && i < home_recent.count; i++) {
      const BrowseItem &it = home_recent.items[i];
      lv_color_t c = recent_is_playing(it, player) ? COL_ACCENT : ((it.flags & IF_TITLE_FALLBACK) ? COL_TEXT_3 : COL_TEXT);
      ui_set_text_color(tiles[i].title, c);
    }
  }

  // Home vuota (01e): cerchio con la casa, titolo, testo e due pulsanti
  lv_obj_t *make_empty_block()
  {
    lv_obj_t *c = make_layer();
    lv_obj_clear_flag(c, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *circle = ui_make_box(c, (712 - 80) / 2, 112, 80, 80, COL_SURFACE, LV_RADIUS_CIRCLE);
    lv_obj_t *ic = ui_make_label(circle, &font_ic_40, COL_TEXT_2, IC_HOME);
    lv_obj_center(ic);
    lv_obj_t *t = ui_make_label(c, &font_it_sb20, COL_TEXT, "La tua Home \xC3\xA8 ancora vuota");
    lv_obj_set_width(t, 712);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(t, 0, 216);
    lv_obj_t *d = ui_make_label(c, &font_it_16, COL_TEXT_2,
                                "Ascolta qualcosa: qui compariranno i brani recenti e i tuoi artisti preferiti.");
    lv_obj_set_width(d, 664);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(d, 24, 250);
    ui_make_button(c, BTN_PRIMARY, CX(238), 296, 220, 56, IC_LIBRARY, "Apri la libreria",
                   [](lv_event_t *e) { nav_open_section(SEC_LIBRARY); });
    ui_make_button(c, BTN_SECONDARY, CX(474), 296, 176, 56, IC_SEARCH, "Cerca",
                   [](lv_event_t *e) { nav_open_section(SEC_SEARCH); });
    return c;
  }

  static void tile_event(lv_event_t *e)
  {
    HomeView *v = (HomeView *)lv_event_get_user_data(e);
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i >= v->home_count()) return;
    BrowseItem it = home_recent.items[i];
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
      lv_obj_t *root = v->root;
      recent_long(it, [root, it]() { ui_track_menu_open(root, it, 240, nullptr); });
      return;
    }
    if (ui_click_is_gesture() || nav_play_busy()) return;
    v->busy_tile = i;
    v->render();
    recent_play(it);
  }

  static void cell_event(lv_event_t *e)
  {
    if (ui_click_is_gesture()) return;
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i >= home_artists.count) return;
    BrowseItem a = home_artists.items[i];
    strlcpy(a.subtitle, "Artista", sizeof(a.subtitle));
    nav_push(view_artist(a, true));
  }

  int home_count() const { return min(home_recent.count, TILES); }
};

View *view_home() { return new HomeView(); }

// ---------- Ascoltati di recente, tutti (3.3) ----------

class RecentView : public View {
  Header header;
  PagedList list;
  PlayerState player;

public:
  RecentView()
  {
    list.noun_plural = "elementi";
    list.what = "gli ascolti recenti";
    list.empty_icon = IC_NOTE;
    list.empty_title = "Niente ascolti recenti";
    list.empty_text = "Qui compariranno le playlist e gli album che ascolti.";
    list.on_long = [this](int i) {
      PagedList *l = &list;
      recent_long(list.store.items[i], [l, i]() { l->open_menu(i); });
    };
    // Se i 50 ascolti sono già in memoria non si chiede niente (SPEC 7 §3.3)
    if (home_recent.loaded_ms && home_recent_full) {
      list.store.append(home_recent.items, home_recent.count);
      list.store.total = home_recent.count;
      list.store.loaded_ms = home_recent.loaded_ms;
    }
    list.fetch = [](int offset, const char *) -> uint32_t { return offset ? 0 : browse(BR_RECENT, "", 0, ALL_RECENT_LIMIT); };
    list.on_result = [](const BrowseResult &r) {
      if (r.code < 200 || r.code >= 300) return;
      resolve_names(r.items, r.count, r.count);
      store_set(home_recent, r.items, r.count, r.total);
      home_recent_full = true;
    };
    list.right_text = [](const BrowseItem &it, char *out, size_t n) { time_ago(it.played_at, out, n); };
    list.is_playing = [this](const BrowseItem &it) { return recent_is_playing(it, player); };
    list.on_click = [this](int i) {
      if (nav_play_busy()) return;
      list.busy_index = i;
      list.refresh();
      recent_play(list.store.items[i]);
    };
  }
  Section section() const override { return SEC_HOME; }
  void build(lv_obj_t *root) override
  {
    spotify_get_player(&player);
    header = Header();
    header.create(root, true, "Ascoltati di recente");
    list.build(root);
    list.start();
    tick();
  }
  void destroyed() override
  {
    header = Header();
    list.destroyed();
  }
  void tick() override
  {
    spotify_get_player(&player);
    list.tick();
    header.set_loading(list.loading());
    if (list.busy_index >= 0 && !nav_play_busy()) {
      list.busy_index = -1;
      list.refresh();
    }
    if (list.store.loaded_ms) ui_set_text(header.info, "%d %s", list.store.count, list.store.count == 1 ? "elemento" : "elementi");
  }
  bool back() override
  {
    if (!list.menu_open()) return false;
    list.close_menu();
    return true;
  }
  void gesture(lv_dir_t d) override { list.gesture(d); }
};

View *view_recent() { return new RecentView(); }

// ---------- Più ascoltati (3.4) ----------

static int top_range = 0;  // periodo scelto: resta finché non si spegne la scheda

class TopView : public View {
  PagedList lists[2][3];  // [Artisti, Brani] x [4 settimane, 6 mesi, 1 anno]
  int tab = 0;
  int built_tab = -1, built_range = -1;  // tab e periodo cambiano prima della ricostruzione
  lv_obj_t *spinner = nullptr;

public:
  TopView()
  {
    for (int t = 0; t < 2; t++) {
      for (int g = 0; g < 3; g++) {
        PagedList &l = lists[t][g];
        l.style = ROW_RANK;
        l.round_images = t == 0;
        l.track_menu = t == 1;
        l.noun_plural = t == 0 ? "artisti" : "brani";
        l.what = t == 0 ? "gli artisti" : "i brani";
        l.empty_icon = t == 0 ? IC_USER : IC_NOTE;
        l.empty_title = "Ancora pochi ascolti in questo periodo";
        l.empty_text = "Spotify aggiorna la classifica una volta al giorno. Prova un periodo pi\xC3\xB9 lungo.";
        top_key(l.cache_key, sizeof(l.cache_key), t, g);
        BrowseKind kind = t == 0 ? BR_TOP_ARTISTS : BR_TOP_TRACKS;
        const char *range = RANGES[g];
        l.fetch = [kind, range](int offset, const char *) { return browse(kind, range, offset, TOP_LIMIT); };
        if (t == 0) {
          l.on_click = [&l](int i) {
            BrowseItem a = l.store.items[i];
            strlcpy(a.subtitle, "Artista", sizeof(a.subtitle));
            nav_push(view_artist(a, true));
          };
        } else {
          l.on_click = [&l](int i) { play_from(l, i); };
        }
      }
    }
  }
  Section section() const override { return SEC_HOME; }
  void build(lv_obj_t *root) override
  {
    ui_make_back_button(root);
    // Schede Artisti / Brani (SPEC 6 §4.10)
    static const char *const TABS[] = { "Artisti", "Brani" };
    for (int i = 0; i < 2; i++) {
      lv_obj_t *t = ui_make_touch(root, CX(152 + i * 104), 0, 104, 64, COL_BG, false);
      lv_obj_t *l = ui_make_label(t, &font_it_sb20, i == tab ? COL_TEXT : COL_TEXT_3, TABS[i]);
      lv_obj_set_style_text_color(l, COL_TEXT, LV_STATE_PRESSED);
      lv_obj_center(l);
      lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
      if (i == tab) ui_make_box(t, 24, 55, 56, 3, COL_ACCENT);
      lv_obj_add_event_cb(t, tab_cb, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(t, (void *)(intptr_t)i);
    }
    // Selettore del periodo (4.4)
    lv_obj_t *sel = ui_make_box(root, CX(476), 8, 300, 48, COL_SURFACE, 24);
    static const char *const LABELS[] = { "4 settimane", "6 mesi", "1 anno" };
    static const int SW[] = { 124, 84, 84 };
    int sx = 4;
    for (int g = 0; g < 3; g++) {
      bool selected = g == top_range;
      lv_obj_t *seg = ui_make_box(sel, sx, 4, SW[g], 40, selected ? COL_TEXT : COL_SURFACE, 20);
      lv_obj_add_flag(seg, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_ext_click_area(seg, 4);  // tutta l'altezza del selettore
      if (!selected) lv_obj_set_style_bg_color(seg, COL_RAISED, LV_STATE_PRESSED);
      lv_obj_t *l = ui_make_label(seg, &font_it_16, selected ? COL_ON_ACCENT : COL_TEXT_2, LABELS[g]);
      if (!selected) lv_obj_set_style_text_color(l, COL_TEXT, LV_STATE_PRESSED);
      lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
      lv_obj_center(l);
      lv_obj_add_event_cb(seg, range_cb, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(seg, (void *)(intptr_t)g);
      sx += SW[g];
    }
    spinner = ui_make_spinner(root, 28);
    lv_obj_set_pos(spinner, CX(754), 250);
    PagedList &l = lists[tab][top_range];
    l.build(root);
    built_tab = tab;
    built_range = top_range;
    l.start();
    tick();
  }
  void destroyed() override
  {
    spinner = nullptr;
    if (built_tab >= 0) lists[built_tab][built_range].destroyed();
    built_tab = built_range = -1;
  }
  void tick() override
  {
    if (built_tab != tab || built_range != top_range) return;
    PagedList &l = lists[tab][top_range];
    l.tick();
    if (l.busy_index >= 0 && !nav_play_busy()) {
      l.busy_index = -1;
      l.refresh();
    }
    ui_set_hidden(spinner, !(l.loading() && l.store.count == 0));
  }
  bool back() override
  {
    PagedList &l = lists[tab][top_range];
    if (!l.menu_open()) return false;
    l.close_menu();
    return true;
  }
  void gesture(lv_dir_t d) override { lists[tab][top_range].gesture(d); }

private:
  // La SPEC 7 §1.6 vuole la classifica dal brano toccato in poi ({"uris":[...]}), ma Spotify non suona
  // gli elenchi di brani: parte il brano dentro il suo album (vedi play_in_album)
  static void play_from(PagedList &l, int index)
  {
    if (nav_play_busy()) return;
    l.busy_index = index;
    l.refresh();
    play_in_album(l.store.items[index], l.store.items[index].uri);
  }
  static void tab_cb(lv_event_t *e)
  {
    TopView *v = (TopView *)lv_event_get_user_data(e);
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i == v->tab) return;
    v->tab = i;
    nav_rebuild();
  }
  static void range_cb(lv_event_t *e)
  {
    int g = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (g == top_range) return;
    top_range = g;
    nav_rebuild();
  }
};

View *view_top() { return new TopView(); }
