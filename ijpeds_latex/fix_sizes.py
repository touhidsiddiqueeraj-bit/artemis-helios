import re
txt = open('paper.tex').read()

# Cap tall figures by height instead of width
replacements = [
    # (pattern, replacement)
    (r'[width=0.95\textwidth]{figs/fig13.png}', '[height=9cm]{figs/fig13.png}'),
    (r'[width=0.75\textwidth]{figs/fig18.png}', '[height=8.5cm]{figs/fig18.png}'),
    (r'[width=0.3\textwidth]{figs/fig22.png}', '[width=4cm]{figs/fig22.png}'),
    (r'[width=0.95\textwidth]{figs/fig07.png}', '[height=8.5cm]{figs/fig07.png}'),
    (r'[width=0.95\textwidth]{figs/fig05.png}', '[height=9cm]{figs/fig05.png}'),
    (r'[width=0.95\textwidth]{figs/fig08.png}', '[height=8cm]{figs/fig08.png}'),
    (r'[width=0.95\textwidth]{figs/fig06.png}', '[height=7.5cm]{figs/fig06.png}'),
    (r'[width=0.95\textwidth]{figs/fig11.png}', '[height=7.5cm]{figs/fig11.png}'),
    (r'[width=0.7\textwidth]{figs/fig21.png}', '[height=6.5cm]{figs/fig21.png}'),
    (r'[width=0.85\textwidth]{figs/fig17.png}', '[width=0.7\textwidth]{figs/fig17.png}'),
    (r'[width=0.9\textwidth]{figs/fig02.png}', '[height=6cm]{figs/fig02.png}'),
    (r'[width=0.85\textwidth]{figs/fig03.png}', '[height=5cm]{figs/fig03.png}'),
    (r'[width=0.95\textwidth]{figs/fig10.png}', '[height=7cm]{figs/fig10.png}'),
    (r'[width=0.95\textwidth]{figs/fig12.png}', '[height=6.5cm]{figs/fig12.png}'),
    (r'[width=0.95\textwidth]{figs/fig04.png}', '[height=6.5cm]{figs/fig04.png}'),
]
count = 0
for old, new in replacements:
    if old in txt:
        txt = txt.replace(old, new)
        count += 1
    else:
        print(f"NOT FOUND: {old}")

open('paper.tex','w').write(txt)
print(f"Applied {count}/{len(replacements)} figure size caps")

# Recalculate total height
from PIL import Image
import os
total = 0
for f in sorted(os.listdir('figs')):
    if f.startswith('fig') and f.endswith('.png'):
        im = Image.open(os.path.join('figs',f))
        ar = im.size[1]/im.size[0]
        # find what constraint applies
        n = f.replace('fig','').replace('.png','').zfill(2)
        caps = {'13':9,'18':8.5,'07':8.5,'05':9,'08':8,'06':7.5,'11':7.5,'21':6.5,'02':6,'03':5,'10':7,'12':6.5,'04':6.5}
        if n in caps:
            h = caps[n]
        else:
            w_cm = 15.5 * 0.95
            h = w_cm * ar
        if f == 'fig22.png': h = 4
        if f == 'fig17.png': h = 15.5*0.7*ar
        total += h
print(f"New total figure height: {total:.0f} cm = {total/24.5:.1f} pages")
