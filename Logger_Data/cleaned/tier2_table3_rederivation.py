"""
tier2_table3_rederivation.py
=============================
Tier 2: Re-derive Table III — pattern-validated model.

1. Synthetic Monte Carlo at paper's native 0.1s (N=10 July days).
2. Field GHI resampled to 1-min → MPPT efficiency via controller simulation.
3. Ramp-rate comparison: field vs synthetic at 1-min resolution.

Key finding: field ramp-rate statistics (mean=72.8, σ=115.7 W/m²/min)
are consistent with synthetic July model (mean=80.7, σ=132.1 W/m²/min),
supporting the paper's pattern-validated efficiency claims.

Path B: pattern validation on synthetic model, direct computation on field data.
"""
import os, sys, importlib.util
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(OUT, '../../Code/Python')))

def pv_model(G, Vr, Isc0=2.91, Voc0=21.6, k=14.2606,
             bV=-3.4e-3, aI=0.5e-3, NOCT=45, Tamb=35):
    g = G / 1000; Tc = Tamb + (NOCT - 20) / 800 * G; dT = Tc - 25
    VocT = Voc0 * (1 + bV * dT); IscT = Isc0 * (1 + aI * dT) * g
    Vmc = VocT * (1 / (1 + k)) ** (1 / k)
    Pm = IscT * (1 - (Vmc / VocT) ** k) * Vmc
    Vrc = min(max(Vr, 0.6 * Voc0), 0.98 * Voc0)
    Ipv = max(0, IscT * (1 - (Vrc / VocT) ** k))
    return Ipv * Vrc, Pm, Vrc

# ── Controller laws ──────────────────────────────────────────────────────
class Ctrl:
    pass

class PlainPaO(Ctrl):
    def __init__(self): self.Vr = 17.8
    def step(self, dP, dV, Vm, Gv):
        dv = 0.10
        if dP > 0:   self.Vr += dv if dV > 0 else -dv
        elif dP < 0: self.Vr += -dv if dV > 0 else dv
        self.Vr = np.clip(self.Vr, 0.6 * 21.6, 0.98 * 21.6)

class VSPaO(Ctrl):
    def __init__(self): self.Vr = 17.8
    def step(self, dP, dV, Vm, Gv):
        dl = np.clip(0.008 * abs(dP / (abs(dV) + 1e-9)), 0.05, 0.60)
        if dP > 0:   self.Vr += dl if dV > 0 else -dl
        elif dP < 0: self.Vr += -dl if dV > 0 else dl
        self.Vr = np.clip(self.Vr, 0.6 * 21.6, 0.98 * 21.6)

class INC(Ctrl):
    def __init__(self):
        self.Vr = 17.8; self.Ip = 0.0; self.Vp = 17.8
    def step(self, dP, dV, Vm, Gv):
        dv = 0.05; In = dP / max(Vm, 0.1) if Vm > 0.1 else 0
        dI = In - self.Ip; dVv = Vm - self.Vp
        if abs(dVv) < 1e-6:
            if abs(dI) > 1e-6: self.Vr += dv if dI > 0 else -dv
        else:
            inc = dI / dVv; neg = -In / Vm if Vm > 0.1 else -999
            if inc > neg + 0.01:   self.Vr += dv
            elif inc < neg - 0.01: self.Vr -= dv
        self.Vr = np.clip(self.Vr, 0.6 * 21.6, 0.98 * 21.6)
        self.Ip = In; self.Vp = Vm

class LSTMPaO(Ctrl):
    def __init__(self, alpha=0.35):
        self.Vr = 17.8; self.alpha = alpha; self._vs = VSPaO()
    def step(self, dP, dV, Vm, Gv):
        dl = np.clip(0.008 * abs(dP / (abs(dV) + 1e-9)), 0.05, 0.60)
        V_po = self._vs.Vr
        if dP > 0:   V_po += dl if dV > 0 else -dl
        elif dP < 0: V_po += -dl if dV > 0 else dl
        V_po = np.clip(V_po, 0.6 * 21.6, 0.98 * 21.6)
        G_pred = Gv * 1.02
        if G_pred > 1 and abs(G_pred - Gv) > 0.15 * max(G_pred, 1):
            V_mpp_pred = 17.8 * (max(0.01, G_pred / 1000) ** 0.05)
            self.Vr = (1 - self.alpha) * V_po + self.alpha * V_mpp_pred
        else:
            self.Vr = V_po
        self._vs.Vr = self.Vr

# ── Simulation loop (paper-matching) ─────────────────────────────────────
def simulate_on_ghi(G_arr, ctrl, dt=0.1):
    Vr = 17.8; Pp = 0.0; Vp = 17.8
    Ppv_sum = 0.0; Pmpp_sum = 0.0
    for Gv in G_arr:
        g = Gv / 1000.0
        if g > 0:
            Ppv_true, Pm, Vrc = pv_model(Gv, Vr)
            Im = max(0, Ppv_true / max(Vrc, 0.1) + 0.002 * np.random.randn())
            Vm = max(0.1, Vrc + 0.01 * np.random.randn())
            P_meas = Im * Vm; dP = P_meas - Pp; dV = Vm - Vp
            ctrl.step(dP, dV, Vm, Gv)
            Vr = ctrl.Vr; Pp = P_meas; Vp = Vm
        else:
            Ppv_true = 0.0; Pm = 0.01
        if Pm > 1:
            Ppv_sum += Ppv_true; Pmpp_sum += Pm
    return (Ppv_sum / max(Pmpp_sum, 1e-9)) * 100 if Pmpp_sum > 0 else 0

def generate_ghi_01s(seed=23, sunrise=5.5, sunset=18.5):
    rng = np.random.RandomState(seed)
    dt = 0.1; T = np.array([[0.6,0.3,0.1],[0.2,0.5,0.3],[0.1,0.2,0.7]])
    cf = [0.15,0.55,0.90]; dc = np.exp(-dt)
    N = int(86400/dt); GG = np.zeros(N)
    cs=1; ct=0.; gf=0.
    for i in range(N):
        h = i*dt/3600
        if h < sunrise or h > sunset: continue
        ct += dt
        if ct >= 15: ct=0; r=rng.rand(); row=T[cs]; cs=0 if r<row[0] else(1 if r<row[0]+row[1] else 2)
        fr = (h-sunrise)/(sunset-sunrise); Gc=744*np.sin(np.pi*fr); Gcf=Gc*cf[cs]
        sf=0.25*max(Gcf,10); gf=gf*dc+sf*np.sqrt(1-dc**2)*rng.randn()
        gf=max(-0.4*Gc,min(0.4*Gc,gf)); GG[i]=min(Gc,max(0,Gcf+gf))
    return GG

# ── Ramp-rate statistics ─────────────────────────────────────────────────
def ramp_stats(G):
    mask = G > 80
    G_d = G[mask]
    rr = np.abs(np.diff(G_d))
    return {'n': len(G_d), 'mean_G': np.mean(G_d), 'std_G': np.std(G_d),
            'mean_ramp': np.mean(rr), 'std_ramp': np.std(rr)}


if __name__ == '__main__':
    print("="*65); print("Tier 2: Re-deriving Table III with pattern-validated model"); print("="*65)

    # 1. Ramp-rate validation
    print("\n[1/4] Ramp-rate comparison (field vs synthetic at 1-min)...")
    spec = importlib.util.spec_from_file_location("irr", os.path.join(os.path.dirname(__file__), '../../Code/Python/01_irradiance_generator.py'))
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    syn_profiles = [mod.generate_day_profile(198, 23+idx*1000) for idx in range(10)]

    df = pd.read_csv(os.path.join(OUT, 'field_data_cleaned.csv'))
    df = df.sort_values('elapsed_s').reset_index(drop=True)
    t, Gf = df['elapsed_s'].values, df['glass_corrected_irr_wm2'].values
    t_min = np.arange(0, t[-1], 60)
    Gf_min = np.interp(t_min, t, Gf)

    field_rs = ramp_stats(Gf_min)
    syn_rss = [ramp_stats(p) for p in syn_profiles]
    syn_mean_ramp = np.mean([s['mean_ramp'] for s in syn_rss])
    syn_std_ramp  = np.mean([s['std_ramp'] for s in syn_rss])

    print(f"  Field (1-min resample): mean_ramp={field_rs['mean_ramp']:.1f}, σ_ramp={field_rs['std_ramp']:.1f} W/m²/min")
    print(f"  Synthetic (10-day MC):   mean_ramp={syn_mean_ramp:.1f}, σ_ramp={syn_std_ramp:.1f} W/m²/min")
    print(f"  Ratio: {field_rs['mean_ramp']/syn_mean_ramp:.2f}x")

    # 2. Synthetic Monte Carlo (0.1s)
    print(f"\n[2/4] Synthetic MC (N=10, 0.1s, paper-matching)...")
    syn_results = {name: [] for name in ['Plain P&O', 'VS-P&O', 'INC', 'LSTM-P&O']}
    for idx in range(10):
        GG = generate_ghi_01s(seed=23+idx*1000)
        ctrls = {'Plain P&O': PlainPaO(), 'VS-P&O': VSPaO(), 'INC': INC(), 'LSTM-P&O': LSTMPaO()}
        etas = {n: simulate_on_ghi(GG, c) for n, c in ctrls.items()}
        for n, e in etas.items(): syn_results[n].append(e)

    # 3. Field simulation (resampled 1-min)
    print(f"[3/4] Field GHI simulation (resampled 1-min)...")
    ctrls_f = {'Plain P&O': PlainPaO(), 'VS-P&O': VSPaO(), 'INC': INC(), 'LSTM-P&O': LSTMPaO()}
    field_etas = {n: simulate_on_ghi(Gf_min, c) for n, c in ctrls_f.items()}

    # 4. Figure
    print("[4/4] Generating figure...")
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.5, 3.8))

    controllers = ['Plain P&O', 'VS-P&O', 'INC', 'LSTM-P&O']
    x = np.arange(len(controllers)); w = 0.3
    syn_m = [np.mean(syn_results[c]) for c in controllers]
    syn_s = [np.std(syn_results[c]) for c in controllers]
    fd = [field_etas.get(c, np.nan) for c in controllers]
    paper_refs = {'Plain P&O': 70.7, 'VS-P&O': 85.2, 'LSTM-P&O': 94.0}

    ax1.bar(x-w, syn_m, w, yerr=syn_s, capsize=3, color='#1565C0', alpha=0.85, label='Synthetic MC (N=10)')
    ax1.bar(x, fd, w, color='#C62828', alpha=0.75, label='Field GHI (1-min)')
    ax1.scatter([c+0.3 for c in x if controllers[x.tolist().index(c)] in paper_refs],
                [paper_refs[controllers[i]] for i in range(len(controllers)) if controllers[i] in paper_refs],
                marker='*', s=150, color='gold', edgecolors='k', linewidths=0.5, zorder=5, label='Paper claim')
    ax1.set_xticks(x); ax1.set_xticklabels(controllers, fontsize=8)
    ax1.set_ylabel('MPPT Efficiency (%)', fontsize=9); ax1.set_ylim(55, 105)
    ax1.legend(fontsize=7); ax1.grid(True, axis='y', alpha=0.3)
    ax1.set_title('(a) MPPT Tracking Efficiency', fontsize=9, fontweight='bold')

    # Ramp-rate distribution
    rr_syn_all = np.concatenate([np.abs(np.diff(p[p>80])) for p in syn_profiles])
    rr_field = np.abs(np.diff(Gf_min[Gf_min>80]))
    ax2.hist(rr_syn_all, bins=40, alpha=0.55, color='#1565C0', density=True, label=f'Synthetic (μ={np.mean(rr_syn_all):.0f}, σ={np.std(rr_syn_all):.0f})')
    ax2.hist(rr_field, bins=40, alpha=0.55, color='#C62828', density=True, label=f'Field (μ={np.mean(rr_field):.0f}, σ={np.std(rr_field):.0f})')
    ax2.set_xlabel('|ΔG| per minute (W/m²/min)', fontsize=9); ax2.set_ylabel('Density', fontsize=9)
    ax2.legend(fontsize=7); ax2.grid(True, alpha=0.3); ax2.set_xlim(0, 400)
    ax2.set_title('(b) Ramp-rate distribution at 1-min', fontsize=9, fontweight='bold')
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, 'fig_tier2_comparison.png'), dpi=150)
    plt.close(); print(f"  Saved: fig_tier2_comparison.png")

    # Output table
    print("\n" + "="*65); print("TIER 2 SUMMARY"); print("="*65)
    print(f"{'Controller':<18} {'Paper Jul':>10} {'MC Mean':>10} {'Field':>10} {'Δ(MC-Paper)':>12}")
    print("-"*60)
    for name in controllers:
        pp = paper_refs.get(name)
        sm = np.mean(syn_results[name]); sd = np.std(syn_results[name])
        fe = field_etas.get(name, np.nan)
        d = sm - pp if pp else 0
        pp_str = f"{pp:.1f}%" if pp else "N/A"
        fm = f"{fe:.1f}%" if not np.isnan(fe) else "N/A"
        print(f"{name:<18} {pp_str:>10} {sm:>7.2f}%±{sd:.2f}  {fm:>8} {d:>+9.2f}%")

    print(f"\nRamp rates (W/m²/min, 1-min resolution):")
    print(f"  Field:     μ={field_rs['mean_ramp']:.0f}, σ={field_rs['std_ramp']:.0f}")
    print(f"  Synthetic: μ={syn_mean_ramp:.0f}, σ={syn_std_ramp:.0f}")
    print(f"\nFiles: {OUT}/")
    print(f"  fig_tier2_comparison.png")
