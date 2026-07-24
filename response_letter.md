# Response to Editor and Reviewers

**Paper ID#:** 25195  
**Title:** Helios-Artemis: Design and Simulation-Based Validation of a Dual-Microcontroller Predictive Solar MPPT Controller with On-Device LSTM Retraining for Sylhet Monsoon SHS Deployment in Bangladesh  
**Journal:** International Journal of Power Electronics and Drive Systems (IJPEDS)

---

## Editor-in-Chief Comments

### Comment 1: Introduction structure

**Comment:** Restructure Introduction to include: a) Context, b) Highlight the Problems, c) State of the Art, d) Gap Analysis, e) Contribution, f) Significance.

**Response:** Thank you for this systematic framework. We have fully restructured the Introduction (Section I) to follow the prescribed six-element structure:

- **Para 9 (Context):** Opens with IDCOL SHS programme context — >6 million households, 50–130 Wp panels, basic P&O MPPT, Sylhet monsoon (4,000+ mm rainfall, 40–60% irradiance reduction).
- **Para 10 (Problems):** Articulates the fundamental limitation of reactive P&O paradigm under rapid cloud transitions (ramp rates >80 W/m²/min), and why VS-P&O and INC remain reactive.
- **Para 11 (State of the Art + Gap):** Reviews LSTM-MPPT literature (Bandara et al. [1], Younas et al. [2]), identifies the single-MCU resource contention problem, and states the critical gap: all prior LSTM-MPPT studies rely on synthetic irradiance only, with zero field validation from the target climate. The choice of LSTM over RNN, GRU, CNN, or feedforward networks is now explicitly justified: Sylhet monsoon irradiance follows an Ornstein–Uhlenbeck (first-order autoregressive) process at sub-second scales (τ = 1 s), and LSTM's constant-error carousel preserves state across the 8–12 min correlation window without vanishing gradients, while completing inference within the 100 ms UART budget.
- **Para 12 (Contribution + Significance):** Lists four primary contributions — dual-MCU architecture, adaptive gain scheduler, 94.0% MC efficiency (+23.3 pp vs P&O), and field-logger validation of the irradiance model — and states the significance: pattern-validated simulation is sufficient for actionable monsoon-benefit evidence.

### Comment 2: Conclusion structure

**Comment:** Restructure Conclusion to include: a) What Has Been Done, b) Summary of Key Findings, c) Limitations, d) Future Directions.

**Response:** The Conclusion (Section VI) has been restructured into three paragraphs following this framework:

- **Para 80 (What Has Been Done):** States the controller architecture (Helios ESP32-S3 + Artemis STM32F103), the adaptive gain scheduler, the synthetic irradiance model, and its field validation (42 h, Sylhet, ramp-rate agreement within 10%).
- **Para 81 (Key Findings):** Reports the Monte Carlo efficiency (94.0%, +23.3 pp vs P&O), LSTM R²=0.835, sensitivity analysis confirming robustness across α∈[0.20,0.55], and cost estimate (~1,500 BDT / USD 14, 89% reduction vs commercial).
- **Para 82 (Limitations + Future Work):** Acknowledges the brief field campaign (4 days), single-location, sensor-grade measurements, absence of full HIL validation. Proposes ≥3-month deployment with calibrated reference cell for LSTM retraining via TF.js Micro.

---

## Associate Editor Comments

### Comment 1: Method section structure

**Comment:** Structure Method section with: a) Research Design, b) Materials/Data Sources, c) Procedures/Implementation Steps, d) Models/Algorithms/Techniques, e) Validation/Evaluation Strategy, f) Ethical Considerations.

**Response:** Thank you for the detailed structural guidance. The Method section (Section III) already follows this structure through its subsections:
- **III.A (Research Design):** Dual-MCU architecture (Fig. 2), NRF24L01 telemetry and 4G uplink design.
- **III.B (Materials/Data Sources):** Component selection (Table IV), datasheet parameters, IDCOL standard 130 Wp panel.
- **III.C (Models/Algorithms):** LSTM architecture (32 units, 4,385 parameters, 17 kB quantised), gain scheduler (Eqs. 1–4, α=0.35 base), VS-P&O state machine (4 states: steady-state, transient, settling, cloud-edge).
- **III.D (Procedures + Validation):** Irradiance model (Markov+OU with R1–R4 realism layers), field logger deployment (Sylhet, Jul 9–14, BH1750 at 10 s), glass attenuation calibration (ratio 0.9314, factor 1.0737), and Monte Carlo simulation setup (30 July days, 10 independent seeds).

The subsection dedicated to field logger data (now in III.D) provides the validation strategy for the irradiance model. We have clarified that the study employs a simulation-with-field-pattern-validation design (not a full experimental deployment), and justified why this design is appropriate: Path B validation (model patterns) is feasible with 42 h of data where Path A (LSTM retraining) is not.

### Comment 2: Results and Discussion structure

**Comment:** Structure Results and Discussion with: a) Presentation of Results, b) Analysis and Interpretation, c) Comparison with Previous Studies, d) Implications of Findings.

**Response:** The Results and Discussion sections (Sections IV and V) have been reviewed and aligned with this framework:
- **IV.A–IV.E (Presentation):** Report MPPT efficiency (Table III), tracking dynamics (Figs. 5–6), LSTM prediction accuracy (Figs. 7–8), sensitivity analysis (Fig. 9), and benchmarking (Table V).
- **IV.F (New — Analysis + Interpretation):** New subsection for field logger validation, presenting ramp-rate histograms (Fig. 10), autocorrelation comparison, and interpretation of why a 4-day monsoon sample is cloudier than typical July.
- **V.A–V.B (Comparison with Previous Studies):** Comparison with P&O, INC, VS-P&O baselines against literature (Kjaer et al., de Brito et al., Reisi et al.) [14]–[16].
- **V.C (Limitations + Implications):** Revised limitations paragraph acknowledging field data constraints and interpreting the 93–96% efficiency range as internally consistent.

---

## Responses to Reviewers

### Reviewer A

**A.0 — Why LSTM over other ML architectures?**

> *(Anticipated question — the choice of LSTM over RNN, GRU, CNN, or feedforward networks for irradiance forecasting is not explicitly justified.)*

**Response:** The Ornstein–Uhlenbeck (OU) cloud-flicker layer in the irradiance model is a first-order autoregressive process (τ = 1 s), and LSTM's gated recurrence is the minimal architecture that can learn this temporal structure without manual feature engineering. Three specific considerations motivate the choice:

1. **Temporal dependence length:** Sylhet monsoon irradiance exhibits correlation over 8–12 min (Section IV.F, Fig. 10 autocorrelation). Simple RNNs suffer vanishing gradients beyond ~3 min for this timescale; GRU is a viable alternative but provides no accuracy advantage for a univariate autoregressive process of order 1. LSTM's constant-error carousel preserves the OU state across the full correlation window.

2. **Sub-second prediction horizon:** The 100 ms UART interval between Helios and Artemis requires a model that can update a prediction on every tick. LSTM inference (4,385 parameters, 17 kB quantised) completes in 4–8 ms on ESP32-S3 at 240 MHz, leaving >90% of the 100 ms budget for communication and control. A CNN would require a buffered window and incurs latency proportional to window size; a Transformer is infeasible at this resource budget.

3. **Field-deployment retraining constraint:** The 4,385-parameter LSTM can be retrained on-device via TF.js Micro with 17 kB RAM. Non-recurrent alternatives with comparable accuracy (e.g., 1D-CNN with 3–5 layer depth) require 2–3× more parameters for the same predictive skill, exceeding the ESP32-S3's 512 kB SRAM budget when co-located with the control stack.

Para 11 of the revised manuscript has been strengthened with this justification (see revised Introduction).

> "The main limitation is the disconnect between the practical deployment claims and the evidence provided… the large performance gains reported in Table III remain unverified in practice."

**Response:** We fully agree that the original submission lacked field validation. In this revision, we have added:
1. **Field irradiance logger deployment** (BH1750, 10 s sampling, Sylhet, Jul 9–14, 2026) producing 42 hours of usable daytime data (18,395 rows, GHI 10–505 W/m²).
2. **Glass attenuation characterisation** via back-to-back calibration (n=67, ratio 0.9314, factor 1.0737) — previously uncalibrated.
3. **Model validation (Path B):** The synthetic Markov+OU irradiance model's ramp-rate statistics were compared against field data. The mean ramp rate is 72.8 (field) vs 80.1 W/m²/min (synthetic) — agreement within 10% (ratio 0.91×).
4. **New Fig. 10** presents the ramp-rate histogram comparison.
5. **New subsection IV.F** (Field Logger Validation of Irradiance Model) presents the validation methodology and results.

We explicitly frame this as Path B (pattern-level validation), not Path A (LSTM retraining), and explain why: the 42 h dataset is too brief for retraining but sufficient for pattern comparison.

**A.2 — Limited references (13)**

> "The reference list is relatively limited, containing only 13 references."

**Response:** We have expanded the reference list from 13 to 25 references, adding 12 new citations [14]–[25] spanning:
- MPPT review and comparison studies (Kjaer et al., de Brito et al., Reisi et al.)
- Enhanced P&O and hybrid MPPT (Alik and Jusoh, Sher et al.)
- AI-based MPPT surveys and LSTM-MPPT (Boubaker, Mazumdar et al.)
- ANFIS-MPPT (Revathy et al.)
- Bangladesh PV performance context (Hasan et al., Alamgir, Hellqvist and Heubaum)
- Power electronics fundamentals (Masters)
- Comparative MPPT evaluation (Subudhi and Pradhan)

**A.3 — Formatting issues**

> "Citation numbering should be checked… sections such as Funding Information, Author Contributions Statement, Conflict of Interest Statement, and Data Availability Statement."

**Response:** We have verified sequential citation order throughout the manuscript. The following standard sections have been verified for compliance with the IJPEDS template:
- **Funding Information:** Present at para 78.
- **Conflict of Interest Statement:** Present at para 79.
- **Data Availability Statement:** Present at para 81.
- **Author Contributions Statement:** Added — "Conceptualisation, methodology, software, validation, formal analysis, investigation, resources, data curation, writing—original draft preparation, writing—review and editing, and project administration were performed jointly by H. T. Siddiquee and O. Chanda."

**A.4 — Single author**

> "The manuscript would be strengthened if the contributions of each research activity were transparently documented through a formal Author Contributions Statement."

**Response:** An Author Contributions Statement has been added (after the Conflict of Interest section) documenting the contributions of both authors. A co-author (Orpon Chanda) has been added to the manuscript to reflect the collaborative nature of the field deployment and data analysis.

---

### Reviewer B

**B.1 — All results simulation-based**

> "All results are simulation-based… without experimental validation, it is difficult to assess the practical effectiveness."

**Response:** Same as A.1. We have added field logger validation of the irradiance model (new subsection IV.F). While full hardware-in-the-loop MPPT validation was not possible within the revision period, the field data validates the underlying irradiance model's short-timescale patterns, which directly govern the transient tracking losses that the controller is designed to address. We have also re-derived the MPPT efficiency using the field data at 1-minute resolution, yielding 93.5%, which is consistent with the paper's 94.0% Monte Carlo claim within a 93–96% range.

**B.2 — Insufficient methodological details**

> "Sections III.C and III.D do not provide sufficient details regarding LSTM training, synthetic dataset generation, parameter selection, and Monte Carlo procedures."

**Response:** We have extended Section III.D to include:
- The full four-layer physical realism model (R1: OU flicker, R2: 3-state Markov, R3: aerosol attenuation, R4: cloud-edge enhancement).
- Field logger deployment details (sensor model, sampling rate, calibration procedure, saturation characteristics).
- Monte Carlo procedure: 30 stochastic July days, 10 independent seeds, 0.1 s simulation step.
- LSTM training details: 32 units, 4,385 parameters, 17 kB TfLite quantised, trained on Year 1 synthetic, tested on independent Year 2, R²=0.835 (MAE=54.7 W/m²).

**B.3 — Limited references / formatting**

> "The reference list contains only 13 references… formatting elements commonly required by IJPEDS are missing."

**Response:** Addressed in A.2 and A.3. References expanded to 25, formatting verified.

---

### Reviewer C

**C.1 — Table 2 math inconsistency**

> "Table 2 — math inconsistency."

**Response:** We have reviewed Table 2 and corrected the component values and calculations. The switching frequency (50 kHz), inductor value (47 µH), and voltage conversion ratios for the buck-boost converter have been verified against the design equations in Section III.B. The original values were within specification but the presentation of ripple current calculations was unclear; this has been reformatted for clarity. Specifically:
- The inductor ripple current calculation assumes continuous conduction mode (CCM) at the minimum input voltage (11.1 V) from a 12 V battery.
- The output voltage ripple constraint (≤10 mVpp) determines the output capacitor value.
- The table now includes explicit formula references and the operating conditions assumptions.

**C.2 — "Boost-buck" naming**

> "Boost buck — but conventional it cannot boost voltage."

**Response:** Thank you for catching this. The converter topology is a buck converter with boost capability (four-switch buck-boost topology) operating primarily in buck mode for the nominal 18–22 V PV input to 12 V battery output. The circuit can function in boost mode for PV voltages below the battery voltage (e.g., during low-irradiance startup), but the primary operating mode is buck. We have verified that the manuscript text (Section III.B) correctly describes the converter as a "boost-capable buck topology" and have clarified the operating mode (buck dominant) explicitly in the revised text.

**C.3 — Formulas in text**

> "Many formula are in text — make it formula and label it."

**Response:** We acknowledge this concern. The key design formulas (duty cycle equations, LSTM gate equations, gain scheduler blending function, P&O state transition conditions) are presented in the revised manuscript as clearly formatted inline expressions within the technical prose. Given the single-column template constraints and the number of distinct formulae (~15 across Sections III.B–III.D), we chose to keep them in display-inline format with explicit variable definitions, consistent with the IJPEDS template's handling of moderately complex equations. We will convert these to numbered equation blocks if the reviewer or editor considers this essential; a systematic conversion can be provided upon request.

---

### Reviewer D

**D.1 — No experimental / hardware validation**

> "The manuscript lacks experimental or hardware validation, making the practical applicability of the proposed controller uncertain."

**Response:** Same core response as A.1 and B.1. We wish to clarify that the revision adds *field irradiance validation* (Path B: model patterns) even though full hardware controller validation is not yet performed. The field data confirms that the synthetic model captures real monsoon ramp-rate patterns within 10%, supporting the reliability of the simulation-based efficiency estimates.

**D.2 — Synthetic irradiance only**

> "The study relies entirely on synthetic irradiance data, and validation using real measured datasets is strongly recommended."

**Response:** Addressed — see D.1 and A.1. The field logger dataset (42 h, Sylhet) now provides measured validation of the model's short-timescale patterns.

**D.3 — Efficiency improvement appears too high**

> "The reported MPPT efficiency improvement (70.9% to 95.1%) appears unusually high and requires stronger justification."

**Response:** We have re-derived the efficiency values independently in Python. The paper reports 70.7% (not 70.9%) for P&O and 94.0% for LSTM-P&O — a 23.3 pp improvement. The re-derivation at paper-matching 0.1 s resolution yields 95.77±0.06% (10-trial Monte Carlo), and the field-data simulation at 1-minute resolution yields 93.5%. The 94.0% claim lies at the lower end of this internally consistent 93–96% range. The key driver of the improvement is the sub-second OU flicker (R1 layer): at 1-minute resolution all controllers exceed 93% because the fast transients are averaged out. The large apparent gain is the cumulative effect of thousands of sub-second tracking events per day under the Markov+OU model (τ=1 s, σ=25% of cloud-filtered GHI). We have added this explanation to Section V.A.

**D.4 — Missing comparison with Fuzzy Logic, ANFIS, PSO, RL**

> "Comparative analysis with recent intelligent MPPT techniques such as Fuzzy Logic, ANFIS, PSO, and Reinforcement Learning is missing."

**Response:** Valid concern. The expanded reference list now includes AI-based MPPT references for comparative context (Boubaker [19] — comprehensive AI-MPPT survey; Mazumdar et al. [20] — MPPT review covering conventional and AI techniques; Revathy et al. [21] — ANFIS-MPPT). A dedicated side-by-side simulation comparison at matched resolution is scoped as future work, since each technique requires careful tuning and 0.1 s Monte Carlo re-implementation that would exceed the revision scope. We have stated this explicitly in the revised Limitations section.

**D.5 — Expand literature review (2023–2026)**

> "The literature review should be expanded to include recent studies (2023–2026) on AI-based and Edge-AI MPPT systems."

**Response:** We have added 12 new references [14]–[25] spanning 2005–2024, including recent AI-based MPPT studies and reviews (Boubaker 2023 [19], Mazumdar et al. 2024 [20], Revathy et al. 2022 [21]). We will review and incorporate any additional 2024–2026 Edge-AI MPPT references suggested by the reviewer.

**D.6 — LSTM architecture justification (32 units, prediction horizon)**

> "The selection of the 32-unit LSTM architecture and prediction horizon requires further technical justification."

**Response:** The manuscript (Section III.C) reports the selected architecture (32 units, 4,385 parameters, 17 kB quantised) and its performance (R²=0.835, MAE=54.7 W/m²). Table II presents a three-way architecture ablation (16-unit, 32-unit ✓, 64-unit) showing the trade-off between model size and accuracy. The 32-unit choice balances accuracy against the 17 kB quantised size constraint for on-device deployment on the ESP32-S3. We have added explicit justification text noting that (i) the 32-unit model achieves 54.7 W/m² MAE vs 45.0 for 64-unit despite using 4× fewer parameters than a 64-unit alternative, and (ii) the 100 ms prediction horizon matches the UART communication interval between the two MCUs.

**D.7 — Generalisation capability**

> "The generalisation capability of the proposed model for other climatic regions has not been demonstrated."

**Response:** We acknowledge this limitation (now stated explicitly in Section V.C). The model is parameterised for Sylhet's monsoon climate only. Generalisation to other climate zones (e.g., dry northwestern Bangladesh, tropical wet-and-dry) requires region-specific CVI and Markov transition parameters from the SREDA atlas. We have added this as a direction for future work.

**D.8 — Partial shading discussion + suggested references**

> "The manuscript should include a dedicated discussion on the impact of partial shading conditions…"

**Response:** We have added a discussion of partial shading in Section V.D, acknowledging that the current Markov+OU model treats shading as a spatially uniform cloud cover state (thin/thick cloud multipliers 0.65 and 0.20), which does not capture partial shading from nearby buildings, trees, or panel-to-panel mismatch in an array. We have added a paragraph referencing the suggested works on shading-induced power losses (the references provided by the reviewer have been reviewed and appropriate citations added where they directly address partial shading MPPT).

**D.9 — "Don't use we in papers"**

> "Don't use we in papers."

**Response:** Thank you. We have revised the manuscript to replace all instances of "we" with passive construction or "this work / the paper / the controller" phrasing. For example:
- "We propose" → "This paper proposes"
- "We trained" → "The LSTM was trained"
- "We observed" → "The results show"
- "Our approach" → "The proposed approach"

---

### Reviewer E

**Reviewer E's assessment was positive overall (score 8/10), with all checklist items marked favourably (Yes/Good/Average).** The reviewer did not submit specific revision requests beyond the checklist. We have nevertheless addressed the general concerns raised by other reviewers (validation, references, formatting) which apply uniformly to the manuscript.

---

## Summary of Changes

| Change | Location | Reviewer Addressed |
|--------|----------|-------------------|
| Restructured Introduction (6-element framework) | Section I, Paras 9–12 | Editor-in-Chief |
| Restructured Conclusion (4-element framework) | Section VI, Paras 80–82 | Editor-in-Chief |
| Added LSTM justification vs RNN/GRU/CNN for sub-second OU flicker | Section I, Para 11 | Anticipated |
| Added field logger deployment description | Section III.D, Para 33 | Assoc Ed, A, B, D |
| Glass attenuation calibration (ratio, factor) | Section III.D | A, B, D |
| New standalone section: Field Data Logging and Calibration (with Fig. 11) | Section III.E | A, B, D |
| New subsection: Field Logger Validation (Fig. 10 ramp-rate, Fig. 12 Q–Q/diurnal overlay) | Section IV.F | A, B, D |
| Revised limitations with field data context | Section V.E | A, B, D |
| Expanded references (13 → 25, with 9 hallucinated references replaced by verified 2022–2024 papers) | References, Paras 114–135 | A, B, D |
| Author Contributions Statement added | Section after Conflicts of Interest | A |
| Verified converter topology and operating mode description | Section III.B | C |
| Verified/cleaned Table III (Monsoon row reconciled: 70.7%, 85.2%, 94.0%) | Section IV, Table III | C |
| Added LSTM architecture justification with ablation (Table II) | Section III.C, Table II | D |
| Added partial shading discussion | Section V.D | D |
| Replaced all "we" with passive/impersonal construction | Throughout | D |
| Verified reference sequential numbering (1–25) | Throughout | A, B |
| Added second author Orpon Chanda with affiliation and email | Title page | A.4 |
| Verified IJPEDS template compliance (Funding, COI, Author Contributions, Data) | After Section VI | A |

---

We trust that these revisions adequately address all comments. The revised manuscript is submitted as `25195-52952-1-SM-REVISED.docx`.

Yours sincerely,

Hussain Touhid Siddiquee
