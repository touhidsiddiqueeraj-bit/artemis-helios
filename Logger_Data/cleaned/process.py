"""
process.py — Helios Logger field data cleaning pipeline
======================================================
Reads raw ESP32 CSV logs from esp32_storage/data/, applies:
  1. Glass attenuation correction (×1.0737)
  2. Saturation flagging (≥470.80 W/m²)
  3. Thermal gap detection
  4. Daytime filtering (>10 W/m²)

Output: field_data_cleaned.csv

Run: python3 process.py

GLASS TRANSMISSION — derived from calibration/ pair:
  noglass5min.csv  (stable n=33)  mean = 356.00 W/m²
  withglass.csv    (stable n=34)  mean = 331.57 W/m²
  ratio = 331.57 / 356.00 = 0.9314
  correction = 1 / 0.9314 = 1.0737

SATURATION — BH1750 in CONTINUOUS_LOW_RES_MODE caps at 54612.5 lux:
  max_irr = 54612.5 / 116 = 470.80 W/m²
  Any reading ≥ 470.79 is hardware-clipped, flagged saturation_flag=1.

Author: Hussain Touhid Siddiquee · Leading University Sylhet
"""

import csv
import os
from statistics import mean, stdev

BASE = os.path.dirname(os.path.abspath(__file__))
DATA_SRC = os.path.join(BASE, '..', 'esp32_storage', 'data')
OUT_DIR = BASE

GLASS_CORRECTION = 1.0737   # 1 / 0.9314
SATURATION_THRESH = 470.79  # W/m² (54612.5 lux / 116)
DAYTIME_MIN = 10.0          # W/m² — below this is night/zero
THERMAL_GAP_S = 120         # seconds — gap larger than this = thermal event or reboot

DAYS = [
    ('2026-07-09', 'startup'),
    ('2026-07-10', 'dawn_to_dusk'),
    ('2026-07-11', 'monsoon_afternoon'),
    ('2026-07-12', 'variable_clearing'),
    ('2026-07-13', 'variable_hot'),
    ('2026-07-14', 'no_data'),
]

def parse_time_seconds(t_str):
    h, m, s = t_str.split(':')
    return int(h) * 3600 + int(m) * 60 + int(s)

def parse_csv(fpath):
    rows = []
    with open(fpath) as f:
        reader = csv.reader(f)
        header = next(reader, None)
        for r in reader:
            if len(r) < 7:
                continue
            try:
                rows.append({
                    'date': r[0].strip(),
                    'time': r[1].strip(),
                    'elapsed_s': int(r[2]),
                    'lux': float(r[3]),
                    'raw_irr': float(r[4]),
                    'blue_channel': int(r[5]),
                    'temp_c': float(r[6]),
                })
            except (ValueError, IndexError):
                continue
    return rows

def main():
    all_rows = []

    for date_str, label in DAYS:
        fpath = os.path.join(DATA_SRC, f'{date_str}.csv')
        if not os.path.exists(fpath):
            continue

        raw = parse_csv(fpath)
        if not raw:
            continue

        # Track gaps
        prev_ts = None
        prev_elapsed = None

        for r in raw:
            ts = parse_time_seconds(r['time'])
            elapsed = r['elapsed_s']

            # Gap detection (elapsed_s jump > 120 or time jump > 120)
            gap_seconds = 0
            if prev_elapsed is not None:
                gap_seconds = elapsed - prev_elapsed
            elif prev_ts is not None:
                gap_seconds = ts - prev_ts
            thermal_gap = 1 if gap_seconds > THERMAL_GAP_S else 0

            # Daytime filter
            daytime = 1 if r['raw_irr'] > DAYTIME_MIN else 0

            # Glass correction
            corrected_irr = r['raw_irr'] * GLASS_CORRECTION

            # Saturation flag
            saturation = 1 if r['raw_irr'] >= SATURATION_THRESH else 0

            hour_decimal = ts / 3600.0

            all_rows.append({
                'date': r['date'],
                'time': r['time'],
                'hour_decimal': round(hour_decimal, 5),
                'elapsed_s': elapsed,
                'lux': r['lux'],
                'raw_irradiance_wm2': round(r['raw_irr'], 4),
                'glass_corrected_irr_wm2': round(corrected_irr, 4),
                'saturation_flag': saturation,
                'blue_channel': r['blue_channel'],
                'temp_c': r['temp_c'],
                'daytime_flag': daytime,
                'thermal_gap_flag': thermal_gap,
                'day_id': date_str,
            })

            prev_ts = ts
            prev_elapsed = elapsed

        print(f'{date_str} ({label}): {len(raw)} raw → {len([r for r in all_rows if r["day_id"] == date_str])} processed')

    # Sort by date then time
    all_rows.sort(key=lambda r: (r['date'], r['time']))

    out_path = os.path.join(OUT_DIR, 'field_data_cleaned.csv')
    fieldnames = [
        'date', 'time', 'hour_decimal', 'elapsed_s',
        'lux', 'raw_irradiance_wm2', 'glass_corrected_irr_wm2',
        'saturation_flag', 'blue_channel', 'temp_c',
        'daytime_flag', 'thermal_gap_flag', 'day_id',
    ]

    with open(out_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(all_rows)

    n_daytime = sum(1 for r in all_rows if r['daytime_flag'])
    n_sat = sum(1 for r in all_rows if r['saturation_flag'])
    n_gap = sum(1 for r in all_rows if r['thermal_gap_flag'])
    corrected_daytime = [r['glass_corrected_irr_wm2'] for r in all_rows if r['daytime_flag'] and not r['saturation_flag']]

    print(f'\n=== CLEANING SUMMARY ===')
    print(f'Total rows written:      {len(all_rows)}')
    print(f'Daytime rows (>10 W/m²): {n_daytime}')
    print(f'Saturated readings:      {n_sat} ({100*n_sat/len(all_rows):.1f}%)')
    print(f'Thermal gap markers:     {n_gap}')
    if corrected_daytime:
        print(f'Corrected daytime mean:  {mean(corrected_daytime):.2f} W/m² (non-saturated)')
        print(f'Corrected daytime peak:  {max(corrected_daytime):.2f} W/m² (non-saturated)')
    print(f'\nOutput: {out_path}')

if __name__ == '__main__':
    main()
