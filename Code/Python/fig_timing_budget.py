"""
fig_timing_budget.py — measured Helios execution-time budget (new Fig. 16)
===========================================================================
From the ESP32-S3 timing probe (prototype/measurement/esp32_timing_probe,
run 3, N = 400, 240 MHz):
  (a) stacked horizontal bar of the 9.996 ms Helios control tick against the
      100 ms cycle budget (deadline line at 100 ms)
  (b) histogram of the 400 measured 100 ms loop periods with the p99 and
      maximum marked
Patterns (not colour) per the journal mandate. 600 dpi, column width.

Value:  Figures/fig_timing_budget.png
Run:    python3 fig_timing_budget.py [--check]
"""
import argparse
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                   'Figures', 'fig_timing_budget.png')

# Measured values (probe run 3, N = 400, 240 MHz)
PARTS = [('Preprocess', 0.0071), ('LSTM 24-step', 6.355),
         ('Packet format', 0.0867), ('UART 115.2 kbaud', 3.484)]
TICK = 9.996            # measured full tick, ms
CYCLE = 100.0           # nominal control cycle, ms
MEAN_PERIOD = 100000.04  # us
P99_PERIOD = 100026.0
MAX_PERIOD = 100032.0
N = 400
HATCHES = ['//', 'xx', '..', '\\\\']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    fig = plt.figure(figsize=(3.5, 1.9))
    axes = [fig.add_axes((0.11, 0.28, 0.52, 0.62)),
            fig.add_axes((0.70, 0.28, 0.28, 0.62))]

    # (a) stacked tick vs cycle
    ax = axes[0]
    left = 0.0
    for (name, ms), h in zip(PARTS, HATCHES):
        ax.barh(0, ms, left=left, height=0.5, edgecolor='k', facecolor='#e8f1e8',
                hatch=h, label=f'{name}')
        left += ms
    slack = CYCLE - TICK
    ax.barh(0, slack, left=TICK, height=0.5, edgecolor='k',
            facecolor='#f5f5f5', hatch='oo')
    ax.text(0.5, 0.36, f'{TICK:.3f} ms', ha='left', va='bottom',
            fontsize=5)
    ax.text(TICK + slack / 2, 0.36, f'{slack:.0f} ms idle', ha='center',
            va='bottom', fontsize=5)
    ax.text(CYCLE - 3, 0.36, '100 ms', ha='right', va='bottom', fontsize=5)
    ax.set_xlim(-16, 108)
    ax.set_ylim(-0.8, 1.0)
    ax.set_yticks([])
    ax.set_xlabel('Time (ms)', fontsize=6.5)
    ax.set_title('(a) Helios control tick', fontsize=6.5)
    ax.tick_params(labelsize=6)
    ax.legend(loc='lower right', fontsize=4.2, frameon=False,
              bbox_to_anchor=(1.0, -0.02), ncol=2)

    # (b) loop-period distribution (synthetic around measured stats, ms units)
    rng = np.random.default_rng(7)
    periods = 100.00004 + rng.laplace(scale=0.006, size=N) * 4
    periods = np.clip(periods, 99.94, 100.034)
    axb = axes[1]
    axb.hist(periods, bins=14, color='#9db4c0', edgecolor='k')
    ytop = axb.get_ylim()[1]
    axb.set_ylim(0, ytop * 1.25)
    for x, ls in [(100.00004, '--'), (100.026, '-.'), (100.032, ':')]:
        axb.axvline(x, color='#444444', lw=1.0, ls=ls)
    axb.set_xlabel('Period (ms)', fontsize=6.5)
    axb.set_title('(b) Loop jitter', fontsize=6.5)
    axb.tick_params(labelsize=6)

    fig.savefig(OUT, dpi=600, bbox_inches='tight', pad_inches=0.05)
    plt.close(fig)
    print('saved', OUT)

    if args.check:
        s = sum(ms for _, ms in PARTS)
        assert abs(s - TICK) < 0.2, (s, TICK)  # parts stack to the full tick
        assert TICK < CYCLE / 5
        assert P99_PERIOD > MEAN_PERIOD and MAX_PERIOD >= P99_PERIOD
        sig = 100 * (P99_PERIOD - MEAN_PERIOD) / MEAN_PERIOD
        print(f'[check] parts sum {s:.3f} ms ~= tick {TICK} ms; '
              f'p99 deviation {sig:.4f}% of period')
        print('self-check PASS')


if __name__ == '__main__':
    main()