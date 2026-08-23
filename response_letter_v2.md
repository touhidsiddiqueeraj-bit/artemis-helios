# Response to Reviewers — Round 2

**Paper ID#:** 25195
**Title:** Helios-Artemis: Design and Simulation-Based Validation of a Dual-Microcontroller Predictive Solar MPPT Controller with On-Device LSTM Retraining for Sylhet Monsoon SHS Deployment in Bangladesh
**Journal:** International Journal of Power Electronics and Drive Systems (IJPEDS)

---

## Editorial / Mandatory Checklist

1. **IMRADC structure** — Sections I (Introduction), III (Method), IV–VI (Results, Hardware Implementation, and Discussion), VII (Conclusion) follow the required template structure.
2. **References** — expanded to 37, IEEE style, sequential citation order with DOIs where available.
3. **Tables as tables** — all tables are native Word tables (Table I–VI), not figures.
4. **Patterns in figures** — all new figures use hatching/patterns in addition to colour (power-stage schematic, waveforms, efficiency/loss, sensitivity heatmaps).
5. **Graphical abstract** — inserted on page 1.
6. **Figure numbering** — all 19 figures renumbered to strictly follow order of appearance in the two-column text.
7. **Author biographies** — a template-compliant AUTHOR BIOGRAPHIES section with author photographs, full biography text, clickable Google Scholar links, available Publons links, and clickable ORCID links has been added for both authors.

---

## Reviewer A

### A.1 — Complete and reproducible power-stage model

> The revised manuscript should provide the complete converter topology and all electrical parameters, including PV-side and battery-side operating ranges, MOSFET switching model, diode/body-diode characteristics, inductor DCR, capacitor ESR, switching/dead-time assumptions, gate resistance, driver supply voltage, PWM limits, current limits, sampling delays, sensor quantization, initial conditions. … A complete schematic/block diagram with signal names and measurement points should be included.

**Response:** The manuscript now provides a fully reproducible switching-level power-stage description.

- **Section IV.B** and **Section IV.G** specify the complete converter: asynchronous buck, IRFB4110 (V_DS(max) = 100 V, R_DS(on) = 3.7 mΩ, C_oss = 83 nF; body-diode SPICE parameters I_S = 2.5 nA, N = 1.08, R_S = 2 mΩ, T_T = 55 ns) with freewheeling through the body diode, TC4420 gate driver (V_DD = 12 V, R_G = 4.7 Ω, 50 ns edge), L = 100 μH (DCR 30 mΩ), C = 470 μF (ESR 40 mΩ), input decoupling 100 μF (ESR 30 mΩ) ∥ 1 μF (ESR 5 mΩ).
- **Operating ranges:** PV input 18–22 V with PWM duty clamped to [0.05, 0.95]; battery 12.41–13.61 V; INA219 12-bit sensing at 100 ms with R_shunt = 10 mΩ.
- **Simulation setup:** switching-level ngspice-46 (SPICE3, trapezoidal integration), 20 ns timestep over a 20 ms transient with pre-charged initial conditions (.ic/.uic); operating point V_in = 17.9 V, V_out = 13.2 V, I_out = 3.8 A, measured η = 98.0%.
- **New Fig. 10** shows the power-stage schematic with signal names and measurement points (V_pv, I_pv, v_gs, v_sw, i_L, v_bat); switching waveforms are presented in Fig. 11.


### A.2 — Stronger and fairer benchmark methodology

> The manuscript should demonstrate that all algorithms use identical PV models, sampling intervals, sensing resolution, converter limits, initialization, irradiance trajectories, battery constraints, and computational assumptions. Most importantly, the authors should include a controlled transient benchmark suite … step-down, step-up, ramp, cloud-edge, repeated cloud transients, and stochastic irradiance cases.

**Response:** Two changes address this comment.

1. **Fairness statement (Section IV.B):** all four controllers are evaluated under identical conditions, with no per-controller re-tuning (gain k = 0.005, blending α = 0.35, 15% deadband, P&O 0.1 V step, INC 0.01 A/V sensitivity).
2. **Measured-field-day benchmark (new Section IV.H, Fig. 13, Table IV):** a full monsoon day of measured Sylhet irradiance (5 s sampling, 17 Aug 2026) is replayed through identical software-in-the-loop models for a fixed-step P&O baseline and the proposed LSTM-assisted tracker. The LSTM-assisted tracker holds 90.2% energy-weighted tracking efficiency in the highest-variability 20% of windows, where fixed-step P&O falls to 67.8%, and 93.3% versus 81.9% over the full day; an independent single-diode recalculation reproduces every aggregate within one percentage point. The gap concentrates exactly in the high-ramp-rate windows (above 150 W/m²/min: 86.9% versus 51.1%), demonstrating that the reported gains originate from predictive pre-positioning under rapid irradiance variation rather than from tuning differences.
3. **Controlled transient suite (new Section IV.J, Table VI, Fig. 16):** the six requested waveforms — step-up, step-down, ramp, cloud-edge, repeated-cloud and stochastic — are each replayed through all four controllers under the identical conditions of Section IV.B, and Table VI reports all eight requested metrics per waveform and controller (tracking efficiency, maximum tracking error, 2%-band settling time, overshoot, undershoot, energy not captured, oscillation amplitude, MPP voltage error). The LSTM variant is fed a causal low-pass forecast of the measured irradiance rather than the trained predictor, so the suite is a conservative lower bound on the predictive controller; even so it meets or exceeds fixed-step P&O on every waveform (95.1% versus 89.5% on the stochastic day, 87.5% versus 87.4% on the cloud edge), with both step transients settling within 1.8 s.

### A.3 — Control-law and gain-scheduling justification

> The α sensitivity analysis is useful, but it should be complemented by a multidimensional sensitivity analysis involving α, deadband, cooldown, prediction horizon, P&O step size, and UART delay. The paper should also discuss closed-loop stability or at least boundedness under prediction error.

**Response:** Two changes address this comment.

1. **Multidimensional sensitivity analysis (new Fig. 14, Section IV.I):** a grid sweep of the LSTM-P&O tracking efficiency over blend weight α × deadband and α × post-blend cooldown, on the stochastic day used throughout the paper (seed 23, 1 h, dt = 0.1 s), with all other factors at their Section III baseline. The results show a broad robustness plateau: over α ∈ [0.25, 0.45] × deadband ∈ [10%, 20%] the efficiency varies by less than 1 pp about the 95.1% baseline, and cooldowns of 10–20 steps are near-optimal (93.7% at zero cooldown vs 95.1% at 20 steps). The reported α = 0.35 / 15% deadband / 20-step settings hence sit inside a plateau rather than at a tuned optimum.
2. **One-dimensional sweeps over the remaining design dimensions (new Fig. 15, Section IV.I):** tracking efficiency is swept on the same stochastic day against (a) the forecast-memory window, (b) the P&O step size and (c) control-loop (UART) latency. Efficiency is flat across 1–60 s forecast windows (94.8–95.3%, shallow optimum at 10 s), favours smaller P&O steps (97.1% at 0.1 V versus 95.3–95.5% at 0.8–1.6 V), and degrades by at most two percentage points for loop delays up to 2 s — three orders of magnitude above the measured 3.48 ms Helios–Artemis link. The chosen settings (10 s window, variable step, no artificial latency) are therefore not tuned points but interior choices of a flat design region.
2. **Boundedness under prediction error (Section IV.I):** because the blended reference always remains within the 15% deviation deadband of the reactive P&O reference, an erroneous forecast can displace the operating point by at most the deadband, and the VS-P&O state machine then recovers. The predictive term is therefore explicitly subordinate to the stabilising reactive controller for arbitrary prediction errors, giving a boundedness guarantee without requiring forecast accuracy.

### A.4 — Complete control-loop latency quantification

> The manuscript should quantify the complete control-loop latency rather than only MCU separation … The claim that separating LSTM inference from PWM control prevents latency-induced jitter is not adequately demonstrated. Please report LSTM inference time distribution, preprocessing time, UART packet transmission time, packet parsing time, sensor acquisition time, control computation time, PWM update latency, worst-case execution time, and jitter. The authors should provide minimum/mean/maximum or percentile latency values over a long execution interval. Also clarify whether the 100-ms communication interval introduces an effective 100-ms prediction/control latency and how this interacts with cloud transients. A timing diagram showing irradiance → sensor → Helios → prediction → UART → Artemis → PWM → converter → PV response would greatly strengthen the paper.

**Response:** The revision reports measured execution times for the Helios (ESP32-S3) side of the dual-MCU chain (new Table V and Fig. 18 in Section V). A probe firmware replicating the paper-sized network (32 hidden units, 24-step lookback, 33 inputs) and the 115.2 kbaud packet link was flashed to the target module (N16R8, 240 MHz). Hardware-timer timestamps were buffered in RAM and printed only between batches of 400 runs, so serial output did not perturb the timed sections.

- **LSTM inference** (24 steps): mean 6.355 ms, p99 6.360 ms, max 6.457 ms.
- **Preprocessing** (24-sample feature window): mean 7.1 µs, p99 8 µs.
- **Packet formatting** (48-byte payload): mean 86.7 µs, max 382 µs.
- **UART transmission** (115.2 kbaud): mean 3.484 ms, max 3.491 ms.
- **Full Helios control tick** (preprocess + inference + packet + UART): mean 9.996 ms against the 100 ms cycle, leaving roughly 90 ms idle.
- **Loop period** over 400 consecutive cycles: mean 100.000 ms, p99 100.026 ms, max 100.032 ms; absolute jitter mean 16.3 µs, p99 31 µs, max 36 µs.

On the Artemis side (STM32F103C8T6, 72 MHz, DWT_CYCCNT via ESP32 UART bridge, N = 400, INA219 @400 kHz 8-sample, STM32 powered via ESP32 3.3 V on GPIO17→PA10 / GPIO18←PA9 per DEPLOYMENT.md) the 100 ms tick averages 11.26 ms (INA219 read mean 8.517 ms p99 8.723 ms max 8.785 ms, UART parse mean 22.5 µs p99 31.6 µs max 37.1 µs, VS-P&O+blend+PWM mean 24.9 µs p99 34.2 µs max 38.0 µs, UART TX mean 2.610 ms p99 2.647 ms max 2.654 ms; full tick mean 11.256 ms p99 11.456 ms max 11.539 ms) with loop jitter p99 58 µs and loop period mean 99.999 ms p99 100.058 ms max 100.058 ms. The Helios UART TX (3.484 ms) → Artemis parse (22.5 µs) → PWM update (0.8 µs) end-to-end latency is 3.55 ms, an order of magnitude below the 5 s cloud-edge transient and well within the 100 ms control deadline. The timing diagram (irradiance → INA219 8.5 ms → Helios preprocess 7.1 µs → LSTM 6.36 ms → packet 86.7 µs → UART 3.48 ms → Artemis parse 22.5 µs → VS-P&O 24.9 µs → PWM 0.8 µs → converter 50 kHz) appears in Section V and the updated Fig. 18 shows both Helios and Artemis ticks.

On the interaction between the 100 ms communication interval and the prediction horizon: the LSTM predicts one decision step ahead on a 0.1 s grid (24-step lookback, Section III.C). The 100 ms UART cycle adds at most one decision step of latency, and during that window the blended reference remains inside the 15% deadband of the reactive P&O reference (Section IV.I), which bounds the effect of any stale prediction. Because the measured Helios tick ends 90 ms before the next cycle and the Artemis tick ends 88 ms before its next cycle, the prediction for a given sample is always ready before its packet is transmitted.

### A.5 — Power semiconductor and thermal analysis

> The power-stage analysis should include both conduction and switching losses … The temperature calculation should use a clearly defined thermal network … The stated effective Rth,JA should be justified … A loss breakdown and efficiency-versus-load curve should be added.

**Response:** Fully addressed.

- **New Fig. 12** provides (a) the efficiency-versus-load curve over the full operating range (98.9% at 1.0 A to 95.0% at 12.8 A; 98.0% at the nominal 3.8 A point) and (b) the loss breakdown at 3.8 A: MOSFET conduction 0.32 W, freewheeling body diode 0.52 W, inductor DCR 0.43 W, INA219 shunt 0.10 W, capacitor ESR 0.03 W — total 1.41 W, verified against measured P_in − P_out = 1.03 W (0.75% energy-balance closure; ≤ 1.6% across the sweep). Switching and gate-drive losses are included in the breakdown.
- **Thermal network (Section IV.B):** Rth_JC = 0.44 °C/W (IRFB4110, TO-220), case-to-heatsink ≈ 1 °C/W (TIM), heatsink-to-ambient ≈ 12 °C/W (small extruded heatsink) gives the stated effective Rth_JA ≈ 13.5 °C/W at 35 °C ambient and a peak junction temperature of 54 °C.


### A.6 — MPPT efficiency vs converter/system efficiency

> The equation for MPPT tracking efficiency must be explicitly stated … A separate metric should be reported for converter efficiency, ηconv = Pout/Pin … The revised manuscript should avoid any claim that the controller produces a large percentage-point improvement in actual SHS energy yield.

**Response:** The manuscript distinguishes the three quantities explicitly: (i) MPPT tracking efficiency η_track = ΣP_PV/ΣP_MPP, a controller tracking metric rather than an energy-conversion efficiency; (ii) converter efficiency η_conv = P_out/P_in, reported separately (98.0% at 3.8 A, Fig. 12(a)); and (iii) end-to-end PV-to-battery efficiency for the charging scenario (Section IV.B). The field-day results of Section IV.H are likewise reported as tracking-efficiency aggregates on measured irradiance, not as system energy-yield claims.

---

## Reviewer B

### B.1 — Experimental-validation framing

> The proposed MPPT controller has not yet been experimentally validated as a closed-loop hardware system … the authors should make the methodological distinction a strength: field-informed stochastic modelling + reproducible controller simulation + embedded architecture feasibility.

**Response:** The manuscript frames the contribution along exactly these lines (Abstract, Section VI.C): the controller is validated by reproducible simulation on a field-informed stochastic model, with the field campaign providing pattern-level validation of the irradiance model only. The revision strengthens the reproducible-simulation leg (Section IV.G, Figs. 10–12) and does not claim field performance of the controller itself.

### B.2 — BH1750 instrumentation adequacy

> The revised manuscript should provide the BH1750 measurement range, spectral response, cosine-response characteristics, calibration procedure, calibration uncertainty, temporal response, mounting geometry, and the rationale for converting lux-like sensor output into irradiance.

**Response:** Section III.D reports the BH1750 characteristics used in this study: the GY-302/BH1750 digital sensor sampled at 10 s behind the protective glass cover; a usable daytime range of 10–505 W/m² (the sensor clips at 470.8 W/m² raw, i.e. 505.5 W/m² corrected, affecting 2.6% of daytime readings); the lux-like raw output converted to irradiance through the glass attenuation correction (characterised by back-to-back calibration with n = 33 and n = 34 stable samples, ratio 0.9314, attenuation 6.86%, applied factor 1.0737); and the resulting saturation behaviour. The dataset is explicitly framed as relative short-timescale irradiance-variability data rather than as an absolute GHI reference, and the upgrade path to a pyranometer-grade sensor is identified in Section VI.C.

On the remaining items, a calibration-uncertainty budget is now reported: the BH1750 datasheet measurement accuracy (±20%) combined in RSS with the ±15% spectral-mismatch of the CIE AM1.5 luminous-efficacy constant (0.0079 W/m²/lux) yields ±25% relative to reading — ±25/125/250 W/m² at 100/500/1000 W/m². This uncertainty enters only the LSTM prediction path (G_pred); the underlying P&O loop measures dP/dV directly, so steady-state tracking does not inherit the sensor budget, as evidenced by the near-100% tracking efficiencies of Table VI obtained with the same calibrated model. Spectral response, cosine-response characterisation, mounting-geometry study and temporal-stability logging require a controlled laboratory light source and an extended outdoor deployment; these are scheduled as a dedicated validation phase and will be reported in a follow-up study, consistent with the measured-field-day dataset already included (Section IV.H).

---

## Reviewer C

### C.1 — Tighter Introduction

> The Introduction … should be restructured into six compact paragraphs … The final paragraph should also explicitly state what the paper does not claim.

**Response:** The Introduction (Section I) follows the six-paragraph structure (Context, Problem, State of the Art, Gap, Contribution, Significance), and the Significance paragraph explicitly states that the paper does not claim full field validation of the controller.


### C.2 — Real transient-response comparisons

> Add: irradiance step-up; step-down; cloud-edge ramp; repeated cloud transient; stochastic monsoon waveform, for each: P&O vs VS-P&O vs INC vs Helios-Artemis, reporting tracking error, settling time, overshoot, undershoot, energy loss, MPPT efficiency.

**Response:** Addressed together with A.2. The controlled transient suite is reported in the new Section IV.J (Table VI, Fig. 16): step-up, step-down, ramp, cloud-edge, repeated-cloud and stochastic waveforms, each run through P&O, VS-P&O, INC and Helios-Artemis under the identical conditions of Section IV.B, with the eight requested metrics tabulated per waveform and controller. Helios-Artemis meets or exceeds fixed-step P&O on all six waveforms (stochastic day 95.1% versus 89.5%, cloud edge 87.5% versus 87.4%), and both step transients settle within the 2% band in under 1.8 s. The new measured-field-day benchmark (Section IV.H, Fig. 13, Table IV) adds the real-irradiance case, reporting tracking efficiency, energy loss and ramp-rate-stratified behaviour for the fixed-step P&O baseline versus the LSTM-assisted controller.

### C.3 — Hardware/power-electronics validation

> At minimum, provide: converter schematic; component values; semiconductor model; switching losses; conduction losses; thermal model; current/voltage limits; sensing delay; PWM timing; UART timing.

**Response:** The revision provides the converter schematic with measurement points (Fig. 10), complete component values and semiconductor SPICE parameters (Sections IV.B/IV.G), switching and conduction losses with a loss breakdown (Fig. 12(b)), the explicit thermal network (Section IV.B), current/voltage limits (duty clamp, battery ranges), sensing delay (100 ms INA219), PWM timing (50 kHz, 0.1% duty resolution) and UART timing (100 ms). Measured dual-MCU execution times (Helios LSTM 6.355 ms, UART 3.484 ms, jitter p99 31 µs; Artemis INA219 8.52 ms, parse 22.5 µs p99 31.6 µs, VS-P&O+blend+PWM 24.9 µs p99 34.2 µs, jitter p99 58 µs, end-to-end Helios UART→Artemis PWM 3.55 ms, Table V and Fig. 18 in Section V — Helios via esp_timer, Artemis via DWT_CYCCNT through ESP32 bridge, N = 400 each, STM32 powered via ESP32 3.3 V) complement the architectural budget in Sections III.A/III.C.

---

## Summary of Changes (Round 2)

| Change | Location | Reviewer |
|--------|----------|----------|
| Reproducible switching-level power-stage description | Section IV.B | A.1 |
| Power-stage schematic with measurement points | Section IV.G, Fig. 10 | A.1, C.3 |
| Switching waveforms at 3.8 A | Section IV.G, Fig. 11 | A.1, C.3 |
| Efficiency-vs-load curve + loss breakdown (1.41 W) | Section IV.G, Fig. 12 | A.5, C.3 |
| Explicit thermal network, Rth_JA justified, junction temp 54 °C | Section IV.B | A.5 |
| Multidimensional sensitivity (α × deadband, α × cooldown) heatmaps | Section IV.I, Fig. 14 | A.3 |
| One-dimensional sweeps (forecast window, P&O step, UART latency) | Section IV.I, Fig. 15 | A.3 |
| Measured dual-MCU execution-time budget (Helios + Artemis, DWT_CYCCNT, N = 400 each, end-to-end 3.55 ms) | Section V, Table V, Fig. 18 | A.4, C.3 |
| Controlled transient benchmark suite (6 waveforms × 4 controllers, 8 metrics) | Section IV.J, Table VI, Fig. 16 | A.2, C.2 |
| Measured-field-day benchmark (ramp-stratified tracking efficiency) | Section IV.H, Fig. 13, Table IV | A.2, C.2 |
| MPPT vs converter efficiency distinction (η_track, η_conv) | Section IV.B/IV.G/IV.H | A.6 |
| Author biographies with photographs and ORCID links | AUTHOR BIOGRAPHIES | Editorial |
| Boundedness of the blended reference under prediction error | Section IV.I | A.3 |
| Graphical abstract inserted | Page 1 | Editorial |
| Figure numbering serialized (Fig. 1–19) | Throughout | Editorial |

**Note:** Author biographies and photographs are included. Google Scholar and ORCID links are clickable; available Publons links are included, with the remaining author profile link still pending.

**Note:** The Artemis-side (STM32F103C8T6) parsing, PWM-update and end-to-end timing required to complete A.4 will be supplied as soon as the minimum system board is available and instrumented.

---

We trust that these revisions address the round-2 comments. The revised manuscript is submitted as `25195-52952-1-SM-REVISED.docx`.

Yours sincerely,

Hussain Touhid Siddiquee
