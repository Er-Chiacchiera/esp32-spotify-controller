# Brief per Claude Design: Controller Spotify, fase 7 (Home personale)

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Come per la fase 6, le decisioni di design le prendi tu. A me servono specifiche precise in pixel da tradurre in LVGL 8.

La fase 6 è **realizzata e funziona** sulla scheda seguendo la tua `SPEC_FASE6.md`: barra laterale, Libreria, elenchi a pagine da 5 righe, dettagli, ricerca con tastiera e dispositivi. La fase 7 si appoggia allo stesso sistema: colori, caratteri, componenti e regole di §3-4 della SPEC restano validi. Riusali ovunque puoi. Se aggiungi o cambi qualcosa, dimmelo esplicitamente.

**Allegati.** Catture reali 800×480 dal framebuffer della scheda, in `fase6/schermate/`:
- `01_now_playing.png`
- `03_libreria.png`
- `04_playlist.png`
- `07_album.png`
- `10_artista.png`
- `12b_risultati.png`
- `13_dispositivi.png`

Inoltre i tuoi mockup della fase 6, in `fase6/mockup/`.

---

## 1. Cosa serve progettare

**Obiettivo della fase.** Una **Home personale** con la musica di Valerio:
- ascoltati di recente;
- artisti più ascoltati;
- brani più ascoltati;
- playlist e album in riquadri con la copertina.

**Test di accettazione (dal piano).** La Home si carica in **meno di 3 secondi**. La mia proposta di interpretazione:
- entro 3 s dal tocco ci sono tutti i testi e la struttura della pagina visibile;
- le copertine arrivano subito dopo, una alla volta.

Se il design permette di avere entro 3 s anche le copertine della prima vista, ancora meglio.

**Posto nella navigazione.** È già riservato: voce della barra laterale a `0,152 88×64`, tra play/pausa e Libreria (SPEC §1 e §4.1). Oggi quella fascia è vuota.

Schermate e stati da progettare:
1. **Home**: la schermata principale della sezione.
2. **Più ascoltati** con i tre periodi (vedi §2 per cosa significano davvero): come si sceglie il periodo e come si passa da artisti a brani.
3. **Ascoltati di recente**, se non sta tutto nella Home: elenco completo.
4. **Stati**:
   - caricamento della prima vista;
   - errore di rete con Riprova;
   - Home "vuota" (account nuovo: niente recenti né top);
   - riquadro con immagine non ancora arrivata.
5. **Azioni al tocco** per ogni tipo di voce. Come si comportano oggi gli elenchi della fase 6:
   - brano: parte subito;
   - pressione lunga: menu Riproduci / Aggiungi in coda;
   - album, playlist e artista: si apre il dettaglio, già esistente.

## 2. Dati disponibili da Spotify (regole 2026, Development Mode)

| Contenuto | Endpoint | Cosa arriva | Limiti e note |
|---|---|---|---|
| Ascoltati di recente | `GET /me/player/recently-played` | per ogni ascolto: brano (titolo, artisti, album, copertina 640/300/64, durata), ora dell'ascolto, **contesto** da cui è partito (playlist/album/artista: solo l'URI, **senza nome né immagine**) | max 50; nessun totale; si pagina all'indietro nel tempo. Lo stesso brano compare più volte se riascoltato. Solo brani (niente podcast) |
| Artisti più ascoltati | `GET /me/top/artists?time_range=…` | nome, foto (640/320/160 px) | max 50. **Niente generi, popolarità né follower** (tolti nel 2026) |
| Brani più ascoltati | `GET /me/top/tracks?time_range=…` | titolo, artisti, album, copertina 640/300/64, durata | max 50 |
| Playlist | `GET /me/playlists` | come in fase 6: nome, proprietario, immagine (anche mosaico da 60 px), numero di brani | quelle dell'utente in ordine di Spotify (di solito le più recenti in alto) |
| Album salvati | `GET /me/albums` | come in fase 6 | **risposta pesante**: ogni album porta con sé tutti i suoi brani, ~1 s ogni 10 album |

**I tre periodi** (`time_range`):
- `short_term`: circa ultime 4 settimane;
- `medium_term`: circa ultimi 6 mesi;
- `long_term`: **circa un anno**, secondo la documentazione di Spotify, non "da sempre".

Scegli tu le etichette. "4 settimane · 6 mesi · 1 anno" è più onesto di "sempre".

**Il nome del contesto dei recenti non c'è.** Per mostrare "Dalla playlist X" servirebbe una richiesta in più per ogni playlist o album diverso, quindi lenta. Meglio un design che non ne abbia bisogno. Al massimo lo mostriamo per i contesti che conosciamo già, cioè le playlist dell'utente.

Non esistono più (come in fase 6): consigli, "creati per te", nuove uscite, categorie, Daily Mix, brani popolari di un artista.

## 3. Vincoli tecnici (riassunto, dettagli nella SPEC §3 e nel brief della fase 6)

**Schermo**:
- niente zoom, rotazioni, trasparenze su aree grandi o slide tra schermate;
- cambio schermata istantaneo;
- scorrimento possibile solo in aree piccole: gli elenchi sono a **pagine**, swipe su/giù o ▲▼;
- un carosello orizzontale che scorre sarebbe costoso; uno a pagine (swipe = scatto alla pagina dopo, senza animazione) va bene.

**Rete: il punto decisivo per i 3 secondi.** Misurato oggi sulla scheda, con il Wi-Fi normale (segnale -60 dBm, la scheda sta lontana dal router):
- c'è **una sola connessione alla volta**, quindi le richieste si fanno **una dopo l'altra**, mai in parallelo;
- una richiesta di elenco impiega **0,3-1 s**;
- in 3 secondi stanno quindi **al massimo 3 richieste** per la prima vista. Il resto si carica dopo, o quando si apre la parte corrispondente;
- l'ordine conta: prima il contenuto in alto/a sinistra;
- tornando alla Home entro 5 minuti, i dati sono già in memoria e ricompare subito (come gli elenchi della fase 6).

**Immagini**:
- si scaricano **una alla volta**, dopo i testi;
- tempi misurati: 0,05-0,3 s per una miniatura da 64 px, ~0,3 s per un riquadro da 150 px;
- 6 riquadri da 150 px ≈ 2 s;
- dimensioni possibili senza ridimensionamenti arbitrari: **64** (dalla 64 di Spotify), **150** (300/2), **300**; per gli artisti 80 (640/8) e 160 (640/4), ritagliate al centro;
- oggi la memoria tiene **60 miniature da 64 px e 6 immagini da 150 px**. Se la Home ne vuole di più da 150, si può alzare (ognuna costa 45 KB di PSRAM, ce n'è in abbondanza): dimmi il numero massimo visibile insieme;
- segnaposto e regole come SPEC §3.6, artisti tondi compresi.

**Caratteri e icone**:
- Montserrat M16/M20/M24, SB20/SB24/SB30, icone 20/28/40 (elenco in `fase6/CARATTERI_E_ICONE.md`);
- c'è spazio per **1-2 dimensioni in più**: la memoria del programma è al 64%;
- le icone nuove si aggiungono senza problemi (Font Awesome 5 Solid). Elenca quelle che servono, a cominciare da quella della Home nella barra.

**Touch**: bersagli minimi 56-64 px, un dito, tocco / pressione lunga / swipe.

**Area utile**: tutto lo schermo meno la barra laterale, cioè **712×480 a partire da x 88**. Intestazione 712×64 se serve (SPEC §4.2).

## 4. Domande aperte (decidi tu)

- **Home come schermata di partenza?** Oggi, quando non suona niente, si vede l'Orologio (`fase6/mockup/02b_orologio.png`). Deve restare così, oppure la Home prende il suo posto, o l'Orologio diventa parte della Home?
- **Struttura.** Una sola pagina con poche "vetrine" (es. 1 fila di recenti + 1 di top artisti + 1 di riquadri), con "Vedi tutti" verso gli elenchi completi? Oppure schede (Recenti · Top · Raccolte)? Ricorda il limite di ~3 richieste per la prima vista.
- **Recenti**: brani singoli, oppure raggruppati per contesto ("hai ascoltato l'album X")? Togliamo i doppioni?
- **Periodo dei più ascoltati**: schede (componente 4.10 già pronto), chip, o altro? Quale periodo all'apertura?
- **Playlist e album a riquadri**: quanti, in che ordine, e se la Home li mescola o li tiene separati. Nella Libreria restano comunque in elenco.
- **Brano nei più ascoltati**: toccandolo parte solo quel brano, oppure parte la lista dei più ascoltati da quel brano in poi (come facciamo con i Brani piaciuti)?

## 5. Cosa ci serve da te

Stesso formato della fase 6, che ha funzionato benissimo:
1. **Mockup 800×480** di ogni schermata e stato della §1, compresa la barra laterale con la nuova voce Home attiva.
2. **Specifiche in pixel**:
   - posizioni e dimensioni (x, y, larghezza, altezza);
   - colori in esadecimale (meglio quelli della SPEC);
   - carattere e dimensione di ogni testo, con le regole di troncamento;
   - stati normale / premuto / attivo / disabilitato.
3. **Ordine di caricamento** della Home: quali richieste partono per prime e cosa compare in quale momento. Mi serve per stare nei 3 secondi.
4. **Componenti nuovi** (es. riquadro di griglia, fila di vetrina, selettore del periodo) e **modifiche a quelli esistenti**, se ce ne sono.
5. **Caratteri e icone** nuovi, se servono.
6. Una sezione **"Da verificare sul dispositivo"**, come la §9 della SPEC della fase 6.
