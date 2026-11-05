// Schermate della Libreria, Coda e Dispositivi (SPEC §5.3-5.9 e 5.11). La Ricerca è in ui_search.cpp.

#include "ui_views.h"

#include <Arduino.h>
#include "ui_common.h"
#include "ui_list.h"

int library_total_playlists = -1, library_total_liked = -1, library_total_albums = -1, library_total_artists = -1;

static const char *uri_id(const char *uri)
{
  const char *c = strrchr(uri, ':');
  return c ? c + 1 : uri;
}

static uint32_t browse(BrowseKind kind, const char *arg, int offset, int limit = 50, const char *arg2 = "")
{
  BrowseRequest r = {};
  r.kind = kind;
  strlcpy(r.arg, arg ? arg : "", sizeof(r.arg));
  strlcpy(r.arg2, arg2 ? arg2 : "", sizeof(r.arg2));
  r.offset = offset;
  r.limit = limit;
  return spotify_browse(r);
}

static void play_context(const char *context_uri, const char *name, int position = -1, const char *track_uri = nullptr,
                         const char *fallback = nullptr)
{
  char body[200];
  if (track_uri) snprintf(body, sizeof(body), "{\"context_uri\":\"%s\",\"offset\":{\"uri\":\"%s\"}}", context_uri, track_uri);
  else if (position >= 0) snprintf(body, sizeof(body), "{\"context_uri\":\"%s\",\"offset\":{\"position\":%d}}", context_uri, position);
  else snprintf(body, sizeof(body), "{\"context_uri\":\"%s\"}", context_uri);
  nav_play(body, name, fallback);
}

// ---------- Schermata con intestazione ed elenco ----------

class ListScreen : public View {
public:
  Section sec;
  const char *title;
  bool with_back;
  Header header;
  PagedList list;
  int *library_total = nullptr;

  ListScreen(Section s, const char *t, bool back) : sec(s), title(t), with_back(back) {}
  Section section() const override { return sec; }
  void build(lv_obj_t *root) override
  {
    header = Header();
    header.create(root, with_back, title);
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
    list.tick();
    header.set_loading(list.loading());
    if (list.busy_index >= 0 && !nav_play_busy()) {
      list.busy_index = -1;
      list.refresh();
    }
    if (library_total && list.store.total >= 0) *library_total = list.store.total;
    update_info();
  }
  bool back() override
  {
    if (!list.menu_open()) return false;
    list.close_menu();
    return true;
  }
  void gesture(lv_dir_t d) override { list.gesture(d); }
  virtual void update_info() {}
};

// ---------- Libreria (5.3) ----------

class LibraryView : public View {
  lv_obj_t *counts[4] = {};

public:
  Section section() const override { return SEC_LIBRARY; }
  void build(lv_obj_t *root) override
  {
    Header h;
    h.create(root, false, "Libreria");
    static const char *const ICONS[] = { IC_NOTE, IC_HEART, IC_DISC, IC_USER };
    static const char *const LABELS[] = { "Playlist", "Brani piaciuti", "Album", "Artisti seguiti" };
    static const int XS[] = { 112, 452, 112, 452 }, YS[] = { 80, 80, 280, 280 };
    for (int i = 0; i < 4; i++) {
      lv_obj_t *t = ui_make_touch(root, CX(XS[i]), YS[i], 324, 184, COL_SURFACE_PRESSED);
      lv_obj_set_style_radius(t, 12, 0);
      lv_obj_set_style_bg_color(t, COL_SURFACE, 0);
      lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
      lv_obj_t *ic = ui_make_label(t, &font_ic_28, i == 1 ? COL_ACCENT : COL_TEXT, ICONS[i]);
      lv_obj_set_pos(ic, 24, 24);
      lv_obj_t *l = ui_make_label(t, &font_it_sb24, COL_TEXT, LABELS[i]);
      lv_obj_set_pos(l, 24, 116);
      counts[i] = ui_make_label(t, &font_it_16, COL_TEXT_2, "");
      lv_obj_set_pos(counts[i], 24, 148);
      lv_obj_add_event_cb(t, tile_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    tick();
  }
  void destroyed() override { memset(counts, 0, sizeof(counts)); }
  void tick() override
  {
    // Conteggi dell'ultima apertura di ogni elenco: niente richieste solo per i numeri
    const int totals[] = { library_total_playlists, library_total_liked, library_total_albums, library_total_artists };
    static const char *const NOUNS[] = { "playlist", "brani", "album", "artisti" };
    for (int i = 0; i < 4; i++) {
      if (totals[i] < 0) ui_set_text_raw(counts[i], "");
      else ui_set_text(counts[i], "%d %s", totals[i], (i == 1 && totals[i] == 1) ? "brano" : NOUNS[i]);
    }
  }
  static void tile_cb(lv_event_t *e)
  {
    switch ((intptr_t)lv_event_get_user_data(e)) {
      case 0: nav_push(view_playlists()); break;
      case 1: nav_push(view_liked()); break;
      case 2: nav_push(view_albums()); break;
      default: nav_push(view_artists()); break;
    }
  }
};

View *view_library() { return new LibraryView(); }

// ---------- Playlist (5.4) ----------

class PlaylistsView : public ListScreen {
public:
  PlaylistsView() : ListScreen(SEC_LIBRARY, "Playlist", true)
  {
    strcpy(list.cache_key, "playlists");
    library_total = &library_total_playlists;
    list.noun_plural = "playlist";
    list.what = "le playlist";
    list.empty_title = "Nessuna playlist";
    list.empty_text = "Salvale dall'app Spotify: compariranno qui.";
    list.fetch = [](int offset, const char *) { return browse(BR_PLAYLISTS, "", offset); };
    list.on_click = [this](int i) { nav_push(view_playlist(list.store.items[i])); };
  }
  void update_info() override
  {
    if (list.store.total >= 0) ui_set_text(header.info, "%d playlist", list.store.total);
  }
};

View *view_playlists() { return new PlaylistsView(); }

// ---------- Dettaglio playlist (5.5) ----------

class PlaylistView : public View {
  BrowseItem pl;
  bool show_list;
  DetailPanel panel;
  PagedList list;
  lv_obj_t *info = nullptr, *spinner = nullptr;

public:
  explicit PlaylistView(const BrowseItem &p) : pl(p)
  {
    // L'elenco dei brani si vede solo per le playlist proprie o collaborative (regole 2026)
    show_list = (p.flags & (IF_OWNED | IF_COLLABORATIVE)) != 0;
    snprintf(list.cache_key, sizeof(list.cache_key), "pl:%s", uri_id(p.uri));
    list.style = ROW_NUMBERED;
    list.x = CX(320);
    list.w = 416;
    list.track_menu = true;
    list.noun_plural = "brani";
    list.what = "i brani";
    list.empty_icon = IC_NOTE;
    list.empty_title = "Questa playlist \xC3\xA8 vuota";
    list.empty_text = "Aggiungi i brani dall'app Spotify: compariranno qui.";
    list.fetch = [this](int offset, const char *) { return browse(BR_PLAYLIST_ITEMS, uri_id(pl.uri), offset); };
    list.on_click = [this](int i) {
      if (nav_play_busy() || !list.store.items[i].uri[0]) return;
      list.busy_index = i;
      list.refresh();
      play_context(pl.uri, pl.title, list.store.items[i].number - 1);
    };
  }
  Section section() const override { return SEC_LIBRARY; }
  void build(lv_obj_t *root) override
  {
    panel = DetailPanel();
    panel.create(root, "Playlist", false, IC_NOTE, play_cb, this);
    panel.set(pl, pl.subtitle);
    if (show_list) {
      info = ui_make_label(root, &font_it_16, COL_TEXT_3, "");
      lv_obj_set_pos(info, CX(344), 21);
      spinner = ui_make_spinner(root, 28);
      lv_obj_set_pos(spinner, CX(692), 18);
      list.build(root);
      list.start();
    } else {
      ui_state_block(root, CX(320), 480, IC_QUEUE, COL_TEXT_2, "Spotify non mostra i brani di questa playlist",
                     "\xC3\x88 una playlist di Spotify o di un altro utente. Puoi comunque avviarla con Riproduci.");
    }
    tick();
  }
  void destroyed() override
  {
    panel = DetailPanel();
    info = spinner = nullptr;
    if (show_list) list.destroyed();
  }
  void tick() override
  {
    panel.image.tick();
    if (!show_list) return;
    list.tick();
    ui_set_hidden(spinner, !list.loading());
    if (list.busy_index >= 0 && !nav_play_busy()) {
      list.busy_index = -1;
      list.refresh();
    }
    int total = list.store.total;
    ui_button_set_enabled(panel.play, total != 0);
    if (total > 0) {
      int first = list.page * list.rows + 1;
      ui_set_text(info, "Brani %d\xE2\x80\x93%d di %d", first, min(first + list.rows - 1, total), total);
    } else {
      ui_set_text_raw(info, "");
    }
  }
  bool back() override
  {
    if (!list.menu_open()) return false;
    list.close_menu();
    return true;
  }
  void gesture(lv_dir_t d) override
  {
    if (show_list) list.gesture(d);
  }
  static void play_cb(lv_event_t *e)
  {
    PlaylistView *v = (PlaylistView *)lv_event_get_user_data(e);
    play_context(v->pl.uri, v->pl.title);
  }
};

View *view_playlist(const BrowseItem &p) { return new PlaylistView(p); }

// ---------- Brani piaciuti (5.6) ----------

class LikedView : public ListScreen {
public:
  LikedView() : ListScreen(SEC_LIBRARY, "Brani piaciuti", true)
  {
    strcpy(list.cache_key, "liked");
    library_total = &library_total_liked;
    list.track_menu = true;
    list.noun_plural = "brani";
    list.what = "i brani";
    list.empty_icon = IC_HEART;
    list.empty_title = "Nessun brano piaciuto";
    list.empty_text = "Tocca il cuore su un brano nell'app Spotify e lo ritroverai qui.";
    // 20 per richiesta: con 50 Spotify impiega ~6,5 s a rispondere, con 20 molto meno
    list.fetch = [](int offset, const char *) { return browse(BR_LIKED, "", offset, 20); };
    list.on_click = [this](int i) {
      if (nav_play_busy()) return;
      list.busy_index = i;
      list.refresh();
      play(i);
    };
  }
  void build(lv_obj_t *root) override
  {
    ListScreen::build(root);
    // Il contatore sta subito dopo il titolo: a destra c'è il pulsante Riproduci
    lv_obj_set_align(header.info, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(header.info, CX(368), 21);
    lv_obj_set_width(header.title, CX(352) - CX(156));
    lv_obj_set_pos(header.spinner, CX(368), 18);
    ui_make_button(root, BTN_PRIMARY, CX(576), 8, 144, 48, IC_PLAY, "Riproduci",
                   [](lv_event_t *e) { ((LikedView *)lv_event_get_user_data(e))->play(-1); }, this);
  }
  void update_info() override
  {
    if (list.store.total >= 0) ui_set_text(header.info, "%d %s", list.store.total, list.store.total == 1 ? "brano" : "brani");
  }
  // Context della raccolta (da verificare con l'API 2026); se Spotify lo rifiuta, i brani caricati
  // a partire da quello toccato (max 50)
  void play(int index)
  {
    SpotifyStatus st;
    spotify_get_status(&st);
    char ctx[96];
    snprintf(ctx, sizeof(ctx), "spotify:user:%s:collection", st.user_id);
    String fallback = "{\"uris\":[";
    int start = max(index, 0);
    for (int i = start, n = 0; i < list.store.count && n < 50; i++) {
      if (!list.store.items[i].uri[0]) continue;
      if (n++) fallback += ',';
      fallback += '"';
      fallback += list.store.items[i].uri;
      fallback += '"';
    }
    fallback += "]}";
    play_context(ctx, "Brani piaciuti", -1, index >= 0 ? list.store.items[index].uri : nullptr,
                 list.store.count ? fallback.c_str() : nullptr);
  }
};

View *view_liked() { return new LikedView(); }

// ---------- Album (5.7) ----------

class AlbumsView : public ListScreen {
public:
  AlbumsView() : ListScreen(SEC_LIBRARY, "Album", true)
  {
    strcpy(list.cache_key, "albums");
    library_total = &library_total_albums;
    list.noun_plural = "album";
    list.what = "gli album";
    list.empty_icon = IC_DISC;
    list.empty_title = "Nessun album salvato";
    list.empty_text = "Salvali dall'app Spotify: compariranno qui.";
    // 10 per richiesta: ogni album salvato porta con sé anche i suoi brani (risposta pesante, ~8 KB ad album)
    list.fetch = [](int offset, const char *) { return browse(BR_ALBUMS, "", offset, 10); };
    list.on_click = [this](int i) { nav_push(view_album(list.store.items[i].uri, &list.store.items[i])); };
  }
  void update_info() override
  {
    if (list.store.total >= 0) ui_set_text(header.info, "%d album", list.store.total);
  }
};

View *view_albums() { return new AlbumsView(); }

class AlbumView : public View {
  BrowseItem al;
  bool have_meta;
  DetailPanel panel;
  PagedList list;
  lv_obj_t *info = nullptr, *spinner = nullptr;

public:
  AlbumView(const char *uri, const BrowseItem *meta)
  {
    memset(&al, 0, sizeof(al));
    have_meta = meta != nullptr;
    if (meta) al = *meta;
    strlcpy(al.uri, uri, sizeof(al.uri));
    list.style = ROW_NUMBERED;
    list.x = CX(320);
    list.w = 416;
    list.track_menu = true;
    list.noun_plural = "brani";
    list.what = "i brani";
    list.empty_title = "Questo album non ha brani";
    // Prima pagina: GET /albums/{id} (dati dell'album + primi 50 brani), poi /albums/{id}/tracks
    list.fetch = [this](int offset, const char *) {
      return browse(offset == 0 ? BR_ALBUM : BR_ALBUM_TRACKS, uri_id(al.uri), offset);
    };
    list.on_result = [this](const BrowseResult &r) {
      if (!r.meta) return;
      al = *r.meta;
      have_meta = true;
      if (panel.name) panel.set(al, panel_data().c_str());
    };
    list.on_click = [this](int i) {
      if (nav_play_busy() || !list.store.items[i].uri[0]) return;
      list.busy_index = i;
      list.refresh();
      play_context(al.uri, al.title, -1, list.store.items[i].uri);
    };
  }
  String panel_data()
  {
    String s = al.artist_name;
    if (al.year) s += String(s.length() ? " \xC2\xB7 " : "") + al.year;
    if (al.count) s += String(s.length() ? " \xC2\xB7 " : "") + al.count + (al.count == 1 ? " brano" : " brani");
    return s;
  }
  Section section() const override { return SEC_LIBRARY; }
  void build(lv_obj_t *root) override
  {
    panel = DetailPanel();
    panel.create(root, "Album", false, IC_DISC, play_cb, this);
    panel.set(al, have_meta ? panel_data().c_str() : "");
    info = ui_make_label(root, &font_it_16, COL_TEXT_3, "");
    lv_obj_set_pos(info, CX(344), 21);
    spinner = ui_make_spinner(root, 28);
    lv_obj_set_pos(spinner, CX(692), 18);
    list.build(root);
    list.start();
    tick();
  }
  void destroyed() override
  {
    panel = DetailPanel();
    info = spinner = nullptr;
    list.destroyed();
  }
  void tick() override
  {
    panel.image.tick();
    list.tick();
    ui_set_hidden(spinner, !list.loading());
    if (list.busy_index >= 0 && !nav_play_busy()) {
      list.busy_index = -1;
      list.refresh();
    }
    // "12 brani · 41 min": la durata solo se si conoscono tutti i brani
    int total = list.store.total;
    if (total > 0) {
      if (list.store.count >= total) {
        uint32_t ms = 0;
        for (int i = 0; i < list.store.count; i++) ms += list.store.items[i].duration_ms;
        uint32_t min = (ms + 30000) / 60000;
        if (min >= 60) ui_set_text(info, "%d brani \xC2\xB7 %lu h %lu min", total, (unsigned long)(min / 60), (unsigned long)(min % 60));
        else ui_set_text(info, "%d brani \xC2\xB7 %lu min", total, (unsigned long)min);
      } else {
        ui_set_text(info, "%d brani", total);
      }
    }
  }
  bool back() override
  {
    if (!list.menu_open()) return false;
    list.close_menu();
    return true;
  }
  void gesture(lv_dir_t d) override { list.gesture(d); }
  static void play_cb(lv_event_t *e)
  {
    AlbumView *v = (AlbumView *)lv_event_get_user_data(e);
    play_context(v->al.uri, v->al.title);
  }
};

View *view_album(const char *uri, const BrowseItem *meta) { return new AlbumView(uri, meta); }

// ---------- Artisti (5.8) ----------

class ArtistsView : public ListScreen {
public:
  ArtistsView() : ListScreen(SEC_LIBRARY, "Artisti seguiti", true)
  {
    strcpy(list.cache_key, "artists");
    library_total = &library_total_artists;
    list.round_images = true;
    list.noun_plural = "artisti";
    list.what = "gli artisti";
    list.empty_icon = IC_USER;
    list.empty_title = "Nessun artista seguito";
    list.empty_text = "Seguili dall'app Spotify: compariranno qui.";
    // Paginazione a cursore: "after" = id dell'ultimo artista ricevuto
    list.fetch = [](int offset, const char *cursor) -> uint32_t {
      if (offset > 0 && !cursor[0]) return 0;
      return browse(BR_ARTISTS, "", offset, 50, cursor);
    };
    list.on_click = [this](int i) { nav_push(view_artist(list.store.items[i], true)); };
  }
  void update_info() override
  {
    if (list.store.total >= 0) ui_set_text(header.info, "%d artisti", list.store.total);
  }
};

View *view_artists() { return new ArtistsView(); }

class ArtistView : public View {
  BrowseItem ar;
  bool complete;
  uint32_t meta_req = 0;
  DetailPanel panel;
  PagedList lists[3];
  int tab = 0;
  int built_tab = -1;  // la scheda cambia prima della ricostruzione: si smonta quella costruita
  lv_obj_t *spinner = nullptr;

public:
  ArtistView(const BrowseItem &a, bool c) : ar(a), complete(c)
  {
    static const char *const GROUPS[] = { "album", "single", "compilation,appears_on" };
    static const char *const EMPTY[] = { "Nessun album", "Nessun singolo", "Nient'altro" };
    for (int i = 0; i < 3; i++) {
      PagedList &l = lists[i];
      l.style = ROW_DETAIL_MEDIA;
      l.x = CX(320);
      l.w = 416;
      l.noun_plural = "album";
      l.what = "gli album";
      l.empty_icon = IC_DISC;
      l.empty_title = EMPTY[i];
      const char *group = GROUPS[i];
      l.fetch = [this, group](int offset, const char *) { return browse(BR_ARTIST_ALBUMS, uri_id(ar.uri), offset, 50, group); };
      l.on_click = [this, i](int k) { nav_push(view_album(lists[i].store.items[k].uri, &lists[i].store.items[k])); };
    }
  }
  Section section() const override { return SEC_LIBRARY; }
  void build(lv_obj_t *root) override
  {
    panel = DetailPanel();
    panel.create(root, "Artista", true, IC_USER, play_cb, this);
    panel.set(ar, complete ? ar.subtitle : "");
    if (!complete && !meta_req) meta_req = browse(BR_ARTIST, uri_id(ar.uri), 0);
    // Schede Album / Singoli / Altro (SPEC 4.10)
    static const char *const TABS[] = { "Album", "Singoli", "Altro" };
    static const int TX[] = { 336, 440, 544 };
    for (int i = 0; i < 3; i++) {
      lv_obj_t *t = ui_make_touch(root, CX(TX[i]), 0, 104, 64, COL_BG, false);
      lv_obj_t *l = ui_make_label(t, &font_it_sb20, i == tab ? COL_TEXT : COL_TEXT_3, TABS[i]);
      lv_obj_set_style_text_color(l, COL_TEXT, LV_STATE_PRESSED);
      lv_obj_center(l);
      lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
      if (i == tab) ui_make_box(t, 24, 55, 56, 3, COL_ACCENT);
      lv_obj_add_event_cb(t, tab_cb, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(t, (void *)(intptr_t)i);
    }
    spinner = ui_make_spinner(root, 28);
    lv_obj_set_pos(spinner, CX(692), 18);
    lists[tab].build(root);
    built_tab = tab;
    lists[tab].start();
    tick();
  }
  void destroyed() override
  {
    panel = DetailPanel();
    spinner = nullptr;
    if (built_tab >= 0) lists[built_tab].destroyed();
    built_tab = -1;
  }
  void tick() override
  {
    panel.image.tick();
    BrowseResult r;
    if (meta_req && spotify_take_result(meta_req, &r)) {
      meta_req = 0;
      if (r.meta) {
        ar = *r.meta;
        complete = true;
        panel.set(ar, ar.subtitle);
      }
      spotify_free(r.items);
      spotify_free(r.meta);
    }
    if (built_tab != tab) return;
    lists[tab].tick();
    ui_set_hidden(spinner, !lists[tab].loading() && !meta_req);
  }
  void gesture(lv_dir_t d) override { lists[tab].gesture(d); }
  static void tab_cb(lv_event_t *e)
  {
    ArtistView *v = (ArtistView *)lv_event_get_user_data(e);
    int i = (intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (i == v->tab) return;
    v->tab = i;
    nav_rebuild();
  }
  static void play_cb(lv_event_t *e)
  {
    ArtistView *v = (ArtistView *)lv_event_get_user_data(e);
    play_context(v->ar.uri, v->ar.title);
  }
};

View *view_artist(const BrowseItem &a, bool complete) { return new ArtistView(a, complete); }

// ---------- Coda (5.9) ----------

class QueueView : public ListScreen {
  char shown_track[48] = "";
  lv_obj_t *separator = nullptr;

public:
  QueueView() : ListScreen(SEC_QUEUE, "Coda", false)
  {
    list.clickable = false;
    list.noun_plural = "brani";
    list.what = "la coda";
    list.hide_end_note = true;
    list.empty_icon = IC_QUEUE;
    list.empty_title = "La coda \xC3\xA8 vuota";
    list.empty_text = "Avvia una playlist o un album: i prossimi brani compariranno qui.";
    list.empty_button = "Apri la libreria";
    list.empty_cb = [](lv_event_t *e) { nav_open_section(SEC_LIBRARY); };
    list.fetch = [](int offset, const char *) -> uint32_t { return offset == 0 ? browse(BR_QUEUE, "", 0) : 0; };
    // Riga 1 = brano attuale, in verde
    list.on_row_extra = [this](lv_obj_t *row, int i) {
      if (i != 0) return;
      lv_obj_t *title = lv_obj_get_child(row, 1), *sub = lv_obj_get_child(row, 2);
      ui_set_text_color(title, COL_ACCENT);
      char buf[128];
      snprintf(buf, sizeof(buf), "In riproduzione \xC2\xB7 %s", list.store.items[0].subtitle);
      ui_set_text_raw(sub, buf);
      ui_set_text_color(sub, COL_ACCENT);
    };
  }
  void build(lv_obj_t *root) override
  {
    PlayerState p;
    spotify_get_player(&p);
    strlcpy(shown_track, p.track_uri, sizeof(shown_track));
    list.store.loaded_ms = 0;  // la coda cambia di continuo: sempre da ricaricare
    ListScreen::build(root);
    separator = ui_make_box(root, CX(104), 144, 616, 1, COL_PRESSED);
  }
  void destroyed() override
  {
    ListScreen::destroyed();
    separator = nullptr;
  }
  void tick() override
  {
    ListScreen::tick();
    // A ogni cambio di brano la coda si ricarica
    PlayerState p;
    spotify_get_player(&p);
    if (strcmp(p.track_uri, shown_track) != 0) {
      strlcpy(shown_track, p.track_uri, sizeof(shown_track));
      list.reload();
    }
    if (separator) ui_set_hidden(separator, !(list.page == 0 && list.store.count > 1));
  }
  void update_info() override
  {
    if (list.store.loaded_ms) ui_set_text(header.info, "%d in coda", max(0, list.store.count - 1));
  }
};

View *view_queue() { return new QueueView(); }

// ---------- Dispositivi (5.11) ----------

class DevicesView : public View {
  bool pending;
  bool from_home;
  char *body;
  char name[100];
  Header header;
  PagedList list;
  uint32_t act_req = 0;
  lv_obj_t *note = nullptr;

public:
  DevicesView(bool p, const char *b, const char *n, bool home) : pending(p), from_home(home)
  {
    body = b ? spotify_strdup(b) : nullptr;
    strlcpy(name, n ? n : "", sizeof(name));
    list.style = ROW_DEVICE;
    if (pending) {
      list.y = 136;
      list.rows = 4;
    }
    list.noun_plural = "dispositivi";
    list.what = "i dispositivi";
    list.hide_end_note = true;
    list.empty_icon = IC_COMPUTER;
    list.empty_title = "Nessun dispositivo trovato";
    list.empty_text = "Apri Spotify sul computer o sul telefono, poi tocca Riprova.";
    list.empty_button = "Riprova";
    list.empty_cb = [](lv_event_t *e) { ((DevicesView *)lv_event_get_user_data(e))->list.reload(); };
    list.empty_user = this;
    list.fetch = [](int offset, const char *) -> uint32_t { return offset == 0 ? browse(BR_DEVICES, "", 0) : 0; };
    list.on_click = [this](int i) { choose(i); };
  }
  ~DevicesView() override { spotify_free(body); }
  Section section() const override { return SEC_DEVICES; }
  bool back_goes_home() const override { return from_home; }
  void build(lv_obj_t *root) override
  {
    header = Header();
    header.create(root, pending || nav_depth() > 1, "Dispositivi");
    ui_make_icon_button(root, CX(672), 0, IC_REFRESH, COL_TEXT_2,
                        [](lv_event_t *e) { ((DevicesView *)lv_event_get_user_data(e))->list.reload(); }, this);
    lv_obj_set_pos(header.spinner, CX(620), 18);
    if (pending) {
      // Avviso arancio (SPEC 4.13)
      lv_obj_t *w = ui_make_box(root, CX(104), 68, 680, 56, COL_WARN_BG, 12);
      lv_obj_set_style_border_width(w, 1, 0);
      lv_obj_set_style_border_color(w, COL_WARN, 0);
      lv_obj_t *ic = ui_make_label(w, &font_ic_20, COL_WARN, IC_WARN);
      lv_obj_set_pos(ic, 15, 16);
      lv_obj_t *t = ui_make_line_label(w, &font_it_16, COL_TEXT, 47, 16, 616);
      ui_set_text_raw(t, "Nessun dispositivo attivo: scegli dove far partire la musica.");
    }
    note = ui_make_line_label(root, &font_it_16, COL_TEXT_3, CX(112), 412, 600);
    if (pending && name[0]) ui_set_text(note, "Appena scegli, parte \xC2\xAB%s\xC2\xBB su quel dispositivo.", name);
    else if (pending) ui_set_text_raw(note, "Appena scegli, la musica riparte su quel dispositivo.");
    else ui_set_text_raw(note, "Non vedi un dispositivo? Apri Spotify l\xC3\xAC e aggiorna l'elenco.");
    list.build(root);
    list.store.loaded_ms = 0;  // sempre aggiornato all'apertura
    list.start();
    tick();
  }
  void destroyed() override
  {
    header = Header();
    note = nullptr;
    list.destroyed();
  }
  void tick() override
  {
    list.tick();
    header.set_loading(list.loading());
    // La nota in basso solo se c'è spazio sotto l'ultima riga
    int shown = list.store.count - list.page * list.rows;
    if (note) ui_set_hidden(note, list.store.count == 0 || shown > list.rows - 1 || list.failed());

    BrowseResult r;
    if (act_req && spotify_take_result(act_req, &r)) {
      act_req = 0;
      int i = list.busy_index;
      list.busy_index = -1;
      if (r.code >= 200 && r.code < 300 && i >= 0 && i < list.store.count) {
        char msg[140];
        snprintf(msg, sizeof(msg), "In riproduzione su \xC2\xAB%s\xC2\xBB", list.store.items[i].title);
        nav_toast(msg);
        for (int k = 0; k < list.store.count; k++) {
          if (k == i) list.store.items[k].flags |= IF_ACTIVE;
          else list.store.items[k].flags &= ~IF_ACTIVE;
        }
        list.refresh();
        if (pending) nav_back();  // il comando sospeso è partito: torno dov'ero
      } else {
        nav_toast("Non \xC3\xA8 partito. Riprova.", true);
        list.refresh();
      }
    }
  }
  void gesture(lv_dir_t d) override { list.gesture(d); }
  void choose(int i)
  {
    const BrowseItem &d = list.store.items[i];
    if (act_req || (d.flags & IF_RESTRICTED) || ((d.flags & IF_ACTIVE) && !pending)) return;
    BrowseRequest r = {};
    if (pending && body) {
      // Una sola richiesta: il comando sospeso direttamente su quel dispositivo
      r.kind = ACT_PLAY;
      strlcpy(r.arg2, d.uri, sizeof(r.arg2));
      r.body = spotify_strdup(body);
    } else {
      r.kind = ACT_TRANSFER;
      strlcpy(r.arg, d.uri, sizeof(r.arg));
    }
    act_req = spotify_browse(r);
    if (!act_req) return;
    list.busy_index = i;
    list.refresh();
  }
};

View *view_devices(bool pending, const char *body, const char *name, bool from_home)
{
  return new DevicesView(pending, body, name, from_home);
}
