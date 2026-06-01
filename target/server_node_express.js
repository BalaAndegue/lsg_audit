/**
 * Serveur cible 3 — Node.js Express (plus réaliste)
 * Caractéristique : framework Express, routes multiples, middleware
 * Port : 5000
 *
 * Représente une application de développement plus élaborée (API REST).
 * npm install express  (requis)
 */

const express = require('express');
const crypto  = require('crypto');

const PORT = parseInt(process.argv[2]) || 5000;
const app   = express();

app.use(express.text({ limit: '1mb' }));
app.use(express.json({ limit: '1mb' }));

// Endpoint statique léger
app.get('/', (req, res) => {
    res.send('Express Development Server — OK');
});

// Endpoint API lourd (calcul cryptographique itératif)
app.post('/api/compute', (req, res) => {
    const data = req.body || 'default';
    let h = String(data);
    for (let i = 0; i < 10000; i++) {
        h = crypto.createHash('sha256').update(h + i).digest('hex');
    }
    res.json({ result: h.slice(0, 16), port: PORT });
});

// Endpoint de simulation de base de données (sleep synchrone via busy-wait)
app.get('/api/db-query', (req, res) => {
    const delay = parseInt(req.query.delay) || 50;  // ms
    const until = Date.now() + Math.min(delay, 500);  // max 500ms
    while (Date.now() < until) { /* busy wait */ }
    res.json({ rows: [], delay_ms: delay });
});

app.listen(PORT, '0.0.0.0', () => {
    console.log(`[Express] Listening on 0.0.0.0:${PORT}`);
    console.log('  -> GET  /           : statique');
    console.log('  -> POST /api/compute: calcul lourd');
    console.log('  -> GET  /api/db-query?delay=N : latence simulée');
});
