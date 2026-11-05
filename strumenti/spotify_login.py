#!/usr/bin/env python3
"""Primo login Spotify (Authorization Code + PKCE) e invio del refresh token alla scheda.

Uso (dalla cartella del progetto):
    .venv/bin/python strumenti/spotify_login.py            # login + invio alla scheda
    .venv/bin/python strumenti/spotify_login.py --solo-invio  # reinvia il token già salvato in .env

Client ID e refresh token finiscono in `.env` (escluso da git) e nella NVS della scheda.
Il token non viene mai stampato.
"""

import argparse
import base64
import glob
import hashlib
import http.server
import json
import os
import re
import secrets
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV_PATH = os.path.join(ROOT, ".env")
REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = [
    "user-read-playback-state",
    "user-modify-playback-state",
    "user-read-currently-playing",
    "playlist-read-private",
    "playlist-read-collaborative",
    "user-library-read",
    "user-follow-read",
    "user-top-read",
    "user-read-recently-played",
]


# ---------- .env ----------

def load_env():
    env = {}
    if os.path.exists(ENV_PATH):
        with open(ENV_PATH) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    k, v = line.split("=", 1)
                    env[k.strip()] = v.strip()
    return env


def save_env(env):
    lines = ["# Segreti del Controller Spotify: NON mettere su git.",
             "# Il refresh token viene rinnovato dalla scheda: questa copia può diventare vecchia,",
             "# in quel caso rifai il login con strumenti/spotify_login.py."]
    lines += [f"{k}={v}" for k, v in env.items()]
    with open(ENV_PATH, "w") as f:
        f.write("\n".join(lines) + "\n")
    os.chmod(ENV_PATH, 0o600)


def ask_client_id(env):
    cid = env.get("SPOTIFY_CLIENT_ID", "")
    if re.fullmatch(r"[0-9a-f]{32}", cid):
        return cid
    print("Incolla il Client ID della tua app Spotify (developer.spotify.com/dashboard → app → Settings).")
    while True:
        cid = input("Client ID: ").strip().lower()
        if re.fullmatch(r"[0-9a-f]{32}", cid):
            env["SPOTIFY_CLIENT_ID"] = cid
            save_env(env)
            return cid
        print("Non sembra un Client ID: devono essere 32 caratteri tra 0-9 e a-f. Riprova.")


# ---------- Login PKCE ----------

def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def wait_for_code(expected_state):
    result = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            url = urllib.parse.urlparse(self.path)
            if url.path != "/callback":
                self.send_response(404)
                self.end_headers()
                return
            q = urllib.parse.parse_qs(url.query)
            if q.get("state", [""])[0] != expected_state:
                result["error"] = "state non corrispondente"
            elif "error" in q:
                result["error"] = q["error"][0]
            else:
                result["code"] = q.get("code", [""])[0]
            ok = "code" in result
            body = ("<h2>Fatto! Puoi chiudere questa pagina e tornare al Terminale.</h2>" if ok
                    else f"<h2>Login non riuscito: {result.get('error')}</h2>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(f"<html><body style='font-family:sans-serif'>{body}</body></html>".encode())

        def log_message(self, *args):
            pass

    server = http.server.HTTPServer(("127.0.0.1", 8888), Handler)
    server.timeout = 1
    deadline = time.time() + 300
    while not result and time.time() < deadline:
        server.handle_request()
    server.server_close()
    if not result:
        sys.exit("Tempo scaduto: nessuna risposta dal browser in 5 minuti.")
    if "error" in result:
        sys.exit(f"Login non riuscito: {result['error']}")
    return result["code"]


def post_form(url, data):
    req = urllib.request.Request(url, data=urllib.parse.urlencode(data).encode(),
                                 headers={"Content-Type": "application/x-www-form-urlencoded"})
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit(f"Errore da Spotify ({e.code}): {e.read().decode(errors='replace')}")


def login(client_id):
    verifier = b64url(secrets.token_bytes(64))
    challenge = b64url(hashlib.sha256(verifier.encode()).digest())
    state = secrets.token_urlsafe(16)
    auth_url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode({
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "code_challenge_method": "S256",
        "code_challenge": challenge,
        "scope": " ".join(SCOPES),
        "state": state,
    })
    print("\nSi apre il browser: fai il login a Spotify e premi \"Accetta\".")
    print("Se non si apre, copia questo indirizzo nel browser:\n" + auth_url + "\n")
    webbrowser.open(auth_url)
    code = wait_for_code(state)

    tokens = post_form("https://accounts.spotify.com/api/token", {
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": REDIRECT_URI,
        "client_id": client_id,
        "code_verifier": verifier,
    })
    if "refresh_token" not in tokens:
        sys.exit("Spotify non ha restituito un refresh token.")

    req = urllib.request.Request("https://api.spotify.com/v1/me",
                                 headers={"Authorization": "Bearer " + tokens["access_token"]})
    with urllib.request.urlopen(req, timeout=20) as r:
        me = json.load(r)
    print(f"Login riuscito come: {me.get('display_name') or me.get('id')}")
    return tokens["refresh_token"]


# ---------- Invio alla scheda ----------

def find_port():
    ports = sorted(glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.wchusbserial*") + glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("Scheda non trovata: collega il cavo alla porta \"UART\" e riprova con --solo-invio.")
    return ports[0]


def send_to_board(client_id, refresh_token, port):
    import serial  # pyserial, installato in .venv

    print(f"Invio il token alla scheda su {port}...")
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.2
    s.dtr = False  # non riavviare la scheda all'apertura della porta
    s.rts = False
    s.open()
    line = f"SPOTIFY_SET {client_id} {refresh_token}\n".encode()
    deadline = time.time() + 15
    buf = b""
    next_send = 0
    while time.time() < deadline:
        if time.time() >= next_send:
            s.write(line)
            next_send = time.time() + 3
        buf += s.read(256)
        if b"SPOTIFY_OK" in buf:
            s.close()
            print("La scheda ha salvato il token. Guarda lo schermo!")
            return
        if b"SPOTIFY_ERR" in buf:
            break
    s.close()
    sys.exit("La scheda non ha confermato. Controlla che il firmware sia aggiornato e riprova con --solo-invio.")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--solo-invio", action="store_true", help="non rifare il login, reinvia il token di .env")
    ap.add_argument("--porta", help="porta seriale della scheda (di solito si trova da sola)")
    args = ap.parse_args()

    env = load_env()
    client_id = ask_client_id(env)
    if args.solo_invio:
        refresh = env.get("SPOTIFY_REFRESH_TOKEN")
        if not refresh:
            sys.exit("Nessun refresh token in .env: lancia lo script senza --solo-invio.")
    else:
        refresh = login(client_id)
        env["SPOTIFY_REFRESH_TOKEN"] = refresh
        save_env(env)
        print("Token salvato in .env (escluso da git).")

    send_to_board(client_id, refresh, args.porta or find_port())


if __name__ == "__main__":
    main()
