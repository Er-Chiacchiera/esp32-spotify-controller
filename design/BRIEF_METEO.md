# Brief per Claude Design: Controller Spotify, meteo sull'Orologio

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Valerio ha scelto la prossima funzione: **il meteo sull'Orologio**. L'Orologio è la schermata di riposo, quella che si vede quando non suona niente. Le scelte di design le fai tu; a me servono posizioni, misure e colori in numeri, da tradurre in LVGL 8, come per le fasi 6 e 7.

È la prima funzione non legata a Spotify. In seguito potrebbero arrivarne altre "da guardare", per esempio il prossimo impegno del calendario o le ore ascoltate oggi. Se ti sembra utile, imposta il meteo in modo che altre informazioni possano affiancarlo, ma non progettarle adesso.

**Allegati:**
- cattura reale 800×480 dell'Orologio di oggi: `fase7/schermate/04_orologio.png`;
- il tuo mockup: `fase7/mockup/04_orologio.png`;
- il sistema di design: `fase6/SPEC_FASE6.md` (§3 colori e caratteri, §5.2 Orologio) e `fase7/SPEC_FASE7.md` (§3.5 Orologio con Home);
- caratteri e icone già presenti: `fase6/CARATTERI_E_ICONE.md`.

---

## 1. I dati che ho (già scaricati dalla scheda)

Open-Meteo, aggiornato **ogni 30 minuti**. Posizione trovata in automatico dalla connessione: **Roma** (confermata da Valerio), e il nome della città è in italiano. Esempio vero della scheda, 4 novembre pomeriggio:

| Dato | Esempio |
|---|---|
| Adesso: temperatura, percepita | 17,7 °C, percepiti 16,5 °C |
| Adesso: condizione (codice WMO, tabella in §4) e giorno/notte | 2 = parzialmente nuvoloso, giorno |
| Oggi e domani: minima, massima, condizione, probabilità massima di pioggia | oggi 11/20 °C, coperto, 8% · domani 9/20 °C, coperto, 15% |
| Prossime 12 ore, dall'ora in corso: ora, temperatura, condizione, giorno/notte, probabilità di pioggia | 17:00 18° · 18:00 16° (notte) · 19:00 15° · 20:00 14° (coperto) … |

Se serve altro (alba e tramonto, vento, umidità, più giorni), chiedilo pure: la risposta resta piccola.

## 2. Cosa serve progettare

1. **Dove sta il meteo sull'Orologio** e quanto spazio prende: rivedi pure la disposizione di §5.2 / fase 7 §3.5. Oggi ci sono:
   - l'ora (y 96);
   - la data (y 228);
   - la riga di stato "Ciao, … Niente in riproduzione." (y 276);
   - il suggerimento (y 308);
   - i pulsanti Home/Cerca (y 348);
   - Wi-Fi e "Cambia Wi-Fi" in basso.

   Dimmi cosa resta, cosa si sposta e cosa si può togliere.
2. **Quanto meteo mostrare.** Per esempio solo "adesso" (icona + temperatura + condizione + min/max), oppure anche una fila di prossime ore. Dalle 18 in poi potrebbe servire la previsione di domani invece di quella di oggi: decidi tu se e quando.
3. **Tocco:** il meteo si tocca? Se sì, cosa apre (per esempio una pagina con le 12 ore e domani), con il suo mockup.
4. **Icone e parole per ogni condizione:** tabella codice WMO → icona (giorno e notte) → testo italiano breve ("Sereno", "Pioggia debole"…). Le icone vanno scelte da **Font Awesome 5 Free Solid**, lo stesso file delle altre (§3).
5. **Stati:**
   - meteo non ancora arrivato (all'accensione, 5-20 s);
   - nessun dato per errore di rete;
   - dato vecchio (per esempio più di 2 ore senza aggiornamento);
   - temperature negative.
6. **Mockup 800×480:** almeno giorno sereno, notte, pioggia, e lo stato "non ancora arrivato". Dove serve, anche la pagina del tocco.

## 3. Vincoli tecnici

- **Sobrio come il resto**:
  - niente animazioni, sfumature larghe o trasparenze;
  - fondi scuri (sotto le zone chiare, con il Wi-Fi attivo, compaiono trattini lampeggianti: SPEC 6 §9);
  - lo schermo è RGB565.
- **Icone:**
  - si generano da `FontAwesome5-Solid+Brands+Regular.woff` (Font Awesome 5 Free Solid) nelle misure 20 / 28 / 40, o in una misura nuova se serve;
  - candidate: sun `f185`, moon `f186`, cloud `f0c2`, cloud-sun `f6c4`, cloud-moon `f6c3`, cloud-rain `f73d`, cloud-showers-heavy `f740`, cloud-sun-rain `f743`, cloud-moon-rain `f73c`, bolt `f0e7`, snowflake `f2dc`, smog `f75f`, wind `f72e`, umbrella `f0e9`, temperature-high `f769`, tint `f043`;
  - **sono monocromatiche**: un colore per icona, niente sole giallo con nuvola grigia nella stessa icona. Se vuoi due colori, sono due icone sovrapposte: dimmelo esplicitamente.
- **Caratteri:**
  - ci sono i 6 Montserrat (Medium 16/20/24, SemiBold 20/24/30) e `CLOCK112`, che ha solo le cifre e ":";
  - per una temperatura grande serve un carattere nuovo, con solo cifre, "-" e "°": costa poco, basta indicarne misura e peso;
  - il segno meno è "-" (ASCII), perché il carattere tipografico "−" (U+2212) non c'è.
- **Aggiornamento:**
  - il meteo cambia ogni 30 minuti e ridisegno solo quando cambia;
  - niente orari che scorrono ogni secondo oltre all'ora esistente.
- **Luce dello schermo**: scende al 15% dopo 2 minuti senza tocchi e si spegne dopo 12. Il meteo deve leggersi bene anche al 15%.

## 4. Codici WMO (quelli che manda Open-Meteo)

| Codice | Significato |
|---|---|
| 0 | sereno |
| 1, 2, 3 | prevalentemente sereno, parzialmente nuvoloso, coperto |
| 45, 48 | nebbia, nebbia con brina |
| 51, 53, 55 | pioviggine debole, moderata, intensa |
| 56, 57 | pioviggine gelata debole, intensa |
| 61, 63, 65 | pioggia debole, moderata, forte |
| 66, 67 | pioggia gelata debole, forte |
| 71, 73, 75 | neve debole, moderata, forte |
| 77 | granelli di neve |
| 80, 81, 82 | rovesci deboli, moderati, violenti |
| 85, 86 | rovesci di neve deboli, forti |
| 95 | temporale |
| 96, 99 | temporale con grandine debole, forte |

Puoi raggruppare i codici in poche condizioni.

## 5. Cosa ci serve da te

Stesso formato delle fasi 6 e 7:
1. **SPEC** con posizioni, misure, caratteri e colori di ogni elemento, stati compresi.
2. **Mockup 800×480** (§2.6).
3. **Tabella condizioni** codice → icona giorno/notte → testo (§2.4).
4. **Icone e caratteri nuovi** da generare (codici e misure).
5. **Modifiche alle SPEC precedenti**, scritte esplicitamente.
6. Una sezione **"Da verificare sul dispositivo"**.
