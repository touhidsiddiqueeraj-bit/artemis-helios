"""
05_battery_model.py — SLA battery charge model (1b)
===================================================
Faithful Python port of the archived Simulink battery block
(Code/MatLab/ha_battery_v3.m): 7 Ah / 12 V SLA.

  - Shepherd open-circuit voltage:  Voc(SoC) = 11.84 + 1.98*SoC - 0.28*SoC^2
  - Internal resistance Rint = 50 mOhm, bulk charge limit 6 A
  - Coulomb-counting state (no charge-efficiency loss at the rates used here)
  - Three-stage charging: bulk CC -> CV at V_cv (13.6 V) -> taper

Reproduces the paper's claimed battery operating range 12.41-13.61 V
(SoC 0.30 -> 1.00 on a representative July day, seed 23).

Usage:
    python3 05_battery_model.py            # full-day charge simulation + report
    python3 05_battery_model.py --check    # self-check
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

Q_NOM = 7.0        # Ah
RINT = 0.050       # ohm
I_BULK_MAX = 6.0   # A, controller bulk limit
V_CV = 13.6        # V, constant-voltage target
ETA_BUCK = 0.980   # measured buck efficiency at the 3.9 A operating point


def voc(soс):
    """Shepherd OCV (V) as a function of state of charge (0-1)."""
    s = float(np.clip(soс, 0.0, 1.0))
    return 11.84 + 1.98 * s - 0.28 * s * s


def charge_step(soс, i_demand, dt=0.1):
    """
    One control-cycle charge step (dt in seconds).

    i_demand: current requested by the buck stage (A).
    Returns (SoC_new, Vt_new, i_actual).
    CV: current tapers so that Vt = Voc + Rint*I never exceeds V_CV.
    """
    i_cc = float(min(max(0.0, i_demand), I_BULK_MAX))
    i_cv = (V_CV - voc(soс)) / RINT
    i = min(i_cc, i_cv)
    i = max(0.0, i)
    dq = i * dt / 3600.0
    soс_new = min(1.0, soс + dq / Q_NOM)
    if soс_new >= 1.0:
        i = 0.0
        soс_new = 1.0
    vt = voc(soс_new) + RINT * i
    return soс_new, vt, i


def full_day(seed=23, dt=0.1, soс0=0.30):
    """
    Charge the battery from the paper's July day GHI profile (seed 23)
    through the verified buck stage (50 Wp panel, eta 98%).

    Returns dict with SoC/Vt/I histories and day summary.
    """
    ghi = _paper_ghi_01s(seed, dt)
    n = len(ghi)
    soс = np.empty(n); vt = np.empty(n); ich = np.empty(n)
    s = soс0
    for i in range(n):
        # Panel power at MPP for this G (true model), buck -> battery current
        _, pmpp, _ = _pv_mpp(ghi[i])
        i_dem = ETA_BUCK * pmpp / V_CV if pmpp > 1.0 else 0.0
        s, v, i_ch = charge_step(s, i_dem, dt)
        soс[i], vt[i], ich[i] = s, v, i_ch
    day = 24.0
    return {"t_h": np.arange(n) * dt / 3600.0, "soс": soс, "vt": vt,
            "ich": ich, "final_soс": s, "v_min": float(vt.min()),
            "v_max": float(vt.max()), "soc_start": soс0,
            "charged_ah": float((np.trapezoid(ich, dx=dt) / 3600.0))}


def _pv_mpp(G):
    """True 50 Wp model MPP (port of gen_figures_hires.py::pv_model)."""
    g = G / 1000.0
    if g <= 0:
        return 0.0, 0.0, 0.0
    Tc = 35.0 + (45.0 - 20.0) / 800.0 * G
    dT = Tc - 25.0
    VocT = 21.6 * (1 - 3.4e-3 * dT)
    IscT = 2.91 * (1 + 0.5e-3 * dT) * g
    k = 14.2606
    Vmc = VocT * (1 / (1 + k)) ** (1 / k)
    Pm = IscT * (1 - (Vmc / VocT) ** k) * Vmc
    return Pm, Vmc, VocT


def _paper_ghi_01s(seed, dt=0.1):
    """Markov + OU full-day irradiance, paper protocol (tier2, seed 23)."""
    rng = np.random.RandomState(seed)
    T = np.array([[0.6, 0.3, 0.1], [0.2, 0.5, 0.3], [0.1, 0.2, 0.7]])
    cf = [0.15, 0.55, 0.90]
    dc = np.exp(-dt)
    N = int(86400 / dt)
    GG = np.zeros(N)
    cs, ct, gf = 1, 0.0, 0.0
    for i in range(N):
        h = i * dt / 3600
        if h < 5.5 or h > 18.5:
            continue
        ct += dt
        if ct >= 15:
            ct = 0
            r = rng.rand()
            row = T[cs]
            cs = 0 if r < row[0] else (1 if r < row[0] + row[1] else 2)
        fr = (h - 5.5) / (18.5 - 5.5)
        Gc = 744 * np.sin(np.pi * fr)
        Gcf = Gc * cf[cs]
        sf = 0.25 * max(Gcf, 10)
        gf = gf * dc + sf * np.sqrt(1 - dc ** 2) * rng.randn()
        gf = max(-0.4 * Gc, min(0.4 * Gc, gf))
        GG[i] = min(Gc, max(0, Gcf + gf))
    return GG


def report():
    d = full_day()
    print("Battery charge simulation — representative July day (seed 23)")
    print("=" * 62)
    print(f"SoC: {d['soc_start']*100:.0f}% -> {d['final_soс']*100:.0f}%")
    print(f"Terminal voltage range: {d['v_min']:.2f} V .. {d['v_max']:.2f} V"
          f"  (paper claim 12.41-13.61 V)")
    print(f"Energy into battery: {d['charged_ah']:.2f} Ah "
          f"({d['charged_ah']*13.0:.0f} Wh)")
    t = d["t_h"]
    m = d["ich"] > 0
    print(f"Bulk phase: {t[m].min():.1f} h .. {t[m].max():.1f} h")
    idx_full = np.argmax(d["soс"] >= 1.0)
    print(f"Reaches full (SoC=100%): {t[idx_full]:.1f} h")


def self_check():
    s, v, i = charge_step(0.30, 3.9)
    assert abs(voc(0.0) - 11.84) < 1e-9
    assert abs(voc(0.30) - 12.409) < 1e-3, voc(0.30)   # paper's low V
    assert abs(voc(1.0) - 13.54) < 1e-3, voc(1.0)
    assert 0.30 < s < 0.31 and i > 0, (s, i)           # CC moves SoC up
    s2, v2, i2 = charge_step(0.99, 6.0)                # CV tapers current
    assert v2 <= V_CV + 1e-3 and i2 < 6.0, (v2, i2)
    d = full_day()
    assert d["final_soс"] >= 0.98, f"SoC only reached {d['final_soс']}"
    assert 12.3 < d["v_min"] < 12.6, d["v_min"]
    assert 13.5 < d["v_max"] < 13.7, d["v_max"]
    print(f"Self-check PASS: SoC {d['soc_start']:.2f}->{d['final_soс']:.2f}, "
          f"V {d['v_min']:.2f}-{d['v_max']:.2f} V")


if __name__ == "__main__":
    if "--check" in sys.argv:
        self_check()
    else:
        report()
