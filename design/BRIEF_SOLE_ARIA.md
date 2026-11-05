# Brief per Claude Design: Controller Spotify, sole, luna, aria e pollini

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Il meteo sull'Orologio è finito e funziona sulla scheda, secondo la tua `meteo/SPEC_METEO.md`. Valerio vuole aggiungere altre informazioni "da guardare", dello stesso genere:

1. **Sole e luna:** alba, tramonto, ore di luce, fase della luna.
2. **Aria:** qualità dell'aria e pollini.

Le scelte di design le fai tu; a me servono posizioni, misure e colori in numeri, da tradurre in LVGL 8, come per il meteo.

**Allegati:**
- catture reali 800×480 di oggi:
  - Orologio con il meteo: `meteo/schermate/16_finale_dati_veri.png`;
  - pagina Meteo: `meteo/schermate/02_pagina_meteo.png`;
- la tua SPEC del meteo: `meteo/SPEC_METEO.md` (§1 "Spazio per altre informazioni in futuro", §3 Orologio, §4 pagina Meteo);
- il sistema di design: `fase6/SPEC_FASE6.md` (§3 colori e caratteri) e `fase7/SPEC_FASE7.md`;
- caratteri e icone già presenti: `fase6/CARATTERI_E_ICONE.md` (§3 per quelli del meteo).

---

## 1. I dati (già provati per Roma, 4 novembre sera)

Tutto arriva da Open-Meteo in HTTP semplice, senza chiave, insieme al meteo, ogni 30 minuti.

### Sole
| Dato | Esempio |
|---|---|
| Alba e tramonto di oggi, domani, dopodomani | oggi 06:46 / 17:00 · domani 06:47 / 16:59 |
| Ore di luce (calcolate) | oggi 10 h 14 min, domani 2 min in meno |
| Indice UV massimo del giorno (se ti serve) | oggi 3, domani 3, dopodomani 0 |

### Luna
Calcolata dalla scheda, senza rete: **fase** (8 nomi: Luna nuova, Falce crescente, Primo quarto, Gibbosa crescente, Luna piena, Gibbosa calante, Ultimo quarto, Falce calante), **percentuale illuminata** e **giorni alla prossima luna piena / nuova**. Alba e tramonto della luna non ci sono.

### Aria
| Dato | Esempio di stasera |
|---|---|
| Indice europeo della qualità dell'aria (EAQI), adesso | 59 |
| Lo stesso indice per le prossime 24 ore, ora per ora | 59, 55, 51 … 32 (minimo alle 13) … 66 alle 21 |
| Inquinanti principali adesso, µg/m³ | PM2,5 32,9 · PM10 38,2 · biossido di azoto 58,5 · ozono 1,0 |

Fasce ufficiali dell'EAQI (Agenzia europea dell'ambiente), con i nomi italiani:

| Indice | Giudizio |
|---|---|
| 0-20 | Buona |
| 20-40 | Discreta |
| 40-60 | Moderata |
| 60-80 | Scadente |
| 80-100 | Molto scadente |
| oltre 100 | Pessima |

### Pollini
Grani/m³ adesso e il massimo delle prossime 24 ore per 6 tipi: **graminacee, olivo, betulla, ontano, artemisia, ambrosia**. Solo in Europa.

**A novembre sono tutti a 0**: la stagione va più o meno da febbraio a settembre, quindi per buona parte dell'anno non c'è niente da mostrare. Serve uno stato "fuori stagione" o "nessun polline".

Per trasformare i grani in un livello propongo le classi della rete italiana POLLnet (da confermare o cambiare):

| Tipo | Bassa | Media | Alta |
|---|---|---|---|
| Graminacee | 0,1-9,9 | 10-29,9 | ≥ 30 |
| Olivo, artemisia, ambrosia | 0,1-4,9 | 5-24,9 | ≥ 25 |
| Betulla, ontano | 0,1-15,9 | 16-49,9 | ≥ 50 |

## 2. Cosa serve progettare

1. **Dove stanno.** Lo spazio è poco:
   - l'Orologio ha già il meteo nelle due "aree informazioni" (§1 della SPEC_METEO);
   - la pagina Meteo è piena fino a y 464.

   Possibilità: una riga piccola sull'Orologio (per esempio "Tramonto 17:00 · Aria moderata"), una pagina nuova, schede nella pagina Meteo, un'area che alterna meteo e aria… Decidi tu, e scrivi esplicitamente cosa cambia nella SPEC_METEO.
2. **Quanto mostrare** di ogni dato: per esempio dell'aria solo il giudizio, oppure anche l'andamento delle 24 ore e gli inquinanti.
3. **Tocco:** cosa si apre da dove.
4. **Colori** delle fasce dell'aria e dei livelli dei pollini, se li vuoi colorati.
5. **Icona della luna per fase.** Font Awesome 5 Free ha solo la falce (`f186`). Possibilità:
   - solo testo e percentuale;
   - 8 piccole immagini delle fasi, che posso generare io alla misura che scegli;
   - un disegno con due cerchi sovrapposti (va bene per le falci, non per le gibbose).
6. **Stati:**
   - dati non ancora arrivati;
   - errore di rete. L'aria arriva con una richiesta separata dal meteo, quindi può mancare anche con il meteo presente;
   - dato vecchio, come per il meteo;
   - pollini fuori stagione;
   - notte polare non serve: la scheda è in Italia.
7. **Mockup 800×480** per ogni schermata nuova o cambiata, con almeno:
   - aria buona e aria scadente;
   - pollini in stagione (alti) e fuori stagione;
   - una falce e una luna piena.

## 3. Vincoli tecnici (come per il meteo)

- **Sobrio come il resto:**
  - niente animazioni, sfumature larghe o trasparenze;
  - fondi scuri: sotto le zone chiare, con il Wi-Fi attivo, compaiono trattini lampeggianti (SPEC 6 §9);
  - lo schermo è RGB565.
- **Grafici:** se vuoi l'andamento dell'aria nelle 24 ore, va bene fatto di barre o rettangoli pieni, non di linee sottili oblique o curve sfumate.
- **Icone:**
  - si generano da Font Awesome 5 Free Solid nelle misure 20 / 28 / 40 / 56, o in una nuova;
  - **monocromatiche**, un colore per icona;
  - provate nel file e presenti: sun `f185`, moon `f186`, wind `f72e`, leaf `f06c`, seedling `f4d8`, tree `f1bb`, spa `f5bb`, arrow-up `f062`, arrow-down `f063`, smog `f75f` (già usata per la nebbia);
  - **assenti nel file**: lungs `f604`, head-side-mask `f963`; non c'è un'icona alba/tramonto.
- **Caratteri:** i 6 Montserrat (Medium 16/20/24, SemiBold 20/24/30), `CLOCK112` e `font_temp_64` (solo cifre, "-" e "°"). Un carattere nuovo costa poco: basta indicarne misura, peso e caratteri. Pedici come "PM₂.₅" non ci sono: scrivere "PM2,5".
- **Aggiornamento:** i dati cambiano ogni 30 minuti; ridisegno solo quando cambiano. La luna cambia una volta al giorno.
- **Luce dello schermo:** scende al 15% dopo 2 minuti senza tocchi. Tutto deve leggersi anche così.

## 4. Cosa ci serve da te

Stesso formato del meteo:
1. **SPEC** con posizioni, misure, caratteri e colori di ogni elemento, stati compresi.
2. **Mockup 800×480** (§2.7).
3. **Testi** di ogni stato e giudizio.
4. **Icone, immagini e caratteri nuovi** da generare (codici e misure).
5. **Modifiche alle SPEC precedenti**, scritte esplicitamente.
6. Una sezione **"Da verificare sul dispositivo"**.
