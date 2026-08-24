"""
Timing diagram for Section V — irradiance to PV response
Shows: Irradiance sensor (INA219 8.5ms) → Helios preprocess 7.58us → LSTM 6.359ms → packet 80.94us → UART 3.485ms → Artemis parse 79.7us → VS-P&O+blend 41.6us → PWM 19.9us → Converter 50kHz (20us period)
Full color, horizontal timeline.
"""
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches

fig, ax = plt.subplots(figsize=(7.0, 1.6))
ax.set_xlim(0, 105)
ax.set_ylim(0, 1)
ax.axis('off')

# Timeline from 0 to 100ms
# Segments: INA219 8.517, Helios 10.002, UART is part of Helios, Artemis 9.238, but end-to-end is 3.58ms from Helios TX start?
# For diagram, show sequential: INA219 (8.517) + Helios (10.002) is not sequential — INA219 is on Artemis, Helios is parallel
# Better show: Sensor → Helios (INA219 is actually on Artemis, but for diagram show as sensor acquisition)
# Sequence per Section V text: irradiance → INA219 8.517ms → Helios preprocess 7.58us → LSTM 6.359ms → packet 80.94us → UART 3.485ms → Artemis parse 79.7us → VS-P&O 41.6us → PWM 19.9us → Converter
segments = [
    ("INA219\n8.52 ms", 8.517, "#E76F51"),
    ("Preproc\n7.6 µs", 0.00758, "#F4A261"),
    ("LSTM\n6.36 ms", 6.359, "#2A9D8F"),
    ("Packet\n80.9 µs", 0.08094, "#E9C46A"),
    ("UART\n3.48 ms", 3.4847, "#264653"),
    ("Parse\n79.7 µs", 0.0797, "#C73E1D"),
    ("VS-P&O\n41.6 µs", 0.0416, "#F4A261"),
    ("PWM\n19.9 µs", 0.0199, "#2E86AB"),
]

x=0
for label, w, color in segments:
    ax.add_patch(patches.Rectangle((x, 0.35), w, 0.3, facecolor=color, edgecolor='black', linewidth=0.7))
    # Label inside if wide enough, else above
    if w > 1.0:
        ax.text(x+w/2, 0.50, label, ha='center', va='center', fontsize=5.5, weight='bold', color='white' if color=="#264653" else 'black')
    else:
        ax.text(x+w/2, 0.75, label, ha='center', va='bottom', fontsize=5, rotation=0)
    x += w

# Idle and budget
ax.add_patch(patches.Rectangle((x, 0.35), 100-x, 0.3, facecolor='#EEEEEE', edgecolor='black', linewidth=0.7, hatch='///', alpha=0.5))
ax.text((100+x)/2, 0.50, "Idle ~86 ms", ha='center', va='center', fontsize=6, style='italic')
ax.text(100, 0.80, "100 ms", ha='right', va='bottom', fontsize=6, style='italic')
ax.axvline(100, color='black', ls='--', lw=0.8)

ax.text(0.5, 0.85, "Irradiance → Sensor → Helios → UART → Artemis → PWM → Converter (50 kHz)", ha='left', va='center', fontsize=7, weight='bold')
ax.set_title("Signal-path timing diagram (single 100 ms control cycle, not to scale for µs segments)", fontsize=7, pad=8)

plt.tight_layout()
plt.savefig("Figures/timing_diagram.png", dpi=600, bbox_inches='tight', pad_inches=0.04)
print("saved Figures/timing_diagram.png")
