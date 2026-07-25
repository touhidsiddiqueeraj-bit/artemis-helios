"""
gen_fig11_tree.py — Generate binary methodology tree SVG for the manuscript.
Output: fig_11_methodology_tree.svg
"""
import os, math

OUT = os.path.dirname(os.path.abspath(__file__))

SVG_W, SVG_H = 700, 520
LX, RX = 350, 350  # left/right margins
TOP = 30
LEVEL_H = 65
NODE_W = 170
NODE_H = 28

def node_rect(cx, cy, w, h):
    return f'x="{cx - w//2}" y="{cy - h//2}" width="{w}" height="{h}"'

def text_el(x, y, txt, size=9, bold=False, color='#1a1a1a'):
    fw = "bold" if bold else "normal"
    return f'<text x="{x}" y="{y}" text-anchor="middle" font-family="DejaVu Sans, Arial, sans-serif" font-size="{size}" font-weight="{fw}" fill="{color}">{txt}</text>'

def group(id_str, children):
    return f'<g id="{id_str}">\n{children}\n</g>'

def arrow(x1, y1, x2, y2):
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#555" stroke-width="1.2" marker-end="url(#arrowhead)"/>'

def arrow_down(cx, y1, y2):
    return arrow(cx, y1, cx, y2)

# Node data: (id, label1, label2, cx, cy, color)
def make_node(nid, label1, label2, cx, cy, color='#e8f0fe', border='#1a73e8'):
    r = node_rect(cx, cy, NODE_W, NODE_H)
    bg = f'<rect {r} rx="4" ry="4" fill="{color}" stroke="{border}" stroke-width="1.2"/>'
    txt1 = text_el(cx, cy - 4, label1, 9)
    txt2 = text_el(cx, cy + 10, label2, 7.5, color='#555')
    return (cx, cy, f'<g id="{nid}">\n{bg}\n{txt1}\n{txt2}\n</g>')

# Build tree
nodes_info = []

# Level 0: Root
cx0 = SVG_W // 2
cy0 = TOP + 0 * LEVEL_H
nodes_info.append(make_node('root', 'Field Data Collection', 'BH1750 · 10 s · Sylhet roof', cx0, cy0, '#e8f0fe', '#1a73e8'))

# Level 1: Two branches
cy1 = TOP + 1 * LEVEL_H
# Left: data processing
nodes_info.append(make_node('proc', 'Data Processing', '', cx0 - 150, cy1, '#fce8e6', '#c5221f'))
# Right: synthetic model
nodes_info.append(make_node('synth', 'Synthetic Model', 'Markov+OU R1–R4', cx0 + 150, cy1, '#e6f4ea', '#137333'))

# Level 2
cy2 = TOP + 2 * LEVEL_H
nodes_info.append(make_node('glass', 'Glass Attenuation', 'Back-to-back calib.', cx0 - 210, cy2, '#fef7e0', '#e37400'))
nodes_info.append(make_node('clean', 'Data Cleaning', '42 h usable · 18,395 rows', cx0 - 90, cy2, '#fef7e0', '#e37400'))
nodes_info.append(make_node('params', 'Parameterisation', 'NASA POWER [9] · SREDA [10]', cx0 + 90, cy2, '#e6f4ea', '#137333'))
nodes_info.append(make_node('mc', 'Monte Carlo Gen.', '30 July days · 10 seeds', cx0 + 210, cy2, '#e6f4ea', '#137333'))

# Level 3: Merge
cy3 = TOP + 3 * LEVEL_H
nodes_info.append(make_node('merge', 'Path B — Pattern Validation', 'Ramp-rate KS D = 0.402 · μ within 10%', cx0, cy3, '#f3e8fd', '#7b1fa2'))

# Level 4: Output
cy4 = TOP + 4 * LEVEL_H
nodes_info.append(make_node('output', 'MPPT Efficiency', 'MC: 94.0% · Field resampled: 93.5%', cx0, cy4, '#e8f0fe', '#1a73e8'))

# Arrows
arrows = []
arrows.append(arrow_down(cx0, cy0 + NODE_H//2, cy1 - NODE_H//2))

arrows.append(arrow(cx0, cy0 + NODE_H//2 + 8, cx0 - 150, cy1 - NODE_H//2))
arrows.append(arrow(cx0, cy0 + NODE_H//2 + 8, cx0 + 150, cy1 - NODE_H//2))
arrows.append(arrow(cx0 - 150, cy1 + NODE_H//2, cx0 - 210, cy2 - NODE_H//2))
arrows.append(arrow(cx0 - 150, cy1 + NODE_H//2, cx0 - 90, cy2 - NODE_H//2))
arrows.append(arrow(cx0 + 150, cy1 + NODE_H//2, cx0 + 90, cy2 - NODE_H//2))
arrows.append(arrow(cx0 + 150, cy1 + NODE_H//2, cx0 + 210, cy2 - NODE_H//2))
arrows.append(arrow(cx0 - 210, cy2 + NODE_H//2, cx0, cy3 - NODE_H//2))
arrows.append(arrow(cx0 - 90, cy2 + NODE_H//2, cx0, cy3 - NODE_H//2))
arrows.append(arrow(cx0 + 90, cy2 + NODE_H//2, cx0, cy3 - NODE_H//2))
arrows.append(arrow(cx0 + 210, cy2 + NODE_H//2, cx0, cy3 - NODE_H//2))
arrows.append(arrow_down(cx0, cy3 + NODE_H//2, cy4 - NODE_H//2))

# Build SVG
arrowhead_def = '''<defs>
  <marker id="arrowhead" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
    <polygon points="0 0, 8 3, 0 6" fill="#555"/>
  </marker>
</defs>'''

nodes_svg = "\n".join([n for _, _, n in nodes_info])
arrows_svg = "\n".join(arrows)

svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{SVG_W}" height="{SVG_H}" viewBox="0 0 {SVG_W} {SVG_H}">
{arrowhead_def}
<rect width="100%" height="100%" fill="#ffffff"/>
{arrows_svg}
{nodes_svg}
</svg>'''

out_path = os.path.join(OUT, 'fig_11_methodology_tree.svg')
with open(out_path, 'w') as f:
    f.write(svg)
print(f"Saved {out_path}")
print(f"Size: {os.path.getsize(out_path)} bytes")
