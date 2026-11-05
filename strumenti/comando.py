#!/usr/bin/env python3
"""Manda uno o più comandi alla scheda dalla porta UART e stampa il registro che segue.

Uso (dalla cartella del progetto):
    .venv/bin/python strumenti/comando.py "TAP 44 248" "attesa 2" "SWIPE 400 300 400 100"

Comandi della scheda: TAP x y, LONG x y, SWIPE x1 y1 x2 y2 (tocco simulato), SCREENSHOT.
"attesa N" aspetta N secondi (leggendo il registro). "foto file.png" salva lo schermo.
Alla fine legge ancora 3 secondi.
La porta si apre con DTR/RTS bassi per non riavviare la scheda, ma su macOS l'apertura a volte
la riavvia lo stesso (rst:0x1 POWERON): meglio fare tutta una prova in un solo comando.
"""

import glob
import os
import sys
import time

import serial  # pyserial, installato in .venv

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import screenshot  # noqa: E402  (decodifica e PNG)


def main():
    ports = sorted(glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.wchusbserial*"))
    if not ports:
        sys.exit("Scheda non trovata: collega il cavo alla porta \"UART\".")
    s = serial.Serial()
    s.port = ports[0]
    s.baudrate = 115200
    s.timeout = 0.1
    s.open()
    # Prima RTS e poi DTR: con l'ordine inverso per un attimo resta alto solo RTS, che riavvia la scheda
    s.rts = False
    s.dtr = False

    def read_for(sec):
        end = time.time() + sec
        while time.time() < end:
            data = s.read(4096)
            if data:
                for line in data.decode("utf-8", "replace").splitlines():
                    if line.strip() and not line.startswith("[Warn]"):
                        print(line)

    def photo(path):
        for _ in range(3):
            s.reset_input_buffer()
            s.write(b"SCREENSHOT\n")
            end = time.time() + 10
            header = None
            while time.time() < end:
                line = s.readline().decode(errors="replace").strip()
                if line.startswith("SCREENSHOT_BEGIN"):
                    header = line
                    break
            if not header:
                continue
            _, w, h, length, checksum = header.split()
            w, h, length, checksum = int(w), int(h), int(length), int(checksum)
            data = b""
            old = s.timeout
            s.timeout = 5
            while len(data) < length:
                chunk = s.read(length - len(data))
                if not chunk:
                    break
                data += chunk
            s.timeout = old
            if len(data) == length and sum(data) == checksum:
                os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
                screenshot.write_png(path, w, h, screenshot.decode(w, h, data))
                print(f"Salvata {path}")
                return
        print(f"Foto {path} non riuscita")

    for cmd in sys.argv[1:]:
        if cmd.startswith("attesa "):
            read_for(float(cmd.split()[1]))
        elif cmd.startswith("foto "):
            photo(cmd.split(None, 1)[1])
        else:
            s.write((cmd + "\n").encode())
            read_for(0.6)
    read_for(3)


if __name__ == "__main__":
    main()
