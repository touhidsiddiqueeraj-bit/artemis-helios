# Helios Data Logger — v2.6

**ESP32-S3 N16R8 WROOM · GY-302 (BH1750) · OV2640 · DS3231 RTC**

Logs solar irradiance, sky blue channel, temperature, and sky images to internal flash for multi-day field deployment. Validates the Markov+OU simulation model in the Helios-Artemis paper.

---

## Features

### Core Logging
- **10-second sampling** (configurable 5–300 s) of irradiance + blue channel + die temp
- **Solar-scheduled** — auto-starts at sunrise, stops at sunset (NOAA-based calculator)
- **No RTC needed for basic lux-triggered mode** — but DS3231 gives wall-clock timestamps
- **Outlier filter** — rejects single-sample spikes >10× previous (configurable)
- **Night confirmation** — requires N consecutive below-threshold readings to stop (default 5)
- **Manual override** — force logging outside solar hours via dashboard button

### Sky Images
- 96×96 JPEG captured every 3 minutes during daylight (interval configurable 1–60 min)
- Stored in `/imgs/YYYYMMDD_HHMMSS.jpg`
- Viewable in dashboard gallery

### Power Management
- **Light sleep** between samples (~8 s sleep, ~2 s awake) = ~60 mA during logging
- **WiFi on-demand** — AP starts on boot, auto-off 5 min after last client disconnects
- **Thermal cooldown** — pauses logging if die temp exceeds threshold (default 75°C), resumes 10°C below threshold (hysteresis)
- **Night idle** — lux-triggered sleep ~2 mA

### Web Dashboard
- **Live readings** — irradiance (W/m²), illuminance (lux), sky blue (0–255), die temp (°C), sample count, elapsed time, RTC date/time, WiFi clients
- **Day/Night banner** — shows sunrise/sunset times, override state
- **Chart** — irradiance + temperature + blue channel, with 1h/3h/6h/All zoom, day history selector
- **CVI Summary Table** — per-day coefficient of variation, mean/peak irradiance, mean temp
- **Sky image gallery** — latest 48 thumbnails
- **Live camera** — MJPEG stream and snapshot (QVGA)
- **RTC setter** — manual date/time input or "Use Browser Time"
- **File browser** — CSV download, per-file and bulk delete
- **Settings page** — all parameters adjustable at runtime
- **OTA firmware update** — drag-and-drop .bin upload
- **Web serial monitor** — SSE-based wireless log viewer at `/serial`

### Data Integrity
- **CSV tail repair** — auto-truncates incomplete last line after unexpected power loss
- **Write buffering** — buffers N samples in RAM before flash write (default 10, reduces wear)
- **Day summary** — `/data/summary.csv` with CVI, peak irradiance, mean temp, thermal events
- **Thermal event log** — `/data/YYYY-MM-DD.therm` with timestamp + die temp

---

## Hardware

### Bill of Materials

| Component                 | Notes                                                            |
|---------------------------|------------------------------------------------------------------|
| ESP32-S3 N16R8 WROOM     | Module with PSRAM; e.g. ESP32-S3-DevKitC-1 or similar            |
| GY-302 (BH1750)          | I²C digital light sensor, 1–65535 lux                            |
| OV2640 camera module     | 2MP, connected via DVP parallel bus                              |
| DS3231 RTC module        | Precision RTC with battery backup                                |
| WS2812B NeoPixel         | Built-in on many ESP32-S3 WROOM boards (GPIO 48); status LED     |
| Push button (x2)         | BOOT button on GPIO 0 (built-in) + optional external on GPIO 3   |
| 12V 7Ah battery          | Field deployment power source                                    |
| LM2596 DC-DC converter   | 12V → 5V, ~78% efficiency for ESP32                              |

### Wiring

```
GY-302 (BH1750)
  SDA → GPIO 45
  SCL → GPIO 46
  VCC → 3.3V
  GND → GND
  ADDR → GND (0x23)

DS3231 RTC
  SDA → GPIO 45   (shares I2C bus with BH1750 — no conflict, 0x68 vs 0x23)
  SCL → GPIO 46
  VCC → 3.3V
  GND → GND

OV2640 Camera (ESP32-S3 WROOM)
  XCLK  → GPIO 15   VSYNC → GPIO 6    HREF  → GPIO 7
  PCLK  → GPIO 13   SIOD  → GPIO 4    SIOC  → GPIO 5
  D0    → GPIO 11   D1    → GPIO 9    D2    → GPIO 8
  D3    → GPIO 10   D4    → GPIO 12   D5    → GPIO 18
  D6    → GPIO 17   D7    → GPIO 16
  PWDN  → -1 (none)  RESET → -1 (none)

Buttons
  BOOT  → GPIO 0    (built-in, INPUT_PULLUP)
  EXT   → GPIO 3    (external, other leg to GND)

Status LED
  GPIO 48 — WS2812B NeoPixel (built-in on many S3 WROOM boards)
```

<details>
<summary>Pin reference for XIAO ESP32S3 Sense (alternative board)</summary>

```
GY-302: SDA→GPIO 8, SCL→GPIO 9
OV2640: XCLK→10, SIOD→40, SIOC→39, Y9→48, Y8→11, Y7→12, Y6→14,
        Y5→16, Y4→18, Y3→17, Y2→15, VSYNC→38, HREF→47, PCLK→13
```

To use the XIAO Sense pinout, comment out the WROOM pin block and uncomment the XIAO block in `helios_logger.ino` (Section 1, lines ~147–162). Note: XIAO Sense does NOT have a WS2812B on GPIO 48 — the status LED NeoPixel code must be disabled for that board.
</details>

---

## Arduino IDE Setup

### Board Configuration

| Setting               | Value                                          |
|-----------------------|------------------------------------------------|
| Board                 | ESP32S3 Dev Module                             |
| Flash Size            | 16MB                                           |
| Partition Scheme      | Custom (16M Flash: 2MB APP / 12.5MB FATFS)     |
| PSRAM                 | OPI PSRAM                                      |
| Upload Speed          | 921600                                         |

### Required Partition Table

Flash layout (defined in `partitions.csv`):

| Partition | Size   |
|-----------|--------|
| bootloader| 64 KB  |
| nvs       | 20 KB  |
| otadata   | 8 KB   |
| app0      | 2 MB   |
| ffat      | 12.5 MB|

Without the custom partition, `FFat.begin()` will fail. Place `partitions.csv` in the sketch folder and select **Tools → Partition Scheme → Custom**.

### Libraries

Install via Arduino Library Manager:

| Library         | Author               | Search term        |
|-----------------|----------------------|--------------------|
| BH1750          | Christopher Laws     | `BH1750`           |
| RTClib          | Adafruit             | `RTClib`           |
| Adafruit NeoPixel | Adafruit           | `NeoPixel`         |
| ESP32 Arduino   | Espressif (≥ 2.0.17) | (board package)    |

No ArduinoJson dependency — config parsing uses hand-written string search.

---

## First Boot Setup Wizard

On first power-on, Helios serves a setup wizard instead of the dashboard:

1. **Step 0 — Date & Time**: Set current wall-clock time (or tap "Use Phone Time")
2. **Step 1 — Location**: Enter latitude/longitude (or tap "GPS" for browser geolocation)
3. **Step 2 — Deploy Duration**: How many days the logger will run
4. **Finish**: Saves config, RTC is set, device reboots to dashboard

The wizard can be re-triggered from the Settings page (Danger Zone → "Re-run Setup Wizard") or by calling `POST /api/reset-setup`.

**Fallback coordinates** (if wizard skipped): 24.9°N, 91.9°E (Sylhet, Bangladesh), UTC+6.

---

## Dashboard

Connect to WiFi AP `Helios-Logger` (password: `helios2026`). Open http://192.168.4.1 or http://helios.local (mDNS).

### Panels

| Section              | Content                                                          |
|----------------------|------------------------------------------------------------------|
| **Day/Night Banner** | Sunrise/sunset times, day/night/override state, Force Start btn  |
| **Live Readings**    | Irradiance, illuminance, sky blue, die temp (with bar), samples, elapsed time, rejected outliers, RTC date/time, WiFi clients |
| **Today's Schedule** | Sunrise, sunset, time until next image, image count today        |
| **Flash Storage**    | Used/free space bar                                              |
| **Chart**            | Irradiance + temperature + blue channel with time range selector |
| **Day History**      | Click any past day to load its CSV into the chart                |
| **Daily CVI**        | Per-day summary table: samples, duration, mean/peak W/m², CVI, mean temp |
| **Set RTC**          | Manual date/time input or auto-fill from browser                 |
| **Live Camera**      | Stream (MJPEG) or snapshot buttons, frame counter, FPS           |
| **Sky Images**       | Thumbnail gallery of latest 48 images                            |
| **Files**            | CSV file list with download/delete                               |
| **OTA**              | Drag-and-drop firmware upload                                    |
| **Serial Monitor**   | SSE-based live log viewer at `/serial`                           |
| **Settings**         | Link to `/settings` page                                         |

### Dashboard Auto-Refresh

| Data        | Interval |
|-------------|----------|
| Status      | 5 s      |
| Live chart  | 10 s     |
| File list   | 30 s     |
| Images      | 60 s     |
| Summary     | 30 s     |

---

## Settings Page (`/settings`)

All parameters saved to `/data/config.json`, loaded on boot, apply immediately.

| Parameter           | Default | Range    | Description                                      |
|---------------------|---------|----------|--------------------------------------------------|
| Sample Interval     | 10 s    | 5–300    | Time between sensor readings                     |
| Image Interval      | 3 min   | 1–60     | How often sky images are captured                |
| JPEG Quality        | 5       | 1–63     | Image compression (1=smallest file)              |
| Write Buffer Size   | 10      | 1–50     | Samples to buffer before flash write             |
| WiFi Auto-Off       | 5 min   | 1–120    | AP shuts down after last client leaves           |
| Outlier Factor      | 10×     | 2–100    | Reject reading if lux jumps >N× previous         |
| Night Confirm       | 5       | 1–20     | Consecutive low-lux readings to confirm sunset   |
| Shutdown Temp       | 75°C    | 0–85     | Die temp threshold (0 = disabled)                |
| Cooldown Duration   | 120 s   | 30–600   | Thermal sleep duration                           |

---

## API Reference

All endpoints serve from the ESP32's IP when WiFi is active.

### Web Pages

| Endpoint       | Method | Description                            |
|----------------|--------|----------------------------------------|
| `/`            | GET    | Dashboard (or setup wizard if not done)|
| `/setup`       | GET    | Setup wizard page                      |
| `/settings`    | GET    | Settings page                          |
| `/serial`      | GET    | Web serial monitor                     |
| `/ota`         | GET    | OTA firmware upload page               |

### JSON API

| Endpoint              | Method | Description                                 |
|-----------------------|--------|---------------------------------------------|
| `/api/status`         | GET    | Full device status (logging, samples, temp, RTC, storage, WiFi, sunrise/sunset, latest sample) |
| `/api/live?count=N`   | GET    | Last N samples from ring buffer (max 360)   |
| `/api/files`          | GET    | List CSV + meta files in `/data/` with row count |
| `/api/images`         | GET    | List image files in `/imgs/`                |
| `/api/config`         | GET    | Get current config JSON                     |
| `/api/config`         | POST   | Save config JSON (body: `application/json`) |
| `/api/override?on=1`  | GET    | Toggle manual logging override on/off       |
| `/api/settime`        | GET    | Set RTC (`?date=YYYY-MM-DD&time=HH:MM:SS`)  |
| `/api/setup`          | POST   | Submit setup wizard (body: JSON + query params) |
| `/api/reset-setup`    | POST   | Re-enable setup wizard on next connect       |
| `/api/delete?file=`   | DELETE | Delete a single CSV                          |
| `/api/deleteall`      | DELETE | Wipe all CSV + images                        |

### Camera

| Endpoint              | Method | Description                                 |
|-----------------------|--------|---------------------------------------------|
| `/api/cam-snapshot`   | GET    | Single JPEG snapshot                        |
| `/api/cam-stream`     | GET    | MJPEG multipart stream (open in `<img>` tag)|
| `/api/cam-stats`      | GET    | Stream frame count + FPS                    |

### OTA

| Endpoint              | Method | Description                                 |
|-----------------------|--------|---------------------------------------------|
| `/api/ota-upload`     | POST   | Upload .bin firmware (multipart, field `firmware`) |

### Serial Monitor (SSE)

| Endpoint              | Method | Description                                 |
|-----------------------|--------|---------------------------------------------|
| `/serial/stream`      | GET    | SSE event stream of log lines               |
| `/serial/dump`        | GET    | Plain-text dump of last 120 log lines       |

### Captive Portal

The following endpoints all return a 302 redirect to `/`:
`/generate_204`, `/connecttest.txt`, `/hotspot-detect.html`, `/library/test/success.html`, `/ncsi.txt`, `/redirect`, `/canonical.html`, and any unmatched route (before setup is complete).

---

## Data Formats

### CSV (`/data/YYYY-MM-DD.csv`)

```
date,time,elapsed_s,lux,irradiance_wm2,blue_channel,temp_c
2026-06-12,06:15:00,0,125.4,1.0810,42,31.2
2026-06-12,06:15:10,10,127.1,1.0957,44,31.3
```

| Column           | Description                                      |
|------------------|--------------------------------------------------|
| `date`           | Wall-clock date (RTC)                            |
| `time`           | Wall-clock time (RTC)                            |
| `elapsed_s`      | Seconds since logging started this day            |
| `lux`            | BH1750 measured illuminance                       |
| `irradiance_wm2` | Estimated W/m² = lux ÷ 116 (AM1.5 approximation) |
| `blue_channel`   | Mean blue chroma 0–255 from camera (RGB565)       |
| `temp_c`         | ESP32-S3 internal die temperature                 |

### Summary (`/data/summary.csv`)

```
date,samples,duration_s,mean_irr,std_irr,cvi,peak_irr,mean_temp_c,thermal_events
2026-06-12,3240,32400,0.4821,0.4012,0.832,0.744,42.5,0
```

### Sky Images (`/imgs/YYYYMMDD_HHMMSS.jpg`)

- 96×96 pixels, configurable JPEG quality (default 5)
- Software-compressed from RGB565 via `frame2jpg()` — no camera deinit/reinit

### Thermal Events (`/data/YYYY-MM-DD.therm`)

```
date,time,die_temp_c,threshold_c,sleep_s
2026-06-12,12:30:00,76.2,75.0,120
```

---

## Lux → W/m² Conversion

Uses the standard AM1.5 solar spectrum approximation:

```
1 W/m² ≈ 116 lux
```

This is appropriate for broadband solar irradiance but is an approximation. For simulation validation, use the **relative shape** of the irradiance curve (CVI, transient frequency) more than absolute W/m².

---

## Solar Calculator

An on-device implementation of the NOAA sunrise/sunset algorithm based on Jean Meeus' *Astronomical Algorithms* (referenced in the code as the "Meeus algorithm"). No lookup tables, no NTP dependence — pure `math.h` on the ESP32.

### Algorithm Steps (implemented in `calcSunTimes()`, Section 2 of firmware)

```
Input:  yr, mo, dy, lat, lon, utcOffsetHours
Output: srMin, ssMin (minutes from local midnight)
```

| Step | Computation |
|------|-------------|
| 1 | Convert Gregorian date to **Julian Day Number** (`dateToJulian()`: `JDN = floor(365.25×(Y+4716)) + floor(30.6001×(M+1)) + D + B - 1524.5`, with B = 2 - floor(Y/100) + floor(floor(Y/100)/4)) |
| 2 | Compute **Julian centuries from J2000**: `T = (JDN - 2451545.0) / 36525.0` |
| 3 | **Solar mean longitude** (corrected for precession): `L0 = 280.46646 + T·(36000.76983 + T·0.0003032) mod 360` |
| 4 | **Solar mean anomaly**: `M = 357.52911 + T·(35999.05029 - T·0.0001537) mod 360` |
| 5 | **Equation of centre** (series expansion in sine of M): `C = (1.914602 - T·0.004817 - T²·0.000014)·sin(Mrad) + (0.019993 - 0.000101·T)·sin(2·Mrad) + 0.000289·sin(3·Mrad)` |
| 6 | **True longitude**: `sunLon = L0 + C` |
| 7 | **Apparent longitude** (nutation correction): `lambda = sunLon - 0.00569 - 0.00478·sin(ω·DEG)` where ω = 125.04 - 1934.136·T is the ascending node |
| 8 | **Obliquity of ecliptic** (corrected for nutation): `eps0 = 23 + (26 + (21.448 - T·(46.8150 + T·(0.00059 - T·0.001813)))/60)/60`, then `eps = eps0 + 0.00256·cos(ω·DEG)` |
| 9 | **Sun declination**: `sinDec = sin(eps·DEG)·sin(lambda·DEG)`, `decl = asin(sinDec)` (radians) |
| 10 | **Equation of time** (minutes): computes `y = tan²(eps/2)`, then `EqT = 4·[y·sin(2·L0) - 2·e·sin(M) + 4·e·y·sin(M)·cos(2·L0) - 0.5·y²·sin(4·L0) - 1.25·e²·sin(2·M)]` in degrees, converted to minutes. e = 0.016708634 - T·0.000042037 is the Earth's orbital eccentricity |
| 11 | **Hour angle for sunrise**: `cosHA = (cos(90.833°) - sin(latRad)·sin(decl)) / (cos(latRad)·cos(decl))`. The 90.833° zenith accounts for: 90° geometric zenith + 0.833° atmospheric refraction (at horizon) + solar semidiameter (~0.5° for half the disc). **Polar check**: if `cosHA < -1` (midnight sun) or `> 1` (polar night), returns false. |
| 12 | **Solar noon** (local minutes from midnight): `solarNoon = 720 - 4·lon + EqT + utcOffset·60` |
| 13 | **Sunrise/sunset**: `sr = solarNoon - 4·HA`, `ss = solarNoon + 4·HA`. Values clamped to [0, 1440). |

### Reference

The calculation is called every `loop()` iteration inside `isDaytime()`:

```cpp
bool isDaytime() {
  if (manualOverride) return true;
  if (!rtcReady) return false;
  DateTime now = rtc.now();
  int srMin, ssMin;
  getSunTimesForDate(now.year(), now.month(), now.day(), srMin, ssMin);
  int nowMin = now.hour() * 60 + now.minute();
  return (nowMin >= srMin && nowMin < ssMin);
}
```

Execution time: < 1 ms on ESP32-S3 at 240 MHz (no floating-point hardware, but the FPU handles doubles efficiently).

### Fallback

If no RTC is available, `isDaytime()` returns `false` and logging must be triggered via the BH1750 lux-threshold mechanism (v1.x style). The RTC is required for the solar calculator to work — without it, the device cannot know wall-clock time.

---

## Power Strategy

### Logging Mode (daytime, WiFi off)

| State         | Current | Duration per cycle |
|---------------|---------|--------------------|
| Reading       | ~280 mA | ~200 ms            |
| Processing    | ~280 mA | ~800 ms            |
| Flash write   | ~280 mA | ~200 ms            |
| Light sleep   | ~30 mA  | ~8.8 s             |
| **Average**   | **~60 mA** | 10 s cycle       |

### Night Mode (~2 mA average)

ESP32 in light sleep, camera powered down, no sensors polled.

### WiFi Active

Adds ~100 mA during AP operation. Auto-shuts off 5 min after last client disconnects.

### Thermal Cooldown

Camera and WiFi powered down, repeated short sleeps polling die temp until 10°C below threshold.

### Battery Life Estimate (12V 7Ah + LM2596 @ 78% eff)

| Condition     | 5V draw  | 12V draw | Daily   |
|---------------|----------|----------|---------|
| Logging 12 h  | 60 mA    | ~32 mA   | 384 mAh |
| Night 12 h    | 2 mA     | ~1 mA    | 12 mAh  |
| **Total**     |          |          | 396 mAh |

**Runtime: 7000 / 396 ≈ 17.7 days** — covers a 14-day deployment.

---

## Status LED (GPIO 48 — WS2812B NeoPixel)

The ESP32-S3 WROOM module has a built-in RGB NeoPixel on GPIO 48 (not a plain GPIO — requires NeoPixel library).

| Mode          | Color   | Pattern              | Meaning                            |
|---------------|---------|----------------------|------------------------------------|
| `LED_SOLID`   | Blue    | Solid on             | WiFi AP active                     |
| `LED_SLOW_BLINK` | Green | 900 ms on / 100 ms off | Daytime logging active            |
| `LED_FAST_BLINK` | Red   | 80 ms on / 80 ms off  | RTC error or thermal cooldown     |
| `LED_OFF`     | —       | Off                  | Night/idle, RTC ready, not logging |

**Priority**: WiFi (blue) > RTC error/thermal (fast red) > logging (slow green) > off.

---

## v2.6 Camera Fix — Permanent GDMA Solution

### The Problem

`esp_camera_deinit()` calls `gdma_disconnect()` without calling `gdma_stop()` first. If any DMA transaction is in-flight, the DMA channel enters an undefined hardware state. The subsequent `esp_camera_init()` inherits that corruption → `FB-OVF` → cascade failure.

### The Fix

**Zero deinit/reinit cycles.** The camera is initialized ONCE in RGB565 mode (QQVGA, `GRAB_WHEN_EMPTY`, `fb_count=1`) and stays in that mode forever. JPEG images for storage and the web stream are produced by `frame2jpg()` — the same software encoder the driver uses internally. This:
- Eliminates all DMA mode switches
- Removes all drain loops, delays, and tick-guards
- Produces identical JPEG quality and size (96×96, configurable quality)
- Reduces code complexity (camEnterJpeg/camRestoreRgb are now no-ops)

---

## OTA Updates

1. Navigate to `/ota` in the dashboard
2. Drag and drop a compiled `.bin` file onto the upload area (or click to browse)
3. Click "Flash Firmware"
4. Progress bar shows upload status; device reboots automatically after flashing

The binary must be compiled with the same partition scheme (2MB APP). Uploading an incompatible image will NOT brick the device — the ESP32 falls back to the previous app partition on failed boot (thanks to `otadata`).

---

## Troubleshooting

| Symptom                     | Likely cause & fix                                            |
|-----------------------------|---------------------------------------------------------------|
| `FFat.begin()` fails        | Wrong partition scheme — use custom 16MB (2MB APP / 12.5MB FFat) |
| BH1750 not found            | Check SDA→GPIO45, SCL→GPIO46, 3.3V supply                     |
| DS3231 not found            | Check address 0x68, SDA/SCL on the same bus as BH1750         |
| Camera init fails           | Check pin wiring, PSRAM enabled, 20 MHz XCLK                  |
| FB-OVF / gdma_disconnect    | Update to v2.6 — the permanent fix eliminates deinit/reinit   |
| `helios.local` not resolving| mDNS fails on Android; use `192.168.4.1` directly             |
| No samples after sunrise    | Check RTC time is correct — solar calculator uses it          |
| Images not appearing        | Check `/imgs/` directory exists (created on boot)             |
| WDT reboot                  | Thermal shutdown or blocking loop — check serial log          |
| Brownout detected           | Power supply unstable — check battery/regulator               |
| Blue channel always 0       | Camera not ready — check cam init log message                 |
| Dashboard says "IDLE" during day | Manual override may be off, or RTC date incorrect        |

---

## Changelog

| Version | Key changes                                                      |
|---------|------------------------------------------------------------------|
| v1.0    | Initial release: LittleFS, lux-triggered, XIAO Sense             |
| v1.5    | Sky images, settings page, GPIO3 button, light theme             |
| v1.9    | Embedded OTA, no Guardian required                               |
| v2.0    | Custom partition 2MB+12.5MB FFat                                 |
| v2.1    | Thermal protection, status LED (WS2812B), die temp readout       |
| v2.2    | WDT reset in cooldown, boot reason logging, thermal event count, NTP sync, CSV tail repair |
| v2.3    | FB-OVF drain + 80ms settle                                       |
| v2.4    | Double drain for deinit path                                     |
| v2.5    | Fixed restore-path drain bug, didSample guard                    |
| **v2.6**| **Permanent GDMA fix**: single RGB565 init, `frame2jpg()` for JPEG, zero deinit/reinit, all workarounds removed |

---

## File Structure

```
helios-logger/
├── helios_logger.ino     ← Main firmware (3943 lines, single-file)
├── README.md             ← This file
```

On the device at runtime (FFat):

```
/data/
├── YYYY-MM-DD.csv        ← Daily CSV logs
├── YYYY-MM-DD.meta       ← Expected row count metadata
├── YYYY-MM-DD.therm      ← Thermal event log
├── summary.csv           ← Per-day CVI summary
└── config.json           ← Runtime configuration

/imgs/
├── YYYYMMDD_HHMMSS.jpg   ← Sky images
```

---

## Data Analysis (Python Starter)

```python
import pandas as pd, numpy as np

df = pd.read_csv('2026-06-12.csv')

# Cloud Variability Index
cvi = df['irradiance_wm2'].std() / df['irradiance_wm2'].mean()
print(f"CVI: {cvi:.3f}  (July target: ~0.85)")

# Peak irradiance
print(f"Peak GHI: {df['irradiance_wm2'].max():.3f} W/m²")

# Blue-irradiance correlation
r = df['irradiance_wm2'].corr(df['blue_channel'])
print(f"Blue-Irradiance r: {r:.3f}")

# Temp vs irradiance
r2 = df['irradiance_wm2'].corr(df['temp_c'])
print(f"Temp-Irradiance r: {r2:.3f}")

# Transient frequency (>15% drop in 10-min window)
df['irr_diff'] = df['irradiance_wm2'].diff()
print(f"Transients (>15% drop): {(df['irr_diff'] < -0.15*df['irradiance_wm2'].shift()).sum()}")
```

---

*Helios-Artemis · Hussain Touhid Siddiquee · Leading University Sylhet · 2026*
