#!/usr/bin/env python3
"""
Analyse statistique et génération de graphiques
pour l'article Localhost Security Gap.

Usage : python3 analyze.py results/mesures_XXXXX.csv
"""

import sys
import csv
import math
import statistics
from pathlib import Path

# Tentative d'import matplotlib (optionnel)
try:
    import matplotlib
    matplotlib.use('Agg')  # mode sans écran
    import matplotlib.pyplot as plt
    HAS_MPL = True
except ImportError:
    HAS_MPL = False
    print("[WARN] matplotlib absent — pas de graphiques PNG. "
          "Installer : pip3 install matplotlib")

def read_csv(path: str) -> list[dict]:
    rows = []
    with open(path, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows

def safe_float(v: str) -> float:
    try:
        return float(v)
    except (ValueError, TypeError):
        return float('nan')

def print_summary(rows: list[dict]):
    print("\n── Résumé statistique ──────────────────────────────────────")
    print(f"{'Port':>6}  {'T_base(ms)':>10}  {'σ(ms)':>7}  "
          f"{'T_load(ms)':>10}  {'ΔT(ms)':>8}  {'Z-score':>8}  "
          f"{'Dual-R':>7}  Confirmé")
    print("-" * 72)
    for r in rows:
        port   = r.get('port', '?')
        t_base = safe_float(r.get('t_baseline_mean_ms', 'nan'))
        sigma  = safe_float(r.get('t_baseline_sigma_ms', 'nan'))
        t_load = safe_float(r.get('t_load_mean_ms', 'nan'))
        delta  = safe_float(r.get('delta_T_ms', 'nan'))
        z      = safe_float(r.get('z_score', 'nan'))
        ratio  = safe_float(r.get('dual_ratio', 'nan'))
        conf   = r.get('dual_confirmed', 'NON')
        print(f"{port:>6}  {t_base:>10.2f}  {sigma:>7.2f}  "
              f"{t_load:>10.2f}  {delta:>8.2f}  {z:>8.2f}  "
              f"{ratio:>7.2f}  {conf}")
    print("-" * 72)

def plot_latency_comparison(rows: list[dict], outdir: Path):
    if not HAS_MPL:
        return

    ports      = [r['port'] for r in rows]
    t_baseline = [safe_float(r['t_baseline_mean_ms']) for r in rows]
    t_load     = [safe_float(r['t_load_mean_ms'])     for r in rows]
    sigma      = [safe_float(r['t_baseline_sigma_ms']) for r in rows]

    x = range(len(ports))
    fig, ax = plt.subplots(figsize=(8, 5))
    bars1 = ax.bar([i - 0.2 for i in x], t_baseline, 0.4,
                   label='$\\bar{T}_{\\mathrm{r\\acute{e}seau}}$ (baseline)',
                   color='steelblue', alpha=0.85)
    bars2 = ax.bar([i + 0.2 for i in x], t_load, 0.4,
                   label='$\\bar{T}_{\\mathrm{total}}$ (charge)',
                   color='tomato', alpha=0.85)
    ax.errorbar([i - 0.2 for i in x], t_baseline, yerr=sigma,
                fmt='none', color='navy', capsize=4, linewidth=1.5)

    ax.set_xlabel('Port cible', fontsize=12)
    ax.set_ylabel('Latence applicative (ms)', fontsize=12)
    ax.set_title('Comparaison $T_{\\mathrm{r\\acute{e}seau}}$ vs $T_{\\mathrm{total}}$ par port',
                 fontsize=13)
    ax.set_xticks(list(x))
    ax.set_xticklabels(ports)
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    out = outdir / 'fig_latency_comparison.pdf'
    plt.savefig(out, format='pdf', dpi=150)
    # Aussi en PNG pour visualisation rapide
    plt.savefig(outdir / 'fig_latency_comparison.png', dpi=150)
    print(f"  [OK] Graphique sauvegardé : {out}")
    plt.close()

def plot_z_scores(rows: list[dict], outdir: Path):
    if not HAS_MPL:
        return

    ports   = [r['port'] for r in rows]
    zscores = [safe_float(r['z_score']) for r in rows]
    colors  = ['tomato' if abs(z) > 3 else 'steelblue' for z in zscores]

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.barh(ports, zscores, color=colors, alpha=0.85)
    ax.axvline(x=3,  color='red',   linestyle='--', linewidth=1.2, label='Seuil $z=3$')
    ax.axvline(x=-3, color='red',   linestyle='--', linewidth=1.2)
    ax.axvline(x=0,  color='black', linestyle='-',  linewidth=0.8)
    ax.set_xlabel('Z-score (test de Welch)', fontsize=12)
    ax.set_ylabel('Port', fontsize=12)
    ax.set_title('Détection de dégradation par z-score', fontsize=13)
    ax.legend()
    ax.grid(axis='x', linestyle='--', alpha=0.5)
    plt.tight_layout()
    out = outdir / 'fig_zscores.pdf'
    plt.savefig(out, format='pdf', dpi=150)
    plt.savefig(outdir / 'fig_zscores.png', dpi=150)
    print(f"  [OK] Graphique sauvegardé : {out}")
    plt.close()

def compute_cohen_d(rows: list[dict]):
    """Effect size pour chaque port."""
    print("\n── Effect Size (Cohen's d) ──────────────────────────────────")
    for r in rows:
        mu1 = safe_float(r.get('t_baseline_mean_ms', 'nan'))
        mu2 = safe_float(r.get('t_load_mean_ms', 'nan'))
        s1  = safe_float(r.get('t_baseline_sigma_ms', 'nan'))
        s2  = safe_float(r.get('t_load_sigma_ms', 'nan'))
        n1  = safe_float(r.get('n_calib', '1'))
        n2  = safe_float(r.get('n_load', '1'))
        if math.isnan(s1) or math.isnan(s2) or s1 + s2 < 1e-9:
            continue
        sp = math.sqrt(((n1 - 1)*s1**2 + (n2 - 1)*s2**2) / (n1 + n2 - 2))
        d  = (mu2 - mu1) / sp if sp > 0 else float('inf')
        interp = "faible" if abs(d) < 0.5 else ("moyen" if abs(d) < 0.8 else "fort")
        print(f"  Port {r.get('port', '?'):>5} : d = {d:+.3f}  ({interp})")

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <fichier.csv>")
        sys.exit(1)

    csv_path = sys.argv[1]
    rows = [r for r in read_csv(csv_path) if r.get('port', '').isdigit()]

    if not rows:
        print("[ERREUR] Aucune ligne de données trouvée.")
        sys.exit(1)

    outdir = Path(csv_path).parent
    print_summary(rows)
    compute_cohen_d(rows)

    if HAS_MPL:
        print("\n── Génération des graphiques ─────────────────────────────────")
        plot_latency_comparison(rows, outdir)
        plot_z_scores(rows, outdir)

    print("\n[DONE] Analyse terminée. Insérez les chiffres dans l'article.")

if __name__ == '__main__':
    main()
