# PLAN — v3 Reframe (NOT for v2)

**Paper 25195 round-3 direction** — recorded 2026-08-16. Do NOT implement in v2.

## Work state (2026-08-18)

### DONE — ESP32-S3 execution-time measurement (partial A.4)
- Probe firmware: `prototype/measurement/esp32_timing_probe/esp32_timing_probe.ino`
  (paper-sized LSTM: 32 units × 24 steps × 33 inputs; 115.2 kbaud packet link;
  N = 400 batches; hardware-timer stamps, USB output only between batches).
- Run 3 results (240 MHz, ESP32-S3 N16R8): preprocess 7.1 µs mean (p99 8);
  LSTM kernel 6.355 ms mean (p99 6.360, max 6.457); packet format 86.7 µs mean
  (max 382); UART drain 3.484 ms mean (max 3.491); full Helios tick 9.996 ms;
  loop period 100.000 ms mean (p99 100.026, max 100.032); abs jitter 16.3 µs
  mean (p99 31, max 36).
- Manuscript (revision23.py, BAK41→BAK42): new Section V timing paragraph +
  **Table V** + **Fig. 16** (`Code/Python/fig_timing_budget.py`, self-check
  PASS; vision-verified clean after ms-axis + legend fixes); BOM fig renumbered
  16→17, QR 17→18; VI.C limitations + future-work sentences note STM32/end-to-
  end latency pending hardware. Letter: A.4 section added with measured values
  + 100 ms-cycle discussion, C.3 updated, checklist 18 figs/Table I–V, summary
  rows updated, letter rebuilt (8 pp). PDFs + Docs/Upload_package refreshed.

### PENDING — STM32F103C8T6 minimum board (closes A.4 fully)
- User ordered board; wire ESP32 TX1→STM32 USART1 RX, RX1→TX1, common GND
  (both 3.3 V), BOOT0 = 0.
- Then: STM32 probe firmware (STM32duino Arduino core or HAL) measuring RX
  interrupt → parse → PWM update (TIM PWM on PA0/PA6, GPIO markers); round-trip
  or logic-analyzer end-to-end; then close A.4 in letter + manuscript Table V
  row, renumber if new figure added.

## Positioning
Plain P&O is adequate under slow irradiance variation; its efficiency collapses
specifically during rapid monsoon (cloud) transients. Helios-Artemis
(LSTM-assisted + on-device retraining) recovers a large share of that
*transient energy loss* — the exact operational scenario the paper addresses.
We claim a large benefit in THAT scenario, not a large average-efficiency lead.

## Sub-claims (all testable)
- C1. Tracking loss is concentrated in high-ramp-rate windows, not steady periods.
- C2. LSTM-assisted tracking reduces transient energy loss / improves η_track
      within fast-variation windows vs plain P&O (and VS-P&O, INC); it reverts to
      reactive behaviour in slow periods (deadband + cooldown).
- C3. Gain grows monotonically with irradiance variability (benefit-vs-variability).
- C4. On-device retraining keeps the forecaster within the tested MAE≈55 W/m² band
      seasonally — stated as architecture capability (mechanism), not separately
      simulated in v2 evidence.

## Metrics
- M1 ramp-rate-stratified η_track + energy loss (bins of |ΔG| per 0.1 s step).
- M2 headline: transient energy-loss fraction L_trans in high-rate windows
      (explicitly a controller tracking metric, not energy yield — A.6 language).
- M3 benefit-vs-variability curve (OU σ / cloud-event-density sweep, fixed seeds).
- M4 forecaster-attribution bracket: ema forecast vs ema + N(0, MAE≈55) noise
      (or plug the actual trained LSTM) so LSTM attribution is defensible.

## Code
- `stochastic_day`: add sigma + (opt) Markov transition-rate params (currently
  hardcoded σ=0.25).
- New `Code/Python/07_variability_study.py`: M1/M2/M3/M4 from saved traces with a
  self-check (L_trans ≥ 0, monotonicity direction, plateau stability).
- Regenerate `transient_benchmark.csv` + fig17 under the cooldown-enabled
  controller (fixes stale-data flag).

## Manuscript
- New IV.H (or extend IV.G): M3 curve + M1 stratified bars; new Table (bins ×
  controllers). Cascades renumbering (BOM/logger/QR shift by +1; QR→17), reuse
  revision18-style remap.
- STRIP all 23.3 pp / 70.7 % everywhere: Abstract, Introduction contributions,
  Table III, Section V.A/D, Fig. 6 caption. Never re-insert under this framing.
- Keep GA claim-free (already is).

## Letter (v3)
- Reactivates A.2 (controlled transient suite — now central), A.6 (L_trans as
  tracking metric, no yield-gain claim), C.2. A.4 latency still deferred.
- A.1/A.3/A.5/B.1/B.2/C.1/C.3 + editorial checklist stay as in v2.

## Definition of done
PDF verified page-by-page (gemini), numbers match CSV traces, serialization
intact, letter ↔ manuscript consistent.
