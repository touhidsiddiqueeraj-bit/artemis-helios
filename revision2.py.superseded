"""Revision script: fix figure ordering, III.D/III.E dedup, IV.A arithmetic,
   V.C missing section, ramp-rate precision, KS D interpretation."""

import copy
from docx import Document
from docx.shared import Pt
from lxml import etree

NS = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
DOCPATH = '25195-52952-1-SM-REVISED.docx'
OUTPATH = '25195-52952-1-SM-REVISED.docx'

doc = Document(DOCPATH)
paras = doc.paragraphs

# ──────────────────────────────────────────────────────────────────
# 1. Fix Fig. 9 / Fig. 10 caption ordering
#    Current: ...76(Fig9 image) 77(page break) 78(Fig10 image) 79(Fig10 cap) 80(blank) 81(Fig9 cap)
#    Target:  ...76(Fig9 image) 81(Fig9 cap)  77(page break) 78(Fig10 image) 79(Fig10 cap) 80(blank)
# ──────────────────────────────────────────────────────────────────
fig9_cap_elem = paras[81]._element
fig9_img_elem = paras[76]._element
page_break_elem = paras[77]._element

# Detach Fig. 9 caption from its current position
fig9_cap_elem.getparent().remove(fig9_cap_elem)
# Insert Fig. 9 caption after Fig. 9 image (before the page break)
fig9_img_elem.addnext(fig9_cap_elem)

# ──────────────────────────────────────────────────────────────────
# 2. Strip field-logger details from III.D (Para 36)
#    Remove: "For model validation, a field irradiance logger..." onward
# ──────────────────────────────────────────────────────────────────
para36 = paras[36]
old36_text = para36.text
# Find the logger sentence and remove it
idx = old36_text.find('For model validation, a field irradiance logger')
if idx > 0:
    new36_text = old36_text[:idx]
    # Remove trailing space
    new36_text = new36_text.rstrip('. ') + '.'
    # Clear and rewrite
    for run in para36.runs:
        run._element.getparent().remove(run._element)
    run = para36.add_run(new36_text)

# ──────────────────────────────────────────────────────────────────
# 3. Fix IV.A: 1.7 pp → 9.9 pp, add 16-unit explanation
# ──────────────────────────────────────────────────────────────────
para56 = paras[56]
# The full text of the sentence to fix
old_iv_a = "The 64-unit model (Table II) yields only a 1.7 pp R² improvement at the cost of a 3.1× parameter increase in the irradiance forecaster alone (4,385 → 22,849), which extrapolates to approximately 37 ms inference — exceeding the 12 ms budget confirmed for the 32-unit model and risking violation of the 100 ms UART synchronisation interval under load; the 32-unit model therefore represents the optimal Pareto point for this resource-constrained deployment."

new_iv_a = "Table II reveals that the 16-unit model achieves higher R² (0.858) than the 32-unit model (0.835), but with higher MAE (67.1 vs 54.7 W/m²), indicating it better captures test-set variance at the cost of larger point errors. The 64-unit model yields a 9.9 pp R² improvement (0.934 vs 0.835) at the cost of a 3.1× parameter increase (4,385 → 22,849), which extrapolates to approximately 37 ms inference — exceeding the 12 ms budget confirmed for the 32-unit model and risking violation of the 100 ms UART synchronisation interval under load. The 32-unit model therefore represents the optimal Pareto point for this resource-constrained deployment, minimising MAE within the inference budget."

# Replace text in para56
for run in para56.runs:
    parent = run._element.getparent()
    if parent is not None:
        parent.remove(run._element)
run56 = para56.add_run(new_iv_a)

# ──────────────────────────────────────────────────────────────────
# 4. Fix V section numbering: V.D → V.C, V.E → V.D
# ──────────────────────────────────────────────────────────────────
# Para 92: "D.  Partial Shading Considerations" → "C.  Partial Shading Considerations"
paras[92].clear()
paras[92].add_run('C.  Partial Shading Considerations')

# Para 94: "E.  Limitations and Future Work" → "D.  Limitations and Future Work"
paras[94].clear()
paras[94].add_run('D.  Limitations and Future Work')

# ──────────────────────────────────────────────────────────────────
# 5. Ramp-rate precision: specify μ, add σ ratio in IV.F
# ──────────────────────────────────────────────────────────────────
# Para 83: "a pattern agreement of within 10% (ratio 0.91×)" → clarify it's μ
para83 = paras[83]
old83 = para83.text
# Find and replace the ramp-rate sentence
old_ramp_sent = "a pattern agreement of within 10% (ratio 0.91×)"
new_ramp_sent = "a mean ramp-rate agreement of within 10% (μ ratio 0.91×); the standard-deviation ratio is 89.9/102.7 = 0.88"
para83_text_updated = old83.replace(old_ramp_sent, new_ramp_sent)

# Also add KS interpretation after the KS D mention
# Find "with KS D = 0.402" and add interpretation
ks_sent = "with KS D = 0.402"
ks_interpret = "with KS D = 0.402, indicating moderate distributional divergence consistent with the wider climatological variance of the 31-day synthetic July ensemble against the 4-day monsoon field sample"

para83_text_updated = para83_text_updated.replace(ks_sent, ks_interpret)

paras[83].clear()
paras[83].add_run(para83_text_updated)

# Para 84 caption: "Agreement within 10%" → "Mean ramp-rate agreement within 10%"
paras[84].clear()
paras[84].add_run('Fig. 11.  Field-logger ramp-rate validation vs synthetic Markov+OU model (July, 1-minute resolution). Grey bars: field data (4 days); blue bars: synthetic (10-day Monte Carlo). Mean ramp-rate agreement within 10% validates the model\'s short-timescale pattern.')

# ──────────────────────────────────────────────────────────────────
# 6. Update IV.F cross-reference: "Fig. 10 presents" → stays as Fig. 10
#    (since we didn't swap numbers, just moved the caption)
# ──────────────────────────────────────────────────────────────────
# The text in Para 83 says "Fig. 10 presents a direct diurnal overlay..."
# After the reordering, Fig. 10 is now Para 78 image + Para 79 caption.
# The reference at the end of Para 83 to Fig. 10 is still correct — 
# it's a forward reference to Fig. 10 which appears in IV.E.
# No change needed.

# ──────────────────────────────────────────────────────────────────
# SAVE
# ──────────────────────────────────────────────────────────────────
doc.save(OUTPATH)
print("Saved revised document.")

# ──────────────────────────────────────────────────────────────────
# VERIFY
# ──────────────────────────────────────────────────────────────────
doc2 = Document(OUTPATH)
print("\n=== VERIFICATION ===")
for i, p in enumerate(doc2.paragraphs):
    t = p.text.strip()
    if t and ('Fig.' in t[:10] or ('C.' == t[:2] and 'Partial' in t) or ('D.' == t[:2] and 'Limitations' in t)):
        print(f'  [{i}] {t[:120]}')
    # check IV.A para
    if i == 56:
        print(f'  [{i}] IV.A: {t[:200]}')
    # check III.D para
    if i == 36:
        print(f'  [{i}] III.D: {t[:120]}...')
    if i == 83:
        print(f'  [{i}] IV.F: ...{t[-150:]}')
    if i == 84:
        print(f'  [{i}] Caption: {t[:120]}')
    if i == 92:
        print(f'  [{i}] V.C: {t}')
    if i == 94:
        print(f'  [{i}] V.D: {t}')
