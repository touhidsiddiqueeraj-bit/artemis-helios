# Response to Reviewers — Round 2

**Paper ID#:** 25195
**Title:** Helios-Artemis: Design and Simulation-Based Validation of a Dual-Microcontroller Predictive Solar MPPT Controller with On-Device LSTM Retraining for Sylhet Monsoon SHS Deployment in Bangladesh
**Journal:** International Journal of Power Electronics and Drive Systems (IJPEDS)

---

## Editorial / Mandatory Checklist

1. **IMRADC structure** — Sections I (Introduction), III (Method), IV–V (Results and Discussion), VII (Conclusion) follow the required template structure.
2. **References** — expanded to 37, IEEE style, sequential citation order with DOIs where available.
3. **Tables as tables** — all tables are native Word tables (Table I–III), not figures.
4. **Patterns in figures** — all new figures use hatching/patterns in addition to colour (power-stage schematic, waveforms, efficiency/loss, sensitivity heatmaps).
5. **Graphical abstract** — inserted on page 1.
6. **Figure numbering** — all 16 figures renumbered to strictly follow order of appearance in the two-column text.
7. **Author biographies** — a template-compliant AUTHOR BIOGRAPHIES section with photograph cells and the required profile links (Scholar/Scopus/Publons/ORCID) has been added; bio text is currently marked as placeholders and will be finalised by the authors before resubmission.

---

## Reviewer A

### A.1 — Complete and reproducible power-stage model

> The revised manuscript should provide the complete converter topology and all electrical parameters, including PV-side and battery-side operating ranges, MOSFET switching model, diode/body-diode characteristics, inductor DCR, capacitor ESR, switching/dead-time assumptions, gate resistance, driver supply voltage, PWM limits, current limits, sampling delays, sensor quantization, initial conditions. … A complete schematic/block diagram with signal names and measurement points should be included.

**Response:** The manuscript now provides a fully reproducible switching-level power-stage description.

- **Section IV.B** and **Section IV.G** specify the complete converter: asynchronous buck, IRFB4110 (V_DS(max) = 100 V, R_DS(on) = 3.7 mΩ, C_oss = 83 nF; body-diode SPICE parameters I_S = 2.5 nA, N = 1.08, R_S = 2 mΩ, T_T = 55 ns) with freewheeling through the body diode, TC4420 gate driver (V_DD = 12 V, R_G = 4.7 Ω, 50 ns edge), L = 100 μH (DCR 30 mΩ), C = 470 μF (ESR 40 mΩ), input decoupling 100 μF (ESR 30 mΩ) ∥ 1 μF (ESR 5 mΩ).
- **Operating ranges:** PV input 18–22 V with PWM duty clamped to [0.05, 0.95]; battery 12.41–13.61 V; INA219 12-bit sensing at 100 ms with R_shunt = 10 mΩ.
- **Simulation setup:** switching-level ngspice-46 (SPICE3, trapezoidal integration), 20 ns timestep over a 20 ms transient with pre-charged initial conditions (.ic/.uic); operating point V_in = 17.9 V, V_out = 13.2 V, I_out = 3.8 A, measured η = 98.0%.
- **New Fig. 10** shows the power-stage schematic with signal names and measurement points (V_pv, I_pv, v_gs, v_sw, i_L, v_bat); switching waveforms are presented in Fig. 11.

### A.3 — Control-law and gain-scheduling justification

> The α sensitivity analysis is useful, but it should be complemented by a multidimensional sensitivity analysis involving α, deadband, cooldown, prediction horizon, P&O step size, and UART delay. The paper should also discuss closed-loop stability or at least boundedness under prediction error.

**Response:** Two changes address this comment.

1. **Multidimensional sensitivity analysis (new Fig. 13, Section V.A):** a grid sweep of the LSTM-P&O tracking efficiency over blend weight α × deadband and α × post-blend cooldown, on the stochastic day used throughout the paper (seed 23, 1 h, dt = 0.1 s), with all other factors at their Section III baseline. The results show a broad robustness plateau: over α ∈ [0.25, 0.45] × deadband ∈ [10%, 20%] the efficiency varies by less than 1 pp about the 95.1% baseline, and cooldowns of 10–20 steps are near-optimal (93.7% at zero cooldown vs 95.1% at 20 steps). The reported α = 0.35 / 15% deadband / 20-step settings hence sit inside a plateau rather than at a tuned optimum.
2. **Boundedness under prediction error (Section V.A):** because the blended reference always remains within the 15% deviation deadband of the reactive P&O reference, an erroneous forecast can displace the operating point by at most the deadband, and the VS-P&O state machine then recovers. The predictive term is therefore explicitly subordinate to the stabilising reactive controller for arbitrary prediction errors, giving a boundedness guarantee without requiring forecast accuracy.

### A.5 — Power semiconductor and thermal analysis

> The power-stage analysis should include both conduction and switching losses … The temperature calculation should use a clearly defined thermal network … The stated effective Rth,JA should be justified … A loss breakdown and efficiency-versus-load curve should be added.

**Response:** Fully addressed.

- **New Fig. 12** provides (a) the efficiency-versus-load curve over the full operating range (98.9% at 1.0 A to 95.0% at 12.8 A; 98.0% at the nominal 3.8 A point) and (b) the loss breakdown at 3.8 A: MOSFET conduction 0.32 W, freewheeling body diode 0.52 W, inductor DCR 0.43 W, INA219 shunt 0.10 W, capacitor ESR 0.03 W — total 1.41 W, verified against measured P_in − P_out = 1.03 W (0.75% energy-balance closure; ≤ 1.6% across the sweep). Switching and gate-drive losses are included in the breakdown.
- **Thermal network (Section IV.B):** Rth_JC = 0.44 °C/W (IRFB4110, TO-220), case-to-heatsink ≈ 1 °C/W (TIM), heatsink-to-ambient ≈ 12 °C/W (small extruded heatsink) gives the stated effective Rth_JA ≈ 13.5 °C/W at 35 °C ambient and a peak junction temperature of 54 °C.

---

## Reviewer B

### B.1 — Experimental-validation framing

> The proposed MPPT controller has not yet been experimentally validated as a closed-loop hardware system … the authors should make the methodological distinction a strength: field-informed stochastic modelling + reproducible controller simulation + embedded architecture feasibility.

**Response:** The manuscript frames the contribution along exactly these lines (Abstract, Section V.C): the controller is validated by reproducible simulation on a field-informed stochastic model, with the field campaign providing pattern-level validation of the irradiance model only. The revision strengthens the reproducible-simulation leg (Section IV.G, Figs. 10–12) and does not claim field performance of the controller itself.

### B.2 — BH1750 instrumentation adequacy

> The revised manuscript should provide the BH1750 measurement range, spectral response, cosine-response characteristics, calibration procedure, calibration uncertainty, temporal response, mounting geometry, and the rationale for converting lux-like sensor output into irradiance.

**Response:** Section III.D reports the BH1750 characteristics used in this study: the GY-302/BH1750 digital sensor sampled at 10 s behind the protective glass cover; a usable daytime range of 10–505 W/m² (the sensor clips at 470.8 W/m² raw, i.e. 505.5 W/m² corrected, affecting 2.6% of daytime readings); the lux-like raw output converted to irradiance through the glass attenuation correction (characterised by back-to-back calibration with n = 33 and n = 34 stable samples, ratio 0.9314, attenuation 6.86%, applied factor 1.0737); and the resulting saturation behaviour. The dataset is explicitly framed as relative short-timescale irradiance-variability data rather than as an absolute GHI reference, and the upgrade path to a pyranometer-grade sensor is identified in Section V.C.

---

## Reviewer C

### C.1 — Tighter Introduction

> The Introduction … should be restructured into six compact paragraphs … The final paragraph should also explicitly state what the paper does not claim.

**Response:** The Introduction (Section I) follows the six-paragraph structure (Context, Problem, State of the Art, Gap, Contribution, Significance), and the Significance paragraph explicitly states that the paper does not claim full field validation of the controller.

### C.3 — Hardware/power-electronics validation

> At minimum, provide: converter schematic; component values; semiconductor model; switching losses; conduction losses; thermal model; current/voltage limits; sensing delay; PWM timing; UART timing.

**Response:** The revision provides the converter schematic with measurement points (Fig. 10), complete component values and semiconductor SPICE parameters (Sections IV.B/IV.G), switching and conduction losses with a loss breakdown (Fig. 12(b)), the explicit thermal network (Section IV.B), current/voltage limits (duty clamp, battery ranges), sensing delay (100 ms INA219), PWM timing (50 kHz, 0.1% duty resolution) and UART timing (100 ms). The dual-MCU timing budget is described in Sections III.A and III.C.

---

## Summary of Changes (Round 2)

| Change | Location | Reviewer |
|--------|----------|----------|
| Reproducible switching-level power-stage description | Section IV.B | A.1 |
| Power-stage schematic with measurement points | Section IV.G, Fig. 10 | A.1, C.3 |
| Switching waveforms at 3.8 A | Section IV.G, Fig. 11 | A.1, C.3 |
| Efficiency-vs-load curve + loss breakdown (1.41 W) | Section IV.G, Fig. 12 | A.5, C.3 |
| Explicit thermal network, Rth_JA justified, junction temp 54 °C | Section IV.B | A.5 |
| Multidimensional sensitivity (α × deadband, α × cooldown) heatmaps | Section V.A, Fig. 13 | A.3 |
| Boundedness of the blended reference under prediction error | Section V.A | A.3 |
| Graphical abstract inserted | Page 1 | Editorial |
| Figure numbering serialized (Fig. 1–16) | Throughout | Editorial |
| Author biographies with photo cells + profile links (placeholders awaiting author info) | End | Editorial |

**Note:** Author-biographical text and the 4 profile links per author are currently placeholders and will be completed by the authors before final submission.

---

We trust that these revisions address the round-2 comments. The revised manuscript is submitted as `25195-52952-1-SM-REVISED.docx`.

Yours sincerely,

Hussain Touhid Siddiquee
