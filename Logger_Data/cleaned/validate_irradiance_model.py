"""
validate_irradiance_model.py — Tier 1: Empirical validation of the Markov+OU
                                 synthetic irradiance model against field data.
================================================================================
Compares statistical/distributional properties of the synthetic generator
(01_irradiance_generator.py) against Helios logger field measurements.

Outputs:
  - fig_validation_diurnal.png    — Diurnal envelope (hourly mean ± 1σ)
  - fig_validation_ramprates.png  — Ramp rate distribution (field vs synthetic)
  - fig_validation_autocorr.png   — Autocorrelation decay
  - fig_validation_cdf.png        — Cumulative distribution comparison

This is PATTERN-LEVEL, not magnitude-level validation. The sensor saturation
ceiling (~470 W/m² raw, ~505 W/m² corrected) means peak magnitudes are censored.

Usage:
    python3 Logger_Data/cleaned/validate_irradiance_model.py

Author: Hussain Touhid Siddiquee · Leading University Sylhet
"""
import csv, math, os, sys
import numpy as np

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# ── paths ────────────────────────────────────────────────────────────────────
OUT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.join(OUT_DIR, '..', '..')

sys.path.insert(0, os.path.join(REPO_ROOT, 'Code', 'Python'))
from importlib import import_module
irr_mod = import_module('01_irradiance_generator')

# ── styling ──────────────────────────────────────────────────────────────────
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
C_CSKY  = '#333333'
C_SAT   = '#999999'

# ── constants ────────────────────────────────────────────────────────────────
SAT_RAW_WM2   = 470.80
SAT_CORR_WM2  = SAT_RAW_WM2 * 1.0737
GLASS_CORR    = 1.0737
DAYTIME_MIN   = 10.0
FIELD_DT      = 10       # seconds between field samples
SYNTH_DT      = 60       # synthetic model runs at 1-min

# ── load field data ──────────────────────────────────────────────────────────
def load_field():
    path = os.path.join(OUT_DIR, 'field_data_cleaned.csv')
    rows = []
    with open(path) as f:
        reader = csv.DictReader(f)
        for r in reader:
            h = float(r['hour_decimal'])
            irr = float(r['glass_corrected_irr_wm2'])
            day = r['day_id']
            rows.append({
                'hour': h, 'irr': irr,
                'sat': int(r['saturation_flag']),
                'daytime': int(r['daytime_flag']),
                'day': day,
                'elapsed': int(r['elapsed_s']),
            })
    return rows

# ── generate synthetic ensemble ──────────────────────────────────────────────
def generate_synthetic_july(n_days=31, seed_offset=0):
    """Generate a multi-day synthetic July ensemble at 1-minute resolution."""
    profiles = []
    for doy in range(182, 182 + n_days):  # DOY 182 = Jul 1
        seed = 137 * doy + 500 + seed_offset
        prof = irr_mod.generate_day_profile(doy, seed)
        profiles.append(prof)
    return np.array(profiles)  # (n_days, 1440)

# ── resample field to 1-min ──────────────────────────────────────────────────
def resample_field_1min(field_rows):
    """Average 10s field readings to 1-minute bins."""
    by_bin = {}
    for r in field_rows:
        if not r['daytime'] or r['sat']:
            continue
        minute = int(r['hour'] * 60)
        if minute not in by_bin:
            by_bin[minute] = []
        by_bin[minute].append(r['irr'])
    times, vals = [], []
    for m in sorted(by_bin.keys()):
        v = np.mean(by_bin[m])
        if v > DAYTIME_MIN:
            times.append(m / 60.0)
            vals.append(v)
    return np.array(times), np.array(vals)

# ── ramp rates ───────────────────────────────────────────────────────────────
def compute_ramp_rates(vals, dt_min):
    dt = dt_min / 60.0  # convert to hours for W/m²/h
    return np.diff(vals) / dt

# ── autocorrelation ──────────────────────────────────────────────────────────
def autocorrelation(vals, max_lag):
    n = len(vals)
    if n <= max_lag + 1:
        return np.array([])
    mean_v = np.mean(vals)
    var_v = np.var(vals)
    if var_v == 0:
        return np.zeros(max_lag + 1)
    acf = np.zeros(max_lag + 1)
    for lag in range(max_lag + 1):
        acf[lag] = np.mean((vals[:n-lag] - mean_v) * (vals[lag:] - mean_v)) / var_v
    return acf

# ── main ─────────────────────────────────────────────────────────────────────
def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    # ── Load ──────────────────────────────────────────────────────────────
    print('Loading field data...')
    field_raw = load_field()

    field_resamp_hours = {}
    for day in ['2026-07-10', '2026-07-11', '2026-07-12', '2026-07-13']:
        day_rows = [r for r in field_raw if r['day'] == day]
        t, v = resample_field_1min(day_rows)
        field_resamp_hours[day] = (t, v)

    # Pool all field 1-min data
    all_fh = np.concatenate([v for _, (_, v) in field_resamp_hours.items()])
    all_ft = np.concatenate([t for _, (t, _) in field_resamp_hours.items()])
    print(f'  Field 1-min samples: {len(all_fh)} ({len(all_fh)/60:.1f} h)')

    print('Generating synthetic ensemble...')
    syn_profiles = generate_synthetic_july(n_days=100)
    print(f'  Synthetic ensemble: {syn_profiles.shape[0]} days × 1440 min')

    # ── 1. Diurnal envelope ──────────────────────────────────────────────────
    print('\n[1/4] Diurnal envelope...')
    fig, axes = plt.subplots(1, 2, figsize=(8.5, 3.8))

    for ax, label, field_dict, syn_arr in [
        (axes[0], 'Jul 10 – Partly Cloudy', {'2026-07-10': field_resamp_hours['2026-07-10']}, syn_profiles),
        (axes[1], 'Jul 12 – Variable Clearing', {'2026-07-12': field_resamp_hours['2026-07-12']}, syn_profiles),
    ]:
        # Field: hourly means
        f_hour_bins = {}
        for t, v in zip(*field_dict[next(iter(field_dict))]):
            h = int(t)
            if h not in f_hour_bins:
                f_hour_bins[h] = []
            f_hour_bins[h].append(v)

        f_hours = sorted(f_hour_bins.keys())
        f_mean = [np.mean(f_hour_bins[h]) for h in f_hours]
        f_std  = [np.std(f_hour_bins[h]) for h in f_hours]

        # Synthetic: hourly means across ensemble
        s_hour_bins = {h: [] for h in range(5, 19)}
        for day_prof in syn_arr:
            for m in range(300, 1140):  # 05:00–19:00
                h = m // 60
                if h in s_hour_bins and day_prof[m] > DAYTIME_MIN:
                    s_hour_bins[h].append(day_prof[m])

        s_hours = sorted(s_hour_bins.keys())
        s_mean = [np.mean(s_hour_bins[h]) for h in s_hours]
        s_std  = [np.std(s_hour_bins[h]) for h in s_hours]

        # Plot
        f_h = np.array(f_hours)
        s_h = np.array(s_hours)
        ax.fill_between(s_h,
                        np.array(s_mean) - np.array(s_std),
                        np.array(s_mean) + np.array(s_std),
                        color=C_SYNTH, alpha=0.12)
        ax.plot(s_h, s_mean, color=C_SYNTH, linewidth=1.5,
                label='Synthetic (mean ± 1σ)')

        ax.fill_between(f_h,
                        np.array(f_mean) - np.array(f_std),
                        np.array(f_mean) + np.array(f_std),
                        color=C_FIELD, alpha=0.15)
        ax.plot(f_h, f_mean, color=C_FIELD, linewidth=1.5, marker='s',
                markersize=3, label='Field (mean ± 1σ)')

        # Saturation zone
        ax.axhspan(SAT_CORR_WM2, 800, color=C_SAT, alpha=0.12, zorder=0)
        ax.text(18.5, SAT_CORR_WM2 + 15, 'Sensor\nsaturation\nceiling',
                fontsize=6.5, color=C_SAT, ha='right', va='bottom',
                linespacing=1.2)

        ax.set_xlim(5.5, 18.5)
        ax.set_ylim(-10, 810)
        ax.set_xlabel('Hour of Day')
        ax.set_ylabel('GHI (W/m²)')
        ax.set_title(label, fontweight='bold')
        ax.legend(fontsize=7)
        ax.grid(True, alpha=0.2, linewidth=0.3)
        ax.set_xticks(range(6, 19, 2))

    fig.suptitle('Diurnal Envelope Comparison (Hourly Mean ± 1σ)',
                 fontweight='bold', y=1.02)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_DIR, 'fig_validation_diurnal.png'))
    plt.close(fig)
    print('  → fig_validation_diurnal.png')

    # ── 2. Ramp rate distribution ────────────────────────────────────────────
    print('[2/4] Ramp rate distributions...')
    fig, ax = plt.subplots(figsize=(4.25, 3.5))

    # Field ramp rates (1-min resolution)
    f_ramps = []
    for day, (t, v) in field_resamp_hours.items():
        if len(v) < 2:
            continue
        f_ramps.extend(np.diff(v))  # W/m² per minute

    f_ramps = np.array(f_ramps)
    f_ramps = f_ramps[np.abs(f_ramps) < 100]  # remove extreme outliers

    # Synthetic ramp rates
    s_ramps = []
    for day_prof in syn_profiles:
        day_prof = day_prof[300:1140]  # 05:00–19:00
        day_prof = day_prof[day_prof > DAYTIME_MIN]
        if len(day_prof) < 2:
            continue
        s_ramps.extend(np.diff(day_prof))
    s_ramps = np.array(s_ramps)
    s_ramps = s_ramps[np.abs(s_ramps) < 100]

    # Histograms
    bins = np.linspace(-100, 100, 51)
    ax.hist(s_ramps, bins=bins, density=True, alpha=0.35,
            color=C_SYNTH, label=f'Synthetic (σ={np.std(s_ramps):.2f})')
    ax.hist(f_ramps, bins=bins, density=True, alpha=0.45,
            color=C_FIELD, label=f'Field (σ={np.std(f_ramps):.2f})')

    ax.set_xlabel('Ramp Rate (W/m²/min)')
    ax.set_ylabel('Probability Density')
    ax.set_title('Ramp Rate Distribution\n(1-min resolution, non-saturated)',
                 fontweight='bold')
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.2, linewidth=0.3)
    ax.set_xlim(-60, 60)

    fig.tight_layout()
    fig.savefig(os.path.join(OUT_DIR, 'fig_validation_ramprates.png'))
    plt.close(fig)
    print('  → fig_validation_ramprates.png')
    print(f'    Field σ={np.std(f_ramps):.4f}  |  Synth σ={np.std(s_ramps):.4f}')

    # ── 3. Autocorrelation ──────────────────────────────────────────────────
    print('[3/4] Autocorrelation...')
    fig, ax = plt.subplots(figsize=(4.25, 3.5))

    # Field ACF
    f_acf_list = []
    for day, (t, v) in field_resamp_hours.items():
        if len(v) < 15:
            continue
        acf = autocorrelation(v, 30)  # 30 min
        if len(acf) > 1:
            f_acf_list.append(acf)

    # Synthetic ACF per day
    s_acf_list = []
    for day_prof in syn_profiles[:50]:
        day_prof = day_prof[day_prof > DAYTIME_MIN]
        if len(day_prof) < 15:
            continue
        acf = autocorrelation(day_prof, 30)
        if len(acf) > 1:
            s_acf_list.append(acf)

    if f_acf_list:
        f_acf_mean = np.mean(f_acf_list, axis=0)
        f_acf_std  = np.std(f_acf_list, axis=0)
        s_acf_mean = np.mean(s_acf_list, axis=0)
        s_acf_std  = np.std(s_acf_list, axis=0)

        lags = np.arange(len(f_acf_mean))
        ax.fill_between(lags,
                        f_acf_mean - f_acf_std, f_acf_mean + f_acf_std,
                        color=C_FIELD, alpha=0.15)
        ax.plot(lags, f_acf_mean, color=C_FIELD, linewidth=1.5,
                label='Field (mean ± 1σ)')

        ax.fill_between(lags[:len(s_acf_mean)],
                        s_acf_mean[:len(s_acf_mean)] - s_acf_std[:len(s_acf_mean)],
                        s_acf_mean[:len(s_acf_mean)] + s_acf_std[:len(s_acf_mean)],
                        color=C_SYNTH, alpha=0.12)
        ax.plot(lags[:len(s_acf_mean)], s_acf_mean[:len(s_acf_mean)],
                color=C_SYNTH, linewidth=1.5,
                label='Synthetic (mean ± 1σ)')

        ax.axhline(0, color='#333', linewidth=0.4, linestyle='--')
        ax.set_xlabel('Lag (minutes)')
        ax.set_ylabel('Autocorrelation')
        ax.set_title('Autocorrelation Decay\n(non-saturated daytime segments)',
                     fontweight='bold')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.2, linewidth=0.3)
        ax.set_xlim(0, 30)
        ax.set_ylim(-0.2, 1.05)
        ax.set_xticks(range(0, 31, 5))

    fig.tight_layout()
    fig.savefig(os.path.join(OUT_DIR, 'fig_validation_autocorr.png'))
    plt.close(fig)
    print('  → fig_validation_autocorr.png')

    # ── 4. CDF comparison ──────────────────────────────────────────────────
    print('[4/4] Cumulative distribution...')
    fig, ax = plt.subplots(figsize=(4.25, 3.5))

    # Field: all non-saturated daytime
    f_all = all_fh.copy()
    # Synthetic: all non-zero daytime
    s_all = syn_profiles[:, 300:1140].flatten()
    s_all = s_all[s_all > DAYTIME_MIN]
    # Also apply a synthetic ceiling for fair comparison
    s_clipped = np.clip(s_all, None, SAT_CORR_WM2)

    # Sort for CDF
    f_sorted = np.sort(f_all)
    s_sorted = np.sort(s_clipped)

    f_cdf = np.arange(1, len(f_sorted) + 1) / len(f_sorted)
    s_cdf = np.arange(1, len(s_sorted) + 1) / len(s_sorted)

    ax.plot(f_sorted, f_cdf, color=C_FIELD, linewidth=1.5,
            label=f'Field (n={len(f_all)})')
    ax.plot(s_sorted, s_cdf, color=C_SYNTH, linewidth=1.5, linestyle='--',
            label=f'Synthetic clipped-capped (n={len(s_clipped)})')

    # Also plot uncapped for reference
    s_uncapped = np.sort(s_all)
    s_ucdf = np.arange(1, len(s_uncapped) + 1) / len(s_uncapped)
    ax.plot(s_uncapped, s_ucdf, color=C_SYNTH, linewidth=0.8, linestyle=':',
            alpha=0.5, label='Synthetic (uncapped)')

    ax.axvline(SAT_CORR_WM2, color=C_SAT, linewidth=0.6, linestyle='-',
               alpha=0.6)
    ax.text(SAT_CORR_WM2 + 5, 0.15, 'Saturation\nceiling ({:.0f} W/m²)'.format(
            SAT_CORR_WM2), fontsize=6.5, color=C_SAT, ha='left')

    ax.set_xlabel('GHI (W/m²)')
    ax.set_ylabel('Cumulative Probability')
    ax.set_title('Cumulative Distribution Function\n(non-saturated daytime)',
                 fontweight='bold')
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.2, linewidth=0.3)
    ax.set_xlim(0, 850)

    # KS statistic
    from scipy.stats import ks_2samp
    ks_stat, ks_p = ks_2samp(f_all, s_clipped)
    print(f'  KS statistic (field vs capped-synth): D={ks_stat:.4f}, p={ks_p:.2e}')

    fig.tight_layout()
    fig.savefig(os.path.join(OUT_DIR, 'fig_validation_cdf.png'))
    plt.close(fig)
    print('  → fig_validation_cdf.png')

    # ── Summary statistics ──────────────────────────────────────────────────
    print('\n=== VALIDATION SUMMARY ===')
    print(f'Saturation ceiling: {SAT_RAW_WM2:.1f} W/m² raw → {SAT_CORR_WM2:.1f} W/m² corrected')
    print(f'Field daytime non-saturated: {len(all_fh)} 1-min samples')
    print(f'Synthetic daytime: {len(s_all)} 1-min samples')
    print(f'Field ramp rate σ: {np.std(f_ramps):.4f} W/m²/min')
    print(f'Synthetic ramp rate σ: {np.std(s_ramps):.4f} W/m²/min')
    print(f'Field mean GHI: {np.mean(all_fh):.2f} W/m²')
    print(f'Synthetic mean GHI (clipped): {np.mean(s_clipped):.2f} W/m²')
    print(f'Field 95th percentile: {np.percentile(all_fh, 95):.2f} W/m²')
    print(f'Synthetic 95th percentile (clipped): {np.percentile(s_clipped, 95):.2f} W/m²')
    print(f'\nCensoring bias: Saturation and thermal gaps cluster at peak')
    print(f'irradiance hours (11:00–15:00). Validation is PATTERN-LEVEL only.')
    print(f'Magnitude claims require a sensor without the 470 W/m² ceiling.')
    print(f'\nDone. Figures saved to {OUT_DIR}')

if __name__ == '__main__':
    main()
