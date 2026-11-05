# Specifica · Meteo sull'Orologio

Risposta a `design/BRIEF_METEO.md`. Si aggiunge a `fase6/SPEC_FASE6.md`, `fase7/SPEC_FASE7.md` e `colore_copertina/SPEC_COLORE.md`: quello che qui non è nominato resta com'è. I nomi dei colori (`TEXT_2`, `SURFACE`…) sono quelli della SPEC 6 §3.1.

**In breve**
- L'Orologio diventa un **cruscotto in due colonne**: l'ora e la data a sinistra, il meteo di adesso a destra. Sotto c'è una **fascia** con 5 orari (ogni 2 ore, fino a +10 h) e **Domani**.
- Escono la riga "Ciao, … Niente in riproduzione." e il suggerimento: il chip in alto dice già se suona qualcosa. I pulsanti scendono in basso, nella stessa riga di "Cambia Wi-Fi".
- Un tocco sul meteo apre la pagina **Meteo**: 12 ore, una per una, più le schede Oggi e Domani.
- Le icone sono bianche. Fanno eccezione il **sole**, giallo, e la **probabilità di pioggia**, azzurra.
- Servono un carattere nuovo per la temperatura grande (`font_temp_64`), uno per l'icona grande (`font_wx_56`) e 13 codici in più nei caratteri delle icone (§6).

Mockup 800×480 in `meteo/mockup/`, disegnati nel canvas alla pagina "Meteo":

| File | Cosa mostra |
|---|---|
| `01_giorno-sereno.png` | giorno sereno, sole giallo; la fascia passa dal giorno alla notte |
| `02_notte.png` | sera, poco nuvoloso di notte; pioggia in arrivo nella fascia e domani |
| `03_pioggia.png` | pioggia adesso, probabilità di oggi sotto il min/max |
| `04_in-arrivo.png` | all'accensione, meteo non ancora arrivato (scheletro) |
| `05_errore.png` | nessun dato per errore di rete |
| `06_dato-vecchio.png` | ultimo aggiornamento più di 2 ore fa |
| `07_neve-sotto-zero.png` | temperature negative, neve, nome di città lungo |
| `08_premuto.png` | blocco di adesso e fascia premuti |
| `09_pagina-meteo.png` | pagina aperta dal tocco: 12 ore, Oggi e Domani |

---

## 1. Risposte alle domande del brief

**1. Dove sta e quanto spazio prende.**
Prende la metà destra della parte alta e una fascia larga quanto lo schermo. L'ora resta il primo elemento che si vede (stessa misura, 112 px) ma si sposta a sinistra. La data va sotto l'ora, a sinistra.

Escono:
- "Ciao, … Niente in riproduzione.": il chip in alto dice già "Nessun dispositivo attivo" o il nome del dispositivo fermo;
- il suggerimento: i pulsanti Home e Cerca bastano.

Wi-Fi e "Cambia Wi-Fi" restano, nella riga dei pulsanti.

**2. Quanto meteo.**
- **Adesso**: icona, temperatura, condizione, città con temperatura percepita, minima e massima di oggi, pioggia se ≥ 20%.
- **Fascia**: 5 orari a +2, +4, +6, +8, +10 ore, più **Domani** sempre in fondo. Il passo di 2 ore copre la giornata in 5 celle. Domani è sempre visibile, così non serve una regola "dalle 18 in poi": la sera si guarda l'ultima cella, la mattina si guardano le ore.

**3. Tocco.**
Il blocco di adesso e la fascia aprono la pagina **Meteo** (`09`), con le prossime 12 ore una per una e le schede Oggi e Domani. La pagina non fa richieste: mostra i dati già in memoria.

**4. Icone e testi:** tabella al §5.

**5. Stati:** §3.4.

**Spazio per altre informazioni in futuro.** Il blocco di destra (336×156) e la fascia (720×104) sono le due "aree informazioni" dell'Orologio. Calendario o "ore ascoltate oggi" potranno occupare una di queste due aree, al posto del meteo o alternandosi con lui. Oggi non è progettato.

---

## 2. Navigazione

| Da | Tocco | Va a |
|---|---|---|
| Orologio | blocco di adesso o fascia | Meteo (`09`) |
| Meteo | freccia indietro | Orologio |
| Meteo | voci della barra laterale | come in tutte le schermate della fase 6/7 |

- La pagina Meteo ha la barra laterale con **nessuna voce attiva**, perché non appartiene a nessuna sezione.
- Se parte la musica mentre la pagina è aperta, si resta lì, come per ogni altra schermata con la barra.
- Negli stati "in arrivo" ed "errore" il meteo non si tocca.

---

## 3. Orologio

Barra in alto invariata (SPEC 7 §3.5): chip `16,8`, icone `536 / 600 / 664 / 728, 0`.

### 3.1 Ora, data, pulsanti

| Elemento | x,y | L×A | Stile |
|---|---|---|---|
| Ora | 40,72 | auto × 120 | `CLOCK112 TEXT`, **allineata a sinistra** |
| Data | 44,196 | L 380 | `M20 TEXT_2` |
| Home | 40,392 | 176×48 | `bp sm`, icona casa 20 (come oggi) |
| Cerca | 232,392 | 176×48 | `bs sm`, icona lente 20 (come oggi) |
| Wi-Fi | gruppo allineato a destra che finisce a x 568, y 405; L max 144 | — | icona wifi 20 `TEXT_3` + 8 px + nome della rete `M16 TEXT_3`, troncato con "…"; **senza "Wi-Fi:"** |
| Cambia Wi-Fi | 584,392 | 176×48 | `bs sm`, bordo `#535353` (come oggi) |

### 3.2 Adesso (blocco di destra)

Area di tocco `440,72` 336×156, raggio 12. Premuto: fondo `PRESSED #2A2A2A`.

| Elemento | x,y | Stile |
|---|---|---|
| Icona condizione | x 456, centrata verticalmente sulla temperatura (y 88) | `font_wx_56`. Colore `TEXT`, ma `WX_SUN` per il sole (f185). La larghezza cambia per icona (§5): la temperatura la segue. |
| Temperatura | dopo l'icona + 16 px, y 80 | `font_temp_64` (Medium 64, interlinea 72) `TEXT`: "21°", "-4°" |
| Max | destra a 760, y 84, L 88 | `M16 TEXT_2`: "Max 22°" |
| Min | destra a 760, y 106, L 88 | `M16 TEXT_2`: "Min 14°" |
| Pioggia oggi | destra a 760, y 128 | icona goccia 20 + 4 px + "90%", tutto `WX_RAIN`; **solo se ≥ 20%** |
| Condizione | 456,160 | L 304 | `M20 TEXT`, 1 riga (§5) |
| Città e percepita | 456,190 | L 304 | `M16 TEXT_2`: due etichette in fila. Città (max 180, troncata con "…") + " · percepiti 20°". Così si tronca la città e mai la percepita. Città sconosciuta: solo "Percepiti 20°". |

Fonti dei dati:
- temperatura, percepita, condizione e giorno/notte da `current`;
- Max, Min e pioggia da `daily[0]`.

Caso peggiore: nuvola (larga 70) e "-12°". La riga finisce a x ~675, mentre la colonna Max/Min, allineata a destra, comincia intorno a x 690: non si toccano.

### 3.3 Fascia (prossime ore + Domani)

Area di tocco `40,248` 720×104, raggio 12, fondo `SURFACE #242424`. Premuto: `SURFACE_PRESSED #333333`.

Sei celle larghe 120, con centro a x = 100 + i·120 (i = 0…5). Fra la quinta e la sesta c'è un separatore 1×72 `RAISED #3A3A3A` a `640,264`.

| Riga della cella | y | Stile |
|---|---|---|
| Etichetta | 258 | `M16`, centrata. Ore: "13:00" in `TEXT_2` (formato `%02d:00`, come l'orologio). Ultima cella: "Domani" in `TEXT`. |
| Icona (+ pioggia) | 286, alta 28 | icona 28 `TEXT` (sole `WX_SUN`); se la pioggia è ≥ 20%, dopo 6 px "40%" in `M16 WX_RAIN`. **Il gruppo icona + percentuale è centrato nella cella.** |
| Temperatura | 320 | `M20 TEXT`, centrata. Ore: "22°". Domani: "15°/23°" (min/max, senza spazi). |

Dati:
- celle 0-4: le voci orarie a **+2, +4, +6, +8, +10 ore** dall'ora in corso (alle 11:05: 13, 15, 17, 19, 21), con il loro `is_day` per l'icona di giorno o di notte;
- cella 5: `daily[1]`, sempre con l'icona di giorno.

La fascia si ricalcola **al cambio d'ora** e all'arrivo di dati nuovi, non ogni minuto.

### 3.4 Stati

| Stato | Quando | Blocco di destra | Fascia | Tocco |
|---|---|---|---|---|
| **In arrivo** (`04`) | dall'accensione al primo dato (5-20 s) | scheletro fisso, senza animazione (sotto) + "Meteo in arrivo…" `M16 TEXT_3` a `456,190` | pannello `SURFACE` con lo scheletro delle 6 celle e il separatore | no |
| **Normale** (`01`-`03`, `07`) | dato con meno di 2 h | §3.2 | §3.3 | sì |
| **Dato vecchio** (`06`) | ultimo aggiornamento riuscito fra 2 e 6 h fa | come normale, ma al posto di città/percepita: icona avviso 20 `WARN` + 8 px + "Aggiornato alle 19:10" `M16 TEXT_2` | normale | sì |
| **Errore** (`05`) | nessun dato riuscito dall'accensione, o l'ultimo ha più di 6 h | nuvola `font_wx_56` `DISABLED_FG #5A5A5A` a `456,88`; "Meteo non disponibile" `M20 TEXT_2` a `456,160`; "Riprovo tra 5 minuti" `M16 TEXT_3` a `456,190` | **non disegnata**: la banda resta vuota e il resto non si sposta | no |

Scheletro dell'**In arrivo**:
- blocco di destra:
  - cerchio 56 `SURFACE` a `456,88`;
  - barra 112×48 `SURFACE` a `528,92`;
  - barre 72×16 a `688,88` (`SURFACE`) e a `688,110` (`SKELETON_2`);
  - barra 160×20 `SURFACE` a `456,164`;
- fascia, per ogni cella con centro cx:
  - barra 48×14 `RAISED` a `cx−24, 262`;
  - cerchio 28 `RAISED` a `cx−14, 288`;
  - barra 40×16 `SURFACE_PRESSED` a `cx−20, 324`.

Altre regole:
- **Dato vecchio, valori di adesso.** Conviene prendere temperatura, condizione e giorno/notte dalla voce oraria dell'**ora in corso** invece che da `current`: una previsione di poche ore fa vale più di un'osservazione di tre ore fa. Per questo servono 24 ore orarie (§7).
- **Errore:** "Riprovo tra 5 minuti" va bene se il firmware riprova ogni 5 minuti; se l'intervallo è un altro, scrivi "Riprovo tra poco".
- **Temperature negative** (`07`): segno "-" ASCII attaccato al numero, in tutte le misure ("-4°", "-9°/2°"). Arrotonda all'intero; "-0" diventa "0".
- **Giorno/notte:** per adesso e per le ore si usa `is_day`; per Domani e Oggi sempre l'icona di giorno.

---

## 4. Pagina Meteo — `09_pagina-meteo.png`

- Barra laterale (SPEC 6 §4.1), nessuna voce attiva.
- Freccia indietro `88,0`; titolo "Meteo" `SB24` a `156,17`.
- In alto a destra: "Napoli · aggiornato alle 16:15", `M16 TEXT_3`, allineato a destra a x 776, y 21, L max 320. Nello stato "dato vecchio" l'ora è preceduta dall'icona avviso 20 `WARN`.

**12 ore**, dall'ora in corso (+0 … +11), in due pannelli `SURFACE` raggio 12:
- pannelli `104,72` e `104,184`, 672×104 ciascuno;
- 6 celle per pannello, con centro a x = 160 + i·112;
- righe della cella come la fascia (§3.3): etichetta y +10, icona e pioggia y +38, temperatura y +72 dal bordo del pannello.

I pannelli non si toccano.

**Oggi / Domani**: due schede `SURFACE` raggio 12, 328×168, a `104,296` e `448,296`.

| Elemento | Posizione nella scheda | Stile |
|---|---|---|
| Titolo | 20,16 | `SB20 TEXT`: "Oggi" / "Domani" |
| Icona | 20,60 | icona 40 (`font_ic_40`), `TEXT`, sole `WX_SUN` |
| Condizione | 80,56, L 228 | `M20 TEXT` |
| Min e max | 80,84 | `M16 TEXT_2`: "Min 13° · Max 19°" |
| Pioggia | 20,128 | goccia 20 `WX_RAIN` + 8 px + "Pioggia " `M16 TEXT_2` + "60%" `M16 WX_RAIN`. **Sempre mostrata**, anche "0%". |

Non sono toccabili. Nella pagina non ci sono stati "in arrivo" o "errore", perché si apre solo con dati validi. Se i dati scadono mentre è aperta (oltre 6 h), resta com'è fino all'uscita.

---

## 5. Condizioni: codice WMO → icona → testo

Icone da Font Awesome 5 Free Solid. "L" è la larghezza dell'icona rispetto all'altezza: le icone non sono quadrate, e LVGL usa la larghezza vera, quindi i gruppi icona + testo vanno centrati sulla loro larghezza reale.

| Codici | Giorno | Notte | Testo |
|---|---|---|---|
| 0 | sun `F185` (L 1,0) **giallo** | moon `F186` (L 1,0) | Sereno |
| 1 | cloud-sun `F6C4` (L 1,25) | cloud-moon `F6C3` (L 1,125) | Quasi sereno |
| 2 | cloud-sun `F6C4` | cloud-moon `F6C3` | Poco nuvoloso |
| 3 | cloud `F0C2` (L 1,25) | cloud `F0C2` | Coperto |
| 45, 48 | smog `F75F` (L 1,25) | smog `F75F` | Nebbia |
| 51 | cloud-rain `F73D` (L 1,0) | cloud-rain | Pioviggine debole |
| 53 | cloud-rain | cloud-rain | Pioviggine |
| 55 | cloud-rain | cloud-rain | Pioviggine intensa |
| 56, 57 | cloud-rain | cloud-rain | Pioviggine gelata |
| 61 | cloud-rain | cloud-rain | Pioggia debole |
| 63 | cloud-showers-heavy `F740` (L 1,0) | cloud-showers-heavy | Pioggia |
| 65 | cloud-showers-heavy | cloud-showers-heavy | Pioggia forte |
| 66, 67 | cloud-showers-heavy | cloud-showers-heavy | Pioggia gelata |
| 71 | snowflake `F2DC` (L 0,875) | snowflake | Neve debole |
| 73 | snowflake | snowflake | Neve |
| 75 | snowflake | snowflake | Neve forte |
| 77 | snowflake | snowflake | Nevischio |
| 80 | cloud-sun-rain `F743` (L 1,125) | cloud-moon-rain `F73C` (L 1,125) | Rovesci deboli |
| 81 | cloud-sun-rain | cloud-moon-rain | Rovesci |
| 82 | cloud-showers-heavy | cloud-showers-heavy | Rovesci forti |
| 85 | snowflake | snowflake | Rovesci di neve |
| 86 | snowflake | snowflake | Forti rovesci di neve |
| 95 | bolt `F0E7` (L 0,625) | bolt | Temporale |
| 96, 99 | bolt | bolt | Temporale con grandine |
| altro | cloud `F0C2` | cloud | *(nessun testo)* |

Il testo più lungo, "Temporale con grandine", sta in 304 px a `M20`.

### Colori nuovi (solo meteo)

| Nome | Hex | Uso | Contrasto |
|---|---|---|---|
| `WX_SUN` | `#FFC24B` | solo l'icona del sole (`F185`), in tutte le misure | 11,7:1 su `BG`, 9,5:1 su `SURFACE` |
| `WX_RAIN` | `#6CB4F0` | percentuali di pioggia e icona goccia | 8,4:1 su `BG`, 6,8:1 su `SURFACE` |

Tutte le altre icone sono `TEXT #FFFFFF`. **Niente icone a due colori**: il sole dentro cloud-sun resta bianco come la nuvola.

**Luce al 15%:** le informazioni da leggere da lontano (ora, temperatura, condizione, temperature delle ore) sono tutte `TEXT` bianco; `TEXT_3` resta solo per il nome del Wi-Fi e i testi di servizio.

---

## 6. Caratteri e icone da generare

Si aggiungono a `fase6/CARATTERI_E_ICONE.md`.

| File | Sorgente | px | Intervallo | Uso |
|---|---|---|---|---|
| `font_temp_64` (**nuovo**) | Montserrat **Medium** | 64 | `0x2D,0x30-0x39,0xB0` ("-", cifre, "°") | temperatura di adesso |
| `font_wx_56` (**nuovo**) | `FontAwesome5-Solid+Brands+Regular.woff` | 56 | `0xF185,0xF186,0xF6C4,0xF6C3,0xF0C2,0xF75F,0xF73D,0xF740,0xF743,0xF73C,0xF2DC,0xF0E7` | icona di adesso, nuvola dell'errore |
| `font_ic_28` (+12) | come sopra | 28 | gli stessi 12 codici di `font_wx_56` | fascia e 12 ore |
| `font_ic_40` (+12) | come sopra | 40 | gli stessi 12 codici | schede Oggi/Domani |
| `font_ic_20` (+1) | come sopra | 20 | `0xF043` (tint, goccia) | pioggia |

Avviso `0xF071` e wifi `0xF1EB` sono già in `font_ic_20`. Le cifre e "°" a 16/20 ci sono già nei `font_it_*` (U+00B0 è in `0xA0-0x17F`).

```bash
npx lv_font_conv@1.5.2 --no-compress --bpp 4 --size 64 --format lvgl --lv-include lvgl.h \
  --font Montserrat-Medium.ttf -r 0x2D,0x30-0x39,0xB0 -o font_temp_64.c
npx lv_font_conv@1.5.2 --no-compress --bpp 4 --size 56 --format lvgl --lv-include lvgl.h \
  --font FontAwesome5-Solid+Brands+Regular.woff \
  -r 0xF185,0xF186,0xF6C4,0xF6C3,0xF0C2,0xF75F,0xF73D,0xF740,0xF743,0xF73C,0xF2DC,0xF0E7 -o font_wx_56.c
```

Memoria stimata: `font_temp_64` ~17 KB, `font_wx_56` ~25 KB, le aggiunte a 28 e 40 ~18 KB. In tutto **circa 60 KB** di flash.

---

## 7. Dati da chiedere a Open-Meteo

- **`forecast_hours=24`** invece di 12. La fascia arriva a +10 ore e la pagina a +11 ore *dall'ora in corso*, e i dati possono avere fino a 6 ore (stato "dato vecchio"). Con 24 ore la fascia resta piena fino a +16 h e la pagina fino a +17 h.
- Non serve altro: alba, tramonto, vento e umidità non sono usati.

---

## 8. Modifiche alle SPEC precedenti

**SPEC 6 §5.2 e SPEC 7 §3.5 (Orologio):** sostituite dal §3 di questa specifica.
- Ora: da centrata a `40,72`, allineata a sinistra.
- Data: da centrata a `44,196`.
- **Tolti** lo stato ("Ciao, … Niente in riproduzione.") e il suggerimento.
- Home e Cerca: da `216,348` / `408,348` a `40,392` / `232,392`.
- Wi-Fi: da `24,437` con "Wi-Fi: nome" a gruppo icona + nome allineato a destra che finisce a x 568, y 405.
- Cambia Wi-Fi: da `600,420` a `584,392`.
- Barra in alto invariata.

**SPEC 6 §2 (mappa):** nuova pagina **Meteo**, raggiunta dall'Orologio; indietro torna all'Orologio.

**SPEC 6 §3.1 (colori):** aggiunti `WX_SUN #FFC24B` e `WX_RAIN #6CB4F0`, usati solo dal meteo. La regola "niente sfumature, trasparenze o ombre" resta.

**SPEC 6 §3.2 (caratteri) e `CARATTERI_E_ICONE.md`:** aggiunti `font_temp_64`, `font_wx_56` e i codici del §6.

**Non cambiano:** Now playing (con il colore della copertina), Home, liste, barra laterale, Orologio senza meteo per chi non ha rete (stato errore).

---

## 9. Da verificare sul dispositivo (Claude Code)

1. **Larghezza delle icone in LVGL.** cloud, cloud-sun e smog sono larghe 1,25 volte l'altezza. Controlla che il gruppo icona + temperatura in alto e i gruppi icona + "%" nelle celle restino centrati e non si sovrappongano. Caso peggiore: cloud 56 + "-12°".
2. **Leggibilità al 15% di luce** da 1-2 metri: temperatura grande, ore e "%" azzurre. Se le ore in `TEXT_2` si leggono male, passale a `TEXT`.
3. **Trattini con il Wi-Fi attivo** sotto il sole giallo e il pannello `SURFACE` della fascia: sono aree piccole o scure, dovrebbero andare bene.
4. **Ridisegni:** la fascia si ridisegna solo al cambio d'ora e all'arrivo di dati nuovi; l'ora resta l'unica cosa aggiornata ogni minuto.
5. **Nomi di città lunghi** dalla geolocalizzazione (es. "San Giovanni a Teduccio"): la città si tronca, "percepiti" resta intero.
6. **Stati:** forza "in arrivo", "dato vecchio" ed "errore", per esempio con un comando seriale che cambia l'età del dato, e confrontali con `04`, `05` e `06`.
7. **Memoria** dei caratteri nuovi (circa 60 KB), dopo la compilazione.
