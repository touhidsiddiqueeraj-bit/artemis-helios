"""Revision 3: embed Fig. 11 image once, restore equation, verify."""

import re
from docx import Document
from docx.shared import Inches
from lxml import etree
from copy import deepcopy

DOCPATH = '25195-52952-1-SM-REVISED.docx'
OUTPATH = '25195-52952-1-SM-REVISED.docx'

doc = Document(DOCPATH)
body = doc.element.body

w_ns = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'

# ──────────────────────────────────────────────────────────────────
# 1. Identify ALL rId25 paragraphs using regex on serialised body
# ──────────────────────────────────────────────────────────────────
body_xml = etree.tostring(body, encoding='unicode')

# Find all <w:p> that contain r:embed="rId25"
# Use simple approach: iterate children, check XML string
rid25_indices = []
for idx, child in enumerate(body):
    child_str = etree.tostring(child, encoding='unicode')
    if 'r:embed="rId25"' in child_str:
        rid25_indices.append(idx)

print(f"rId25 paragraphs at body indices: {rid25_indices}")

if rid25_indices:
    # Remove all but the last one (which is our freshly created image)
    for idx in reversed(rid25_indices[:-1]):
        body.remove(list(body)[idx])
    print(f"Removed {len(rid25_indices)-1} stale rId25 paragraphs")

# ──────────────────────────────────────────────────────────────────
# 2. Find Fig. 11 caption, detach it
# ──────────────────────────────────────────────────────────────────
fig11_cap = None
for child in body:
    texts = [t.text for t in child.iter(f'{w_ns}t') if t.text]
    if 'Fig. 11.  Field-logger ramp-rate validation' in ''.join(texts):
        fig11_cap = child
        break
if fig11_cap is None:
    print("ERROR: Fig. 11 caption not found")
    import sys; sys.exit(1)

# Find V. DISCUSSION
disc_elem = None
for child in body:
    texts = [t.text for t in child.iter(f'{w_ns}t') if t.text]
    if ''.join(texts).strip() == 'V.  DISCUSSION':
        disc_elem = child
        break
if disc_elem is None:
    print("ERROR: V. DISCUSSION not found")
    import sys; sys.exit(1)

# Detach caption from old position
body.remove(fig11_cap)

# ──────────────────────────────────────────────────────────────────
# 3. Find and detach the single remaining rId25 image
# ──────────────────────────────────────────────────────────────────
for child in body:
    child_str = etree.tostring(child, encoding='unicode')
    if 'r:embed="rId25"' in child_str:
        body.remove(child)
        print("Detached remaining rId25 image")
        break

# ──────────────────────────────────────────────────────────────────
# 4. Create fresh image paragraph (will get new rId)
# ──────────────────────────────────────────────────────────────────
img_para = doc.add_paragraph()
img_run = img_para.add_run()
img_run.add_picture('Logger_Data/cleaned/fig_validation_ramprates.png', width=Inches(5.2))
img_elem = img_para._element
body.remove(img_elem)
print("Created fresh image")

# ──────────────────────────────────────────────────────────────────
# 5. Insert blank + image + caption before V. DISCUSSION
# ──────────────────────────────────────────────────────────────────
blank = doc.add_paragraph()
blank_elem = blank._element
body.remove(blank_elem)

disc_elem.addprevious(blank_elem)
disc_elem.addprevious(img_elem)
disc_elem.addprevious(fig11_cap)
print("Inserted blank + image + caption before V. DISCUSSION")

# ──────────────────────────────────────────────────────────────────
# 6. Restore equation at paragraph 31
# ──────────────────────────────────────────────────────────────────
correct_eq = "(1)  V_ref,new = (1 \u2212 α) · V_ref,P&O + α · V_MPP,pred"
para31 = doc.paragraphs[31]
if para31.text.strip() != correct_eq:
    for run in para31.runs:
        run._element.getparent().remove(run._element)
    para31.add_run(correct_eq)
    print(f"Restored equation at p[31]")
else:
    print(f"NOTE: equation already correct at p[31]")

# ──────────────────────────────────────────────────────────────────
# SAVE
# ──────────────────────────────────────────────────────────────────
doc.save(OUTPATH)
print(f"\nSaved to {OUTPATH}")

# ──────────────────────────────────────────────────────────────────
# VERIFY
# ──────────────────────────────────────────────────────────────────
doc2 = Document(OUTPATH)
body2 = doc2.element.body
print("\n=== VERIFICATION ===")

body_xml2 = etree.tostring(body2, encoding='unicode')
all_embeds = re.findall(r'r:embed="([^"]+)"', body_xml2)
from collections import Counter
counts = Counter(all_embeds)
print(f"  rId25 count: {counts.get('rId25', 0)} (should be 1)")
print(f"  Total image rIds: {dict(counts)}")

for idx, child in enumerate(body2):
    child_str = etree.tostring(child, encoding='unicode')
    texts = ''.join(t.text or '' for t in child.iter(f'{w_ns}t')).strip()
    if 'r:embed="rId25"' in child_str:
        print(f"  Fig.11 IMAGE at body[{idx}]")
    elif 'Fig. 11.  Field-logger ramp-rate validation' in texts:
        print(f"  Fig.11 CAPTION at body[{idx}]")
    elif texts == 'V.  DISCUSSION':
        print(f"  V. DISCUSSION at body[{idx}]")

for i, p in enumerate(doc2.paragraphs):
    t = p.text.strip()
    if 'V_ref,new' in t and '(1)' in t:
        print(f"  Equation OK at p[{i}]: {t[:80]}")
        break
for i, p in enumerate(doc2.paragraphs):
    t = p.text.strip()
    if t.startswith('The power stage employs a ') and 'buck' in t and 'boost-capable' not in t:
        print(f"  Buck fix OK at p[{i}]")
        break
