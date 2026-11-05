// Navigazione della fase 6: barra laterale fissa a sinistra, area contenuto 712x480 a destra,
// pila del "indietro". La barra si crea una volta sola; cambiando schermata si ricostruisce solo
// il contenuto (SPEC §1 e §4.1).
#pragma once

#include <lvgl.h>
#include "spotify.h"

enum Section { SEC_LIBRARY, SEC_SEARCH, SEC_QUEUE, SEC_DEVICES, SEC_HOME, SECTION_COUNT,
               SEC_NONE };  // pagina fuori dalle sezioni (Meteo): nessuna voce della barra accesa

// Coordinate della SPEC (schermo intero) dentro l'area contenuto, che parte a x 88
#define CX(x) ((x) - 88)

class View {
public:
  virtual ~View() {}
  virtual Section section() const = 0;
  virtual void build(lv_obj_t *root) = 0;  // crea i widget nell'area contenuto
  virtual void destroyed() {}              // i widget sono stati cancellati: azzerare i puntatori
  virtual void tick() {}                   // ogni 100 ms mentre è visibile
  virtual bool back() { return false; }    // true se l'ha gestito da sola (es. chiude il menu)
  virtual void gesture(lv_dir_t dir) {}    // swipe su/giù (pagine); quello a destra è "indietro"
  virtual void reselect() {}               // voce della barra toccata mentre si è già alla radice
  virtual bool back_goes_home() const { return false; }  // indietro alla radice torna a Now playing
};

void nav_create(lv_obj_t *scr);
lv_obj_t *nav_panel();       // contenitore a tutto schermo (barra + contenuto)
bool nav_browsing();         // true: si mostra la navigazione, false: Now playing / Orologio
void nav_open_section(Section s);
void nav_open_view(View *root);  // apre una schermata come radice (es. Dispositivi con comando sospeso)
void nav_push(View *v);
void nav_back();
void nav_home();
int nav_depth();
void nav_tick();             // dal timer dell'interfaccia
void nav_rebuild();          // ricostruisce la schermata in cima (es. passaggio tastiera/risultati)

void nav_toast(const char *text, bool error = false);

// Avvio di un contesto o di un brano (PUT /me/player/play con il corpo JSON indicato).
// Se Spotify risponde "nessun dispositivo attivo" si apre Dispositivi con il comando in sospeso.
// fallback_json: corpo da provare se Spotify rifiuta il primo (es. context dei brani piaciuti)
void nav_play(const char *body_json, const char *context_name, const char *fallback_json = nullptr);
bool nav_play_busy();
// Comando qualsiasi con un toast di conferma (es. aggiunta alla coda)
void nav_action(const BrowseRequest &req, const char *ok_toast);
