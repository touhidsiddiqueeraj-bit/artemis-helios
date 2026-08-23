"""
fig13_power_stage_schematic.py — buck power-stage schematic (2a/2c)
===================================================================
Block-level schematic of the asynchronous buck power stage with signal
names and measurement points (Reviewer A, item 1). All series elements
sit ON the power rail; shunt/INA219 sense on the left; TC4420 + Rg above
the MOSFET. Single-column width (~3.5 in), 600 dpi.

Output: Figures/fig13_power_stage_schematic.png
"""
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Figures')
os.makedirs(OUT, exist_ok=True)

plt.rcParams.update({'font.family': 'serif', 'font.size': 6.5})

fig, ax = plt.subplots(figsize=(3.55, 1.95))
ax.set_xlim(0, 100); ax.set_ylim(0, 54); ax.axis('off')
fig.subplots_adjust(left=0, right=1, top=1, bottom=0)

def box(x, y, w, h, lines, fs=5.6, ec='k', fc='#f4f6f8'):
    """Box with a list of (text, fontsize, y_fraction) lines."""
    ax.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=0.8, zorder=2))
    for t, f, yf in lines:
        ax.text(x + w / 2, y + h * yf, t, ha='center', va='center',
                fontsize=f, zorder=3)

def line(p1, p2, lw=0.8):
    ax.plot([p1[0], p2[0]], [p1[1], p2[1]], 'k-', lw=lw, zorder=1)

def lbl(x, y, s, rot=0.0, fs=4.9, ha='center'):
    ax.text(x, y, s, ha=ha, va='center', fontsize=fs, rotation=rot,
            color='#111', zorder=4)

RAIL, GND = 36, 6

# ── rails ────────────────────────────────────────────────────────────────
line((12.5, RAIL), (31, RAIL))          # PV+ to M1
line((43, RAIL), (60, RAIL))            # M1 to L
line((74, RAIL), (85.5, RAIL))          # L to battery box
line((5, GND), (97, GND))               # ground rail

# ── PV source ────────────────────────────────────────────────────────────
box(6, 21, 13, 12, [('50 Wp PV', 5.6, 0.72), ('Isc0=2.91 A', 4.4, 0.40),
                    ('Voc0=21.6 V', 4.4, 0.16)])
line((12.5, 33), (12.5, RAIL))          # + terminal
line((12.5, 21), (12.5, 17))            # - terminal
lbl(14.8, 34.5, '+', fs=7)
lbl(14.8, 18.5, '\u2212', fs=7)
lbl(9.5, 40, 'V$_{pv}$', fs=5.2)

# shunt on the negative lead
ax.add_patch(Rectangle((10.5, 13), 4, 4, fc='#f4f6f8', ec='k', lw=0.8, zorder=2))
line((12.5, 13), (12.5, GND))
lbl(15.8, 10.8, 'Rsh 10m\u03a9', fs=4.2, ha='left')

# INA219 sense
line((10.5, 15), (5, 15)); line((5, 15), (5, 45))
lbl(3.6, 30, 'V$_{sh}$', fs=4.2)
box(2, 45, 17, 5.5, [('INA219 12-bit', 5.6, 0.64), ('V$_{pv}$, I$_{pv}$', 4.4, 0.26)])

# ── input decoupling caps ────────────────────────────────────────────────
line((18, RAIL), (18, GND)); ax.plot(18, 20, 'ko', ms=1.5)
line((27, RAIL), (27, GND)); ax.plot(27, 20, 'ko', ms=1.5)
lbl(22.5, 27.5, '1\u00b5F', fs=4.4); lbl(22.5, 24.5, '5m\u03a9', fs=4.2)
lbl(22.5, 15.5, '100\u00b5F', fs=4.4); lbl(22.5, 12.5, '30m\u03a9', fs=4.2)

# ── M1 on the rail ───────────────────────────────────────────────────────
box(31, 31, 12, 9, [('M1', 5.6, 0.74), ('IRFB4110', 4.4, 0.48), ('3.7m\u03a9', 4.2, 0.20)])
ax.annotate('', xy=(32.8, 38.5), xytext=(32.8, 33),
            arrowprops=dict(arrowstyle='-|>', color='k', lw=0.8))  # body diode
lbl(46.5, 39.5, 'V$_{sw}$', fs=5.2)

# gate drive: TC4420 + Rg above M1
line((37, 40), (37, 45))
ax.add_patch(Rectangle((35.9, 41.4), 2.2, 2.4, fc='w', ec='k', lw=0.7, zorder=3))
lbl(39.3, 42.6, 'R$_g$ 4.7\u03a9', fs=4.4, ha='left')
box(30, 45, 14, 5.5, [('TC4420', 5.6, 0.64), ('50 kHz \u00b7 12 V', 4.4, 0.26)])
lbl(37, 52.6, 'PWM \u2208 [0.05, 0.95]', fs=4.4)

# ── inductor on the rail ─────────────────────────────────────────────────
box(60, 32, 14, 8, [('L 100\u00b5H', 5.6, 0.68), ('DCR 30m\u03a9', 4.4, 0.28)])
lbl(67, 43, 'I$_L$ (CCM)', fs=5.0)

# ── output cap ───────────────────────────────────────────────────────────
line((80, RAIL), (80, GND)); ax.plot(80, 20, 'ko', ms=1.5)
lbl(77.5, 26.5, '470\u00b5F', fs=4.4, ha='right'); lbl(77.5, 23.5, '40m\u03a9', fs=4.2, ha='right')

# ── battery on the rail ──────────────────────────────────────────────────
box(85.5, 30.5, 13.5, 11, [('12 V / 7 Ah', 5.6, 0.76), ('SLA Shepherd', 4.4, 0.50),
                           ('R$_{int}$ 50m\u03a9', 4.2, 0.22)])
line((92.25, 30.5), (92.25, GND))
lbl(92, 45.5, 'V$_{bat}$ 12.41\u201313.61 V', fs=4.2)

# ── measurement markers ──────────────────────────────────────────────────
for (x, y) in [(12.5, RAIL), (43, RAIL), (77, RAIL), (84, RAIL)]:
    ax.scatter([x], [y], marker='o', s=7, fc='w', ec='k', lw=0.7, zorder=4)

fig.savefig(os.path.join(OUT, 'fig13_power_stage_schematic.png'), dpi=600,
            bbox_inches='tight', pad_inches=0.02)
print('Saved fig13 at', os.path.join(OUT, 'fig13_power_stage_schematic.png'))
