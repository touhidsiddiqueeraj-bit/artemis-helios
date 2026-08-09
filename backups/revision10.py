"""
revision10.py — Wrap wide figures in full-width 1x1 borderless tables
(dual-column template: inline images wider than a column must live in a
full-text-width table or they bleed over the neighbor column / page edge).
  QA: figures 1-10 (+ captions) inside tables; QR stays inline.
"""
import re, shutil, os
from docx import Document
from docx.oxml.ns import qn
from lxml import etree

DOCX = '25195-52952-1-SM-REVISED.docx'
BAK = '25195-52952-1-SM-REVISED-BAK10.docx'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
W_ = f'{{{W}}}'
EMU_IN = 914400
FULL_W_TWIPS = 10466  # text width = 7.268 in

if not os.path.exists(BAK):
    shutil.copy2(DOCX, BAK)
    print(f"Backup -> {BAK}")

doc = Document(DOCX)
body = doc.element.body

def txt(el):
    return ''.join(t.text or '' for t in el.iter() if t.tag.endswith('}t')).strip()

os_all = list(body.iterchildren(f'{W_}p'))

# blip-order = display order; figures 1..10 wide, QR (12th blip para? no: 11th) inline
blip_paras = [e for e in os_all if e.findall('.//' + qn('a:blip'))]
assert len(blip_paras) == 11, f"expected 11 blip paras, got {len(blip_paras)}"
QR = blip_paras[-1]

def make_tbl():
    tbl = etree.SubElement(body, f'{W_}tbl')  # placeholder; insert() repositions
    body.remove(tbl)
    tblPr = etree.SubElement(tbl, f'{W_}tblPr')
    tblW = etree.SubElement(tblPr, f'{W_}tblW')
    tblW.set(W_ + 'w', str(FULL_W_TWIPS)); tblW.set(W_ + 'type', 'dxa')
    etree.SubElement(tblPr, f'{W_}tblLayout').set(W_ + 'type', 'fixed')
    etree.SubElement(tblPr, f'{W_}jc').set(W_ + 'val', 'center')
    borders = etree.SubElement(tblPr, f'{W_}tblBorders')
    for side in ('top', 'left', 'bottom', 'right', 'insideH', 'insideV'):
        etree.SubElement(borders, f'{W_}{side}').set(W_ + 'val', 'nil')
    mar = etree.SubElement(tblPr, f'{W_}tblCellMar')
    for side in ('top', 'left', 'bottom', 'right'):
        s = etree.SubElement(mar, f'{W_}{side}')
        s.set(W_ + 'w', '0'); s.set(W_ + 'type', 'dxa')
    etree.SubElement(tblPr, f'{W_}tblInd').set(W_ + 'w', '0')
    grid = etree.SubElement(tbl, f'{W_}tblGrid')
    etree.SubElement(grid, f'{W_}gridCol').set(W_ + 'w', str(FULL_W_TWIPS))
    tr = etree.SubElement(tbl, f'{W_}tr')
    tc = etree.SubElement(tr, f'{W_}tc')
    tcPr = etree.SubElement(tc, f'{W_}tcPr')
    tcW = etree.SubElement(tcPr, f'{W_}tcW')
    tcW.set(W_ + 'w', str(FULL_W_TWIPS)); tcW.set(W_ + 'type', 'dxa')
    etree.SubElement(tcPr, f'{W_}tcMar')
    etree.SubElement(tcPr, f'{W_}vAlign').set(W_ + 'val', 'center')
    return tbl, tc

n_ok = 0
for idx, img_el in enumerate(blip_paras):
    if img_el is QR:
        print(f"blip {idx+1}: QR left inline")
        continue
    fig_no = idx + 1
    # find caption within next 4 paragraphs (skipping blanks): starts with "Fig. {n}. "
    cap_el = None
    pos = os_all.index(img_el)
    for k in range(pos + 1, min(pos + 5, len(os_all))):
        t = txt(os_all[k])
        if t.startswith(f'Fig. {fig_no}. '):
            cap_el = os_all[k]
            break
        if t and not t.startswith('Fig.'):
            break  # don't skip past real prose? captions may follow blank only
    if cap_el is None:
        # relax: blank between image and caption (Fig 1 case)
        for k in range(pos + 1, min(pos + 5, len(os_all))):
            t = txt(os_all[k])
            if t.startswith(f'Fig. {fig_no}. '):
                cap_el = os_all[k]
                break
    assert cap_el is not None, f"caption for fig {fig_no} not found near para {pos}"
    tbl, tc = make_tbl()
    body.insert(body.index(img_el), tbl)
    for el in (img_el, cap_el):
        body.remove(el)
        tc.append(el)
    n_ok += 1
    print(f"fig {fig_no:2d}: wrapped image + caption in full-width table")

assert n_ok == 10, f"expected 10 wrapped, got {n_ok}"
doc.save(DOCX)
print("saved")

print("[verify]")
doc = Document(DOCX)
W_ = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
W__ = f'{{{W_}}}'
tables = doc.element.body.findall(f'{W__}tbl')
print("  tables:", len(tables))
blips = 0
for t in tables:
    imgs = t.findall('.//' + qn('a:blip'))
    caps = [''.join(x.text or '' for x in c.iter() if x.tag.endswith('}t'))
            for c in t.iter() if c.tag == f'{W__}p' and ''.join(x.text or '' for x in c.iter() if x.tag.endswith('}t')).strip().startswith('Fig.')]
    blips += len(imgs)
    cap = caps[0][:45] if caps else '(missing)'
    print(f"  tbl: {len(imgs)} image(s), caption: {cap}")
print("  blips in tables:", blips)
body_blips = [e for e in doc.element.body.iterchildren(f'{W__}p') if e.findall('.//' + qn('a:blip'))]
print("  inline blip paras left (should be 1 = QR):", len(body_blips))
assert blips == 10 and len(body_blips) == 1
print("ALL CHECKS PASSED ✓")