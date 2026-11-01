#!/usr/bin/env python3
"""Local dev server for the uberlogger web frontend.

Serves front/www/ and mocks the ESP32 REST API so the live view can be tested
on a normal machine. Notably it returns a *moving* /ajax/getValues (advancing
timestamp + varying values) so you can watch points accumulate and confirm the
history survives a page refresh (localStorage persistence).

Usage:
    python3 dev_server.py            # serves on http://localhost:8000
    python3 dev_server.py 9000       # custom port

Stdlib only - no dependencies.
"""

import json
import math
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

WWW_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "www")
# UL_WWW_DIR=<build>/www_stage serves the gzipped image contents like the device does
# (mock ajax/*.json files are not in the image and still come from front/www).
SRC_WWW_DIR = WWW_DIR
WWW_DIR = os.path.abspath(os.environ.get("UL_WWW_DIR", WWW_DIR))

CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".png": "image/png",
    ".ico": "image/x-icon",
}


def guess_type(path):
    _, ext = os.path.splitext(path)
    return CONTENT_TYPES.get(ext, "application/octet-stream")


def demo_signals(t):
    """Demo channel values at time t (seconds), shared by the live mock and the sample CSVs.

    AIN1 steps 0 / 2.5 / 5 / 7.5 V every 10 s (like the bench test), AIN2 is an NTC
    sweeping -100..+100 degC (2 min sine), AIN3 sweeps -60..+60 V (60 V range, 40 s
    triangle), AIN4..8 are small 10 V signals, DIO1 toggles every 5 s."""
    ain1 = [0.0, 2.5, 5.0, 7.5][int(t // 10) % 4] - 0.2 + 0.01 * math.sin(t * 7)
    ntc = 100.0 * math.sin(2 * math.pi * t / 120)
    phase = (t % 40) / 40
    ain3 = 60.0 * (4 * phase - 1 if phase < 0.5 else 3 - 4 * phase)
    small = [0.5 * math.sin(t / (2 + k) + k) - 0.02 for k in range(4, 9)]
    dio1 = 1 if int(t // 5) % 2 == 0 else 0
    return ain1, ntc, ain3, small, dio1


LOGGING = {"on": False}   # toggled by /ajax/loggerStart and /ajax/loggerStop
MOCK_ERROR = {"code": 0}   # dev only: POST /ajax/devSetError {"code": 8} to preview an error


def make_get_values():
    """Mimic /ajax/getValues like the real device: only enabled channels, keyed
    AIN<n> (temperature channels too) and DIO<n>, digital values as 0/1.

    AIN1 steps 0 / 2.5 / 5 / 7.5 V every 10 s, AIN2 is an NTC around 20 degC,
    AIN3..8 are small signals, DIO1 toggles every 5 s (like the bench test)."""
    now_ms = int(time.time() * 1000)
    ain1, ntc, ain3, small, dio1 = demo_signals(time.time())
    analog = {"AIN1": ain1, "AIN3": ain3}
    for i, v in enumerate(small, start=4):
        analog[f"AIN{i}"] = v
    temps = {"AIN2": round(ntc, 1)}
    digital = {f"DIO{i}": (dio1 if i == 1 else 0) for i in range(1, 7)}
    return {
        "TIMESTAMP": now_ms,
        "READINGS": {
            "TEMPERATURE": {"UNITS": "DEG C", "VALUES": temps},
            "ANALOG": {"UNITS": "Volt", "VALUES": {k: round(v, 4) for k, v in analog.items()}},
            "DIGITAL": {"UNITS": "Level", "VALUES": digital},
        },
        "LOGGER_STATE": 2 if LOGGING["on"] else 1,
        "SD_CARD_STATUS": 2,
        "WIFI_TEST_STATUS": 3,
        "ERRORCODE": MOCK_ERROR["code"],
        "FW_VERSION": "dev-local",
        "SD_CARD_FREE_SPACE": 30394096,   # KiB, like the device
        "SD_CARD_TOTAL_SPACE": 31154176,  # KiB (newer firmware)
    }


MOCK_FILES = {
    "20261101_18-17-53_log.csv": 180,   # seconds of 250 Hz data
    "20261101_17-31-44_log.csv": 60,
    "20261101_16-56-53_log.csv": 30,
}


def make_sample_csv(seconds, rate=250):
    """Uberlogger-style CSV with the demo signals (see demo_signals)."""
    lines = ["time(utc),AIN1,AIN2,AIN3,AIN4,AIN5,AIN6,AIN7,AIN8,DIO1,DIO2,DIO3,DIO4,DIO5,DIO6"]
    t0 = time.gmtime(int(time.time()) - seconds)
    for i in range(seconds * rate):
        t = i / rate
        ms = int(round(t * 1000))
        tt = time.gmtime(time.mktime(t0) + ms // 1000 - time.timezone)
        ts = time.strftime("%Y-%m-%d %H:%M:%S", tt) + f".{ms % 1000:03d}"
        ain1, ntc, ain3, small, dio1 = demo_signals(t)
        fx = lambda v, d: ("-" if v < 0 and round(abs(v), d) != 0 else "") + f"{abs(v):.{d}f}"
        lines.append(",".join([ts, fx(ain1, 6), fx(ntc, 1), fx(ain3, 6)] + [fx(v, 7) for v in small] +
                              [str(dio1), "0", "0", "0", "0", "0"]))
    return ("\r\n".join(lines) + "\r\n").encode()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):  # quieter logging
        pass

    def _send_json(self, obj, status=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _safe_path(self, url_path):
        rel = url_path.split("?", 1)[0].lstrip("/")
        if rel == "":
            rel = "index.html"
        full = os.path.normpath(os.path.join(WWW_DIR, rel))
        if not full.startswith(WWW_DIR):  # path traversal guard
            return None
        return full

    def do_GET(self):
        path = self.path.split("?", 1)[0]

        # Live, moving mock of the device values endpoint.
        if path.rstrip("/") == "/ajax/getValues":
            self._send_json(make_get_values())
            return

        # Mock SD-card file browser: list + downloads of generated sample CSVs.
        if path.startswith("/ajax/getFileList"):
            name = path[len("/ajax/getFileList"):].strip("/")
            if name in MOCK_FILES:
                body = make_sample_csv(MOCK_FILES[name])
                self.send_response(200)
                self.send_header("Content-Type", "text/csv")
                self.send_header("Content-Disposition", f'attachment; filename="{name}"')
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            root = {str(i + 1): {"NAME": n, "TYPE": "FILE", "SIZE": str(sec * 250 * 125)}
                    for i, (n, sec) in enumerate(MOCK_FILES.items())}
            if name == "":   # root: also a folder and a small file, to preview those rows
                root["0"] = {"NAME": "old_logs", "TYPE": "DIRECTORY"}
                root[str(len(root) + 1)] = {"NAME": "settings_backup.json", "TYPE": "FILE", "SIZE": "1843"}
            self._send_json({"root": root, "pagination": {"current_page": 1, "total_pages": 1,
                                                           "total_entries": len(root), "entries_per_page": 25}})
            return

        full = self._safe_path(path)
        if full is None:
            self.send_error(403)
            return

        # Mimic the .htaccess rewrite: /ajax/foo -> ajax/foo.json
        if not os.path.isfile(full) and os.path.isfile(full + ".json"):
            full += ".json"
        if path.startswith("/ajax/") and not os.path.isfile(full):
            alt = os.path.join(SRC_WWW_DIR, path.lstrip("/"))
            full = alt + ".json" if os.path.isfile(alt + ".json") else alt

        # Mimic the ESP32 serving pre-gzipped assets (jquery/plotly are *.gz only).
        if not os.path.isfile(full) and os.path.isfile(full + ".gz"):
            with open(full + ".gz", "rb") as f:
                body = f.read()
            self.send_response(200)
            self.send_header("Content-Type", guess_type(full))
            self.send_header("Content-Encoding", "gzip")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if os.path.isfile(full):
            with open(full, "rb") as f:
                body = f.read()
            self.send_response(200)
            self.send_header("Content-Type", guess_type(full))
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_error(404, f"Not found: {path}")

    def do_POST(self):
        # Accept any control endpoint (loggerStart/Stop, etc.) so the UI is happy.
        length = int(self.headers.get("Content-Length", 0) or 0)
        body = self.rfile.read(length) if length else b""
        path = self.path.split("?", 1)[0]
        if path.startswith("/delete/"):   # the device answers a delete with plain "ack"
            body = b"ack"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if path == "/ajax/devSetError":
            try:
                MOCK_ERROR["code"] = int(json.loads(body or b"{}").get("code", 0))
            except ValueError:
                MOCK_ERROR["code"] = 0
        if path == "/ajax/loggerStart":
            LOGGING["on"] = True
        elif path == "/ajax/loggerStop":
            LOGGING["on"] = False
        self._send_json({"resp": "ack"})


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    if not os.path.isdir(WWW_DIR):
        sys.exit(f"www dir not found: {WWW_DIR}")
    httpd = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print(f"uberlogger dev server -> http://localhost:{port}  (serving {WWW_DIR})")
    print("Open the live view, let points accumulate, then refresh to verify history persists.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")


if __name__ == "__main__":
    main()
