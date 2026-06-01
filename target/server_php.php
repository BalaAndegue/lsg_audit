<?php
/**
 * Serveur cible 4 — PHP built-in server (non durci)
 * Caractéristique : single-process (une requête à la fois par défaut)
 * Lancement : php -S 0.0.0.0:8082 target/server_php.php
 */

$method = $_SERVER['REQUEST_METHOD'];
$uri    = $_SERVER['REQUEST_URI'];

if ($method === 'POST' && strpos($uri, '/api/compute') !== false) {
    $body = file_get_contents('php://input') ?: 'default';
    $t0 = microtime(true);

    // Calcul cryptographique itératif
    $h = $body;
    for ($i = 0; $i < 7000; $i++) {
        $h = hash('sha256', $h . $i);
    }

    $elapsed_ms = round((microtime(true) - $t0) * 1000, 2);
    header('Content-Type: application/json');
    echo json_encode(['result' => substr($h, 0, 16), 'elapsed_ms' => $elapsed_ms]);

} else {
    header('Content-Type: text/plain');
    echo 'PHP Development Server -- OK';
}
