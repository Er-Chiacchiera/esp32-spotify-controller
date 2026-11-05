// PROPOSTA di Claude Design (SPEC_SCATOLA.md): copia di scatola/scatola.scad del 5/11 con le modifiche al supporto.

// Scatola del controller Spotify per la VIEWE UEDX80480050E-WB-A (5", 800x480).
// Misure della scheda dal disegno ufficiale "UEDX80480050E-WB-A V3.3 SPEC", pagina 12.
//
// Tre pezzi da stampare:
//   cornice  - fronte con la finestra dello schermo e le pareti (si stampa a faccia in giu')
//   retro    - coperchio posteriore con le colonnine delle viti (si stampa a faccia in giu')
//   supporto - leggio da scrivania inclinato di 20 gradi (si stampa coricato su un fianco)
// Viti: 4 x M3x10 a testa cilindrica, dal retro nelle colonnine della cornice.
//
// Esportare un pezzo:  openscad -D 'part="cornice"' -o cornice.stl scatola.scad
// Viste: "insieme" (sul supporto), "esploso", "scheda" (solo la sagoma della scheda).

part = "insieme";
posa = false;          // true: il pezzo esportato e' gia' nella posizione sul supporto (per i render)
$fn = 64;

// ---------------------------------------------------------------- scheda
// Vista frontale, schermo dritto, origine nell'angolo in basso a sinistra del circuito.
PCB_W = 136.73;
PCB_H = 83.70;
PCB_T = 1.60;
HOLE_IN = 3.50;                    // fori M3 (diametro 3,2) a 3,5 mm dai bordi
LCM = [8.01, 3.95, 120.70, 75.80]; // vetro: x, y, larghezza, altezza
AA  = [14.36, 11.39, 108.00, 64.80]; // area attiva (800x480)
LCM_T = 4.80;                      // dal fronte del circuito al vetro
BACK_PARTS = 3.6;                  // componenti piu' alti sul retro (prese USB-C, modulo ESP32)

// Bordo inferiore (prese verso il basso), distanze dal bordo sinistro visto da davanti
UART_X  = 50.69;
USB_X   = 68.97;
BOOT_X  = 27.27;
RESET_X = 17.02;
// Bordo destro: fessura della microSD; bordo superiore: antenna del modulo ESP32
SD_Y  = PCB_H - 14.26;
ESP_X = PCB_W - 59.20;

// ---------------------------------------------------------------- scatola
GAP     = 0.4;   // gioco attorno al circuito
WALL    = 2.0;
BACK_T  = 2.0;
FRONT_T = 1.5;
R_OUT   = 5.0;   // raggio degli angoli visti da davanti
R_EDGE  = 1.6;   // arrotondamento degli spigoli fronte/retro

PCB_BACK_Z  = BACK_T + BACK_PARTS + 1.0;  // 6,6
PCB_FRONT_Z = PCB_BACK_Z + PCB_T;         // 8,2
GLASS_Z     = PCB_FRONT_Z + LCM_T;        // 13,0
FRONT_IN_Z  = GLASS_Z + 0.4;              // 13,4 (gioco per la tolleranza del vetro)
TOP_Z       = FRONT_IN_Z + FRONT_T;       // 14,9: spessore totale

OX = -GAP - WALL;
OY = -GAP - WALL;
OW = PCB_W + 2 * (GAP + WALL);            // 141,53
OH = PCB_H + 2 * (GAP + WALL);            // 88,50

WIN_M = 0.8;                              // la finestra supera l'area attiva di 0,8 mm
PORT_Z = PCB_BACK_Z - 1.6;                // centro delle prese USB-C
PORT_W = 12.5;                            // spina del cavo con la sua plastica
PORT_H = 7.0;

HOLES = [[HOLE_IN, HOLE_IN], [PCB_W - HOLE_IN, HOLE_IN],
         [HOLE_IN, PCB_H - HOLE_IN], [PCB_W - HOLE_IN, PCB_H - HOLE_IN]];

// ---------------------------------------------------------------- supporto
TILT    = 25;    // inclinazione all'indietro
SHELF_H = 36;    // altezza del bordo inferiore della scatola dal tavolo: spazio per spina e cavo
ST_W    = 116;   // larghezza del supporto
ST_T    = 4.0;   // spessore delle pareti del supporto
ST_GAP  = 0.4;
REST_T  = 50;    // fin dove arriva lo schienale sulla scatola
LIP_H   = 4;     // labbro davanti alla scatola
ROD_D   = 12;    // tondino d'acciaio di zavorra
BOSS_D  = 20;    // rinforzo attorno al tondino
FOOT_U  = 82;    // dove finisce la base dietro

// ---------------------------------------------------------------- forme di base
module rr2d(x, y, w, h, r) {
    translate([x + r, y + r]) offset(r = r) square([w - 2 * r, h - 2 * r]);
}
module outer2d(d = 0) { rr2d(OX - d, OY - d, OW + 2 * d, OH + 2 * d, max(0.5, R_OUT + d)); }
module inner2d(d = 0) { rr2d(-GAP - d, -GAP - d, PCB_W + 2 * (GAP + d), PCB_H + 2 * (GAP + d), max(0.3, 1 + d)); }

// Lastra con gli spigoli arrotondati sul lato z1 (fronte) o z0 (retro)
module soft_slab(z0, z1, round_top = true, round_bottom = false, r = R_EDGE) {
    steps = 6;
    hull() {
        translate([0, 0, z0 + (round_bottom ? r : 0)])
            linear_extrude(z1 - z0 - (round_top ? r : 0) - (round_bottom ? r : 0)) outer2d();
        for (i = [1 : steps]) {
            a = 90 * i / steps;
            if (round_top)
                translate([0, 0, z1 - r + r * sin(a) - 0.01]) linear_extrude(0.01) outer2d(-(r - r * cos(a)));
            if (round_bottom)
                translate([0, 0, z0 + r - r * sin(a)]) linear_extrude(0.01) outer2d(-(r - r * cos(a)));
        }
    }
}

module pill_y(x, z, w, h, y0, len) {   // asola orizzontale che attraversa una parete lungo Y
    translate([x, y0, z]) rotate([-90, 0, 0])
        hull() for (dx = [-(w - h) / 2, (w - h) / 2]) translate([dx, 0, 0]) cylinder(d = h, h = len);
}

// Aperture sui fianchi, uguali per cornice e retro
module side_openings() {
    for (x = [UART_X, USB_X]) pill_y(x, PORT_Z, PORT_W, PORT_H, OY - 1, WALL + GAP + 2);
    for (x = [BOOT_X, RESET_X])         // fori per premere boot e reset con una graffetta
        translate([x, OY - 1, PCB_BACK_Z - 1.4]) rotate([-90, 0, 0]) cylinder(d = 2.4, h = WALL + GAP + 2);
    // microSD sul bordo destro
    translate([PCB_W - 1, SD_Y - 7, PCB_BACK_Z - 2.8]) cube([GAP + WALL + 2, 14, 3.4]);
    // incavo per il dito attorno alla microSD
    translate([OX + OW + 5.2, SD_Y, PCB_BACK_Z - 1.1]) scale([1, 1, 0.55]) sphere(d = 16);
}

// ---------------------------------------------------------------- cornice
module cornice() {
    difference() {
        union() {
            difference() {
                soft_slab(BACK_T, TOP_Z, round_top = true);
                translate([0, 0, BACK_T - 1]) linear_extrude(FRONT_IN_Z - BACK_T + 1) inner2d();
            }
            // colonnine: tengono il circuito a distanza dal fronte e ricevono le viti
            for (p = HOLES) translate([p[0], p[1], PCB_FRONT_Z]) cylinder(d = 6.4, h = FRONT_IN_Z - PCB_FRONT_Z + 0.01);
        }
        // finestra con lo smusso a 45 gradi, cosi' il dito arriva fino ai bordi dello schermo
        hull() {
            translate([0, 0, FRONT_IN_Z - 0.01]) linear_extrude(0.01)
                rr2d(AA[0] - WIN_M, AA[1] - WIN_M, AA[2] + 2 * WIN_M, AA[3] + 2 * WIN_M, 0.6);
            translate([0, 0, TOP_Z + 0.01]) linear_extrude(0.01)
                rr2d(AA[0] - WIN_M - FRONT_T, AA[1] - WIN_M - FRONT_T,
                     AA[2] + 2 * (WIN_M + FRONT_T), AA[3] + 2 * (WIN_M + FRONT_T), 0.6 + FRONT_T);
        }
        // fori pilota per viti M3 autofilettanti (o inserti a caldo M3 da 4 mm)
        for (p = HOLES) translate([p[0], p[1], PCB_FRONT_Z - 1]) cylinder(d = 2.6, h = FRONT_IN_Z - PCB_FRONT_Z + 1);
        side_openings();
    }
}

// ---------------------------------------------------------------- retro
module retro() {
    difference() {
        union() {
            soft_slab(0, BACK_T, round_top = false, round_bottom = true);
            // bordino che entra nella cornice e la centra
            translate([0, 0, BACK_T - 0.01]) linear_extrude(1.6) difference() { inner2d(-0.25); inner2d(-1.85); }
            // colonnine: larghe in basso (testa della vite), strette in alto (accanto c'e' la microSD)
            intersection() {
                for (p = HOLES) translate([p[0], p[1], BACK_T - 0.01]) {
                    cylinder(d = 8.0, h = 2.4);
                    cylinder(d = 5.6, h = PCB_BACK_Z - BACK_T + 0.01);
                }
                translate([0, 0, -1]) linear_extrude(20) inner2d(-0.25);
            }
        }
        for (p = HOLES) translate([p[0], p[1], -1]) {
            cylinder(d = 3.4, h = 20);
            cylinder(d = 6.0, h = 1 + 3.0);   // testa della vite a filo del retro
        }
        // feritoie sopra il modulo ESP32 (scalda un po')
        for (i = [0 : 5]) translate([ESP_X, PCB_H - 8 - i * 4.2, -1])
            linear_extrude(BACK_T + 2) hull() for (dx = [-20, 20]) translate([dx, 0]) circle(d = 2.0);
        side_openings();
    }
}

// ---------------------------------------------------------------- sagoma della scheda (solo per le viste)
module scheda() {
    translate([0, 0, PCB_BACK_Z]) {
        color("#1b1b1b") difference() {
            cube([PCB_W, PCB_H, PCB_T]);
            for (p = HOLES) translate([p[0], p[1], -1]) cylinder(d = 3.2, h = 5);
        }
        color("#101010") translate([LCM[0], LCM[1], PCB_T]) cube([LCM[2], LCM[3], LCM_T]);
        color("#c8c8c8") {
            for (x = [UART_X, USB_X]) translate([x - 4.5, -0.5, -3.2]) cube([9, 7.5, 3.2]);
            translate([ESP_X - 9, PCB_H - 25.5, -3.1]) cube([18, 25.5, 3.1]);
            translate([PCB_W - 15, SD_Y - 7, -1.9]) cube([15, 14, 1.9]);
        }
    }
}

// ---------------------------------------------------------------- supporto
function g(s, t) = [s * cos(TILT) + t * sin(TILT), SHELF_H - s * sin(TILT) + t * cos(TILT)];

module bar(a, b, d = ST_T) { hull() { translate(a) circle(d = d); translate(b) circle(d = d); } }

module supporto() {
    T = TOP_Z;
    c = ST_GAP + ST_T / 2;
    lip_top   = g(-c, LIP_H);
    lip_bot   = g(-c, -c);
    shelf_end = g(T + c, -c);
    rest_top  = g(T + c, REST_T);
    // il davanti continua l'inclinazione dello schermo fino al tavolo
    front     = [lip_bot[0] - (lip_bot[1] - ST_T / 2) * tan(TILT), ST_T / 2];
    rear      = [FOOT_U, ST_T / 2];
    zb        = BOSS_D / 2 - 1;   // centro del tondino (la base parte da z = -1)
    boss      = [front[0] + (zb - ST_T / 2) * tan(TILT) + (BOSS_D / 2 - ST_T / 2) / cos(TILT), zb];
    difference() {
        rotate([90, 0, 90]) translate([0, 0, -ST_W / 2]) linear_extrude(ST_W) {
            bar(lip_top, lip_bot);
            bar(lip_bot, shelf_end);
            bar(shelf_end, rest_top);
            bar(lip_bot, front);
            bar(rest_top, rear);
            bar(front, rear, ST_T + 2);   // base piu' spessa: tiene giu' il supporto quando si tocca lo schermo
            hull() { translate(boss) circle(d = BOSS_D); translate(front) circle(d = ST_T + 2);
                     translate([boss[0] + BOSS_D / 2, ST_T / 2]) circle(d = ST_T + 2); }
        }
        // foro passante per il tondino di zavorra (lungo X: si stampa verticale, rotondo)
        translate([-ST_W / 2 - 1, boss[0], boss[1]]) rotate([0, 90, 0]) cylinder(d = ROD_D + 0.4, h = ST_W + 2);
        // asola nel ripiano per le spine USB-C
        slot_x = (UART_X + USB_X) / 2 - PCB_W / 2;
        sm = g(T - PORT_Z, 0);
        translate([slot_x, sm[0], sm[1]]) rotate([-TILT, 0, 0])
            translate([-(USB_X - UART_X + PORT_W + 2) / 2, -(PORT_H + 2) / 2, -12])
            linear_extrude(14) rr2d(0, 0, USB_X - UART_X + PORT_W + 2, PORT_H + 2, 2);
        // l'asola continua fino allo schienale: passa la spina a L
        sw = USB_X - UART_X + PORT_W + 2;
        a0 = g(T - PORT_Z - (PORT_H + 2) / 2, 1); a1 = g(T + c + ST_T / 2 + 6, 1);
        b0 = g(T - PORT_Z - (PORT_H + 2) / 2, -16); b1 = g(T + c + ST_T / 2 + 6, -16);
        translate([slot_x - sw / 2, 0, 0]) rotate([90, 0, 90]) linear_extrude(sw) polygon([a0, a1, b1, b0]);
        // uscita del cavo sul retro della base: arco 16 x 10
        translate([slot_x, FOOT_U - 20, 0]) rotate([-90, 0, 0]) linear_extrude(40)
            hull() { for (dx = [-4, 4]) translate([dx, -6]) circle(r = 4); translate([-8, 2]) square([16, 1]); }
        // incavi per 4 piedini adesivi in gomma da 10 mm
        for (x = [-ST_W / 2 + 10, ST_W / 2 - 10], u = [front[0] + 9, FOOT_U - 9])
            translate([x, u, -1.01]) cylinder(d = 10.5, h = 1.0);
    }
}

// Posizione della scatola appoggiata sul supporto (x centrato, Y verso il retro, Z in alto)
SLAB_M = [[1, 0, 0, -PCB_W / 2],
          [0, sin(TILT), -cos(TILT), -OY * sin(TILT) + TOP_Z * cos(TILT)],
          [0, cos(TILT),  sin(TILT), SHELF_H - OY * cos(TILT) - TOP_Z * sin(TILT)],
          [0, 0, 0, 1]];
echo(SLAB_M = SLAB_M);

module on_stand() { multmatrix(SLAB_M) children(); }

// ---------------------------------------------------------------- uscita
if (part == "cornice")       { if (posa) on_stand() cornice(); else cornice(); }
else if (part == "retro")    { if (posa) on_stand() retro();   else retro(); }
else if (part == "supporto") supporto();
else if (part == "scheda")   { if (posa) on_stand() scheda();  else scheda(); }
else if (part == "esploso") {
    color("#2b2b2b") translate([0, 0, 40]) cornice();
    scheda();
    color("#2b2b2b") translate([0, 0, -30]) retro();
}
else {   // insieme
    color("#3a3a3a") supporto();
    on_stand() { color("#2b2b2b") cornice(); scheda(); color("#2b2b2b") retro(); }
}
