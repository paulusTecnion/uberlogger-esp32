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


def make_get_values():
    """Mimic /ajax/getValues with a live, advancing payload."""
    now_ms = int(time.time() * 1000)
    t = time.time()
    temps = {f"T{i}": round(20 + 5 * math.sin(t / 5 + i), 2) for i in range(1, 9)}
    analog = {f"AIN{i}": round(2.5 + 2 * math.sin(t / 3 + i), 4) for i in range(1, 9)}
    digital = {f"DI{i}": (int(t) + i) % 2 == 0 for i in range(1, 7)}
    return {
        "TIMESTAMP": now_ms,
        "READINGS": {
            "TEMPERATURE": {"UNITS": "deg. C", "VALUES": temps},
            "ANALOG": {"UNITS": "Volt", "VALUES": analog},
            "DIGITAL": {"UNITS": "State", "VALUES": digital},
        },
        "LOGGER_STATE": 1,
        "SD_CARD_STATUS": 2,
        "WIFI_TEST_STATUS": 3,
        "ERRORCODE": 0,
        "FW_VERSION": "dev-local",
        "SD_CARD_FREE_SPACE": 8589934592,
    }


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

        full = self._safe_path(path)
        if full is None:
            self.send_error(403)
            return

        # Mimic the .htaccess rewrite: /ajax/foo -> ajax/foo.json
        if not os.path.isfile(full) and os.path.isfile(full + ".json"):
            full += ".json"

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
        if length:
            self.rfile.read(length)
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
