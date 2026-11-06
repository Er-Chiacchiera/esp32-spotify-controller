# Brief per Claude Design: Controller Spotify, comandi del Mac

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. La tua `computer/SPEC_COMPUTER.md` è sulla scheda e funziona: la pagina Computer mostra batteria, processore, memoria, disco e cuffie del Mac di Valerio.

Adesso Valerio vuole **comandare il Mac** dalla scheda: volume, microfono e qualche altra cosa. Finora la scheda guardava e basta; da qui in poi tocca e il Mac obbedisce.

Le scelte di design le fai tu. A me servono posizioni, misure e colori in numeri, da tradurre in LVGL 8, come sempre.

**Allegati:**
- catture reali 800×480 di oggi:
  - Home: `home_giorni/schermate/02_home_senza_musica.png`;
  - pagina Computer: `computer/schermate/02_computer.png`;
  - Impostazioni: `computer/schermate/09_impostazioni.png`;
  - Now playing, con il suo cursore del volume in basso: `colore_copertina/schermate/01_copertina_vera.png`;
- le SPEC:
  - `computer/SPEC_COMPUTER.md`: §6 pagina Computer, §7 stati;
  - `home_giorni/SPEC_HOME_GIORNI.md`: §3.5 riquadri piccoli, §4 barra laterale;
  - `fase6/SPEC_FASE6.md`: §3 colori e caratteri;
- caratteri e icone: `fase6/CARATTERI_E_ICONE.md`.

---

## 1. Come funziona

- Il programmino sul Mac che oggi manda lo stato ogni 5 secondi impara anche ad **ascoltare**. La scheda gli manda un comando via Wi-Fi e il Mac risponde con lo stato nuovo, di solito in **meno di mezzo secondo**.
- Se la risposta non arriva entro 2 secondi, il comando non è andato: serve uno stato "non riuscito".
- Se il volume si cambia dal Mac (tasti, menu), la scheda lo vede entro 1-5 secondi.
- Quando il Mac non è raggiungibile (spento, in stop, su un'altra rete), i comandi non si possono usare. Vale lo stato "non raggiungibile" della SPEC_COMPUTER.
- **Sicurezza:** chiunque sia sulla stessa rete Wi-Fi potrebbe mandare comandi al Mac. Per questo la scheda e il Mac si **abbinano una volta sola**: la scheda mostra un codice di 6 cifre, che si scrive sul Mac (lo farà Claude Code per Valerio). Senza abbinamento i comandi restano spenti. Il codice si può rifare (per esempio con un Mac nuovo).

## 2. I comandi possibili

Provati sul Mac di Valerio il 6 novembre 2026 (MacBook Pro, macOS). Si dividono in due gruppi.

**Gruppo A: funzionano subito, senza permessi né programmi da installare.**

| Comando | Dati che arrivano alla scheda | Note |
|---|---|---|
| Volume del Mac 0-100 | volume attuale (oggi 63) e nome dell'uscita ("MacBook Pro Speakers", "AirPods Pro"…) | è il volume di **tutto** il Mac |
| Audio muto / attivo | muto sì/no | |
| Microfono muto / attivo | microfono acceso sì/no, nome del microfono ("MacBook Pro Microphone") | abbassa a zero il microfono di tutto il Mac. Valerio ha Microsoft Teams: in chiamata Teams mostra il microfono acceso, ma gli altri non sentono niente. |
| Spegni lo schermo del Mac | — | se il Mac chiede la password al risveglio, questo lo blocca |
| Metti in stop il Mac | — | dopo pochi secondi il Mac non risponde più: il riquadro diventa "non raggiungibile" e torna la luna (SPEC_COMPUTER) |
| Tieni sveglio il Mac | acceso sì/no | il Mac non va in stop e non spegne lo schermo finché non lo si spegne dalla scheda |

**Gruppo B: si possono aggiungere, ma serve qualcosa in più.** Dimmi quali valgono la pena.

| Comando | Cosa serve |
|---|---|
| Uscita audio: casse del Mac o cuffie | un programmino gratuito da installare sul Mac (SwitchAudioSource) |
| Non disturbare (Full immersion) | un "Comando rapido" da creare una volta sul Mac |
| Modalità scura del Mac | un consenso di macOS da dare una volta |
| Muto **dentro** Teams (il pulsante della chiamata) | permesso di "Accessibilità" sul Mac, una volta |

## 3. Cosa serve progettare

1. **Dove stanno i comandi.** Per esempio:
   - una seconda scheda nella pagina Computer ("Stato · Comandi"), come la pagina Meteo a schede;
   - i comandi nella pagina Computer, sotto o al posto di qualcosa;
   - una pagina a sé;
   - oppure altro.

   La pagina Computer oggi è piena (SPEC_COMPUTER §6).
2. **Accesso rapido.** Volume e microfono servono **senza cercare**, per esempio quando inizia una chiamata. Ha senso un accesso più veloce? Per esempio:
   - dalla Home;
   - dalla barra laterale;
   - un microfono muto che si vede sempre, così ci si accorge di averlo lasciato spento.

   Oppure no: decidi tu.
3. **Due volumi.** In Now playing c'è già un cursore del volume: è quello dell'app Spotify (o delle casse su cui suona). Il volume del Mac è un'altra cosa e moltiplica il primo. Come si evita la confusione? Per esempio con nomi diversi ("Volume Spotify" / "Volume del Mac") o icone diverse.
4. **Comandi "pericolosi".** Stop e spegni schermo non si possono annullare dalla scheda (il Mac si sveglia solo toccandolo). Serve una conferma o un tocco lungo? Ricorda che il tocco lungo in LVGL, al rilascio, preme anche il pulsante: una conferma a due tocchi è più sicura.
5. **Stati:**
   - normale;
   - comando in corso (fra il tocco e la risposta, di solito meno di mezzo secondo);
   - comando non riuscito;
   - Mac non raggiungibile;
   - non abbinato, con il codice di 6 cifre da scrivere sul Mac;
   - abbinamento riuscito;
   - versione vecchia del programmino sul Mac, che manda lo stato ma non sa ricevere comandi.
6. **Abbinamento:** dove si fa (Impostazioni › Computer?) e come appare il codice.
7. **Gruppo B:** quali comandi aggiungeresti e dove li metteresti, anche solo come posto lasciato libero.

## 4. Vincoli tecnici (come sempre)

- **Sobrio come il resto:**
  - niente animazioni, sfumature larghe o trasparenze;
  - fondi scuri: sotto le zone chiare, con il Wi-Fi attivo, compaiono trattini lampeggianti (SPEC 6 §9);
  - lo schermo è RGB565.
- **Cursori:** come quello del volume di Now playing (LVGL `lv_slider`). Mentre lo si trascina, la scheda manda al Mac al massimo 6-7 comandi al secondo, quindi il suono segue il dito con un piccolo ritardo.
- **Pulsanti:** almeno 56 px di lato per il dito.
- **Icone:** Font Awesome 5 Free Solid nelle misure 20 / 28 / 40 / 56. Candidate:
  - volume-up `f028` (c'è già), volume-down `f027`, volume-mute `f6a9`;
  - microphone `f130`, microphone-slash `f131`;
  - moon `f186`, power-off `f011`, lock `f023`, desktop `f108` (c'è già), coffee `f0f4` (tieni sveglio);
  - link `f0c1` / unlink `f127` (abbinamento).

  Un'icona in più costa poco: dimmi codice e misura.
- **Caratteri:** i 6 Montserrat, `CLOCK112`, `font_temp_64` (solo cifre, "-", "°" e "%").
- **Luce dello schermo:** scende al 15% dopo 2 minuti senza tocchi. Dopo 2 minuti senza tocchi si torna alla Home, tranne Now playing con la musica.
- **Più computer** in futuro: come nella SPEC_COMPUTER, non progettarlo, ma non chiudere la strada.

## 5. Cosa ci serve da te

Stesso formato delle SPEC precedenti:
1. **SPEC** con posizioni, misure, caratteri e colori di ogni elemento, stati compresi.
2. **Mockup 800×480**: almeno i comandi in stato normale, il microfono muto, l'accesso rapido se c'è, una conferma, il Mac non raggiungibile e l'abbinamento con il codice.
3. **Navigazione** (tabella "da / tocco / va a").
4. **Icone e caratteri nuovi** da generare.
5. **Modifiche alle SPEC precedenti**, scritte esplicitamente.
6. Una sezione **"Da verificare sul dispositivo"**.

Salva tutto in `design/controllo_mac/` (SPEC in `design/controllo_mac/SPEC_CONTROLLO_MAC.md`, mockup in `design/controllo_mac/mockup/`).
