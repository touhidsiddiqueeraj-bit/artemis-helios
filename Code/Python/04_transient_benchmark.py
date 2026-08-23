"""
04_transient_benchmark.py
=========================
Helios-Artemis IJPEDS round-2 revision — paper item 1:
transient benchmark suite demanded by Reviewer A/C.

Benchmarks all 4 MPPT controllers (Plain P&O, VS-P&O, INC, LSTM-P&O)
from 03_mppt_controllers.py on 6 irradiance waveforms at dt=0.1 s:

  1. step-up      600->1000 W/m^2  (t=60)
  2. step-down    1000->600 W/m^2  (t=60)
  3. ramp         400->900 W/m^2   (t=30..90)
  4. cloud-edge   900->300 (5 s) -> 900 (20 s recovery)
  5. repeated-cloud   5 x cloud-edge cycles (t=60,120,180,240,300)
  6. stochastic   Markov(3-state) + Ornstein-Uhlenbeck flicker, 3600 s

Metrics per waveform x controller (evaluation window = first event to end):
  eta_track, max_tracking_error, settling_time, overshoot, undershoot,
  energy_not_captured, oscillation_amplitude, mpp_voltage_error

Outputs:
  results/transient_benchmark.csv      (long format, seeds Table IV)
  Figures/fig15_transient_benchmark.png (300 dpi, pattern-based, CB-safe)

Usage:
  python 04_transient_benchmark.py            # full run + printout
  python 04_transient_benchmark.py --check    # + self-check asserts

Reference: Sections III-B, V and Table IV of the paper.
"""

import os
import sys
import argparse
import importlib.util
from dataclasses import dataclass

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
DT = 0.1  # s, control loop interval (paper UART interval)
T_AMB = 30.0  # °C, fixed everywhere

# ─────────────────────────────────────────────────────────────────────────────
# Import 03_mppt_controllers.py (module name starts with digits -> importlib)
# ─────────────────────────────────────────────────────────────────────────────
def _load_controllers():
    path = os.path.join(HERE, '03_mppt_controllers.py')
    spec = importlib.util.spec_from_file_location('mppt_controllers', path)
    if spec is None or spec.loader is None:
        raise ImportError(f'cannot load module from {path}')
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

MC = _load_controllers()
# True PV model ported verbatim from gen_figures_hires.py::pv_model
# (the authoritative model — paper Table III numbers were computed with it):
#   single-diode, 50Wp panel, Isc0=2.91 A, Voc0=21.6 V, k=14.2606, NOCT=45.
compute_tracking_efficiency = MC.compute_tracking_efficiency
PlainPaO, VariableStepPaO, IncrementalConductance, LSTMAssistedPaO = (
    MC.PlainPaO, MC.VariableStepPaO, MC.IncrementalConductance, MC.LSTMAssistedPaO)


def pv_power(V_ref: float, G: float, T_amb: float = T_AMB,
             Isc0=2.91, Voc0=21.6, k=14.2606, bV=-3.4e-3,
             aI=0.5e-3, NOCT=45) -> tuple[float, float, float]:
    """True-model (I, V, P) at operating voltage V_ref.

    Port of gen_figures_hires.py lines 32-40 (same equations, same clamps):
    Vrc = clamp(V_ref, 0.6*Voc0, 0.98*Voc0); Ipv = max(0, IscT*(1-(Vrc/VocT)^k)).
    """
    g = G / 1000.0
    Tc = T_amb + (NOCT - 20.0) / 800.0 * G
    dT = Tc - 25.0
    VocT = Voc0 * (1.0 + bV * dT)
    IscT = Isc0 * (1.0 + aI * dT) * g
    Vrc = min(max(V_ref, 0.6 * Voc0), 0.98 * Voc0)
    Ipv = max(0.0, IscT * (1.0 - (Vrc / VocT) ** k))
    return Ipv, Vrc, Ipv * Vrc


def mpp_power(G: float, T_amb: float = T_AMB,
              Isc0=2.91, Voc0=21.6, k=14.2606, bV=-3.4e-3,
              aI=0.5e-3, NOCT=45) -> tuple[float, float]:
    """Analytic true-model MPP: Vmc = VocT*(1/(1+k))**(1/k). Exact, no search."""
    g = G / 1000.0
    Tc = T_amb + (NOCT - 20.0) / 800.0 * G
    dT = Tc - 25.0
    VocT = Voc0 * (1.0 + bV * dT)
    IscT = Isc0 * (1.0 + aI * dT) * g
    Vmc = VocT * (1.0 / (1.0 + k)) ** (1.0 / k)
    Pm = IscT * (1.0 - (Vmc / VocT) ** k) * Vmc
    return Pm, Vmc

CONTROLLERS = [
    ('Plain-P&O',   lambda: PlainPaO()),
    ('VS-P&O',      lambda: VariableStepPaO()),
    ('INC',         lambda: IncrementalConductance()),
    ('LSTM-P&O',    lambda: LSTMAssistedPaO()),
]
V_INIT = 17.0

# ─────────────────────────────────────────────────────────────────────────────
# 1. Waveform generator (all at dt = 0.1 s)
# ─────────────────────────────────────────────────────────────────────────────
def _t_grid(t_end):
    return np.arange(0.0, t_end + DT / 2, DT)

def waveform_step_up():
    t = _t_grid(240.0)
    G = np.where(t < 60.0, 600.0, 1000.0)
    return {'name': 'step-up', 't': t, 'G': G, 'event': 60.0}

def waveform_step_down():
    t = _t_grid(240.0)
    G = np.where(t < 60.0, 1000.0, 600.0)
    return {'name': 'step-down', 't': t, 'G': G, 'event': 60.0}

def waveform_ramp():
    t = _t_grid(180.0)
    G = np.clip(400.0 + (t - 30.0) * (500.0 / 60.0), 400.0, 900.0)
    return {'name': 'ramp', 't': t, 'G': G, 'event': 30.0}

def waveform_cloud_edge():
    t = _t_grid(180.0)
    G = np.full_like(t, 900.0)
    drop = (t >= 60.0) & (t < 65.0)
    rec = (t >= 65.0) & (t < 85.0)
    G[drop] = 900.0 - (t[drop] - 60.0) * (600.0 / 5.0)
    G[rec] = 300.0 + (t[rec] - 65.0) * (600.0 / 20.0)
    return {'name': 'cloud-edge', 't': t, 'G': G, 'event': 60.0}

def waveform_repeated_cloud():
    t = _t_grid(360.0)
    G = np.full_like(t, 900.0)
    for start in (60.0, 120.0, 180.0, 240.0, 300.0):
        drop = (t >= start) & (t < start + 5.0)
        rec = (t >= start + 5.0) & (t < start + 25.0)
        G[drop] = 900.0 - (t[drop] - start) * (600.0 / 5.0)
        G[rec] = 300.0 + (t[rec] - start - 5.0) * (600.0 / 20.0)
    return {'name': 'repeated-cloud', 't': t, 'G': G, 'event': 60.0}

def stochastic_day(seed: int = 23, dt: float = 0.1, duration: float = 3600.0):
    """
    Ornstein-Uhlenbeck flicker around a 3-state Markov cloud level on a
    clear-sky day shape (Sylhet July: peak 744 W/m^2, sunrise 05:18,
    sunset 19:06). Markov states transition every 15 s during daylight.

    OU: dG = -(dt/tau)*(G - G_base) + sigma*sqrt(2*dt/tau)*N(0,1)*G_base
    tau = 1 s, sigma = 25% of cloud-filtered level. Fixed seed.
    """
    rng = np.random.default_rng(seed)
    t = np.arange(0.0, duration + dt / 2, dt)

    # Clear-sky day shape: sinusoid peaking at 744 W/m^2 at solar noon.
    sunrise, sunset, peak = 5.3, 19.1, 744.0  # hours (05:18, 19:06)
    noon = 0.5 * (sunrise + sunset)            # 12.2 h
    hour = noon + (t - duration / 2) / 3600.0  # 1 h window centred on noon
    clear = np.clip(peak * np.sin(np.pi * (hour - sunrise) / (sunset - sunrise)),
                    0.0, None)

    # 3-state Markov cloud level (clear 1.00, thin 0.65, thick 0.20),
    # transitions every 15 s during daylight.
    mult = {0: 1.00, 1: 0.65, 2: 0.20}
    P = np.array([  # rows: clear, thin, thick; per-15 s transition probs
        [0.95, 0.05, 0.00],
        [0.10, 0.75, 0.15],
        [0.00, 0.20, 0.80],
    ])
    step15 = int(round(15.0 / dt))
    state = 0
    G_base = np.zeros_like(clear)
    for i in range(0, len(t), step15):
        if i > 0:
            state = int(rng.choice(3, p=P[state]))
        j = min(i + step15, len(t))
        G_base[i:j] = mult[state] * clear[i:j]

    # OU flicker around the cloud-filtered base.
    tau, sigma = 1.0, 0.25
    a = dt / tau
    b = sigma * np.sqrt(2.0 * dt / tau)
    G = np.zeros_like(t)
    G[0] = G_base[0]
    noise = rng.standard_normal(len(t))
    for i in range(len(t) - 1):
        G[i + 1] = G[i] - a * (G[i] - G_base[i]) + b * G_base[i] * noise[i]
    G = np.maximum(G, 0.0)

    return {'name': 'stochastic', 't': t, 'G': G, 'event': 0.0}


def all_waveforms():
    return [waveform_step_up(), waveform_step_down(), waveform_ramp(),
            waveform_cloud_edge(), waveform_repeated_cloud(),
            stochastic_day(seed=23)]

# ─────────────────────────────────────────────────────────────────────────────
# 2. Fair benchmark harness
# ─────────────────────────────────────────────────────────────────────────────
def run_controller(ctrl, G, dt=DT, T_amb=T_AMB):
    """
    Run one controller over an irradiance series at dt=0.1 s.
    V_ref <- ctrl.step(I, V); P_pv evaluated at the POST-step V_ref.
    For LSTMAssistedPaO, G_pred is the AR(1) optimal low-pass forecast
    (ema = 0.9*ema + 0.1*G_now, causal, no future information).
    Returns (P_pv, V_ref) arrays.
    """
    n = len(G)
    P_pv = np.empty(n)
    V_ref = V_INIT
    V_refs = np.empty(n)
    ema = G[0]
    for i in range(n):
        I, V, _ = pv_power(V_ref, G[i], T_amb)
        if isinstance(ctrl, LSTMAssistedPaO):
            ema = 0.9 * ema + 0.1 * G[i]
            V_ref = ctrl.step(I, V, G[i], ema)
        else:
            V_ref = ctrl.step(I, V)
        _, _, P = pv_power(V_ref, G[i], T_amb)  # post-step power
        P_pv[i] = P
        V_refs[i] = V_ref
    return P_pv, V_refs


def mpp_series(G):
    """Theoretical MPP power and voltage for every sample."""
    P_mpp = np.empty(len(G))
    V_mpp = np.empty(len(G))
    for i, g in enumerate(G):
        P_mpp[i], V_mpp[i] = mpp_power(g, T_AMB)
    return P_mpp, V_mpp


def settling_time_from(rel_err, t, t_event, band=0.02):
    """Time after t_event until |P-Pmpp|/Pmpp enters and STAYS within band."""
    idx0 = int(np.searchsorted(t, t_event))
    rel = rel_err[idx0:]
    tt = t[idx0:]
    if len(rel) == 0:
        return np.nan
    # last index where rel exceeds the band; settling index = one past it
    bad = np.flatnonzero(rel > band)
    if len(bad) == 0:
        settle_idx = 0
    elif bad[-1] == len(rel) - 1:
        return np.nan  # never stays inside
    else:
        settle_idx = bad[-1] + 1
    return tt[settle_idx] - t_event


def compute_metrics(P_pv, P_mpp, V_ref, V_mpp, t, t_event,
                    steady_s=20.0, settle_ok=True):
    """All 8 metrics over the post-event evaluation window."""
    win = t >= t_event
    pw, mw = P_pv[win], P_mpp[win]
    vw, uw = V_ref[win], V_mpp[win]
    tw = t[win]
    n_win = len(tw)

    # steady tail of the window (stochastic: last 600 s)
    if steady_s >= tw[-1] - tw[0]:
        steady = np.ones(n_win, dtype=bool)
    else:
        steady = tw >= tw[-1] - steady_s

    rel_err = np.abs(mw - pw) / np.maximum(mw, 1e-6)
    deficit = 100.0 * (mw - pw) / np.maximum(mw, 1e-6)
    excess = 100.0 * (pw - mw) / np.maximum(mw, 1e-6)

    eta = 100.0 * compute_tracking_efficiency(pw, mw)
    pos_deficit = deficit[deficit > 0]
    max_track_err = float(pos_deficit.max()) if pos_deficit.size else 0.0
    settling = (settling_time_from(rel_err, tw, t_event)
                if settle_ok else np.nan)
    overshoot = float(max(0.0, excess.max()))
    undershoot = float(excess.min())
    energy_lost = DT * float(np.maximum(0.0, mw - pw).sum()) / 3600.0
    osc = float(np.std(pw[steady] - mw[steady]))
    v_err = float(np.mean(np.abs(vw[steady] - uw[steady])))

    return {
        'eta_track': eta,
        'max_tracking_error': max_track_err,
        'settling_time': settling,
        'overshoot': overshoot,
        'undershoot': undershoot,
        'energy_not_captured': energy_lost,
        'oscillation_amplitude': osc,
        'mpp_voltage_error': v_err,
    }


# ─────────────────────────────────────────────────────────────────────────────
# 3. Outputs: CSV + printout + figure
# ─────────────────────────────────────────────────────────────────────────────
CSV_PATH = os.path.join(HERE, 'results', 'transient_benchmark.csv')
FIG_PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                        'Figures', 'fig15_transient_benchmark.png')

# Colour-blind-safe palette (Okabe-Ito) + pattern-based differentiation.
CB_COLORS = ['#0072B2', '#D55E00', '#009E73', '#CC79A7']
HATCHES = ['', '//', 'xx', '\\\\']
LINESTYLES = ['-', '--', '-.', ':']

METRIC_ORDER = ['eta_track', 'max_tracking_error', 'settling_time',
                'overshoot', 'undershoot', 'energy_not_captured',
                'oscillation_amplitude', 'mpp_voltage_error']


def write_csv(rows):
    os.makedirs(os.path.dirname(CSV_PATH), exist_ok=True)
    with open(CSV_PATH, 'w', newline='') as f:
        f.write('waveform,controller,metric,value\n')
        for r in rows:
            f.write(f'{r[0]},{r[1]},{r[2]},{r[3]:.6g}\n')


def print_summary(results):
    """Waveform x controller eta_track table + compact metric tables."""
    waves = list(results.keys())
    ctrls = list(results[waves[0]].keys())

    print('\n' + '=' * 78)
    print('TRANSIENT BENCHMARK SUMMARY  (seeds paper Table IV)')
    print('=' * 78)

    print(f'\neta_track (%)  — waveform rows x controller columns')
    hdr = f'{"waveform":<16}' + ''.join(f'{c:>12}' for c in ctrls)
    print(hdr); print('-' * len(hdr))
    for w in waves:
        print(f'{w:<16}' + ''.join(
            f'{results[w][c]["eta_track"]:>12.2f}' for c in ctrls))

    for metric in METRIC_ORDER[1:]:
        unit = {'max_tracking_error': ' %', 'settling_time': ' s',
                'overshoot': ' %', 'undershoot': ' %',
                'energy_not_captured': ' Wh',
                'oscillation_amplitude': ' W',
                'mpp_voltage_error': ' V'}[metric]
        print(f'\n{metric} ({unit.strip()})')
        hdr = f'{"waveform":<16}' + ''.join(f'{c:>12}' for c in ctrls)
        print(hdr); print('-' * len(hdr))
        for w in waves:
            vals = []
            for c in ctrls:
                v = results[w][c][metric]
                vals.append(f'{v:>12.3f}' if np.isfinite(v) else f'{"N/A":>12}')
            print(f'{w:<16}' + ''.join(vals))

    # Step-down settling vs the paper's VS-P&O cold-start claim (~18 steps).
    print('\n' + '-' * 78)
    print('Step-down 2% settling time (from t=60 s):')
    for c in ctrls:
        v = results['step-down'][c]['settling_time']
        print(f'  {c:<12} {v:8.2f} s' if np.isfinite(v) else f'  {c:<12}     N/A')
    print('(paper: VS-P&O ~18 steps to 94% from cold start — reference point)')
    print('=' * 78)


def make_figure(results, step_down_trace, step_down_mpp):
    """fig17: (a) P_pv traces on step-down, (b) eta_track grouped bars."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.0, 3.5))
    waves = list(results.keys())
    ctrls = list(results[waves[0]].keys())

    # (a) step-down P_pv traces
    t = step_down_trace[0]
    for ci, c in enumerate(ctrls):
        ax1.plot(t, step_down_trace[1][c], color=CB_COLORS[ci],
                 ls=LINESTYLES[ci], lw=1.2, label=c)
    ax1.plot(t, step_down_mpp, 'k--', lw=1.0, label='$P_{MPP}$')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('$P_{pv}$ (W)')
    ax1.set_title('(a) Step-down transient (1000$\\rightarrow$600 W/m$^2$)')
    ax1.legend(fontsize=10, framealpha=0.9)
    ax1.set_xlim(0, 240)
    ax1.grid(alpha=0.3)

    # (b) eta_track grouped bars with hatches
    x = np.arange(len(waves))
    width = 0.2
    for ci, c in enumerate(ctrls):
        vals = [results[w][c]['eta_track'] for w in waves]
        ax2.bar(x + (ci - 1.5) * width, vals, width,
                color=CB_COLORS[ci], hatch=HATCHES[ci],
                edgecolor='black', linewidth=0.4, label=c)
    ax2.set_xticks(x)
    ax2.set_xticklabels(waves, rotation=20, ha='right')
    ax2.set_ylabel('$\\eta_{track}$ (%)')
    ax2.set_title('(b) Tracking efficiency per waveform')
    ax2.legend(fontsize=10, framealpha=0.9, ncol=2)
    ax2.set_ylim(80, 100.5)
    ax2.grid(axis='y', alpha=0.3)

    fig.tight_layout()
    os.makedirs(os.path.dirname(FIG_PATH), exist_ok=True)
    fig.savefig(FIG_PATH, dpi=300)
    plt.close(fig)


# ─────────────────────────────────────────────────────────────────────────────
# 4. Self-check
# ─────────────────────────────────────────────────────────────────────────────
def run_constant_check():
    """Constant G=800 clear run: every controller eta_track > 95%."""
    t = _t_grid(120.0)
    G = np.full_like(t, 800.0)
    P_mpp, _ = mpp_series(G)
    etas = {}
    for name, make in CONTROLLERS:
        ctrl = make(); ctrl.reset(V_INIT)
        P_pv, _ = run_controller(ctrl, G)
        etas[name] = 100.0 * compute_tracking_efficiency(P_pv, P_mpp)
    return etas


def main():
    ap = argparse.ArgumentParser(description='Transient MPPT benchmark')
    ap.add_argument('--check', action='store_true',
                    help='run self-check asserts after the benchmark')
    args = ap.parse_args()

    waves = all_waveforms()
    results = {}
    step_down_trace = None
    step_down_mpp = None

    for wf in waves:
        name = wf['name']
        P_mpp, V_mpp = mpp_series(wf['G'])
        results[name] = {}
        steady_s = 600.0 if name == 'stochastic' else 20.0
        settle_ok = name not in ('stochastic', 'repeated-cloud')
        for cname, make in CONTROLLERS:
            ctrl = make(); ctrl.reset(V_INIT)
            P_pv, V_ref = run_controller(ctrl, wf['G'])
            results[name][cname] = compute_metrics(
                P_pv, P_mpp, V_ref, V_mpp, wf['t'], wf['event'],
                steady_s=steady_s, settle_ok=settle_ok)
            if name == 'step-down':
                if step_down_trace is None:
                    step_down_trace = (wf['t'], {})
                step_down_trace[1][cname] = P_pv
                step_down_mpp = P_mpp

    rows = [(w, c, m, results[w][c][m])
            for w in results for c in results[w] for m in METRIC_ORDER]
    write_csv(rows)
    print_summary(results)
    make_figure(results, step_down_trace, step_down_mpp)
    print(f'\nCSV  : {CSV_PATH}  ({len(rows)} rows)')
    print(f'FIG  : {FIG_PATH}')

    if args.check:
        checks = []

        # 1. constant G=800: every controller > 95%
        etas = run_constant_check()
        for name, eta in etas.items():
            checks.append(('constant G=800: eta_track > 95%',
                           eta > 95.0, f'{name} = {eta:.2f}%'))
        print('[check] constant G=800: '
              + ' '.join(f'{k}={v:.2f}%' for k, v in etas.items()))

        # 2. core claim: LSTM-P&O > Plain P&O on step-down
        l_eta = results['step-down']['LSTM-P&O']['eta_track']
        p_eta = results['step-down']['Plain-P&O']['eta_track']
        checks.append(('step-down: LSTM-P&O > Plain-P&O (core claim)',
                       l_eta > p_eta,
                       f'LSTM {l_eta:.2f}% vs Plain {p_eta:.2f}% '
                       f'(delta {l_eta - p_eta:+.2f} pp)'))

        # 3. energy_not_captured >= 0 everywhere
        all_nonneg = all(v >= 0.0 for w, c, m, v in rows
                         if m == 'energy_not_captured')
        checks.append(('energy_not_captured >= 0 for all runs',
                       all_nonneg, ''))

        # 4. CSV shape: 6 waveforms x 4 controllers x 8 metrics
        checks.append(('CSV = 6 waveforms x 4 controllers x 8 metrics',
                       len(rows) == 6 * 4 * 8, f'{len(rows)} rows'))

        failed = False
        for label, ok, detail in checks:
            status = 'PASS' if ok else 'FAIL'
            failed |= not ok
            print(f'  [{status}] {label}'
                  + (f'  ({detail})' if detail else ''))
        print('RESULT: ' + ('ALL CHECKS PASSED.' if not failed else
                            f'{sum(1 for _, ok, _ in checks if not ok)} CHECK(S) FAILED — see above.'))
        sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
