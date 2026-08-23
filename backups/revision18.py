"""
revision18.py — Serialize figure numbering
==========================================
Current figures are out of order: Section IV.G holds Figs 13-15 while
Figs 10-12 (BOM, logger schematic, QR) appear later. Renumber so figure
numbers follow document order:

  old 13 -> 10  (power-stage schematic, IV.G)
  old 14 -> 11  (switching waveforms, IV.G)
  old 15 -> 12  (efficiency/loss, IV.G)
  old 10 -> 13  (BOM breakdown, V.B)
  old 11 -> 14  (field logger schematic, VI)
  old 12 -> 15  (QR code)
  1-9 unchanged.

Also repairs the stale cross-ref "Section VII, Figs. 12-13" -> "Section VI
(Fig. 14)" (hardware section is VI and contains a single schematic figure).

Run:  python3 backups/revision18.py
"""
import re
from docx import Document

DOC = '25195-52952-1-SM-REVISED.docx'
REMAP = {13: 10, 14: 11, 15: 12, 10: 13, 11: 14, 12: 15}

doc = Document(DOC)

def para_text(p):
    return ''.join(r.text or '' for r in p.runs)

def set_text(p, new):
    for r in p.runs:
        r.text = ''
    p.runs[0].text = new

FIGS_RE = re.compile(
    r'\b(Fig(?:ure)?s?\.?)\s*(\d+)\s*([\u2013\u2014-]\s*(\d+))?\s*(\([a-d]\))?')

def _sub(m):
    n1 = int(m.group(2))
    out = f"{m.group(1)}{REMAP.get(n1, n1)}"
    if m.group(3):
        n2 = int(m.group(4))
        sep = m.group(3)
        sep = sep.replace(str(n2), '')
        out += f"{sep}{REMAP.get(n2, n2)}"
    if m.group(5):
        out += m.group(5)
    return out

changed = 0
for p in doc.paragraphs:
    t = para_text(p)
    new = t
    new = new.replace('is presented in Section VII, Figs. 12\u201313.',
                      'is presented in Section VI (Fig. 14).')
    new = new.replace('is presented in Section VII, Figs. 12-13.',
                      'is presented in Section VI (Fig. 14).')
    new = FIGS_RE.sub(_sub, new)
    if new != t:
        set_text(p, new)
        changed += 1

doc.save(DOC)
print(f'revision18 applied: {changed} paragraphs updated')
