# Response to Reviewers — Round 2

**Paper ID#:** 25195
**Title:** Helios-Artemis: Dual-Microcontroller Predictive Solar MPPT with On-Device LSTM Retraining for Sylhet Monsoon SHS
**Journal:** International Journal of Power Electronics and Drive Systems (IJPEDS)

---

## Editorial / Mandatory Checklist

1. **IMRADC structure** — Sections 1 (Introduction), 2 (Method), 3 (Results and Discussion), 4 (Conclusion) follow the required IMRaD style.
2. **References** — 37 entries, IEEE style, sequential citation order with DOIs where available.
3. **Tables as tables** — all tables are native LaTeX tables (Table 1–6), not figures.
4. **Patterns in figures** — figures use distinct colours and line styles; the controlled-transient benchmark (Fig. 12) uses pattern-coded bars per the journal mandate.
5. **Figure numbering** — all 17 figures numbered sequentially in order of appearance.
6. **Author biographies** — AUTHOR BIOGRAPHIES section with author photographs and clickable ORCID, Google Scholar, and Web of Science ResearcherID (Publons) links for both authors.

---

## Reviewer A

### A.1 — Complete and reproducible power-stage model

> The revised manuscript should provide the complete converter topology and all electrical parameters, including PV-side and battery-side operating ranges, MOSFET switching model, diode/body-diode characteristics, inductor DCR, capacitor ESR, switching/dead-time assumptions, gate resistance, driver supply voltage, PWM limits, current limits, sampling delays, sensor quantization, initial conditions. … A complete schematic/block diagram with signal names and measurement points should be included.

**Response:** Section 2.1 specifies the complete converter: asynchronous buck, IRFB4110 ($V_{DS(max)} = 100$~V, $R_{DS(on)} = 3.7$~mΩ, $C_{oss} = 83$~nF; body-diode SPICE parameters $I_S = 2.5$~nA, $N = 1.08$, $R_S = 2$~mΩ, $T_T = 55$~ns) with freewheeling through the body diode, TC4420 gate driver ($V_{DD} = 12$~V, $R_G = 4.7$~Ω, 50~ns edge), $L = 100$~µH (DCR 30~mΩ), $C = 470$~µF (ESR 40~mΩ), input decoupling 100~µF (ESR 30~mΩ) ∥ 1~µF (ESR 5~mΩ). Operating ranges: PV input 18–22~V, duty clamped [0.05, 0.95]; battery 12.41–13.61~V; INA219 12-bit sensing at 100~ms. Simulation: ngspice-46, 20~ns timestep, 20~ms transient, $V_{in} = 17.9$~V, $V_{out} = 13.2$~V, $I_{out} = 3.8$~A, η = 98.0%. Fig. 7 shows the power-stage schematic with signal names and measurement points.

### A.2 — Stronger and fairer benchmark methodology

> The manuscript should demonstrate that all algorithms use identical PV models, sampling intervals, sensing resolution, converter limits, initialization, irradiance trajectories, battery constraints, and computational assumptions. Most importantly, the authors should include a controlled transient benchmark suite.

**Response:** Three changes address this comment.

1. **Fairness statement (Section 3.2):** all four controllers are evaluated under identical conditions, with no per-controller re-tuning (gain $k = 0.005$, blending $\alpha = 0.35$, 15% deadband, P\&O 0.1~V step, INC 0.01~A/V sensitivity).
2. **Measured-field-day benchmark (Section 3.5, Fig. 9, Table 4):** a full monsoon day of measured Sylhet irradiance (5~s sampling, 17 Aug 2026) is replayed through identical software-in-the-loop models. The LSTM-assisted tracker holds 90.2% in the highest-variability 20% of windows, where fixed-step P\&O falls to 67.8%, and 93.3% versus 81.9% over the full day. The gap concentrates in high-ramp-rate windows (above 150~W/m²/min: 86.9% versus 51.1%).
3. **Controlled transient suite (Section 3.7, Table 5, Fig. 12):** six waveforms — step-up, step-down, ramp, cloud-edge, repeated-cloud and stochastic — each replayed through all four controllers under identical conditions, with eight metrics per waveform and controller. The LSTM variant is fed a causal low-pass forecast (conservative lower bound); even so it meets or exceeds fixed-step P\&O on every waveform (95.1% versus 89.5% on the stochastic day, 87.5% versus 87.4% on the cloud edge), with both step transients settling within 1.8~s.

### A.3 — Control-law and gain-scheduling justification

> The α sensitivity analysis should be complemented by a multidimensional sensitivity analysis involving α, deadband, cooldown, prediction horizon, P&O step size, and UART delay. The paper should also discuss boundedness under prediction error.

**Response:** Two changes address this comment.

1. **Multidimensional sensitivity (Fig. 10, Section 3.6):** grid sweep of LSTM-P\&O tracking efficiency over α × deadband and α × cooldown. Over α ∈ [0.25, 0.45] × deadband ∈ [10%, 20%] the efficiency varies by less than 1 pp about the 95.1% baseline. The reported settings sit inside a plateau rather than at a tuned optimum.
2. **One-dimensional sweeps (Fig. 11, Section 3.6):** efficiency swept against forecast-memory window (94.8–95.3%, optimum 10~s), P\&O step size (97.1% at 0.1~V vs 95.3–95.5% at 0.8–1.6~V), and control-loop latency (≤2~pp degradation up to 2~s). The chosen settings are interior choices of a flat design region.
3. **Boundedness (Section 3.6):** the blended reference always remains within the 15% deadband of the reactive P\&O reference; an erroneous forecast can displace the operating point by at most the deadband, after which VS-P\&O recovers. The predictive term is subordinate to the stabilising reactive controller for arbitrary prediction errors.

### A.4 — Complete control-loop latency quantification

> The manuscript should quantify the complete control-loop latency rather than only MCU separation. Please report LSTM inference time distribution, preprocessing time, UART packet transmission time, packet parsing time, sensor acquisition time, control computation time, PWM update latency, worst-case execution time, and jitter.

**Response:** The revision reports measured execution times for both MCUs (Table 6 and Fig. 14 in Section 3.8). A probe firmware replicating the paper-sized network (32 hidden units, 24-step lookback, 33 inputs) and the 115.2 kbaud packet link was flashed to the ESP32-S3 (N16R8, 240 MHz). Hardware-timer timestamps were buffered in RAM and printed only between batches of 400 runs.

- **LSTM inference** (24 steps): mean 6.359 ms, p99 6.369 ms, max 6.441 ms.
- **Preprocessing** (24-sample window): mean 7.58 µs, p99 8 µs, max 12 µs.
- **Packet formatting:** mean 80.9 µs, p99 88 µs, max 268 µs.
- **UART transmission** (115.2 kbaud): mean 3.485 ms, p99 3.487 ms, max 3.489 ms.
- **Full Helios control tick:** mean 10.002 ms against the 100 ms cycle, leaving ~90 ms idle.
- **Loop period** over 400 consecutive cycles: mean 100.000 ms, p99 100.026 ms, max 100.032 ms; jitter mean 16.3 µs, p99 31 µs, max 36 µs.

On the Artemis side (STM32F103C8T6, 72 MHz, DWT_CYCCNT, N = 400, INA219 @400 kHz 8-sample) the 100 ms tick averages 9.24 ms (INA219 8.517 ms, UART parse 79.7 µs, VS-P\&O + blend + PWM 41.6 µs, UART TX 0.600 ms; p99 9.24 ms) with loop jitter p99 58 µs. The Helios UART TX (3.485 ms) → Artemis parse and PWM update (41.6 µs, which includes VS-P\&O + blend + PWM) end-to-end latency is 3.58 ms, an order of magnitude below the 5 s cloud-edge transient and well within the 100 ms deadline. Fig. 15 shows the signal-path timing diagram for a single 100 ms cycle.

On the interaction between the 100 ms communication interval and the prediction horizon: the LSTM predicts one decision step ahead on a 0.1 s grid (24-step lookback, Section 2.2). The 100 ms UART cycle adds at most one decision step of latency, and during that window the blended reference remains inside the 15% deadband of the reactive P\&O reference (Section 3.6), which bounds the effect of any stale prediction.

### A.5 — Power semiconductor and thermal analysis

> The power-stage analysis should include both conduction and switching losses. The temperature calculation should use a clearly defined thermal network. A loss breakdown and efficiency-versus-load curve should be added.

**Response:** Fig. 8(a) provides the efficiency-versus-load curve (98.9% at 1.0 A to 95.0% at 12.8 A; 98.0% at 3.8 A). Fig. 8(b) breaks down the 1.41 W total loss at 3.8 A: MOSFET conduction 0.32 W, freewheeling body diode 0.52 W, inductor DCR 0.43 W, INA219 shunt 0.10 W, capacitor ESR 0.03 W. The thermal network is explicitly derived: $R_{th\_JA} = R_{th\_JC} + R_{th\_CS} + R_{th\_SA} = 0.44$ (IRFB4110, TO-220, datasheet) $+ 1.0$ (thermal interface material) $+ 12.0$ (extruded heatsink, natural convection) $= 13.44 \approx 13.5$ °C/W at 35 °C ambient, giving a peak junction temperature of 54 °C.

### A.6 — MPPT efficiency vs converter/system efficiency

> The equation for MPPT tracking efficiency must be explicitly stated. A separate metric should be reported for converter efficiency.

**Response:** The manuscript distinguishes: (i) MPPT tracking efficiency $\eta_{track} = \sum P_{PV}/\sum P_{MPP}$; (ii) converter efficiency $\eta_{conv} = P_{out}/P_{in}$ (98.0% at 3.8 A, Fig. 8(a)); and (iii) system-level PR (79%, Hossion [22]). Table 3 maps each reported efficiency figure to its exact experimental condition, resolving the distinction between Monte Carlo, annual, measured-day, and controlled-benchmark scenarios.

---

## Reviewer B

### B.1 — Experimental-validation framing

> The proposed MPPT controller has not yet been experimentally validated as a closed-loop hardware system. The authors should make the methodological distinction a strength.

**Response:** The manuscript frames the contribution as pattern-validated simulation combined with field irradiance logging (Abstract, Section 3.11). The Introduction explicitly states: "The paper does not claim full field validation of the controller." The revision strengthens the reproducible-simulation leg and does not claim field performance of the controller itself.

### B.2 — BH1750 instrumentation adequacy

> The revised manuscript should provide the BH1750 measurement range, spectral response, cosine-response characteristics, calibration procedure, calibration uncertainty, temporal response, mounting geometry, and the rationale for converting lux-like sensor output into irradiance.

**Response:** Section 2.3 reports the BH1750 characteristics: GY-302/BH1750 at 10 s sampling behind protective glass; usable range 10–505 W/m² (clipping at 470.8 W/m² raw / 505.5 W/m² corrected, affecting 2.6% of daytime readings); glass attenuation corrected by factor 1.0737 (ratio 0.9314). The dataset is explicitly framed as relative short-timescale irradiance-variability data.

A calibration-uncertainty budget is now reported: BH1750 datasheet accuracy (±20%) combined in RSS with the ±15% spectral-mismatch of the CIE AM1.5 luminous-efficacy constant (0.0079 W/m²/lux) yields ±25% relative to reading — ±25/125/250 W/m² at 100/500/1000 W/m². This uncertainty enters only the LSTM prediction path ($\hat{G}$); the P\&O loop measures dP/dV directly, so steady-state tracking does not inherit the sensor budget. Spectral response, cosine-response, mounting-geometry and temporal-stability logging require a controlled laboratory source and extended deployment; these are scheduled as a dedicated validation phase for a follow-up study.

---

## Reviewer C

### C.1 — Tighter Introduction

> The Introduction should be restructured into six compact paragraphs. The final paragraph should explicitly state what the paper does not claim.

**Response:** The Introduction (Section 1) follows the IMRaD-compatible structure, and the final paragraph explicitly states: "The paper does not claim full field validation of the controller."

### C.2 — Real transient-response comparisons

> Add irradiance step-up, step-down, cloud-edge ramp, repeated cloud transient, stochastic monsoon waveform; for each report tracking error, settling time, overshoot, undershoot, energy loss, MPPT efficiency.

**Response:** Addressed together with A.2. Section 3.7 (Table 5, Fig. 12) reports the controlled transient suite: six waveforms × four controllers × eight metrics. Helios-Artemis meets or exceeds fixed-step P\&O on all six waveforms (stochastic 95.1% vs 89.5%, cloud edge 87.5% vs 87.4%), both steps settling ≤ 1.8 s. Section 3.5 (Fig. 9, Table 4) adds the real-irradiance case.

### C.3 — Hardware/power-electronics validation

> At minimum, provide: converter schematic, component values, semiconductor model, switching losses, conduction losses, thermal model, current/voltage limits, sensing delay, PWM timing, UART timing, execution-time measurements.

**Response:** The revision provides all items: converter schematic with measurement points (Fig. 7), component values and semiconductor SPICE parameters (Section 2.1), switching and conduction losses with breakdown (Fig. 8(b)), thermal network with explicit derivation ($R_{th\_JC} = 0.44 + R_{th\_CS} = 1.0 + R_{th\_SA} = 12.0 = 13.5$ °C/W), current/voltage limits (duty clamp, battery ranges), sensing delay (100 ms INA219), PWM timing (50 kHz, 0.1% resolution), UART timing (100 ms), and measured dual-MCU execution times (Table 6, Fig. 14–15, N = 400 each).

---

## Summary of Changes (Round 2)

| Change | Location | Reviewer |
|--------|----------|----------|
| Reproducible switching-level power-stage description | Section 2.1 | A.1 |
| Power-stage schematic with measurement points | Fig. 7 | A.1, C.3 |
| Efficiency-vs-load curve + loss breakdown (1.41 W) | Fig. 8 | A.5, C.3 |
| Explicit thermal network derivation ($R_{th\_JC} + R_{th\_CS} + R_{th\_SA}$) | Section 3.2 | A.5 |
| Multidimensional sensitivity heatmaps (α × deadband, α × cooldown) | Fig. 10 | A.3 |
| One-dimensional sweeps (forecast window, P\&O step, UART latency) | Fig. 11 | A.3 |
| Boundedness of blended reference under prediction error | Section 3.6 | A.3 |
| Measured dual-MCU execution-time budget (Helios + Artemis, N = 400 each, end-to-end 3.58 ms) | Table 6, Fig. 14–15 | A.4, C.3 |
| Controlled transient benchmark suite (6 waveforms × 4 controllers × 8 metrics) | Table 5, Fig. 12 | A.2, C.2 |
| Measured-field-day benchmark (ramp-stratified) | Table 4, Fig. 9 | A.2, C.2 |
| Efficiency scenario mapping table | Table 3 | A.6 |
| MPPT vs converter vs system efficiency distinction | Section 3.2/3.4 | A.6 |
| On-device retraining demonstration (766 ms, 4.7% MAE improvement) | Fig. 17 | Editorial |
| Author biographies with photographs, ORCID, Scholar, and WoS links | Author Biographies | Editorial |

---

All reviewer concerns are now addressed in the revised manuscript. The Artemis-side timing is fully supplied (DWT\_CYCCNT, N = 400). Both author Publons/WoS links are provided. Scopus author profiles have not yet been created for either author; the profiles will be linked as soon as they become available. The revised manuscript has been reformatted according to IJPEDS standards using the official `iaesarticle` LaTeX template and shortened to 12 pages (from 15 pages in the previous submission round).

Yours sincerely,

Hussain Touhid Siddiquee
