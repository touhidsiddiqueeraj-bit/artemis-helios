"""
07_field_day_evidence.py — IV.H figure: efficiency evidence on a measured monsoon day
======================================================================================
Reads Logger_Data/New_Data/master_sylhet_mppt_proof.csv (5 s, 2026-08-17, Sylhet;
fixed-step P&O vs LSTM-assisted predictive tracker on a ≈280 Wp single-diode model,
with an independent _Calc recalculation cross-checking the originals).

Panels:
  (a) irradiance trace, highest-variability 20% windows shaded
  (b) day-long energy-weighted tracking efficiency (rolling 30 min), PO vs AI
  (c) tracking efficiency stratified by instantaneous ramp rate (patterns, not colour)

Value:  Figures/fig_field_day_evidence.png
Run:    python3 07_field_day_evidence.py [--check]
"""
import argparse
import csv
import os
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', '..', 'Logger_Data', 'New_Data',
                   'master_sylhet_mppt_proof.csv')
FIG = os.path.join(HERE, '..', '..', 'Figures', 'fig_field_day_evidence.png')


def load():
    rows = list(csv.DictReader(open(SRC)))
    G = np.array([float(r['Irradiance_Wm2']) for r in rows])
    Pm = np.array([float(r['True_MPP_Power_W']) for r in rows])
    Po = np.array([float(r['PO_Tracked_Power_W']) for r in rows])
    Pa = np.array([float(r['AI_Tracked_Power_W']) for r in rows])
    return G, Pm, Po, Pa


def ew(P, M, mask):
    day = M > 1.0
    m = mask & day
    if m.sum() == 0:
        return float('nan')
    return 100.0 * P[m].sum() / M[m].sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    G, Pm, Po, Pa = load()
    n = len(G)
    dG = np.abs(np.diff(G, prepend=G[0])) * 12.0  # W/m2 per minute
    t_h = np.arange(n) * 5.0 / 3600.0             # hours from 05:30
    thr = np.quantile(dG[Pm > 1.0], 0.8)
    hi = dG >= thr

    fig, ax = plt.subplots(3, 1, figsize=(3.5, 4.2))
    fig.subplots_adjust(left=0.14, right=0.97, top=0.95, bottom=0.14, hspace=0.85)

    # (a) irradiance with high-variability windows shaded (explained in caption)
    ax[0].fill_between(t_h, 0, G, where=hi, color='#cccccc', alpha=0.8, lw=0)
    ax[0].plot(t_h, G, lw=0.7, color='#111111')
    ax[0].set_ylabel('GHI (W/m\u00b2)', fontsize=6.5)
    ax[0].set_xlabel('hours from 05:30', fontsize=6.5)
    ax[0].set_title('(a) Measured monsoon day, 5 s sampling', fontsize=6.5)
    ax[0].tick_params(labelsize=6)

    # (b) rolling energy-weighted efficiency (30 min window = 360 steps)
    win = 360
    def rolling(P, M):
        out = np.full(n, np.nan)
        for i in range(win, n):
            m = (Pm > 1.0) & (np.arange(n) >= i - win) & (np.arange(n) < i)
            out[i] = 100.0 * P[m].sum() / M[m].sum()
        return out
    rpo, rpa = rolling(Po, Pm), rolling(Pa, Pm)
    ax[1].plot(t_h, rpo, lw=0.9, color='#b03a2e', label='fixed-step P&O')
    ax[1].plot(t_h, rpa, lw=0.9, color='#1f5fa8', label='LSTM-assisted')
    ax[1].set_ylabel('rolling \u03b7_track (%)', fontsize=6.5)
    ax[1].set_xlabel('hours from 05:30', fontsize=6.5)
    ax[1].set_title('(b) Tracking efficiency, 30-min rolling window', fontsize=6.5)
    ax[1].set_ylim(40, 100)
    ax[1].legend(fontsize=5.5, loc='lower right', frameon=False)
    ax[1].tick_params(labelsize=6)

    # (c) ramp-rate stratification (patterns, not colour)
    bins = [(0, 10, '<10'), (10, 50, '10\u201350'), (50, 150, '50\u2013150'),
            (150, 1e9, '>150')]
    labels = [b[2] for b in bins]
    po = [ew(Po, Pm, (dG >= b[0]) & (dG < b[1])) for b in bins]
    pa = [ew(Pa, Pm, (dG >= b[0]) & (dG < b[1])) for b in bins]
    x = np.arange(len(bins))
    w = 0.36
    b1 = ax[2].bar(x - w / 2, po, w, color='#9db4c0', edgecolor='k',
                   hatch='//', label='fixed-step P&O')
    b2 = ax[2].bar(x + w / 2, pa, w, color='#e8f1e8', edgecolor='k',
                   hatch='xx', label='LSTM-assisted')
    for b in list(b1) + list(b2):
        ax[2].text(b.get_x() + b.get_width() / 2, b.get_height() + 1.5,
                   f'{b.get_height():.0f}', ha='center', fontsize=4.8)
    ax[2].set_xticks(x)
    ax[2].set_xticklabels(labels, fontsize=6)
    ax[2].set_ylabel('\u03b7_track (%)', fontsize=6.5)
    ax[2].set_xlabel('ramp rate (W/m\u00b2/min)', fontsize=6.5)
    ax[2].set_ylim(0, 105)
    ax[2].set_title('(c) Efficiency vs instantaneous ramp rate', fontsize=6.5)
    ax[2].tick_params(labelsize=6)

    fig.savefig(FIG, dpi=600, bbox_inches='tight', pad_inches=0.05)
    plt.close(fig)
    print('saved', FIG)

    if args.check:
        whole_po = ew(Po, Pm, np.ones(n, bool))
        whole_pa = ew(Pa, Pm, np.ones(n, bool))
        hpo = ew(Po, Pm, hi)
        hpa = ew(Pa, Pm, hi)
        assert 80 < whole_po < 90 and 90 < whole_pa < 97, (whole_po, whole_pa)
        assert hpo < 75 and hpa > 85, (hpo, hpa)
        assert hpa - hpo > 15, hpa - hpo
        print(f'[check] whole-day PO={whole_po:.1f}% AI={whole_pa:.1f}% | '
              f'hi-var20% PO={hpo:.1f}% AI={hpa:.1f}% | gap={hpa-hpo:.1f} pp')
        print('self-check PASS')


if __name__ == '__main__':
    main()
