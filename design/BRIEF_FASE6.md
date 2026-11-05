# Brief per Claude Design: Controller Spotify, fase 6 (navigazione)

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Le decisioni su design e interfaccia le prendi tu. A Claude Code servono specifiche precise da tradurre in codice (LVGL 8 su ESP32-S3). Qui sotto trovi il contesto, i vincoli tecnici (alcuni sono rigidi e li abbiamo scoperti a nostre spese), i dati che Spotify mette a disposizione e cosa ci serve da te.

Allegato: `schermate/now_playing.png`, cattura reale 800×480 dello schermo attuale, presa dal framebuffer e non fotografata.

---

## 1. Il prodotto

- **Cos'è**: un telecomando/dashboard di Spotify da tenere sulla scrivania, accanto al computer. La scheda non riproduce audio: comanda il dispositivo che suona (PC, telefono, casse) tramite la Spotify Web API.
- **Chi lo usa**: Valerio, in italiano. Uso quotidiano da seduto, a 50-70 cm. Gesti rapidi con un dito, spesso mentre fa altro al computer.
- **Hardware**: display touch capacitivo da 5", **800×480 px in orizzontale** (circa 187 ppi: 48 px ≈ 6,5 mm). Un solo dito alla volta. Per ora niente case: vetro e cornice nera.
- **Lingua**: italiano. Gli accenti sono supportati nei caratteri attuali (Latin-1 + Latin Extended-A).

## 2. Cosa esiste già (fasi 1-5)

| Schermata | Quando si vede | Contenuto |
|---|---|---|
| **Now playing** (`schermate/now_playing.png`) | C'è qualcosa in riproduzione, anche in pausa | Copertina 300×300 a sinistra. A destra titolo (max 2 righe, font 30), artisti (24, grigio), album (16). Barra di avanzamento trascinabile con tempi. Indietro / play-pausa (cerchio verde 88 px) / avanti. Riga del volume. In alto: "In riproduzione su <dispositivo>" e l'ora. |
| **Orologio** | Spotify collegato ma niente in riproduzione | Ora grande (cifre 112 px), data, riga di stato Spotify ("Ciao, Valerio! Niente in riproduzione…"), riga Wi-Fi in basso, pulsante "Cambia Wi-Fi". |
| **Configura Wi-Fi** | Primo avvio senza rete | Istruzioni in 3 passi e QR code per collegarsi all'access point della scheda. |
| **Mi collego…** | Ricollegamento al Wi-Fi | Spinner e nome della rete. |

**Linguaggio visivo attuale**: l'ho deciso io (Claude Code) per far funzionare le cose, non è un design pensato. Sentiti libero di cambiarlo, anche sulle schermate già fatte.
- Colori: fondo `#121212`, superfici `#282828`, testo `#FFFFFF`, testo secondario `#B3B3B3`, accento `#1DB954` (verde Spotify), avvisi arancio `#FF9800`.
- Carattere: Montserrat Medium, alle dimensioni 16 / 20 / 24 / 30 px, più le cifre a 112 px.
- Icone: il set di simboli di LVGL (FontAwesome 5 Solid): play, pausa, avanti, indietro, volume, lista, casa, cerca, impostazioni, Wi-Fi, frecce, ecc. Le icone extra si possono aggiungere, generandole da FontAwesome o da un altro font icone.

## 3. Cosa serve progettare (fase 6)

**Obiettivo della fase**: dalla scheda si sfoglia la propria libreria e si fa partire un brano sul PC.
**Test di accettazione**: da una playlist o da un album parte un brano sul PC.

Schermate e flussi da progettare:
1. **Navigazione generale**: come si passa dal now playing alla libreria e si torna indietro. Serve un mini-player nelle altre schermate? Barra di navigazione fissa, pulsanti, gesti (lo swipe è disponibile)?
2. **Libreria**: punto d'ingresso verso Playlist, Brani piaciuti, Album, Artisti seguiti.
3. **Elenco playlist**, con apertura del **dettaglio playlist** (lista dei brani e un tasto per avviarla tutta).
4. **Brani piaciuti**: lista dei brani.
5. **Album salvati**, con apertura del **dettaglio album** (brani e tasto play).
6. **Artisti seguiti**, con apertura della **pagina artista**. Contiene solo gli album e i singoli dell'artista: nel 2026 Spotify ha tolto i "brani più popolari".
7. **Coda**: brano attuale e prossimi brani (circa 20).
8. **Ricerca**: **tastiera a schermo** in italiano (con accenti? tasto per cancellare, invio). I risultati sono divisi per tipo: brani, album, artisti, playlist.
9. **Scelta del dispositivo**: elenco con tipo (computer, telefono, cassa, TV…), quello attivo evidenziato, tocco per spostare la musica lì. Deve aprirsi anche quando un comando fallisce perché non c'è un dispositivo attivo (oggi mostriamo solo una scritta arancio).
10. **Stati per ogni lista**: caricamento, lista vuota, errore di rete con "riprova", fine della lista o "carica altri".

Per coerenza, tieni d'occhio anche la **fase 7** (home personale: ascoltati di recente, artisti e brani più ascoltati in 4 settimane / 6 mesi / sempre, playlist e album a riquadri). Non serve progettarla adesso, ma la navigazione della fase 6 deve avere già lo spazio per accoglierla.

## 4. Dati disponibili da Spotify (regole 2026, Development Mode)

| Schermata | Endpoint | Campi utili | Limiti |
|---|---|---|---|
| Playlist | `GET /me/playlists` | nome, proprietario, immagine (spesso 640/300/60 px, a volte mosaico), numero di brani, collaborativa sì/no, descrizione | 50 per pagina |
| Dettaglio playlist | `GET /playlists/{id}/items` | per brano: titolo, artisti, album, copertina dell'album, durata, data di aggiunta | **Solo per le playlist dell'utente o collaborative.** Le altre (es. playlist di Spotify o di amici) si possono avviare, ma l'elenco dei brani non c'è: serve un layout "senza lista" |
| Brani piaciuti | `GET /me/tracks` | titolo, artisti, album, copertina, durata, data di aggiunta | 50 per pagina |
| Album salvati | `GET /me/albums` | nome, artisti, copertina, anno, numero di brani | 50 per pagina |
| Dettaglio album | `GET /albums/{id}/tracks` | numero, titolo, artisti, durata (la copertina è quella dell'album) | 50 per pagina |
| Artisti seguiti | `GET /me/following?type=artist` | nome, foto, generi | 50 per pagina. Niente follower né popolarità |
| Pagina artista | `GET /artists/{id}/albums` | tipo (album / singolo / compilation / "appare in"), nome, copertina, data | **Niente top tracks** |
| Coda | `GET /me/player/queue` | brano attuale e circa 20 successivi | |
| Ricerca | `GET /search` | brani, album, artisti, playlist | **Max 10 risultati per tipo e per richiesta**, poi si pagina |
| Dispositivi | `GET /me/player/devices` | nome, tipo, attivo sì/no, volume, volume regolabile sì/no | |
| Avvio | `PUT /me/player/play` | avvia un contesto (playlist/album) da un certo brano, oppure una lista di brani | |
| Cambio dispositivo | `PUT /me/player` | sposta la riproduzione su un altro dispositivo | |

Non esistono più: categorie e "sfoglia", nuove uscite, consigli, brani popolari di un artista, profili e playlist di altri utenti, popolarità e follower.

## 5. Vincoli tecnici rigidi

**Prestazioni dello schermo**: è il punto più delicato. Lo schermo non ha memoria propria e la scheda gli ridisegna l'immagine di continuo, leggendo dalla stessa memoria (PSRAM) usata dal Wi-Fi. Se in un istante si chiede troppo, compaiono "trattini" lampeggianti. Ci è già successo con effetti che sembravano innocui. Quindi:
- **NO a zoom, rotazioni, sfocature e trasparenze su aree grandi.** Con lo zoom di una scritta lo schermo è sceso a 8 FPS e glitchava.
- **NO ad animazioni che ridisegnano tutto lo schermo a ogni fotogramma**: slide tra schermate, dissolvenze a schermo intero, parallasse. Il cambio schermata ideale è istantaneo. Animazioni piccole e localizzate (uno spinner, un tasto che si illumina, una barra) vanno bene.
- **Lo scorrimento delle liste è costoso**: ogni frame ridisegna tutta l'area che scorre. Si può fare, ma meglio in aree contenute; liste paginate o a "pagine" sono più sicure di uno scroll lunghissimo. Dimmi tu cosa preferisci e lo misuriamo.
- **Ombre grandi e sfumature su ampie superfici costano**. Angoli arrotondati, bordi e colori pieni costano poco.
- **Sfondi scuri aiutano**: eventuali difetti residui dello schermo non si vedono sul nero.
- **Colori a 16 bit (RGB565)**: le sfumature molto delicate fanno "scalini" visibili.

**Immagini**:
- Spotify dà le copertine solo a **640, 300 e 64 px** (60 px per alcune playlist). Ogni immagine va scaricata (0,5-3 s, dipende dal Wi-Fi) e decodificata, una alla volta.
- In una lista da 20 voci le miniature arrivano quindi una dopo l'altra, in diversi secondi. Serve un **segnaposto** e un layout che funzioni bene anche prima che arrivino. Più avanti le copertine verranno salvate su microSD.
- Le immagini si possono solo ridurre a 1/2, 1/4 o 1/8, oppure mostrare alle dimensioni originali (64 o 300 px). Niente ridimensionamenti arbitrari.

**Testo**:
- Titoli e nomi possono essere lunghissimi ("Pochette (feat. Noyz Narcos) - Remastered 2011"). Serve una regola di troncamento: 1 o 2 righe con "…".
- Lettere non latine (giapponese, coreano, cirillico…) **non si vedono** e compaiono come quadratini. Indicaci come gestirle.
- Si può usare **un solo carattere**, generato alle dimensioni che servono. Ogni dimensione in più costa 100-500 KB di memoria del programma, e ne restano circa 1,2 MB. Quindi **massimo 3-4 dimensioni o pesi in più** rispetto a quelli attuali. Si può cambiare famiglia (es. Inter, un Montserrat SemiBold o Bold) se ne vale la pena: dicci quale e con che licenza.

**Touch**:
- Un dito, niente hover. Tocco, pressione lunga, trascinamento e swipe in 4 direzioni.
- Bersagli minimi consigliati **56-64 px** (≈8 mm). Il touch è preciso, ma la scheda sta in piedi sulla scrivania e si tocca "al volo".

**Rete**:
- Ogni richiesta a Spotify richiede 0,2-0,5 s, ma con Wi-Fi debole anche secondi o un errore. Servono stati di caricamento ed errore chiari, e una risposta immediata al tocco: si evidenzia subito, i dati arrivano dopo.
- Meglio poche richieste per schermata: c'è un limite di frequenza.

**Libreria grafica**: LVGL 8.4. Componenti pronti: liste, pulsanti, slider, tab, tastiera a schermo (personalizzabile), roller, immagini, etichette, layout flex/griglia, spinner, finestre di dialogo. Componenti più complessi si possono costruire, sempre restando dentro i vincoli di prestazione qui sopra.

## 6. Cosa ci serve da te

1. **Mappa della navigazione**: schermate, come ci si arriva, come si torna indietro, dove si trova la ricerca e dove la scelta del dispositivo.
2. **Mockup a 800×480 px** di ogni schermata della sezione 3, compresi gli stati di caricamento, vuoto ed errore principali. Se vuoi, rivedi anche Now playing e Orologio per renderli coerenti.
3. **Specifiche implementabili**, per ogni schermata e componente:
   - posizioni e dimensioni in **pixel** (x, y, larghezza, altezza), margini e spaziature;
   - colori in **esadecimale**;
   - carattere e **dimensione in px** per ogni testo, con le regole di troncamento;
   - stati dei componenti: normale, premuto, selezionato/attivo, disabilitato;
   - animazioni, se ce ne sono: cosa si muove, quanto dura, che area occupa.
4. **Componenti riutilizzabili**: riga di lista (brano, playlist, album, artista), riquadro di griglia, intestazione con "indietro", barra di navigazione o mini-player, tastiera, voce dispositivo.
5. **Elenco dei caratteri e delle icone** che servono, per dimensione e peso, così li genero tutti in una volta.

Il formato ideale per Claude Code: le immagini dei mockup e un documento (markdown va benissimo) con le misure in tabella. Le misure in pixel sullo schermo 800×480 sono quelle che usa direttamente.

## 7. Domande aperte (decidi tu)

- Libreria e now playing: schermate separate, schermata divisa, oppure mini-player sempre visibile?
- Liste o griglie per playlist e album?
- Barra di navigazione fissa (a sinistra o in basso) o una schermata "hub"?
- Tastiera: QWERTY italiana completa con accenti, o ridotta con tasti più grandi?
- Dove va la scelta del dispositivo: icona fissa, tocco sulla scritta "In riproduzione su…", o altro?
- Cosa succede toccando un brano in una lista: parte subito, oppure si apre un menu (riproduci, aggiungi in coda)?
