"""
Fig 8: MPPT efficiency comparison with Hossion (2024) Bangladesh field reference.
Replaces fabricated Hossain et al. field data with real Hossion 2024 PR=79%.
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'output')
os.makedirs(OUT, exist_ok=True)

plt.rcParams.update({
    'font.family':'serif','font.serif':['Times New Roman','DejaVu Serif'],
    'font.size':8,'axes.labelsize':8,'axes.titlesize':8.5,
    'xtick.labelsize':7,'ytick.labelsize':7,
    'legend.fontsize':7,'legend.framealpha':0.9,
    'lines.linewidth':1.0,'axes.linewidth':0.6,
    'grid.linewidth':0.35,'grid.alpha':0.35,
    'figure.dpi':300,'savefig.dpi':300,
    'savefig.bbox':'tight','savefig.pad_inches':0.04,
})

C2 = 7.16
B = '#1565C0'
R = '#C62828'
G = '#2E7D32'
O = '#E65100'
GR = '#546E7A'

def fig9():
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(C2, 3.0))

    methods = ['Plain P&O', 'VS-P&O\n(no LSTM)', 'Helios-Artemis\n(this work)']
    mon = [70.7, 85.2, 94.0]
    ann = [85.8, 88.1, 94.0]
    x = np.arange(3)
    w = 0.36

    b1 = a1.bar(x - w/2, mon, w, label='Monsoon July', color=B, alpha=0.85, edgecolor='white', lw=0.4)
    b2 = a1.bar(x + w/2, ann, w, label='Annual equiv.', color=G, alpha=0.85, edgecolor='white', lw=0.4)
    a1.bar_label(b1, fmt='%.1f%%', fontsize=5.8, padding=2)
    a1.bar_label(b2, fmt='%.1f%%', fontsize=5.8, padding=2)
    a1.set_xticks(x)
    a1.set_xticklabels(methods, fontsize=6.5)
    a1.set_ylabel('MPPT Efficiency (%)')
    a1.set_ylim(60, 104)
    a1.legend(fontsize=6)
    a1.grid(True, axis='y', alpha=0.35)
    a1.set_title('(a) Monsoon vs annual tracking efficiency')

    b3 = a2.bar(x, ann, color=[B, O, G], alpha=0.85, edgecolor='white', lw=0.4, width=0.5)
    a2.bar_label(b3, fmt='%.1f%%', fontsize=5.8, padding=2)
    a2.axhline(79, color=R, ls='--', lw=1.0, zorder=3)
    a2.text(2.2, 79.8, 'Hossion (2024)\nPR=79% (122.4 kW\nrooftop PV, Dhaka)',
            fontsize=6, color=R, ha='left', va='bottom',
            bbox=dict(boxstyle='round,pad=0.2', fc='white', ec=R, alpha=0.85))
    a2.text(0.5, 62, 'PR is system-level — includes\ninverter, wiring, thermal, soiling losses',
            fontsize=5.5, color=GR, ha='center', va='bottom',
            bbox=dict(boxstyle='round,pad=0.2', fc='#F5F5F5', ec=GR, alpha=0.8))
    a2.set_xticks(x)
    a2.set_xticklabels(methods, fontsize=6.5)
    a2.set_ylabel('MPPT Efficiency (%)')
    a2.set_ylim(60, 104)
    a2.grid(True, axis='y', alpha=0.35)
    a2.set_title('(b) Annual efficiency with field benchmark')

    fig.suptitle(
        'Fig. 8.  Simulated MPPT efficiency comparison with Bangladesh field reference.\n'
        'Hossion [8] reports system-level performance ratio (PR) of 79% for a 122.4 kW\n'
        'rooftop installation in Dhaka — PR is a different metric from MPPT tracking efficiency.',
        fontsize=8.5, fontweight='bold')
    fig.tight_layout()
    fig.savefig(f'{OUT}/fig9_validation.png')
    plt.close()
    print('[OK] Fig 9 (new Fig. 8)')

if __name__ == '__main__':
    fig9()
    print(f'Output: {OUT}/fig9_validation.png')
