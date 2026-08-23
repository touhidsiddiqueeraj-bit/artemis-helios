"""
07_sensitivity_sweeps.py — A.3 one-dimensional sensitivity sweeps (horizon / step / UART delay)
================================================================================================
Three single-axis sweeps of tracking efficiency eta_track on the 1-hour
Markov+OU stochastic day (seed 23, dt = 0.1 s), LSTM-P&O controller:

  (a) forecast memory: EMA window of the irradiance forecast stand-in
      (1 / 5 / 10 / 30 / 60 s)  -- the "prediction horizon" dimension
  (b) P&O step size: delta_max of the internal variable-step P&O
      (0.10 / 0.20 / 0.40 / 0.80 / 1.60 V)
  (c) control-loop latency: artificial UART-style actuation delay on V_ref
      (0 / 1 / 5 / 10 / 20 ticks @ 0.1 s)

All other factors at baseline (alpha = 0.35, deadband 0.15, cooldown 20,
k = 0.005, delta_min = 0.05, T_amb = 30 C). Answers Reviewer A.3's
remaining dimensions with a single compact figure.

Value:   Figures/fig15_sensitivity_sweeps.png
CSV:     Code/Python/results/sensitivity_sweeps.csv
Run:     python3 07_sensitivity_sweeps.py [--check]
"""
import argparse
import os
import sys
from collections import deque

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _bench_import import (LSTMAssistedPaO, pv_power, stochastic_day,
                           mpp_series, compute_tracking_efficiency)

HERE = os.path.dirname(os.path.abspath(__file__))
FIG = os.path.join(HERE, '..', '..', 'Figures', 'fig15_sensitivity_sweeps.png')
CSV = os.path.join(HERE, 'results', 'sensitivity_sweeps.csv')

SEED, T_AMB, V_INIT = 23, 30.0, 17.0
BASE_ALPHA, BASE_DEADBAND, BASE_COOLDOWN = 0.35, 0.15, 20
BASE_K, BASE_DMIN, BASE_DMAX = 0.005, 0.05, 0.80
WINDOWS_S = [1, 5, 10, 30, 60]
DELTAS = [0.10, 0.20, 0.40, 0.80, 1.60]
DELAY_TICKS = [0, 1, 5, 10, 20]
DT = 0.1


def run_lstm(ctrl, G, ema_win_s=10.0, delay_ticks=0):
    """LSTM-P&O run loop; ema_win_s controls forecast-memory smoothing,
    delay_ticks lags the actuated V_ref (UART-latency stand-in)."""
    n = len(G)
    P_pv = np.empty(n)
    V_ref = V_INIT
    w = max(1.0, ema_win_s / DT)          # window in samples
    c = 2.0 / (w + 1.0)                    # EMA coefficient
    ema = G[0]
    queue = deque()
    for i in range(n):
        I, V, _ = pv_power(V_ref, G[i], T_AMB)
        ema += c * (G[i] - ema)
        new_ref = ctrl.step(I, V, G[i], ema)
        queue.append(new_ref)
        if i >= delay_ticks:
            V_ref = queue.popleft()
        _, _, P = pv_power(V_ref, G[i], T_AMB)
        P_pv[i] = P
    return P_pv


def eta(ctrl, G, **kw):
    ctrl.reset()
    P_mpp, _ = mpp_series(G)
    return 100.0 * compute_tracking_efficiency(run_lstm(ctrl, G, **kw), P_mpp)


def build():
    G = stochastic_day(seed=SEED)['G']
    rows = []

    def base(delta_max=BASE_DMAX):
        return LSTMAssistedPaO(alpha=BASE_ALPHA, blend_threshold=BASE_DEADBAND,
                               cooldown=BASE_COOLDOWN, k=BASE_K,
                               delta_min=BASE_DMIN, delta_max=delta_max)

    for w in WINDOWS_S:
        rows.append(('horizon', w, eta(base(), G, ema_win_s=w)))
    for d in DELTAS:
        rows.append(('step', d, eta(base(delta_max=d), G)))
    for t in DELAY_TICKS:
        rows.append(('delay', t, eta(base(), G, delay_ticks=t)))

    with open(CSV, 'w') as f:
        f.write('sweep,param_value,eta_track\n')
        for sweep, val, e in rows:
            f.write(f'{sweep},{val},{e:.4f}\n')
    return rows


def plot(rows):
    fig, axes = plt.subplots(1, 3, figsize=(9.2, 2.9))
    suites = [('horizon', WINDOWS_S, 'Forecast-memory window (s)'),
              ('step', DELTAS, 'P&O step delta_max (V)'),
              ('delay', DELAY_TICKS, 'Control-loop delay (ticks @ 0.1 s)')]
    for ax, (name, xs, xlabel) in zip(axes, suites):
        ys = [e for n, v, e in rows if n == name]
        ax.plot(xs, ys, 'o-', color='#c0392b', lw=1.6, ms=4)
        ax.set_xlabel(xlabel, fontsize=8)
        ax.set_ylabel('$\\eta_{track}$ (%)', fontsize=8)
        ax.tick_params(labelsize=7)
        ax.grid(alpha=0.3, lw=0.5)
        ax.set_ylim(70, 100)
    fig.tight_layout()
    fig.savefig(FIG, dpi=300)
    print('figure written:', FIG)


def check():
    assert os.path.exists(CSV), 'CSV missing'
    rows = [l.split(',') for l in open(CSV)][1:]
    assert len(rows) == 3 * 5, f'expected 15 rows, got {len(rows)}'
    for _, _, e in rows:
        assert 50.0 <= float(e) <= 101.0, f'eta out of range: {e}'
    etas = {n: [float(e) for m, v, e in rows if m == n]
            for n in ('horizon', 'step', 'delay')}
    assert etas['delay'][0] >= max(etas['delay']) - 0.5, \
        'zero-delay should be at/near the best efficiency'
    print('check PASS: 15 rows, ranges sane, zero-delay best')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    rows = build()
    plot(rows)
    if args.check:
        check()
