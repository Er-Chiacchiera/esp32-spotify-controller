// Componenti riutilizzabili della navigazione (SPEC §4): immagine con segnaposto, intestazione,
// blocco vuoto/errore, pannello dettaglio, elenco a pagine da 5 righe con colonna pagine e menu del brano.
#pragma once

#include <functional>
#include <lvgl.h>
#include "spotify.h"

// Immagine 64 o 150 con segnaposto (quadrato PRESSED con icona) finché non arriva (SPEC §3.6)
struct ImageBox {
  lv_obj_t *box = nullptr, *placeholder = nullptr, *img = nullptr;
  lv_img_dsc_t dsc;
  char url[200] = "";
  int size = 64;
  uint8_t scale = 0;
  bool shown = false;
  void create(lv_obj_t *parent, int x, int y, int size, bool round, const char *placeholder_icon);
  void set(const char *url, uint8_t scale, const char *placeholder_icon = nullptr);
  void tick();  // mostra l'immagine appena è pronta
};

// Intestazione 712x64 (SPEC 4.2)
struct Header {
  lv_obj_t *back = nullptr, *title = nullptr, *info = nullptr, *spinner = nullptr;
  void create(lv_obj_t *root, bool with_back, const char *title_text);
  void set_loading(bool loading);
};

// Freccia indietro 64x64 a x 88 (intestazione e pannello dettaglio)
lv_obj_t *ui_make_back_button(lv_obj_t *root);

// Blocco vuoto/errore centrato nell'area x0..x0+w (coordinate dell'area contenuto) (SPEC 4.9)
lv_obj_t *ui_state_block(lv_obj_t *parent, int x0, int w, const char *icon, lv_color_t icon_color, const char *title,
                         const char *text, const char *button = nullptr, lv_event_cb_t cb = nullptr,
                         void *user = nullptr);

// Altezza della scritta su misura (1..max_lines righe), con i puntini se non ci sta
void ui_label_lines(lv_obj_t *label, const char *text, int width, int max_lines);

// Pannello sinistro dei dettagli 232x480 (SPEC 4.6)
struct DetailPanel {
  lv_obj_t *bg = nullptr, *context = nullptr, *name = nullptr, *data = nullptr, *play = nullptr;
  ImageBox image;
  void create(lv_obj_t *root, const char *context_text, bool round, const char *placeholder_icon, lv_event_cb_t play_cb,
              void *user);
  void set(const BrowseItem &it, const char *data_text);
};

// Voci caricate a blocchi di 50 (in PSRAM)
struct ItemStore {
  BrowseItem *items = nullptr;
  int count = 0, cap = 0;
  int total = -1;          // -1: non ancora noto
  bool more = false;       // ricerca: Spotify ne ha altre oltre a quelle caricate
  char cursor[40] = "";    // artisti: cursore della pagina dopo
  uint32_t loaded_ms = 0;
  ~ItemStore() { clear(); }
  void clear();
  void append(const BrowseItem *src, int n);
};

// Elenchi chiusi da poco, ripresi senza richieste se riaperti entro 5 minuti (Playlist, classifiche...)
const ItemStore *list_cache_peek(const char *key);   // nullptr se manca o è scaduto
void list_cache_put(const char *key, ItemStore &store);  // sposta le voci nella memoria: store resta vuoto

// Menu del brano (SPEC 4.11): pannello 320x235 a x 400 (schermo) e y indicata, nell'area contenuto.
// Uno solo alla volta; on_close parte quando si chiude (non quando sparisce con la schermata).
void ui_track_menu_open(lv_obj_t *parent, const BrowseItem &it, int y, std::function<void()> on_close);
void ui_track_menu_close();
void ui_track_menu_forget();  // la schermata che lo conteneva è stata cancellata
bool ui_track_menu_is_open();

enum RowStyle {
  ROW_MEDIA,         // 648x80: immagine, titolo, sottotitolo, durata (SPEC 4.3)
  ROW_NUMBERED,      // 416x80: numero, titolo, sottotitolo, durata (SPEC 4.4)
  ROW_DETAIL_MEDIA,  // 416x80 con copertina (pagina artista)
  ROW_DEVICE,        // 648x80 con cerchio icona
  ROW_RANK,          // 648x80: posizione, immagine, titolo, sottotitolo, durata (SPEC 7 §4.5)
};

class PagedList {
public:
  // Configurazione (prima di build)
  RowStyle style = ROW_MEDIA;
  int x = 0, y = 64, w = 648;   // area dell'elenco nelle coordinate dell'area contenuto
  int rows = 5;
  bool round_images = false;
  bool clickable = true;        // la coda è di sola lettura
  bool track_menu = false;      // pressione lunga sui brani
  bool hide_end_note = false;
  int search_type = -1;         // elenco di una scheda dei risultati: tiene solo le voci di quel tipo
  char cache_key[56] = "";      // se impostata, uscendo l'elenco resta in memoria 5 minuti (riaperto subito)
  const char *noun_plural = "voci";            // "Fine dell'elenco · 27 artisti"
  const char *what = "l'elenco";               // "Non riesco a caricare ..."
  const char *empty_icon = nullptr, *empty_title = nullptr, *empty_text = nullptr, *empty_button = nullptr;
  lv_event_cb_t empty_cb = nullptr;
  void *empty_user = nullptr;
  std::function<uint32_t(int offset, const char *cursor)> fetch;  // restituisce l'id della richiesta
  std::function<void(int index)> on_click;
  std::function<void(lv_obj_t *row, int index)> on_row_extra;  // personalizza una riga (es. coda)
  std::function<void(const BrowseResult &r)> on_result;          // dati in più del risultato (es. meta dell'album)
  std::function<void(int index)> on_long;                         // pressione lunga (al posto del menu del brano)
  std::function<bool(const BrowseItem &it)> is_playing;           // voce in riproduzione (default: stesso brano)
  std::function<void(const BrowseItem &it, char *out, size_t n)> right_text;  // testo a destra al posto della durata

  ItemStore store;
  int page = 0;
  ~PagedList();
  int busy_index = -1;          // riga "in corso" (spinner al posto della durata)

  void build(lv_obj_t *parent);
  void destroyed();
  void tick();
  void refresh();               // ridisegna la pagina corrente
  bool gesture(lv_dir_t dir);   // swipe su/giù: pagina dopo/prima
  void start();                 // prima richiesta (se l'elenco non è in memoria da meno di 5 minuti)
  void reload();                // Riprova / Aggiorna
  void set_result(const BrowseResult &r);  // risultato arrivato da fuori (es. ricerca); non lo libera
  void set_page(int p);
  bool loading() const { return req != 0; }
  bool failed() const { return error; }
  int page_count() const;
  int known_total() const;      // total se noto, altrimenti le voci caricate
  bool menu_open() const { return menu_index >= 0; }
  void close_menu();
  void open_menu(int index);

private:
  struct Row;
  struct Widgets;
  Widgets *wd = nullptr;
  int menu_index = -1;
  uint32_t req = 0;
  bool error = false;
  int error_code = 0;
  uint32_t shown_player_version = UINT32_MAX;
  char playing_uri[48] = "";
  void request_more(bool background = false);
  void render_row(Row &r, int index);
  static void row_event(lv_event_t *e);
  static void pager_event(lv_event_t *e);
  static void retry_event(lv_event_t *e);
};
