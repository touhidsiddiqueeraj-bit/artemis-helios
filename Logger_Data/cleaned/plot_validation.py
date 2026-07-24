"""
plot_validation.py — Field-vs-model irradiance comparison figure
================================================================
Reads field_data_cleaned.csv and the representative-days table,
plots all 4 deployment days as a 2×2 grid with clear-sky and
stochastic model overlays. Bad values (saturated, night) are greyed out.

Output: Logger_Data/cleaned/fig_field_validation.png (300 DPI)

Usage:
    python3 Logger_Data/cleaned/plot_validation.py

Author: Hussain Touhid Siddiquee · Leading University Sylhet
"""
import csv, math, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(OUT_DIR, '..', 'esp32_storage', 'data')
REPO_ROOT = os.path.join(OUT_DIR, '..', '..')

# ── styling ────────────────────────────────────────────────────────────────
plt.rcParams.update({
    'font.family': 'serif', 'font.serif': ['Times New Roman', 'DejaVu Serif'],
    'font.size': 9, 'axes.labelsize': 9, 'axes.titlesize': 10,
    'xtick.labelsize': 8, 'ytick.labelsize': 8,
    'legend.fontsize': 7.5, 'legend.framealpha': 0.9,
    'lines.linewidth': 1.2, 'axes.linewidth': 0.6,
    'figure.dpi': 300, 'savefig.dpi': 300,
    'savefig.bbox': 'tight', 'savefig.pad_inches': 0.05,
})

CLEAR_SKY_C = '#C62828'      # red
STOCH_C     = '#E65100'       # orange
FIELD_C     = '#1565C0'       # blue
SAT_C       = '#999999'       # grey for saturated
NIGHT_C     = '#DDDDDD'       # light grey for night
THERMAL_M   = 'v'             # triangle marker for thermal events

DAYS = ['2026-07-10', '2026-07-11', '2026-07-12', '2026-07-13']
DAY_LABELS = {
    '2026-07-10': 'Jul 10 – Partly Cloudy',
    '2026-07-11': 'Jul 11 – Monsoon Afternoon',
    '2026-07-12': 'Jul 12 – Variable, Clearing',
    '2026-07-13': 'Jul 13 – Variable, Hot',
}

# ── model functions ─────────────────────────────────────────────────────────
def clear_sky_ghi(hour, peak=800, sunrise=5.30, sunset=19.10):
    aerosol = 0.93
    if hour < sunrise or hour > sunset:
        return 0.0
    angle = math.pi * (hour - sunrise) / (sunset - sunrise)
    return max(0.0, peak * math.sin(angle) * aerosol)

def load_stochastic_model():
    path = os.path.join(REPO_ROOT, 'Tables',
                        'helios_artemis_irradiance_representative_days.csv')
    hours, vals = [], []
    with open(path) as f:
        reader = csv.DictReader(f)
        for r in reader:
            v = r['Jul_DOY196'].strip()
            if not v:
                continue
            hours.append(float(r['hour_decimal']))
            vals.append(float(v))
    return np.array(hours), np.array(vals)

def load_thermal_events():
    events = []
    for day in DAYS:
        tp = os.path.join(DATA_DIR, f'{day}.therm')
        if os.path.exists(tp):
            with open(tp) as f:
                next(f)
                for line in f:
                    parts = line.strip().split(',')
                    if len(parts) >= 2:
                        events.append((day, parts[1]))
    return events

# ── load data ───────────────────────────────────────────────────────────────
def load_field():
    path = os.path.join(OUT_DIR, 'field_data_cleaned.csv')
    by_day = {}
    with open(path) as f:
        reader = csv.DictReader(f)
        for r in reader:
            d = r['day_id']
            if d not in by_day:
                by_day[d] = {'h': [], 'corr': [], 'sat': [], 'sat_h': [], 'sat_corr': [],
                             'night_h': [], 'night_corr': [], 'temp': []}
            h = float(r['hour_decimal'])
            corr = float(r['glass_corrected_irr_wm2'])
            sat_flag = int(r['saturation_flag'])
            daytime = int(r['daytime_flag'])
            temp = float(r['temp_c'])

            if daytime and not sat_flag:
                by_day[d]['h'].append(h)
                by_day[d]['corr'].append(corr)
                by_day[d]['temp'].append(temp)
            elif daytime and sat_flag:
                by_day[d]['sat_h'].append(h)
                by_day[d]['sat_corr'].append(corr)
            else:
                by_day[d]['night_h'].append(h)
                by_day[d]['night_corr'].append(corr)
    return by_day

# ── generate figure ─────────────────────────────────────────────────────────
def main():
    field = load_field()
    stoch_h, stoch_v = load_stochastic_model()
    thermal_events = load_thermal_events()

    fig, axes = plt.subplots(2, 2, figsize=(8.5, 7))
    axes = axes.flatten()

    for idx, day in enumerate(DAYS):
        ax = axes[idx]
        d = field.get(day, {'h': [], 'corr': [], 'sat_h': [], 'sat_corr': [],
                            'night_h': [], 'night_corr': [], 'temp': []})

        # Night / sub-10 W/m² (greyed out, faint)
        if d['night_h']:
            ax.scatter(d['night_h'], d['night_corr'], s=1.5, c=NIGHT_C,
                       alpha=0.3, rasterized=True, label='Night / <10 W/m²' if idx == 0 else '')

        # Saturated (grey)
        if d['sat_h']:
            ax.scatter(d['sat_h'], d['sat_corr'], s=5, c=SAT_C,
                       alpha=0.5, rasterized=True, label=f'Saturated (n={len(d["sat_h"])})' if idx == 0 else '')

        # Good field data (blue)
        if d['h']:
            ax.scatter(d['h'], d['corr'], s=3, c=FIELD_C,
                       alpha=0.5, rasterized=True, label='Field (cleaned)' if idx == 0 else '')

        # Clear-sky model (red line)
        cs_h = np.linspace(4, 20, 200)
        cs_v = np.array([clear_sky_ghi(h) for h in cs_h])
        ax.plot(cs_h, cs_v, color=CLEAR_SKY_C, linewidth=1.5,
                label='Clear-sky model')

        # Stochastic representative-day (orange dashed)
        ax.plot(stoch_h, stoch_v, color=STOCH_C, linewidth=1.2, linestyle='--',
                label='Stochastic model\n(July typical day)')

        # Thermal event markers
        for ev_day, ev_time in thermal_events:
            if ev_day == day:
                hh, mm = ev_time.split(':')[:2]
                ev_h = int(hh) + int(mm) / 60
                ax.scatter(ev_h, 780, marker=THERMAL_M, s=60, c='#000',
                           zorder=5, label='Thermal event' if idx == 0 else '')

        # Stats annotation
        good_corr = d['corr']
        n_sat = len(d['sat_h'])
        n_night = len(d['night_h'])
        if good_corr:
            mean_val = np.mean(good_corr)
            peak_val = max(good_corr)
            peak_h = d['h'][np.argmax(good_corr)]
            stats = (f'Mean: {mean_val:.0f} W/m²\n'
                     f'Peak: {peak_val:.0f} W/m² @ {peak_h:.1f}h\n'
                     f'Sat: {n_sat}  Night: {n_night}')
            ax.text(0.97, 0.97, stats, transform=ax.transAxes,
                    fontsize=7.5, va='top', ha='right',
                    bbox=dict(boxstyle='round,pad=0.3', fc='white', ec='#CCC', alpha=0.85))

        # Axes
        ax.set_xlim(4.5, 19.5)
        ax.set_ylim(-10, 810)
        ax.set_xlabel('Hour of Day')
        ax.set_ylabel('GHI (W/m²)')
        ax.set_title(DAY_LABELS[day], fontweight='bold')
        ax.grid(True, alpha=0.25, linewidth=0.3)
        ax.set_xticks(range(6, 20, 2))

    # Shared legend (top-right of last subplot or outside)
    handles, labels = axes[0].get_legend_handles_labels()
    # Remove duplicate labels
    unique = {}
    for h, l in zip(handles, labels):
        if l not in unique:
            unique[l] = h
    fig.legend(unique.values(), unique.keys(),
               loc='lower center', ncol=5, framealpha=0.95,
               fontsize=7.5, borderpad=0.4, handletextpad=0.5,
               columnspacing=1.0)
    fig.subplots_adjust(bottom=0.12, hspace=0.35, wspace=0.3)

    out_path = os.path.join(OUT_DIR, 'fig_field_validation.png')
    fig.savefig(out_path)
    print(f'Figure saved: {out_path}')
    plt.close(fig)

if __name__ == '__main__':
    main()
