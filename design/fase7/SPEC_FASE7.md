# Specifiche fase 7: Home personale

Documento di Claude Design per Claude Code. Risponde a `../BRIEF_FASE7.md`.

- **Mockup**: `mockup/NN_nome.png`, 800×480.
- **Sorgente dei mockup**: `../fase6/canvas/project/f7_*.dc.html`, con gli stili in `../fase6/canvas/project/ui.css` (sezione "fase 7" in fondo).
- **Canvas online** (privato), pagina "Fase 7 · Home": https://claude.ai/artifact/2BmkheovXWkQTH5Pqxk8tw
- **Base**: tutto quello che c'è in `../fase6/SPEC_FASE6.md` resta valido: colori §3.1, caratteri §3.2, griglia §3.3, stati §3.4, testo §3.5, immagini §3.6, rete e pagine §3.7, componenti §4. Qui c'è solo ciò che è **nuovo** o **cambia**; i nomi dei colori (`TEXT_2`, `PRESSED`…) sono quelli della SPEC 6.

Le catture reali della fase 6 sono fedeli ai mockup: non cambio niente di quello che c'è, a parte i tre ritocchi del §6.

---

## 1. Risposte alle domande aperte

**1. La Home è la schermata di partenza?**
No. **L'Orologio resta la schermata di riposo** quando non suona niente: sulla scrivania si guarda l'ora molto più spesso di quanto si sfogli la musica, e l'Orologio non fa richieste in rete mentre la scheda è ferma. Ma l'Orologio diventa la **porta della Home**: il suo pulsante principale passa da "Libreria" a **"Home"** (`04_orologio.png`). All'accensione non cambia niente: Now playing se suona qualcosa, altrimenti l'Orologio.

**2. Struttura: vetrine o schede?**
**Una pagina sola, senza scorrimento, con due vetrine**:
1. "Ascoltati di recente": 4 riquadri da 150 px;
2. "Artisti più ascoltati · 4 settimane": 5 cerchi da 80 px.

Ogni vetrina ha **"Tutti ›"**, che apre l'elenco completo a pagine (componenti della fase 6). Due vetrine stanno in 480 px senza rimpicciolire nulla, e la prima vista costa **2 richieste** (3 se il nome di una playlist non è ancora in memoria). Le schede avrebbero nascosto metà del contenuto dietro un tocco in più.

**3. Recenti: brani singoli o raggruppati?**
**Raggruppati per contesto e senza doppioni**: un riquadro per ogni playlist o album da cui hai ascoltato, il più recente prima. Il tocco **riprende** quel contesto dal brano ascoltato per ultimo. È quello che serve davvero da un telecomando: "rimetti quello che stavo ascoltando". Il nome del contesto non richiede richieste in più:
- per gli **album** il nome è già nel brano;
- per le **playlist dell'utente** viene dall'elenco playlist della fase 6, quasi sempre già in memoria;
- per tutto il resto (playlist di Spotify o di altri, radio) il riquadro mostra il **brano** ("Brano · Kungs"), e il tocco lo fa ripartire comunque dentro quel contesto.

**4. Periodo dei più ascoltati**
Un **selettore a tre segmenti** nell'intestazione, **"4 settimane · 6 mesi · 1 anno"** (componente nuovo, §4.4). All'apertura c'è sempre **4 settimane**: è il periodo che cambia di più e racconta cosa ascolti adesso. Artisti e brani sono due **schede** (componente 4.10 della fase 6), nella stessa intestazione.

**5. Playlist e album a riquadri**
I riquadri della Home **sono** le playlist e gli album che hai ascoltato di recente: mescolati, in ordine di ascolto, **4 nella Home** e tutti (fino a ~50) in "Tutti". Una terza vetrina con le playlist salvate ripeterebbe la Libreria, costerebbe una richiesta in più e 4 copertine da 150 in più. La Libreria resta l'elenco completo.

**6. Tocco su un brano dei più ascoltati**
Parte **la classifica da quel brano in poi** (fino a 50 brani, `uris`), come i Brani piaciuti. Chi apre "i miei brani più ascoltati" vuole sentirne più di uno.

---

## 2. Navigazione

```
Barra: [copertina] [play] [HOME] [Libreria] [Cerca] [Coda] [Dispositivi]

Home ─┬─ riquadro (tocco) ............... riprende il contesto, si resta sulla Home
      ├─ riquadro (pressione lunga) ..... dettaglio playlist / album / pagina artista (fase 6)
      ├─ "Tutti" (recenti) ─── Ascoltati di recente (elenco) ── stessi tocchi dei riquadri
      ├─ artista ─────────────── Pagina artista (fase 6)
      └─ "Tutti" (artisti) ─── Più ascoltati [Artisti | Brani] × [4 settimane | 6 mesi | 1 anno]
                                  ├─ artista → Pagina artista
                                  └─ brano → parte la classifica da lì · pressione lunga → menu del brano (05b)
Orologio ── "Home" ── Home
Now playing / Orologio ── icona Home in alto ── Home
```

- La voce **Home** è a `0,152 88×64`, nel posto riservato, con l'icona casa 28 px. Stati come le altre voci (SPEC 6 §4.1).
- **Voce attiva = sezione da cui è partita la pila**: tutto ciò che si apre dalla Home (Recenti, Più ascoltati, pagina artista, dettagli) tiene accesa la Home; la stessa pagina artista aperta dalla Libreria accende la Libreria.
- Indietro e swipe a destra come in fase 6. Toccare Home nella barra riporta sempre alla Home, all'inizio.

---

## 3. Schermate

### 3.1 Home — `01_home.png`

Nessuna intestazione: i titoli delle vetrine fanno da intestazione.

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Titolo vetrina 1 "Ascoltati di recente" | 112,15 | — | `SB20 TEXT` |
| "Tutti ›" vetrina 1 | 680,0 | 96×56 (tocco) | §4.1 |
| Riquadri 1-4 | x 112 · 283 · 454 · 626, y 56 | 150×202 | §4.2 |
| Titolo vetrina 2 "Artisti più ascoltati" | 112,297 | — | `SB20 TEXT` |
| "· 4 settimane" | subito dopo il titolo, +8 px, y 301 | — | `M16 TEXT_3` |
| "Tutti ›" vetrina 2 | 680,282 | 96×56 | §4.1 |
| Celle artista 1-5 | x 112 · 245 · 378 · 511 · 644, y 334 | 132×124 | §4.3 |

**Contenuto dei riquadri** (da `recently-played`, vedi §5):

| Contesto dell'ascolto | Titolo | Sottotitolo | Immagine | Tocco | Pressione lunga |
|---|---|---|---|---|---|
| Album | nome dell'album | "Album · " primo artista dell'album | copertina album | `play` con `context_uri` album + `offset.uri` ultimo brano | dettaglio album |
| Playlist dell'utente (trovata nella cache playlist) | nome della playlist | "Playlist" | immagine della playlist | `context_uri` playlist + `offset.uri` | dettaglio playlist |
| Brani piaciuti (`…:collection`) | "Brani piaciuti" | "Playlist" | quadrato `PRESSED` con cuore 40 `ACCENT` (non è un segnaposto: resta così) | `context_uri` collection + `offset.uri` | apre Brani piaciuti |
| Artista | nome dell'artista (preso dagli artisti del brano) | "Artista" | copertina dell'album del brano | `context_uri` artista + `offset.uri` | pagina artista |
| Altra playlist, radio, nessun contesto | titolo del brano | "Brano · " primo artista | copertina album | `context_uri` (se c'è) + `offset.uri`, altrimenti `uris:[brano]` | menu del brano (05b) |

- **Contesto in riproduzione** (`context.uri` uguale a quello di `/me/player`): titolo in `ACCENT` (`Tecno house` nel mockup).
- Dopo il tocco: il riquadro resta "premuto" e il sottotitolo diventa spinner 20 + "Avvio…" `M16 TEXT_3`, finché arriva la risposta. Poi si resta sulla Home (come in fase 6). Errori e 404 come in fase 6 (toast, Dispositivi `13b`).
- Celle artista: tocco → pagina artista (fase 6 §5.8). Nessuna pressione lunga.

### 3.2 Home — stati — `01b_home-caricamento.png`, `01c_home-parziale.png`, `01d_home-errore.png`, `01e_home-vuota.png`

**Caricamento (`01b`, istante 0)**: i due titoli delle vetrine sono testo fisso e compaiono **subito**: la struttura c'è già al tocco.

| Elemento | Posizione | Stile |
|---|---|---|
| Spinner della vetrina che sta caricando | 740,14 (vetrina 1) o 740,296 (vetrina 2), al posto di "Tutti" | 28 px, SPEC 6 §4.14 |
| Copertina finta | come il riquadro, 150×150 | `SURFACE #242424`, raggio 8 |
| Barra titolo | x riquadro, y 218, alta 16, larghezze 110/124/96/116 | `SURFACE`, raggio 8 |
| Barra sottotitolo | x riquadro, y 244, alta 12, larghezze 72/96/110/84 | `SKELETON_2 #1E1E1E`, raggio 8 |
| Cerchio finto | cella + 26, y 338, 80×80 | `SURFACE`, tondo |
| Barra nome | cella + 26, y 434, 80×14 | `SURFACE`, raggio 7 |

Tutto statico; l'unica cosa che si muove è lo spinner.

**Dopo ~1 s (`01c`)**: la vetrina 1 ha già i testi, due copertine sono arrivate e due mostrano il segnaposto (SPEC 6 §3.6: quadrato `PRESSED`, icona 40 `PLACEHOLDER_FG`: disco per gli album, nota per playlist e brani, persona per gli artisti). La vetrina 2 è ancora scheletro, con lo spinner al posto del suo "Tutti".

**Errore (`01d`)**: se fallisce `recently-played` e non c'è niente in memoria → blocco errore della SPEC 6 §4.9, centrato nell'area 88–800: "Non riesco a caricare la Home" / "Controlla il Wi-Fi e riprova." / Riprova (che rifà tutta la sequenza §5).
Errori parziali (nessun mockup):
- fallisce solo la richiesta degli artisti → nell'area della vetrina 2: testo `M16 TEXT_2` "Non riesco a caricare gli artisti." a `112,350` e pulsante `bs sm` "Riprova" 160×48 a `112,384` (riprova solo quella richiesta);
- fallisce solo la richiesta delle playlist → nessun errore visibile: i riquadri di quelle playlist usano la regola "Brano · artista".

**Vuota (`01e`)**: se recenti **e** artisti sono vuoti (account nuovo) → blocco stato: icona casa 40 nel cerchio, "La tua Home è ancora vuota" / "Ascolta qualcosa: qui compariranno i brani recenti e i tuoi artisti preferiti." Sotto ci sono due pulsanti: `bp` "Apri la libreria" 220×56 a `238,296` (icona Libreria 20) e `bs` "Cerca" 176×56 a `474,296`.
Se è vuota una sola vetrina, il titolo resta e al posto dei riquadri o dei cerchi c'è una riga `M16 TEXT_3`:
- vetrina 1, a `112,120`: "Qui compariranno le playlist e gli album che ascolti.";
- vetrina 2, a `112,350`: "Spotify calcola i tuoi artisti dopo qualche giorno di ascolti.".

"Tutti" è nascosto nelle vetrine vuote.

### 3.3 Ascoltati di recente — tutti — `02_recenti-tutti.png`

- Intestazione SPEC 6 §4.2: freccia, "Ascoltati di recente", contatore "N elementi" (numero di contesti dopo il raggruppamento).
- Elenco a pagine, righe 648×80 (SPEC 6 §4.3) nella variante **Recente**: immagine 64; titolo e sottotitolo come i riquadri §3.1; **a destra il tempo trascorso** `M16 TEXT_3`, larghezza 64, allineato a destra:

| Tempo dall'ascolto | Testo |
|---|---|
| < 1 min | "adesso" |
| < 60 min | "12 min" |
| < 24 h | "5 h" |
| ieri (data di calendario) | "ieri" |
| prima | "2 nov" (giorno + mese abbreviato: gen feb mar apr mag giu lug ago set ott nov dic) |

  Se è il contesto in riproduzione: titolo `ACCENT` e icona volume 20 `ACCENT` al posto del tempo.
- Tocco e pressione lunga come i riquadri.
- **Nessuna richiesta in più**: usa la stessa risposta di `recently-played` della Home.

### 3.4 Più ascoltati — `03_piu-ascoltati-artisti.png`, `03b_piu-ascoltati-brani.png`

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Freccia indietro | 88,0 | 64×64 | SPEC 6 §4.2 |
| Scheda "Artisti" | 152,0 | 104×64 | SPEC 6 §4.10 |
| Scheda "Brani" | 256,0 | 104×64 | SPEC 6 §4.10 |
| Selettore del periodo | 476,8 | 300×48 | §4.4 |
| Righe | y 64 · 144 · 224 · 304 · 384 | 648×80 | §4.5 |
| Colonna pagine | 736,64 | 64×400 | SPEC 6 §4.5 |

- Niente titolo testuale: le schede dicono cosa c'è, la voce Home accesa dice dove si è.
- Si apre da "Tutti" della vetrina artisti, con **Artisti + 4 settimane**. Il periodo scelto resta finché non si spegne la scheda.
- Cambiare scheda o periodo: istantaneo se i dati sono in memoria; altrimenti righe scheletro (SPEC 6 §4.8), colonna pagine nascosta, spinner 28 a `754,250` (centro della colonna).
- **Artisti**: posizione, foto tonda 64, nome `SB20` centrato in verticale (`y` riga + 27). Niente sottotitolo: Spotify non dà più i generi. Tocco → pagina artista.
- **Brani**: posizione, copertina 64, titolo, artisti, durata. Il brano in riproduzione ha il titolo `ACCENT` e l'icona volume al posto della durata. Tocco → `PUT /me/player/play` con `uris` = i brani della classifica caricati, **dal brano toccato in poi** (max 50). Pressione lunga → menu del brano (SPEC 6 §4.11).
- Dati: `GET /me/top/artists` e `GET /me/top/tracks`, `time_range` = `short_term` / `medium_term` / `long_term`, `limit=50`: una richiesta per ogni combinazione scheda × periodo, tenuta 5 minuti. "Artisti · 4 settimane" è già in memoria dalla Home.
- Vuoto: blocco stato "Ancora pochi ascolti in questo periodo" / "Spotify aggiorna la classifica una volta al giorno. Prova un periodo più lungo.", senza pulsante. La riga di schede e periodo resta usabile.

### 3.5 Orologio con Home — `04_orologio.png`

Uguale a SPEC 6 §5.2, salvo:
- icona **Home** 64×64 a `536,0` (prima di Libreria, Cerca, Coda);
- pulsante primario `bp sm` 176×48 a `216,348`: icona casa 20 + **"Home"** (prima era "Libreria"); il secondario resta "Cerca";
- suggerimento a `y 308`: "Riprendi da quello che hai ascoltato o cerca un brano.".

### 3.6 Now playing con Home — `05_now-playing.png`

Uguale a SPEC 6 §5.1, salvo l'icona **Home** 64×64 a `536,0`, `ibtn` `TEXT_2`. Le icone in alto diventano Home 536 · Libreria 600 · Cerca 664 · Coda 728.

---

## 4. Componenti nuovi

### 4.1 "Tutti ›" (link di vetrina)

- Area di tocco 96×56, allineata a destra a `x 776`.
- Contenuto allineato a destra: "Tutti" `M16 TEXT_2`, spazio 4, chevron destro 20 `TEXT_2`, centrato in verticale.
- Premuto: testo e chevron `TEXT`, fondo `PRESSED` raggio 8 sull'area di tocco.
- Nascosto mentre la vetrina carica (al suo posto lo spinner) o se è vuota.

### 4.2 Riquadro — 150×202

| Parte | Posizione relativa | Stile |
|---|---|---|
| Immagine | 0,0 150×150 | 300/2 (SPEC 6 §3.6), quadrata, senza angoli arrotondati |
| Titolo | 0,158, L 150 | `M16 TEXT`, 1 riga con "…" |
| Sottotitolo | 0,180, L 150 | `M16 TEXT_3`, 1 riga con "…" |

| Stato | Resa |
|---|---|
| Normale | come sopra, senza fondo |
| Premuto | rettangolo `PRESSED #2A2A2A` raggio 8 dietro al riquadro, 8 px più grande per lato (166×218 a −8,−8) |
| In riproduzione | titolo `ACCENT` |
| In avvio | resta premuto; sottotitolo = spinner 20 + "Avvio…" `M16 TEXT_3` |
| Immagine non arrivata | segnaposto SPEC 6 §3.6 con icona 40 |

Lo spazio tra i riquadri è di 21 px: il fondo premuto (8 px) non tocca i vicini.

### 4.3 Cella artista — 132×124

| Parte | Posizione relativa | Stile |
|---|---|---|
| Foto | 26,4 80×80, tonda | 160/2 (oppure 320/4 se manca la 160), `clip_corner` come in fase 6 |
| Nome | 4,92, L 124, centrato | `M16 TEXT`, 1 riga con "…" |
| Premuto | tutta la cella | fondo `PRESSED`, raggio 8 |
| Foto non arrivata | — | cerchio `PRESSED` con persona 40 `PLACEHOLDER_FG` |

### 4.4 Selettore del periodo — 300×48

- Contenitore: fondo `SURFACE #242424`, raggio 24, padding 4.
- Tre segmenti alti 40, raggio 20, larghi **124 · 84 · 84**: "4 settimane", "6 mesi", "1 anno", testo `M16` centrato.
- Area di tocco di ogni segmento: tutta l'altezza 48 e la sua larghezza.

| Stato | Fondo | Testo |
|---|---|---|
| Normale | nessuno | `TEXT_2` |
| Selezionato | `TEXT #FFFFFF` | `#000000` |
| Premuto (non selezionato) | `RAISED #3A3A3A` | `TEXT` |

Non c'è uno stato disabilitato. Toccare un altro periodo mentre un elenco sta caricando annulla quel caricamento e avvia il nuovo.

### 4.5 Riga classifica — 648×80 (variante della SPEC 6 §4.3)

| Parte | Posizione relativa | Stile |
|---|---|---|
| Posizione | 0,29, L 48, centrata | `M16 TEXT_3` |
| Immagine | 48,8 64×64 | tonda per gli artisti |
| Titolo | 128,15 (con sottotitolo) o 128,27 (senza) | `SB20 TEXT` |
| Sottotitolo | 128,43 | `M16 TEXT_2` |
| Durata | come SPEC 6 §4.3 | `M16 TEXT_3` |

Titolo e sottotitolo vanno fino a −24 dal bordo destro, −96 se c'è la durata.

### 4.6 Riga recente

È la riga della SPEC 6 §4.3 con il tempo trascorso (§3.3) al posto della durata. Nient'altro di nuovo.

---

## 5. Ordine di caricamento della Home (obiettivo: < 3 s)

Una richiesta alla volta, nell'ordine:

| # | Richiesta | Quando | Cosa compare |
|---|---|---|---|
| 0 | — | al tocco | titoli delle vetrine + scheletro (`01b`) |
| R1 | `GET /me/player/recently-played?limit=50` | subito | (aspetta R2 se serve) |
| R2 | `GET /me/playlists?limit=50&fields=…` (come fase 6) | **solo se** un riquadro dei primi 4 ha un contesto playlist e la cache delle playlist è vuota o scaduta | testi della vetrina 1 (dopo R1, o dopo R2 se serve: così i titoli non cambiano sotto gli occhi) |
| R3 | `GET /me/top/artists?time_range=short_term&limit=50` | dopo R1/R2 | nomi degli artisti |
| I1-I4 | copertine 150 dei 4 riquadri, da sinistra a destra | dopo R3 | copertine una alla volta |
| I5-I9 | foto 80 dei 5 artisti, da sinistra a destra | dopo I4 | foto una alla volta |

Stima con i tempi misurati (0,5 s a richiesta, 0,3 s a copertina, ~0,15 s a foto):
- **tutti i testi a ~1,0-1,5 s**;
- **copertine dei riquadri entro ~2,7 s**;
- foto degli artisti entro ~3,5 s.

Nel caso peggiore (1 s a richiesta) i testi arrivano a 3 s.

- **Filtro JSON** di `recently-played` (`build_filter()`): servono solo `items[].played_at`, `items[].context.{type,uri}`, `items[].track.{name,uri,duration_ms}`, `track.artists[].{name,id}`, `track.album.{name,uri,images,artists[0].name}`.
- **Raggruppamento**: scorri gli ascolti dal più recente; chiave = `context.uri`, oppure `track.uri` se il contesto manca. Tieni il primo ascolto di ogni chiave (brano per l'`offset` e `played_at`). Massimo 50 contesti.
- **Ritorno alla Home entro 5 minuti**: tutto compare subito dalla memoria. Se `recently-played` ha più di 60 s, R1 riparte in silenzio, senza spinner, e si ridisegnano solo i riquadri che cambiano.
- Nient'altro si carica in anticipo: "Più ascoltati · Brani" e gli altri periodi partono solo quando si toccano.
- **Memoria immagini**: le copertine da 150 visibili insieme sono al massimo **4** sulla Home, più le 6 della fase 6 (pannelli dei dettagli): porta la cache da 150 a **10**. Serve anche una cache per le foto **da 80** degli artisti: **10** voci (5 visibili più margine). Le miniature da 64 restano 60.

---

## 6. Cosa cambia rispetto alla fase 6

| Dove | Cambiamento |
|---|---|
| Barra laterale §4.1 | la fascia `0,152 88×64` diventa la voce **Home** (icona casa 28); stati come le altre voci |
| Now playing §5.1, Orologio §5.2 | icona Home 64×64 a `536,0` |
| Orologio §5.2 | il pulsante primario diventa "Home"; nuovo suggerimento (§3.5) |
| Chip del dispositivo §4.15 | larghezza massima **320** (nome max 224 px): con 360 poteva toccare l'ora centrata a `x 350` |
| §3.6 immagini | nuova misura **80** (foto degli artisti nella Home); cache come al §5 |

Colori e caratteri **nessuno nuovo**.

## 7. Icone nuove

Tutte da Font Awesome 5 Solid, come le altre (`../fase6/CARATTERI_E_ICONE.md`):

| Icona | Codice | Aggiungere a | Uso |
|---|---|---|---|
| chevron-right | `0xF054` | `font_ic_20` | "Tutti ›" |
| home | `0xF015` | `font_ic_20` | pulsante "Home" dell'Orologio |
| home | `0xF015` | `font_ic_40` | Home vuota |

La casa a 28 è già in `font_ic_28`. Il cuore 40 (riquadro Brani piaciuti) e la persona 40 ci sono già.

## 8. Indice dei mockup

| File | Schermata |
|---|---|
| `01_home.png` | Home completa, con un contesto in riproduzione |
| `01b_home-caricamento.png` | Home al tocco (0 s): titoli e scheletro |
| `01c_home-parziale.png` | Home dopo ~1 s: recenti con 2 copertine arrivate, artisti in caricamento |
| `01d_home-errore.png` | Errore di rete |
| `01e_home-vuota.png` | Account nuovo |
| `02_recenti-tutti.png` | Ascoltati di recente, elenco completo |
| `03_piu-ascoltati-artisti.png` | Più ascoltati, Artisti, 4 settimane |
| `03b_piu-ascoltati-brani.png` | Più ascoltati, Brani, 6 mesi, con brano in riproduzione |
| `04_orologio.png` | Orologio con il pulsante Home |
| `05_now-playing.png` | Now playing con l'icona Home in alto |

## 9. Da verificare sul dispositivo

1. **Tempo di `recently-played?limit=50`** con il filtro. Se supera 1 s, chiedete `limit=20` per la Home e i 50 solo quando si apre "Tutti".
2. **`context` negli ascolti recenti**: controllate cosa arriva per Brani piaciuti (`spotify:user:…:collection`), per le radio e per gli ascolti senza contesto, e che la regola del §3.1 copra tutti i casi visti.
3. **Ripresa con `offset.uri`**: se il brano non è più nel contesto (playlist modificata), Spotify risponde 404. In quel caso riprovate senza `offset`.
4. **Foto degli artisti da 160**: se per qualche artista manca, usate 320/4. Verificate la nitidezza dei cerchi da 80.
5. **`uris` con 50 brani** per la classifica: verificate che il corpo della richiesta (~2 KB) passi senza problemi di RAM con la sessione TLS aperta. Se no, scendete a 20.
6. **Tre segmenti in 300 px**: "4 settimane" in `M16` deve stare nei suoi 124 px. Se il testo resta stretto, il fallback è "4 sett.".
7. **Ridisegno della Home**: al primo ingresso si ridisegnano 712×480 una volta, poi solo 150×150 o 80×80 per ogni immagine. Controllate i trattini con le copertine chiare, come in fase 6.
