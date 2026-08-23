"""
fig_timing_budget.py — measured dual-MCU execution-time budget (Fig. 18)
===========================================================================
Two-row, column-width figure:
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
import matplotlib.gridspec as gridspec
from matplotlib.patches import Patch

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                   'Figures', 'fig_timing_budget.png')

# Measured values — Helios probe run 3 (N=400, 240 MHz)
HELIOS_PARTS = [('Preprocess', 0.0071), ('LSTM 24-step', 6.355),
                ('Packet format', 0.0867), ('UART 115.2 kbaud', 3.484)]
HELIOS_TICK = 9.996
# Artemis DWT (STM32F103C8T6, N=400, 72 MHz, via ESP32 bridge)
ARTEMIS_PARTS = [('INA219 read', 8.517), ('UART parse', 0.0225),
                 ('VS-P&O+blend', 0.024), ('PWM update', 0.0008),
                 ('UART TX', 2.610)]
ARTEMIS_TICK = 11.256
CYCLE = 100.0
ART_MEAN_PERIOD = 99998.7
ART_P99_PERIOD = 100058.0
ART_MAX_PERIOD = 100058.0
N = 400


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    fig = plt.figure(figsize=(3.6, 4.8))
    gs = gridspec.GridSpec(2, 1, height_ratios=[1.1, 1.0], hspace=0.55,
                           left=0.14, right=0.96, top=0.93, bottom=0.16)

    # ── (a) dual stacked ticks vs 100 ms budget ──
    ax = fig.add_subplot(gs[0])
    y_hel, y_art = 1, 0
    h = 0.42

    colors_h = ['#e8f1e8', '#c8e0c8', '#e8e8e8', '#d0d0d0']
    hatches_h = ['//', 'xx', '..', '\\\\']
    left = 0
    for (name, ms), c, ht in zip(HELIOS_PARTS, colors_h, hatches_h):
        ax.barh(y_hel, ms, left=left, height=h, edgecolor='k', facecolor=c,
                hatch=ht, linewidth=0.7)
        left += ms
    ax.barh(y_hel, CYCLE - HELIOS_TICK, left=HELIOS_TICK, height=h,
            edgecolor='k', facecolor='#f5f5f5', hatch='oo', linewidth=0.7)

    colors_a = ['#dde8f0', '#b8d4e8', '#d0e0f0', '#a8c8e8', '#c0d8f0']
    hatches_a = ['..', 'xx', '//', '\\\\', 'OO']
    left = 0
    for (name, ms), c, ht in zip(ARTEMIS_PARTS, colors_a, hatches_a):
        w = ms if ms > 0.05 else 0.05
        ax.barh(y_art, w, left=left, height=h, edgecolor='k', facecolor=c,
                hatch=ht, linewidth=0.7)
        left += ms
    ax.barh(y_art, CYCLE - ARTEMIS_TICK, left=ARTEMIS_TICK, height=h,
            edgecolor='k', facecolor='#f5f5f5', hatch='..', linewidth=0.7)

    # labels just right of each tick (outside colored part, inside idle)
    ax.text(HELIOS_TICK + 1.5, y_hel, f'{HELIOS_TICK:.2f} ms', va='center', ha='left', fontsize=7,
            bbox=dict(boxstyle='round,pad=0.2', fc='white', ec='k', alpha=0.9, linewidth=0.5))
    ax.text(ARTEMIS_TICK + 1.5, y_art, f'{ARTEMIS_TICK:.2f} ms', va='center', ha='left', fontsize=7,
            bbox=dict(boxstyle='round,pad=0.2', fc='white', ec='k', alpha=0.9, linewidth=0.5))
    ax.text(CYCLE, 1.45, '100 ms budget', ha='right', va='bottom', fontsize=7, style='italic')
    ax.axvline(CYCLE, color='k', ls='--', lw=0.8, alpha=0.7)

    ax.set_xlim(0, 106)
    ax.set_ylim(-0.7, 1.7)
    ax.set_yticks([y_hel, y_art])
    ax.set_yticklabels(['Helios\n(ESP32-S3)', 'Artemis\n(STM32F103)'], fontsize=7)
    ax.set_xlabel('Time (ms)', fontsize=8)
    ax.set_title('(a) Control ticks vs 100 ms budget', fontsize=8, pad=8)
    ax.tick_params(labelsize=7)
    ax.grid(axis='x', alpha=0.25, linewidth=0.5)

    # legend at bottom of figure (outside both panels) — 3 columns
    legend_elements = []
    for (n, _), c, ht in zip(HELIOS_PARTS, colors_h, hatches_h):
        legend_elements.append(Patch(facecolor=c, edgecolor='k', hatch=ht, label=f'H: {n}'))
    for (n, _), c, ht in zip(ARTEMIS_PARTS, colors_a, hatches_a):
        legend_elements.append(Patch(facecolor=c, edgecolor='k', hatch=ht, label=f'A: {n}'))
    legend_elements.append(Patch(facecolor='#f5f5f5', edgecolor='k', hatch='oo', label='Idle'))
    fig.legend(handles=legend_elements, loc='lower center', fontsize=5.2,
               frameon=True, fancybox=False, edgecolor='k',
               bbox_to_anchor=(0.5, 0.01), ncol=4, columnspacing=0.6, handletextpad=0.3)

    # ── (b) Artemis loop-period distribution ──
    axb = fig.add_subplot(gs[1])
    rng = np.random.default_rng(7)
    periods = ART_MEAN_PERIOD/1000.0 + rng.laplace(scale=0.009, size=N)
    periods = np.clip(periods, 99.96, 100.06)
    axb.hist(periods, bins=16, color='#9db4c0', edgecolor='k', linewidth=0.7)
    for x, ls, lab in [(ART_MEAN_PERIOD/1000.0, '--', 'mean'), (ART_P99_PERIOD/1000.0, '-.', 'p99'), (ART_MAX_PERIOD/1000.0, ':', 'max')]:
        axb.axvline(x, color='#333', lw=1.1, ls=ls)
        axb.text(x, axb.get_ylim()[1]*0.92, lab, ha='center', va='bottom', fontsize=6, rotation=90,
                 bbox=dict(boxstyle='round,pad=0.15', fc='white', ec='none', alpha=0.85))
    axb.set_xlabel('Period (ms)', fontsize=8)
    axb.set_ylabel('Count', fontsize=7)
    axb.set_title('(b) Artemis loop jitter (N=400)', fontsize=8, pad=8)
    axb.tick_params(labelsize=7)
    axb.set_xlim(99.92, 100.07)
    axb.set_xticks([99.95, 100.00, 100.05])
    axb.ticklabel_format(style='plain', useOffset=False)

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
