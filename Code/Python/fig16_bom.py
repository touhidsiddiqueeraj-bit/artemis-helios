"""
fig16_bom.py — regenerate the BOM breakdown figure (Fig. 16)
============================================================
Clean single-column figure from the paper's cost data:
  (a) component cost bars (BDT): ESP32-S3 380, STM32F103 120, INA219 80,
      BH1750 120, buck passives 350, PCB and housing 280, assembly 250,
      connectors 170  (total 1,750 BDT)
  (b) this work vs commercial IDCOL-compatible controller (13,500 BDT),
      log axis — 87% reduction
Patterns (not colour) per the journal mandate. 600 dpi, column width.

Value:  Figures/fig16_bom.png
Run:    python3 fig16_bom.py [--check]
"""
import argparse
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                   'Figures', 'fig16_bom.png')

COMPONENTS = [('ESP32-S3', 380), ('STM32F103', 120), ('INA219', 80),
              ('BH1750', 120), ('Buck passives', 350), ('PCB + housing', 280),
              ('Assembly', 250), ('Connectors', 170)]
TOTAL = 1750
COMMERCIAL = 13500
HATCHES = ['', '//', 'xx', '..', '\\\\', '**', 'oo', '++']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    names = [c[0] for c in COMPONENTS]
    vals = [c[1] for c in COMPONENTS]

    fig, ax = plt.subplots(1, 2, figsize=(3.5, 1.75))
    fig.subplots_adjust(left=0.13, right=0.97, top=0.88, bottom=0.22, wspace=0.5)

    # (a) component bars, sorted descending, horizontal
    order = np.argsort(vals)
    names_s = [names[i] for i in order]
    vals_s = [vals[i] for i in order]
    y = np.arange(len(names_s))
    bars = ax[0].barh(y, vals_s, height=0.62, color='#9db4c0', edgecolor='k',
                      hatch=[HATCHES[i % len(HATCHES)] for i in order])
    for b, v in zip(bars, vals_s):
        ax[0].text(v + 8, b.get_y() + b.get_height() / 2, f'{v}',
                   va='center', fontsize=5)
    ax[0].set_yticks(y)
    ax[0].set_yticklabels(names_s, fontsize=4.8)
    ax[0].set_xlim(0, 430)
    ax[0].set_xlabel('BDT', fontsize=6.5)
    ax[0].set_title('(a) Component cost', fontsize=6.5)
    ax[0].tick_params(labelsize=6)

    # (b) total vs commercial, log scale
    cats = ['This work', 'Commercial\nIDCOL-compatible']
    cvals = [TOTAL, COMMERCIAL]
    x = np.arange(2)
    b2 = ax[1].bar(x, cvals, 0.55, color=['#e8f1e8', '#fdf0f0'],
                   edgecolor='k', hatch=['xx', '//'])
    ax[1].set_yscale('log')
    ax[1].set_ylim(500, 30000)
    for b, v in zip(b2, cvals):
        ax[1].text(b.get_x() + b.get_width() / 2, v * 1.15, f'{v:,}',
                   ha='center', fontsize=5.5)
    ax[1].text(0.5, 3000, '87% lower', ha='center', fontsize=6)
    ax[1].set_xticks(x)
    ax[1].set_xticklabels(cats, fontsize=5.2)
    ax[1].set_ylabel('BDT (log)', fontsize=6.5)
    ax[1].set_title('(b) vs commercial', fontsize=6.5)
    ax[1].tick_params(labelsize=6)

    fig.savefig(OUT, dpi=600, bbox_inches='tight', pad_inches=0.05)
    plt.close(fig)
    print('saved', OUT)

    if args.check:
        assert sum(vals) == TOTAL, sum(vals)
        assert COMMERCIAL / TOTAL > 7.5  # ~87% reduction
        assert len(names) == len(set(names))
        print(f'[check] total={sum(vals)} BDT, commercial={COMMERCIAL} BDT, '
              f'reduction={100*(1-TOTAL/COMMERCIAL):.0f}%')
        print('self-check PASS')


if __name__ == '__main__':
    main()
