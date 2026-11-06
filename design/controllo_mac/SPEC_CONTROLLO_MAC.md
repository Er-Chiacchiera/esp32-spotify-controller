# Specifica · Comandi del Mac

Risposta a `design/BRIEF_CONTROLLO_MAC.md`. Si aggiunge a `computer/SPEC_COMPUTER.md`, `home_giorni/SPEC_HOME_GIORNI.md` e `fase6/SPEC_FASE6.md`: quello che qui non è nominato resta com'è. I nomi dei colori (`TEXT_2`, `SURFACE`, `WARN`…) sono quelli della SPEC 6 §3.1, più `AQ_2` della SPEC_SOLE_ARIA §5.1.

**In breve**
- La pagina Computer diventa **a due schede**: **Stato** (quella di oggi) e **Comandi** (nuova), come la pagina Meteo. Il titolo lascia il posto alle schede, e il nome del Mac va a destra.
- La scheda Comandi ha:
  - in alto il **Volume del Mac**, con il cursore e il pulsante muto;
  - due **interruttori grandi**: Microfono e Tieni sveglio;
  - due **azioni con conferma**: Spegni lo schermo e Metti in stop.
- **Accesso rapido:** un pulsante nuovo in alto nella Home, a sinistra delle cuffie, apre subito i comandi. È anche la **spia**: quando il microfono è muto diventa un microfono barrato **arancio**, così ci si accorge di averlo lasciato spento.
- **Due volumi:** quello del Mac si chiama sempre "Volume del Mac", con l'icona del portatile. Il cursore di Now playing resta quello di Spotify, senza modifiche.
- **Conferma** per stop e schermo: la riga delle due azioni diventa una domanda con "Annulla" e il pulsante verde. Niente tocco lungo.
- **Abbinamento** nella scheda Comandi: un codice di 6 cifre grande, nel carattere della temperatura. Si apre da sola finché il Mac non è abbinato; da Impostazioni › Computer si rifà.
- **Gruppo B:** sì al muto dentro Teams e all'uscita audio, forse al Non disturbare, no alla modalità scura (§9).
- Servono **6 icone in più**, nessun carattere e nessun colore nuovo.

Mockup 800×480 in `controllo_mac/mockup/`, disegnati nel canvas alla pagina "Comandi del Mac" (https://claude.ai/artifact/2BmkheovXWkQTH5Pqxk8tw):

| File | Cosa mostra |
|---|---|
| `01_home.png` | Home con il pulsante del Mac (portatile) |
| `02_home-microfono-muto.png` | Home con il microfono muto: pulsante arancio |
| `03_comandi.png` | scheda Comandi, tutto normale |
| `04_comandi-microfono-muto.png` | microfono muto e Mac tenuto sveglio; uscita sulle AirPods |
| `05_comandi-audio-muto-in-corso.png` | audio muto; "Tieni sveglio" appena toccato, in attesa della risposta |
| `06_conferma-stop.png` | conferma prima dello stop |
| `07_conferma-schermo.png` | conferma prima di spegnere lo schermo |
| `08_comando-non-riuscito.png` | il Mac non ha risposto entro 2 s |
| `09_mac-in-stop.png` | subito dopo lo stop |
| `10_non-raggiungibile.png` | Mac spento, in stop da tempo o su un'altra rete |
| `11_abbina-codice.png` | non abbinato: codice da scrivere sul Mac |
| `12_abbinata.png` | abbinamento riuscito |
| `13_programma-vecchio.png` | il programma sul Mac non sa ricevere comandi |
| `14_stato.png` | scheda Stato con l'intestazione nuova |
| `15_impostazioni-abbina.png` | Impostazioni, Mac collegato ma non abbinato |
| `16_impostazioni-abbinata.png` | Impostazioni, Mac abbinato |
| `17_abbina-di-nuovo.png` | nuovo abbinamento aperto da Impostazioni |

Nei mockup delle Impostazioni il nome della rete Wi-Fi è "Rete di casa", di fantasia.

---

## 1. Risposte alle domande del brief

**1. Dove stanno i comandi.**
- In una **seconda scheda della pagina Computer**. La pagina Stato è piena, e Stato e Comandi parlano dello stesso Mac. Le schede sono già note dalla pagina Meteo e non aggiungono passi alla pila indietro.
- Una pagina a sé avrebbe un'altra intestazione con lo stesso nome. Mettere i comandi nella pagina di oggi vorrebbe dire togliere dati che servono.

**2. Accesso rapido: sì, dalla Home.**
- Un **pulsante 64×64 nella barra in alto della Home**, a `472,0`, prima di Ascolti. È lo stesso posto dove Now playing ha la casa. Il chip, largo al massimo 320, finisce a x 336: c'è spazio.
- Dalla Home i comandi sono a **un tocco**. La Home è anche la schermata dove la scheda torna da sola dopo 2 minuti, quindi è quella che si vede di più.
- Il pulsante fa anche da **spia**:
  - microfono muto: microfono barrato `WARN`;
  - audio muto: altoparlante barrato `TEXT_2`;
  - altrimenti: il portatile.
- La barra laterale è piena (8 voci in 480 px) e resta com'è.
- Il riquadro piccolo del computer non basta: compare solo se sole e aria gli lasciano posto.

**3. Due volumi: nomi, icone e posti diversi.**
- Il volume del Mac sta solo nella scheda Comandi, con titolo **"Volume del Mac"** e l'icona del **portatile**. A destra c'è il nome dell'uscita ("MacBook Pro Speakers", "AirPods Pro").
- Il cursore di Now playing resta com'è: **altoparlante**, senza titolo, dentro la pagina della musica. Comanda il volume di Spotify sul dispositivo che suona, che può anche non essere il Mac.
- Il cursore del Mac è più grosso (binario 8, pomello 28, contro 4 e 16) e ha il valore in grande: anche così non si scambiano.

**4. Comandi "pericolosi": conferma a due tocchi.**
- Il tocco su "Spegni lo schermo" o "Metti in stop" trasforma la riga delle due azioni in una domanda, con **"Annulla"** e il pulsante verde dell'azione (§3.4).
- Il pulsante verde risponde solo **0,5 s dopo** essere comparso, così un doppio tocco veloce non conferma per sbaglio.
- Il tocco lungo non si usa: in LVGL al rilascio preme anche il pulsante.

**5. Stati:** §5.

**6. Abbinamento.** Si fa nella **scheda Comandi**, che lo mostra da sola finché il Mac non è abbinato (`11`).
- In Impostazioni la riga Computer ha un pulsante "Abbina" (o "Abbina di nuovo", per cambiare Mac) che apre la stessa schermata.
- Il codice è grande, in `font_temp_64`, in due gruppi da 3 cifre (§6).

**7. Gruppo B:** §9.

---

## 2. Navigazione

| Da | Tocco | Va a |
|---|---|---|
| Home | pulsante del Mac (`472,0`) | pagina Computer, scheda **Comandi** |
| Home | riquadro piccolo Computer | pagina Computer, scheda **Stato** (come oggi) |
| Impostazioni | riga Computer | pagina Computer, scheda **Stato** (come oggi) |
| Impostazioni | pulsante "Abbina" / "Abbina di nuovo" | pagina Computer, scheda **Comandi**, con il codice (`11`, `17`) |
| Pagina Computer | scheda Stato / Comandi | cambia scheda **sul posto**, senza aggiungere un passo alla pila indietro |
| Pagina Computer | freccia indietro | da dove si è arrivati (Home o Impostazioni), da qualsiasi scheda |
| Scheda Comandi | Spegni lo schermo / Metti in stop | conferma sul posto (`06`, `07`) |
| Conferma | Annulla, oppure 5 s senza tocchi | torna alle due azioni |
| Conferma | Spegni | comando; poi toast "Schermo del Mac spento" e si resta sulla scheda |
| Conferma | Metti in stop | comando; poi la scheda mostra "è in stop" (`09`) |
| Abbinamento riuscito (`12`) | "Vai ai comandi", oppure 3 s | i comandi (`03`) |
| Codice da Impostazioni (`17`) | Annulla | indietro (Impostazioni); l'abbinamento di prima resta valido |

- La pagina ha la barra laterale con la voce **Home** accesa, come oggi.
- Il ritorno automatico alla Home dopo 2 minuti vale anche qui. Una conferma aperta si chiude senza mandare niente.

---

## 3. Scheda Comandi — `03`-`08`

### 3.1 Intestazione (uguale nelle due schede)

Sostituisce l'intestazione della SPEC_COMPUTER §6.

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Freccia indietro | 88,0 | 64×64 | come oggi |
| Scheda "Stato" | 152,0 | 104×64 | schede SPEC 6 §4.10, come la pagina Meteo: `SB20`, normale `TEXT_3`, selezionata `TEXT` + sottolineatura `ACCENT` alta 3 a y 55 |
| Scheda "Comandi" | 256,0 | 128×64 | come sopra; il testo è largo 92 px |
| Testo a destra | allineato a destra a x 776, y 21, L max 368 | — | collegato: pallino 10 `AQ_2` + 8 px + nome del Mac `M16 TEXT_3`, troncato con "…"; non raggiungibile: avviso 20 `WARN` + 8 px + "ultimo dato alle 18:40" `M16 TEXT_2`; Mac appena messo in stop: niente |

Il titolo con il nome a `156,17` **sparisce**: lo sostituiscono le schede, e il nome va nel testo a destra. La parola "collegato" non serve più: lo dice il pallino verde accanto al nome.

Il contenuto usa la stessa griglia della scheda Stato: margine 16, distanza 8, da `104,72` a `776,464`.

### 3.2 Volume del Mac — `104,72` 672×136, `SURFACE`, raggio 12

Coordinate relative al pannello.

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Icona + titolo | 20,15 | — | laptop 28 `TEXT` + 12 px + "Volume del Mac" `SB20 TEXT` |
| Uscita | allineata a destra a x 652, y 19, L 300 | — | `M16 TEXT_2`, il nome che manda il Mac ("MacBook Pro Speakers", "AirPods Pro"), troncato con "…" |
| Pulsante muto | 20,64 | cerchio 56 | normale: fondo `RAISED`, volume-up 28 `TEXT`; **muto**: fondo `TEXT`, volume-mute 28 `#000000`; premuto: fondo `KEY_PRESSED` (`#5A5A5A`) in tutti e due i casi |
| Cursore | binario 96,88, 472×8, raggio 4 | tocco 96-568 × 64-120 | binario `RAISED`; parte piena e pomello (cerchio 28, centrato sul valore, y 78) `TEXT`; **muto**: parte piena e pomello `DISABLED_FG` |
| Valore | allineato a destra a x 652, y 73, L 64 | — | `SB30 TEXT`, da "0" a "100"; **muto**: "Muto" `SB20 TEXT_2` a y 79 |

Cursore:
- È un `lv_slider` 0-100, con `ext_click_area` per arrivare a 56 di altezza toccabile.
- Il valore segue il dito subito. Al Mac partono al massimo 6-7 comandi al secondo, l'ultimo sempre al rilascio.
- Mentre il dito è sul cursore, i valori che arrivano dal Mac **non** spostano il pomello. Dopo il rilascio sì, se diversi.
- Toccare o trascinare il cursore con l'audio muto lo riattiva, come fa il Mac con i tasti del volume.
- Nessuna animazione: il pomello salta al valore nuovo.

### 3.3 Interruttori — 2 schede 332×140 a y 216, x 104 e 444

Tutta la scheda è un pulsante: il tocco inverte lo stato. Coordinate relative alla scheda.

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Cerchio | 16,16 | 56×56 | icona 28 al centro, colori sotto |
| Titolo | 88,19 | L 228 | `SB20 TEXT` |
| Stato | 88,47 | — | `M16`: spento `TEXT_2`, acceso o muto `TEXT` |
| Dettaglio | 16,88 | L 300, max 2 righe | `M16 TEXT_3`; nella scheda arancio `TEXT_2` |

| Interruttore | Stato | Fondo scheda | Cerchio | Icona | Stato (testo) | Dettaglio |
|---|---|---|---|---|---|---|
| Microfono | acceso | `SURFACE` | `RAISED` | microphone `TEXT` | "Acceso" | nome del microfono: "MacBook Pro Microphone" |
| Microfono | **muto** (`04`) | `WARN_BG` + bordo 1 `WARN` | `WARN` | microphone-slash `#000000` | "Muto" | "Nessuno ti sente, anche se Teams mostra il microfono acceso." |
| Tieni sveglio | spento | `SURFACE` | `RAISED` | coffee `TEXT` | "Spento" | "Il Mac va in stop da solo, come sempre." |
| Tieni sveglio | **acceso** (`04`) | `SURFACE` | `ACCENT` | coffee `#000000` | "Acceso" | "Il Mac non va in stop e non spegne lo schermo." |

- **Premuto:** fondo `SURFACE_PRESSED`, anche nella scheda arancio (il bordo sparisce mentre è premuta).
- Il microfono muto è l'unico caso **arancio**: è l'unico stato che può creare un problema senza che te ne accorga. "Tieni sveglio" acceso è solo un'opzione attiva, quindi verde.
- La frase su Teams descrive il caso che Valerio ha visto davvero: Teams mostra il microfono acceso anche quando il Mac lo tiene a zero.

### 3.4 Azioni — 2 schede 332×100 a y 364, x 104 e 444

| Elemento | x,y | Stile |
|---|---|---|
| Cerchio | 16,22 | 56×56 `RAISED`, icona 28 `TEXT` |
| Titolo | 88,24, L 228 | `SB20 TEXT` |
| Dettaglio | 88,52, L 228 | `M16 TEXT_3`, 1 riga |

| Azione | Icona | Titolo | Dettaglio |
|---|---|---|---|
| Spegni lo schermo | desktop | "Spegni lo schermo" | "Lo riaccendi dal Mac" |
| Metti in stop | moon | "Metti in stop" | "Si sveglia solo dal Mac" |

Premuto: fondo `SURFACE_PRESSED`.

**Conferma** (`06`, `07`). Al tocco, le due schede lasciano il posto a un pannello unico `104,364` 672×100, fondo `SURFACE`, bordo 1 `PLACEHOLDER_FG` (`#535353`), raggio 12. Coordinate relative al pannello:

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Cerchio | 16,22 | 56×56 | come la scheda toccata (moon o desktop) |
| Domanda | 88,24, L 264 | — | `SB20 TEXT`: "Mettere in stop il Mac?" / "Spegnere lo schermo?" |
| Spiegazione | 88,52, L 264 | — | `M16 TEXT_2`: "Si sveglia solo dal Mac." / "Lo riaccendi dal Mac." |
| Annulla | 352,22 | 136×56 | `bs` con bordo `#535353`, come "Cambia Wi-Fi" |
| Conferma | 500,22 | 156×56 | `bp` verde: "Metti in stop" / "Spegni" |

- La conferma risponde solo **0,5 s dopo** essere comparsa; prima il tocco si ignora, e il pulsante non cambia aspetto.
- Si chiude da sola dopo **5 s** senza tocchi, oppure con Annulla, senza mandare niente.
- Dopo la conferma, il pulsante verde mostra lo spinner 20 sul verde (SPEC 6 §4.14) finché arriva la risposta.
  - **Schermo:** toast "Schermo del Mac spento", e la riga torna alle due azioni. Il Mac resta raggiungibile: si spegne solo lo schermo.
  - **Stop:** la scheda mostra il blocco "è in stop" (`09`, §5). Il pulsante della Home sparisce subito, senza aspettare i 30 s della SPEC_COMPUTER.

### 3.5 Aggiornamento

- Con la scheda aperta, i valori arrivano dal Mac dopo ogni comando (di solito entro 0,5 s) e comunque con lo stato periodico ogni 5 s. Si ridisegna solo quello che cambia.
- Un cambio fatto sul Mac (tasti del volume, menu) si vede entro 1-5 s.
- Mentre un comando è in corso, l'interruttore toccato tiene il suo aspetto vecchio con lo spinner, e gli altri tocchi sullo stesso interruttore si ignorano.

---

## 4. Pulsante del Mac nella Home — `01`, `02`

Nuovo elemento della barra in alto (SPEC_HOME_GIORNI §3.1):

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Comandi del Mac | 472,0 | 64×64 | `ibtn` come le altre icone in alto, icona 28 |

| Situazione (in quest'ordine) | Icona |
|---|---|
| Microfono muto | microphone-slash 28 `WARN` (`02`) |
| Audio muto | volume-mute 28 `TEXT_2` |
| Normale | laptop 28 `TEXT_2` (`01`) |

- C'è solo se il Mac è **abbinato, raggiungibile e con il programma che sa ricevere comandi**. Altrimenti il posto resta vuoto, senza fondo e senza tocco.
- Tocco: pagina Computer, scheda Comandi.
- La spia si aggiorna con lo stato del Mac (entro 5 s), con `ui_set_text()`.

---

## 5. Stati della scheda Comandi

| Stato | Quando | Scheda Comandi | Pulsante Home | Impostazioni (sotto) |
|---|---|---|---|---|
| **Normale** (`03`, `04`) | abbinato, raggiungibile | §3 | sì | "Valerio's MacBook Pro · collegato" |
| **Comando in corso** (`05`) | fra il tocco e la risposta | spinner 20 nel cerchio dell'interruttore o del muto (sul verde o sull'arancio: spinner su colore, SPEC 6 §4.14); il cursore non ne ha | sì | — |
| **Non riuscito** (`08`) | nessuna risposta entro 2 s | il controllo torna allo stato di prima (il cursore all'ultimo valore del Mac) + toast di errore "Il Mac non ha risposto. Riprova." | sì | — |
| **In stop** (`09`) | dopo uno stop confermato | blocco: moon 40 `TEXT_3`, "Valerio's MacBook Pro è in stop", "Si sveglia solo dal Mac. I comandi tornano quando si riaccende." | no | come la SPEC_COMPUTER |
| **Non raggiungibile** (`10`) | come la SPEC_COMPUTER §7 | blocco: laptop 40 `TEXT_3`, "Valerio's MacBook Pro non raggiungibile", "Ultimo dato alle 18:40. I comandi tornano quando il Mac è acceso e sulla stessa rete Wi-Fi." | no | "… · ultimo dato alle 18:40" |
| **Mai avviato** | come la SPEC_COMPUTER §7 | lo stesso blocco della scheda Stato ("Nessun computer collegato") | no | "Nessun computer collegato" |
| **Programma vecchio** (`13`) | il Mac manda lo stato ma non dichiara di saper ricevere comandi | blocco: laptop 40 `TEXT_3`, "Programma del Mac da aggiornare", "La versione sul Mac manda i dati ma non riceve comandi. Aggiornala dal computer: i comandi compaiono da soli." | no | "… · collegato" |
| **Non abbinato** (`11`) | raggiungibile, programma nuovo, nessun abbinamento | codice (§6) | no | "… · collegato, da abbinare" + pulsante "Abbina" (`15`) |
| **Abbinamento riuscito** (`12`) | il Mac ha scritto il codice giusto | blocco: cerchio 80 `ACCENT` con check 40 `#000000`, "Abbinata a Valerio's MacBook Pro", "Da adesso puoi comandare il Mac da qui.", `bp` 200×56 "Vai ai comandi" 24 px sotto; dopo 3 s passa da solo ai comandi | sì | "… · collegato" + "Abbina di nuovo" (`16`) |

Blocco (come la SPEC_COMPUTER §7, centrato in 88-800):
- cerchio 80 `SURFACE` a **y 120**;
- titolo `SB20 TEXT` 24 px sotto il cerchio;
- testo `M16 TEXT_2`, L 520, al massimo 2 righe, 8 px sotto il titolo.

Se più problemi capitano insieme, vince il primo della tabella in quest'ordine: non raggiungibile / mai avviato > in stop > programma vecchio > non abbinato.

Toast (SPEC 6 §4.12, nuovi testi):
- informazione: "Schermo del Mac spento";
- errore: "Il Mac non ha risposto. Riprova.", "Codice scaduto: ne ho fatto uno nuovo." (§6).

---

## 6. Abbinamento — `11`, `17`

Al posto del contenuto della scheda Comandi, centrato in 88-800:

| Elemento | y | Stile |
|---|---|---|
| Cerchio | 80 | 80×80 `SURFACE`, link 40 `TEXT_2` |
| Titolo | 176 | `SB20 TEXT`: "Abbina la scheda al Mac" |
| Testo | 208, L 520 | `M16 TEXT_2`, 2 righe: "Per comandare il Mac da qui, scrivi questo codice sul Mac quando il programma della scheda te lo chiede." |
| Riquadro del codice | 264 | `284,264` 320×88, `SURFACE`, raggio 12 |
| Codice | dentro il riquadro | `font_temp_64 TEXT`, due gruppi da 3 cifre ("482" e "913") con 32 px fra loro, centrati |
| Scadenza | 362 | `M16 TEXT_3`: "Valido fino alle 16:52" |
| Pulsanti | 396 | `bs` alti 56, bordo `#535353`. Primo abbinamento: "Nuovo codice" 200 a x 344. Da "Abbina di nuovo": "Annulla" 160 a x 258 e "Nuovo codice" 200 a x 430. |

Regole:
- Il codice è casuale, di 6 cifre, valido **10 minuti**. Dopo **5 tentativi sbagliati** dal Mac la scheda ne fa uno nuovo e lo dice con il toast "Codice scaduto: ne ho fatto uno nuovo."; allo scadere fa lo stesso.
- "Nuovo codice" sostituisce subito quello vecchio.
- Il codice serve **solo** ad abbinare. Dopo, la scheda e il Mac usano una chiave loro, salvata in NVS e sul Mac, e il codice non vale più. La scheda accetta comandi solo dal Mac abbinato.
- "Abbina di nuovo" non cancella niente: l'abbinamento vecchio resta valido finché un Mac non scrive il codice nuovo. Così "Annulla" non rompe niente.
- Il carattere `font_temp_64` ha già tutte le cifre; lo spazio fra i gruppi sono due etichette separate.

---

## 7. Impostazioni — `15`, `16`

Riga Computer a `88,144` (SPEC_COMPUTER §4, SPEC_CITTA §3), con l'azione facoltativa delle Impostazioni (SPEC_HOME_GIORNI §6): `bs sm` 176×48 a `512,16`, bordo `#535353`.

| Stato | Sotto | Azione |
|---|---|---|
| abbinato | "Valerio's MacBook Pro · collegato" | "Abbina di nuovo" → codice (`17`) |
| non abbinato | "Valerio's MacBook Pro · collegato, da abbinare" | "Abbina" → codice (`11`) |
| programma vecchio, non raggiungibile, mai avviato | come la SPEC_COMPUTER §4 | nessuna |

Il tocco sul resto della riga apre la scheda Stato, come oggi.

---

## 8. Icone e caratteri da generare

| Carattere | Aggiungere | Uso |
|---|---|---|
| `font_ic_28` | microphone `0xF130`, microphone-slash `0xF131`, volume-mute `0xF6A9`, coffee `0xF0F4` | interruttori, muto, pulsante della Home |
| `font_ic_40` | link `0xF0C1`, check `0xF00C` | abbinamento, abbinamento riuscito |

Già presenti: laptop 28/40, desktop 28, moon 28/40 (fra le icone del meteo), volume-up 28, avviso 20. Nessun carattere di testo nuovo: le cifre del codice sono in `font_temp_64`. Nessun colore nuovo.

---

## 9. Gruppo B

| Comando | Sì / no | Dove |
|---|---|---|
| **Muto dentro Teams** | **sì, per primo** | Risolve il problema vero: in chiamata gli altri vedono il muto e Teams non dice "microfono acceso". Durante una chiamata Teams, l'interruttore Microfono preme il muto di Teams invece di azzerare il microfono del Mac; lo stato dice "Muto in Teams" e il dettaglio "Chiamata Teams in corso". Fuori dalle chiamate resta il muto del Mac. Nessun posto nuovo. |
| **Uscita audio** | **sì** | Il nome dell'uscita nel pannello Volume diventa un chip (SPEC 6 §4.15, alto 48, allineato a destra a x 652, y 8 nel pannello, nome max 240 + chevron giù 20). Il tocco apre l'elenco delle uscite nel menu della SPEC 6 (§4.11), con il segno di spunta su quella in uso. |
| **Non disturbare** | forse | Terzo interruttore. Le due schede da 332 diventano tre da 216 (la griglia di Processore, Memoria e Disco); il dettaglio passa a L 184 e i testi si accorciano ("Nessuno ti sente", "Non va in stop"). Icona moon serve già allo stop: per Non disturbare usare bell-slash `0xF1F6`. |
| Modalità scura del Mac | no | Si cambia una volta e poi si dimentica: non vale un posto sulla scheda. |

---

## 10. Modifiche alle SPEC precedenti

**SPEC_COMPUTER**
- §2 Navigazione: righe nuove della tabella del §2 qui sopra. La riga "Nella pagina non si tocca nient'altro" non vale più.
- §6 Intestazione: sostituita dal §3.1 qui sopra (schede Stato · Comandi; il titolo con il nome sparisce; a destra pallino + nome). Il contenuto della scheda Stato non cambia (`14`).
- §7 Stati: la colonna "Pagina" vale per la scheda Stato; per la scheda Comandi vale il §5 qui sopra. Nel testo a destra "collegato" diventa il nome del Mac.
- §4 Riga in Impostazioni: testi "da abbinare" e pulsanti "Abbina" / "Abbina di nuovo" (§7 qui sopra).

**SPEC_HOME_GIORNI**
- §3.1 Barra in alto: nuovo pulsante "Comandi del Mac" a `472,0` (§4 qui sopra).

**SPEC 6**
- §4.12 Toast: nuovi testi del §5.

**Now playing:** nessuna modifica.

---

## 11. Da verificare sul dispositivo (Claude Code)

1. **Cursore:** che il pomello 28 si trascini bene col dito vero sul binario da 472 e che il valore segua senza scatti. Che con 6-7 comandi al secondo il Mac non resti indietro (al rilascio, il valore sul Mac deve essere quello sulla scheda entro 1 s).
2. **Trattini:** sotto il cerchio bianco del muto, il binario bianco, il cerchio arancio e il fondo `WARN_BG` del microfono muto, con il Wi-Fi attivo. Se si vedono sul cerchio bianco del muto, usare fondo `RAISED` e icona `WARN` per il muto.
3. **Spia della Home:** che il microfono arancio si veda alla luce del 15%.
4. **Conferma:** che un doppio tocco veloce su "Metti in stop" **non** metta in stop il Mac (attesa di 0,5 s). Che dopo 5 s la conferma si chiuda da sola.
5. **Stop:** dopo la conferma, che compaia subito "è in stop" e sparisca il pulsante della Home. Al risveglio del Mac, che i comandi tornino da soli entro 5 s.
6. **Testi:** "Nessuno ti sente, anche se Teams mostra il microfono acceso." su due righe in L 300; il nome dell'uscita lungo ("Valerio's AirPods Pro Max") troncato con "…" prima di x 352.
7. **Abbinamento:** codice giusto → `12` e poi i comandi da soli; codice sbagliato 5 volte → codice nuovo e toast; "Abbina di nuovo" + Annulla → i comandi funzionano ancora con il Mac di prima.
8. **Programma vecchio:** con la versione di oggi di `ponte_computer.py`, la scheda Comandi deve mostrare `13` e la Home niente pulsante.
9. **Due volumi:** con Spotify che suona sul Mac, muovere il Volume del Mac e controllare che il cursore di Now playing **non** si muova (sono due volumi diversi).
