#!/usr/bin/env python3
"""
Serveur cible 2 — Python http.server (non durci)
Caractéristique : single-threaded (une requête à la fois), pas de pool de threads
Port : 8080

Représente un serveur de développement typique lancé avec
`python3 -m http.server` ou un mini-serveur Flask en mode debug.
"""

import sys
import time
import hashlib
import json
from http.server import HTTPServer, BaseHTTPRequestHandler

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080


def heavy_computation(data: bytes, iterations: int = 6000) -> str:
    h = data
    for _ in range(iterations):
        h = hashlib.sha256(h).digest()
    return h.hex()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        # Silencer les logs par défaut (réduire le bruit sur stdout)
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'Python Development Server -- OK')

    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length) if length > 0 else b'default'

        t0 = time.time()
        result = heavy_computation(body)
        elapsed = (time.time() - t0) * 1000

        response = json.dumps({'result': result[:16], 'elapsed_ms': round(elapsed, 2)})
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(response)))
        self.end_headers()
        self.wfile.write(response.encode())


if __name__ == '__main__':
    server = HTTPServer(('0.0.0.0', PORT), Handler)
    print(f'[Python vulnerable] Listening on 0.0.0.0:{PORT}')
    print(f'  -> Endpoint lourd  : POST /')
    print(f'  -> Endpoint léger  : GET /')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\n[Arrêt]')
        server.server_close()
