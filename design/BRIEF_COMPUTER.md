# Brief per Claude Design: Controller Spotify, stato del computer

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. La Home a riquadri della tua `home_giorni/SPEC_HOME_GIORNI.md` è sulla scheda e funziona. Valerio vuole aggiungere una funzione nuova: **lo stato del computer** accanto a cui sta la scheda (oggi un MacBook Pro).

Le scelte di design le fai tu; a me servono posizioni, misure e colori in numeri, da tradurre in LVGL 8, come sempre.

**Allegati:**
- catture reali 800×480 di oggi:
  - Home: `home_giorni/schermate/02_home_senza_musica.png`;
  - Impostazioni: `home_giorni/schermate/08_impostazioni.png`;
  - pagina a schede: `home_giorni/schermate/13_aria_domani.png`;
- le SPEC: `home_giorni/SPEC_HOME_GIORNI.md` (§1 domanda 7 "funzioni future", §3.5 riquadri piccoli, §6 Impostazioni), `fase6/SPEC_FASE6.md` (§3 colori e caratteri);
- caratteri e icone: `fase6/CARATTERI_E_ICONE.md`.

---

## 1. Come arrivano i dati

Un piccolo programma sul computer manda i dati alla scheda **via Wi-Fi ogni 5 secondi**: niente cavi in più, niente password da amministratore. Se il computer è spento, dorme o è su un'altra rete, i dati smettono di arrivare: dopo 30 secondi la scheda lo considera **non raggiungibile**.

## 2. I dati (veri, dal Mac di Valerio, 5 novembre)

| Dato | Esempio | Note |
|---|---|---|
| Nome del computer | "Valerio's MacBook Pro" | quello scelto nelle impostazioni del Mac |
| Batteria | 80% | |
| Stato della batteria | collegato all'alimentatore e non in carica | 4 casi: in carica · carica completa / non in carica (ricarica ottimizzata del Mac) · a batteria · senza batteria (computer fisso) |
| Tempo rimanente | "3 h 12 min" (a batteria), "fino al 100% in 40 min" (in carica) | non sempre disponibile: il Mac lo calcola dopo qualche minuto |
| Processore | 29% in uso | media degli ultimi secondi, 0-100% |
| Memoria | 48% libera | il Mac la chiama "pressione della memoria": sopra il 30% libera va tutto bene |
| Disco | 36,9 GB liberi su 494 | |
| Acceso da | 2 giorni e 3 ore | |
| Avviso di temperatura | di solito nessuno | quando il Mac rallenta per il caldo lo segnala |
| Batteria delle cuffie Bluetooth | AirPods Pro 70% | solo con cuffie collegate che la comunicano (le AirPods sì, molte altre no) |

Se serve altro, chiedilo: per esempio la rete Wi-Fi del computer o lo spazio di una seconda unità. La temperatura in gradi invece richiede la password da amministratore.

## 3. Cosa serve progettare

1. **Il riquadro della Home.** I tre riquadri piccoli hanno già le loro priorità: Sole > Aria > Pollini (solo medi o alti) > Luna. Dove entra il computer? Per esempio:
   - un riquadro fisso al posto della luna;
   - un riquadro che compare solo quando c'è qualcosa da dire (batteria sotto il 20%, disco quasi pieno, processore al massimo da un po');
   - oppure altro.
2. **Cosa mostra il riquadro:** una sola cosa (la batteria?) o un riassunto.
3. **Il tocco:** apre una pagina "Computer" con tutti i dati? Con il mockup.
4. **Soglie e colori:** quando una batteria, una memoria o un disco diventano "da guardare" (per esempio batteria sotto il 20%, disco sotto il 10%).
5. **Stati:**
   - programma mai avviato sul computer (da spiegare: "Avvia il programma sul computer");
   - computer non raggiungibile (spento, in stop, altra rete), con l'ora dell'ultimo dato;
   - computer fisso senza batteria;
   - nessuna cuffia collegata.
6. **Impostazioni (facoltativo):** una riga "Computer" con il nome e lo stato del collegamento.
7. **Più computer** in futuro (per esempio un PC Windows): non progettarlo adesso, ma non chiudere la strada.

## 4. Vincoli tecnici (come sempre)

- **Sobrio come il resto:**
  - niente animazioni, sfumature larghe o trasparenze;
  - fondi scuri: sotto le zone chiare, con il Wi-Fi attivo, compaiono trattini lampeggianti (SPEC 6 §9);
  - lo schermo è RGB565.
- **Grafici:** barre o rettangoli pieni vanno bene, niente linee sottili oblique.
- **Aggiornamento:** i dati arrivano ogni 5 secondi, ma ridisegno solo se cambia quello che si vede. Un numero che cambia ogni 5 secondi, come il processore, va bene in una pagina aperta, meno sulla Home.
- **Icone:** Font Awesome 5 Free Solid nelle misure 20 / 28 / 40 / 56. Candidate presenti nel file: laptop `f109`, desktop `f108` (c'è già), battery-full / three-quarters / half / quarter / empty `f240`-`f244`, plug `f1e6`, bolt `f0e7` (c'è già), microchip `f2db`, memory `f538`, hdd `f0a0`, headphones `f025` (c'è già), thermometer-half `f2c9`. Un'icona in più costa poco: dimmi codice e misura.
- **Caratteri:** i 6 Montserrat, `CLOCK112`, `font_temp_64` (solo cifre, "-" e "°": il "%" non c'è, se lo vuoi grande va aggiunto).
- **Luce dello schermo:** scende al 15% dopo 2 minuti senza tocchi.

## 5. Cosa ci serve da te

Stesso formato delle SPEC precedenti:
1. **SPEC** con posizioni, misure, caratteri e colori di ogni elemento, stati compresi.
2. **Mockup 800×480**: almeno la Home con il riquadro del computer (normale e da guardare), la pagina del computer se c'è, il computer non raggiungibile.
3. **Navigazione** (tabella "da / tocco / va a").
4. **Icone e caratteri nuovi** da generare.
5. **Modifiche alle SPEC precedenti**, scritte esplicitamente.
6. Una sezione **"Da verificare sul dispositivo"**.
