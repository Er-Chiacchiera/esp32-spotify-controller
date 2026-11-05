# Specifica · Home generale e giorni

Risposta a `design/BRIEF_HOME_GIORNI.md`. Si aggiunge a `fase6/SPEC_FASE6.md`, `fase7/SPEC_FASE7.md`, `meteo/SPEC_METEO.md`, `sole_aria/SPEC_SOLE_ARIA.md` e `colore_copertina/SPEC_COLORE.md`: quello che qui non è nominato resta com'è. I nomi dei colori (`TEXT_2`, `SURFACE`…) sono quelli della SPEC 6 §3.1, più `WX_SUN`, `WX_RAIN`, `AQ_*`, `POL_*` delle SPEC del meteo e dell'aria.

Ho tenuto Home e giorni in **una sola SPEC**, perché le due parti si toccano in un punto: la barra laterale e l'intestazione della pagina Meteo. Si possono però implementare in due tempi: prima la Home (§3-§7), poi i giorni (§8).

**In breve**
- **La Home è un cruscotto a riquadri** e prende il posto dell'Orologio. Contiene:
  - l'ora grande, come oggi;
  - un riquadro **Spotify** con copertina, brano e i comandi ⏮ ⏯ ⏭;
  - un riquadro **Meteo** (adesso + prossime ore + domani);
  - tre **riquadri piccoli**: sole, aria e luna (o pollini).
- **Now playing non si apre più da solo** quando parte la musica: la Home lo mostra già. Si apre toccando il riquadro Spotify o la copertina nella barra.
- **Barra laterale:** in tutte le pagine ha la voce **Home** in cima alla navigazione e il tasto **Avanti** sotto play/pausa. La Coda esce dalla barra e resta in Now playing.
- **Nomi:** la Home della scheda si chiama **Home** (casa); la Home di Spotify della fase 7 diventa **Ascolti** (cuffie).
- **Giorni:** nella pagina Meteo, a destra delle schede, un **selettore ‹ Oggi ›** cambia giorno. Si può anche scorrere col dito. Va da 7 giorni fa a 7 giorni avanti, e il giorno scelto resta uguale cambiando scheda.
- Servono solo 5 icone in più. Nessun carattere o colore nuovo.

Mockup 800×480 in `home_giorni/mockup/`, disegnati nel canvas alla pagina "Home e giorni" (https://claude.ai/artifact/2BmkheovXWkQTH5Pqxk8tw):

| File | Cosa mostra |
|---|---|
| `01_home-musica.png` | Home con la musica: Valerio's MacBook Pro, malibu · sangiovanni |
| `02_home-senza-musica.png` | Home la sera, niente in riproduzione: pulsanti Ascolti e Cerca nel riquadro Spotify; "Alba domani" |
| `03_home-in-arrivo.png` | Home all'accensione: scheletro dei riquadri |
| `04_home-senza-rete.png` | Wi-Fi assente: Spotify non collegato, meteo non disponibile, resta solo la luna |
| `05_home-premuto.png` | musica in pausa, riquadro Aria premuto |
| `06_now-playing.png` | Now playing con le icone nuove in alto (Home, Ascolti) |
| `07_ascolti.png` | Ascolti (la Home di Spotify della fase 7) con la barra laterale nuova |
| `08_impostazioni.png` | Impostazioni: Wi-Fi e Informazioni |
| `09_meteo-oggi.png` | scheda Meteo di oggi con il selettore dei giorni |
| `10_meteo-domani.png` | scheda Meteo di domani: 12 orari ogni 2 ore |
| `11_meteo-passato.png` | scheda Meteo di lunedì 2 (passato): pioggia caduta in mm |
| `12_sole-luna-futuro.png` | scheda Sole e luna di giovedì 12, ultimo giorno disponibile (› spento) |
| `13_aria-futuro.png` | scheda Aria di sabato 7: massimo del giorno, ora per ora |
| `14_aria-passato.png` | scheda Aria di martedì 3 (passato) |
| `15_aria-oltre.png` | scheda Aria di mercoledì 11: oltre i 4 giorni di previsione |
| `16_giorno-in-arrivo.png` | giorno in arrivo (domenica 8, scheda Meteo) |
| `17_giorno-errore.png` | giorno non arrivato (lunedì 9, scheda Meteo) con Riprova |

---

## 1. Risposte alle domande del brief

### La Home

**1. Forma: cruscotto a riquadri.** Sono d'accordo con il parere tecnico del brief. Un menu di icone alla CarPlay aggiunge un tocco prima di qualsiasi cosa, e su uno schermo che si guarda di sfuggita la cosa più utile è vedere già l'informazione. Ogni riquadro mostra il suo dato e si tocca per aprire la sua pagina. La Home ha quattro zone (`01`):
- l'ora a sinistra;
- il riquadro Spotify a destra;
- il riquadro Meteo, largo quanto lo schermo;
- tre riquadri piccoli in basso.

**2. L'Orologio diventa la Home.** L'ora resta **grande** (112 px, stessa misura e stesso carattere): è la cosa che si guarda più spesso e deve leggersi da lontano anche con la luce al 15%. Rispetto all'Orologio spariscono:
- i pulsanti "Home" e "Cerca" in basso: le stesse destinazioni sono già fra le icone in alto, e nel riquadro Spotify quando non suona niente;
- il nome del Wi-Fi e "Cambia Wi-Fi": vanno in **Impostazioni** (icona ingranaggio in alto, §6);
- la città e la temperatura percepita: la città resta nell'intestazione della pagina Meteo; la percepita non si mostra più.

**3. Now playing e Home: la Home resta visibile.** Oggi la musica apre Now playing da sola. Con la Home nuova, il riquadro Spotify mostra già brano, copertina e comandi, quindi uno schermo che cambia da solo non serve. Inoltre disturba: chi sta guardando il meteo si ritroverebbe in Now playing senza averlo chiesto.
- Now playing si apre **toccando** il riquadro Spotify (o la copertina nella barra laterale).
- Con la musica **in pausa** non cambia niente: il riquadro mostra il brano con ▶.
- All'accensione si parte sempre dalla Home.
- Dopo 2 minuti senza tocchi (quando la luce scende al 15%) ogni schermata torna alla Home. Fa eccezione Now playing mentre la musica suona: chi lo ha aperto lo vuole tenere.

**4. Barra di Spotify nelle pagine: sì, nella barra laterale.** Una barra orizzontale in basso costerebbe una riga di elenco su cinque e obbligherebbe a ridisegnare tutte le pagine della fase 6. La barra laterale ha già copertina e play/pausa: aggiungo **Avanti** sotto play/pausa (§4). Così i comandi stanno **sempre nello stesso posto**, in alto a sinistra, in ogni pagina, compreso il meteo (il problema segnalato da Valerio). Senza musica, play e avanti sono spenti e la copertina è il segnaposto (come oggi).

**5. Nomi.**
- **Home**, con l'icona casa: la Home della scheda.
- **Ascolti**, con l'icona cuffie: la Home di Spotify della fase 7, cioè "Ascoltati di recente" e "Più ascoltati". Il nome dice cosa c'è dentro e non si confonde.

**6. Barra laterale:** voce **Home** in cima alla navigazione. Restano le voci di Spotify: Ascolti, Libreria, Cerca, Dispositivi. **La Coda esce** dalla barra e resta come icona in Now playing: è di sola lettura e riguarda il brano che suona, quindi lì è al suo posto. Le voci passano da 64 a 56 px di altezza, per fare posto ad Avanti e Home (§4).

**7. Funzioni future.** Una funzione nuova si aggiunge in tre modi, senza ridisegnare la Home:
1. un **riquadro piccolo** in basso, con le stesse regole di priorità di sole/aria/luna (§3.5). Per esempio "Timer 4:32" o "Prossimo evento 15:00";
2. una **pagina con la barra laterale**, aperta dal suo riquadro, con la voce Home accesa. Se ha più viste usa le schede come la pagina Meteo;
3. le **impostazioni** della funzione diventano una riga in Impostazioni (§6).

Se un giorno i riquadri piccoli non bastano, la riga si può trasformare in una pagina di "app" a riquadri. Oggi non serve.

**8. Stati:** §3.6.

### I giorni

**1. Come si cambia giorno:** frecce **‹ ›** nell'intestazione, a destra delle schede, più lo **scorrimento col dito** a sinistra e a destra sul contenuto. Ho scartato due alternative:
- una fila di giorni costerebbe 48 px di altezza, e le schede sono piene fino a y 464;
- un calendario è troppo per 15 giorni.

**2. Dove si vede e come si torna a oggi:** fra le due frecce c'è il nome del giorno ("Oggi", "Domani", "Sabato 7"). Su un giorno diverso da oggi, sotto il nome compare **"Torna a oggi"** in verde e il nome diventa un pulsante che riporta a oggi (§8.1). Uscendo dalla pagina e rientrando si riparte sempre da oggi.

**3. Fin dove:** da **7 giorni fa a 7 giorni avanti**, uguale per le tre schede. Oltre 7 giorni la previsione è poco affidabile, e il passato più lontano è una curiosità che non vale un intervallo diverso per scheda. L'aria si ferma a 4 giorni avanti: dal quinto la scheda Aria lo dice (§8.6, `15`).

**4. Cosa mostra ogni scheda per un altro giorno:** §8.3-§8.5. In breve:
- **Meteo:** la giornata intera, 12 orari ogni 2 ore, più quel giorno e il seguente;
- **Sole e luna:** senza il segno "adesso", con la differenza di luce **rispetto a oggi**;
- **Aria:** il **massimo del giorno** al posto di "adesso", con le 24 ore di quel giorno.

**5. Il giorno resta uguale cambiando scheda:** sì. Meteo di sabato → Sole e luna di sabato → Aria di sabato.

**6. Stati:** giorno in arrivo, errore, aria oltre i 4 giorni, passato: §8.6.

---

## 2. Navigazione

```
Home ─┬─ riquadro Spotify ─────────── Now playing
      ├─ riquadro Meteo ───────────── pagina a schede, Meteo, Oggi
      ├─ riquadro Sole / Luna ──────── pagina a schede, Sole e luna, Oggi
      ├─ riquadro Aria / Pollini ───── pagina a schede, Aria, Oggi
      ├─ icone in alto: Ascolti · Libreria · Cerca · Impostazioni
      └─ chip del dispositivo ──────── Dispositivi
Barra laterale: [copertina] [play] [avanti] — [HOME] [Ascolti] [Libreria] [Cerca] [Dispositivi]
```

| Da | Tocco | Va a |
|---|---|---|
| Accensione | — | **Home** |
| Home | chip del dispositivo | Dispositivi |
| Home | icone Ascolti / Libreria / Cerca | la sezione, alla radice (voce accesa nella barra) |
| Home | icona Impostazioni | Impostazioni (§6) |
| Home | riquadro Spotify, fuori dai pulsanti | Now playing |
| Home | ⏮ / ⏯ / ⏭ nel riquadro Spotify | comando; si resta sulla Home |
| Home | "Ascolti" / "Cerca" (niente in riproduzione) | Ascolti / Cerca |
| Home | riquadro Meteo | pagina a schede, scheda **Meteo**, Oggi |
| Home | riquadro Sole o Luna | pagina a schede, scheda **Sole e luna**, Oggi |
| Home | riquadro Aria o Pollini | pagina a schede, scheda **Aria**, Oggi |
| Now playing | icona Home | Home |
| Now playing | icone Ascolti / Libreria / Cerca / Coda | la sezione |
| Now playing | chip | Dispositivi |
| Barra laterale | copertina | Now playing; **Home** se non c'è niente in riproduzione |
| Barra laterale | Avanti | `POST /me/player/next`; si resta dove si è |
| Barra laterale | voce Home | Home (la pila si azzera) |
| Barra laterale | altre voci | la sezione, alla radice (come in fase 6) |
| Pagina a schede | ‹ / › oppure scorrimento a destra / sinistra | giorno prima / dopo, sulla stessa scheda |
| Pagina a schede | nome del giorno (solo se non è oggi) | Oggi |
| Pagina a schede | una scheda | quella scheda, **sullo stesso giorno** |
| Pagina a schede | freccia indietro | Home |
| Impostazioni | Cambia Wi-Fi | Configura Wi-Fi (il portale di oggi) |
| Impostazioni | freccia indietro | Home |
| Qualsiasi schermata | 2 minuti senza tocchi | Home, tranne Now playing mentre la musica suona |
| Qualsiasi comando con 404 | automatico | Dispositivi (`13b`, come in fase 6) |

Regole:
- **Voce accesa nella barra** = sezione da cui è partita la pila (regola della fase 7). La pagina Meteo e Impostazioni si aprono dalla Home, quindi accendono **Home**. Le schermate della fase 7 accendono **Ascolti**.
- Cambiare giorno o scheda **non aggiunge passi** alla pila indietro.
- Nella pagina a schede lo scorrimento verso destra **cambia giorno e non torna indietro**: è un'eccezione alla SPEC 6 §1, solo per questa pagina. Si torna indietro con la freccia.
- Now playing non si chiude più da solo quando la musica si ferma: resta in pausa finché si esce o scattano i 2 minuti.
- Tutti i cambi di schermata e di giorno sono istantanei, senza animazioni.

---

## 3. Home — `01`-`05`

Schermata intera, **senza barra laterale**, come oggi l'Orologio e Now playing. Fondo `BG`.

### 3.1 Barra in alto

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Chip del dispositivo | 16,8 | auto × 48, max 320 | SPEC 6 §4.15 e SPEC 7 §6 (nome max 224) |
| Ascolti | 536,0 | 64×64 | `ibtn`, icona cuffie 28 `TEXT_2` |
| Libreria | 600,0 | 64×64 | `ibtn`, libro 28 |
| Cerca | 664,0 | 64×64 | `ibtn`, lente 28 |
| Impostazioni | 728,0 | 64×64 | `ibtn`, ingranaggio 28. Wi-Fi non collegato: pallino `WARN` 10×10 a `772,14` (`04`) |

### 3.2 Ora e data

| Elemento | x,y | Stile |
|---|---|---|
| Ora | 40,60 | `CLOCK112 TEXT`, allineata a sinistra, interlinea 120 |
| Data | 44,184 | `M20 TEXT_2`, L 340: "Giovedì 5 novembre" |

"00:00", il caso più largo, misura 324 px e finisce a x 364, prima del riquadro Spotify (x 400).

### 3.3 Riquadro Spotify — `400,72` 360×152

Fondo `SURFACE`, raggio 12. Premuto (fuori dai pulsanti): `SURFACE_PRESSED`. Coordinate relative al riquadro.

**Con un brano (in riproduzione o in pausa)** (`01`, `05`):

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Copertina | 16,16 | 64×64 | box 64 (la stessa miniatura della barra laterale, già in memoria) |
| Titolo | 96,18 | L 248 | `SB20 TEXT`, 1 riga, "…" |
| Artisti | 96,46 | L 248 | `M16 TEXT_2`, 1 riga, "…" |
| Precedente | 84,88 | 64×56 | icona 28 `TEXT`; premuto: cerchio 48 `RAISED` dietro l'icona |
| Play/pausa | 152,88 | cerchio 56 | fondo `TEXT`, icona 28 `#000000` (pausa se suona, play se è in pausa); premuto `TEXT_2` |
| Avanti | 212,88 | 64×56 | come Precedente |

- Il trio è centrato sul riquadro (centro di play a x 180).
- Dopo il tocco, play/pausa mostra lo spinner 20 al posto dell'icona finché arriva la risposta (SPEC 6 §3.4).
- Il premuto dei pulsanti usa `RAISED` e non `PRESSED`: sul fondo `SURFACE` il `PRESSED` non si vedrebbe.
- Il dispositivo è già scritto nel chip in alto, quindi il riquadro non lo ripete.

**Niente in riproduzione** (`02`): `/me/player` vuoto o senza brano.

| Elemento | x,y | Stile |
|---|---|---|
| Segnaposto | 16,16 | 64×64 `PRESSED`, nota 20 `PLACEHOLDER_FG` |
| Titolo | 96,18 | `SB20 TEXT`: "Niente in riproduzione" (231 px) |
| Sotto | 96,46 | `M16 TEXT_2`: "Scegli cosa ascoltare" |
| Ascolti | 16,88 | `bp sm` 156×48, icona cuffie 20 + "Ascolti" |
| Cerca | 188,88 | `bs sm` 156×48, lente 20 + "Cerca" |

In questo stato il riquadro non si tocca: rispondono solo i due pulsanti.

**Spotify non collegato** (`04`): stesso segnaposto, titolo e motivo; niente pulsanti, il riquadro non si tocca.

| Elemento | x,y | Stile |
|---|---|---|
| Titolo | 96,18 | `SB20 TEXT`: "Spotify non collegato" |
| Motivo | 96,46, L 248 | `M16 TEXT_2`, 1 riga |
| Nota | 16,104, L 328 | `M16 TEXT_3`, 1 riga |

| Caso | Motivo | Nota |
|---|---|---|
| Wi-Fi non collegato | "Wi-Fi non collegato" | "Riprovo da solo appena torna la rete." |
| Rete presente, Spotify non risponde | "Spotify non risponde" | "Riprovo tra poco." |
| Accesso scaduto (rinnovo token rifiutato) | "Accesso scaduto" | "Rifai il login dal computer." |

Sono gli stessi casi che il firmware oggi scrive sull'Orologio al posto dei pulsanti (40,394). I testi possono restare quelli già in uso, purché stiano nelle misure.

**In arrivo** (`03`), dall'accensione alla prima risposta di `/me/player`: scheletro fermo.
- quadrato 64×64 `RAISED` a 16,16;
- barre a 96,22 (180×18, `RAISED`) e a 96,50 (120×14, `SURFACE_PRESSED`), raggio 8;
- "Mi collego a Spotify…" `M16 TEXT_3` a 16,104.

### 3.4 Riquadro Meteo — `40,236` 720×120

Fondo `SURFACE`, raggio 12. Premuto: `SURFACE_PRESSED`. Il tocco apre la pagina a schede sulla scheda Meteo, oggi. Riunisce in un solo riquadro il blocco "adesso" e la fascia dell'Orologio (SPEC_METEO §3.2-§3.3), con gli stessi dati e le stesse regole (icone, giorno e notte, temperature negative, pioggia solo dal 20%).

**Zona Adesso** (x 0-312, coordinate relative al riquadro):

| Elemento | x,y | Stile |
|---|---|---|
| Icona + temperatura | 20,10, alta 72 | riga: icona `font_wx_56` (sole in `WX_SUN`), 12 px, temperatura `font_temp_64 TEXT` |
| Max | destra a 296, y 16, L 88 | `M16 TEXT_2`: "Max 18°" |
| Min | destra a 296, y 38, L 88 | `M16 TEXT_2`: "Min 9°" |
| Pioggia oggi | destra a 296, y 60 | goccia 20 + 4 px + "90%", in `WX_RAIN`, solo se ≥ 20% |
| Condizione | 20,86, L 276 | `M20 TEXT`, 1 riga, "…" ("Temporale con grandine" misura 248) |

**Separatori:** 1×88 `RAISED` a 312,16; 1×72 `RAISED` a 632,24.

**Celle delle ore:**
- 4 celle larghe 80 a x = 312 + i·80, con centro a 352, 432, 512 e 592;
- orari a **+2, +4, +6 e +8 ore** dall'ora in corso (prima erano 5, fino a +10);
- cella **Domani** a x 632, larga 88, con centro a 676.

Righe di ogni cella, centrate:

| Riga | y | Stile |
|---|---|---|
| Etichetta | 14 | `M16`: ore in `TEXT_2` ("11:00"), "Domani" in `TEXT` |
| Icona (+ pioggia) | 42, alta 28 | icona 28; se la pioggia è ≥ 20%, 6 px e "40%" `M16 WX_RAIN`; gruppo centrato |
| Temperatura | 76 | `M20 TEXT`: "16°"; per Domani "12°/15°" |

Caso più largo nella zona Adesso: nuvola (70 px) e "-12°". La temperatura finisce intorno a x 240, mentre la colonna Max/Min comincia a x 232: in Italia non capita. Se capita, nascondi Max e Min.

**Stati** (stesse soglie della SPEC_METEO §3.4):

| Stato | Resa | Tocco |
|---|---|---|
| In arrivo (`03`) | scheletro fermo. Adesso: cerchio 56 a 20,18 e barra 100×48 a 88,22 (`RAISED`); barre 72×16 a 224,18 (`RAISED`) e a 224,40 (`SURFACE_PRESSED`); "Meteo in arrivo…" `M16 TEXT_3` a 20,88. Ogni cella, con centro cx: barra 48×14 a cx−24,18, cerchio 28 a cx−14,46 (`RAISED`), barra 40×16 a cx−20,84 (`SURFACE_PRESSED`). Separatori come sopra. | no |
| Normale | come sopra | sì |
| Dato vecchio | come normale; in basso a destra della zona Adesso (allineato a destra a x 296, y 88) icona avviso 20 `WARN` + 4 px + l'ora del dato `M16 TEXT_2` ("19:10"). La condizione si accorcia a L 180. | sì |
| Errore (`04`) | il riquadro resta, vuoto tranne: nuvola `font_wx_56` `DISABLED_FG` a 20,32; "Meteo non disponibile" `M20 TEXT_2` a 100,34; "Riprovo tra 5 minuti" `M16 TEXT_3` a 100,62 ("Riprovo tra poco" se l'intervallo è un altro) | no |

### 3.5 Riquadri piccoli — 3 × 232×96, y 368

Posti a x **40, 284, 528**, distanza 12. Fondo `SURFACE`, raggio 12; premuto `SURFACE_PRESSED` (`05`). Coordinate relative al riquadro.

| Elemento | x,y | Stile |
|---|---|---|
| Icona | 16,15 | icona 20 (o immagine della luna 20×20) |
| Etichetta | 44,14 | `M16 TEXT_2` |
| Valore | 16,40, L 200 | `SB20 TEXT`, 1 riga |
| Dettaglio | 16,66, L 200 | `M16 TEXT_3`, 1 riga |

| Riquadro | Icona | Etichetta | Valore | Dettaglio | Apre |
|---|---|---|---|---|---|
| Sole | sun `WX_SUN` | "Alba" / "Tramonto" / "Alba domani" | "16:59" | ore di luce di quel giorno: "luce 10 h 12 min" | Sole e luna |
| Aria | wind, colore della fascia | "Aria" | giudizio con l'iniziale maiuscola: "Moderata", "Molto scadente" | "indice 42" | Aria |
| Pollini | spa, colore del livello | "Pollini" | "Alti" / "Medi" | i due tipi con più grani a quel livello, separati da ", ": "Olivo, graminacee" | Aria |
| Luna | fase 20×20 | "Luna" | nome della fase: "Falce calante" | "illuminata al 22%" | Sole e luna |

- **Priorità e regole di comparsa: invariate** rispetto alla riga dell'Orologio (SPEC_SOLE_ARIA §3.2). L'ordine è Sole > Aria > Pollini (solo Media o Alta) > Luna, al massimo 3 riquadri, riempiti da sinistra.
- I posti vuoti **non si disegnano**: niente fondo e niente tocco (`04`: resta solo la Luna, a sinistra).
- L'evento del sole segue le stesse regole: prima dell'alba "Alba", fra alba e tramonto "Tramonto", dopo il tramonto "Alba domani" con l'alba di domani.
- Misure: "Gibbosa crescente" in `SB20` misura 190 px, "Molto scadente" 159: entrano in 200.
- **In arrivo** (`03`): finché meteo e aria non hanno fatto il primo tentativo, tre riquadri con lo scheletro e nessun tocco. Barre a 16,16 (100×16, `RAISED`), a 16,44 (140×20, `RAISED`) e a 16,72 (110×12, `SURFACE_PRESSED`).

### 3.6 Stati della Home

| Situazione | Chip | Riquadro Spotify | Meteo | Riquadri piccoli |
|---|---|---|---|---|
| Accensione (`03`) | "Nessun dispositivo attivo" | in arrivo | in arrivo | scheletro |
| Musica (`01`) | dispositivo attivo (verde) | brano, ⏸ | normale | Sole · Aria · Luna |
| Pausa (`05`) | dispositivo attivo (verde) | brano, ▶ | normale | normale |
| Niente in riproduzione (`02`) | "Nessun dispositivo attivo" | Ascolti / Cerca | normale | normale |
| Senza Wi-Fi (`04`) | "Nessun dispositivo attivo" | non collegato | errore (o dato vecchio, finché ha meno di 6 h) | solo la Luna; ingranaggio con il pallino |

Il caso "nessun dispositivo attivo, ma c'era un brano" equivale a "niente in riproduzione". Se poi il comando trova un 404, si apre Dispositivi come oggi.

### 3.7 Ridisegni

| Cosa | Quando |
|---|---|
| Ora | ogni minuto (come oggi) |
| Riquadro Spotify | cambio di brano, play/pausa, cambio di stato; **niente barra di avanzamento**, quindi nessun ridisegno al secondo |
| Copertina 64 | all'arrivo della miniatura (64×64) |
| Riquadro Meteo | al cambio d'ora e all'arrivo di dati nuovi |
| Riquadri piccoli | come la riga dell'Orologio (SPEC_SOLE_ARIA §3.4) |

Usare `ui_set_text()` per tutte le scritte. Al primo ingresso si ridisegna tutto lo schermo una volta.

---

## 4. Barra laterale nuova — 88×480, fondo `RAIL`

Vale in tutte le schermate che oggi hanno la barra (fase 6, fase 7, pagina Meteo) e in Impostazioni. Sostituisce la tabella della SPEC 6 §4.1. Si vede in `07`-`17`.

| Elemento | Area di tocco | Contenuto | Stati |
|---|---|---|---|
| Copertina | 0,0 88×80 | copertina 64×64 a `12,8` | come oggi; niente in riproduzione: segnaposto, e il tocco porta alla Home |
| Play/pausa | 0,80 88×56 | cerchio 52 a `18,82`, icona 28 | come oggi |
| **Avanti** | 0,136 88×56 | step-forward 28 `TEXT`, centrata (a `30,150`) | premuto: cerchio 48 `PRESSED` dietro l'icona; disabilitato (niente in riproduzione): icona `DISABLED_FG_2`; in corso: spinner 20 |
| Separatore | — | 32×1 `RAISED` a `28,196` | — |
| **Home** | 0,200 88×56 | casa 28 | voce come le altre |
| **Ascolti** (ex Home) | 0,256 88×56 | cuffie 28 | voce |
| Libreria | 0,312 88×56 | libro 28 | voce |
| Cerca | 0,368 88×56 | lente 28 | voce |
| Dispositivi | 0,424 88×56 | icona del tipo di dispositivo | come oggi; pallino `WARN` a `52,436` |

- **Voce**: normale `TEXT_2`; accesa `TEXT` + barretta `ACCENT` 4×32 a x 0, **y = cima della voce + 12**; premuta: fondo `#1F1F1F`.
- La **Coda non è più nella barra**: si apre dall'icona in Now playing (§5).
- Dove la barra c'è, ma la pagina non è di Spotify (Meteo, Impostazioni), è accesa la voce **Home**.

---

## 5. Now playing — `06`

Tutto come SPEC 6 §5.1, SPEC 7 §3.6 e SPEC_COLORE, tranne le icone in alto, che diventano 5:

| Icona | x,y | Va a |
|---|---|---|
| Home (casa) | 472,0 | Home |
| Ascolti (cuffie) | 536,0 | Ascolti |
| Libreria | 600,0 | Libreria |
| Cerca | 664,0 | Cerca |
| Coda | 728,0 | Coda |

- Le icone sono `ibtn` 64×64 da 28 px, nel colore dei toni della copertina come le altre (SPEC_COLORE).
- Il chip, largo al massimo 320, finisce a x 336; l'ora centrata occupa x 350-450: la prima icona a x 472 non tocca niente.
- Now playing **non si apre e non si chiude da solo** (§1, domanda 3).

---

## 6. Impostazioni — `08`

Barra laterale con la voce Home accesa. Freccia indietro a `88,0` (torna alla Home); titolo "Impostazioni" `SB24` a `156,17`.

Ogni riga è 712×80, a partire da `88,64`, una sotto l'altra. Coordinate relative alla riga:

| Elemento | x,y | Stile |
|---|---|---|
| Cerchio dell'icona | 16,12 | 56×56 `SURFACE`, icona 28 `TEXT_2` (come in Dispositivi) |
| Titolo | 88,15 | `SB20 TEXT` |
| Sotto | 88,43 | `M16 TEXT_2`, 1 riga, "…" |
| Azione (facoltativa) | 512,16 | `bs sm` 176×48 |

| Riga | Icona | Titolo | Sotto | Azione |
|---|---|---|---|---|
| Wi-Fi | wifi | "Wi-Fi" | "CasaRibaldi · segnale buono / discreto / debole" (oltre −65 dBm buono, fra −65 e −78 discreto, sotto −78 debole); senza rete: "Non collegato" | "Cambia Wi-Fi", con bordo `#535353` come l'ex pulsante dell'Orologio → Configura Wi-Fi |
| Informazioni | info-circle | "Informazioni" | "Firmware 3f2a9c1e0 · IP 192.168.1.40" (l'impronta di 9 caratteri usata dagli aggiornamenti via Wi-Fi) | — (non si tocca) |

Le impostazioni future (luce dello schermo, città del meteo…) sono righe in più, nello stesso formato. Con più di 5 righe si passa alle pagine della SPEC 6 §3.7.

---

## 7. Ascolti (ex Home di Spotify) — `07`

Contenuto e comportamento invariati (SPEC 7 §3). Cambiano solo:
- il **nome**, in Ascolti;
- l'**icona**: cuffie al posto della casa;
- il **posto nella barra**: `0,256`.

La regola "Toccare Home nella barra riporta sempre alla Home, all'inizio" della SPEC 7 §2 vale ora per Ascolti.

---

## 8. I giorni (pagina a schede)

### 8.1 Intestazione con il selettore — `09`-`17`

Freccia indietro e schede come nella SPEC_SOLE_ARIA §4.1 (Meteo 152, Sole e luna 256 largo 144, Aria 400). Il **testo a destra è tolto** e al suo posto c'è il selettore:

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| ‹ Giorno prima | 512,0 | 64×64 | chevron-left 28 `TEXT`; premuto: cerchio 48 `PRESSED`; spento (−7 giorni): `DISABLED_FG_2`, non si tocca |
| Nome del giorno | 576,4 | 136×56, raggio 12 | due righe centrate: riga 1 `SB20 TEXT` a y 8, riga 2 `M16` a y 34 |
| › Giorno dopo | 712,0 | 64×64 | chevron-right 28, come ‹; spento a +7 giorni (`12`) |

**Riga 1:** "Oggi", "Domani", "Ieri"; per gli altri giorni, giorno della settimana + numero: "Sabato 7", "Mercoledì 11". Il più largo, "Mercoledì 30", misura 132 px.

**Riga 2:**
- **Oggi** (`09`): la città, `TEXT_3` ("Roma"), troncata con "…". Se il dato della scheda è vecchio (meteo per Meteo e Sole e luna, aria per Aria): icona avviso 20 `WARN` + 6 px + "alle 15:00" `TEXT_2`, cioè l'ora dell'ultimo dato.
- **Altro giorno** (`10`-`17`): "Torna a oggi" in `ACCENT`. Il blocco del nome diventa un pulsante (premuto: fondo `PRESSED`, raggio 12) che riporta a oggi.

Cosa sostituisce il vecchio testo a destra:
- la città va nella riga 2;
- la data piena va nei titoli dei pannelli degli altri giorni (§8.4);
- "aggiornato alle" compare solo quando il dato è vecchio.

### 8.2 Intervallo e gesti

- Da **−7 a +7 giorni** rispetto a oggi, per tutte le schede.
- **Scorrimento orizzontale** sul contenuto (y ≥ 64):
  - verso sinistra = giorno dopo, verso destra = giorno prima;
  - senza animazione, con la soglia dei gesti standard di LVGL;
  - in fondo all'intervallo non succede niente.
- Il giorno scelto vale per **tutte e tre le schede**.
- Uscendo dalla pagina (indietro, barra, ritorno automatico dopo 2 minuti) il giorno si dimentica: la volta dopo si riparte da oggi.
- **A mezzanotte**, con la pagina aperta, il giorno scelto resta la stessa data e i nomi si ricalcolano ("Domani" diventa "Oggi").
- Il **giorno di oggi** usa i dati già in memoria, come adesso, e non fa richieste.

### 8.3 Scheda Meteo di un altro giorno — `10`, `11`

- **Due pannelli** come oggi (`104,72` e `104,184`, 6 celle con centro a x = 160 + i·112). Al posto delle "prossime 12 ore" ci sono **12 orari ogni 2 ore**: 00:00…10:00 nel primo pannello, 12:00…22:00 nel secondo. Celle, icone e giorno/notte come la SPEC_METEO §4.
- **Pioggia nelle celle:** nei giorni futuri come oggi (percentuale dal 20%); nel passato non si mostra, perché l'icona dice già se ha piovuto.
- **Scheda di sinistra**: il giorno scelto. **Scheda di destra**: il giorno dopo. Il formato è quello delle schede Oggi/Domani.
  - Titolo: "Domani", "Ieri", "Oggi" quando valgono, altrimenti "Sabato 7".
  - **Pioggia nel futuro**: come oggi, "Pioggia 73%".
  - **Pioggia nel passato**: goccia 20 `WX_RAIN` + "Pioggia caduta 6,4 mm" `M16 TEXT_2` (`precipitation_sum`, un decimale con la virgola); sotto 0,1 mm: "Nessuna pioggia".
- Oggi la scheda resta com'è (`09`: 12 ore da adesso, Oggi e Domani).

### 8.4 Scheda Sole e luna di un altro giorno — `12`

**Pannello in alto:**
- Il titolo diventa la data piena: "Giovedì 12 novembre" (`SB20`, al massimo circa 240 px). L'indice UV, allineato a destra, è quello di quel giorno.
- Alba, tramonto e ore di luce sono quelli di quel giorno.
- La riga della differenza confronta con **oggi**: "15 min in meno di oggi", "4 min in più di oggi", "come oggi". Oggi resta "… domani".
- Nel binario c'è la luce di quel giorno ma **non il segno "adesso"**.

**Scheda Prossimi giorni:** i 2 giorni dopo quello scelto, chiamati "Domani" se lo è, altrimenti "Venerdì 13".

**Scheda Luna:**
- fase e percentuale di quel giorno, calcolate a mezzogiorno;
- la riga "prossima" diventa una data: "Luna nuova il 9 novembre", "Luna piena il 24 novembre". Oggi resta "tra N giorni";
- la regola del giorno di luna piena o nuova non cambia.

### 8.5 Scheda Aria di un altro giorno — `13`, `14`

Disponibile da −7 a **+4** giorni.

**Pannello in alto:**
- **Indice grande** = il **valore orario più alto** del giorno, con pallino, giudizio e segno sulla scala di quel valore.
- **Sottotitolo**: "Massimo del giorno, alle 20:00", con l'ora del massimo (a parità di valore, la prima).
- **Titolo del grafico**: "Ora per ora", al posto di "Prossime 24 ore". La riga minimo/massimo ha lo stesso formato di oggi.
- **Barre**: le 24 ore del giorno, dalle 00:00 alle 23:00. Etichette sotto le barre 0, 6, 12 e 18: "00:00", "06:00", "12:00", "18:00".
- **Ore senza dato** (succede a +4, dove la previsione finisce a metà giornata): barra 20×4 `RAISED`. Minimo e massimo si calcolano sulle ore presenti, e il titolo diventa "Ora per ora · fino alle 12:00".

**Scheda Inquinanti:**
- valori e pallini **all'ora del massimo**, con i loro sotto-indici orari;
- a destra "µg/m³ · alle 20:00" al posto del solo "µg/m³".

**Scheda Pollini:**
- come la SPEC_SOLE_ARIA §4.4, ma sul massimo **di quel giorno** invece che delle prossime 24 ore;
- nota a destra "massimo del giorno";
- se sono tutti a 0: "Nessun polline" con sotto "Fuori stagione" (da ottobre a gennaio) oppure "In questo giorno".

Oggi la scheda resta com'è.

### 8.6 Stati del giorno

| Stato | Quando | Resa |
|---|---|---|
| **In arrivo** (`16`) | dalla scelta del giorno all'arrivo dei dati (0,2-1 s) | Pannelli e schede con il fondo `SURFACE` e lo scheletro fermo: per il Meteo come nel mockup, cioè barre 48×14, cerchi 28 e barre 40×16 nelle celle, con gli stessi colori del §3.4; per le altre schede barre `RAISED` al posto dei testi. La **scheda Luna** si mostra subito, perché è calcolata. Il selettore resta attivo: se si cambia ancora giorno, vale l'ultimo. |
| **Normale** | dati arrivati | §8.3-§8.5 |
| **Errore** (`17`) | richiesta fallita o più di 10 s senza risposta | Blocco stato centrato in 88-800 (SPEC 6 §4.9): cerchio 80 `SURFACE` a y 120 con avviso 40 `WARN`; titolo `SB20 TEXT` a y 224; testo `M16 TEXT_2` a y 258: "Controlla la connessione, poi riprova."; pulsante `bs sm` 176×48 a y 306, icona sync 20 + "Riprova". Titoli: "Meteo di lunedì 9 non disponibile", "Qualità dell'aria di lunedì 9 non disponibile". In Sole e luna il blocco non c'è: il pannello e Prossimi giorni mostrano solo il titolo, "Alba e tramonto non disponibili" `M16 TEXT_3` a 20,60 e "Riprova" `bs sm` 176×48 a 20,100 (dentro il pannello); la scheda Luna resta normale. |
| **Aria oltre i 4 giorni** (`15`) | scheda Aria, giorni +5…+7 | Blocco stato come l'errore, ma: cerchio con vento 40 `TEXT_3` (non è un errore); titolo "Previsioni dell'aria fino a lunedì 9" (il giorno +4); testo "La qualità dell'aria si prevede solo per i prossimi 4 giorni."; pulsante `bs sm` 200×48 "Vai a lunedì 9", che porta al giorno +4. |
| **Passato** (`11`, `14`) | giorni −1…−7 | normale, con le regole del passato (pioggia in mm, niente percentuali) |
| **Dato vecchio** | solo oggi | avviso nella riga 2 del selettore (§8.1) |

### 8.7 Dati

Le richieste partono dallo stesso task e allo stesso modo del meteo (HTTP semplice, connessione chiusa dopo ogni richiesta), con **precedenza** sull'aggiornamento periodico.

**Meteo e sole del giorno D** (una richiesta serve le schede Meteo e Sole e luna):

```
api.open-meteo.com/v1/forecast?latitude=…&longitude=…&timezone=auto
  &start_date=D&end_date=D+2
  &hourly=temperature_2m,weather_code,precipitation_probability,is_day
  &daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,
         precipitation_sum,sunrise,sunset,daylight_duration,uv_index_max
```

D+2 serve per la scheda del giorno dopo (Meteo) e per Prossimi giorni (Sole e luna). Per i giorni passati la percentuale di pioggia può arrivare `null`: non si mostra comunque.

**Aria del giorno D:**

```
air-quality-api.open-meteo.com/v1/air-quality?latitude=…&longitude=…&timezone=auto
  &start_date=D&end_date=D
  &hourly=european_aqi,pm2_5,pm10,nitrogen_dioxide,ozone,
          european_aqi_pm2_5,european_aqi_pm10,european_aqi_nitrogen_dioxide,european_aqi_ozone,
          grass_pollen,olive_pollen,birch_pollen,alder_pollen,mugwort_pollen,ragweed_pollen
```

Regole:
- si chiede solo quello che serve alla **scheda visibile**; cambiando scheda, si chiede quello che manca;
- si aspettano **400 ms** dall'ultimo cambio di giorno prima di chiedere: chi tocca › tre volte di fila fa una richiesta sola;
- una risposta che arriva per un giorno non più scelto si tiene in memoria ma non si disegna;
- **memoria**: gli ultimi 6 giorni per tipo, in PSRAM, validi 30 minuti; si svuota quando si esce dalla pagina;
- costo: 3 giorni × 24 ore di numeri piccoli per il meteo, 24 ore × 15 valori per l'aria: pochi KB.

---

## 9. Icone e caratteri da generare

| File | Aggiunte | Uso |
|---|---|---|
| `font_ic_28` | headphones `0xF025`, cog `0xF013`, chevron-right `0xF054`, wifi `0xF1EB`, info-circle `0xF05A` | Ascolti (barra, icone in alto), Impostazioni, selettore ›, righe di Impostazioni |
| `font_ic_20` | headphones `0xF025` | pulsante "Ascolti" nel riquadro Spotify |

- Ci sono già:
  - step-forward e step-backward `0xF051`/`0xF048` a 28 px (Avanti nella barra e nel riquadro Spotify);
  - chevron-left a 28 px;
  - play/pausa a 28 px;
  - casa a 28 px;
  - sync e avviso a 20 px;
  - avviso a 40 px;
  - vento a 40 px;
  - le immagini della luna a 20 px.
- **Nessun carattere di testo nuovo e nessun colore nuovo.**
- Memoria: circa 3 KB.

---

## 10. Modifiche alle SPEC precedenti

**SPEC 6 §1 (mappa e regole):**
- l'**Orologio è sostituito dalla Home** (§3);
- Now playing **non si apre più da solo** quando parte la musica e non si chiude quando si ferma;
- all'accensione si parte dalla Home;
- dopo 2 minuti senza tocchi si torna alla Home, tranne Now playing mentre la musica suona;
- la barra laterale non c'è in Home, Now playing, Configura Wi-Fi e Mi collego;
- lo scorrimento a destra nella pagina a schede cambia giorno invece di tornare indietro (§8.2);
- toccare la copertina della barra senza niente in riproduzione porta alla Home.

**SPEC 6 §4.1 (barra laterale):** sostituita dalla tabella del §4:
- voci alte 56;
- Avanti a `0,136`;
- Home a `0,200`, Ascolti a `0,256`, Libreria a `0,312`, Cerca a `0,368`, Dispositivi a `0,424`;
- **Coda tolta**;
- barretta della voce accesa a +12.

**SPEC 6 §5.1 (Now playing):** icone in alto come al §5.

**SPEC 6 §5.2 e SPEC 7 §3.5 (Orologio):** superate, sostituite dalla Home (§3).

**SPEC 7 §1.1 ("L'Orologio resta la schermata di riposo"):** la schermata di riposo è la Home, con o senza musica.

**SPEC 7 §2 e §6 (voce Home):** la voce si chiama **Ascolti**, ha l'icona cuffie ed è a `0,256`. La voce Home a `0,200` è quella della Home della scheda.

**SPEC 7 §3.6:** icone di Now playing come al §5 di questa SPEC.

**SPEC_METEO §1 ("spazio per altre informazioni"):** le aree informazioni sono ora i riquadri della Home: il riquadro Meteo e i 3 riquadri piccoli (§3.5, §1 domanda 7).

**SPEC_METEO §2:** la pagina si apre dal riquadro Meteo della Home; la freccia indietro torna alla Home; nella barra è accesa la voce Home.

**SPEC_METEO §3 (Orologio):** sostituita dal riquadro Meteo (§3.4):
- blocco "adesso" e fascia in un solo riquadro 720×120 a `40,236`;
- 4 orari (+2 … +8) invece di 5;
- niente città né temperatura percepita;
- il dato vecchio è segnalato con l'avviso in basso a destra della zona Adesso.

Restano invariati le icone, i testi del §5, le regole di arrotondamento e le soglie degli stati.

**SPEC_METEO §4 e SPEC_SOLE_ARIA §4.1 (intestazione della pagina):** il testo a destra (città, data, "aggiornato alle") è sostituito dal selettore dei giorni (§8.1). La scheda Sole e luna di oggi non mostra più la data completa, che resta sulla Home.

**SPEC_SOLE_ARIA §2 (navigazione):** le schede si aprono dai riquadri piccoli della Home invece che dalla riga dell'Orologio.

**SPEC_SOLE_ARIA §3 (riga dell'Orologio):** sostituita dai riquadri piccoli (§3.5):
- stesse voci, priorità e regole;
- formato su tre righe (etichetta, valore, dettaglio);
- "Alba domani" al posto di "Alba" dopo il tramonto.

**SPEC_COLORE:** nessuna modifica; la nuova icona Home di Now playing usa i toni della copertina come le altre.

**`fase6/CARATTERI_E_ICONE.md`:** aggiungere una sezione "5. Home e giorni" con le icone del §9.

**Non cambiano:**
- le schermate di Spotify della fase 6 e della fase 7, tranne la barra;
- il contenuto delle schede di oggi;
- colori e caratteri.

---

## 11. Da verificare sul dispositivo (Claude Code)

1. **Barra con voci da 56.**
   - Prova col tocco vero Avanti, Home e le voci vicine: fra Avanti e Home ci sono solo 8 px di separatore.
   - Se partono tocchi sbagliati, porta il separatore a 16 e le voci a 54.
2. **Riquadro Spotify.**
   - ⏮ ⏯ ⏭ rispondono senza aprire Now playing; il tocco sul resto del riquadro lo apre.
   - Sul fondo `SURFACE` il premuto `RAISED` dei pulsanti si vede.
3. **Trattini con il Wi-Fi attivo:**
   - il cerchio bianco 56 di play sulla Home (come quello da 52 della barra);
   - la copertina 64 nel riquadro;
   - il pulsante verde "Ascolti".
4. **Luce al 15%,** da 1-2 metri:
   - si leggono ora, temperatura, il titolo del brano e i valori `SB20` dei riquadri piccoli?
   - Le etichette in `TEXT_2` e i dettagli in `TEXT_3` possono vedersi meno.
5. **Ora:** "00:00" con `CLOCK112` deve finire prima di x 400. Ho misurato 324 px con Montserrat Medium; controlla con il carattere generato.
6. **Ritorno automatico dopo 2 minuti:**
   - la pila si azzera;
   - il giorno della pagina Meteo torna a oggi;
   - Now playing con la musica resta;
   - Now playing in pausa torna alla Home.
7. **Open-Meteo per un giorno solo:**
   - `start_date`/`end_date` nel passato (fino a −7) e nel futuro (fino a +9, per D+2);
   - tempo di risposta;
   - cosa arriva per `precipitation_probability` e `precipitation_sum` nel passato;
   - fin dove arrivano le ore dell'aria a +4.
8. **Scorrimento orizzontale** nella pagina a schede: deve cambiare giorno senza far partire anche il tocco su un pannello, e senza tornare indietro.
9. **RAM:** la Home prende il posto dell'Orologio e Impostazioni è piccola. Controlla la RAM interna libera con la pagina Meteo aperta su un altro giorno (con la sessione TLS di Spotify aperta): deve restare sopra i ~40 KB.
10. **Now playing che non si apre da solo** è la scelta più grande di questa SPEC. Se Valerio, usandola, preferisce il comportamento di prima, basta riattivare l'apertura automatica: il resto della SPEC non cambia.
