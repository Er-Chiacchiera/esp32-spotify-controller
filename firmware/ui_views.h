// Schermate della navigazione (fase 6). Ognuna è una View (ui_nav.h) creata da queste funzioni.
#pragma once

#include "spotify.h"
#include "ui_nav.h"

View *view_library();
View *view_playlists();
View *view_playlist(const BrowseItem &playlist);
View *view_liked();
View *view_albums();
View *view_album(const char *uri, const BrowseItem *meta);  // meta: dati già noti (elenco, ricerca) o nullptr
View *view_artists();
View *view_artist(const BrowseItem &artist, bool complete);  // complete: foto e generi già presenti
View *view_queue();
View *view_search();
View *view_home();     // Ascolti, la Home di Spotify della fase 7 (ui_home.cpp)
View *view_recent();   // Ascoltati di recente, tutti
View *view_top();      // Più ascoltati
View *view_settings(); // Impostazioni: Wi-Fi e Informazioni (ui_settings.cpp, SPEC_HOME_GIORNI §6)
// pending: aperta da un comando fallito per "nessun dispositivo attivo". body: corpo JSON del comando
// da far ripartire (nullptr: riprende la riproduzione). from_home: indietro torna alla Home.
View *view_devices(bool pending = false, const char *body = nullptr, const char *name = nullptr, bool from_home = false);

// Totali visti l'ultima volta (conteggi dei riquadri della Libreria; -1 se mai aperti)
extern int library_total_playlists, library_total_liked, library_total_albums, library_total_artists;
