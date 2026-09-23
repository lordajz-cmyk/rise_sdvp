#!/usr/bin/env python3
"""
Läser 4G/5G-mottagningen från en Teltonika-router (RutOS 7, t.ex. RUT951) via
routerns HTTP-API och lämnar ut den som JSON på en egen port, så att
RControlStation kan visa den i statusrutan.

    GET http://<pi>:8310/  ->  {"ok": true, "conntype": "4G (LTE)", "operator": "Telia",
                                "rssi": -63, "rsrp": -92, "rsrq": -11, "sinr": 12, "age": 1.2}

Inställningar (användare/lösenord till routern) läses från
~/.config/router_signal.conf, se router_signal.conf.example. Filen ska ha chmod 600.

Testa mot routern utan att starta servern:
    python3 router_signal.py --test
"""

import configparser
import json
import os
import ssl
import sys
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CONF_PATH = os.path.expanduser("~/.config/router_signal.conf")
POLL_S = 5.0

# RutOS-versioner har lite olika sökvägar; den första som svarar med modemdata används.
STATUS_PATHS = ["/api/modems/status", "/api/mobile/status", "/api/modems/modem_status"]

# Nyckelnamn vi letar efter i routerns svar (olika firmware använder olika namn).
KEYS = {
    "rssi": ["rssi", "signal"],
    "rsrp": ["rsrp"],
    "rsrq": ["rsrq"],
    "sinr": ["sinr"],
    "conntype": ["conntype", "connection_type", "net_mode_str", "network_type"],
    "operator": ["operator", "provider", "oper"],
}

_lock = threading.Lock()
_latest = {"ok": False, "error": "ingen mätning än"}
_latest_time = 0.0


def load_conf():
    # RawConfigParser: lösenordet läses exakt som det står (ingen %-tolkning)
    cp = configparser.RawConfigParser()
    if not cp.read(CONF_PATH):
        sys.exit(f"Hittar inte {CONF_PATH} (se router_signal.conf.example)")
    r = cp["router"]
    return {
        "url": r.get("url", "https://192.168.1.1").rstrip("/"),
        "username": r.get("username", "admin").strip(),
        "password": r.get("password", "").strip(),
        "verify_tls": r.getboolean("verify_tls", False),
        "port": cp.getint("server", "port", fallback=8310),
    }


class Router:
    def __init__(self, conf):
        self.conf = conf
        self.token = None
        self.token_time = 0.0
        self.path = None
        self.ctx = ssl.create_default_context()
        if not conf["verify_tls"]:
            # Routern har ett självsignerat certifikat på det lokala nätet.
            self.ctx.check_hostname = False
            self.ctx.verify_mode = ssl.CERT_NONE

    def _req(self, path, data=None, auth=True):
        headers = {"Content-Type": "application/json"}
        if auth and self.token:
            headers["Authorization"] = "Bearer " + self.token
        body = json.dumps(data).encode() if data is not None else None
        req = urllib.request.Request(self.conf["url"] + path, data=body, headers=headers,
                                     method="POST" if body else "GET")
        with urllib.request.urlopen(req, timeout=5, context=self.ctx) as resp:
            return json.loads(resp.read().decode("utf-8", "replace"))

    def login(self):
        try:
            res = self._req("/api/login", {"username": self.conf["username"],
                                           "password": self.conf["password"]}, auth=False)
        except urllib.error.HTTPError as e:
            # Routern berättar oftast varför i svaret, t.ex. "Authorization failed"
            detail = e.read().decode("utf-8", "replace")[:200]
            raise RuntimeError(f"inloggning misslyckades, HTTP {e.code} "
                               f"(användare '{self.conf['username']}'): {detail}") from None
        token = (res.get("data") or {}).get("token")
        if not res.get("success") or not token:
            raise RuntimeError("inloggning misslyckades (fel användare/lösenord?)")
        self.token = token
        self.token_time = time.monotonic()

    def status_raw(self):
        # Token gäller ~5 min; logga in igen i god tid.
        if not self.token or time.monotonic() - self.token_time > 240:
            self.login()
        paths = [self.path] if self.path else STATUS_PATHS
        last_err = None
        for p in paths:
            try:
                res = self._req(p)
            except urllib.error.HTTPError as e:
                if e.code == 401:
                    self.token = None
                last_err = e
                continue
            if res.get("success") and res.get("data"):
                self.path = p
                return res["data"]
            last_err = RuntimeError(f"{p}: {res.get('errors') or 'tomt svar'}")
        self.path = None
        raise last_err or RuntimeError("inget svar från routern")


def pick(d, names):
    for n in names:
        if n in d and d[n] not in (None, "", "N/A"):
            return d[n]
    return None


def to_num(v):
    try:
        return round(float(v), 1)
    except (TypeError, ValueError):
        return None


def parse(data):
    # data är antingen en lista med modem eller ett enskilt modem.
    modem = data[0] if isinstance(data, list) and data else data
    if not isinstance(modem, dict):
        raise RuntimeError("oväntat svarsformat")
    out = {"ok": True}
    for key in ("rssi", "rsrp", "rsrq", "sinr"):
        out[key] = to_num(pick(modem, KEYS[key]))
    out["conntype"] = pick(modem, KEYS["conntype"])
    out["operator"] = pick(modem, KEYS["operator"])
    if out["rssi"] is None and out["rsrp"] is None:
        raise RuntimeError("hittade ingen signalstyrka i svaret")
    return out


def poll_loop(router):
    global _latest, _latest_time
    while True:
        try:
            result = parse(router.status_raw())
        except Exception as e:  # nätverksfel, inloggning, format – visa felet i stället
            result = {"ok": False, "error": str(e)[:120]}
            router.token = None if "inloggning" in str(e) else router.token
        with _lock:
            _latest = result
            _latest_time = time.monotonic()
        time.sleep(POLL_S)


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        with _lock:
            body = dict(_latest)
            body["age"] = round(time.monotonic() - _latest_time, 1) if _latest_time else None
        raw = json.dumps(body, ensure_ascii=False).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def log_message(self, *args):
        pass  # ingen logg per förfrågan


def main():
    conf = load_conf()
    router = Router(conf)

    if "--test" in sys.argv:
        pw = conf["password"]
        print(f"Router: {conf['url']}  användare: '{conf['username']}'  "
              f"lösenord: {len(pw)} tecken{' (ÄNDRA BYT_MIG!)' if pw == 'BYT_MIG' else ''}")
        try:
            data = router.status_raw()
        except Exception as e:
            sys.exit(f"FEL: {e}")
        print("Sökväg:", router.path)
        print("Tolkat:", json.dumps(parse(data), ensure_ascii=False))
        if "--raw" in sys.argv:
            print(json.dumps(data, indent=2, ensure_ascii=False))
        return

    threading.Thread(target=poll_loop, args=(router,), daemon=True).start()
    ThreadingHTTPServer(("0.0.0.0", conf["port"]), Handler).serve_forever()


if __name__ == "__main__":
    main()
