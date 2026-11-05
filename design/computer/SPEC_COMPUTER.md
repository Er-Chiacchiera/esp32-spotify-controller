# Specifica · Stato del computer

Risposta a `design/BRIEF_COMPUTER.md`. Si aggiunge a `home_giorni/SPEC_HOME_GIORNI.md` e a `fase6/SPEC_FASE6.md`: quello che qui non è nominato resta com'è. I nomi dei colori (`TEXT_2`, `SURFACE`, `WARN`…) sono quelli della SPEC 6 §3.1, più `AQ_2` e `AQ_5` della SPEC_SOLE_ARIA §5.1.

**In breve**
- Sulla Home il computer è un **riquadro piccolo**, come sole, aria e luna:
  - normalmente mostra la **batteria** ("Batteria 80%");
  - quando c'è qualcosa da guardare (batteria scarica, disco quasi pieno, caldo…) lo dice, con l'icona arancio o rossa.
- **Priorità:** il riquadro prende il posto della **luna** quando il computer è acceso e raggiungibile. Se c'è un avviso, passa anche davanti ai pollini. Quando il computer dorme, la luna torna.
- Il tocco apre la pagina **Computer**, con batteria, processore, memoria, disco, cuffie e sistema.
- In **Impostazioni** c'è una riga "Computer" con il nome e lo stato del collegamento: da lì si apre la pagina anche quando il riquadro non c'è.
- Servono 11 icone in più e il segno "%" nel carattere grande. Nessun colore nuovo.

Mockup 800×480 in `computer/mockup/`, disegnati nel canvas alla pagina "Computer" (https://claude.ai/artifact/2BmkheovXWkQTH5Pqxk8tw):

| File | Cosa mostra |
|---|---|
| `01_home-computer.png` | Home con il riquadro del computer: Batteria 80%, alimentatore collegato |
| `02_home-avviso.png` | Home con la batteria da guardare: 18%, icona arancio |
| `03_computer.png` | pagina Computer, tutto normale, AirPods al 70% |
| `04_computer-avvisi.png` | pagina con gli avvisi: batteria 18%, processore al massimo, memoria, disco quasi pieno (rosso), cuffie scariche, caldo |
| `05_computer-non-raggiungibile.png` | computer spento, in stop o su un'altra rete |
| `06_computer-mai-avviato.png` | nessun computer ha mai mandato dati |
| `07_computer-fisso.png` | computer fisso senza batteria, nessuna cuffia |
| `08_impostazioni.png` | Impostazioni con la riga Computer |

---

## 1. Risposte alle domande del brief

**1. Dove entra nella Home.**
- In un **riquadro piccolo**. È il modo previsto dalla SPEC_HOME_GIORNI (§1 domanda 7) per le funzioni nuove.
- Il riquadro compare **solo quando il computer manda dati**. Così si vede quando sei alla scrivania e sparisce la sera, quando il Mac dorme: al suo posto torna la luna.
- Ha **due priorità**:
  - **con un avviso** passa davanti ai pollini: un "Batteria 8%" è più urgente;
  - **normale** passa davanti alla luna, ma dietro ai pollini medi o alti.

**2. Cosa mostra: la cosa più utile in quel momento.**
- Normalmente, per un portatile, la **batteria**: il valore cambia poco e si legge da lontano.
- Il processore cambia ogni 5 secondi, quindi sulla Home compare solo se diventa un avviso.
- Per un computer fisso, "Tutto in ordine" con lo spazio libero su disco.

**3. Il tocco** apre la pagina **Computer** (`03`), con tutti i dati, aggiornati ogni 5 secondi mentre è aperta.

**4. Soglie e colori:** §5. Due livelli:
- **attenzione** in `WARN` (arancio);
- **critico** in `AQ_5` (rosso).

Il colore va solo su icone e barre, mai sul testo, come per l'aria.

**5. Stati:** §6.

**6. Impostazioni:** sì, riga "Computer" (§4).

**7. Più computer.** Non è progettato, ma la strada resta aperta:
- il titolo della pagina è il **nome del computer**;
- ogni computer avrebbe il suo riquadro, la sua riga in Impostazioni e la sua pagina;
- l'etichetta del riquadro "Computer" diventerebbe il nome, troncato.

---

## 2. Navigazione

| Da | Tocco | Va a |
|---|---|---|
| Home | riquadro Computer | pagina Computer |
| Impostazioni | riga Computer | pagina Computer (anche se il computer non è raggiungibile o non è mai stato avviato) |
| pagina Computer | freccia indietro | da dove si è arrivati (Home o Impostazioni) |
| pagina Computer | voci della barra | come in tutte le pagine |

- La pagina Computer ha la barra laterale con la voce **Home** accesa, come Meteo e Impostazioni.
- Nella pagina non si tocca nient'altro.
- Il ritorno automatico alla Home dopo 2 minuti vale anche qui.

---

## 3. Riquadro della Home — `01`, `02`

Stesso componente dei riquadri piccoli (SPEC_HOME_GIORNI §3.5): 232×96, fondo `SURFACE`, raggio 12, premuto `SURFACE_PRESSED`.

**Correzione per tutti i riquadri piccoli:** l'etichetta comincia **8 px dopo l'icona**, invece che sempre a x 44. Le icone del computer (batteria, portatile, memoria) sono larghe circa 25 px e toccherebbero il testo. Per sole, aria, pollini e luna non cambia niente: sono larghe 20, quindi l'etichetta resta a 44.

| Elemento | Contenuto |
|---|---|
| Icona 20 | vedi tabella sotto, `TEXT` se normale, `WARN` o `AQ_5` se c'è un avviso |
| Etichetta | "Computer" (`M16 TEXT_2`) |
| Valore | `SB20 TEXT`, L 200 |
| Dettaglio | `M16 TEXT_3`, L 200 |

**Normale:**

| Caso | Icona | Valore | Dettaglio |
|---|---|---|---|
| Portatile in carica | batteria del livello | "Batteria 64%" | "piena tra 40 min"; senza stima: "in carica" |
| Portatile, alimentatore collegato e non in carica | batteria del livello | "Batteria 80%" | "alimentatore collegato" |
| Portatile, carica completa | batteria piena | "Batteria 100%" | "carica completa" |
| Portatile a batteria | batteria del livello | "Batteria 72%" | "3 h 12 min rimanenti"; senza stima: "a batteria" |
| Computer fisso | desktop | "Tutto in ordine" | "disco 612 GB liberi" |

Icona della batteria secondo il livello:
- dall'88%: `F240` piena;
- dal 63%: `F241` tre quarti;
- dal 38%: `F242` metà;
- dal 13%: `F243` un quarto;
- sotto: `F244` vuota.

**Con un avviso** (`02`): il riquadro mostra l'avviso più importante (§5.2). L'icona è quella dell'argomento, nel colore del livello.

| Avviso | Icona | Valore | Dettaglio |
|---|---|---|---|
| Batteria | batteria del livello | "Batteria 18%" | "35 min rimanenti" / "collega l'alimentatore" |
| Temperatura | thermometer-half | "Fa caldo" | "il Mac rallenta" |
| Disco | hdd | "Disco quasi pieno" | "8,2 GB liberi" |
| Memoria | memory | "Memoria quasi piena" | "17% libera" |
| Processore | microchip | "Processore al massimo" | "da 2 min" |
| Cuffie | headphones | "Cuffie 15%" | il nome: "AirPods Pro" |

**Ridisegno:** solo quando cambia il testo o l'icona, con `ui_set_text()`. La batteria cambia di un punto ogni qualche minuto; il processore sulla Home non c'è, salvo come avviso.

### 3.1 Priorità dei riquadri piccoli (sostituisce la tabella della SPEC_SOLE_ARIA §3.2)

| Priorità | Riquadro | Compare se |
|---|---|---|
| 1 | Sole | il meteo ha un dato valido |
| 2 | Aria | l'aria ha un dato valido |
| 3 | **Computer con avviso** | il computer è raggiungibile e c'è almeno un avviso (§5) |
| 4 | Pollini | almeno un tipo a Media o Alta nelle prossime 24 ore |
| 5 | **Computer normale** | il computer è raggiungibile, senza avvisi |
| 6 | Luna | sempre |

Valgono le regole di prima: al massimo 3 riquadri, riempiti da sinistra, e i posti vuoti non si disegnano. Quindi:
- alla scrivania si vede Sole · Aria · Computer (`01`);
- la sera, con il Mac in stop, si vede Sole · Aria · Luna;
- in primavera con i pollini alti si vede Sole · Aria · Pollini, e il computer ricompare solo se ha un avviso.

---

## 4. Riga in Impostazioni — `08`

Nuova riga fra Wi-Fi e Informazioni, nel formato della SPEC_HOME_GIORNI §6: Wi-Fi a `88,64`, **Computer a `88,144`**, Informazioni a `88,224`.

| Elemento | Contenuto |
|---|---|
| Icona | laptop 28 `TEXT_2` nel cerchio 56 `SURFACE` |
| Titolo | "Computer" |
| Sotto | "Valerio's MacBook Pro · collegato"; "Valerio's MacBook Pro · ultimo dato alle 18:40"; "Nessun computer collegato" |
| Tocco | tutta la riga (premuta: fondo `PRESSED`) → pagina Computer |

Nella cattura di oggi il nome della rete non si vede perché è coperto apposta nella foto (regola del repository pubblico): sulla scheda c'è ("CasaRibaldi · segnale discreto").

---

## 5. Soglie e colori

### 5.1 Colori

Nessun colore nuovo. Si usano:

| Uso | Colore |
|---|---|
| Barra normale | `TEXT_2 #B3B3B3` |
| Barra della batteria in carica | `AQ_2 #5CC48A` |
| Attenzione: icona, barra, pallino | `WARN #FF9800` |
| Critico: icona, barra | `AQ_5 #E5484D` |
| Binario delle barre | `RAISED #3A3A3A` |
| Pallino "collegato" nell'intestazione | `AQ_2` |

Le barre normali sono grigie apposta: il colore compare solo quando c'è qualcosa da guardare. Testi e valori restano sempre `TEXT` o `TEXT_2`.

### 5.2 Quando un dato diventa "da guardare"

In ordine di importanza. A pari livello vince il primo della tabella; il critico vince sempre sull'attenzione.

| Dato | Attenzione | Critico | Note |
|---|---|---|---|
| Batteria | ≤ 20% **a batteria** | ≤ 10% a batteria | collegato all'alimentatore non è mai un avviso |
| Temperatura | il Mac segnala che rallenta per il caldo | — | |
| Disco | meno di 20 GB liberi | meno di 10 GB liberi | in GB e non in percentuale: 37 GB su 494 (7%) non sono un problema |
| Memoria | meno del 20% libera per almeno 30 s | meno del 10% libera per almeno 30 s | il Mac sta bene sopra il 30% |
| Processore | ≥ 90% per almeno 60 s | — | i picchi brevi sono normali |
| Cuffie | ≤ 20% | ≤ 10% | solo con cuffie che mandano la batteria |

Per evitare che l'avviso compaia e sparisca di continuo, esce solo quando il valore torna oltre la soglia con un margine: batteria +3 punti, disco +2 GB, memoria +5 punti, processore sotto l'80% per 30 s, cuffie +3 punti.

---

## 6. Pagina Computer — `03`-`07`

Barra laterale con la voce Home accesa.

**Intestazione:**

| Elemento | x,y | Stile |
|---|---|---|
| Freccia indietro | 88,0 | come sempre |
| Titolo | 156,17, L 364 | `SB24 TEXT`: il nome del computer, troncato con "…" |
| Stato | allineato a destra a x 776, y 21, L 240 | collegato: pallino 10 `AQ_2` + 8 px + "collegato" `M16 TEXT_3`; non raggiungibile: avviso 20 `WARN` + 8 px + "ultimo dato alle 18:40" `M16 TEXT_2` |

**Contenuto** (stessa griglia della pagina Meteo: margine 16, distanza 8):

### 6.1 Batteria — `104,72` 672×136, `SURFACE`, raggio 12

| Elemento | x,y | Stile |
|---|---|---|
| Icona + titolo | 20,15 | batteria 28 del livello (colore dell'avviso, se c'è) + 12 px + "Batteria" `SB20 TEXT` |
| Stima | allineata a destra a x 652, y 19, L 300 | `M16 TEXT_2`: "3 h 12 min rimanenti", "piena tra 40 min"; vuota se il Mac non la dà |
| Percentuale | 20,52 | `font_temp_64 TEXT` con "%" (§7): "80%". "100%" finisce intorno a x 195. |
| Barra | 216,80, 436×12, raggio 6 | binario `RAISED`; pieno = percentuale, colore del §5.1 |
| Stato | 216,100 | `M16 TEXT_2`: "In carica", "Alimentatore collegato · non in carica", "Carica completa", "A batteria" |

**Computer fisso** (`07`): stesso pannello con plug 28 + 12 px + "Alimentazione", "Senza batteria" `M20 TEXT` a 20,60 e "Computer fisso, collegato alla corrente." `M16 TEXT_2` a 20,88.

### 6.2 Processore, Memoria, Disco — 3 schede 216×140 a y 216, x 104 / 332 / 560

| Elemento | x,y | Stile |
|---|---|---|
| Icona + etichetta | 16,15 | icona 20 `TEXT_2` (colore dell'avviso, se c'è) + 8 px + etichetta `M16 TEXT_2` |
| Valore | 16,44 | `SB30 TEXT` |
| Barra | 16,92, 184×8, raggio 4 | binario `RAISED`, pieno nel colore del §5.1 |
| Dettaglio | 16,108, L 184 | `M16 TEXT_3`; con un avviso `TEXT_2` |

| Scheda | Icona | Valore | Barra | Dettaglio |
|---|---|---|---|---|
| Processore | microchip | "29%" | in uso | "in uso"; avviso: "al massimo da 2 min" |
| Memoria | memory | **in uso** = 100 − libera: "52%" | in uso | "in uso · 48% libera" |
| Disco | hdd | GB liberi: "36,9 GB" (un decimale sotto i 100 GB, intero sopra) | spazio **occupato** | "liberi su 494 GB" |

La memoria si mostra come "in uso", per leggerla come il processore: barra più lunga = più carico. Il dato vero, la parte libera, resta nel dettaglio.

### 6.3 Cuffie — `104,364` 332×100

| Elemento | x,y | Stile |
|---|---|---|
| Icona + etichetta | 16,15 | headphones 20 + 8 px + "Cuffie" |
| Nome | 16,44, L 220 | `SB20 TEXT`: "AirPods Pro" |
| Percentuale | allineata a destra a x 316, y 44 | `SB20 TEXT`: "70%" |
| Barra | 16,80, 300×8 | come sopra |

Senza cuffie che mandano la batteria (`07`): "Nessuna cuffia" `M20 TEXT_2` a 16,44 e "Le AirPods mostrano qui la batteria." `M16 TEXT_3` a 16,72.

### 6.4 Sistema — `444,364` 332×100

| Elemento | x,y | Stile |
|---|---|---|
| Icona + etichetta | 16,15 | thermometer-half 20 (`WARN` se fa caldo) + 8 px + "Sistema" |
| Acceso da | 16,44, L 300 | `SB20 TEXT`: "Acceso da 2 giorni e 3 ore"; sotto un giorno "Acceso da 5 ore" o "Acceso da 40 min" |
| Temperatura | 16,72 | normale: "Temperatura normale" `M16 TEXT_3`; caldo: pallino 10 `WARN` + 8 px + "Rallenta per il caldo" `M16 TEXT` |

### 6.5 Aggiornamento

- Con la pagina aperta, i valori si aggiornano all'arrivo dei dati (ogni 5 s), ma si ridisegnano solo le scritte e le barre che cambiano.
- Le barre si spostano di colpo, senza animazione.
- Arrotondamenti: percentuali all'intero; ore e minuti come per il sole ("3 h 12 min").

---

## 7. Stati

| Stato | Quando | Home | Pagina | Impostazioni |
|---|---|---|---|---|
| **Collegato** | ultimo dato da meno di 30 s | riquadro normale o con avviso | §6 (`03`, `04`) | "… · collegato" |
| **Non raggiungibile** (`05`) | il computer ha mandato dati da quando la scheda è accesa, ma l'ultimo ha più di 30 s | riquadro **tolto** (torna la luna o i pollini) | blocco stato, sotto | "… · ultimo dato alle 18:40" |
| **Mai avviato** (`06`) | nessun dato da quando la scheda è accesa | nessun riquadro | blocco stato, con titolo della pagina "Computer" | "Nessun computer collegato" |
| **Computer fisso** (`07`) | il computer dice di non avere batteria | riquadro "Tutto in ordine" | pannello Alimentazione | come collegato |
| **Nessuna cuffia** (`07`) | nessuna cuffia con batteria | — | scheda Cuffie vuota | — |

Blocco stato (SPEC 6 §4.9, senza pulsante, centrato in 88-800):
- cerchio 80 `SURFACE` a y 120, con laptop 40 `TEXT_3`: non è un errore della scheda;
- titolo `SB20 TEXT` a y 224;
- testo `M16 TEXT_2` a y 258, L 520, al massimo 2 righe.

| Stato | Titolo | Testo |
|---|---|---|
| Non raggiungibile | "Valerio's MacBook Pro non raggiungibile" | "Ultimo dato alle 18:40. Il computer è spento, in stop o su un'altra rete Wi-Fi." |
| Mai avviato | "Nessun computer collegato" | "Avvia sul computer il programma che manda i dati alla scheda. Computer e scheda devono essere sulla stessa rete Wi-Fi." |

Se il programma ha un nome che Valerio riconosce (per esempio quello dell'icona sul Mac), mettilo nel testo: "Avvia «…» sul computer…".

L'ora dell'ultimo dato: "alle 18:40" se è di oggi, "ieri alle 18:40" se è di ieri, "il 3 novembre" se è più vecchio.

Se il computer torna raggiungibile mentre la pagina è aperta, la pagina passa subito dal blocco ai dati.

---

## 8. Icone e caratteri da generare

| File | Aggiunte | Uso |
|---|---|---|
| `font_ic_20` | laptop `0xF109`, battery `0xF240` `0xF241` `0xF242` `0xF243` `0xF244`, microchip `0xF2DB`, memory `0xF538`, hdd `0xF0A0`, thermometer-half `0xF2C9` | riquadro della Home, schede della pagina |
| `font_ic_28` | laptop `0xF109`, battery `0xF240`-`0xF244`, plug `0xF1E6` | riga di Impostazioni, pannello Batteria/Alimentazione |
| `font_ic_40` | laptop `0xF109` | blocco stato |
| `font_temp_64` | percento `0x25` | percentuale della batteria |

- Ci sono già: desktop 20, headphones 20, avviso 20.
- Nessun carattere di testo nuovo e nessun colore nuovo.
- Memoria: circa 8 KB.

---

## 9. Modifiche alle SPEC precedenti

**SPEC_HOME_GIORNI §3.5 (riquadri piccoli):**
- l'etichetta comincia 8 px dopo l'icona, invece che a x 44 fisso (per le icone da 20 di oggi resta a 44);
- la tabella dei riquadri ha una riga in più, "Computer" (§3);
- la tabella delle priorità è quella del §3.1 di questa SPEC.

**SPEC_SOLE_ARIA §3.2 (priorità):** sostituita dal §3.1. Sole, Aria, Pollini e Luna mantengono l'ordine fra loro.

**SPEC_HOME_GIORNI §2 (navigazione):** due righe in più: Home → riquadro Computer → pagina Computer; Impostazioni → riga Computer → pagina Computer.

**SPEC_HOME_GIORNI §6 (Impostazioni):** nuova riga "Computer" a `88,144`; Informazioni scende a `88,224`.

**SPEC_HOME_GIORNI §1 domanda 7:** il computer è il primo esempio di funzione aggiunta così: un riquadro piccolo, una pagina con la barra e una riga in Impostazioni.

**`fase6/CARATTERI_E_ICONE.md`:** aggiungere una sezione "6. Computer" con il §8.

**Non cambiano:** il resto della Home, la barra laterale, le pagine di Spotify e del meteo.

---

## 10. Da verificare sul dispositivo (Claude Code)

1. **Icone larghe nei riquadri:**
   - batteria, laptop e memory a 20 px sono larghe circa 25 px; controlla che l'etichetta parta 8 px dopo e che non tocchi l'icona;
   - stessa cosa per la batteria 28 nel titolo del pannello (12 px).
2. **Avvisi che compaiono e spariscono:** prova i margini del §5.2 con valori finti, per esempio con un comando seriale come per il meteo (`COMPUTER PROVA batteria 19`). Il riquadro non deve cambiare a ogni pacchetto.
3. **Il Mac che va in stop:** dopo 30 s il riquadro deve sparire e la luna tornare. Al risveglio il riquadro deve tornare senza riavviare niente.
4. **Ridisegni:** con la pagina aperta si aggiornano solo le scritte e le barre che cambiano; sulla Home, niente ridisegni ogni 5 secondi.
5. **Trattini con il Wi-Fi attivo:** le barre grigie e arancio da 436×12 e 184×8, e la percentuale grande bianca.
6. **Luce al 15%:** "Batteria 80%" sul riquadro, e la differenza fra arancio e rosso negli avvisi.
7. **Tempo rimanente:** quando il Mac non lo dà ancora, il dettaglio dice "a batteria" o "in carica" e la stima nel pannello resta vuota.
8. **Wi-Fi in Impostazioni:** il nome della rete resta come oggi; nelle foto da pubblicare va coperto, lasciando l'icona (§4).
