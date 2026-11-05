# Specifica · Città del meteo

Risposta a `design/BRIEF_CITTA.md`. Si aggiunge a `home_giorni/SPEC_HOME_GIORNI.md`, `computer/SPEC_COMPUTER.md` e `fase6/SPEC_FASE6.md`: quello che qui non è nominato resta com'è. I nomi dei colori (`TEXT_2`, `SURFACE`, `ACCENT`…) sono quelli della SPEC 6 §3.1.

**In breve**
- In **Impostazioni** c'è una riga nuova, **"Città del meteo"**, che apre l'elenco delle città.
- L'**elenco** ha in cima la **Posizione attuale** (quella automatica di oggi) e sotto le città salvate, al massimo **8**. Ogni riga mostra la temperatura di adesso.
- La **predefinita** si sceglie con la **stella** a destra della riga. È la città della Home e quella con cui la pagina Meteo si apre.
- Le città si **aggiungono** con il pulsante "Aggiungi": stessa tastiera di Cerca, poi l'elenco dei risultati con provincia, regione e nazione.
- Si **tolgono** con "Modifica", che mostra un cestino su ogni riga. Niente pressione lunga.
- Nella pagina Meteo il **nome della città** sotto "Oggi" diventa un pulsante: apre l'elenco, e toccando una città si guarda il suo meteo.
- **Regola della città scelta al volo:** vale solo nella pagina Meteo · Sole e luna · Aria e si dimentica uscendo, come il giorno scelto. La Home mostra sempre la predefinita.
- Servono 5 icone in più. Nessun colore o carattere nuovo.

Mockup 800×480 in `citta/mockup/`, disegnati nel canvas alla pagina "Città del meteo" (https://claude.ai/artifact/2BmkheovXWkQTH5Pqxk8tw):

| File | Cosa mostra |
|---|---|
| `01_impostazioni.png` | Impostazioni con la riga "Città del meteo" (premuta) |
| `02_citta.png` | elenco: Posizione attuale predefinita (Roma) e 4 città salvate, con la temperatura di adesso |
| `03_citta-modifica.png` | modalità Modifica: cestini al posto di stelle e temperature |
| `04_cerca-tastiera.png` | ricerca di una città, "milano" scritto |
| `04b_cerca-vuota.png` | ricerca con il campo vuoto: "Cerca" spento |
| `05_risultati.png` | risultati per "milano": abitanti a destra, "In elenco" per quella già salvata |
| `06_risultati-omonimi.png` | 10 "San Giovanni": si distinguono per provincia e regione (una riga premuta) |
| `07_ricerca-in-corso.png` | ricerca in corso: scheletro e spinner |
| `08_nessuna-citta.png` | nessuna città trovata |
| `09_ricerca-errore.png` | ricerca non riuscita (rete) |
| `10_elenco-pieno.png` | elenco pieno, pagina 2/2: "Aggiungi" spento e una nota |
| `11_meteo-predefinita.png` | pagina Meteo con la predefinita: "Roma ⌄" sotto "Oggi" (premuto) |
| `12_meteo-cambio.png` | pagina Meteo mentre arriva il meteo di Milano |
| `13_meteo-altra-citta.png` | pagina Meteo di Milano, scelta al volo |
| `14_meteo-altra-citta-domani.png` | Milano, domani: il nome della città resta visibile |
| `15_home-predefinita.png` | Home con Milano predefinita: il nome compare nel riquadro Meteo |
| `16_home-senza-posizione.png` | Home quando la posizione automatica non si trova |
| `17_citta-senza-posizione.png` | elenco con la posizione non trovata e Milano predefinita |

---

## 1. Risposte alle domande del brief

**1. Dove si gestiscono le città.** In due posti, che aprono la stessa pagina:
- la riga **"Città del meteo"** in Impostazioni (`01`), dove si va per sistemare l'elenco;
- il **nome della città** sotto "Oggi" nella pagina Meteo (`11`), dove si va per guardare un'altra città.

La pagina è una sola (§4): cambia solo dove si torna dopo aver scelto (§2).

**2. L'elenco.**
- **La posizione automatica è la prima voce**, "Posizione attuale", con sotto la città stimata ("Roma, stimata dalla rete"). Non si può togliere. Come sull'iPhone, è chiaro che è un'opzione fra le altre.
- **Predefinita:** una stella a destra di ogni riga, verde su quella predefinita e grigia sulle altre. Toccare una stella grigia la rende predefinita. Una sola alla volta. La riga lo scrive anche a parole ("Predefinita · …"), così non conta solo il colore.
- **Togliere:** con il pulsante **Modifica** in alto, che mostra un cestino su ogni città (`03`). Il cestino toglie subito la città, senza conferma: rimetterla costa una ricerca. Niente pressione lunga e niente scorrimento sulla riga.
- **Massimo 8 città**, più la posizione attuale: 9 righe, cioè 2 pagine da 5.
- **Temperatura di adesso:** sì, con l'icona del tempo. È una richiesta piccola per tutte le città insieme (§10.3) e rende l'elenco utile anche solo da guardare.

**3. La ricerca.**
- Stessa tastiera di Cerca (SPEC 6 §4.18), accenti compresi.
- Risultati con nome, poi **provincia · regione · nazione**, più gli **abitanti** a destra. I "San Giovanni" si distinguono dalla provincia, e fra i "Milano" quello vero è il primo e ha "1,4 mln".
- **Al tocco** su un risultato la città entra nell'elenco e si apre il suo meteo. **Non diventa predefinita**: quella si sceglie solo con la stella, così una ricerca fatta per curiosità non cambia la Home.

**4. Città in uso e predefinita.** La regola è la stessa dei giorni:
- la **predefinita** vale per la Home, per l'aggiornamento ogni 30 minuti e come città iniziale della pagina Meteo;
- una città **scelta al volo** vale solo dentro la pagina a schede (Meteo · Sole e luna · Aria), su tutte e tre le schede e su tutti i giorni;
- **uscendo dalla pagina** (indietro, barra laterale, ritorno automatico dopo 2 minuti) si dimentica, come il giorno scelto.

Così la Home non cambia mai città senza che Valerio l'abbia deciso con la stella, e guardare il meteo di un'altra città non ha conseguenze.

**5. Home.** Sì: se la predefinita **non è la Posizione attuale**, il nome compare accanto alla condizione nel riquadro Meteo ("Pioggia 📍 Milano", `15`). Con la posizione attuale non serve, perché è il posto dove si trova la scheda.

**6. Stati:** §9.

**7. Navigazione:** §2.

---

## 2. Navigazione

```
Impostazioni ── riga Città del meteo ──┐
                                       ├── Città del meteo ─┬─ riga di una città ───── pagina Meteo di quella città
Pagina Meteo ── nome della città ──────┘                    ├─ stella ─────────────── diventa predefinita (si resta)
                                                            ├─ Modifica ───────────── modalità Modifica (cestini)
                                                            └─ Aggiungi ───────────── Cerca una città ── Cerca ── Risultati
                                                                                                                    └─ un risultato ── pagina Meteo di quella città
```

| Da | Tocco | Va a |
|---|---|---|
| Impostazioni | riga "Città del meteo" | Città del meteo (`02`) |
| Pagina a schede, giorno **Oggi** | blocco del giorno (nome + città) | Città del meteo |
| Pagina a schede, altro giorno | blocco del giorno | Oggi (come prima) |
| Home, riquadro Meteo nello stato "Posizione non trovata" | tutto il riquadro | Città del meteo (`16`) |
| Città del meteo | riga di una città (fuori dalla stella) | pagina a schede con quella città (vedi sotto) |
| Città del meteo | stella grigia | quella città diventa predefinita; si resta nell'elenco |
| Città del meteo | stella verde | niente |
| Città del meteo | Modifica | modalità Modifica (`03`) |
| Città del meteo | Aggiungi | Cerca una città (`04b`) |
| Città del meteo | freccia indietro | da dove si è venuti (Impostazioni o pagina a schede, con la città di prima) |
| Modifica | cestino | toglie la città; si resta in Modifica |
| Modifica | Fine o freccia indietro | elenco normale |
| Cerca una città | Cerca (tasto) | Risultati (`07`, poi `05`) |
| Cerca una città | freccia indietro | Città del meteo |
| Risultati | chip della ricerca | Cerca una città, con il testo |
| Risultati | un risultato | la città entra nell'elenco; pagina a schede con quella città |
| Risultati | freccia indietro | Città del meteo |
| Nessuna città trovata | Modifica la ricerca | Cerca una città, con il testo |
| Qualsiasi pagina | 2 minuti senza tocchi | Home (la città scelta al volo e la modalità Modifica si dimenticano) |

**Dove porta la scelta di una città** (riga dell'elenco o risultato):
- **aperto dalla pagina a schede:** si torna alla pagina a schede, sulla **stessa scheda e lo stesso giorno**, con la città nuova. Città del meteo, Cerca e Risultati escono dalla pila: la freccia indietro della pagina a schede porta di nuovo alla Home;
- **aperto da Impostazioni:** si apre la pagina a schede, scheda **Meteo, Oggi**, con quella città. Cerca e Risultati escono dalla pila: la freccia indietro porta a Città del meteo.

Regole:
- Voce accesa nella barra: **Home**, come per Impostazioni e la pagina Meteo.
- Toccare la predefinita nell'elenco equivale a tornare alla predefinita.
- Tutti i cambi di schermata sono istantanei, senza animazioni.

---

## 3. Impostazioni — `01`

Righe di 712×80 nel formato della SPEC_HOME_GIORNI §6: Wi-Fi a `88,64`, Computer a `88,144`, **Città del meteo a `88,224`**, Informazioni a `88,304`.

| Elemento | Contenuto |
|---|---|
| Icona | map-marker-alt 28 `TEXT_2` nel cerchio 56 `SURFACE` |
| Titolo | "Città del meteo" |
| Sotto | la predefinita, poi quante città salvate: "Posizione attuale (Roma) · 4 città salvate", "Milano · 4 città salvate"; senza città salvate solo "Posizione attuale (Roma)"; posizione non trovata: "Posizione attuale · non trovata" |
| Tocco | tutta la riga (premuta: fondo `PRESSED`) → Città del meteo |

---

## 4. Città del meteo — `02`

Barra laterale con la voce Home accesa. Fondo `BG`.

### 4.1 Intestazione

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Freccia indietro | 88,0 | 64×64 | SPEC 6 §4.2 |
| Titolo | 156,17 | fino a x 416 | "Città del meteo" `SB24 TEXT` |
| Modifica | 432,8 | 136×48 | `bs sm`, "Modifica"; nascosto se non ci sono città salvate |
| Aggiungi | 584,8 | 152×48 | `bp sm`, plus 20 + 8 px + "Aggiungi"; spento con l'elenco pieno (§9) |

### 4.2 Righe — 648×80 a `x 88`, `y` = 64, 144, 224, 304, 384

Formato della SPEC 6 §4.3 con il cerchio delle icone (come i dispositivi). Coordinate relative alla riga:

| Elemento | x,y | Stile |
|---|---|---|
| Area di tocco | intera riga, tranne la stella | premuto: fondo `PRESSED` |
| Cerchio dell'icona | 16,12, 56×56 | `SURFACE`; icona 28 `TEXT_2`: location-arrow per la posizione attuale, map-marker-alt per le città |
| Nome | 96,15, fino a x 464 | `SB20 TEXT`, 1 riga, "…" |
| Sotto | 96,43, fino a x 464 | `M16 TEXT_2`, 1 riga, "…" |
| Tempo di adesso | allineato a destra a x 568, y 26, alto 28 | icona meteo 28 (`font_ic_28`, giorno/notte come la SPEC_METEO) + 8 px + temperatura `M20 TEXT` ("16°") |
| Stella | 576,8, 64×64 | star 28: `ACCENT` sulla predefinita, `PLACEHOLDER_FG #535353` sulle altre; premuta: cerchio 48 `PRESSED` dietro l'icona |

**Testo "Sotto":**

| Riga | Normale | Predefinita |
|---|---|---|
| Posizione attuale | "Roma, stimata dalla rete" | "Predefinita · Roma, stimata dalla rete" |
| Città salvata | regione, nazione: "Lombardia, Italia" | "Predefinita · Lombardia, Italia" |

- Se due città salvate hanno lo stesso nome e la stessa regione (due "San Giovanni" in Sicilia), si usa la provincia al posto della regione: "Provincia di Catania, Italia".
- Senza regione (succede fuori Europa) resta solo la nazione.

**Ordine:** Posizione attuale, poi le città nell'ordine in cui sono state aggiunte; la nuova va in fondo. La predefinita non si sposta.

**Pagine:** più di 5 righe → colonna pagine della SPEC 6 §4.5 a `736,64`. Con 9 righe sono 2 pagine.

**Temperature:** arrivano dopo l'apertura (§10.3). Finché non ci sono, la zona resta vuota; se la richiesta fallisce resta vuota, senza messaggi.

### 4.3 Modalità Modifica — `03`

- In alto, al posto di Modifica e Aggiungi: **"Fine"** `bp sm` 152×48 a `584,8`.
- Su ogni città: **cestino** al posto della stella, a `576,8` 64×64. Cerchio 48 `RAISED` centrato, icona trash 28 `TEXT`; premuto: cerchio `KEY_PRESSED`.
- Temperature nascoste. Le righe non rispondono al tocco e non hanno lo stato premuto.
- La Posizione attuale non ha il cestino.
- **Cestino:** la città sparisce subito e le righe sotto salgono. Toast informativo (SPEC 6 §4.12) "Milano tolta dall'elenco".
- Se si toglie la **predefinita**, la predefinita torna la Posizione attuale. Il toast dice "Milano tolta. Ora vale la posizione attuale".
- Tolta l'ultima città, si esce da Modifica da soli.

---

## 5. Cerca una città — `04`, `04b`

Barra laterale con la voce Home accesa. Il campo si apre vuoto; dal chip dei risultati, con il testo di prima.

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Freccia indietro | 88,8 | 64×64 | SPEC 6 §4.2, centrata sul campo |
| Campo | 152,12 | 632×56, raggio 28 | come Cerca (SPEC 6 §5.10): `SURFACE`, lente 20 `TEXT_2` a +20, testo `M20 TEXT`, cursore `ACCENT` |
| Segnaposto | — | — | "Cerca una città" `M20 TEXT_3` |
| Cancella | 724,12 | 56×56 | come Cerca; solo se c'è testo |
| Suggerimento | 172,92 | L 580, 2 righe | `M16 TEXT_3`: "Scrivi il nome della città e tocca Cerca." / "Se ci sono più posti con lo stesso nome, vedrai provincia e regione." |
| Tastiera | 88,208 | 712×272 | SPEC 6 §4.18, accenti e "123" compresi |

- Il tasto **Cerca** è spento (`#2A2A2A`, testo `#5A5A5A`) finché il testo ha meno di **2 caratteri** (senza contare gli spazi) (`04b`).
- Massimo 48 caratteri.
- La ricerca parte solo con il tasto Cerca, mai a ogni lettera.
- Niente "ricerche recenti": l'elenco delle città fa già da memoria.

---

## 6. Risultati — `05`, `06`

### 6.1 Intestazione

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Freccia indietro | 88,0 | 64×64 | → Città del meteo |
| Chip della ricerca | 152,8 | auto × 48, max 360 | chip SPEC 6 §4.15: lente 20 `TEXT_2` + testo `M20 TEXT`, "…"; tocco → Cerca una città con il testo |
| Contatore | allineato a destra a x 720, y 21 | — | `M16 TEXT_3`: "10 risultati", "1 risultato" |
| Spinner | 692,18 | 28×28 | solo durante la ricerca, al posto del contatore |

### 6.2 Righe — 648×80 a `x 88`, 5 per pagina

| Elemento | x,y | Stile |
|---|---|---|
| Area di tocco | intera riga | premuto: fondo `PRESSED` (`06`) |
| Cerchio dell'icona | 16,12, 56×56 | `SURFACE`, map-marker-alt 28 `TEXT_2` |
| Nome | 96,15, fino a x 520 | `SB20 TEXT` |
| Sotto | 96,43, fino a x 520 | `M16 TEXT_2`: provincia · regione · nazione, saltando quelle che mancano o ripetono la precedente |
| A destra | allineato a destra a −16, y 29, L 96 | `M16 TEXT_3`: abitanti, oppure "In elenco" |

**Abitanti:**
- da 1 milione in su: "1,4 mln", con un decimale;
- sotto il milione: il numero con il punto delle migliaia + " ab.": "421 ab.", "2.900 ab.", "93.200 ab.";
- senza il dato (frazioni piccole): niente.

**"In elenco":** la città è già salvata (stesso `id` di Open-Meteo). Toccarla non la aggiunge una seconda volta: apre il suo meteo.

**Pagine:** colonna pagine (SPEC 6 §4.5). Al massimo 10 risultati, quindi 2 pagine. Niente "carica altri".

**Ordine:** quello di Open-Meteo, che mette prima i posti più grandi.

---

## 7. Pagina a schede — `11`-`14`

Cambia solo il **blocco del giorno** del selettore (SPEC_HOME_GIORNI §8.1, `576,4` 136×56). Riga 1 invariata.

**Riga 2, nel giorno Oggi** (`11`-`13`): un gruppo centrato, spazio 4, largo al massimo 124:

| Città mostrata | Contenuto | Colori |
|---|---|---|
| Predefinita | nome + chevron-down 20 | nome `M16 TEXT_3`, chevron `TEXT_3` |
| Scelta al volo | map-marker-alt 20 + nome + chevron-down 20 | tutto `TEXT` (il nome in `M16`) |
| Predefinita con il dato vecchio | come oggi: avviso 20 `WARN` + "alle 15:00" `TEXT_2` | — |

- Il nome si accorcia con "…" ("Reggio nell'E…").
- Per la Posizione attuale il nome è quello della città stimata ("Roma").
- Il blocco, nel giorno Oggi, **è un pulsante** (premuto: fondo `PRESSED`, raggio 12, `11`) che apre Città del meteo.

**Riga 2, negli altri giorni** (`14`):
- con la **predefinita**: "Torna a oggi" in `ACCENT`, come prima;
- con una città **scelta al volo**: map-marker-alt 20 + nome, in `TEXT`, perché la città conta più del promemoria. Il blocco resta il pulsante che riporta a Oggi.

**Cambio di città** (`12`): la pagina mostra subito lo stato "in arrivo" della SPEC_HOME_GIORNI §8.6, con il nome della città nuova già nella riga 2. Valgono gli stessi stati (errore dopo 10 s, Riprova). Il giorno e la scheda restano quelli di prima.

**Ore e fusi orari:** per una città con un altro fuso orario, ore e "adesso" sono **quelli del posto** (Londra un'ora indietro). "Oggi" resta il giorno della scheda.

---

## 8. Home — `15`, `16`

Il riquadro Meteo e i riquadri piccoli (sole, aria, pollini) mostrano **sempre la predefinita**.

**Nome della città** (`15`), solo se la predefinita **non è** la Posizione attuale. La riga della condizione (`20,86`, L 276) diventa una fila:
- condizione `M20 TEXT`, che si accorcia con "…";
- 8 px, map-marker-alt 20 `TEXT_3`;
- 4 px, nome `M16 TEXT_2`, largo al massimo 120 px, "…".

Se c'è l'avviso del dato vecchio (SPEC_HOME_GIORNI §3.4), il nome non si mostra: l'avviso conta di più e la città si vede comunque nella pagina Meteo.

**Posizione non trovata** (`16`), solo con la Posizione attuale predefinita, quando la stima dalla rete fallisce:
- il riquadro Meteo usa lo stato di errore del §3.4 con due testi diversi: "Posizione non trovata" (`M20 TEXT_2`) e "Tocca per scegliere una città" (`M16 TEXT_3`);
- in questo stato il riquadro **si tocca** (premuto: `SURFACE_PRESSED`) e apre Città del meteo;
- dei riquadri piccoli resta solo la luna, come senza rete.

---

## 9. Stati

| Stato | Dove | Resa |
|---|---|---|
| **Ricerca in corso** (`07`) | Risultati | spinner 28 a `692,18`. 5 righe di scheletro fermo: cerchio 56 `SURFACE` a 16,12; barra alta 16 a 88,20, larga 160/200/140/180/150, `SURFACE` raggio 8; barra alta 12 a 88,48, larga 260/220/280/240/200, `SKELETON_2`. Dopo 10 s senza risposta → errore. |
| **Nessuna città trovata** (`08`) | Risultati | blocco stato SPEC 6 §4.9: cerchio 80 `SURFACE` con map-marker-alt 40 `TEXT_2`; "Nessuna città trovata per «milamo»"; "Controlla come l'hai scritto. Con 2 lettere cerco solo il nome esatto."; `bs` 240×56 "Modifica la ricerca", 24 px sotto il testo |
| **Errore di rete** (`09`) | Risultati | blocco stato: avviso 40 `WARN`; "Non riesco a cercare le città"; "Controlla il Wi-Fi e riprova."; `bs` 176×56 sync 20 + "Riprova" (rifà la stessa ricerca) |
| **Elenco pieno** (`10`) | Città del meteo | "Aggiungi" spento (fondo `#2A2A2A`, testo e icona `#5A5A5A`). Sull'ultima pagina, sotto l'ultima riga, a `112,404`, L 600, 2 righe: "Hai salvato 8 città, il massimo. Per aggiungerne un'altra togline una con Modifica." `M16 TEXT_3`. Con 9 righe l'ultima pagina ne ha 4, quindi c'è sempre posto. |
| **Posizione attuale non trovata** (`17`) | Città del meteo | icona location-arrow `DISABLED_FG`; sotto "Non trovata · riprovo tra 5 minuti" ("tra poco" se l'intervallo è un altro); niente temperatura. La riga non si tocca, ma la stella sì. |
| **Posizione non trovata** (`16`) | Home | §8 |
| **Cambio di città** (`12`) | pagina a schede | §7 |
| **Temperature non arrivate** | Città del meteo | zona vuota (§4.2) |
| **Senza Wi-Fi** | Città del meteo | l'elenco si apre lo stesso, senza temperature; "Aggiungi" funziona, e la ricerca finisce nell'errore di rete |

---

## 10. Dati

### 10.1 Città salvate (memoria permanente, come le ricerche recenti)

Per ogni città, al massimo 8:

| Campo | Note |
|---|---|
| `id` | id di Open-Meteo, per riconoscere "In elenco" |
| nome | max 64 byte UTF-8, già passato da `clean_text()` |
| provincia, regione, nazione | max 48 byte ciascuno, possono mancare |
| latitudine, longitudine | float |

Più l'indice della **predefinita** (−1 = Posizione attuale). Circa 2 KB in tutto.

### 10.2 Ricerca

```
geocoding-api.open-meteo.com/v1/search?name=<testo>&count=10&language=it&format=json
```

Campi usati: `id`, `name`, `latitude`, `longitude`, `admin2` (provincia), `admin1` (regione), `country`, `population`. Si usa già per il nome italiano della posizione automatica: stessa connessione semplice, chiusa dopo la risposta.

### 10.3 Temperature dell'elenco

Una richiesta per tutte le città, con le coordinate separate da virgole:

```
api.open-meteo.com/v1/forecast?latitude=41.89,45.46,…&longitude=12.48,9.19,…
  &current=temperature_2m,weather_code,is_day&timezone=auto
```

- Parte quando si apre Città del meteo, se i valori hanno più di 30 minuti.
- La risposta è un elenco nello stesso ordine delle coordinate: circa 1 KB.
- La Posizione attuale è nella richiesta con le sue coordinate. Se è la predefinita, si può usare il dato già in memoria.

### 10.4 La predefinita

- È la città del meteo "principale" di oggi: Home, aggiornamento ogni 30 minuti, aria e pollini, e la città iniziale della pagina a schede.
- **Posizione attuale predefinita:** tutto come oggi (ip-api.com + nome italiano).
- **Città salvata predefinita:** si usano le sue coordinate e niente ip-api. La Posizione attuale si stima solo quando serve, cioè quando si apre Città del meteo.
- **Cambio di predefinita** (stella): il meteo principale si riscarica subito per la città nuova (previsione + aria). Nel frattempo il riquadro Meteo della Home è nello stato "in arrivo".

### 10.5 Città scelta al volo

- **Non tocca il meteo principale:** la Home resta giusta e al ritorno non c'è niente da riscaricare.
- Usa le **richieste dei giorni** della SPEC_HOME_GIORNI §8.7, con le coordinate della città, **anche per Oggi**.
  - Per Oggi la scheda Meteo mostra 12 ore da adesso e Domani, quindi basta `start_date=D&end_date=D+2`, come per gli altri giorni.
  - Per l'Aria di oggi servono le prossime 24 ore: `start_date=D&end_date=D+1`.
  - "Adesso" è l'ora del posto: si ricava da `utc_offset_seconds` della risposta.
- La memoria dei giorni (6 per tipo, PSRAM, 30 minuti) usa come chiave **città + giorno**. Si svuota uscendo dalla pagina, come oggi.

### 10.6 Comandi seriali

Oggi `METEO <lat> <lon> <città>` fissa la posizione e `METEO AUTO` torna all'automatica. Propongo che:
- `METEO <lat> <lon> <città>` aggiunga la città all'elenco (se c'è posto) e la renda predefinita;
- `METEO AUTO` renda predefinita la Posizione attuale.

Decide Claude Code.

---

## 11. Icone e caratteri da generare

| File | Aggiunte | Uso |
|---|---|---|
| `font_ic_20` | map-marker-alt `0xF3C5` | città nel riquadro Meteo della Home, città scelta al volo nel selettore |
| `font_ic_28` | map-marker-alt `0xF3C5`, location-arrow `0xF124`, star `0xF005`, trash `0xF1F8` | righe dell'elenco e dei risultati, riga di Impostazioni, stella, cestino |
| `font_ic_40` | map-marker-alt `0xF3C5` | "Nessuna città trovata" |

- Ci sono già:
  - plus `0xF067`, chevron-down `0xF078`, times `0xF00D`, search `0xF002`, sync e avviso a 20 px;
  - chevron-up e chevron-down a 28 px per la colonna pagine;
  - le icone meteo a 28 px;
  - avviso a 40 px.
- **Nessun carattere di testo nuovo e nessun colore nuovo.** "Città", "Île-de-France" e gli accenti europei sono già negli intervalli dei `font_it_*`.
- Memoria: circa 3 KB.

---

## 12. Modifiche alle SPEC precedenti

**SPEC_HOME_GIORNI §6 e SPEC_COMPUTER §4 (Impostazioni):** nuova riga "Città del meteo" a `88,224` (§3). **Informazioni scende a `88,304`.**

**SPEC_HOME_GIORNI §2 (navigazione):** righe in più:
- Impostazioni → riga Città del meteo → Città del meteo;
- pagina a schede, Oggi → blocco del giorno → Città del meteo;
- Home → riquadro Meteo in "Posizione non trovata" → Città del meteo.

Il resto è nella tabella del §2 di questa SPEC.

**SPEC_HOME_GIORNI §3.4 (riquadro Meteo):**
- la riga della condizione può avere il nome della città (§8);
- nuovo stato "Posizione non trovata", che si tocca (§8).

**SPEC_HOME_GIORNI §8.1 (selettore dei giorni):**
- nel giorno Oggi la riga 2 ha il chevron-down, e il blocco diventa un pulsante che apre Città del meteo;
- con una città scelta al volo la riga 2 mostra il segnaposto e il nome, anche negli altri giorni (§7).

**SPEC_HOME_GIORNI §8.2:** uscendo dalla pagina si dimentica anche la città scelta al volo, oltre al giorno.

**SPEC_HOME_GIORNI §8.7 (dati dei giorni):** la memoria usa come chiave città + giorno, e le richieste servono anche Oggi per una città scelta al volo (§10.5).

**SPEC_METEO e SPEC_SOLE_ARIA (posizione):** la posizione del meteo è quella della **predefinita**. ip-api.com serve solo per la Posizione attuale (§10.4).

**SPEC 6 §4.18 (tastiera):** nessuna modifica; in Cerca una città il tasto Cerca si accende da 2 caratteri (§5).

**`fase6/CARATTERI_E_ICONE.md`:** aggiungere una sezione "7. Città del meteo" con il §11.

**Non cambiano:**
- la barra laterale e le pagine di Spotify;
- il contenuto delle schede Meteo, Sole e luna, Aria;
- il computer;
- colori e caratteri.

---

## 13. Da verificare sul dispositivo (Claude Code)

1. **Geocoding in italiano:**
   - con `language=it` arrivano "Parigi", "Londra", "Île-de-France"?
   - Per l'Italia `admin2` è "Provincia di Milano" o solo "Milano"? Se è solo il nome, scrivi "Provincia di" davanti o lascia così: basta che si distinguano i "San Giovanni".
   - Le frazioni hanno `population`?
2. **Temperature con più coordinate:** la richiesta del §10.3 con 9 coppie risponde in un solo elenco, nello stesso ordine? Tempo e dimensione?
3. **Stella e riga:** la stella è a 8 px dalla zona della temperatura. Prova col dito che toccare la stella non apra il meteo, e viceversa.
4. **Cestino senza conferma:** se Valerio toglie città per sbaglio, aggiungo una conferma (il cestino diventa un pulsante rosso "Togli" al primo tocco).
5. **Luce al 15%:** la stella grigia `#535353` e il nome della predefinita `TEXT_3` sotto "Oggi" si vedono? Il verde della stella è l'unico segno forte della riga.
6. **Trattini con il Wi-Fi attivo:** il pulsante verde "Aggiungi" in alto, come gli altri pulsanti verdi.
7. **Fusi orari:** prova con Londra e con New York. Le ore della scheda Meteo devono essere quelle del posto, e "Prossime 24 ore" dell'aria deve partire dall'ora del posto.
8. **Caratteri:** un nome con lettere fuori dagli intervalli (per esempio "Đà Nẵng") esce da `clean_text()` senza le lettere mancanti. Se capita, meglio sostituirle con la lettera senza segni ("Da Nang") invece di toglierle.
9. **Riquadro Meteo con la città:** "Temporale con grandine" con "Reggio nell'Emilia" deve accorciarsi senza sovrapporsi.
10. **RAM:** la ricerca usa la stessa connessione semplice del meteo, chiusa dopo la risposta. Controlla la RAM interna libera durante una ricerca con la sessione TLS di Spotify aperta: deve restare sopra i ~40 KB.
