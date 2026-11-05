# Brief per Claude Design: Controller Spotify, città del meteo

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Valerio vuole poter **scegliere la città del meteo** dallo schermo, tenendo un elenco delle sue città.

Le scelte di design le fai tu; a me servono posizioni, misure e colori in numeri, da tradurre in LVGL 8, come sempre.

**Allegati:**
- catture reali 800×480:
  - Home: `home_giorni/schermate/02_home_senza_musica.png`;
  - pagina Meteo, con la città sotto "Oggi": `home_giorni/schermate/09_meteo_oggi.png`;
  - Impostazioni: `computer/schermate/09_impostazioni.png` (il nome della rete Wi-Fi è coperto apposta);
  - Cerca, con la tastiera a schermo e le ricerche recenti: `fase6/schermate/12_ricerca.png`, `fase6/schermate/12_accenti.png`;
- le SPEC: `home_giorni/SPEC_HOME_GIORNI.md` (§6 Impostazioni, pagina Meteo), `computer/SPEC_COMPUTER.md` (§4, riga Computer in Impostazioni), `fase6/SPEC_FASE6.md` (§3 colori e caratteri, §5.10 Cerca e tastiera);
- caratteri e icone: `fase6/CARATTERI_E_ICONE.md`.

---

## 1. Cosa vuole Valerio (parole sue, riassunte)

- **Cercare una città** e usarla per il meteo.
- Le città cercate **restano in un elenco** di opzioni; si possono **eliminare**.
- Una città si può segnare come **predefinita**.
- Resta anche la **posizione automatica** di oggi.

La città vale per tutto quello che dipende dal luogo: meteo, sole (alba, tramonto, UV), aria e pollini. La fase della luna è uguale ovunque.

## 2. Come funziona oggi

- **Automatica:** la scheda ricava la posizione dalla connessione a internet (ip-api.com). È approssimata (la città del fornitore di rete), ma per Valerio giusta: Roma.
- **Fissa:** solo con un comando dal computer col cavo. Nessuna schermata.
- La città si vede solo nella pagina Meteo, sotto il giorno ("Oggi · Roma"). La Home non la mostra.
- Geolocalizzazione vera non c'è: la scheda non ha GPS e i servizi che usano le reti Wi-Fi vicine sono a pagamento o chiusi.

## 3. I dati della ricerca (veri, Open-Meteo, 5 novembre)

La ricerca usa il geocoding di Open-Meteo: gratuito, senza chiave, risposta in meno di un secondo. Dà fino a 10 risultati con nome, regione, provincia, nazione e abitanti, già in italiano.

| Ricerca | Risultati (nome · regione · provincia · nazione) |
|---|---|
| "Milano" | Milano · Lombardia · Provincia di Milano · Italia (1,37 milioni); Milano · Texas · Stati Uniti (421); Milano · Umbria · Provincia di Perugia · Italia (42); … |
| "San Giovanni" | 10 frazioni italiane con lo stesso nome: Liguria (Savona), Sicilia (Catania), Friuli (Pordenone), Emilia-Romagna (Ferrara)… |
| "Pa" | Pa · Burkina Faso; Pa · Camerun… (con 2 lettere cerca il nome esatto, da 3 in su anche simili) |

Quindi:
- nel risultato serve **regione o provincia**, altrimenti i "San Giovanni" non si distinguono;
- la ricerca parte **alla conferma**, non a ogni lettera: ogni richiesta costa un po' di rete;
- alcune città hanno nomi lunghi ("Reggio nell'Emilia", "Castelnuovo di Garfagnana").

## 4. Cosa serve progettare

1. **Dove si gestiscono le città:**
   - una riga nuova in Impostazioni ("Meteo" o "Città del meteo") che apre una pagina con l'elenco?
   - si può cambiare città anche dalla pagina Meteo, per esempio toccando il nome sotto il giorno?
2. **L'elenco delle città:**
   - la posizione automatica è una voce dell'elenco ("Posizione attuale · Roma", come fa l'iPhone)?
   - come si vede la **predefinita** e come si cambia;
   - come si **elimina** una città. Attenzione: la pressione lunga qui è sconsigliata, al rilascio LVGL la conta anche come tocco;
   - quante città al massimo (propongo 8);
   - facoltativo: la temperatura di adesso accanto a ogni città. Open-Meteo dà tutte le città in una sola richiesta piccola (circa 1 KB).
3. **La ricerca:**
   - la stessa tastiera di Cerca (SPEC 6 §5.10), con gli accenti;
   - l'elenco dei risultati: nome, regione o provincia, nazione;
   - cosa succede al tocco: la città entra nell'elenco e diventa quella in uso? Diventa anche predefinita?
4. **Città in uso e predefinita.** Valerio vuole una predefinita, quindi le città possono essere due cose diverse:
   - la **predefinita** è quella della Home;
   - un'altra città scelta "al volo" dalla pagina Meteo vale per sempre o solo finché si resta nella pagina (poi si torna alla predefinita, come il ritorno alla Home dopo 2 minuti)?
   Decidi tu la regola più chiara.
5. **Home:** se la città predefinita non è quella dove si trova la scheda, serve vederne il nome nel riquadro Meteo?
6. **Stati:**
   - ricerca in corso;
   - nessun risultato ("Nessuna città trovata");
   - errore di rete;
   - elenco pieno;
   - posizione automatica non trovata;
   - cambio di città in corso: il meteo della città nuova arriva in 1-2 secondi (previsione + aria).
7. **Navigazione:** tabella "da / tocco / va a", compreso il ritorno dalla ricerca.

## 5. Vincoli tecnici (come sempre)

- **Sobrio come il resto:**
  - niente animazioni, sfumature larghe o trasparenze;
  - fondi scuri: sotto le zone chiare, con il Wi-Fi attivo, compaiono trattini lampeggianti (SPEC 6 §9);
  - lo schermo è RGB565.
- **Memoria:** in memoria c'è il meteo di una sola città alla volta. Cambiare città vuol dire riscaricare meteo e aria (1-2 s), più i giorni scelti nella pagina Meteo. Le città salvate (nome, regione, coordinate) stanno nella memoria permanente della scheda e restano dopo un riavvio, come le ricerche recenti di Cerca.
- **Icone:** Font Awesome 5 Free Solid nelle misure 20 / 28 / 40 / 56. Candidate: map-marker-alt `f3c5`, location-arrow `f124`, star `f005`, trash `f1f8`, times `f00d` (c'è già), check `f00c` (c'è già), plus `f067` (c'è già), search `f002` (c'è già). Un'icona in più costa poco: dimmi codice e misura.
- **Caratteri:** i 6 Montserrat, `CLOCK112`, `font_temp_64`. Montserrat copre gli accenti italiani ed europei comuni; i nomi con caratteri non latini (per esempio città cinesi) arrivano già traslitterati da Open-Meteo, ma qualche carattere raro potrebbe mancare.
- **Luce dello schermo:** scende al 15% dopo 2 minuti senza tocchi, e si torna alla Home.

## 6. Cosa ci serve da te

Stesso formato delle SPEC precedenti:
1. **SPEC** con posizioni, misure, caratteri e colori di ogni elemento, stati compresi.
2. **Mockup 800×480**: almeno l'elenco delle città, la ricerca con i risultati, la pagina Meteo con il cambio di città (se c'è), Impostazioni con la riga nuova, e gli stati principali.
3. **Navigazione** (tabella "da / tocco / va a").
4. **Icone e caratteri nuovi** da generare.
5. **Modifiche alle SPEC precedenti**, scritte esplicitamente.
6. Una sezione **"Da verificare sul dispositivo"**.
