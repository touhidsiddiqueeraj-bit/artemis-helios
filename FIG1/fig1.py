import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
import os

OUT = '/home/claude'

plt.rcParams.update({
    'font.family':'serif','font.serif':['Times New Roman','DejaVu Serif'],
    'font.size':8,'axes.labelsize':8,'axes.titlesize':8.5,
    'xtick.labelsize':7,'ytick.labelsize':7,
    'legend.fontsize':7,'legend.framealpha':0.9,
    'lines.linewidth':1.0,'axes.linewidth':0.6,
    'grid.linewidth':0.35,'grid.alpha':0.35,
    'figure.dpi':300,'savefig.dpi':300,
    'savefig.pad_inches':0.3,
})

B,R,G,O,P,GR = '#1565C0','#C62828','#2E7D32','#E65100','#6A1B9A','#546E7A'

def fig1():
    fig, ax = plt.subplots(figsize=(12, 5.5))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 7.5)
    ax.axis('off')

    def box(x, y, w, h, title, sub='', fc='#E3F2FD', ec='#1565C0'):
        ax.add_patch(FancyBboxPatch((x, y), w, h,
                     boxstyle='round,pad=0.08', fc=fc, ec=ec, lw=1.0, zorder=2))
        lines = sub.split('\n') if sub else []
        n = len(lines)
        # Title offset: center if no sub, else push up
        if n == 0:
            ax.text(x+w/2, y+h/2, title, ha='center', va='center',
                    fontsize=8, fontweight='bold', color='#111', zorder=3)
        else:
            # total text block height estimate
            line_h = 0.28
            total_h = (n) * line_h
            title_y = y + h/2 + total_h/2
            ax.text(x+w/2, title_y, title, ha='center', va='center',
                    fontsize=8, fontweight='bold', color='#111', zorder=3)
            for i, ln in enumerate(lines):
                ly = title_y - 0.32 - i*line_h
                ax.text(x+w/2, ly, ln, ha='center', va='center',
                        fontsize=6.0, color='#333', zorder=3)

    def arr(x1, y1, x2, y2, lbl='', col='#333', bi=False, loff=(0, 0.13)):
        sty = '<->' if bi else '->'
        ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                    arrowprops=dict(arrowstyle=sty, color=col, lw=0.9), zorder=4)
        if lbl:
            mx, my = (x1+x2)/2 + loff[0], (y1+y2)/2 + loff[1]
            ax.text(mx, my, lbl, ha='center', fontsize=6.0, color=col, style='italic')

    # ── Power rail (top row) ─────────────────────────────────────────────
    # PV Panel
    box(0.2, 5.0, 1.8, 1.4, 'PV Panel', '50Wp Mono-Si', fc='#FFF9C4', ec='#F9A825')
    # Buck Converter
    box(2.3, 5.0, 2.0, 1.4, 'Buck Conv.', 'IRFZ44N+TC4420\n50 kHz', fc='#FCE4EC', ec=R)
    # Battery
    box(4.6, 5.0, 1.8, 1.4, 'Battery', '12V/7Ah SLA', fc='#E8F5E9', ec=G)
    # Load
    box(6.7, 5.0, 1.6, 1.4, 'Load', 'DC Output', fc='#F1F8E9', ec='#558B2F')

    # ── Bottom-left: Sensors ─────────────────────────────────────────────
    box(0.2, 0.5, 1.8, 3.8, 'Sensors',
        'TSL2591\nOV2640\nINA219\nSD Card', fc='#FFF3E0', ec=O)

    # ── Bottom-mid: ARTEMIS ──────────────────────────────────────────────
    box(2.3, 0.5, 2.0, 3.8, 'ARTEMIS',
        'STM32F103\nVS-P&O MPPT\nCC/CV\nINA219', fc='#EDE7F6', ec='#4527A0')

    # ── Right: HELIOS (tall, spans full height) ──────────────────────────
    box(8.6, 0.5, 3.0, 5.9, 'HELIOS',
        'ESP32-S3\nDual LSTM\n(irradiance + gain)\nTF.js Retrain\nSD Logging\nWeb Dashboard\nZero Cloud Dep.',
        fc='#E3F2FD', ec=B)

    # ── Arrows: power rail ───────────────────────────────────────────────
    arr(2.0, 5.7, 2.3, 5.7, 'Vin, Iin')
    arr(4.3, 5.7, 4.6, 5.7, 'Vbat, Ibat')
    arr(6.5, 5.7, 6.7, 5.7)

    # Sensors → ARTEMIS
    arr(2.0, 2.4, 2.3, 2.4, 'raw sensor data', col='#555', loff=(0, 0.15))

    # ARTEMIS → Buck (Vref, vertical)
    arr(3.3, 4.3, 3.3, 5.0, 'Vref', col=GR, loff=(0.28, 0))

    # UART bidirectional: ARTEMIS ↔ HELIOS
    ax.annotate('', xy=(8.6, 3.5), xytext=(4.3, 3.5),
                arrowprops=dict(arrowstyle='<->', color=P, lw=1.2), zorder=4)
    ax.text(6.45, 3.75, 'UART 100 ms\n(Vref_pred)', ha='center',
            fontsize=6.5, color=P, fontweight='bold', zorder=5)

    ax.set_title('Fig. 1.  Helios-Artemis dual-MCU predictive MPPT — system architecture',
                 fontsize=9.5, fontweight='bold', pad=8)

    fig.tight_layout()
    out_path = f'{OUT}/fig1_architecture_fixed.png'
    fig.savefig(out_path, bbox_inches='tight')
    plt.close()
    print(f'[OK] Saved: {out_path}')

if __name__ == '__main__':
    fig1()
