#!/usr/bin/env python3
"""Manda alla scheda lo stato del computer (batteria, processore, memoria, disco...) via Wi-Fi.

Uso (dalla cartella del progetto, il computer sulla stessa rete della scheda):
    python3 strumenti/ponte_computer.py            # ogni 5 secondi, finché non lo fermi (ctrl+C)
    python3 strumenti/ponte_computer.py --una      # un solo invio, stampa cosa manda

Ogni 5 secondi manda un pacchetto UDP in broadcast sulla porta 3234: la scheda lo ascolta, nessun altro
lo usa. Solo comandi di sistema del Mac, niente librerie da installare e niente password.
"""

import argparse
import json
import os
import re
import shutil
import socket
import subprocess
import time

PORT = 3234
EVERY_S = 5
HEADPHONES_EVERY_S = 60  # system_profiler è lento: le cuffie si controllano una volta al minuto


def run(*cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def computer_name():
    return run("scutil", "--get", "ComputerName").strip() or socket.gethostname()


def battery():
    """(percentuale o -1, stato, minuti rimanenti o -1). Stato: charging, ac, battery, none"""
    out = run("pmset", "-g", "batt")
    m = re.search(r"(\d+)%;\s*([^;]+);\s*(?:(\d+):(\d+) remaining)?", out)
    if not m:
        return -1, "none", -1
    pct, state = int(m.group(1)), m.group(2).strip().lower()
    left = int(m.group(3)) * 60 + int(m.group(4)) if m.group(3) else -1
    if "discharging" in state:
        kind = "battery"
    elif state == "charging":
        kind = "charging"
    else:  # "charged", "AC attached; not charging" (ricarica ottimizzata), "finishing charge"
        kind = "ac"
    return pct, kind, left


def cpu_percent():
    # Somma del processore usato da tutti i processi, divisa per i core (media degli ultimi secondi)
    out = run("ps", "-A", "-o", "%cpu=")
    total = sum(float(x) for x in out.split() if re.match(r"^[\d.]+$", x))
    return max(0, min(100, round(total / (os.cpu_count() or 1))))


def memory_free():
    m = re.search(r"free percentage:\s*(\d+)%", run("memory_pressure"))
    return int(m.group(1)) if m else -1


def uptime_s():
    m = re.search(r"sec = (\d+)", run("sysctl", "-n", "kern.boottime"))
    return int(time.time()) - int(m.group(1)) if m else -1


def thermal_warning():
    out = run("pmset", "-g", "therm")
    m = re.search(r"CPU_Speed_Limit\s*=\s*(\d+)", out)
    return bool(m and int(m.group(1)) < 100)


def headphones():
    """(nome, batteria %) delle prime cuffie collegate che dicono la batteria, oppure ("", -1)"""
    out = run("system_profiler", "SPBluetoothDataType")
    connected = out.split("Not Connected:")[0].split("Connected:", 1)
    if len(connected) < 2:
        return "", -1
    name, levels = "", []
    for line in connected[1].splitlines():
        dev = re.match(r"^ {10}(\S.*):$", line)
        if dev:
            if name and levels:
                break
            name, levels = dev.group(1), []
            continue
        lvl = re.search(r"(?:Left |Right |)Battery Level:\s*(\d+)%", line)
        if lvl and "Case" not in line:
            levels.append(int(lvl.group(1)))
    return (name, min(levels)) if name and levels else ("", -1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--una", action="store_true", help="un solo invio, con la stampa dei dati")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    name = computer_name()
    hp, hp_at = ("", -1), 0.0
    print(f"Mando lo stato di \"{name}\" alla scheda ogni {EVERY_S} secondi (ctrl+C per fermare)")
    while True:
        if time.time() - hp_at > HEADPHONES_EVERY_S:
            hp, hp_at = headphones(), time.time()
        pct, kind, left = battery()
        disk = shutil.disk_usage("/")
        data = {
            "n": name, "b": pct, "p": kind, "r": left, "c": cpu_percent(), "m": memory_free(),
            "df": round(disk.free / 1e9, 1), "dt": round(disk.total / 1e9), "u": uptime_s(),
            "t": int(thermal_warning()), "hn": hp[0], "hb": hp[1],
        }
        packet = ("PC1 " + json.dumps(data, ensure_ascii=False, separators=(",", ":"))).encode()
        try:
            sock.sendto(packet, ("255.255.255.255", PORT))
        except OSError as e:  # rete assente: si riprova al giro dopo
            print(f"Invio non riuscito: {e}")
        if args.una:
            print(packet.decode())
            return
        time.sleep(EVERY_S)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
