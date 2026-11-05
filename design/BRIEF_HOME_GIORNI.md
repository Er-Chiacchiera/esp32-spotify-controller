# Brief per Claude Design: Controller Spotify, una Home generale e i giorni

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Meteo, sole, luna e aria sono finiti e funzionano sulla scheda secondo le tue SPEC. Valerio ora vuole due cose:

1. **Una Home generale**, una specie di CarPlay: il punto di partenza da cui si vede e si raggiunge tutto (meteo, Spotify e quello che arriverà dopo). Valerio non è sicuro della forma: ha chiesto il tuo parere e preferisce che tu la ridisegni, invece di aggiungere pezzi all'Orologio.
2. **Andare avanti e indietro con i giorni** in meteo, sole e luna, aria.

Le scelte di design le fai tu; a me servono posizioni, misure e colori in numeri, da tradurre in LVGL 8, come sempre.

**Allegati:**
- catture reali 800×480 di oggi:
  - Orologio: `sole_aria/schermate/01_orologio.png`;
  - le tre schede: `sole_aria/schermate/10_meteo_schede.png`, `05_sole_luna.png`, `07_aria_vera.png`;
  - Now playing: `colore_copertina/schermate/01_copertina_vera.png`;
  - Home di Spotify (fase 7): `fase7/schermate/01_home.png`;
- le SPEC: `fase6/SPEC_FASE6.md` (§1-4 struttura, barra laterale, colori), `fase7/SPEC_FASE7.md`, `meteo/SPEC_METEO.md`, `sole_aria/SPEC_SOLE_ARIA.md`, `colore_copertina/SPEC_COLORE.md`;
- caratteri e icone: `fase6/CARATTERI_E_ICONE.md`.

---

## 1. Com'è oggi

- **Orologio**: si vede quando non suona niente. Ora e data, meteo di adesso, fascia delle prossime ore, riga sole/aria/luna, pulsanti Home (di Spotify) e Cerca, Wi-Fi.
- **Now playing**: prende tutto lo schermo appena parte la musica, con il fondo del colore della copertina.
- **Navigazione Spotify** (Libreria, Cerca, Coda, Dispositivi, Home di Spotify): barra laterale a sinistra. Anche la pagina Meteo la usa, senza voce accesa.
- Il passaggio fra Orologio e Now playing è automatico: dipende solo dalla musica.

Problemi che vede Valerio:
- dal meteo, per mettere in pausa o saltare un brano bisogna tornare indietro;
- "Home" oggi significa la Home di Spotify, non una home della scheda;
- non c'è un posto pensato per le funzioni future.

## 2. La Home

**Il mio parere (da tecnico, decidi tu):** un menu di icone alla CarPlay aggiunge un tocco prima di arrivare a qualsiasi cosa. Su uno schermo che si guarda di sfuggita funziona meglio un **cruscotto a riquadri**, dove ogni riquadro mostra già l'informazione e si tocca per aprire la sua pagina. In pratica, l'evoluzione dell'Orologio:
- un riquadro **Spotify** sempre presente: brano, copertina piccola, play/pausa e avanti, oppure "Niente in riproduzione";
- i riquadri **meteo** e sole/aria;
- posto per riquadri futuri (calendario, ore ascoltate, timer, sveglia…).

E nelle pagine (Meteo, liste di Spotify…) una **barra di Spotify** piccola e fissa, con play/pausa e avanti, così i comandi restano sempre a portata.

**Cosa serve decidere:**
1. **Forma della Home**: cruscotto, menu di icone o altro. Con il mockup.
2. **Che fine fa l'Orologio.** Diventa la Home? L'ora resta grande o si rimpicciolisce?
3. **Now playing e Home**: oggi la musica apre Now playing da sola. Resta così, o la Home resta visibile e Now playing si apre toccando il riquadro Spotify? E con la musica in pausa?
4. **Barra di Spotify nelle pagine**: sì o no; dove sta (in basso, nella barra laterale…), cosa contiene, cosa succede senza musica.
5. **Nomi**: "Home" della scheda e "Home" di Spotify (fase 7) non possono chiamarsi uguale.
6. **Barra laterale**: oggi è tutta di Spotify. Con la Home generale serve una voce per tornare alla Home? Le voci di Spotify restano lì?
7. **Funzioni future**: come si aggiunge un riquadro o una pagina senza ridisegnare tutto.
8. **Stati**: Spotify non collegato, nessun dispositivo attivo, meteo in arrivo o in errore (già nelle SPEC del meteo).

## 3. I giorni

Valerio vuole scorrere avanti e indietro con le date in **Meteo**, **Sole e luna** e **Aria**.

**Fin dove si può andare** (provato oggi su Open-Meteo per Roma):

| Dati | Avanti | Indietro |
|---|---|---|
| Meteo: ore e giorni | 16 giorni (oltre 7 la previsione è poco affidabile) | 92 giorni |
| Alba, tramonto, UV | 16 giorni | 92 giorni |
| Aria e pollini | circa 4 giorni e mezzo (poi i valori mancano) | 92 giorni |
| Luna | qualsiasi data (la calcola la scheda) | qualsiasi |

**Come lo farei io:** la scheda tiene in memoria solo oggi e i prossimi giorni, come adesso; gli altri giorni li scarica quando li apri. Ogni giorno nuovo è una richiesta da 0,2-1 s, quindi serve uno stato "in arrivo" breve. Nel passato le "previsioni" sono i dati misurati.

**Cosa serve decidere:**
1. **Come si cambia giorno**: frecce, scorrimento col dito (il tocco simulato lo supporta), una fila di giorni da toccare ("Oggi · Ven 6 · Sab 7…"), un calendario…
2. **Dove si vede il giorno scelto** e come si torna subito a oggi.
3. **Fin dove**: tutto quello che è disponibile o un intervallo più corto (per esempio da 7 giorni fa a 7 giorni avanti).
4. **Cosa mostra ogni scheda per un giorno diverso da oggi.** Per esempio la scheda Meteo di domani: 24 ore invece delle "prossime 12"? La scheda Sole e luna di un altro giorno: senza il segno "adesso"? La scheda Aria nel passato: senza "adesso"?
5. **Il giorno scelto resta uguale cambiando scheda?** (Meteo di sabato → Sole e luna di sabato.)
6. **Stati**: giorno in arrivo, errore per quel giorno, giorno senza aria (oltre i 4 giorni e mezzo), passato.

## 4. Vincoli tecnici (come sempre)

- **Sobrio come il resto:**
  - niente animazioni, sfumature larghe o trasparenze;
  - fondi scuri: sotto le zone chiare, con il Wi-Fi attivo, compaiono trattini lampeggianti (SPEC 6 §9);
  - lo schermo è RGB565.
- **Memoria interna poca:** ogni schermata in più costa RAM. Meglio pagine che si ricostruiscono che tante pagine aperte insieme. Le immagini grandi (copertine) costano: una copertina piccola nel riquadro Spotify va bene, ma dimmi la misura (oggi esistono 64 e 150 px, più quella grande di Now playing).
- **Niente zoom, rotazioni o animazioni di scorrimento**: il disegno passa dalla memoria da cui legge lo schermo e con il Wi-Fi attivo lo fa tremolare. Uno scorrimento col dito può cambiare pagina, ma senza animazione.
- **Icone e caratteri**: Font Awesome 5 Free Solid nelle misure 20 / 28 / 40 / 56, i 6 Montserrat, `CLOCK112`, `font_temp_64`. Un carattere o un'icona nuova costa poco: dimmi codice e misura.
- **Luce dello schermo**: scende al 15% dopo 2 minuti senza tocchi e si spegne dopo 12.

## 5. Cosa ci serve da te

Stesso formato delle SPEC precedenti:
1. **SPEC** con posizioni, misure, caratteri e colori di ogni elemento, stati compresi.
2. **Mockup 800×480**: almeno la Home con e senza musica, una pagina con la barra di Spotify, e ogni scheda su un giorno diverso da oggi (futuro, passato, in arrivo).
3. **Navigazione** completa (tabella "da / tocco / va a").
4. **Icone e caratteri nuovi** da generare.
5. **Modifiche alle SPEC precedenti**, scritte esplicitamente.
6. Una sezione **"Da verificare sul dispositivo"**.

Se ti sembra meglio dividere il lavoro in due SPEC (prima la Home, poi i giorni), fallo pure e dimmelo.
