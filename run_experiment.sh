#!/usr/bin/env bash
# =============================================================================
# Script d'expérience complet — Localhost Security Gap
# Usage : ./run_experiment.sh <IP_machine_cible>
# =============================================================================

set -euo pipefail

TARGET_IP="${1:-}"
if [[ -z "$TARGET_IP" ]]; then
    echo "Usage: $0 <IP_cible>"
    echo "Exemple: $0 192.168.1.42"
    exit 1
fi

BINARY="./telemetry"
RESULTS_DIR="results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Expérience Localhost Security Gap   ║"
echo "║  Cible : $TARGET_IP                  "
echo "╚══════════════════════════════════════╝"
echo ""

# -- Compilation --
echo "[1/4] Compilation..."
make -s
echo "  [OK] Binaire prêt."

mkdir -p "$RESULTS_DIR"

# -- Test de connectivité --
echo ""
echo "[2/4] Test de connectivité..."
for PORT in 3000 5000 8080 8082; do
    if curl -s --connect-timeout 1 "http://${TARGET_IP}:${PORT}/" -o /dev/null; then
        echo "  [OUVERT] Port $PORT"
    else
        echo "  [FERME]  Port $PORT"
    fi
done

# -- Expérience principale : calibration + mesures --
echo ""
echo "[3/4] Mesures de télémétrie (calibration 100 pts + 60 sondes)..."
"$BINARY" "$TARGET_IP" \
    -p "3000,5000,8080,8082" \
    -n 100 \
    -m 60 \
    -o "${RESULTS_DIR}/mesures_${TIMESTAMP}.csv"

# -- Stress test --
echo ""
echo "[4/4] Stress test (concurrence progressive : 5, 10, 20, 50)..."
"$BINARY" "$TARGET_IP" \
    -p "3000,5000,8080,8082" \
    -n 20 \
    -m 20 \
    --stress \
    -o "${RESULTS_DIR}/stress_${TIMESTAMP}.csv"

echo ""
echo "══════════════════════════════════════════════════"
echo " EXPÉRIENCE TERMINÉE"
echo " Fichiers CSV dans : $RESULTS_DIR/"
echo " Utilisez Python/gnuplot pour générer les graphes"
echo "══════════════════════════════════════════════════"
echo ""
echo "Commandes rapides pour analyser :"
echo "  python3 analyze.py ${RESULTS_DIR}/mesures_${TIMESTAMP}.csv"
