# Brief per Claude Design: Controller Spotify, sfondo dal colore della copertina

Ciao Claude Design! Questo brief l'ha preparato Claude Code, che scrive il firmware. Valerio vuole che **Now playing prenda il colore della copertina**, come fa l'app di Spotify. Le scelte di design le fai tu; a me servono regole precise in numeri da tradurre in LVGL 8.

**Il punto di partenza.** Il sistema della `SPEC_FASE6.md` è realizzato e funziona sulla scheda. La SPEC §3.1 oggi dice "niente sfumature, trasparenze o ombre", e Now playing ha il fondo fisso `BG #121212`. Questa richiesta cambia quella regola **solo dove decidi tu**: dimmi esplicitamente cosa cambia e cosa resta valido.

**Allegati:**
- cattura reale 800×480 di Now playing: `fase6/schermate/01_now_playing.png`;
- il tuo mockup: `fase6/mockup/02_now-playing.png`;
- l'Orologio (si vede quando non suona niente): `fase6/mockup/02b_orologio.png`.

---

## 1. Cosa serve progettare

1. **La regola del colore.** Da una copertina di 300×300 pixel ricavo il colore che mi dici. Posso calcolare, in pochi millisecondi, una qualsiasi di queste cose:
   - media dei pixel;
   - colore più frequente (con i pixel raggruppati per tinta);
   - colore più frequente fra quelli "vivi", scartando grigi, quasi neri e quasi bianchi;
   - luminosità e saturazione, per poi scurirlo o desaturarlo.

   Dammi la ricetta in numeri: come lo scelgo, come lo correggo (es. "luminosità massima L 25%, saturazione massima 60%") e cosa uso se la copertina è quasi tutta grigia, nera o bianca.
2. **Dove si applica.** Solo il fondo di Now playing, o anche:
   - il chip del dispositivo (oggi `SURFACE #242424`);
   - i binari degli slider (oggi `RAISED #3A3A3A`);
   - il pulsante play (oggi verde `ACCENT`);
   - la barra laterale delle altre schermate, con la miniatura in alto;
   - l'Orologio.
3. **Testi e controlli sopra il colore.** Bianco e grigi restano gli stessi, oppure cambiano in base al fondo? Mi serve una regola di contrasto che funzioni con *qualsiasi* copertina. Attenzione al pulsante play verde sopra copertine verdi.
4. **Stati:**
   - brano nuovo con la copertina non ancora arrivata (arriva 0,3-1,8 s dopo il cambio): resta il colore vecchio, si torna a `#121212`, altro?
   - brano senza copertina;
   - in pausa: cambia qualcosa?
   - cambio di colore: per forza istantaneo (vedi §2), a meno che tu voglia 2-3 passi intermedi.
5. **Mockup 800×480** di Now playing con almeno 4 copertine diverse:
   - scura;
   - chiara o bianca;
   - molto colorata;
   - in bianco e nero.

   Se vuoi, prendi la cattura come base.

## 2. Vincoli tecnici (importanti per questa richiesta)

- **Il fondo deve restare scuro.** Con il Wi-Fi attivo lo schermo ha un difetto noto: sotto le zone chiare compaiono trattini lampeggianti, pezzi di righe disegnati più in alto (SPEC fase 6 §9). Sui fondi scuri non si vedono. Un fondo di colore pieno e acceso, anche solo medio-chiaro, rischia di farli vedere su tutta la schermata. Quindi meglio un colore **scurito** (indicativamente luminosità sotto il 25-30%), poi lo verifico sul dispositivo.
- **Colori RGB565.** Lo schermo ha 65 mila colori: 5 bit di rosso, 6 di verde, 5 di blu. Una sfumatura su 480 px di altezza mostra **bande visibili**, soprattutto con le tinte scure. La correzione di LVGL (dithering) costa RAM che non abbiamo.
- **Tinta unita: nessun problema.** Una sfumatura verticale semplice (due colori, alto→basso) si può provare, ma va verificata sul dispositivo: la barra di avanzamento e il tempo si ridisegnano ogni secondo e ogni ridisegno ricalcola la sfumatura sotto. Se proponi una sfumatura, dammi anche la versione a tinta unita di riserva.
- **Niente animazioni, dissolvenze o trasparenze su aree grandi.** Il cambio di colore è istantaneo; al massimo 2-3 passi intermedi molto distanziati.
- **Copertine "difficili" da prevedere:** foto quasi nere, copertine bianche con poco testo, copertine con due colori forti, ritratti in bianco e nero.

## 3. Cosa ci serve da te

Stesso formato delle fasi 6 e 7:
1. **Ricetta del colore** in numeri (§1.1), con i casi limite.
2. **Mockup 800×480** (§1.5).
3. **Colori e regole** per ogni elemento di Now playing sopra il fondo colorato, stati compresi (normale, premuto, disabilitato).
4. **Modifiche alla SPEC** (§3.1 e dove serve), scritte esplicitamente.
5. Una sezione **"Da verificare sul dispositivo"**.
