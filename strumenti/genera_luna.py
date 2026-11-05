"""Genera firmware/moon_images.c: le 8 fasi della luna a 64 e 20 px (design/sole_aria/SPEC_SOLE_ARIA.md §6).

Formato LV_IMG_CF_TRUE_COLOR_ALPHA con LV_COLOR_DEPTH 16 e LV_COLOR_16_SWAP 0: per ogni pixel RGB565 in due
byte (prima il basso) e l'opacità. Bordi sfumati con 8x8 campioni per pixel. Uso: python3 strumenti/genera_luna.py
"""
import math
import pathlib

LIT = (0xD6, 0xD6, 0xD6)   # MOON_LIT
DARK = (0x3A, 0x3A, 0x3A)  # MOON_DARK
# (parte illuminata, lato illuminato: +1 destro quando cresce, -1 sinistro quando cala)
PHASES = [(0, 1), (0.22, 1), (0.5, 1), (0.78, 1), (1, 1), (0.78, -1), (0.5, -1), (0.22, -1)]
NAMES = ["nuova", "falce_crescente", "primo_quarto", "gibbosa_crescente", "piena", "gibbosa_calante",
         "ultimo_quarto", "falce_calante"]
SS = 8


def pixels(size, k, side):
    r = size / 2
    out = []
    for py in range(size):
        for px in range(size):
            disk = lit = 0
            for sy in range(SS):
                for sx in range(SS):
                    x = (px + (sx + 0.5) / SS) - r
                    y = (py + (sy + 0.5) / SS) - r
                    if x * x + y * y > r * r:
                        continue
                    disk += 1
                    # terminatore: mezza ellisse con semiasse orizzontale r*(1-2k) dal lato illuminato
                    t = (1 - 2 * k) * math.sqrt(max(0.0, r * r - y * y))
                    if k >= 1 or (k > 0 and x * side > t):
                        lit += 1
            if disk == 0:
                out.append((0, 0))
                continue
            f = lit / disk
            rgb = [round(d + (l - d) * f) for d, l in zip(DARK, LIT)]
            c565 = ((rgb[0] >> 3) << 11) | ((rgb[1] >> 2) << 5) | (rgb[2] >> 3)
            out.append((c565, round(255 * disk / (SS * SS))))
    return out


lines = ["// Generato da strumenti/genera_luna.py: non modificare a mano.", "#include \"lvgl.h\"", ""]
for size in (64, 20):
    for i, (k, side) in enumerate(PHASES):
        data = []
        for c, a in pixels(size, k, side):
            data += [c & 0xFF, c >> 8, a]
        name = f"moon_{size}_{i}"
        lines.append(f"static const uint8_t {name}_map[] = {{")
        for j in range(0, len(data), 24):
            lines.append("  " + ",".join(f"0x{b:02x}" for b in data[j:j + 24]) + ",")
        lines.append("};")
        lines.append(f"const lv_img_dsc_t {name} = {{  // {NAMES[i]}")
        lines.append(f"  .header = {{.cf = LV_IMG_CF_TRUE_COLOR_ALPHA, .always_zero = 0, .reserved = 0, "
                     f".w = {size}, .h = {size}}},")
        lines.append(f"  .data_size = {len(data)},")
        lines.append(f"  .data = {name}_map,")
        lines.append("};")
        lines.append("")
for size in (64, 20):
    refs = ", ".join(f"&moon_{size}_{i}" for i in range(8))
    lines.append(f"const lv_img_dsc_t *const MOON_{size}[8] = {{{refs}}};")
out = pathlib.Path(__file__).resolve().parent.parent / "firmware" / "moon_images.c"
out.write_text("\n".join(lines) + "\n")
print(out, sum((s * s * 3) for s in (64, 20)) * 8, "byte di immagini")
