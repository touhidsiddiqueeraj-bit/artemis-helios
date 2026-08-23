"""
run_buck.py — ngspice driver for the Helios-Artemis buck power stage (1a)
========================================================================
Switching-level simulation, IRFB4110 + TC4420 + 100uH/470uF, 50 kHz.
Runs the netlist, parses the binary .raw, computes per-device losses
(MOSFET, body-diode, inductor DCR, cap ESR, shunt), energy balance,
and efficiency-vs-load. Produces Fig. 15 waveforms and results CSV.

Usage:
    python3 run_buck.py            # main operating point + sweep + figures
    python3 run_buck.py --check    # self-checks (energy balance, ranges)
"""

import os
import subprocess
import sys
import tempfile

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
NETLIST = os.path.join(HERE, "buck_power_stage.cir")
OUTDIR = os.path.join(HERE, "results")
FIGDIR = os.path.join(os.path.dirname(HERE), "..", "Figures")

RLOAD = 3.46      # ohm, nominal charging operating point (13.5 V / 3.9 A)
VIN = 17.9        # V, panel Vmp (50 Wp)
VOUT = 13.5       # V, battery charging target
FSW = 50e3
R_DCR = 0.030     # ohm, inductor DCR
R_ESR = 0.040     # ohm, output cap ESR
R_SHUNT = 0.010   # ohm, INA219 shunt


def run_case(vin=VIN, rload=RLOAD, sim_ms=20.0, fsw=FSW):
    """Run ngspice with substituted params, return parsed raw dict."""
    dut = VOUT / vin if VOUT < vin else 0.9
    params = {"VIN": vin, "VOUT": VOUT, "FSW": fsw,
              "DUTY": dut, "RLOAD": rload, "TEND": sim_ms / 1000.0}
    with open(NETLIST) as f:
        text = f.read()
    for k, v in params.items():
        text = text.replace("{" + k + "}", str(v))
    raw_path = os.path.join(tempfile.gettempdir(), "buck_out.raw")
    if os.path.exists(raw_path):
        os.remove(raw_path)
    with tempfile.NamedTemporaryFile("w", suffix=".cir", delete=False) as tf:
        tf.write(text)
        net = tf.name
    try:
        subprocess.run(["ngspice", "-b", "-r", raw_path, net],
                       check=True, capture_output=True)
    finally:
        os.remove(net)
    return parse_raw(raw_path)


def parse_raw(path):
    """Parse ngspice binary .raw into {name: ndarray}."""
    with open(path, "rb") as f:
        head = f.read(16384)
    marker = b"Binary:\n"
    hdr = head[:head.find(marker)].decode("ascii", "ignore")
    nvars = 0
    names = []
    in_vars = False
    for ln in hdr.splitlines():
        if ln.startswith("No. Variables:"):
            nvars = int(ln.split(":")[1])
        if ln.strip() == "Variables:":
            in_vars = True
            continue
        if in_vars and nvars and len(names) < nvars:
            parts = ln.split()
            if len(parts) >= 2 and parts[0].isdigit():
                names.append(parts[1])
            continue
        if in_vars and nvars and len(names) == nvars:
            break
    data = np.fromfile(path, dtype="<f8", offset=len(hdr) + len(marker))
    data = data.reshape(-1, nvars)
    return {n: data[:, i] for i, n in enumerate(names)}


def metrics(d, rload=RLOAD):
    """Per-device losses and efficiency over the steady-state tail."""
    m = d["time"] > 0.9 * d["time"][-1]
    t, vsw, vpvd, vpv = (d["time"][m], d["v(sw)"][m], d["v(pvd)"][m], d["v(pv)"][m])
    i_cin = d["v(esr_in)"][m] / 0.030
    i_ccer = d["v(esr_cr)"][m] / 0.005
    i_m1 = d["i(lstry)"][m] - i_cin - i_ccer
    i_d1 = d["i(l1)"][m] - i_m1
    il = d["i(l1)"][m]
    ipv, iload = d["i(vpv)"][m], d["v(bat)"][m] / rload

    p_in = -float(np.mean(d["v(pvin)"][m] * d["i(vpv)"][m]))
    p_out = float(np.mean(d["v(bat)"][m] * iload))
    p_mos = float(np.mean((vpvd - vsw) * i_m1))
    p_diode = float(np.mean(-vsw * i_d1))
    p_dcr = float(np.sqrt(np.mean(il ** 2)) ** 2 * R_DCR)
    p_esr = float(np.sqrt(np.mean((d["v(ce)"][m] / R_ESR) ** 2)) ** 2 * R_ESR)
    p_shunt = float(np.sqrt(np.mean(ipv ** 2)) ** 2 * R_SHUNT)
    p_cin = (float(np.sqrt(np.mean(i_cin ** 2)) ** 2 * 0.030)
             + float(np.sqrt(np.mean(i_ccer ** 2)) ** 2 * 0.005))
    loss_sum = p_mos + p_diode + p_dcr + p_esr + p_shunt + p_cin

    return {
        "vin": float(np.mean(d["v(pvin)"][m])),
        "vout": float(np.mean(d["v(bat)"][m])),
        "vout_ripple_mv": 1e3 * (d["v(bat)"][m].max() - d["v(bat)"][m].min()),
        "i_in": float(np.mean(ipv)),
        "i_out": float(np.mean(iload)),
        "il_ripple_a": float(d["i(l1)"][m].max() - d["i(l1)"][m].min()),
        "p_in": p_in, "p_out": p_out, "eta_pct": 100.0 * p_out / p_in,
        "p_mos_w": p_mos, "p_diode_w": p_diode, "p_dcr_w": p_dcr,
        "p_esr_w": p_esr, "p_shunt_w": p_shunt, "p_cin_w": p_cin,
        "loss_sum_w": loss_sum,
        "balance_err_pct": 100.0 * (loss_sum - (p_in - p_out)) / p_in,
        "vsw_max": float(d["v(sw)"][m].max()),
    }


def fig15_waveforms(d, out):
    """Fig. 15: gate/switch node, inductor current, output ripple, input."""
    m = d["time"] > 0.9 * d["time"][-1]
    t = d["time"][m] * 1e3
    i_sw = int(np.argmin(np.abs(t - t[0] - 1.0)))  # steady-state point
    p = slice(i_sw - 200, i_sw + 4800)  # ~5 switching periods (100 us at 20 ns)
    x = (t[p] - t[p][0]) * 1e3  # us since window start
    fig, ax = plt.subplots(2, 2, figsize=(3.5, 2.5))
    ax[0, 0].plot(x, d["v(sw)"][m][p], lw=0.8)
    ax[0, 0].plot(x, d["v(g)"][m][p] - d["v(sw)"][m][p], lw=0.8)
    ax[0, 0].set_ylabel("V (V)"); ax[0, 0].set_title("(a) $v_{sw}$ and $v_{gs}$")
    ax[1, 0].plot(x, d["i(l1)"][m][p], lw=0.8)
    ax[1, 0].set_ylabel("A"); ax[1, 0].set_xlabel("t (\u00b5s)")
    ax[1, 0].set_title("(b) Inductor current $i_L$")
    v_ripple = 1e3 * (d["v(bat)"][m][p] - d["v(bat)"][m][p].mean())
    ax[0, 1].plot(x, v_ripple, lw=0.8)
    ax[0, 1].set_ylabel("mV"); ax[0, 1].set_title("(c) Output ripple (AC)")
    ax[1, 1].plot(x, d["i(vpv)"][m][p], lw=0.8)
    ax[1, 1].set_ylabel("A"); ax[1, 1].set_xlabel("t (\u00b5s)")
    ax[1, 1].set_title("(d) Input (PV) current")
    for a in ax.flat:
        a.grid(alpha=0.3)
        a.tick_params(labelsize=6)
        a.title.set_size(6)
        a.xaxis.label.set_size(6.5)
        a.yaxis.label.set_size(6.5)
    fig.tight_layout(pad=0.4)
    fig.savefig(out, dpi=600)
    plt.close(fig)


def sweep():
    """Efficiency vs load: R_load from 1.0 to 13.5 ohm (I_out ~1-13 A)."""
    rows = []
    for rl in [1.0, 1.5, 2.0, 2.5, 3.0, 3.46, 4.5, 6.0, 8.0, 11.0, 13.5]:
        d = run_case(rload=rl)
        mm = metrics(d, rload=rl)
        mm["rload"] = rl
        rows.append(mm)
        print(f"R_load={rl:5.1f} ohm  I_out={mm['i_out']:5.2f} A  "
              f"eta={mm['eta_pct']:6.2f}%  balance={mm['balance_err_pct']:+.2f}%")
    return rows


def fig16_eff(rows, out):
    """Fig. 16: efficiency-vs-load + loss breakdown (patterns, not colors)."""
    rows = sorted(rows, key=lambda r: r["rload"])
    i_out = np.array([r["i_out"] for r in rows])
    eta = np.array([r["eta_pct"] for r in rows])
    fig, ax = plt.subplots(1, 2, figsize=(3.5, 1.7))
    ax[0].plot(i_out, eta, "o-", ms=4, lw=1.2)
    ax[0].tick_params(labelsize=6)
    ax[0].title.set_size(6)
    ax[0].xaxis.label.set_size(6.5)
    ax[0].yaxis.label.set_size(6.5)
    ax[0].set_xlabel("Output current (A)"); ax[0].set_ylabel("Efficiency (%)")
    ax[0].set_title("(a) \u03b7 vs $I_{out}$")
    ax[0].grid(alpha=0.3)
    r = rows[len(rows) // 2]
    labels = ["MOSFET", "Diode", "Inductor", "Cap", "Shunt", "Cin"]
    vals = [r["p_mos_w"], r["p_diode_w"], r["p_dcr_w"], r["p_esr_w"],
            r["p_shunt_w"], r["p_cin_w"]]
    hatches = ["", "//", "xx", "..", "\\\\", "**"]
    bars = ax[1].bar(range(len(vals)), [1000 * v for v in vals], color="#9db4c0",
                     edgecolor="k", hatch=hatches)
    for b, v in zip(bars, vals):
        ax[1].text(b.get_x() + b.get_width() / 2, 1000 * v + 12, f"{1000*v:.0f}",
                   ha="center", fontsize=6)
    ax[1].set_ylim(0, 1000 * max(vals) * 1.18)
    ax[1].set_xticks(range(len(vals))); ax[1].set_xticklabels(labels, rotation=22,
                                                               ha="right", fontsize=8)
    ax[1].tick_params(labelsize=5)
    ax[1].title.set_size(6)
    ax[1].yaxis.label.set_size(6.5)
    ax[1].set_ylabel("Loss (mW)"); ax[1].set_title(f"(b) Losses @ {r['i_out']:.1f} A")
    fig.tight_layout(pad=0.5)
    fig.subplots_adjust(top=0.88, bottom=0.22)
    fig.savefig(out, dpi=600, bbox_inches='tight', pad_inches=0.05)
    plt.close(fig)


def self_check():
    d = run_case()
    mm = metrics(d)
    assert 0.9 < mm["vout"] < 14.5, f"vout {mm['vout']}"
    assert 3.0 < mm["i_out"] < 5.0, f"i_out {mm['i_out']}"
    assert 95.0 < mm["eta_pct"] < 100.0, f"eta {mm['eta_pct']}"
    assert abs(mm["balance_err_pct"]) < 2.0, f"balance {mm['balance_err_pct']}"
    assert 0.5 < mm["il_ripple_a"] < 0.9, f"ripple {mm['il_ripple_a']}"
    assert mm["vsw_max"] < 40, f"vsw_max {mm['vsw_max']} (ringing?)"
    print("Self-check PASS: vout=%.2f V, i_out=%.2f A, eta=%.2f%%, "
          "balance err=%+.2f%%, ripple=%.3f A, vsw_max=%.1f V"
          % (mm["vout"], mm["i_out"], mm["eta_pct"], mm["balance_err_pct"],
             mm["il_ripple_a"], mm["vsw_max"]))


def main():
    os.makedirs(OUTDIR, exist_ok=True)
    os.makedirs(FIGDIR, exist_ok=True)
    d = run_case()
    mm = metrics(d)
    print(f"Main operating point: Vin={mm['vin']:.2f} V, Vout={mm['vout']:.2f} V, "
          f"I_in={mm['i_in']:.2f} A, I_out={mm['i_out']:.2f} A")
    print(f"eta={mm['eta_pct']:.2f}%  P_in={mm['p_in']:.2f} W  P_out={mm['p_out']:.2f} W")
    print(f"losses: mos={1000*mm['p_mos_w']:.0f} mW diode={1000*mm['p_diode_w']:.0f} "
          f"dcr={1000*mm['p_dcr_w']:.0f} esr={1000*mm['p_esr_w']:.0f} "
          f"shunt={1000*mm['p_shunt_w']:.0f} cin={1000*mm['p_cin_w']:.0f} "
          f"| sum={1000*mm['loss_sum_w']:.0f} vs {1000*(mm['p_in']-mm['p_out']):.0f} mW")
    fig15_waveforms(d, os.path.join(FIGDIR, "fig15_buck_waveforms.png"))
    rows = sweep()
    fig16_eff(rows, os.path.join(FIGDIR, "fig16_eff_loss.png"))
    import csv
    with open(os.path.join(OUTDIR, "buck_operating_points.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print("Saved: Figures/fig15_buck_waveforms.png, Figures/fig16_eff_loss.png, "
          "results/buck_operating_points.csv")


if __name__ == "__main__":
    if "--check" in sys.argv:
        self_check()
    else:
        main()
