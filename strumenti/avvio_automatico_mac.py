#!/usr/bin/env python3
"""Fa partire ponte_computer.py da solo a ogni accesso al Mac (e lo riavvia se si ferma).

Uso (dalla cartella del progetto):
    python3 strumenti/avvio_automatico_mac.py            # installa o aggiorna
    python3 strumenti/avvio_automatico_mac.py --rimuovi  # toglie l'avvio automatico

Copia ponte_computer.py in ~/Library/Application Support/ControllerSpotify/ (macOS chiede un permesso apposta
ai programmi avviati da soli che leggono la cartella Documenti) e crea un LaunchAgent dell'utente in
~/Library/LaunchAgents/. Niente password da amministratore. Dopo una modifica a ponte_computer.py va rilanciato.
Registro: ~/Library/Logs/ponte_computer.log
"""

import argparse
import os
import plistlib
import shutil
import subprocess
import sys

LABEL = "com.controllerspotify.ponte-computer"
HOME = os.path.expanduser("~")
DEST_DIR = os.path.join(HOME, "Library", "Application Support", "ControllerSpotify")
PLIST = os.path.join(HOME, "Library", "LaunchAgents", LABEL + ".plist")
LOG = os.path.join(HOME, "Library", "Logs", "ponte_computer.log")
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ponte_computer.py")
PYTHON = "/usr/bin/python3"  # quello di sistema: non cambia con gli aggiornamenti di Homebrew


def launchctl(*args):
    return subprocess.run(["launchctl", *args], capture_output=True, text=True)


def remove():
    launchctl("bootout", f"gui/{os.getuid()}/{LABEL}")
    if os.path.exists(PLIST):
        os.remove(PLIST)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rimuovi", action="store_true", help="toglie l'avvio automatico")
    args = ap.parse_args()

    if args.rimuovi:
        remove()
        shutil.rmtree(DEST_DIR, ignore_errors=True)
        print("Avvio automatico tolto.")
        return

    if subprocess.run([PYTHON, "--version"], capture_output=True).returncode != 0:
        sys.exit(f"{PYTHON} non funziona: installa gli strumenti da riga di comando (xcode-select --install)")
    os.makedirs(DEST_DIR, exist_ok=True)
    script = os.path.join(DEST_DIR, "ponte_computer.py")
    shutil.copy2(SOURCE, script)
    os.makedirs(os.path.dirname(PLIST), exist_ok=True)
    os.makedirs(os.path.dirname(LOG), exist_ok=True)

    remove()  # se c'era già, si ricarica con la versione nuova
    with open(PLIST, "wb") as f:
        plistlib.dump({
            "Label": LABEL,
            "ProgramArguments": [PYTHON, "-u", script],
            "RunAtLoad": True,
            "KeepAlive": True,         # se si chiude (errore, rete assente) launchd lo riapre
            "ThrottleInterval": 30,
            # "Standard": con "Background" macOS raggruppa i risvegli e le risposte ai comandi arrivavano dopo 2 s
            "ProcessType": "Standard",
            "StandardOutPath": LOG,
            "StandardErrorPath": LOG,
        }, f)
    r = launchctl("bootstrap", f"gui/{os.getuid()}", PLIST)
    if r.returncode != 0:
        sys.exit(f"launchctl non ha caricato l'agente: {r.stderr.strip()}")
    print(f"Fatto: ponte_computer.py parte da solo a ogni accesso al Mac. Registro in {LOG}")


if __name__ == "__main__":
    main()
