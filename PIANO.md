# Controller Spotify ESP32 — Piano di sviluppo

Aggiornato al 2 novembre 2026.

## Scheda e configurazione

Si usa la VIEWE UEDX80480050E-WB-A già in casa: soddisfa tutti i requisiti, non serve comprare altro.

| Componente | Valore |
| --- | --- |
| Modulo | ESP32-S3-WROOM-1 N16R8 (16 MB flash, 8 MB PSRAM OPI) |
| Display | 5", 800×480, RGB parallelo, controller ST7262 |
| Touch | Capacitivo GT911 su I2C |
| Extra | Slot microSD, LED RGB, tasti BOOT e RESET |
| Porta "UART" (USB-C) | Caricamento firmware e log seriali |
| Porta "USB" (USB-C) | USB nativa dell'S3: ponte con il PC e modalità tastiera multimediale |

Impostazioni consigliate dalla libreria ESP32_Display_Panel
([fonte](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/board/board_viewe.md)):

| Voce | Valore |
| --- | --- |
| Board | ESP32S3 Dev Module (core arduino-esp32 3.x) |
| PSRAM | OPI PSRAM |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| USB CDC On Boot | Disabled se carichi dalla porta UART, Enabled se usi la porta USB |

Se cambi USB CDC tra un caricamento e l'altro, attiva una volta "Erase All Flash Before Sketch Upload".

FQBN indicativo per arduino-cli (da verificare con `arduino-cli board details -b esp32:esp32:esp32s3`):
`esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=default`

## Strumenti

Sviluppo con Claude Code nel terminale + arduino-cli (compila, carica, legge il seriale). Arduino IDE 2.x opzionale, condivide core e librerie.

Ambiente installato e verificato il 2 novembre 2026 (fase 1 chiusa). Al primo avvio Claude Code controlla cosa c'è sul computer e installa da solo quello che manca (istruzioni in `CLAUDE.md`), chiedendo all'utente solo password di amministratore o azioni fisiche.

- [x] Arduino IDE 2.x (opzionale) — 2.3.4
- [x] arduino-cli — 1.5.1
- [x] Core esp32 di Espressif 3.x — 3.3.12
- [x] Librerie: ESP32_Display_Panel 1.0.4, ESP32_IO_Expander 1.1.1, esp-lib-utils 0.2.3, lvgl 8.4.0 (versione degli esempi della libreria display), ArduinoJson 7.4.3, JPEGDEC 1.8.5
- [x] Python 3 (script login Spotify e ponte USB) — 3.14.0

## Architettura del firmware

Rete e interfaccia su due core diversi, così lo schermo resta fluido durante i download.

```
  [Programma ponte PC] ---> [Spotify Web API] ---> [Dispositivo che suona]
     ^ USB seriale              ^ HTTPS               (PC, telefono, casse)
     |                          |
 +---|--------------------------|------------------------------------+
 |  [Rete: Wi-Fi/hotspot/USB] <-> [Client Spotify] -> [Copertine]     |
 |                                 ^  token, JSON     JPEG 300px PSRAM|
 |                                 v                  cache microSD   |
 |                           [Stato condiviso] <------ immagine pronta|
 |                                 ^                                  |
 |                                 v                                  |
 |                           [Interfaccia LVGL] -> [Tastiera multim.] |
 |  Scheda ESP32-S3                800x480, GT911    USB, ultima ris. |
 +--------------------------------------------------------------------+
```

Il client Spotify passa sempre dal modulo Rete, quindi Wi-Fi, hotspot o cavo sono intercambiabili. La tastiera multimediale funziona anche senza rete.

## Fasi di sviluppo

Ogni fase si chiude con un test visibile; non si passa alla successiva prima.

1. **Ambiente pronto.** Claude Code controlla e installa gli strumenti mancanti, poi porta UART e sketch che stampa sul seriale.
   - Fatto quando: il seriale mostra ~8 MB di PSRAM. ✅ 2 novembre 2026: 8,00 MB.
2. **Display e touch.** Esempio LVGL di ESP32_Display_Panel configurato per UEDX80480050E-WB-A.
   - Fatto quando: demo fluida, tocco preciso.
   - ✅ 2 novembre 2026: variante `BOARD_VIEWE_UEDX80480050E_WB_A` (touch GT911), 5 bersagli centrati, animazione fluida, colori corretti.
3. **Wi-Fi e configurazione.** Portale al primo avvio (access point), credenziali in NVS, ora via NTP.
   - Fatto quando: dopo un riavvio si ricollega e mostra l'ora esatta. ✅ 2 novembre 2026.
4. **Login Spotify.** App sviluppatore, script Python per il primo login (PKCE), refresh token sulla scheda, rinnovo automatico.
   - Fatto quando: la scheda chiama `GET /me` e mostra il nome utente. ✅ 2 novembre 2026.
5. **Now playing.** Stato ogni 1-2 s, barra calcolata localmente, play/pausa, avanti, indietro, volume, copertina in PSRAM.
   - Fatto quando: cambio brano dal PC visibile entro 2 s; i tasti comandano il PC. ✅ 2 novembre 2026 (con SDK high_perf, vedi Rischi).
6. **Navigazione.** Liste paginate: playlist, brani piaciuti, album e artisti salvati, coda, ricerca con tastiera a schermo, scelta dispositivo.
   - Fatto quando: da playlist/album parte un brano sul PC. ✅ 3 novembre 2026: brano toccato in "Tecno house" e nell'album "Lovebars" parte sul MacBook (HTTP 204).
7. **Home personale.** Recenti, top artisti e brani (4 settimane, 6 mesi, sempre), playlist e album in riquadri.
   - Fatto quando: si carica in meno di 3 s.
   - ✅ 3 novembre 2026: Home secondo `design/fase7/SPEC_FASE7.md`; recenti sullo schermo in 0,7-0,85 s, artisti in 1,4 s (le copertine arrivano dopo, 4-9 s con il Wi-Fi della scrivania).
8. **Connessione senza Wi-Fi.** Programma Python che inoltra i comandi dalla porta USB a Spotify; la scheda sceglie da sola il canale.
   - Fatto quando: con Wi-Fi spento e cavo collegato tutto funziona.
   - ⏸ Rimandata il 3 novembre 2026 su richiesta di Valerio: da riprendere più avanti, dopo la fase 9.
9. **Ultima risorsa e rifiniture.** Tastiera multimediale USB, schermo che si abbassa, aggiornamenti OTA, eventuale case 3D.
   - Fatto quando: senza rete né ponte i tasti base funzionano.
   - 3 novembre 2026: schermo che si abbassa fatto (15% dopo 2 minuti, spento dopo 12, il tocco lo riaccende).
   - 4 novembre 2026: Now playing prende il colore della copertina (fondo scuro a tinta unita, play bianco) secondo `design/colore_copertina/SPEC_COLORE.md`.
   - 3 novembre 2026: aggiornamenti via Wi-Fi fatti (`strumenti/aggiorna_wifi.py`): 2 MB in 26-34 s, compilazione + invio + conferma in circa 1 minuto e mezzo; provato anche il ritorno automatico alla versione precedente con un firmware che si blocca apposta all'avvio.

Le fasi 1-5 sono il cuore: finite quelle, il controller è già usabile.

## Da fare più avanti (in pausa dal 3 novembre 2026)

Valerio ha messo in pausa lo sviluppo: queste cose restano da fare, in un ordine da decidere con lui.

- [x] **Aggiornamenti senza cavo (OTA)**: fatto il 3 novembre 2026, `.venv/bin/python strumenti/aggiorna_wifi.py`. Senza cavo restano fuori il registro, le foto dello schermo e i tocchi simulati: se servisse, si possono spostare anche quelli sulla rete.
- [x] **Meteo sull'Orologio**: fatto il 4 novembre 2026 secondo `design/meteo/SPEC_METEO.md` (`firmware/weather.cpp`, `firmware/ui_weather.cpp`), foto in `design/meteo/schermate/`. Da guardare a occhio: leggibilità al 15% di luce, trattini sotto il sole giallo e la fascia.
- [x] **Sole, luna, aria e pollini**: fatto il 5 novembre 2026 secondo `design/sole_aria/SPEC_SOLE_ARIA.md` (`firmware/weather.cpp`, `firmware/ui_weather.cpp`, `firmware/moon_images.c`), foto in `design/sole_aria/schermate/`. Da guardare a occhio: trattini sotto luna piena, barra gialla e barre dell'aria; luce al 15%; falce a 20 px.
- [x] **Home generale e giorni**: fatto il 5 novembre 2026 secondo `design/home_giorni/SPEC_HOME_GIORNI.md`, foto in `design/home_giorni/schermate/`. Da provare a mano: riquadro Spotify con un brano, Now playing, barra col tocco vero.
- [x] **Pubblicazione su GitHub**: fatta il 5 novembre 2026, https://github.com/Er-Chiacchiera/esp32-spotify-controller.
- [x] **Stato del computer**: chiesto da Valerio il 5 novembre 2026, fatto lo stesso giorno secondo `design/computer/SPEC_COMPUTER.md` (approvata da Valerio): `strumenti/ponte_computer.py` sul Mac, `firmware/computer.cpp` (dati e avvisi), `firmware/ui_computer.cpp` (riquadro della Home, pagina Computer, riga di Impostazioni). Foto in `design/computer/schermate/`. Avvio automatico sul Mac di Valerio dal 5 novembre (`strumenti/avvio_automatico_mac.py`, LaunchAgent).
- [x] **Città del meteo**: chiesto da Valerio il 5 novembre 2026, fatto lo stesso giorno secondo `design/citta/SPEC_CITTA.md` (approvata da Valerio): elenco di 8 città con la predefinita, ricerca, città al volo nella pagina Meteo. Foto in `design/citta/schermate/`.
- [x] **Comandi del Mac**: chiesti da Valerio il 6 novembre 2026 e fatti lo stesso giorno secondo `design/controllo_mac/SPEC_CONTROLLO_MAC.md` (approvata da Valerio): pagina Computer a schede Stato · Comandi (volume del Mac, muto, microfono, tieni sveglio, spegni lo schermo, stop con conferma), pulsante e spia nella Home, "Abbina" in Impostazioni, abbinamento con codice di 6 cifre e chiave firmata (`firmware/computer.cpp`, `firmware/ui_computer.cpp`, `strumenti/ponte_computer.py` versione 2 nell'avvio automatico). Provati sulla scheda con il Mac vero: abbinamento, volume, muto, microfono, tieni sveglio, conferme e Annulla, comando senza risposta; foto in `design/controllo_mac/schermate/`. Da provare a mano da Valerio: "Spegni lo schermo" e "Metti in stop" eseguiti davvero (con un doppio tocco veloce lo stop non deve partire), la scheda "è in stop" e il ritorno dei comandi al risveglio; spia arancio con la luce al 15%; trattini sotto il cerchio bianco del muto e il fondo arancio del microfono. Dopo: muto dentro Teams (con l'API locale di Teams, non con l'Accessibilità) e uscita audio (SPEC §9).
- [ ] **Copertine sulla microSD**: cache persistente delle copertine già viste (oggi 4-9 s per riscaricare quelle della Home). Serve una microSD nella scheda.
- [ ] **Tasti multimediali via USB** (fase 9): la scheda fa da tastiera play/pausa, avanti, indietro, volume dalla porta "USB", anche senza rete.
- [ ] **Scatola stampata in 3D** (fase 9, facoltativa): prima versione disegnata il 5 novembre 2026 in `scatola/` (OpenSCAD, render con Blender, istruzioni in `scatola/LEGGIMI.md`). Rivista da Claude Design lo stesso giorno (`design/scatola/SPEC_SCATOLA.md`, approvata da Valerio): scatola invariata, supporto a 25° con pareti da 4 mm, base fino a 82, labbro da 4, tondino d'acciaio Ø12×112 di zavorra, asola per la spina USB-C a L, uscita del cavo ad arco, incavi dei piedini corretti. Scritta "Er_Chiacchiera" incisa sul bordo superiore della scatola e sul retro del supporto (approvata da Valerio). Cavo: adattatore USB-C su/giù PremiumCord con un cavo dritto (vedi `scatola/LEGGIMI.md`). Da fare: controllare con la scheda in mano che le prese USB stiano sotto, stampare la striscia di prova (`ST_W` 20), poi i pezzi, e fare le prove del §8 della SPEC.
- [ ] **Fase 8, connessione senza Wi-Fi**: ponte Python dalla porta USB, rimandata da Valerio.
- [x] **Prove a mano della fase 7**: riquadro della Home che viene da una playlist o da un album (riprende dal brano giusto, ripiego senza offset se il brano non c'è più); trattini sotto le copertine chiare sullo schermo vero. Fatte da Valerio, funzionano (confermato il 6 novembre 2026).

## Spotify: app, login ed endpoint

App in Development Mode: proprietario con Premium, max 5 utenti
([guida febbraio 2026](https://developer.spotify.com/documentation/web-api/tutorials/february-2026-migration-guide)).

Login:
1. developer.spotify.com → crea app → Web API → Redirect URI esatto `http://127.0.0.1:8888/callback` (niente `localhost`).
2. Script Python: apre il browser, login una volta, ottiene il refresh token (Authorization Code + PKCE).
3. Refresh token sulla scheda (portale o seriale); la scheda rinnova da sola il token di accesso (dura 1 h).

Scope: `user-read-playback-state`, `user-modify-playback-state`, `user-read-currently-playing`, `playlist-read-private`, `playlist-read-collaborative`, `user-library-read`, `user-follow-read`, `user-top-read`, `user-read-recently-played`.

| Funzione | Endpoint |
| --- | --- |
| Stato riproduzione | `GET /me/player` |
| Play, pausa, avanti, indietro | `PUT /me/player/play`, `PUT /me/player/pause`, `POST /me/player/next`, `POST /me/player/previous` |
| Volume, posizione | `PUT /me/player/volume`, `PUT /me/player/seek` |
| Dispositivi e cambio | `GET /me/player/devices`, `PUT /me/player` |
| Coda | `GET /me/player/queue` |
| Playlist | `GET /me/playlists`, `GET /playlists/{id}/items` |
| Brani piaciuti, album, artisti | `GET /me/tracks`, `GET /me/albums`, `GET /me/following?type=artist` |
| Album e artista | `GET /albums/{id}/tracks`, `GET /artists/{id}/albums` |
| Ricerca | `GET /search` (max 10 risultati per richiesta) |
| Home personale | `GET /me/player/recently-played`, `GET /me/top/artists`, `GET /me/top/tracks` |

Limiti 2026:
- I brani di una playlist si elencano solo se è tua o collaborativa; le altre si possono far partire ma senza lista.
- Niente `artists/{id}/top-tracks`: la pagina artista mostra gli album.
- Niente categorie, nuove uscite, consigli.
- Copertine JPEG 640/300/64 px: si usa la 300.

## Rischi e soluzioni

| Rischio | Soluzione |
| --- | --- |
| Schermo che tremola durante i download | Bounce buffer in RAM interna, pixel clock ridotto (~14-16 MHz). Fase 3: 16 MHz + bounce 20 righe. Fase 5 (tornato con le copertine chiare): bounce 30 righe + SDK high_perf 3.2.0 (XIP da PSRAM). Sotto 16 MHz lo schermo va in autotest |
| Memoria esaurita con JSON grandi | Parametro `fields=`, pagine da 20-50, JSON e immagini in PSRAM |
| Interfaccia bloccata | Rete su un core, LVGL sull'altro, code tra i due |
| Errore 429 | Polling 1-2 s solo a schermo attivo, rispetto di Retry-After |
| Nessun dispositivo attivo (404) | Messaggio e apertura della scelta dispositivo |
| Token scaduto/revocato | Rinnovo automatico, altrimenti schermata per rifare il login |
| Credenziali esposte | Token e password Wi-Fi in NVS, mai nel codice o su GitHub |
