# Specifica · Sole, luna, aria e pollini

Risposta a `design/BRIEF_SOLE_ARIA.md`. Si aggiunge a `meteo/SPEC_METEO.md`, `fase6/SPEC_FASE6.md` e `fase7/SPEC_FASE7.md`: quello che qui non è nominato resta com'è. I nomi dei colori (`TEXT_2`, `SURFACE`…) sono quelli della SPEC 6 §3.1, più `WX_SUN` della SPEC_METEO §5.

**In breve**
- Sull'Orologio, sotto la fascia del meteo, c'è una **riga nuova con tre voci**: il prossimo evento del sole ("Tramonto 17:00"), l'aria ("Aria moderata") e, a scelta, i pollini o la luna. Home, Cerca e Wi-Fi scendono di 20 px.
- La pagina Meteo diventa una pagina **a tre schede**: **Meteo** (com'è oggi), **Sole e luna** e **Aria**. Il titolo "Meteo" lascia il posto alle schede.
- Ogni voce della riga apre la sua scheda; il meteo apre la scheda Meteo, come prima.
- **Colori:** 6 per le fasce dell'aria e 3 per i livelli dei pollini. Sono sempre accompagnati dal testo, mai da soli.
- **Luna:** 8 piccole immagini, una per fase, in due misure (20 e 64 px). Le genera Claude Code dagli SVG in `sole_aria/luna/`.
- Si aggiungono 3 icone ai caratteri delle icone. Non serve nessun carattere di testo nuovo.

Mockup 800×480 in `sole_aria/mockup/`, disegnati nel canvas alla pagina "Sole e aria":

| File | Cosa mostra |
|---|---|
| `00_colori-e-fasi.png` | legenda: fasce dell'aria, livelli dei pollini, barra della giornata, 8 fasi della luna |
| `01_orologio.png` | 4 novembre, pomeriggio: Tramonto 17:00 · Aria moderata · Falce calante |
| `02_orologio-primavera.png` | maggio: Tramonto 20:12 · Aria buona · **Pollini alti** (la luna lascia il posto ai pollini) |
| `03_orologio-in-arrivo.png` | all'accensione: scheletro anche nella riga nuova |
| `04_orologio-premuto.png` | voce "Aria" premuta |
| `05_sole-luna-falce.png` | scheda Sole e luna di giorno, **falce** calante |
| `06_sole-luna-piena.png` | scheda Sole e luna dopo il tramonto, **luna piena** |
| `07_aria-scadente.png` | scheda Aria con **aria scadente**, pollini **fuori stagione** |
| `08_aria-buona-pollini.png` | scheda Aria con **aria buona**, **pollini alti** |
| `09_aria-non-disponibile.png` | scheda Aria quando la richiesta dell'aria fallisce |
| `10_meteo-schede.png` | scheda Meteo: identica a oggi, cambia solo l'intestazione |

---

## 1. Risposte alle domande del brief

**1. Dove stanno.**
- **Orologio:** una riga sotto la fascia, larga quanto la fascia (720 px) e divisa in **3 posti da 240 px**. Ogni posto coincide con due celle della fascia, quindi la riga sta in colonna con le ore.
- La riga entra perché Home, Cerca, il Wi-Fi e "Cambia Wi-Fi" scendono di 20 px (da y 392 a 412). Il margine in basso passa da 40 a 20 px; in alto il chip sta a 8 px dal bordo, quindi resta equilibrato.
- Niente alternanza automatica tra meteo e aria: una schermata da guardare di sfuggita non deve cambiare da sola.
- **Pagina:** la pagina Meteo diventa a schede, invece di avere due pagine nuove. È un posto solo per "che tempo fa fuori", raggiunto da quattro punti dell'Orologio.

**2. Quanto mostrare.**
- **Orologio:** una sola informazione per voce. Del sole, il prossimo evento: alba o tramonto, con l'ora. Dell'aria, solo il giudizio. Dei pollini, solo il livello più alto. Della luna, il nome della fase.
- **Scheda Sole e luna:**
  - alba, tramonto e ore di luce di oggi, con la differenza rispetto a domani;
  - la barra della giornata con la posizione di adesso;
  - l'indice UV massimo;
  - domani e dopodomani;
  - la luna: fase, percentuale illuminata, giorni alla prossima luna nuova o piena.
- **Scheda Aria:**
  - l'indice e il giudizio di adesso, con la scala delle 6 fasce;
  - l'andamento delle 24 ore in barre;
  - i 4 inquinanti in µg/m³;
  - i pollini presenti, con il livello.

**3. Tocco:** §2.

**4. Colori:** §5. Sono colori medi, né scuri né chiari: si distinguono sul fondo scuro, si leggono al 15% e non creano grandi zone chiare.

**5. Luna:** **8 immagini**, una per fase, in due misure (20 px nella riga, 64 px nella scheda), §6. Il disegno con due cerchi sovrapposti non va bene per le gibbose, e il solo testo è un peccato per l'unica informazione "da guardare" per davvero.

**6. Stati:** §3.3, §4.4 e §4.5.

---

## 2. Navigazione

| Da | Tocco | Va a |
|---|---|---|
| Orologio | blocco di adesso o fascia del meteo | pagina, scheda **Meteo** |
| Orologio | voce del sole o della luna | pagina, scheda **Sole e luna** |
| Orologio | voce dell'aria o dei pollini | pagina, scheda **Aria** |
| Pagina | una scheda | cambia scheda **sul posto**, senza aggiungere un passo alla pila indietro |
| Pagina | freccia indietro | Orologio (da qualsiasi scheda) |

- La pagina tiene la barra laterale **senza voce attiva** (come oggi la pagina Meteo).
- Le schede si possono sempre toccare, anche se i loro dati mancano: la scheda mostra il suo stato (§4.4, §4.5).
- Le voci della riga si possono toccare anche quando il meteo è in errore. Per esempio la luna c'è sempre: apre "Sole e luna", che mostra la luna e lo stato "non disponibile" per il sole.

---

## 3. Orologio — `01`-`04`

Tutto come SPEC_METEO §3, tranne quanto segue.

### 3.1 Cosa si sposta

| Elemento | Prima | Adesso |
|---|---|---|
| Home | `40,392` | `40,412` |
| Cerca | `232,392` | `232,412` |
| Wi-Fi (icona + nome) | fine a x 568, y 405 | fine a x 568, **y 425** |
| Cambia Wi-Fi | `584,392` | `584,412` |

Ora, data, blocco di adesso e fascia **non si spostano**.

### 3.2 Riga nuova

Contenitore `40,356`, 720×48, senza fondo. Tre posti 240×48, a x **40, 280, 520** (y 356), raggio 12.

| Stato del posto | Resa |
|---|---|
| Normale | nessun fondo |
| Premuto (`04`) | fondo `PRESSED #2A2A2A` su tutto il posto (240×48, raggio 12) |

Ogni voce è un gruppo **centrato nel suo posto**: icona 20, 8 px di spazio, testo. Il testo è `M20` in due colori: la parola che dice *cosa* è in `TEXT_2`, il valore in `TEXT`. In LVGL bastano due etichette in una riga flex, oppure un'etichetta con `recolor`.

| Voce | Icona 20 | Testo (`TEXT_2` + `TEXT`) | Apre |
|---|---|---|---|
| Sole | sun `F185` `WX_SUN` | "Tramonto" + "17:00", oppure "Alba" + "06:47" | Sole e luna |
| Aria | wind `F72E`, colore della fascia (§5.1) | "Aria" + "moderata" | Aria |
| Pollini | spa `F5BB`, colore del livello (§5.2) | "Pollini" + "alti" / "medi" | Aria |
| Luna | immagine della fase 20×20 (§6) | solo il nome, in `TEXT`: "Falce calante" | Sole e luna |

**Quale evento del sole:**
- prima dell'alba di oggi: "Alba" con l'alba di oggi;
- fra alba e tramonto: "Tramonto" con il tramonto di oggi;
- dopo il tramonto: "Alba" con l'alba di **domani**.

Si ricalcola al passaggio dell'alba e del tramonto, e quando arrivano dati nuovi.

**Giudizi dell'aria sull'Orologio**, in minuscolo dopo "Aria": buona, discreta, moderata, scadente, molto scadente, pessima.

**Quali voci e in che ordine.** I posti si riempiono da sinistra, saltando le voci senza dati, al massimo 3:

| Priorità | Voce | Compare se |
|---|---|---|
| 1 | Sole | il meteo ha un dato valido (normale o vecchio, SPEC_METEO §3.4) |
| 2 | Aria | l'aria ha un dato valido (normale o vecchio, §4.5) |
| 3 | Pollini | l'aria è valida e almeno un tipo arriva a **Media** o **Alta** nelle prossime 24 ore |
| 4 | Luna | sempre (la calcola la scheda) |

Quindi:
- in genere si vede Sole · Aria · Luna (`01`);
- nella stagione dei pollini si vede Sole · Aria · Pollini (`02`);
- con il meteo in errore si vede Aria · Luna;
- se mancano sia il meteo sia l'aria, si vede solo la Luna, nel primo posto a sinistra.

I posti vuoti restano vuoti e non si toccano.

Con il livello **Bassa** i pollini non tolgono il posto alla luna; si vedono comunque nella scheda Aria.

Misure verificate (Montserrat Medium 20):
- "Aria molto scadente" è larga 204 px, cioè 232 con l'icona: entra in 240;
- "Gibbosa crescente" è larga 187 px, cioè 215 con l'icona.

### 3.3 Stati della riga

| Stato | Quando | Resa |
|---|---|---|
| **In arrivo** (`03`) | dall'accensione finché **sia** il meteo **sia** l'aria hanno avuto il primo tentativo (riuscito o fallito) | tre barre 140×16 `SURFACE`, raggio 8, centrate nei tre posti (y 372). Ferme, senza animazione. Non si toccano. |
| **Normale** | dopo il primo tentativo | regole del §3.2 |

Nella riga non compaiono avvisi di errore o di dato vecchio:
- il blocco del meteo li mostra già;
- l'aria che manca semplicemente non compare;
- l'età del dato dell'aria si vede nella sua scheda (§4.5).

### 3.4 Ridisegni

La riga si ridisegna solo quando cambia una voce:
- arriva un dato nuovo (ogni 30 minuti);
- passa l'alba o il tramonto;
- cambia la fase della luna (una volta al giorno).

Usare `ui_set_text()`, come per il resto.

---

## 4. Pagina a schede (Meteo · Sole e luna · Aria)

### 4.1 Intestazione (uguale nelle tre schede) — `05`-`10`

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Freccia indietro | 88,0 | 64×64 | come oggi |
| Scheda "Meteo" | 152,0 | 104×64 | schede SPEC 6 §4.10: `SB20`, normale `TEXT_3`, selezionata `TEXT` + sottolineatura `ACCENT` 56×3 a y 55, premuta testo `TEXT` |
| Scheda "Sole e luna" | 256,0 | **144**×64 | come sopra; il testo è largo 112 px |
| Scheda "Aria" | 400,0 | 104×64 | come sopra |
| Testo a destra | allineato a destra a x 776, y 21, L max 264 | — | `M16 TEXT_3`, cambia per scheda (sotto); troncato con "…" |

Testo a destra per scheda:
- **Meteo:** "Roma · aggiornato alle 16:15", come oggi; con il dato vecchio, l'icona avviso 20 `WARN` + 8 px prima del testo.
- **Sole e luna:** la data, "Mercoledì 4 novembre" (la più lunga, "Mercoledì 30 settembre", entra in 264).
- **Aria:** "Roma · aggiornato alle 21:00", ora del dato dell'aria; con il dato vecchio, avviso come per il meteo. Negli stati "in arrivo" ed "errore" il testo non c'è (`09`).

Il titolo "Meteo" a `156,17` **sparisce**: lo sostituiscono le schede.

Il contenuto di tutte e tre le schede usa la stessa griglia della scheda Meteo:
- un pannello alto (`104,72`, 672×216; nella scheda Meteo sono due pannelli da 104 con 8 px fra loro);
- due schede `328×168` a `104,296` e `448,296`.

### 4.2 Scheda Meteo — `10_meteo-schede.png`

Identica a SPEC_METEO §4. Cambia solo l'intestazione (§4.1).

### 4.3 Scheda Sole e luna — `05`, `06`

**Pannello Oggi**: `104,72`, 672×216, `SURFACE`, raggio 12. Coordinate relative al pannello.

| Elemento | x,y | Stile |
|---|---|---|
| Icona | 20,15 | sun 28 `WX_SUN` |
| Titolo | 60,16 | `SB20 TEXT`: "Oggi" |
| UV | allineato a destra a x 652, y 19, L max 340 | `M16 TEXT_2`: "Indice UV massimo 3 · moderato" |
| Etichette | x 20 / 244 / 468, y 60 | `M16 TEXT_2`: "Alba" / "Tramonto" / "Ore di luce" |
| Valori | x 20 / 244 / 468, y 84 | `SB30 TEXT`: "06:46" / "17:00" / "10 h 14 min" |
| Differenza | 468,124 | `M16 TEXT_3`: "2 min in meno domani" / "3 min in più domani" / "come oggi domani" |
| Binario della giornata | 20,158 | 632×12, raggio 6, `RAISED #3A3A3A`. Rappresenta 00:00-24:00: x(h) = 20 + h/24·632 |
| Luce | da x(alba) a x(tramonto), y 158 | 12 di altezza, raggio 6, `WX_SUN` |
| Adesso | centrato su x(ora attuale), y 154 | cerchio 20 `TEXT` con bordo 3 `SURFACE` (si stacca dal binario). Si sposta ogni 15 minuti, non ogni minuto. |
| Ore | centrate su x(6), x(12), x(18), y 178, L 60 | `M16 TEXT_3`: "06:00", "12:00", "18:00" |

Formati:
- ore di luce in "10 h 14 min", senza zeri davanti ("9 h 33 min");
- differenza in minuti interi, arrotondata;
- UV: valore arrotondato all'intero + giudizio OMS, in minuscolo: 0-2 "basso", 3-5 "moderato", 6-7 "alto", 8-10 "molto alto", 11 e oltre "estremo".

**Scheda Prossimi giorni**: `104,296`, 328×168, `SURFACE`, raggio 12.

| Elemento | x,y | Stile |
|---|---|---|
| Titolo | 20,16 | `SB20 TEXT`: "Prossimi giorni" |
| Giorno 1 | 20,56 | `M16 TEXT_2`: "Domani" |
| Luce 1 | allineata a destra a x 308, y 56, L 160 | `M16 TEXT_2`: "luce 10 h 12 min" |
| Orari 1 | 20,78 | `M20 TEXT`: "06:47 – 16:59" (trattino medio U+2013, c'è in `font_it_20`) |
| Giorno 2 | 20,112 / 308,112 / 20,134 | come il giorno 1; il nome è il giorno della settimana: "Venerdì" |

**Scheda Luna**: `448,296`, 328×168, `SURFACE`, raggio 12.

| Elemento | x,y | Stile |
|---|---|---|
| Titolo | 20,16 | `SB20 TEXT`: "Luna" |
| Immagine | 20,56 | fase 64×64 (§6) |
| Fase | 100,62, L 208 | `M20 TEXT`: "Falce calante" |
| Illuminazione | 100,90 | `M16 TEXT_2`: "Illuminata al 22%" |
| Prossima | 20,132 | `M16 TEXT_2`: vedi sotto |

La riga "Prossima" parla dell'evento più vicino fra luna nuova e luna piena, contando da domani:
- "Luna nuova tra 5 giorni" / "Luna piena tra 12 giorni";
- con 1 giorno: "Luna piena domani";
- nel giorno della luna piena (o nuova) la fase lo dice già, quindi si parla dell'altro evento: `06` dice "Luna nuova tra 15 giorni".

**Stati:**
- La luna c'è sempre.
- **Meteo in arrivo o in errore**: il pannello Oggi e la scheda Prossimi giorni restano con il fondo `SURFACE`, ma mostrano solo il loro titolo e, sotto, "Alba e tramonto in arrivo…" oppure "Alba e tramonto non disponibili" (`M16 TEXT_3` a 20,60). Nessun mockup: è lo stesso schema.
- **Dato vecchio**: i valori restano; l'avviso c'è solo nella scheda Meteo.

### 4.4 Scheda Aria — `07`, `08`

**Pannello Adesso + 24 ore**: `104,72`, 672×216, `SURFACE`, raggio 12. Coordinate relative al pannello.

| Elemento | x,y | Stile |
|---|---|---|
| Indice | 20,8 | `font_temp_64 TEXT`: "66" (intero, senza decimali) |
| Pallino | dopo l'indice + 20 px, centrato sulla riga del giudizio | cerchio 14, colore della fascia |
| Giudizio | pallino + 10 px, y 16 | `SB24 TEXT`: "Scadente" (§5.1) |
| Sottotitolo | sotto il giudizio, y 50 | `M16 TEXT_2`: "Qualità dell'aria adesso" |
| Scala | 6 segmenti 36×8, raggio 4, a x 420 + i·39, y 36 | ognuno nel colore della sua fascia (§5.1) |
| Segno | centrato su x = 420 + fascia·39 + min((v − 20·fascia)/20, 1)·36, y 30 | cerchio 20 `TEXT`, bordo 3 `SURFACE` (come "adesso" del sole) |
| Estremi | "Buona" a 420,52; "Pessima" allineato a destra a x 652, y 52 | `M16 TEXT_3` |
| Separatore | 20,96 | 632×1 `RAISED` |
| Titolo 24 ore | 20,108 | `M16 TEXT_2`: "Prossime 24 ore" |
| Minimo e massimo | allineato a destra a x 652, y 108, L 420 | `M16 TEXT_3`: "minimo 33 alle 06:00 · massimo 71 alle 19:00" |
| Barre | 24 barre larghe 20, x = 24 + i·26, base a y 182 | altezza = max(4, min(v,100)/100·48); colore della fascia del valore; angoli in alto raggio 2 (in LVGL anche 0 va bene). La barra 0 è l'ora in corso. |
| Ore | centrate sotto le barre 0, 6, 12, 18; y 186, L 60 | `M16 TEXT_3`: "Ora", poi l'ora della barra ("03:00") |

Il caso più largo, "100" + "Molto scadente", finisce intorno a x 385 e la scala comincia a 420.

**Scheda Inquinanti**: `104,296`, 328×168.

| Elemento | x,y | Stile |
|---|---|---|
| Titolo | 20,16 | `SB20 TEXT`: "Inquinanti" |
| Unità | allineata a destra a x 308, y 19 | `M16 TEXT_3`: "µg/m³" (µ e ³ ci sono già) |
| 4 righe | y 56, 84, 112, 140 | pallino 10 a 20,y+6 nel colore della **sua** sotto-fascia (§7); nome `M16 TEXT_2` a 40,y; valore `M16 TEXT` allineato a destra a x 308 |

Le righe sono "PM2,5", "PM10", "Biossido di azoto" e "Ozono". Il valore ha **un decimale con la virgola**: "36,1".

**Scheda Pollini**: `448,296`, 328×168.

Con almeno un tipo sopra 0 nelle prossime 24 ore (`08`):

| Elemento | x,y | Stile |
|---|---|---|
| Titolo | 20,16 | `SB20 TEXT`: "Pollini" |
| Nota | allineata a destra a x 308, y 19 | `M16 TEXT_3`: "prossime 24 ore" |
| Righe (max 4) | y 56, 84, 112, 140 | pallino 10 nel colore del livello (§5.2); nome `M16 TEXT_2`; livello `M16 TEXT` allineato a destra: "Alta" / "Media" / "Bassa" |

- Si mostrano solo i tipi con livello almeno Bassa.
- L'ordine va dal livello più alto al più basso; a parità di livello, dai grani più alti.
- Se i tipi sono più di 4, quelli in più (i più bassi) non si mostrano.
- Il livello si calcola sul **massimo delle prossime 24 ore**, non sul valore di adesso.

Con tutti i tipi a 0 (`07`):

| Elemento | x,y | Stile |
|---|---|---|
| Titolo | 20,16 | come sopra, senza nota |
| Icona | 20,60 | spa 40 `TEXT_3` |
| Testo | 80,56 | `M20 TEXT`: "Nessun polline" |
| Sotto | 80,84 | `M16 TEXT_2`: "Fuori stagione" (da ottobre a gennaio), altrimenti "Nelle prossime 24 ore" |
| In fondo | 20,128 | `M16 TEXT_3`: "I primi arrivano a febbraio." (solo da ottobre a gennaio) |

Se Open-Meteo restituisce `null` invece di 0, si comporta come 0.

### 4.5 Stati dell'aria

L'aria arriva con una richiesta separata, quindi ha stati suoi, con le stesse soglie del meteo:

| Stato | Quando | Scheda Aria | Riga dell'Orologio |
|---|---|---|---|
| **In arrivo** | prima del primo tentativo | blocco stato (sotto) con il titolo "Qualità dell'aria in arrivo…", senza testo; nessun testo a destra | scheletro (§3.3) |
| **Normale** | dato con meno di 2 h | §4.4 | voci Aria (e Pollini) |
| **Dato vecchio** | ultimo dato riuscito fra 2 e 6 h fa | come normale, con l'avviso `WARN` + "Roma · aggiornato alle 15:00" a destra | come normale |
| **Errore** (`09`) | nessun dato riuscito, o l'ultimo ha più di 6 h | blocco stato | le voci Aria e Pollini spariscono |

Il blocco stato segue SPEC 6 §4.9, nella variante senza pulsante e centrato in 88-800:
- cerchio 80 `SURFACE` a y 120, con l'icona wind 40 `WARN`;
- titolo `SB20 TEXT` a y 224: "Qualità dell'aria non disponibile";
- testo `M16 TEXT_2` a y 258: "Riprovo tra 5 minuti. Il meteo e la luna restano aggiornati."

Se l'intervallo dei tentativi è un altro, scrivi "Riprovo tra poco".

La scheda si aggiorna se i dati arrivano mentre è aperta. Diversamente dalla scheda Meteo, che non cambia mai stato, qui si passa da "in arrivo" a normale al volo.

---

## 5. Colori nuovi

Valgono solo per la riga dell'Orologio e per la scheda Aria. Sono tutti **colori di segnale**:
- pallini, segmenti, barre e icone;
- **mai** il colore di un testo; il giudizio è sempre scritto in `TEXT`.

Contrasti misurati: sono tutti almeno 3,7:1 anche sul fondo `PRESSED`, sopra il minimo di 3:1 per gli elementi grafici.

### 5.1 Fasce dell'aria (EAQI)

| Nome | Hex | Indice | Giudizio scheda / Orologio | Contrasto su `BG` / `SURFACE` |
|---|---|---|---|---|
| `AQ_1` | `#4CC3D9` | 0-20 | Buona / buona | 9,0 / 7,5 |
| `AQ_2` | `#5CC48A` | 20-40 | Discreta / discreta | 8,7 / 7,2 |
| `AQ_3` | `#E8CF4A` | 40-60 | Moderata / moderata | 12,0 / 9,9 |
| `AQ_4` | `#F2873A` | 60-80 | Scadente / scadente | 7,4 / 6,1 |
| `AQ_5` | `#E5484D` | 80-100 | Molto scadente / molto scadente | 4,8 / 4,0 |
| `AQ_6` | `#B76FE3` | oltre 100 | Pessima / pessima | 5,7 / 4,7 |

Fascia = `min(floor(v/20), 5)`. I valori esattamente al confine vanno nella fascia **superiore** (20 → Discreta), come fa Open-Meteo.

La sequenza segue quella ufficiale dell'Agenzia europea dell'ambiente (azzurro, verde, giallo, rosso, rosso scuro, viola). Le ultime due sono schiarite, perché sul nero il rosso scuro e il viola scuro non si vedrebbero.

### 5.2 Livelli dei pollini

| Nome | Hex | Livello | Orologio |
|---|---|---|---|
| `POL_LOW` | `#5CC48A` (= `AQ_2`) | Bassa | — (non compare) |
| `POL_MID` | `#E8CF4A` (= `AQ_3`) | Media | "Pollini medi" |
| `POL_HIGH` | `#E5484D` (= `AQ_5`) | Alta | "Pollini alti" |

Classi POLLnet del brief **confermate** (grani/m³, sul massimo delle 24 ore):

| Tipo | Bassa | Media | Alta |
|---|---|---|---|
| Graminacee | 0,1-9,9 | 10-29,9 | ≥ 30 |
| Olivo, artemisia, ambrosia | 0,1-4,9 | 5-24,9 | ≥ 25 |
| Betulla, ontano | 0,1-15,9 | 16-49,9 | ≥ 50 |

Sotto 0,1 il tipo è assente. I nomi da mostrare sono "Graminacee", "Olivo", "Betulla", "Ontano", "Artemisia" e "Ambrosia".

### 5.3 Luna

| Nome | Hex | Uso |
|---|---|---|
| `MOON_LIT` | `#D6D6D6` | parte illuminata |
| `MOON_DARK` | `#3A3A3A` (= `RAISED`) | parte in ombra (si vede tutto il disco, anche con la luna nuova) |

`MOON_LIT` non è bianco puro, così la luna piena a 64 px è una zona chiara più piccola e meno accesa (§10.2).

---

## 6. Immagini della luna

Riferimento in `sole_aria/luna/`, 8 SVG da 64×64:

| File | Fase | Parte illuminata | Lato illuminato |
|---|---|---|---|
| `0_nuova.svg` | Luna nuova | 0 | — |
| `1_falce-crescente.svg` | Falce crescente | 0,22 | destro |
| `2_primo-quarto.svg` | Primo quarto | 0,50 | destro |
| `3_gibbosa-crescente.svg` | Gibbosa crescente | 0,78 | destro |
| `4_piena.svg` | Luna piena | 1 | — |
| `5_gibbosa-calante.svg` | Gibbosa calante | 0,78 | sinistro |
| `6_ultimo-quarto.svg` | Ultimo quarto | 0,50 | sinistro |
| `7_falce-calante.svg` | Falce calante | 0,22 | sinistro |

Geometria, se preferisci generarle da codice:
- disco di raggio r in `MOON_DARK`;
- sopra, la parte illuminata: mezzo disco dal lato illuminato, più (gibbose) o meno (falci) una mezza ellisse con semiasse orizzontale r·|1 − 2k| e verticale r;
- il lato illuminato è quello visto dall'Italia: a destra quando cresce, a sinistra quando cala.

Da generare:
- **64×64** per la scheda Luna e **20×20** per la riga dell'Orologio, 16 immagini in tutto;
- formato `LV_IMG_CF_TRUE_COLOR_ALPHA`, bordi sfumati come i caratteri; il fondo cambia (`BG`, `SURFACE`, `PRESSED` quando la voce è premuta), quindi niente fondo pieno nell'immagine;
- in tutto circa 16 KB di flash.

L'immagine mostra la fase **nominale**, non la percentuale esatta: la percentuale è nel testo. Per scegliere la fase dalla percentuale e dalla direzione, va bene la regola che usi già (8 nomi).

---

## 7. Dati da chiedere a Open-Meteo

**Meteo** (richiesta esistente):
- aggiungi `daily=sunrise,sunset,daylight_duration,uv_index_max` con `forecast_days=3`;
- le ore di luce si possono anche calcolare da alba e tramonto.

**Aria** (richiesta nuova, `air-quality-api.open-meteo.com`):
- `current=european_aqi,pm2_5,pm10,nitrogen_dioxide,ozone,european_aqi_pm2_5,european_aqi_pm10,european_aqi_nitrogen_dioxide,european_aqi_ozone`;
- `hourly=european_aqi,grass_pollen,olive_pollen,birch_pollen,alder_pollen,mugwort_pollen,ragweed_pollen`;
- `forecast_hours=24` (o le prossime 24 ore dall'ora in corso).

Le sotto-fasce `european_aqi_*` servono per i pallini degli inquinanti (§4.4): un valore in µg/m³ da solo non dice in quale fascia sta. Se Open-Meteo non le dà nel blocco `current`, prendile da `hourly` all'ora in corso.

**Ogni quanto:** come il meteo, ogni 30 minuti. In errore riprova ogni 5 minuti.

**Memoria:** 24 valori di indice e 6×24 valori di pollini sono numeri piccoli, si tengono in PSRAM con il resto.

---

## 8. Icone e caratteri da generare

| File | Aggiunte | Uso |
|---|---|---|
| `font_ic_20` | sun `0xF185`, wind `0xF72E`, spa `0xF5BB` | riga dell'Orologio |
| `font_ic_40` | wind `0xF72E`, spa `0xF5BB` | blocco stato dell'aria, pollini fuori stagione |

- Il sole a 28 (`font_ic_28`) e l'avviso 20 ci sono già.
- **Nessun carattere di testo nuovo:** `font_temp_64` ha già le cifre per l'indice, e µ, ³, à, ì e "–" sono negli intervalli dei `font_it_*`.
- Memoria stimata: circa 4 KB per le icone, più circa 16 KB per le immagini della luna (§6).

---

## 9. Modifiche alle SPEC precedenti

**SPEC_METEO §1, "Spazio per altre informazioni in futuro":**
- le due "aree informazioni" restano al meteo;
- si aggiunge una **terza area**: la riga `40,356` 720×48, con 3 posti da 240. Altre informazioni future (calendario, ore ascoltate) potranno prendere uno dei posti con le regole di priorità del §3.2.

**SPEC_METEO §3.1 (Orologio):** Home, Cerca e Cambia Wi-Fi da y **392 a 412**; il gruppo Wi-Fi da y **405 a 425**. Nuova riga del §3.2.

**SPEC_METEO §3.4 (stati):** lo stato "In arrivo" ha anche lo scheletro della riga (§3.3).

**SPEC_METEO §2 e §4 (pagina Meteo):**
- la pagina diventa a **tre schede**;
- il titolo "Meteo" `SB24` a `156,17` è **tolto**; al suo posto ci sono le schede a `152 / 256 / 400, 0`;
- il testo a destra passa da L max 320 a **L max 264** e cambia per scheda (§4.1);
- il contenuto della scheda Meteo non cambia;
- navigazione al §2.

**SPEC 6 §2 (mappa):** la pagina Meteo (con le schede Sole e luna e Aria) si raggiunge dall'Orologio in quattro modi (§2); indietro torna all'Orologio.

**SPEC 6 §3.1 (colori):** aggiunti `AQ_1` … `AQ_6`, `POL_LOW` / `POL_MID` / `POL_HIGH` (alias di colori `AQ_*`), `MOON_LIT` e `MOON_DARK`. Valgono solo come colori di segnale (§5). La regola "niente sfumature, trasparenze o ombre" resta: le immagini della luna hanno solo i bordi sfumati, come i caratteri.

**SPEC 6 §4.10 (schede):** la scheda "Sole e luna" è larga 144 invece di 104.

**`fase6/CARATTERI_E_ICONE.md`:** aggiungere una sezione "4. Sole e aria" con le icone del §8 e le immagini del §6.

**Non cambiano:**
- ora, data, blocco di adesso e fascia dell'Orologio;
- Now playing, Home, liste e barra laterale;
- il contenuto della scheda Meteo.

---

## 10. Da verificare sul dispositivo (Claude Code)

1. **Riga dell'Orologio.**
   - Con "Aria molto scadente" (232 px con l'icona) il gruppo resta dentro il suo posto da 240.
   - Al tocco: posto giusto → scheda giusta; i posti vuoti non reagiscono.
   - Le voci si ridispongono quando il meteo o l'aria vanno in errore.
2. **Trattini con il Wi-Fi attivo:**
   - **luna piena 64 px** (`MOON_LIT` su 64 px): se compaiono i trattini, scurisci `MOON_LIT` a `#B3B3B3`;
   - barra gialla della giornata (632×12 al massimo);
   - barre gialle e arancio dell'aria. Se le barre danno fastidio, abbassane l'altezza massima da 48 a 40.
3. **Luce al 15%,** da 1-2 metri:
   - i valori della riga in `TEXT` devono leggersi; le etichette in `TEXT_2` ("Tramonto", "Aria") possono anche vedersi meno;
   - i colori dei pallini si distinguono? Soprattutto azzurro e verde (Buona e Discreta).
4. **Immagini della luna** a 20 px: la falce si riconosce? Se è troppo sottile, alza la parte illuminata della falce da 0,22 a 0,28, sia per la 20 sia per la 64.
5. **Ridisegni:** la riga e il segno "adesso" del sole non devono ridisegnarsi ogni minuto (§3.4, §4.3).
6. **Stati dell'aria:** forza "in arrivo", "dato vecchio" ed "errore" (come per il meteo, con un comando seriale) e confrontali con `03` e `09`; nel caso errore controlla anche la riga dell'Orologio (Sole · Luna).
7. **Pollini in stagione:** a novembre sono tutti a 0. Per provare `02` e `08` serve un valore finto, per esempio da un comando seriale che imposta i grani di un tipo.
8. **Memoria** dopo la compilazione: circa 20 KB di flash; RAM interna invariata, perché tutti i dati stanno in PSRAM.
