"""
01_irradiance_generator.py
==========================
Helios-Artemis: Markov-Chain Synthetic Irradiance Profile Generator
Parameterised from NASA POWER + SREDA Bangladesh Solar Resource Atlas
Location: Sylhet, Bangladesh (24.89°N, 91.87°E)

Generates:
  - Per-day 1-minute GHI profiles using a 4-state Markov chain
  - Year 1 (training, seed = 137*day + 500)
  - Year 2 (independent test, seed = 251*day + 9999)

Markov states:
  0 = clear sky           (multiplier = 1.00)
  1 = thin cloud          (multiplier = 0.65)
  2 = thick cloud         (multiplier = 0.20)
  3 = cloud edge enhance  (multiplier = 1.18)  ← lensing effect

Reference: Table I, Sections III-C and IV of the paper.
"""

import numpy as np
import pandas as pd
import math
import random
import os

# ─────────────────────────────────────────────────────────────────────────────
# Monthly climatological parameters (NASA POWER + SREDA Sylhet)
# ─────────────────────────────────────────────────────────────────────────────
MONTHLY_PARAMS = {
     1: {"peak": 820, "cvi": 0.15, "sunrise": 6.30, "sunset": 17.40},
     2: {"peak": 850, "cvi": 0.20, "sunrise": 6.13, "sunset": 17.80},
     3: {"peak": 870, "cvi": 0.25, "sunrise": 5.90, "sunset": 18.20},
     4: {"peak": 880, "cvi": 0.30, "sunrise": 5.60, "sunset": 18.60},
     5: {"peak": 830, "cvi": 0.45, "sunrise": 5.40, "sunset": 18.90},
     6: {"peak": 640, "cvi": 0.70, "sunrise": 5.30, "sunset": 19.00},
     7: {"peak": 580, "cvi": 0.85, "sunrise": 5.30, "sunset": 19.10},
     8: {"peak": 610, "cvi": 0.80, "sunrise": 5.50, "sunset": 19.00},
     9: {"peak": 650, "cvi": 0.65, "sunrise": 5.80, "sunset": 18.20},
    10: {"peak": 780, "cvi": 0.30, "sunrise": 6.00, "sunset": 17.60},
    11: {"peak": 810, "cvi": 0.18, "sunrise": 6.20, "sunset": 17.30},
    12: {"peak": 800, "cvi": 0.15, "sunrise": 6.40, "sunset": 17.10},
}

# State irradiance multipliers
STATE_MULTIPLIERS = np.array([1.00, 0.65, 0.20, 1.18])


def get_transition_matrix(cvi: float) -> np.ndarray:
    """Return 4×4 Markov transition matrix calibrated to Cloud Variability Index."""
    if cvi <= 0.20:           # Dry season (Jan, Dec)
        return np.array([
            [0.90, 0.08, 0.01, 0.01],
            [0.30, 0.55, 0.13, 0.02],
            [0.20, 0.40, 0.38, 0.02],
            [0.50, 0.30, 0.10, 0.10],
        ])
    elif cvi <= 0.45:         # Transition months
        return np.array([
            [0.70, 0.18, 0.08, 0.04],
            [0.20, 0.50, 0.25, 0.05],
            [0.15, 0.30, 0.50, 0.05],
            [0.40, 0.35, 0.15, 0.10],
        ])
    else:                     # Monsoon (Jun–Sep)
        return np.array([
            [0.45, 0.25, 0.24, 0.06],
            [0.15, 0.40, 0.38, 0.07],
            [0.10, 0.20, 0.65, 0.05],
            [0.35, 0.30, 0.25, 0.10],
        ])


def clear_sky_ghi(hour: float, peak: float, sunrise: float, sunset: float) -> float:
    """Sinusoidal clear-sky GHI model."""
    if hour < sunrise or hour > sunset:
        return 0.0
    angle = math.pi * (hour - sunrise) / (sunset - sunrise)
    return max(0.0, peak * math.sin(angle))


def month_from_doy(doy: int) -> int:
    """Convert day-of-year (1–365) to month (1–12)."""
    days = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    cumulative = 0
    for m, n in enumerate(days, 1):
        cumulative += n
        if doy <= cumulative:
            return m
    return 12


def generate_day_profile(doy: int, seed: int, noise_sigma_frac: float = 0.02) -> np.ndarray:
    """
    Generate a 1440-sample (1-minute resolution) GHI profile for one day.

    Args:
        doy:              Day of year (1–365)
        seed:             Random seed for reproducibility
        noise_sigma_frac: Gaussian noise as fraction of clear-sky GHI

    Returns:
        ghi: np.ndarray shape (1440,) — GHI in W/m²
    """
    rng = random.Random(seed)
    month = month_from_doy(doy)
    p = MONTHLY_PARAMS[month]
    matrix = get_transition_matrix(p["cvi"])

    state = 0
    ghi = np.zeros(1440)

    for minute in range(1440):
        hour = minute / 60.0
        cs = clear_sky_ghi(hour, p["peak"], p["sunrise"], p["sunset"])

        # Transition state every 5 minutes during daylight
        if minute % 5 == 0 and cs > 0:
            r = rng.random()
            cumulative = 0.0
            for s, prob in enumerate(matrix[state]):
                cumulative += prob
                if r < cumulative:
                    state = s
                    break

        noise = rng.gauss(0.0, max(1.0, cs * noise_sigma_frac))
        ghi[minute] = max(0.0, cs * STATE_MULTIPLIERS[state] + noise)

    return ghi


def generate_dataset(n_days: int = 365, seed_fn=None,
                     start_doy: int = 1) -> pd.DataFrame:
    """
    Generate a full-year dataset of 1-minute GHI profiles.

    Args:
        n_days:    Number of days to generate
        seed_fn:   Callable(doy) -> seed integer
        start_doy: Starting day of year

    Returns:
        DataFrame with columns: doy, month, minute, hour, ghi_wm2
    """
    records = []
    for day_idx in range(n_days):
        doy = start_doy + day_idx
        seed = seed_fn(doy)
        profile = generate_day_profile(doy, seed)
        month = month_from_doy(doy)
        for minute in range(1440):
            records.append({
                "doy":      doy,
                "month":    month,
                "minute":   minute,
                "hour":     round(minute / 60.0, 4),
                "ghi_wm2":  round(float(profile[minute]), 2),
            })
    return pd.DataFrame(records)


def generate_hourly_dataset(n_days: int = 365, seed_fn=None) -> pd.DataFrame:
    """
    Generate hourly-averaged GHI for LSTM training (24 samples/day).

    Returns:
        DataFrame with columns: doy, month, hour, ghi_hourly_wm2
    """
    records = []
    for day_idx in range(n_days):
        doy = day_idx + 1
        seed = seed_fn(doy)
        profile = generate_day_profile(doy, seed)
        month = month_from_doy(doy)
        for h in range(24):
            hourly_mean = float(np.mean(profile[h*60:(h+1)*60]))
            records.append({
                "doy":            doy,
                "month":          month,
                "hour":           h,
                "ghi_hourly_wm2": round(hourly_mean, 2),
            })
    return pd.DataFrame(records)


# ─────────────────────────────────────────────────────────────────────────────
# Main: generate and save datasets
# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    os.makedirs("data", exist_ok=True)

    # Year 1 — training set (seed = 137*doy + 500)
    print("Generating Year 1 training dataset (365 days × 1440 minutes)...")
    df_y1 = generate_dataset(n_days=365, seed_fn=lambda doy: 137 * doy + 500)
    df_y1.to_csv("data/year1_training_1min.csv", index=False)
    print(f"  Saved: data/year1_training_1min.csv  ({len(df_y1):,} rows)")

    df_y1_hourly = generate_hourly_dataset(
        n_days=365, seed_fn=lambda doy: 137 * doy + 500)
    df_y1_hourly.to_csv("data/year1_training_hourly.csv", index=False)
    print(f"  Saved: data/year1_training_hourly.csv  ({len(df_y1_hourly):,} rows)")

    # Year 2 — independent test set (seed = 251*doy + 9999)
    print("Generating Year 2 independent test dataset (365 days × 1440 minutes)...")
    df_y2 = generate_dataset(n_days=365, seed_fn=lambda doy: 251 * doy + 9999)
    df_y2.to_csv("data/year2_test_1min.csv", index=False)
    print(f"  Saved: data/year2_test_1min.csv  ({len(df_y2):,} rows)")

    df_y2_hourly = generate_hourly_dataset(
        n_days=365, seed_fn=lambda doy: 251 * doy + 9999)
    df_y2_hourly.to_csv("data/year2_test_hourly.csv", index=False)
    print(f"  Saved: data/year2_test_hourly.csv  ({len(df_y2_hourly):,} rows)")

    # Daily summary statistics
    print("Computing daily summary statistics...")
    summary_rows = []
    for year, seed_fn in [(1, lambda d: 137*d+500), (2, lambda d: 251*d+9999)]:
        for doy in range(1, 366):
            profile = generate_day_profile(doy, seed_fn(doy))
            daytime = profile[profile > 10]
            month = month_from_doy(doy)
            summary_rows.append({
                "year":           year,
                "doy":            doy,
                "month":          month,
                "cvi":            MONTHLY_PARAMS[month]["cvi"],
                "peak_ghi_wm2":   round(float(profile.max()), 1),
                "daily_energy_wh_m2": round(float(profile.sum() / 60), 1),
                "daytime_mean_ghi":   round(float(daytime.mean()) if len(daytime) else 0, 1),
                "daytime_std_ghi":    round(float(daytime.std()) if len(daytime) else 0, 1),
                "n_daytime_minutes":  int(len(daytime)),
            })
    pd.DataFrame(summary_rows).to_csv("data/daily_summary_both_years.csv", index=False)
    print("  Saved: data/daily_summary_both_years.csv")

    print("\nDone. All irradiance datasets generated.")
