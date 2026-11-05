#!/usr/bin/env python3
"""Cattura lo schermo della scheda dalla porta UART e lo salva come PNG.

Uso (dalla cartella del progetto):
    .venv/bin/python strumenti/screenshot.py design/schermate/now_playing.png

La scheda risponde al comando SCREENSHOT (firmware.ino) con il framebuffer compresso a run-length.
Ci vuole una ventina di secondi a 115200 baud.
"""

import glob
import os
import struct
import sys
import time
import zlib

import serial  # pyserial, installato in .venv


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.wchusbserial*"))
    if not ports:
        sys.exit("Scheda non trovata: collega il cavo alla porta \"UART\".")
    return ports[0]


def write_png(path, w, h, rgb):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def capture(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 1
    s.open()
    # Prima RTS e poi DTR: con l'ordine inverso per un attimo resta alto solo RTS, che riavvia la scheda
    s.rts = False
    s.dtr = False
    s.reset_input_buffer()
    s.write(b"SCREENSHOT\n")
    deadline = time.time() + 10
    header = None
    while time.time() < deadline:
        line = s.readline().decode(errors="replace").strip()
        if line.startswith("SCREENSHOT_BEGIN"):
            header = line
            break
        if line.startswith("SCREENSHOT_ERR"):
            sys.exit(line)
    if not header:
        sys.exit("La scheda non ha risposto: il firmware ha il comando SCREENSHOT?")
    _, w, h, length, checksum = header.split()
    w, h, length, checksum = int(w), int(h), int(length), int(checksum)
    data = b""
    s.timeout = 5
    while len(data) < length:
        chunk = s.read(length - len(data))
        if not chunk:
            break
        data += chunk
    s.close()
    if len(data) != length or sum(data) != checksum:
        return None
    return w, h, data


def decode(w, h, data):
    rgb = bytearray(w * h * 3)
    o = 0
    for i in range(0, len(data), 3):
        run, px = data[i], data[i + 1] | data[i + 2] << 8
        r, g, b = (px >> 11) & 0x1F, (px >> 5) & 0x3F, px & 0x1F
        pix = bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
        rgb[o:o + 3 * run] = pix * run
        o += 3 * run
    return bytes(rgb)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    port = sys.argv[2] if len(sys.argv) > 2 else find_port()
    for attempt in range(3):
        result = capture(port)
        if result:
            break
        print("Cattura mescolata ai messaggi della scheda, riprovo...")
    else:
        sys.exit("Cattura non riuscita dopo 3 tentativi.")
    w, h, data = result
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    write_png(out, w, h, decode(w, h, data))
    print(f"Salvata {out} ({w}x{h})")


if __name__ == "__main__":
    main()
