/**
 * Serveur cible 1 — Node.js HTTP intégré (non durci)
 * Caractéristique : single-threaded, opérations synchrones bloquantes
 * Port : 3000
 *
 * Représente un serveur de développement typique lancé avec `node server.js`
 */

const http = require('http');
const crypto = require('crypto');

const PORT = parseInt(process.argv[2]) || 3000;

// Simule un traitement applicatif réel (hachage itératif)
function heavy_computation(input, iterations = 5000) {
    let h = input;
    for (let i = 0; i < iterations; i++) {
        h = crypto.createHash('sha256').update(h + i).digest('hex');
    }
    return h;
}

const server = http.createServer((req, res) => {
    const start = Date.now();

    if (req.method === 'POST' && req.url === '/api/compute') {
        // Endpoint lourd : lit le body puis calcule
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            const result = heavy_computation(body || 'default', 8000);
            const elapsed = Date.now() - start;
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ result: result.slice(0, 16), elapsed_ms: elapsed }));
        });
    } else {
        // Endpoint léger : réponse statique
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('Node.js Development Server — OK');
    }
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[Node.js vulnerable] Listening on 0.0.0.0:${PORT}`);
    console.log('  -> Endpoint lourd  : POST /api/compute');
    console.log('  -> Endpoint léger  : GET /');
});
