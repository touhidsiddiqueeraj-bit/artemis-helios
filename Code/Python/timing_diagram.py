"""
Timing diagram for Section V — irradiance to PV response
Two-row, full color, readable at 100% zoom, grouped tiny segments, larger fonts
"""
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.gridspec as gridspec

fig = plt.figure(figsize=(7.2, 2.4))
gs = gridspec.GridSpec(2, 1, height_ratios=[1, 1], hspace=0.40, left=0.08, right=0.97, top=0.88, bottom=0.12)

# Helios row — group tiny Preproc+Packet as visible 1.0ms block
ax = fig.add_subplot(gs[0])
ax.set_xlim(0, 100)
ax.set_ylim(0, 1)
ax.axis('off')
segments_h = [
    ("INA219\n8.52 ms", 8.517, "#E76F51"),
    ("LSTM\n6.36 ms", 6.359, "#2A9D8F"),
    ("UART\n3.48 ms", 3.4847, "#264653"),
    ("Other\n0.09 ms", 1.0, "#F4A261"), # Preproc 7.58 + Packet 80.94 = 0.088, display as 1.0 for visibility
]
x=0
for label, w, color in segments_h:
    ax.add_patch(patches.Rectangle((x, 0.30), w, 0.40, facecolor=color, edgecolor='black', linewidth=0.8))
    ax.text(x+w/2, 0.50, label, ha='center', va='center', fontsize=7, weight='bold', color='white' if color=="#264653" else 'black')
    x+=w
ax.add_patch(patches.Rectangle((x, 0.30), 100-x, 0.40, facecolor='#EEEEEE', edgecolor='black', linewidth=0.7, hatch='///', alpha=0.4))
ax.text((100+x)/2, 0.50, "Idle ~86 ms", ha='center', va='center', fontsize=7, style='italic')
ax.text(100, 0.82, "100 ms", ha='right', va='bottom', fontsize=7, style='italic')
ax.axvline(100, color='black', ls='--', lw=1.0)
ax.set_title("Helios (ESP32-S3) + Sensor  —  10.00 ms tick", fontsize=8, weight='bold', loc='left')

# Artemis row
ax2 = fig.add_subplot(gs[1])
ax2.set_xlim(0, 100)
ax2.set_ylim(0, 1)
ax2.axis('off')
segments_a = [
    ("INA219\n8.52 ms", 8.517, "#E76F51"),
    ("Parse+VS\n0.14 ms", 1.0, "#F4A261"), # Parse 79.7 + VS-P&O 41.6 + PWM 19.9 = 0.141, display as 1.0
    ("UART TX\n0.60 ms", 0.600, "#2A9D8F"),
]
x=0
for label, w, color in segments_a:
    ax2.add_patch(patches.Rectangle((x, 0.30), w, 0.40, facecolor=color, edgecolor='black', linewidth=0.8))
    ax2.text(x+w/2, 0.50, label, ha='center', va='center', fontsize=7, weight='bold', color='black')
    x+=w
ax2.add_patch(patches.Rectangle((x, 0.30), 100-x, 0.40, facecolor='#EEEEEE', edgecolor='black', linewidth=0.7, hatch='///', alpha=0.4))
ax2.text((100+x)/2, 0.50, "Idle ~90 ms", ha='center', va='center', fontsize=7, style='italic')
ax2.text(100, 0.82, "100 ms", ha='right', va='bottom', fontsize=7, style='italic')
ax2.axvline(100, color='black', ls='--', lw=1.0)
ax2.set_title("Artemis (STM32F103) + Converter  —  9.24 ms tick, Buck 50 kHz (20 µs)", fontsize=8, weight='bold', loc='left')

plt.tight_layout()
plt.savefig("Figures/timing_diagram.png", dpi=600, bbox_inches='tight', pad_inches=0.04)
print("saved Figures/timing_diagram.png")
