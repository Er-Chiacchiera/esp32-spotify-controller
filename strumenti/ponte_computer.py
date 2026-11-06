#!/usr/bin/env python3
"""Manda alla scheda lo stato del computer (batteria, processore, memoria, disco...) via Wi-Fi e riceve i comandi
della scheda abbinata (volume, muto, microfono, tieni sveglio, schermo, stop).

Uso (dalla cartella del progetto, il computer sulla stessa rete della scheda):
    python3 strumenti/ponte_computer.py                 # ogni 5 secondi, finché non lo fermi (ctrl+C)
    python3 strumenti/ponte_computer.py --una           # un solo invio, stampa cosa manda
    python3 strumenti/ponte_computer.py --abbina 482913 # abbina la scheda con il codice che mostra
    python3 strumenti/ponte_computer.py --abbina 482913 --ip 192.168.1.50  # se il router blocca i broadcast

Ogni 5 secondi manda un pacchetto UDP in broadcast sulla porta 3234: la scheda lo ascolta, nessun altro
lo usa. Alcuni router (quello della fibra di Valerio, 6/11/2026) lasciano passare i broadcast solo ogni tanto:
il pacchetto va quindi anche direttamente alla scheda, di cui il programma ricorda l'indirizzo, e la scheda che
non lo sente per 12 secondi lo richiama ("PB1"). Solo comandi di sistema del Mac, niente librerie da installare
e niente password.

Comandi (design/controllo_mac/SPEC_CONTROLLO_MAC.md): la scheda li manda a questo programma firmati con una
chiave che si scambiano all'abbinamento (salvata in ~/Library/Application Support/ControllerSpotify/). Senza
abbinamento i comandi si ignorano.
"""

import argparse
import hashlib
import hmac
import json
import os
import re
import secrets
import select
import shutil
import socket
import subprocess
import time

PORT = 3234
MAC_PORT = 3235          # qui la scheda manda comandi e richiami
EVERY_S = 5
AUDIO_EVERY_S = 1        # volume e microfono: un cambio fatto sul Mac arriva alla scheda entro un secondo
HEADPHONES_EVERY_S = 60  # system_profiler è lento: le cuffie si controllano una volta al minuto
VERSION = 2              # dalla 2 il programma riceve comandi
DATA_DIR = os.path.join(os.path.expanduser("~"), "Library", "Application Support", "ControllerSpotify")
KEY_FILE = os.path.join(DATA_DIR, "chiave_scheda.json")
STATE_FILE = os.path.join(DATA_DIR, "stato_ponte.json")


def run(*cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def computer_name():
    return run("scutil", "--get", "ComputerName").strip() or socket.gethostname()


def computer_id():
    """8 caratteri che non cambiano: dall'identificativo hardware del Mac"""
    m = re.search(r'"IOPlatformUUID" = "([^"]+)"', run("ioreg", "-rd1", "-c", "IOPlatformExpertDevice"))
    return hashlib.sha256((m.group(1) if m else socket.gethostname()).encode()).hexdigest()[:8]


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


# ---------- Audio ----------

def osascript(script):
    return run("osascript", "-e", script).strip()


def audio():
    """(volume 0-100 o -1, audio muto, volume del microfono 0-100 o -1)"""
    out = osascript("get volume settings")
    vol = re.search(r"output volume:(\d+)", out)
    mic = re.search(r"input volume:(\d+)", out)
    return (int(vol.group(1)) if vol else -1, "output muted:true" in out, int(mic.group(1)) if mic else -1)


def audio_devices():
    """(nome dell'uscita, nome del microfono) in uso"""
    out, inp, name = "", "", ""
    for line in run("system_profiler", "SPAudioDataType").splitlines():
        dev = re.match(r"^ {8}(\S.*):$", line)
        if dev:
            name = dev.group(1)
        elif "Default Output Device: Yes" in line:
            out = name
        elif "Default Input Device: Yes" in line:
            inp = name
    return out, inp


def load_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_json(path, data):
    os.makedirs(DATA_DIR, exist_ok=True)
    tmp = path + ".tmp"
    with open(os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as f:
        json.dump(data, f)
    os.replace(tmp, path)


# ---------- Firme ----------

def sign(key, text):
    """Primi 16 byte di HMAC-SHA256, in esadecimale (come sign() in firmware/computer.cpp)"""
    return hmac.new(key, text.encode(), hashlib.sha256).hexdigest()[:32]


def key_id(key):
    return hashlib.sha256(key).hexdigest()[:8]


class Bridge:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        try:
            self.sock.bind(("", MAC_PORT))  # la scheda risponde a questa porta
        except OSError:  # già occupata (un'altra copia del programma): una qualsiasi
            self.sock.bind(("", 0))
        self.board_ip = load_json(KEY_FILE).get("scheda")
        self.name = computer_name()
        self.id = computer_id()
        self.key, self.key_mtime = None, None
        self.last_cmd = (0, 0)  # (ora, numero) dell'ultimo comando accettato: niente ripetizioni
        self.awake_proc = None
        self.hp, self.hp_at = ("", -1), 0.0
        self.base = {}
        self.audio = (-1, False, -1)
        self.devices = ("", "")

    def load_key(self):
        try:
            mtime = os.path.getmtime(KEY_FILE)
        except OSError:
            self.key, self.key_mtime = None, None
            return
        if mtime != self.key_mtime:
            k = load_json(KEY_FILE).get("k", "")
            self.key = bytes.fromhex(k) if len(k) == 64 else None
            self.key_mtime = mtime

    def awake(self):
        return self.awake_proc is not None and self.awake_proc.poll() is None

    def audio_fields(self):
        vol, muted, mic = self.audio
        return {"vo": vol, "mu": int(muted), "mi": int(mic == 0), "aw": int(self.awake()),
                "on": self.devices[0], "inn": self.devices[1]}

    def refresh_all(self):
        if time.time() - self.hp_at > HEADPHONES_EVERY_S:
            self.hp, self.hp_at = headphones(), time.time()
        pct, kind, left = battery()
        disk = shutil.disk_usage("/")
        self.base = {
            "n": self.name, "b": pct, "p": kind, "r": left, "c": cpu_percent(), "m": memory_free(),
            "df": round(disk.free / 1e9, 1), "dt": round(disk.total / 1e9), "u": uptime_s(),
            "t": int(thermal_warning()), "hn": self.hp[0], "hb": self.hp[1],
        }
        self.devices = audio_devices()
        self.audio = audio()

    def send_status(self):
        self.load_key()
        data = dict(self.base, v=VERSION, id=self.id, kid=key_id(self.key) if self.key else "", **self.audio_fields())
        packet = "PC1 " + json.dumps(data, ensure_ascii=False, separators=(",", ":"))
        for dest in ["255.255.255.255"] + ([self.board_ip] if self.board_ip else []):
            try:
                self.sock.sendto(packet.encode(), (dest, PORT))
            except OSError as e:  # rete assente: si riprova al giro dopo
                print(f"Invio a {dest} non riuscito: {e}", flush=True)
        return packet

    def remember_board(self, ip):
        """Indirizzo della scheda da un suo pacchetto: da qui in poi lo stato le arriva anche diretto"""
        if ip == self.board_ip:
            return
        self.board_ip = ip
        print(f"{time.strftime('%H:%M:%S')} scheda a {ip}", flush=True)
        if os.path.exists(KEY_FILE):
            save_json(KEY_FILE, dict(load_json(KEY_FILE), scheda=ip))

    # ---------- Comandi ----------

    def run_command(self, cmd, value):
        """Esegue il comando. Restituisce l'azione da fare dopo la risposta (schermo, stop) o None"""
        if cmd == "vol":  # toccare il volume toglie anche il muto, come i tasti del Mac
            osascript(f"set volume output volume {max(0, min(100, int(value)))} without output muted")
        elif cmd == "mute":
            osascript("set volume with output muted" if value else "set volume without output muted")
        elif cmd == "mic":
            state = load_json(STATE_FILE)
            _, _, mic = audio()
            if value:
                if mic > 0:  # il livello di prima, per riaccenderlo uguale
                    save_json(STATE_FILE, dict(state, mic=mic))
                osascript("set volume input volume 0")
            else:
                osascript(f"set volume input volume {state.get('mic', 70) or 70}")
        elif cmd == "awake":
            if value and not self.awake():
                # -d schermo acceso, -i niente stop; -w: si ferma da solo se questo programma si chiude
                self.awake_proc = subprocess.Popen(["caffeinate", "-di", "-w", str(os.getpid())])
            elif not value and self.awake():
                self.awake_proc.terminate()
                self.awake_proc.wait()
        elif cmd == "screen":
            return ["pmset", "displaysleepnow"]
        elif cmd == "sleep":
            return ["pmset", "sleepnow"]
        else:
            raise ValueError(cmd)
        return None

    def handle(self, data, addr):
        received = time.time()
        text = data.decode(errors="replace")
        if text.startswith("PB1"):  # la scheda non sente lo stato: si manda subito, diretto
            self.remember_board(addr[0])
            self.send_status()
            return
        if not text.startswith("MC1 "):
            return
        self.load_key()
        body, _, sig = text[4:].rpartition(" ")
        if not self.key or not hmac.compare_digest(sign(self.key, body), sig):
            print(f"Comando con firma sbagliata da {addr[0]}: ignorato", flush=True)
            return
        try:
            msg = json.loads(body)
            stamp = (int(msg["t"]), int(msg["s"]))
        except (ValueError, KeyError, TypeError):
            return
        if abs(stamp[0] - time.time()) > 120 or stamp <= self.last_cmd:  # vecchio o ripetuto
            print(f"Comando scartato (ora {stamp[0]}, numero {stamp[1]})", flush=True)
            return
        self.last_cmd = stamp
        self.remember_board(addr[0])
        after, ok = None, 1
        try:
            after = self.run_command(msg.get("c"), msg.get("v", 0))
        except (ValueError, OSError) as e:
            print(f"Comando non riuscito: {e}", flush=True)
            ok = 0
        self.audio = audio()
        reply = json.dumps(dict(s=stamp[1], ok=ok, **self.audio_fields()), ensure_ascii=False, separators=(",", ":"))
        self.sock.sendto(f"MR1 {reply} {sign(self.key, reply)}".encode(), addr)
        print(f"{time.strftime('%H:%M:%S')} comando {msg.get('c')} {msg.get('v', '')}: "
              f"{'fatto' if ok else 'non riuscito'} in {time.time() - received:.2f} s", flush=True)
        if after:
            time.sleep(1 if after[1] == "sleepnow" else 0.3)  # prima arriva la risposta
            run(*after)

    def loop(self, once):
        print(f"Mando lo stato di \"{self.name}\" alla scheda ogni {EVERY_S} secondi (ctrl+C per fermare)",
              flush=True)
        next_full = next_audio = 0.0
        sent_audio = None
        while True:
            now = time.time()
            if now >= next_full:
                self.refresh_all()
                packet = self.send_status()
                sent_audio = self.audio_fields()
                next_full = now + EVERY_S
                next_audio = now + AUDIO_EVERY_S
                if once:
                    print(packet)
                    return
            elif now >= next_audio:
                self.audio = audio()
                if self.audio_fields() != sent_audio:  # cambiato dal Mac: la scheda lo vede subito
                    self.send_status()
                    sent_audio = self.audio_fields()
                next_audio = now + AUDIO_EVERY_S
            ready, _, _ = select.select([self.sock], [], [], max(0.05, min(next_full, next_audio) - time.time()))
            if ready:
                data, addr = self.sock.recvfrom(1024)
                self.handle(data, addr)
                sent_audio = self.audio_fields()


def pair(code, board_ip=None):
    """Abbina la scheda: manda il codice firmato e riceve la chiave (vedi handle_pair() in firmware/computer.cpp)"""
    code = re.sub(r"\D", "", code)
    if len(code) != 6:
        raise SystemExit("Il codice è di 6 cifre, come lo mostra la scheda")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(1.0)
    nonce = secrets.token_hex(8)
    body = json.dumps({"id": computer_id(), "n": nonce, "nm": computer_name()}, ensure_ascii=False,
                      separators=(",", ":"))
    packet = f"PA1 {body} {sign(code.encode(), body)}".encode()
    # Anche diretto alla scheda, se si sa dov'è: il broadcast a volte non passa
    dests = ["255.255.255.255"] + [ip for ip in (board_ip, load_json(KEY_FILE).get("scheda")) if ip]
    for _ in range(4):
        for dest in dests:
            sock.sendto(packet, (dest, PORT))
        try:
            while True:
                data, addr = sock.recvfrom(1024)
                text = data.decode(errors="replace")
                if not text.startswith("PA2 "):
                    continue
                reply_body = text[4:]
                if '"e":"codice"' in reply_body:
                    raise SystemExit("Codice sbagliato: controlla le cifre sulla scheda e riprova")
                reply, _, sig = reply_body.rpartition(" ")
                msg = json.loads(reply)
                if msg.get("n") != nonce:
                    continue
                pad = hmac.new(code.encode(), ("chiave" + nonce).encode(), hashlib.sha256).digest()
                key = bytes(a ^ b for a, b in zip(bytes.fromhex(msg["k"]), pad))
                if not hmac.compare_digest(sign(key, reply), sig):
                    raise SystemExit("Risposta della scheda non valida: riprova")
                save_json(KEY_FILE, {"k": key.hex(), "scheda": addr[0]})
                print(f"Abbinato alla scheda {addr[0]}. Il programma che gira già la usa da solo.")
                return
        except socket.timeout:
            continue
    raise SystemExit("La scheda non risponde: deve mostrare il codice ed essere sulla stessa rete Wi-Fi. "
                     "Se il router blocca i broadcast, aggiungi --ip con l'indirizzo della scheda (Impostazioni › "
                     "Informazioni)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--una", action="store_true", help="un solo invio, con la stampa dei dati")
    ap.add_argument("--abbina", metavar="CODICE", help="abbina la scheda con il codice di 6 cifre che mostra")
    ap.add_argument("--ip", help="indirizzo della scheda per --abbina, se il broadcast non passa")
    args = ap.parse_args()
    if args.abbina:
        pair(args.abbina, args.ip)
        return
    Bridge().loop(args.una)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
