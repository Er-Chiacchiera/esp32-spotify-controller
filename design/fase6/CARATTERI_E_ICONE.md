# Caratteri e icone (fase 6)

Tutto ciò che va generato con `npx lv_font_conv@1.5.2`, in una volta sola. Le specifiche d'uso sono in `SPEC_FASE6.md` (§3.2 e componenti).

## 1. Caratteri di testo

Famiglia: **Montserrat** (Julieta Ulanovsky), licenza **SIL Open Font License 1.1**: uso libero anche nel firmware, nessun obbligo se non conservare la licenza.
- Medium: `lvgl/scripts/built_in_font/Montserrat-Medium.ttf` (già usato).
- SemiBold: `Montserrat-SemiBold.ttf` dal repository ufficiale `github.com/JulietaUla/Montserrat` (cartella `fonts/ttf/`) o da Google Fonts.

| Nome file proposto | Peso | px | Stato | Usato per |
|---|---|---|---|---|
| `font_it_16` | Medium 500 | 16 | esiste | sottotitoli, durate, contatori, suggerimenti |
| `font_it_20` | Medium 500 | 20 | esiste | campo di ricerca, chip, menu, ora, data |
| `font_it_24` | Medium 500 | 24 | esiste | artisti in Now playing, lettere della tastiera |
| `font_it_30` | Medium 500 | 30 | **da togliere** | sostituito da `font_it_sb30` |
| `font_it_sb20` | SemiBold 600 | 20 | **nuovo** | titoli delle righe, pulsanti, schede |
| `font_it_sb24` | SemiBold 600 | 24 | **nuovo** | titoli dell'intestazione, nome nel pannello, riquadri della libreria |
| `font_it_sb30` | SemiBold 600 | 30 | **nuovo** | titolo del brano in Now playing |
| `font_clock_112` | Medium 500 | 112 | esiste | ora dell'orologio |

In totale due caratteri in più rispetto a oggi.

**Intervalli Unicode** per tutti i caratteri di testo (16-30 px):

| Intervallo | Cosa contiene | Perché |
|---|---|---|
| `0x20-0x7E` | ASCII | base |
| `0xA0-0xFF` | Latin-1 | accenti italiani, «», ·, ° |
| `0x100-0x17F` | Latin Extended-A | nomi europei (ł, š, ő…) |
| `0x2010-0x2027` | **nuovo**: trattini – —, virgolette ‘ ’ “ ”, •, … | Spotify li usa spesso nei titoli ("Remastered – 2011", apostrofi curvi); senza, sparirebbero |
| `0x20AC` | € | facoltativo |

Orologio 112: solo `0x30-0x3A` (cifre e ":"), come oggi.

Opzioni: le stesse dei `font_it_*` attuali (`--bpp 4 --format lvgl`). Per esempio:

```bash
npx lv_font_conv@1.5.2 --bpp 4 --size 20 --format lvgl --lv-include lvgl.h \
  --font Montserrat-SemiBold.ttf -r 0x20-0x7E,0xA0-0x17F,0x2010-0x2027,0x20AC \
  -o font_it_sb20.c
```

I caratteri di testo **non devono più contenere i simboli LVGL**: le icone stanno nei caratteri `font_ic_*` qui sotto. Se i `font_it_*` attuali li hanno già, possono restare (nessun danno), ma i nuovi SemiBold non ne hanno bisogno.

**Memoria (stima)**: a 4 bpp, con circa 360 caratteri, `sb20` occupa circa 60-80 KB, `sb24` circa 90-110 KB e `sb30` circa 140-170 KB. Togliendo `font_it_30` (circa 150 KB) il saldo è di circa **+150-210 KB** sul 1,2 MB disponibile. Sono stime: misurate dopo la compilazione. Se lo spazio non basta, il primo da togliere è `font_it_sb24` (al suo posto `font_it_sb20`).

## 2. Icone

Fonte: **Font Awesome 5 Free Solid**, il file che LVGL usa già: `lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff`. Licenza: icone CC BY 4.0, font SIL OFL 1.1.
Tre caratteri di sole icone, `--bpp 4`:

| Nome file | px | Uso |
|---|---|---|
| `font_ic_20` | 20 | icone accanto al testo: righe, chip, pulsanti, menu, segnaposto da 64 |
| `font_ic_28` | 28 | barra laterale, icone in alto, trasporto, colonna pagine, cerchi dei dispositivi |
| `font_ic_40` | 40 | play/pausa grande in Now playing, blocchi vuoto/errore, segnaposto da 150 |

| Icona | Nome FA5 | Codice | Simbolo LVGL | 20 | 28 | 40 | Dove |
|---|---|---|---|:-:|:-:|:-:|---|
| Play | play | `0xF04B` | `LV_SYMBOL_PLAY` | ● | ● | ● | Riproduci, barra, Now playing in pausa |
| Pausa | pause | `0xF04C` | `LV_SYMBOL_PAUSE` | | ● | ● | barra, Now playing |
| Precedente | step-backward | `0xF048` | `LV_SYMBOL_PREV` | | ● | | Now playing |
| Successivo | step-forward | `0xF051` | `LV_SYMBOL_NEXT` | | ● | | Now playing |
| Volume / cassa | volume-up | `0xF028` | `LV_SYMBOL_VOLUME_MAX` | ● | ● | | volume, brano in riproduzione, dispositivo "cassa" |
| Cerca | search | `0xF002` | — | ● | ● | ● | barra, campo, chip, nessun risultato |
| Libreria | book | `0xF02D` | — | ● | ● | | barra, icone in alto, pulsanti |
| Coda | th-list | `0xF00B` | `LV_SYMBOL_LIST` | | ● | ● | barra, coda vuota, playlist senza elenco |
| Nota (playlist, brano) | music | `0xF001` | `LV_SYMBOL_AUDIO` | ● | ● | ● | riquadro, segnaposto, tipo sconosciuto |
| Cuore | heart | `0xF004` | — | | ● | ● | Brani piaciuti |
| Disco (album) | compact-disc | `0xF51F` | — | ● | | ● | segnaposto album, menu |
| Persona (artista) | user | `0xF007` | — | ● | ● | ● | segnaposto artisti, menu, riquadro |
| Indietro | chevron-left | `0xF053` | `LV_SYMBOL_LEFT` | | ● | | intestazione |
| Pagina su | chevron-up | `0xF077` | `LV_SYMBOL_UP` | | ● | | colonna pagine |
| Pagina giù / apri | chevron-down | `0xF078` | `LV_SYMBOL_DOWN` | ● | ● | | colonna pagine, chip dispositivo |
| Aggiungi | plus | `0xF067` | `LV_SYMBOL_PLUS` | ● | | | menu "Aggiungi alla coda" |
| Chiudi / cancella | times | `0xF00D` | `LV_SYMBOL_CLOSE` | ● | | | × del campo di ricerca |
| Fatto | check | `0xF00C` | `LV_SYMBOL_OK` | ● | | | dispositivo attivo |
| Aggiorna / riprova | sync | `0xF021` | `LV_SYMBOL_REFRESH` | ● | ● | | Riprova, Aggiorna dispositivi |
| Avviso | exclamation-triangle | `0xF071` | `LV_SYMBOL_WARNING` | ● | | ● | avviso arancio, errore di rete |
| Cancella carattere | backspace | `0xF55A` | `LV_SYMBOL_BACKSPACE` | | ● | | tastiera |
| Computer | desktop | `0xF108` | — | ● | ● | ● | dispositivi, chip, nessun dispositivo |
| Telefono | mobile-alt | `0xF3CD` | — | | ● | | dispositivi |
| Tablet | tablet-alt | `0xF3FA` | — | | ● | | dispositivi |
| TV | tv | `0xF26C` | — | | ● | | dispositivi |
| Console | gamepad | `0xF11B` | — | | ● | | dispositivi |
| Auto | car | `0xF1B9` | — | | ● | | dispositivi |
| Wi‑Fi | wifi | `0xF1EB` | `LV_SYMBOL_WIFI` | ● | | | orologio |
| Home (fase 7) | home | `0xF015` | `LV_SYMBOL_HOME` | | ● | | posto riservato nella barra |

Intervalli pronti da copiare:

```text
font_ic_20: 0xF04B,0xF028,0xF002,0xF02D,0xF001,0xF51F,0xF007,0xF078,0xF067,0xF00D,0xF00C,0xF021,0xF071,0xF108,0xF1EB
font_ic_28: 0xF04B,0xF04C,0xF048,0xF051,0xF028,0xF002,0xF02D,0xF00B,0xF001,0xF004,0xF007,0xF053,0xF077,0xF078,0xF021,0xF55A,0xF108,0xF3CD,0xF3FA,0xF26C,0xF11B,0xF1B9,0xF015
font_ic_40: 0xF04B,0xF04C,0xF002,0xF00B,0xF001,0xF004,0xF51F,0xF007,0xF071,0xF108
```

```bash
npx lv_font_conv@1.5.2 --bpp 4 --size 28 --format lvgl --lv-include lvgl.h \
  --font FontAwesome5-Solid+Brands+Regular.woff \
  -r 0xF04B,0xF04C,0xF048,0xF051,0xF028,0xF002,0xF02D,0xF00B,0xF001,0xF004,0xF007,0xF053,0xF077,0xF078,0xF021,0xF55A,0xF108,0xF3CD,0xF3FA,0xF26C,0xF11B,0xF1B9,0xF015 \
  -o font_ic_28.c
```

Memoria: in tutto circa 50 glifi, **meno di 40 KB** per i tre caratteri.

Nota sui mockup: le icone nei PNG sono disegnate a mano e somigliano a quelle Font Awesome, ma non sono identiche (la Libreria, per esempio, è disegnata come "tre dorsi di libro"). Sullo schermo valgono i glifi di questa tabella.

## 3. Meteo (aggiunto il 4 novembre 2026, `design/meteo/SPEC_METEO.md` §6)

Generati con `--no-compress --bpp 4`; il comando completo di ogni file è nella riga `Opts` in cima al `.c`.

| File | Contenuto |
|---|---|
| `font_temp_64` (nuovo) | Montserrat Medium 64, `0x2D,0x30-0x39,0xB0` (`--force-fast-kern-format`); riga di 47 px |
| `font_wx_56` (nuovo) | le 12 icone meteo `0xF185,0xF186,0xF6C4,0xF6C3,0xF0C2,0xF75F,0xF73D,0xF740,0xF743,0xF73C,0xF2DC,0xF0E7` |
| `font_ic_28`, `font_ic_40` | + le stesse 12 icone |
| `font_ic_20` | + goccia `0xF043` (ora contiene anche chevron `0xF054` e casa `0xF015` della fase 7) |

Spazio occupato dal firmware: da 2 105 518 a 2 155 950 byte (+49 KB, 68% della partizione).

## 4. Sole e aria (aggiunto il 5 novembre 2026, `design/sole_aria/SPEC_SOLE_ARIA.md` §6 e §8)

| File | Contenuto |
|---|---|
| `font_ic_20` | + sole `0xF185` (`IC_SUN`), vento `0xF72E` (`IC_WIND`), spa `0xF5BB` (`IC_SPA`) |
| `font_ic_40` | + vento `0xF72E`, spa `0xF5BB` |
| `moon_images.c` (nuovo) | le 8 fasi della luna a 64 e 20 px, `LV_IMG_CF_TRUE_COLOR_ALPHA`, generate da `strumenti/genera_luna.py` con la geometria del §6 (`MOON_64[fase]`, `MOON_20[fase]`) |

Le immagini della luna occupano 108 KB e non 16: 8 fasi a 64×64 con trasparenza sono già 98 KB. Firmware da 2 166 082 a 2 289 406 byte (72% della partizione).

## 5. Home e giorni (aggiunto il 5 novembre 2026, `design/home_giorni/SPEC_HOME_GIORNI.md` §9)

| File | Contenuto |
|---|---|
| `font_ic_28` | + cuffie `0xF025` (`IC_HEADPHONES`), ingranaggio `0xF013` (`IC_COG`), chevron-right `0xF054`, wifi `0xF1EB`, info-circle `0xF05A` (`IC_INFO`) |
| `font_ic_20` | + cuffie `0xF025` |
