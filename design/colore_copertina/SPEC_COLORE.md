# Specifica · Now playing con il colore della copertina

Risposta a `design/BRIEF_COLORE_COPERTINA.md`. Vale sopra `fase6/SPEC_FASE6.md` e `fase7/SPEC_FASE7.md`: tutto quello che qui non è nominato resta com'è.

**In breve**
- Il colore si applica **solo a Now playing**.
- Il fondo è una **tinta unita scura**: niente sfumature. Prende la tinta più frequente fra i pixel "vivi" della copertina e la scurisce sempre alla stessa luminosità.
- Tutti i grigi di Now playing (chip, binari, testi secondari, stati premuti) diventano **il fondo mescolato col bianco** in proporzioni fisse. Con il fondo neutro `#121212` la formula ridà i grigi di oggi, quindi si scrive un solo codice per tutti i casi.
- Il pulsante play di Now playing diventa **bianco con icona nera**, come quello della barra laterale.
- Il colore cambia **di colpo**, nello stesso istante in cui compare la copertina nuova.

Mockup (800×480) in `colore_copertina/mockup/`, disegnati nel canvas alla pagina "Colore copertina":

| File | Cosa mostra |
|---|---|
| `00_ricetta-colore.png` | dalle copertine di prova al fondo e ai toni derivati, con i valori esadecimali |
| `01_copertina-scura.png` | foto notturna quasi nera, con un po' di blu → `#1A2E66` |
| `02_copertina-chiara.png` | copertina bianca con un cerchio arancio (10% di pixel vivi) → `#4E2713` |
| `03_copertina-colorata.png` | fucsia, giallo e azzurro: vince il fucsia → `#5A1637` |
| `04_copertina-bianco-nero.png` | ritratto in bianco e nero → grigio `#303030` |
| `05_copertina-verde.png` | copertina verde: il play bianco resta ben distinto → `#0E391E` |
| `06_senza-copertina.png` | brano senza immagine (file locale) → `#121212` |
| `07_copertina-in-arrivo.png` | brano cambiato, copertina non ancora arrivata: resta il colore di prima |
| `08_pausa-premuto-disabilitato.png` | in pausa; play, indietro e chip premuti; volume e Coda disabilitati |

Le copertine di prova sono SVG da 300×300 in `fase6/canvas/project/copertine/` (`scura`, `chiara`, `colorata`, `bianco-nero`, `verde`). Puoi usarle come casi di prova (§1.4).

---

## 1. Ricetta del colore

### 1.1 Passi

L'ingresso è la copertina 300×300 già decodificata in RGB565 (quella di Now playing).

1. **Campioni.** Leggi un pixel ogni 6 in orizzontale e in verticale, partendo da (3,3): sono 50×50 = 2500 campioni. Espandi ogni campione a 8 bit per canale *senza* replicare i bit bassi: `r = (p>>8)&0xF8`, `g = (p>>3)&0xFC`, `b = (p<<3)&0xF8`. I valori di prova del §1.4 sono calcolati così.
2. **Scarta i campioni non vivi.** Calcola `max`, `min` e `C = max − min`. Un campione è **vivo** se valgono tutte e tre:
   - `max ≥ 40` (scarta i quasi neri);
   - `C ≥ 24` (scarta grigi e quasi bianchi);
   - `C × 100 ≥ max × 30`, cioè saturazione HSV ≥ 30%.
3. **Istogramma delle tinte.** Per ogni campione vivo calcola la tinta H (0–359°, la stessa per HSL e HSV). Mettila in uno di 36 contenitori da 10°: `k = min(35, H/10)`. Per ogni contenitore tieni il conteggio e la somma di R, G e B.
4. **Copertina acromatica?** Se i campioni vivi sono **meno del 5%** (meno di 125 su 2500), la copertina è in bianco e nero, grigia, quasi nera o quasi bianca. Allora `S = 0` (e H non conta) e vai al passo 7.
5. **Tinta vincente.** Per ogni k calcola il punteggio `cnt[k−1] + 2·cnt[k] + cnt[k+1]`, con indici circolari (35 è vicino di 0). Il k con il punteggio più alto vince; a parità vince il primo trovato.
6. **Colore medio** dei campioni nei tre contenitori k−1, k, k+1, sommando R, G e B. Convertilo in HSL e ottieni **H** e la saturazione `S_src`, poi limitala: `S = clamp(S_src, 0.30, 0.60)`.
7. **Scurisci a luminosità fissa.** Per `L` da 0,30 a 0,06, scendendo di 0,01:
   - converti `HSL(H, S, L)` in RGB 8 bit (arrotondato);
   - calcola la luminanza relativa `Y = 0.2126·lin(R) + 0.7152·lin(G) + 0.0722·lin(B)`, dove `lin(c) = c/255/12.92` se `c/255 ≤ 0.04045`, altrimenti `((c/255 + 0.055)/1.055)^2.4` (tabella di 256 float);
   - appena **Y ≤ 0,032**, fermati: quello è **`NP_BG`**.

   Le tinte blu si fermano già a L 0,30, per il tetto; gialli e verdi scendono di più. Così tutte le tinte sembrano scure allo stesso modo.
8. Passa `NP_BG` a LVGL con `lv_color_make(R,G,B)`: la riduzione a RGB565 la fa lui.

Un'implementazione di riferimento in JavaScript, usata per calcolare i valori del §1.4, è in `colore_copertina/ricetta_riferimento.js`.

Costo: 2500 campioni con un po' di aritmetica intera, più al massimo 25 giri del passo 7. Sono 1-2 ms sul core 0, una volta per copertina.

### 1.2 Parametri (in un solo punto del codice, per le prove sul dispositivo)

| Nome | Valore | Effetto |
|---|---|---|
| `CC_STEP` | 6 | passo dei campioni |
| `CC_MIN_MAX` | 40 | sotto: quasi nero, scartato |
| `CC_MIN_C` | 24 | sotto: grigio, scartato |
| `CC_MIN_SAT` | 30 % | saturazione HSV minima per essere "vivo" |
| `CC_MIN_VIVID` | 5 % | sotto: copertina acromatica → grigio |
| `CC_S_MIN` / `CC_S_MAX` | 0,30 / 0,60 | saturazione del fondo |
| `CC_L_START` | 0,30 | tetto di luminosità HSL |
| `CC_Y` | 0,032 | luminanza del fondo; **non superare 0,040** (vedi §2.3) |

### 1.3 Casi limite

| Copertina | Cosa succede | Fondo |
|---|---|---|
| Foto quasi nera con un velo di colore (`01`) | i pixel scuri ma saturi contano (max ≥ 40), il nero puro no | tinta del velo, es. blu notte `#1A2E66` |
| Quasi bianca con un piccolo elemento colorato ≥ 5% (`02`) | il bianco viene scartato e vince l'elemento | arancio → ruggine `#4E2713` |
| Quasi bianca con meno del 5% di colore, o solo testo nero | acromatica | grigio `#303030` |
| Bianco e nero, seppia molto spento, tutta grigia (`04`) | acromatica | grigio `#303030` |
| Tutta nera | acromatica | grigio `#303030` |
| Due colori forti (`03`) | vince quello con più pixel; i vicini di tinta si sommano (fucsia + rosa insieme) | fucsia scuro `#5A1637` |
| Gialla o arancio chiaro | il giallo scurito diventa oliva o bruno: è normale, succede anche nell'app Spotify | es. `#383008` |
| Verde (`05`) | verde scuro; il play bianco resta distinto (§2) | `#0E391E` |
| Nessuna immagine, o download fallito | non si calcola niente | `BG #121212` |

### 1.4 Valori di prova (per controllare il codice)

Calcolati con la ricetta qui sopra sulle copertine di prova, rasterizzate a 300×300:

| Copertina | Pixel vivi | H | S | `NP_BG` |
|---|---|---|---|---|
| `copertine/scura.svg` | 50 % | 224 | 0,60 | `#1A2E66` |
| `copertine/chiara.svg` | 10 % | 20 | 0,60 | `#4E2713` |
| `copertine/colorata.svg` | 100 % | 331 | 0,60 | `#5A1637` |
| `copertine/bianco-nero.svg` | 0 % | — | 0 | `#303030` |
| `copertine/verde.svg` | 100 % | 142 | 0,60 | `#0E391E` |
| *Relapse: Refill* (dalla cattura `fase6/schermate/01_now_playing.png`) | 52 % | 5 | 0,60 | `#5A1C16` |

Una differenza di ±1-2 su un canale è normale (arrotondamenti e decodifica).

---

## 2. Colori di Now playing

Tutti i toni sono **pieni** e si calcolano una volta per colore: niente trasparenze.

`mix(c, k)` mescola il colore `c` col bianco: per ogni canale `v + (255 − v) · k / 256`, arrotondato.

### 2.1 Toni derivati

| Token | Formula | Con `#121212` (oggi) | Con `#1A2E66` (esempio) | Sostituisce in Now playing |
|---|---|---|---|---|
| `NP_BG` | ricetta §1 | `#121212` | `#1A2E66` | `BG` |
| `NP_SURFACE` | `mix(NP_BG, 20)` | `#252525` | `#2C3E72` | `SURFACE` (chip) |
| `NP_PRESSED` | `mix(NP_BG, 26)` | `#2A2A2A` | `#314376` | `PRESSED` (cerchio premuto, segnaposto, volume disabilitato) |
| `NP_RAISED` | `mix(NP_BG, 44)` | `#3B3B3B` | `#415280` | `RAISED` e `DISABLED_FG_2` (binari, chip premuto, Coda disabilitata) |
| `NP_PH_FG` | `mix(NP_BG, 69)` | `#525252` | `#58668F` | `PLACEHOLDER_FG` (nota nel segnaposto) |
| `NP_TEXT_3` | `mix(NP_BG, 140)` | `#949494` | `#97A0BA` | `TEXT_3` (album) |
| `NP_TEXT_2` | `mix(NP_BG, 174)` | `#B3B3B3` | `#B6BCCE` | `TEXT_2` (artisti, tempi, ora, icone in alto, volume, chevron) |
| `TEXT` | fisso | `#FFFFFF` | `#FFFFFF` | titolo, icone indietro/avanti, parte piena e pomello degli slider |

`NP_TEXT_3` usa `k = 140` e non 129: su fondi colorati `#8A8A8A` scenderebbe a 4,1:1. Sul fondo neutro passa da `#8A8A8A` a `#949494`, una differenza appena visibile.

### 2.2 Elementi

Posizioni invariate rispetto a `fase6/SPEC_FASE6.md` §5.1 e `fase7/SPEC_FASE7.md` (icona Home a 536,0).

| Elemento | Normale | Premuto | Disabilitato |
|---|---|---|---|
| Fondo schermata | `NP_BG` | — | — |
| Chip dispositivo | fondo `NP_SURFACE`; icona `ACCENT` se attivo, `NP_TEXT_2` se no; nome `TEXT` (o `NP_TEXT_2` se "Nessun dispositivo attivo"); chevron `NP_TEXT_2` | fondo `NP_RAISED` | — |
| Ora | `M20 NP_TEXT_2` | — | — |
| Home / Libreria / Cerca / Coda | icone 28 `NP_TEXT_2` | cerchio 56 `NP_PRESSED` | icona `NP_RAISED` (Coda vuota) |
| Copertina | immagine 300×300 | — | segnaposto: quadrato `NP_PRESSED`, nota 40 `NP_PH_FG` |
| Titolo | `SB30 TEXT` | — | — |
| Artisti | `M24 NP_TEXT_2` | — | — |
| Album | `M16 NP_TEXT_3` | — | — |
| Barra avanzamento | binario `NP_RAISED`; parte piena e pomello `TEXT` | uguale (nessun ingrandimento) | — |
| Tempi | `M16 NP_TEXT_2` | — | — |
| Indietro / Avanti | icona 28 `TEXT` | cerchio 56 `NP_PRESSED` | icona `NP_RAISED` |
| **Play / pausa** | **cerchio 88 `#FFFFFF`, icona 40 `#000000`** | cerchio `#B3B3B3` | cerchio `NP_PRESSED`, icona `NP_RAISED` |
| Icona volume | 20 `NP_TEXT_2` | — | — |
| Slider volume | come la barra | — | binario `NP_PRESSED`, niente pomello, valore "—" |
| Valore volume | `M16 NP_TEXT_2` | — | — |

**Perché il play diventa bianco.** Un verde fisso stona su metà delle copertine: su quelle verdi si confonde con il fondo, su quelle rosse vibra. Il bianco ha almeno 12,8:1 su ogni fondo possibile, è uguale al play della barra laterale ed è la scelta dell'app Spotify su questa schermata. Il verde resta l'accento di tutto il resto: icona del dispositivo attivo, indicatore della barra, brano in riproduzione nelle liste, pulsanti primari.

### 2.3 Contrasto garantito

Con `Y(NP_BG) ≤ 0,032`, su **tutte** le tinte (controllate ogni 5°, con S 0, 0,30, 0,45 e 0,60):

| Testo | Contrasto minimo | Requisito |
|---|---|---|
| `TEXT #FFFFFF` | 12,8:1 | 4,5:1 |
| `NP_TEXT_2` | 6,6:1 | 4,5:1 |
| `NP_TEXT_3` | 4,8:1 | 4,5:1 |
| Play bianco sul fondo | 12,8:1 | 3:1 |
| Icona `ACCENT` sul fondo verde `#0E391E` | 5,0:1 | 3:1 |

Se sul dispositivo il fondo sembra troppo scuro, `CC_Y` si può alzare fino a **0,040**: lì `NP_TEXT_3` resta ≥ 4,5:1. Oltre, no.

---

## 3. Dove si applica e dove no

| Elemento | Prende il colore? | Perché |
|---|---|---|
| Fondo di Now playing e i suoi grigi (§2) | **sì** | è la richiesta |
| Barra laterale delle altre schermate | **no**, resta `RAIL #000000` | le liste hanno riquadri e righe premute grigie: su un fondo colorato andrebbe ricalcolato tutto, con più ridisegni. La miniatura in cima basta a ricordare il brano. |
| Altre schermate (liste, Home, Ricerca, Dispositivi) | **no** | come sopra; anche il glitch su aree grandi si controlla meglio su una sola schermata |
| Orologio | **no**, resta `BG #121212` | si vede solo quando non suona niente: non c'è una copertina da cui prendere il colore |
| Toast sopra Now playing | **no** | restano come §4.12 di fase 6 |

---

## 4. Stati e tempi

| Situazione | Fondo | Copertina |
|---|---|---|
| Brano nuovo, copertina non ancora pronta (`07`) | **resta il colore di prima** | segnaposto nei toni del colore di prima; titolo e artisti già nuovi |
| Copertina decodificata | il colore nuovo e l'immagine compaiono **nello stesso aggiornamento dello schermo** | immagine |
| Brano nuovo con la stessa copertina (stesso album) | nessun cambio, nessun ridisegno | uguale |
| Colore nuovo quasi uguale al vecchio (ogni canale differisce di ≤ 8) | tieni il vecchio, per evitare un ridisegno inutile | immagine nuova |
| Brano senza immagine (`06`) | `#121212` subito | segnaposto neutro |
| Download fallito, o nessuna immagine dopo 4 s | `#121212` | segnaposto neutro |
| In pausa (`08`) | **nessun cambio** | uguale; cambia solo l'icona play/pausa |
| Esci da Now playing e torni | lo stesso colore, già pronto | dalla cache |
| Niente in riproduzione | si va all'Orologio, `#121212` | — |

**Cambio di colore: istantaneo, senza passi intermedi.** Due o tre passi vorrebbero dire due o tre ridisegni completi a 800×480 in un secondo, ciascuno un'occasione per i trattini. Il cambio insieme alla copertina sembra una pagina voltata e si legge come voluto. Al cambio di brano c'è comunque un solo ridisegno completo, come oggi quando cambia schermata.

**Cache.** Salva il colore insieme alla copertina, con chiave l'URL dell'immagine: ultimi 16 colori, 4 byte l'uno. Calcolalo sul core 0 subito dopo la decodifica JPEG, nel task che scarica la copertina, e passalo all'interfaccia insieme all'immagine.

**Consiglio per LVGL.** Crea uno stile per ogni token di §2.1 (`st_np_bg`, `st_np_surface`, ...) e assegnalo una volta ai widget di Now playing. Al cambio: `lv_style_set_bg_color` / `lv_style_set_text_color` su quegli stili, poi un solo `lv_obj_report_style_change(NULL)`. Niente da cercare widget per widget.

---

## 5. Modifiche alla SPEC di fase 6

**§3.1 Colori.** La regola "Niente sfumature, trasparenze o ombre" **resta valida ovunque**, Now playing compreso: il fondo colorato è una tinta unita. Sotto la tabella aggiungi:

> **Eccezione, Now playing.** Il fondo non è `BG` ma `NP_BG`, calcolato dalla copertina (`colore_copertina/SPEC_COLORE.md` §1). In Now playing `SURFACE`, `PRESSED`, `RAISED`, `DISABLED_FG_2`, `PLACEHOLDER_FG`, `TEXT_2` e `TEXT_3` diventano i toni `NP_*` (stessa specifica, §2.1). `TEXT` resta `#FFFFFF`. Senza copertina `NP_BG = BG` e i toni tornano ai valori della tabella (`TEXT_3` → `#949494`).

**§4.15 Chip.** Chip dispositivo in Now playing: fondo `NP_SURFACE`, premuto `NP_RAISED`. Sull'Orologio non cambia.

**§4.16 Slider.** In Now playing il binario è `NP_RAISED`; da disabilitato `NP_PRESSED`.

**§5.1 Now playing.** Nella tabella:
- Play/pausa: cerchio 88 `#FFFFFF`, icona 40 `#000000`, premuto `#B3B3B3`, al posto di `ACCENT` / `ACCENT_PRESSED`.
- Ora, icone in alto, artisti, tempi e volume: `NP_TEXT_2`.
- Album: `NP_TEXT_3`.
- Cerchio di Indietro/Avanti premuti: `NP_PRESSED`.

**Non cambiano:** Orologio (anche il suo pulsante verde "Home"), barra laterale, liste, Home, tastiera, toast, spinner, caratteri, icone. Non serve nessun carattere o icona nuova.

**Sfumatura: non proposta.** Su 480 px, in RGB565 e con tinte scure, il rosso e il blu hanno 3-5 livelli in tutto: verrebbero bande larghe 100 px. In più la barra e il tempo, ridisegnati ogni secondo, ricalcolerebbero la sfumatura sotto. La tinta unita è la versione definitiva, non una riserva.

---

## 6. Da verificare sul dispositivo (Claude Code)

1. **Trattini con il Wi-Fi attivo** sui fondi più "accesi". Sono il blu (`#1A2E66`, il canale più alto arriva a 102) e il grigio delle copertine acromatiche (`#303030`). Un comando seriale di prova per forzare il fondo (es. `COLORE 1A2E66`) permette di scorrere i sei fondi del §1.4 senza cercare i brani. Se i trattini compaiono, abbassa `CC_Y` a 0,026 e `CC_L_START` a 0,26.
2. **Resa dei toni vicini in RGB565.** `NP_SURFACE` e `NP_PRESSED` differiscono di pochi livelli: controlla che il chip e il cerchio premuto si distinguano dal fondo. Se no, alza i loro k a 24 e 32.
3. **Saturazione percepita sullo schermo vero.** Se i fondi sembrano troppo accesi, abbassa `CC_S_MAX` a 0,50; se sono troppo spenti, alza `CC_Y` (max 0,040).
4. **Tempo del ridisegno completo** al cambio di colore: deve essere come un cambio di schermata. Con brani dello stesso album non deve esserci nessun ridisegno.
5. **Soglia del 5%** con copertine vere: una bianca con un logo piccolo, un ritratto in bianco e nero, una foto notturna. Se una copertina "grigia" prende un colore a caso (rumore JPEG), alza `CC_MIN_C` a 32.
6. **Valori di prova del §1.4** riprodotti dal codice, con tolleranza ±2 per canale.
7. **Memoria:** 16 colori in cache, 2500 campioni letti al volo dalla copertina già in PSRAM. Niente buffer nuovi.
