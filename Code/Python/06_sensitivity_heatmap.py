"""
06_sensitivity_heatmap.py — A.3 multidimensional sensitivity (α × deadband, α × cooldown)
=========================================================================================
Two heatmaps of LSTM-P&O tracking efficiency η_track on the 1-hour Markov+OU
stochastic day (seed 23, dt = 0.1 s):

  (a) α (blend weight)  ×  deadband (blend threshold): 5 × 5 grid
  (b) α (blend weight)  ×  cooldown (post-blend steps): 5 × 4 grid

All other factors held at baseline (k = 0.005, delta [0.05, 0.80], G_pred =
AR(1) ema, T_amb = 30 °C). Answers Reviewer A.3: why α = 0.35 is inside a
robust plateau and how sensitive efficiency is to deadband/cooldown.

Value:   Figures/fig13_sensitivity_heatmap.png
CSV:     Code/Python/results/sensitivity_heatmap.csv
Run:     python3 06_sensitivity_heatmap.py [--check]
"""
import argparse
import os
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _bench_import import (LSTMAssistedPaO, pv_power, stochastic_day,
                           mpp_series, compute_tracking_efficiency)

HERE = os.path.dirname(os.path.abspath(__file__))
FIG = os.path.join(HERE, '..', '..', 'Figures', 'fig13_sensitivity_heatmap.png')
CSV = os.path.join(HERE, 'results', 'sensitivity_heatmap.csv')

ALPHAS = [0.15, 0.25, 0.35, 0.45, 0.55]
DEADBANDS = [0.05, 0.10, 0.15, 0.20, 0.25]
COOLDOWNS = [0, 10, 20, 30]
SEED, T_AMB, V_INIT = 23, 30.0, 17.0


def run_lstm(ctrl, G):
    """LSTM-P&O run loop (same semantics as 04 run_controller, AR(1) forecast)."""
    n = len(G)
    P_pv = np.empty(n)
    V_ref = V_INIT
    ema = G[0]
    for i in range(n):
        I, V, _ = pv_power(V_ref, G[i], T_AMB)
        ema = 0.9 * ema + 0.1 * G[i]
        V_ref = ctrl.step(I, V, G[i], ema)
        _, _, P = pv_power(V_ref, G[i], T_AMB)
        P_pv[i] = P
    return P_pv


def eta_for(alpha, deadband, cooldown):
    ctrl = LSTMAssistedPaO(alpha=alpha, blend_threshold=deadband,
                           cooldown=cooldown)
    ctrl.reset()
    wf = stochastic_day(seed=SEED)
    P_mpp, _ = mpp_series(wf['G'])
    P_pv = run_lstm(ctrl, wf['G'])
    return 100.0 * compute_tracking_efficiency(P_pv, P_mpp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    g1 = np.empty((len(DEADBANDS), len(ALPHAS)))
    g2 = np.empty((len(COOLDOWNS), len(ALPHAS)))
    rows = []

    for j, a in enumerate(ALPHAS):
        for i, db in enumerate(DEADBANDS):
            g1[i, j] = eta_for(a, db, 20)
            rows.append(('alpha_vs_deadband', a, db, 20, g1[i, j]))
            print(f'a={a:.2f} db={db:.2f} -> {g1[i, j]:.2f}%')
        for i, cd in enumerate(COOLDOWNS):
            g2[i, j] = eta_for(a, 0.15, cd)
            rows.append(('alpha_vs_cooldown', a, 0.15, cd, g2[i, j]))
            print(f'a={a:.2f} cool={cd:3d} -> {g2[i, j]:.2f}%')

    os.makedirs(os.path.dirname(CSV), exist_ok=True)
    with open(CSV, 'w') as f:
        f.write('grid,alpha,deadband,cooldown,eta_track_pct\n')
        for r in rows:
            f.write('%s,%.2f,%.2f,%d,%.3f\n' % r)

    cmap = plt.get_cmap('viridis')
    fig, ax = plt.subplots(1, 2, figsize=(3.7, 2.0))
    fig.subplots_adjust(left=0.10, right=0.88, bottom=0.16, top=0.86,
                        wspace=0.55)
    cax = fig.add_axes((0.895, 0.16, 0.025, 0.70))
    for axx, grid, xlab, ylab, xv, yv, title in (
        (ax[0], g1, 'Blend weight α', 'Deadband', ALPHAS, DEADBANDS,
         '(a) α × deadband'),
        (ax[1], g2, 'Blend weight α', 'Cooldown (steps)', ALPHAS, COOLDOWNS,
         '(b) α × cooldown'),
    ):
        im = axx.imshow(grid, cmap=cmap, aspect='auto',
                        extent=[0, len(xv), 0, len(yv)], origin='lower')
        axx.set_xticks(np.arange(len(xv)) + 0.5)
        axx.set_xticklabels([f'{x:.2f}' if isinstance(x, float) else str(x)
                             for x in xv], fontsize=6)
        axx.set_yticks(np.arange(len(yv)) + 0.5)
        axx.set_yticklabels([f'{y:.2f}' if isinstance(y, float) else str(y)
                             for y in yv], fontsize=6)
        axx.set_xlabel(xlab, fontsize=6.5)
        axx.set_ylabel(ylab, fontsize=6.5)
        axx.set_title(title, fontsize=6.5)
        for i in range(len(yv)):
            for j in range(len(xv)):
                axx.text(j + 0.5, i + 0.5, f'{grid[i, j]:.1f}',
                         ha='center', va='center', fontsize=4.8,
                         color='white' if im.norm(grid[i, j]) > 0.6
                         else 'black')
    cb = fig.colorbar(im, cax=cax)
    cb.ax.tick_params(labelsize=5)
    cb.set_label('η_track (%)', fontsize=6)
    fig.savefig(FIG, dpi=600, bbox_inches='tight', pad_inches=0.05)
    plt.close(fig)
    print('saved', FIG)

    if args.check:
        assert np.isfinite(g1).all() and np.isfinite(g2).all()
        assert (g1 >= 60).all() and (g1 <= 100).all(), g1
        assert (g2 >= 60).all() and (g2 <= 100).all(), g2
        base = g1[DEADBANDS.index(0.15), ALPHAS.index(0.35)]
        plateau = g1[ALPHAS.index(0.25):ALPHAS.index(0.55),
                     DEADBANDS.index(0.10):DEADBANDS.index(0.20) + 1]
        assert plateau.max() - plateau.min() < 3.0, plateau
        print(f'[check] baseline α=0.35/db=0.15: {base:.2f}%  '
              f'25%≤α≤45%,10%≤db≤20% spread {plateau.max()-plateau.min():.2f} pp')
        print('self-check PASS  (45 cells computed)')


if __name__ == '__main__':
    main()
