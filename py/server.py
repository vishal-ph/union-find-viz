#!/usr/bin/env python3
"""Lightweight HTTP server that serves the web UI and exposes /api/generate."""

import json
import sys
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

# Ensure py/ is on the path so we can import main
sys.path.insert(0, str(Path(__file__).resolve().parent))
from main import generate_noisy_circuit, evaluate_dem, compute_detection_events

PROJECT_ROOT = Path(__file__).resolve().parent.parent
WEB_DIR = PROJECT_ROOT / "web"
DATA_DIR = PROJECT_ROOT / "data"


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_DIR), **kwargs)

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    # ------------------------------------------------------------------
    # GET: serve /data/<file> from the project-level data/ directory,
    #      everything else from web/
    # ------------------------------------------------------------------
    def do_GET(self):
        if self.path.startswith("/data/"):
            rel = self.path[len("/data/"):]
            file_path = DATA_DIR / rel
            if not file_path.is_file():
                self._json_error(404, f"File not found: {rel}")
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.end_headers()
            self.wfile.write(file_path.read_bytes())
            return
        super().do_GET()

    # ------------------------------------------------------------------
    # POST /api/generate
    # ------------------------------------------------------------------
    def do_POST(self):
        if self.path != "/api/generate":
            self._json_error(404, "Not found")
            return

        try:
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length))
        except (json.JSONDecodeError, ValueError) as e:
            self._json_error(400, f"Invalid JSON: {e}")
            return

        # Extract and validate parameters
        try:
            distance = int(body["distance"])
            rounds = int(body["rounds"])
            p = float(body["p"])
        except (KeyError, ValueError, TypeError) as e:
            self._json_error(400, f"Missing or invalid parameter: {e}")
            return

        if not (3 <= distance <= 50):
            self._json_error(400, "distance must be between 3 and 50")
            return
        if not (1 <= rounds <= 100):
            self._json_error(400, "rounds must be between 1 and 100")
            return
        if not (0 < p < 1):
            self._json_error(400, "p must be between 0 and 1 (exclusive)")
            return

        # Run stim
        try:
            circuit = generate_noisy_circuit(distance, rounds, p)
            dem = evaluate_dem(circuit)
            events = compute_detection_events(circuit)
        except Exception as e:
            self._json_error(500, f"Simulation error: {e}")
            return

        result = {
            "dem": str(dem),
            "events": "\n".join(str(e) for e in events),
        }
        self._json_response(200, result)

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _json_response(self, code, obj):
        payload = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _json_error(self, code, message):
        self._json_response(code, {"error": message})

    def log_message(self, format, *args):
        # Quieter logging: just method + path
        sys.stderr.write(f"  {args[0]}\n")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    server = HTTPServer(("", port), Handler)
    print(f"Serving on http://localhost:{port}")
    print(f"  Web root: {WEB_DIR}")
    print(f"  Data dir: {DATA_DIR}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.server_close()


if __name__ == "__main__":
    main()
