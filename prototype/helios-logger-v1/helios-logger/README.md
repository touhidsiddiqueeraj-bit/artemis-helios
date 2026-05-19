# Helios Data Logger — v1.0
### ESP32-S3 N16R8 · GY302 · OV2640 · LittleFS

---

## What It Does

Logs real-world irradiance and sky condition data from Sylhet to
internal flash for 14 days. Used to validate the Markov+OU
simulation model in the Helios-Artemis paper.

| Parameter        | Value                          |
|------------------|--------------------------------|
| Sampling interval| 10 seconds                     |
| Trigger          | Lux-based (start >20, stop <8) |
| Storage          | LittleFS on internal 16MB flash|
| Files            | One CSV per day (day_01.csv …) |
| Web UI           | http://helios.local            |
| AP SSID          | Helios-Logger                  |
| AP Password      | helios2026                     |

---

## Hardware

| Component     | Notes                                      |
|---------------|--------------------------------------------|
| ESP32-S3 N16R8| Seeed XIAO ESP32S3 Sense recommended       |
| GY302       | I²C light sensor — connects to SDA/SCL     |
| OV2640        | Embedded on XIAO ESP32S3 Sense             |
| SD card       | Not used in v1 — LittleFS on internal flash|

### GY302 Wiring (XIAO ESP32S3)

```
GY302   →   XIAO ESP32S3
VIN       →   3.3V
GND       →   GND
SDA       →   D4 (GPIO 8)
SCL       →   D5 (GPIO 9)
```

---

## Arduino IDE Setup

### 1. Board
- **Board:** ESP32S3 Dev Module (or Seeed XIAO ESP32S3)
- **Flash Size:** 16MB
- **Partition Scheme:** Select "Custom" and point to `partitions.csv`
  - Or use "8M with spiffs" and it will still work (~4MB data)
- **PSRAM:** OPI PSRAM (for N16**R8**)
- **Upload Speed:** 921600

### 2. Libraries (install via Library Manager)
- No additional library needed (direct I²C)
- `Adafruit Unified Sensor` by Adafruit
- ESP32 Arduino Core ≥ 2.0.17 (includes LittleFS, WebServer, ESPmDNS, camera driver)

### 3. Custom Partition Table
In Arduino IDE, go to:
`Sketch → Show Sketch Folder`

Place `partitions.csv` in the same folder as the `.ino` file.
Then in Tools → Partition Scheme → select **"Custom"**.

If "Custom" is not available in your Arduino IDE version, use
**"Default 8MB with spiffs"** — it gives ~3.5MB for LittleFS which
covers ~16 days at 10s interval.

---

## CSV Format

Each day file (`/data/day_01.csv` through `day_14.csv`) contains:

```
uptime_ms, lux, irradiance_wm2, blue_channel, gy302_raw, integration_ms
```

| Column          | Description                                      |
|-----------------|--------------------------------------------------|
| `uptime_ms`     | Milliseconds since logging started this day      |
| `lux`           | GY302 measured lux                             |
| `irradiance_wm2`| Estimated W/m² (lux / 116)                      |
| `blue_channel`  | OV2640 mean blue value 0–255 (sky condition)     |
| `gy302_raw`    | Raw 16-bit BH1750 reading                        |
| `integration_ms`| GY302 integration time in ms                   |

### Lux → W/m² Conversion
Uses the standard AM1.5 solar spectrum approximation:
`1 W/m² ≈ 116 lux`

This is appropriate for broadband solar irradiance but is an
approximation. For the simulation validation comparison, use the
**relative shape** of the irradiance curve (cloud variability index,
transient frequency) more than the absolute W/m² values.

---

## Web Dashboard

Connect your phone or laptop to the `Helios-Logger` WiFi AP.

Any browser request triggers the **captive portal** which opens the
dashboard automatically. Or navigate to:

- `http://helios.local` (mDNS — works on most phones/laptops)
- `http://192.168.4.1` (direct IP — always works)

### Dashboard Features
- **Live readings** — irradiance, lux, blue channel, sample count
- **Flash usage bar** — shows remaining storage
- **Time series chart** — 1h / 3h / 6h / All views
- **File browser** — see all day files with sizes
- **Download** — tap ↓ CSV next to any file to download
- **Delete** — per-file or wipe all

---

## Comparing to Simulation

### What to look for

1. **Cloud Variability Index (CVI)**
   Compute from your logged data:
   `CVI = std(irradiance) / mean(irradiance)` over each day.
   Compare to Table I in the paper (July target CVI ≈ 0.85).

2. **Irradiance transient frequency**
   Count how many times irradiance drops >15% in a 10-minute window.
   This should match the Markov transition matrix in the simulation.

3. **Blue channel correlation**
   Low blue channel → heavy cloud cover → low irradiance.
   Pearson correlation between blue channel and irradiance should be
   strong (r > 0.7) on variable days.

4. **Peak GHI**
   Simulation models peak at 744 W/m² for July (aerosol-capped).
   Your measured peak irradiance should be in this neighbourhood.

### Python analysis starter

```python
import pandas as pd
import numpy as np

df = pd.read_csv('day_01.csv')
df['t_min'] = df['uptime_ms'] / 60000

# Cloud Variability Index
cvi = df['irradiance_wm2'].std() / df['irradiance_wm2'].mean()
print(f"CVI: {cvi:.3f}  (simulation target July: 0.85)")

# Peak irradiance
print(f"Peak GHI: {df['irradiance_wm2'].max():.3f} W/m²")

# Blue-irradiance correlation
r = df['irradiance_wm2'].corr(df['blue_channel'])
print(f"Blue-Irradiance Pearson r: {r:.3f}")
```

---

## Troubleshooting

| Symptom                     | Likely cause & fix                              |
|-----------------------------|-------------------------------------------------|
| GY302 not found           | Check SDA/SCL wiring; confirm 3.3V not 5V       |
| Camera init failed          | Camera still logs lux-only; check board variant |
| LittleFS mount failed       | Flash partition mismatch — check partition CSV  |
| helios.local not resolving  | Use 192.168.4.1 directly; mDNS fails on Android |
| Blue channel always 0       | Camera failed silently; blue_channel = 0 in CSV |
| Dashboard not loading       | dashboard.h too large — check PROGMEM compiles  |

---

## File Structure

```
helios-logger/
├── helios_logger.ino   ← Main firmware
├── camera_pins.h       ← OV2640 GPIO mapping
├── dashboard.h         ← Web UI (PROGMEM)
├── partitions.csv      ← Custom flash partition table
└── README.md           ← This file
```

---

## Next Steps (v2)

- Add DS3231 RTC for wall-clock timestamps
- Add BME280 for temperature + humidity logging
- Log full JPEG frames at configurable intervals
- OTA firmware update from dashboard
- Automatic daily email/FTP of CSV via WiFi station mode
