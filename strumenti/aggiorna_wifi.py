#!/usr/bin/env python3
"""Aggiorna il firmware della scheda via Wi-Fi, senza cavo.

Uso (dalla cartella del progetto):
    .venv/bin/python strumenti/aggiorna_wifi.py              # compila e invia via Wi-Fi
    .venv/bin/python strumenti/aggiorna_wifi.py --solo-invio # invia l'ultimo firmware compilato
    .venv/bin/python strumenti/aggiorna_wifi.py --chi        # dice solo quale firmware gira sulla scheda
    .venv/bin/python strumenti/aggiorna_wifi.py --prepara    # una volta sola, col cavo UART: password

La password degli aggiornamenti sta in `.env` (OTA_PASSWORD, escluso da git) e nella NVS della scheda.
Mac e scheda devono essere sulla stessa rete Wi-Fi. La scheda si trova chiedendo "CHI?" in broadcast
(porta UDP 3233); l'invio usa il protocollo di ArduinoOTA (porta UDP 3232, poi la scheda scarica dal Mac).
Dopo il riavvio lo script controlla che giri il firmware nuovo: se il nuovo si blocca all'avvio,
la scheda torna da sola alla versione precedente (firmware "in prova" finché non regge 30 s collegato).
"""

import argparse
import hashlib
import os
import secrets
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spotify_login import find_port, load_env, save_env  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FQBN = "esp32:esp32:esp32s3:PSRAM=opi,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=default"
BUILD = os.path.join(ROOT, "build")
BIN = os.path.join(BUILD, "firmware.ino.bin")
INFO_PORT = 3233
OTA_PORT = 3232


# ---------- Scheda in rete ----------

def ask_board(ip=None, tries=3):
    """Chiede "CHI?" alla scheda (in broadcast se ip è None). Restituisce un dizionario o None."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.settimeout(1.0)
    try:
        for _ in range(tries):
            s.sendto(b"CHI?", (ip or "255.255.255.255", INFO_PORT))
            try:
                data, addr = s.recvfrom(128)
            except socket.timeout:
                continue
            parts = data.decode(errors="replace").split()
            if len(parts) == 5 and parts[0] == "controller-spotify":
                return {"ip": addr[0], "sha": parts[1], "partizione": parts[2],
                        "confermato": parts[3] == "1", "acceso_da_s": int(parts[4])}
    except OSError:
        pass
    finally:
        s.close()
    return None


def bin_sha(path):
    # SHA256 dell'ELF scritto da esptool nel .bin (--elf-sha256-offset 0xb0): la scheda ne manda i primi 9
    # caratteri (CONFIG_APP_RETRIEVE_LEN_ELF_SHA), il confronto si fa su quelli
    with open(path, "rb") as f:
        f.seek(0xB0)
        return f.read(8).hex()


# ---------- Passi ----------

def compile_firmware():
    print("Compilo il firmware (la prima volta ci vogliono alcuni minuti)...")
    r = subprocess.run(["arduino-cli", "compile", "--fqbn", FQBN, "--build-path", BUILD,
                        os.path.join(ROOT, "firmware")], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit("Compilazione non riuscita.")
    for line in r.stdout.splitlines():
        if line.startswith("Sketch uses"):
            print(line)


def send(ip, password):
    """Protocollo di ArduinoOTA (lo stesso di espota.py del core), ma il firmware parte tutto di seguito:
    espota manda 1 KB e aspetta la risposta della scheda prima del successivo (2 MB = quasi 4 minuti)."""
    size = os.path.getsize(BIN)
    with open(BIN, "rb") as f:
        data = f.read()
    md5 = hashlib.md5(data).hexdigest()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(("0.0.0.0", 0))
    server.listen(1)
    local_port = server.getsockname()[1]

    # Invito e password (risposta MD5 a un nonce: la password non passa in chiaro)
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.settimeout(3)
    reply = None
    for _ in range(5):
        udp.sendto(f"0 {local_port} {size} {md5}\n".encode(), (ip, OTA_PORT))
        try:
            reply = udp.recv(64).decode(errors="replace")
            break
        except socket.timeout:
            continue
    if reply and reply.startswith("AUTH"):
        nonce = reply.split()[1]
        cnonce = hashlib.md5(f"{BIN}{size}{md5}{ip}".encode()).hexdigest()
        result = hashlib.md5(f"{hashlib.md5(password.encode()).hexdigest()}:{nonce}:{cnonce}".encode()).hexdigest()
        udp.sendto(f"200 {cnonce} {result}\n".encode(), (ip, OTA_PORT))
        try:
            reply = udp.recv(64).decode(errors="replace")
        except socket.timeout:
            reply = None
    udp.close()
    if reply != "OK":
        print("La scheda non accetta l'aggiornamento:", reply or "nessuna risposta",
              "(password diversa da quella in .env?)" if reply and "Auth" in reply else "")
        return False

    server.settimeout(10)
    try:
        conn, _ = server.accept()
    except socket.timeout:
        print("La scheda non si è collegata al Mac per scaricare il firmware.")
        return False
    finally:
        server.close()

    print(f"Invio {size // 1024} KB a {ip}...")
    t0 = time.time()
    conn.settimeout(30)
    try:
        sent, next_pct = 0, 10
        while sent < size:
            conn.sendall(data[sent:sent + 16384])
            sent = min(size, sent + 16384)
            if sent * 100 // size >= next_pct:
                print(f"  {next_pct}%")
                next_pct += 10
        # La scheda risponde a ogni pezzo con i byte scritti, e alla fine con "OK" (o con l'errore)
        answer = b""
        deadline = time.time() + 60
        while time.time() < deadline and b"OK" not in answer:
            chunk = conn.recv(4096)
            if not chunk:
                break
            answer += chunk
    except OSError as e:
        print("Invio interrotto:", e)
        return False
    finally:
        conn.close()
    if b"OK" not in answer:
        print("La scheda non ha confermato la scrittura:", answer[-200:].decode(errors="replace"))
        return False
    print(f"Inviato in {time.time() - t0:.0f} s.")
    return True


def wait_new_firmware(ip, old_sha, new_sha):
    """Aspetta il riavvio e la conferma del firmware nuovo (fino a 2 minuti)."""
    print("Aspetto che la scheda si riavvii...")
    deadline = time.time() + 120
    seen_new = False
    while time.time() < deadline:
        time.sleep(3)
        info = ask_board(ip, tries=1) or ask_board(None, tries=1)  # dopo il riavvio l'IP può cambiare
        if not info:
            continue
        if info["sha"] == new_sha:
            if not seen_new:
                print(f"Firmware nuovo partito ({info['partizione']}), aspetto la conferma (30 s collegato)...")
                seen_new = True
            if info["confermato"]:
                print("Fatto: firmware nuovo attivo e confermato.")
                return True
        elif info["sha"] == old_sha and info["acceso_da_s"] < 100:
            sys.exit("La scheda è tornata alla versione precedente: il firmware nuovo si è bloccato all'avvio.\n"
                     "Per capire perché serve il registro dal cavo UART.")
    sys.exit("La scheda non ha risposto in 2 minuti. Se lo schermo è nero o bloccato, staccala e riattaccala\n"
             "dall'alimentazione: se il firmware nuovo non va, torna da sola alla versione precedente.")


def prepare(env, port):
    import serial  # pyserial, installato in .venv

    password = env.get("OTA_PASSWORD")
    if not password:
        password = secrets.token_urlsafe(24)
        env["OTA_PASSWORD"] = password
        save_env(env)
        print("Password degli aggiornamenti creata e salvata in .env (escluso da git).")
    print(f"La invio alla scheda su {port}...")
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.2
    s.open()
    # Prima RTS e poi DTR: con l'ordine inverso per un attimo resta alto solo RTS, che riavvia la scheda
    s.rts = False
    s.dtr = False
    line = f"OTA_SET {password}\n".encode()
    deadline = time.time() + 20
    buf = b""
    next_send = 0
    while time.time() < deadline:
        if time.time() >= next_send:
            s.write(line)
            next_send = time.time() + 3
        buf += s.read(256)
        if b"OTA_OK" in buf:
            s.close()
            print("La scheda ha salvato la password: da adesso si aggiorna anche via Wi-Fi.")
            return
        if b"OTA_ERR" in buf:
            break
    s.close()
    sys.exit("La scheda non ha confermato: il firmware con gli aggiornamenti via Wi-Fi va prima caricato col cavo.")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--prepara", action="store_true", help="col cavo: crea la password e la salva nella scheda")
    ap.add_argument("--solo-invio", action="store_true", help="non ricompilare, invia build/firmware.ino.bin")
    ap.add_argument("--chi", action="store_true", help="mostra solo quale firmware gira sulla scheda")
    ap.add_argument("--ip", help="indirizzo della scheda, se non si trova da sola")
    ap.add_argument("--porta", help="porta seriale per --prepara (di solito si trova da sola)")
    args = ap.parse_args()

    env = load_env()
    if args.prepara:
        prepare(env, args.porta or find_port())
        return

    print("Cerco la scheda in rete...")
    info = ask_board(args.ip)
    if not info:
        sys.exit("Scheda non trovata in rete. Controlla che sia accesa, collegata al Wi-Fi (orologio o lettore\n"
                 "sullo schermo) e che il Mac sia sulla stessa rete. Puoi anche passare --ip.")
    state = "confermato" if info["confermato"] else "in prova"
    print(f"Scheda a {info['ip']}: firmware {info['sha']} ({info['partizione']}, {state}), accesa da {info['acceso_da_s']} s")
    if args.chi:
        return

    password = env.get("OTA_PASSWORD")
    if not password:
        sys.exit("Manca OTA_PASSWORD in .env: collega il cavo UART e lancia lo script con --prepara.")

    if not args.solo_invio:
        compile_firmware()
    if not os.path.exists(BIN):
        sys.exit("Nessun firmware compilato in build/: lancia lo script senza --solo-invio.")
    new_sha = bin_sha(BIN)[:len(info["sha"])]
    if new_sha == info["sha"]:
        print("La scheda ha già questo firmware: niente da inviare.")
        return

    if not send(info["ip"], password):
        sys.exit("Invio non riuscito (password sbagliata, rete instabile o scheda occupata). Riprova.")
    wait_new_firmware(info["ip"], info["sha"], new_sha)


if __name__ == "__main__":
    main()
