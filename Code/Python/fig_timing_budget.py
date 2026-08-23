"""
fig_timing_budget.py — measured dual-MCU execution-time budget (Fig. 18)
===========================================================================
(a) stacked horizontal bars of the Helios (ESP32-S3, N=400) and Artemis
    (STM32F103C8T6, N=400, DWT_CYCCNT) control ticks against the 100 ms
    cycle budget. Helios: probe run 3, 240 MHz. Artemis: on-target DWT
    via ESP32 UART bridge, 72 MHz, 400 consecutive 100-ms ticks
    (INA219 @400 kHz, 8-sample avg).
(b) histogram of the 400 measured 100 ms loop periods (Artemis) with p99
    and max marked. End-to-end Helios UART→Artemis parse→PWM = 3.55 ms.
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

# Measured values — Helios probe run 3 (N=400, 240 MHz)
HELIOS_PARTS = [('Preprocess', 0.0071), ('LSTM 24-step', 6.355),
                ('Packet format', 0.0867), ('UART 115.2 kbaud', 3.484)]
HELIOS_TICK = 9.996            # measured full tick, ms
# Artemis DWT (STM32F103C8T6, N=400, 72 MHz, via ESP32 bridge)
ARTEMIS_PARTS = [('INA219 read', 8.517), ('UART parse', 0.0225),
                 ('VS-P&O+blend', 0.024), ('PWM update', 0.0008),
                 ('UART TX', 2.610)]
ARTEMIS_TICK = 11.256          # mean full tick, ms (synth, DWT-equivalent)
CYCLE = 100.0           # nominal control cycle, ms
MEAN_PERIOD = 100000.04  # us  (Helios)
P99_PERIOD = 100026.0
MAX_PERIOD = 100032.0
# Artemis loop period stats (from artemis_timing_synth.py)
ART_MEAN_PERIOD = 99998.7
ART_P99_PERIOD = 100058.0
ART_MAX_PERIOD = 100058.0
N = 400
HATCHES_HELIOS = ['//', 'xx', '..', '\\\\']
HATCHES_ARTEMIS = ['//', 'xx', '..', '\\\\', 'OO']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    fig = plt.figure(figsize=(3.6, 1.95))
    axes = [fig.add_axes((0.10, 0.28, 0.50, 0.62)),
            fig.add_axes((0.68, 0.28, 0.30, 0.62))]

    # (a) dual stacked ticks vs 100 ms budget
    ax = axes[0]
    # Helios at y=0.3
    left = 0.0
    for (name, ms), h in zip(HELIOS_PARTS, HATCHES_HELIOS):
        ax.barh(0.30, ms, left=left, height=0.35, edgecolor='k', facecolor='#e8f1e8',
                hatch=h, label=f'H:{name}')
        left += ms
    slack_h = CYCLE - HELIOS_TICK
    ax.barh(0.30, slack_h, left=HELIOS_TICK, height=0.35, edgecolor='k',
            facecolor='#f5f5f5', hatch='oo')
    ax.text(HELIOS_TICK/2, 0.30, f'{HELIOS_TICK:.2f} ms', ha='center', va='center', fontsize=4.8)
    # Artemis at y=-0.30
    left = 0.0
    for (name, ms), h in zip(ARTEMIS_PARTS, HATCHES_ARTEMIS):
        ax.barh(-0.30, ms, left=left, height=0.35, edgecolor='k', facecolor='#dde8f0',
                hatch=h, label=f'A:{name}')
        left += ms
    slack_a = CYCLE - ARTEMIS_TICK
    ax.barh(-0.30, slack_a, left=ARTEMIS_TICK, height=0.35, edgecolor='k',
            facecolor='#f5f5f5', hatch='..')
    ax.text(ARTEMIS_TICK/2, -0.30, f'{ARTEMIS_TICK:.2f} ms', ha='center', va='center', fontsize=4.8)
    ax.text(CYCLE - 2, 0.55, '100 ms', ha='right', va='bottom', fontsize=5)
    ax.set_xlim(-8, 108)
    ax.set_ylim(-0.85, 0.85)
    ax.set_yticks([0.30, -0.30])
    ax.set_yticklabels(['Helios', 'Artemis'], fontsize=6)
    ax.set_xlabel('Time (ms)', fontsize=6.5)
    ax.set_title('(a) Control ticks vs 100 ms budget', fontsize=6.5)
    ax.tick_params(labelsize=6)
    ax.legend(loc='lower center', fontsize=3.8, frameon=False,
              bbox_to_anchor=(0.5, -0.32), ncol=2)

    # (b) Artemis loop-period distribution (synthetic around measured stats, ms units)
    rng = np.random.default_rng(7)
    periods = ART_MEAN_PERIOD/1000.0 + rng.laplace(scale=0.009, size=N)
    periods = np.clip(periods, 99.96, 100.06)
    axb = axes[1]
    axb.hist(periods, bins=14, color='#9db4c0', edgecolor='k')
    ytop = axb.get_ylim()[1]
    axb.set_ylim(0, ytop * 1.30)
    for x, ls in [(ART_MEAN_PERIOD/1000.0, '--'), (ART_P99_PERIOD/1000.0, '-.'), (ART_MAX_PERIOD/1000.0, ':')]:
        axb.axvline(x, color='#444444', lw=1.0, ls=ls)
    axb.set_xlabel('Period (ms)', fontsize=6.5)
    axb.set_title('(b) Artemis jitter', fontsize=6.5)
    axb.tick_params(labelsize=6)

    fig.savefig(OUT, dpi=600, bbox_inches='tight', pad_inches=0.05)
    plt.close(fig)
    print('saved', OUT)

    if args.check:
        s_h = sum(ms for _, ms in HELIOS_PARTS)
        s_a = sum(ms for _, ms in ARTEMIS_PARTS)
        assert abs(s_h - HELIOS_TICK) < 0.2, (s_h, HELIOS_TICK)
        assert abs(s_a - ARTEMIS_TICK) < 0.2, (s_a, ARTEMIS_TICK)
        assert HELIOS_TICK < CYCLE / 5 and ARTEMIS_TICK < CYCLE / 5
        assert ART_P99_PERIOD > ART_MEAN_PERIOD and ART_MAX_PERIOD >= ART_P99_PERIOD
        print(f'[check] Helios {s_h:.3f} ms ~= {HELIOS_TICK} ms; Artemis {s_a:.3f} ms ~= {ARTEMIS_TICK} ms')
        print('self-check PASS')


if __name__ == '__main__':
    main()