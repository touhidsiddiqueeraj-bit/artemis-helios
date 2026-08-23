"""
revision20.py — Add AUTHOR BIOGRAPHIES placeholder section (editorial mandate)
==============================================================================
Appends a two-author biography section after REFERENCES, per IJPEDS template
(two-column paper needs full-width table pattern; we use inline paragraphs to
avoid the column-overflow issue seen earlier):

  AUTHOR BIOGRAPHIES
  [photo placeholder]  Name  — [bio placeholder to be completed by the author]
                        ORCID · Scholar · Scopus · Publons (placeholder links)

Run:  python3 backups/revision20.py
"""
import copy
import re
from lxml import etree
from docx import Document

DOC = '25195-52952-1-SM-REVISED.docx'
W_ = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
FULL_W_TWIPS = 10466

doc = Document(DOC)

def para_text(p):
    return ''.join(r.text or '' for r in p.runs)

paras = list(doc.paragraphs)
refs = next(p for p in paras if para_text(p).strip() == 'REFERENCES')
ack  = next(p for p in paras if para_text(p).strip().startswith('ACKNOWLEDGEMENTS'))
body_tmpl = next(p for p in paras if para_text(p).strip().startswith('The power stage used in all simulations'))

def clone_with_text(tmpl_el, text):
    el = copy.deepcopy(tmpl_el)
    runs = el.findall(f'{W_}r')
    first = runs[0]
    for r in runs[1:]:
        el.remove(r)
    ts = first.findall(f'{W_}t')
    if not ts:
        t = etree.SubElement(first, f'{W_}t')
        t.set(W_ + 'space', 'preserve')
        t.text = text
    else:
        for t in ts[1:]:
            first.remove(t)
        ts[0].text = text
    return el

heading = clone_with_text(ack._p, 'AUTHOR BIOGRAPHIES')
b1 = clone_with_text(body_tmpl._p,
 ':p1: Hussain Touhid Siddiquee. [Author biography — to be completed by the '
 'author: current position, degrees, research interests, and relevant '
 'professional history.] Professional profiles: Scholar [place ORCID/Scholar '
 '/Scopus/Publons URL here] | ORCID [URL] | Scopus [URL] | Publons [URL].')
b2 = clone_with_text(body_tmpl._p,
 ':p2: Orpon Chanda. [Author biography — to be completed by the author: '
 'current position, degrees, research interests, and relevant professional '
 'history.] Professional profiles: Scholar [URL] | ORCID [URL] | Scopus '
 '[URL] | Publons [URL].')

def place(tag):
    return clone_with_text(body_tmpl._p, tag)

def photo_placeholder(name):
    # simple full-width placeholder table cell: bordered box telling the
    # author to drop the photo here. Borderless full-width table (1 cell).
    tbl = etree.Element(f'{W_}tbl')
    tblPr = etree.SubElement(tbl, f'{W_}tblPr')
    tblW = etree.SubElement(tblPr, f'{W_}tblW')
    tblW.set(W_ + 'w', str(FULL_W_TWIPS)); tblW.set(W_ + 'type', 'dxa')
    etree.SubElement(tblPr, f'{W_}tblLayout').set(W_ + 'type', 'fixed')
    borders = etree.SubElement(tblPr, f'{W_}tblBorders')
    for side in ('top', 'left', 'bottom', 'right', 'insideH', 'insideV'):
        etree.SubElement(borders, f'{W_}{side}').set(W_ + 'val', 'single')
    grid = etree.SubElement(tbl, f'{W_}tblGrid')
    etree.SubElement(grid, f'{W_}gridCol').set(W_ + 'w', str(1600))
    etree.SubElement(grid, f'{W_}gridCol').set(W_ + 'w', str(FULL_W_TWIPS - 1600))
    tr = etree.SubElement(tbl, f'{W_}tr')
    # photo cell
    tc1 = etree.SubElement(tr, f'{W_}tc')
    tcPr = etree.SubElement(tc1, f'{W_}tcPr')
    tcW = etree.SubElement(tcPr, f'{W_}tcW'); tcW.set(W_ + 'w', '1600'); tcW.set(W_ + 'type', 'dxa')
    etree.SubElement(tcPr, f'{W_}vAlign').set(W_ + 'val', 'center')
    p1 = etree.SubElement(tc1, f'{W_}p')
    run = etree.SubElement(p1, f'{W_}r')
    t = etree.SubElement(run, f'{W_}t'); t.set(W_ + 'space', 'preserve')
    t.text = f'[{name} photo]'
    # text cell
    tc2 = etree.SubElement(tr, f'{W_}tc')
    tcPr2 = etree.SubElement(tc2, f'{W_}tcPr')
    tcW2 = etree.SubElement(tcPr2, f'{W_}tcW')
    tcW2.set(W_ + 'w', str(FULL_W_TWIPS - 1600)); tcW2.set(W_ + 'type', 'dxa')
    etree.SubElement(tcPr2, f'{W_}vAlign').set(W_ + 'val', 'center')
    return tbl

# insert biographical content just before the final section properties
body = doc.element.body
sectPr = body.find(f'{W_}sectPr')
if sectPr is None:
    sectPr = body.findall(f'.//{W_}sectPr')[-1]

elements = [heading]
for b, name in ((b1, 'Hussain Touhid Siddiquee'), (b2, 'Orpon Chanda')):
    elements.append(photo_placeholder(name))
    elements.append(b)
for el in elements:
    sectPr.addprevious(el)

doc.save(DOC)
print('revision20 applied: AUTHOR BIOGRAPHIES placeholder section appended')
