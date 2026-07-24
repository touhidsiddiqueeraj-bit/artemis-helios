"""
gen_fig12_comparison.py — Field vs model comparison figure.
Left: diurnal overlay (representative field days vs synthetic mean ± 1σ)
Right: Q-Q plot of field vs synthetic GHI distribution, annotated with KS D
Output: fig_12_field_vs_model_comparison.png
"""
import os, sys, csv
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy import stats

OUT = os.path.dirname(os.path.abspath(__file__))

plt.rcParams.update({
    'font.family': 'serif', 'font.serif': ['Times New Roman', 'DejaVu Serif'],
    'font.size': 8, 'axes.labelsize': 9, 'axes.titlesize': 10,
    'xtick.labelsize': 8, 'ytick.labelsize': 8,
    'legend.fontsize': 7.5, 'legend.framealpha': 0.9,
    'lines.linewidth': 1.2, 'axes.linewidth': 0.6,
    'figure.dpi': 300, 'savefig.dpi': 300,
    'savefig.bbox': 'tight', 'savefig.pad_inches': 0.04,
})
C_FIELD = '#1565C0'
C_SYNTH = '#C62828'
C_SAT   = '#999999'
SAT_CORR_WM2 = 470.80 * 1.0737

# ── Load field data ──
rows = []
with open(os.path.join(OUT, 'field_data_cleaned.csv')) as f:
    reader = csv.DictReader(f)
    for r in reader:
        h = float(r['hour_decimal'])
        irr = float(r['glass_corrected_irr_wm2'])
        day = r['day_id']
        sat = int(r['saturation_flag'])
        daytime = int(r['daytime_flag'])
        rows.append({'hour': h, 'irr': irr, 'day': day, 'sat': sat, 'daytime': daytime})

def resample(day_rows):
    by_min = {}
    for r in day_rows:
        if not r['daytime'] or r['sat']:
            continue
        minute = int(r['hour'] * 60)
        if minute not in by_min:
            by_min[minute] = []
        by_min[minute].append(r['irr'])
    times, vals = [], []
    for m in sorted(by_min.keys()):
        v = np.mean(by_min[m])
        if v > 10:
            times.append(m / 60.0)
            vals.append(v)
    return np.array(times), np.array(vals)

field_days = {}
for day in ['2026-07-10', '2026-07-11', '2026-07-12', '2026-07-13']:
    day_rows = [r for r in rows if r['day'] == day]
    t, v = resample(day_rows)
    field_days[day] = (t, v)

# Pool all field 1-min values (non-saturated, daytime)
all_fv = np.concatenate([v for _, (_, v) in field_days.items()])

# ── Synthetic ensemble ──
sys.path.insert(0, '/home/touhid/artemis-helios/Code/Python')
from importlib import import_module
irr_mod = import_module('01_irradiance_generator')

syn_profiles = []
for doy in range(182, 182 + 31):
    seed = 137 * doy + 500
    prof = irr_mod.generate_day_profile(doy, seed)
    syn_profiles.append(prof)
syn_arr = np.array(syn_profiles)

# Synthetic daytime GHI values (daytime only, >10 W/m²)
all_syn = syn_arr[:, 360:1140].ravel()
all_syn = all_syn[all_syn > 10]

# KS test
ks_stat, ks_p = stats.ks_2samp(all_syn, all_fv)
print(f"Field: n={len(all_fv)}, mean={all_fv.mean():.1f}, median={np.median(all_fv):.1f}")
print(f"Synth: n={len(all_syn)}, mean={all_syn.mean():.1f}, median={np.median(all_syn):.1f}")
print(f"KS D = {ks_stat:.3f}, p = {ks_p:.2e}")

# ── Plot ──
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8.5, 3.8))

# Left: Diurnal overlay — two field days vs synthetic mean ± 1σ
for day, color, label in [
    ('2026-07-10', '#2196F3', 'Jul 10 (Overcast)'),
    ('2026-07-13', '#FF9800', 'Jul 13 (Variable)'),
]:
    if day in field_days:
        t, v = field_days[day]
        ax1.plot(t, v, color=color, linewidth=1.0, alpha=0.8, label=label)

# Synthetic diurnal envelope (mean ± 1σ)
syn_hour_bins = {h: [] for h in range(6, 19)}
for day_prof in syn_arr:
    for m in range(360, 1140):
        h = m // 60
        if h in syn_hour_bins and day_prof[m] > 10:
            syn_hour_bins[h].append(day_prof[m])
s_hours = sorted(syn_hour_bins.keys())
s_mean = [np.mean(syn_hour_bins[h]) for h in s_hours]
s_std  = [np.std(syn_hour_bins[h]) for h in s_hours]

ax1.fill_between(s_hours,
                 np.array(s_mean) - np.array(s_std),
                 np.array(s_mean) + np.array(s_std),
                 color=C_SYNTH, alpha=0.12)
ax1.plot(s_hours, s_mean, color=C_SYNTH, linewidth=1.5,
         label='Synthetic (31-day µ ± 1σ)')
ax1.axhspan(SAT_CORR_WM2, 800, color=C_SAT, alpha=0.1, zorder=0)
ax1.text(17.5, SAT_CORR_WM2 + 10, 'Saturation ceiling', fontsize=6.5,
         color=C_SAT, ha='right', va='bottom')
ax1.set_xlim(6, 18.5)
ax1.set_ylim(-10, 810)
ax1.set_xlabel('Hour of Day')
ax1.set_ylabel('GHI (W/m²)')
ax1.set_title('Diurnal Overlay', fontweight='bold')
ax1.legend(fontsize=7, loc='upper left')
ax1.grid(True, alpha=0.2, linewidth=0.3)
ax1.set_xticks(range(6, 19, 2))

# Right: Q-Q plot (quantile-quantile)
min_len = min(len(all_syn), len(all_fv))
quantiles = np.linspace(0, 1, 200)
q_syn = np.quantile(all_syn, quantiles)
q_field = np.quantile(all_fv, quantiles)

ax2.plot(q_syn, q_field, '.', color=C_FIELD, markersize=3, alpha=0.6)
ax2.plot([0, 600], [0, 600], '--', color='#888', linewidth=0.8, label='1:1 (identical dist.)')
# Polynomial fit to show bias
from numpy.polynomial import polynomial as P
mask = q_syn < 500  # avoid saturation tail
coeffs = P.polyfit(q_syn[mask], q_field[mask], 1)
x_line = np.linspace(0, 500, 100)
y_line = P.polyval(x_line, coeffs)
ax2.plot(x_line, y_line, '-', color=C_SYNTH, linewidth=1.2,
         label=f'Trend: y = {coeffs[1]:.2f}x + {coeffs[0]:.1f}')

ax2.text(380, 50, f'KS D = {ks_stat:.3f}\np = {ks_p:.1e}', fontsize=9, fontweight='bold',
         bbox=dict(facecolor='white', edgecolor='#ccc', boxstyle='round,pad=0.3'),
         verticalalignment='bottom')
ax2.set_xlim(0, 550)
ax2.set_ylim(0, 550)
ax2.set_xlabel('Synthetic GHI Quantiles (W/m²)')
ax2.set_ylabel('Field GHI Quantiles (W/m²)')
ax2.set_title('Q–Q Plot: Field vs Synthetic', fontweight='bold')
ax2.legend(fontsize=7, loc='lower right')
ax2.grid(True, alpha=0.2, linewidth=0.3)
ax2.set_aspect('equal')

fig.suptitle('Field Validation of Markov+OU Irradiance Model', fontweight='bold', y=1.02)
fig.tight_layout()
out_path = os.path.join(OUT, 'fig_12_field_vs_model_comparison.png')
fig.savefig(out_path)
plt.close(fig)
print(f"Saved {out_path}")
