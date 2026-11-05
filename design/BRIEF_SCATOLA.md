# Brief: revisione della scatola stampata in 3D

5 novembre 2026. Prima versione della scatola disegnata in OpenSCAD e renderizzata con Blender. A Valerio la forma piace; prima di stamparla vogliamo un secondo parere sul design.

## Cosa è
Il controller Spotify (scheda VIEWE 5", 800×480, touch) sta sulla scrivania accanto al computer. La scatola ha tre pezzi:
- **cornice**: il davanti con la finestra dello schermo, smussata a 45° perché il dito arrivi ai bordi;
- **retro**: chiuso da 4 viti M3, con le feritoie per il calore;
- **supporto**: un leggio che tiene lo schermo inclinato di 20° all'indietro, con la scatola 36 mm sopra il tavolo.

Le prese USB-C sono sul bordo inferiore. Il cavo scende dentro il supporto passando da un'apertura nel ripiano, corre sul fondo ed esce da dietro.

Misure:
- scatola: 141,5 × 88,5 × 14,9 mm;
- con il supporto: circa 142 × 90 × 120 mm.

## Cosa guardare
I render sono in `/Users/valerioribaldi/Documents/Claude/Spotify controller/scatola/render/`, in nero e in bianco:
1. davanti;
2. dietro;
3. di lato;
4. pezzi separati;
5. retro senza supporto;
6. bordo di sotto con le prese;
7. cavo che esce dietro;
8. percorso del cavo visto di lato.

Le misure e le istruzioni sono in `scatola/LEGGIMI.md`, il modello in `scatola/scatola.scad` (i numeri in cima).

## Domande
1. **Aspetto:** proporzioni della cornice attorno allo schermo, spessore, raggi degli angoli. Sta bene su una scrivania accanto a un Mac? Nero o bianco?
2. **Supporto:** il triangolo aperto sui fianchi è bello o sembra incompiuto? Meglio chiuderlo, o dargli un'altra forma, ad esempio un blocco pieno o un piede unico?
3. **Uso col dito:** 20° di inclinazione e 36 mm di altezza sono comodi per toccare lo schermo da seduti? Con un tocco deciso in alto il supporto può inclinarsi all'indietro: come lo renderesti più stabile senza appesantirlo troppo?
4. **Cavo:** sotto la spina restano circa 9 mm prima del fondo, quindi un cavo dritto piega stretto. Meglio alzare il supporto di 1 cm o consigliare un cavo con la spina a L?
5. Dettagli che manderesti a correggere prima di stampare.

## Vincoli
- Le misure della scheda sono fisse (disegno ufficiale VIEWE): la finestra deve scoprire tutta l'area attiva e coprire il bordino nero.
- Stampa FDM in PLA o PETG, senza supporti di stampa.
- Restano accessibili: le prese USB in basso, i forellini di boot e reset, la microSD sul lato destro.

## Risultato
Scrivi `/Users/valerioribaldi/Documents/Claude/Spotify controller/design/scatola/SPEC_SCATOLA.md` con:
- le scelte e il perché;
- le modifiche in numeri (millimetri e gradi), da riportare in `scatola.scad`;
- un render o uno schizzo, se aiuta.

Se va bene così com'è, scrivilo e indica solo i ritocchi.
