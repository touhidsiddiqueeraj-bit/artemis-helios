# Audit-Fix Report — Paper 25195 (Helios-Artemis)

Comparison of `25195-52952-1-SM-REVISED.pdf` (manuscript), `backups/audit.txt` (editor/reviewer comments), and `response_letter.md`. Produced 2026-08-14 before the response-letter fix pass.

## 1. Manuscript ↔ response-letter parity (numbers)

All of the following cross-check cleanly between manuscript, response letter, and code:

| Quantity | Value | Locations agree |
|---|---|---|
| Monsoon efficiency (Table III) | P&O 70.7 / VS-P&O 85.2 / INC 75.2 / LSTM-P&O 94.0 ±2.1 | letter D.3, manuscript, `gen_figures_hires.py` |
| Annual efficiency | 85.8 / 89.1 / 87.7 / 91.3 | letter, manuscript IV.E |
| MC mean | 94.0%, σ=0.6%, 95% CI [92.9, 94.9] | letter, manuscript IV.C |
| LSTM | 32 u, 4,385 params, 17 kB, R²=0.835, MAE 54.7, RMSE 72.6 | letter, manuscript III.C/Table II |
| α plateau | [0.20, 0.55], α=0.35, half-width 0.175 | letter, manuscript V.A |
| Ramp rates | σ 39.7 vs 17.0; mean |ΔG| 29.4 vs 8.9; KS D=0.224; 2.3× | letter A.1, manuscript IV.F |
| Cost | ~1,750 BDT / USD 16; 13,500 BDT commercial; 87% | letter, manuscript V.B, Fig. 10 |
| Glass calibration | ratio 0.9314, factor 1.0737, n=33+34=67 | letter, manuscript III.D |
| Field campaign | Jul 9–14 deployed, 42 h usable (Jul 10–13), 18,395 rows, 10–505 W/m² | letter, manuscript III.D |
| Author contributions | O.C. hardware/field credit | letter A.4, manuscript title page |

## 2. Per-comment verdicts (audit → letter claim → manuscript evidence)

| # | Comment | Letter claims | Manuscript reality | Verdict |
|---|---|---|---|---|
| EIC-1 | Intro 6-element structure | 4-paragraph scheme (A–F) | Section I has unlabelled context ¶ + B–F subsections | ✓ addressed |
| EIC-2 | Conclusion structure | 3 paragraphs (done/findings/limits) | Section VI matches | ✓ addressed |
| AE-1 | Method structure | III.A–III.D cover a–e; Ethics n/a | Matches; but see note 1 | ✓ addressed (weak replication detail, see B.2) |
| AE-2 | Results/Discussion structure | IV.A–F, V.A–D framework | Matches | ✓ addressed |
| A.0 | LSTM choice justification | "Intro ¶3 strengthened… constant-error carousel… ACF≈0.95" | Intro does NOT contain this; it lives in III.C/IV.F; "carousel" absent from manuscript | ✗ letter overclaims location; content exists in III.C/IV.F |
| A.1 | No field validation | IV.F, Fig. 9, Path B, all stats | IV.F present with exact stats | ✓ addressed |
| A.2 | 13 refs limited | "13 → 25, new [14]–[25]; includes Kjaer, Talaat, Chao&Lin, Jazia, Arefin, Saha" | Manuscript has **37 refs**; **none** of those authors appear | ✗ fabricated numbers in letter |
| A.3 | Formatting/sections | Funding, COI, Data, Author Contributions present | All four present | ✓ addressed |
| A.4 | Single author | O.C. added with hardware credit | Title page + contributions | ✓ addressed |
| B.1 | Simulation-only | "field-data re-derivation yields 93.5%" | 93.5% absent from manuscript; fresh run gives 96.3% | ✗ letter number unverifiable → drop |
| B.2 | Methods detail | "extended III.D" incl. LSTM training details | LSTM training is in III.C/IV.A, not III.D; no pseudocode/step-level detail | △ partly addressed; letter mislocates |
| B.3 | Refs/format | "expanded to 25" | 37 | ✗ wrong count in letter |
| C.1 | Table 2 math | corrected specs in III.B | III.B matches | ✓ addressed |
| C.2 | boost-buck naming | buck only | III.B buck-only, explicit | ✓ addressed |
| C.3 | Formulas in text | Eq. 1 numbered | Eq. 1 present; only one numbered eq. | △ partially (rest deferred per letter) |
| D.1/D.2 | No hardware/synthetic-only | Path B field validation | IV.F | ✓ addressed |
| D.3 | Efficiency too high | "95.77±0.06% (10-trial); 93.5% (field); all controllers >93% at 1-min" | 95.77/93.5 absent from manuscript. Fresh run: LSTM-P&O 95.75±0.09 (MC), 96.3 (field); VS-P&O 92.4 at 1-min → "all >93%" false | ✗ numbers not reproducible → drop specifics |
| D.4 | Missing FL/ANFIS/PSO/RL | cites "Talaat [19], Chao&Lin [20], Jazia [21]" | [19] is Kofinas (RL), [20] NASA, [21] SREDA; ANFIS = [13] Aldulaimi & Çevik | ✗ citation misattribution |
| D.5 | Recent literature | "12 new refs [14]–[25] spanning 2005–2024… Talaat 2022 [19]" | 37 refs incl. 2025 ([13], [28]), 2024 ([9], [14], [22]) | △ understated + misattributed |
| D.6 | 32-unit justification | ablation Table II, ΔMAE<0.6, 3.8× | Table II matches exactly | ✓ addressed |
| D.7 | Generalisation | stated in V.C | V.C matches | ✓ addressed |
| D.8 | Partial shading | V.D discussion + refs | V.D present, cites [6],[18],[19],[36],[37] | ✓ addressed |
| D.9 | No "we" | all removed | 0 occurrences in manuscript | ✓ addressed |
| E | Compute complexity | 4,486 params; <12 ms; int8 4.7 ms, ΔR²=−0.009; 40 mA; cores | Manuscript: 4,385+4=4,389 (not 4,486); 12 ms budget; 40 mA; core split ✓; int8 4.7 ms/ΔR² unsupported in code (comment claims are paper-references, converter NOTE says int8 not available) | ✗ fix param count; drop int8 R² delta |

Note 1 (AE-1): letter cites "Table IV" and "Eqs. 1–4"; manuscript has no Table IV and only Eq. 1. Fix in letter.

Note 2: letter A.0 says "4–8 ms" while E says "<12 ms" — unify on "<12 ms".

## 3. Reproduction log (2026-08-14)

Ran `Logger_Data/cleaned/tier2_table3_rederivation.py` (repo's own re-derivation script):

```
Controller          Paper Jul    MC Mean(0.1s)    Field(1-min)
Plain P&O               70.7%    98.45±0.03        95.5%
VS-P&O                  85.2%    95.77±0.10        92.4%
INC                      N/A    98.77±0.05        99.0%
LSTM-P&O                94.0%    95.75±0.09        96.3%
Ramp rates (1-min): field μ=73 σ=90; synthetic μ=80 σ=103
```

- Letter's "95.77±0.06%" ≈ reproduced (95.75±0.09) — no longer cited verbatim.
- Letter's "93.5% field-data" NOT reproduced (96.3%) — **dropped**.
- Letter's "at 1-minute resolution all controllers exceed 93%" falsified (VS-P&O 92.4) — **removed**.
- int8 "4.7 ms / ΔR²=−0.009" has no code support (`02_lstm_training.py` converter NOTE states int8 unavailable for the LSTM Flex pipeline) — **dropped**.

## 4. Fix list applied to response_letter.md

1. A.0 — locate justification honestly (Intro SOTA + III.C; ACF evidence IV.F); unify latency "4–8 ms" → "<12 ms".
2. A.2 / B.3 / D.5 — references 13 → **37**; remove fabricated author names (Kjaer, Talaat, Chao and Lin, Jazia, Arefin, Saha); list real added refs by number ([2]–[37] categories).
3. B.1 / D.3 — drop 93.5%, 95.77±0.06, "all >93%"; keep 70.7≠70.9 clarification and OU-flicker driver; cite reproducible 0.1 s re-derivation ≈95.8% instead.
4. B.2 — correct location: LSTM training in III.C (evaluation IV.A), simulation/MC in III.D.
5. D.4 — remap citations: ANFIS = [13] Aldulaimi & Çevik; RL = [19] Kofinas; surveys = [16],[17].
6. Reviewer E — parameters 4,486 → 4,389 (4,385+4); drop int8 4.7 ms / ΔR²=−0.009; keep <12 ms float32, 40 mA, core split.
7. AE-1 — "Table IV" → Section III.B; "Eqs. 1–4" → Eq. 1.
8. Summary-of-Changes table — reference row (13 → 37, 24 new), numbering (1–37), LSTM-justification location.
9. A.3 wording — Data Availability is stated after Author Contributions (order per manuscript); keep.

## 5. Not touched (out of scope for letter pass)

- Manuscript text itself (docx) unchanged; verified metrics already consistent.
- Optional manuscript edits deferred: add int8 latency to III.C, add 0.1 s re-derivation note to Conclusion — only if wanted later.