"""
revision13.py — Round-2 doc fixes (2a + 2b) into the revised manuscript.

2a (Reviewer A, item 1): full power-stage reproducibility block
    (topology, semiconductor models, LC/ESR/DCR, PWM limits, sensing,
    solver/step/duration, operating point, battery model) inserted
    after the power-stage paragraph.
2b (Reviewer A, item 2): controller-fairness statement (identical PV
    model, sampling, resolution, limits, initialization, trajectories;
    no per-controller re-tuning) inserted at the top of Section IV.B.
Plus: correct the SoC start claim 45% -> 30% (verified battery model
    reproduces V=12.41 V only from SoC0=0.30).

Usage: python3 backups/revision13.py
"""
import copy
import os
import shutil

from docx import Document
from docx.oxml.ns import qn

DOCX = '25195-52952-1-SM-REVISED.docx'
BAK = 'backups/25195-52952-1-SM-REVISED-BAK20.docx'
W_ = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'

if not os.path.exists(BAK):
    shutil.copy2(DOCX, BAK)
    print(f"Backup -> {BAK}")

doc = Document(DOCX)
paras = list(doc.element.body.iterchildren(f'{W_}p'))


def txt(p):
    return ''.join(t.text or '' for t in p.iter() if t.tag.endswith('}t'))


def new_para_like(template_p, text):
    """Deep-copy a template paragraph, replace its text with `text`."""
    np_ = copy.deepcopy(template_p)
    for t in np_.iter():
        if t.tag.endswith('}t'):
            t.text = ''
    first_t = next(t for t in np_.iter() if t.tag.endswith('}t'))
    first_t.text = text
    return np_


body_paras = [p for p in paras if txt(p).strip()]

# ── anchors ────────────────────────────────────────────────────────────────
p_power = next(p for p in body_paras if txt(p).startswith('The power stage employs a buck topology'))
p_secIVB = next(p for p in body_paras if txt(p).strip().startswith('B.  MPPT Efficiency'))
p_fig5 = next(p for p in body_paras if txt(p).startswith('Fig. 5 presents the full-day Simulink simulation'))

# ── 2a: reproducibility block (power stage + battery) ─────────────────────
P1 = ('The switching-level power-stage model used for the loss and efficiency verification '
      '(Section IV, Figs. 15 and 16) is specified as follows. Topology: asynchronous buck '
      'with high-side IRFB4110 (V_DS(max) = 100 V, R_DS(on) = 3.7 m\u03a9, C_oss = 83 nF; '
      'body-diode SPICE model I_S = 2.5 nA, N = 1.08, R_S = 2 m\u03a9, T_T = 55 ns) with '
      'freewheeling through the MOSFET body diode, driven by the TC4420 gate driver '
      '(V_DD = 12 V, R_G = 4.7 \u03a9, 50 ns gate-edge rise/fall). LC filter: L = 100 \u03bch '
      '(DCR = 30 m\u03a9), C = 470 \u03bcF (ESR = 40 m\u03a9); input decoupling 100 \u03bcF '
      '(ESR = 30 m\u03a9) in parallel with 1 \u03bcF ceramic (ESR = 5 m\u03a9), plus 15 nH '
      'stray inductance between the panel and the input capacitor; INA219 sense shunt 10 m\u03a9. '
      'PWM: 50 kHz, duty-cycle resolution 0.1%, duty clamped to [0.05, 0.95]; no dead time is '
      'required because the single high-side switch freewheels through its body diode. Sensing: '
      'INA219 (12-bit, I\u00b2C) polled at the 100 ms control-loop rate; STM32F103 ADC 12-bit. '
      'Simulation: ngspice-46 (Berkeley SPICE3 engine, trapezoidal integration), transient '
      'analysis with a 20 ns time step over a 20 ms duration and pre-charged output '
      '(.ic/.uic initial conditions); operating point V_in = 17.9 V (panel Vmp), '
      'V_out = 13.2 V, I_out = 3.8 A, measured \u03b7 = 98.0% at the nominal 3.9 A charging '
      'point, degrading to 95.0% at 12.8 A (Fig. 16).')
P2 = ('Battery-side operating ranges: 7 Ah/12 V SLA, Shepherd open-circuit voltage '
      'V_oc(SoC) = 11.84 + 1.98\u00b7SoC \u2212 0.28\u00b7SoC\u00b2 V, internal resistance '
      '50 m\u03a9, 6 A bulk-charge limit, constant-voltage target 13.6 V, initial SoC 30%. '
      'The verified full-day charge simulation (Code/Python/05_battery_model.py) reproduces '
      'the reported terminal-voltage range 12.41\u201313.61 V and the complete-charge '
      'trajectory of Section IV.B. The system-level Monte Carlo uses the averaged Simulink '
      'model (solver FixedStepDiscrete, 0.1 s fixed step, 0\u201386,400 s), in which the buck '
      'is represented by the averaged duty-ratio model of Section III with conduction, '
      'switching (C_oss, Q_g) and DCR losses.')

for text in (P1, P2):
    p_power.addnext(new_para_like(p_power, text))
    p_power = p_power.getnext()  # move anchor onto the just-inserted node
print('[2a] reproducibility paragraphs inserted')

# ── 2b: fairness statement at top of Section IV.B ─────────────────────────
P3 = ('All controllers were evaluated under strictly identical conditions: the same '
      'single-diode PV model (Section III), the same 100 ms sampling interval and INA219 '
      '12-bit sensing resolution, the same converter limits (50 kHz PWM, duty clamp '
      '[0.05, 0.95], 6 A bulk-charge limit), the same initialization (V_ref = 17.0 V, '
      'SoC = 30%), and identical irradiance trajectories for each Monte Carlo day. '
      'No controller-specific re-tuning was performed: the VS-P&O step rule (k = 0.005), '
      'the LSTM blend coefficient (\u03b1 = 0.35) and its 15% deviation deadband are the '
      'values reported in Section III, and the plain P&O and INC baselines use their '
      'standard fixed-step (0.1 V) and tolerance (0.01 A/V) settings, respectively.')
p_secIVB.addnext(new_para_like(p_secIVB, P3))
print('[2b] fairness statement inserted')

# ── SoC start correction 45% -> 30% ───────────────────────────────────────
fixed = 0
for p in doc.paragraphs:
    t = p.text
    if '45%' in t:
        nt = t.replace('45%', '30%')
        for r in p.runs:
            r.text = ''
        p.runs[0].text = nt
        fixed += 1
        print(f'[45%] corrected paragraph: ...{nt[:90]}...')
print(f'[45%] {fixed} paragraph(s) corrected')

doc.save(DOCX)
print('Saved:', DOCX)
