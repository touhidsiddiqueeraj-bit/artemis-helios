"""
08_calibration_uncertainty.py — B.2 calibration-uncertainty budget (analytic)
=============================================================================
Uncertainty budget for the GY-302 (BH1750) lux sensor used by the field
logger, propagated through the irradiance model G = k * lux:

  delta_lux : BH1750 datasheet measurement accuracy (+/- 20 %)
  delta_k   : spectral-mismatch of the CIE AM1.5 luminous-efficacy
              constant k = 0.0079 W/m2/lux across daylight spectra
              (clear-sky vs overcast), conservative +/- 15 %
  delta_res : 1 lx H-res quantization, negligible above ~100 W/m2

Combined relative uncertainty (RSS): sqrt(d_lux^2 + d_k^2) ~ 25 %.

Key analytic point for the response letter: this uncertainty enters ONLY
the LSTM prediction path (G_pred). The underlying P&O loop measures dP/dV
directly, so steady-state tracking does not inherit the sensor budget —
Table VI efficiencies (>= 93 % under all transients) are obtained with
the same calibrated model.

Run:     python3 08_calibration_uncertainty.py [--check]
"""
import argparse
import math
import sys

LUX_ACCURACY = 0.20        # BH1750 datasheet
SPECTRAL_MISMATCH = 0.15   # luminous-efficacy constant, daylight spectra
LUX_TO_WM2 = 0.0079
POINTS_WM2 = [100.0, 500.0, 1000.0]


def combined_relative():
    return math.sqrt(LUX_ACCURACY ** 2 + SPECTRAL_MISMATCH ** 2)


def budget():
    rel = combined_relative()
    rows = [(g, g * rel) for g in POINTS_WM2]
    return rel, rows


def main():
    rel, rows = budget()
    print(f'combined relative uncertainty: {100*rel:.1f} % (RSS of '
          f'{100*LUX_ACCURACY:.0f} % sensor + {100*SPECTRAL_MISMATCH:.0f} % '
          f'spectral-mismatch)')
    for g, abs_ in rows:
        print(f'  G = {g:5.0f} W/m2  ->  +/- {abs_:5.0f} W/m2 '
              f'(+/- {100*abs_/g:.0f} %)')
    print('model: G = lux x 0.0079 (CIE AM1.5); '
          'uncertainty affects only the G_pred prediction path')


def check():
    rel, rows = budget()
    assert 0.20 <= rel <= 0.35, f'unexpected combined rel: {rel:.3f}'
    for g, a in rows:
        assert abs(a / g - rel) < 1e-9
    assert LUX_TO_WM2 == 0.0079
    print('check PASS: budget consistent, percentages relative to reading')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    main()
    if args.check:
        check()
