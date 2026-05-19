# Helios-Artemis Dual-MCU Predictive MPPT — Deployment Guide

**PCB Rev 1.0 · Sylhet Deployment 2026**
**Firmware:** `artemis_stm32f103.c` · `helios_esp32s3.cpp`

---

## Table of Contents

1. [Bill of Materials Summary](#1-bill-of-materials-summary)
2. [Hardware Assembly Checklist](#2-hardware-assembly-checklist)
3. [Wiring the Two MCUs Together](#3-wiring-the-two-mcus-together)
4. [Artemis — STM32F103 Firmware Setup](#4-artemis--stm32f103-firmware-setup)
5. [Helios — ESP32-S3 Firmware Setup](#5-helios--esp32-s3-firmware-setup)
6. [SPIFFS Filesystem Preparation](#6-spiffs-filesystem-preparation)
7. [Seven-Step System Commissioning Test](#7-seven-step-system-commissioning-test)
8. [Live Dashboard Usage](#8-live-dashboard-usage)
9. [TF.js On-Device Retraining](#9-tfjs-on-device-retraining)
10. [UART Frame Reference](#10-uart-frame-reference)
11. [Troubleshooting](#11-troubleshooting)
12. [File Index](#12-file-index)

---

## 1. Bill of Materials Summary

| Ref | Part | Notes | BDT |
|-----|------|-------|-----|
| U1 | ESP32-S3 DevKit-C | Helios MCU · USB-C onboard | 380 |
| U2 | STM32F103C8T6 (Blue Pill) | Artemis MCU · LQFP48 | 120 |
| U3 | INA219 breakout | I²C 0x40 · 0.1 Ω shunt | 80 |
| U4 | TC4420 CPA | Gate driver · SO-8 | — |
| U5 | AMS1117-3.3 | 3.3 V LDO · SOT-223 | 30 |
| U6 | TSL2591 | Irradiance sensor · I²C 0x29 | 120 |
| Q1 | IRFB4110 TO-220 | N-ch MOSFET · 100 V · 3.7 mΩ | — |
| D1 | SS34 | Schottky freewheeling · 3 A | — |
| L1 | 100 µH power inductor | I_sat ≥ 8 A · DCR < 50 mΩ | — |
| C1 | 470 µF / 35 V | Buck output · low-ESR | — |
| C2 | 1000 µF / 25 V | Input bulk · near Q1 drain | — |
| R1 | 0.1 Ω / 2 W | INA219 shunt · Kelvin connection | — |
| R4 | 10 Ω / 0603 | Gate series · at TC4420 OUT | — |
| J3 | SD card module (SPI) | Data logging | — |
| — | Misc + enclosure | Wire, connectors, housing | 630 |
| | **Total** | | **≈ 1,360 BDT** |

---

## 2. Hardware Assembly Checklist

Work through these in order before applying any power.

### Power path (do first)
- [ ] J1 → F1 → R1 → Q1 → L1 → J2 traces are ≥ 1.5 mm wide
- [ ] C2 (1000 µF) placed within 10 mm of Q1 drain pin
- [ ] R1 shunt connected with 4-wire Kelvin layout — sense lines carry no switching current
- [ ] R4 (10 Ω gate resistor) placed physically at TC4420 OUT pin, no vias in gate trace

### I²C bus
- [ ] R2/R3 pull-ups (4.7 kΩ) near ESP32-S3 GPIO 21/22
- [ ] INA219 address solder jumpers set to 0x40
- [ ] TSL2591 mounted near board edge, unobstructed sky view, shielded from Q1/L1 heat

### Power supply rails
- [ ] AMS1117-3.3 input capacitor C3 (10 µF tantalum) placed within 2 mm of IN pin
- [ ] AMS1117-3.3 output capacitor C4 (10 µF tantalum) placed within 2 mm of OUT pin
- [ ] 100 nF bypass caps (C5–C8) at every IC VCC pin, return via to GND plane < 2 mm

### Thermal
- [ ] Thermal via array (4×4, 0.6 mm drill) under Q1 source pad → GND plane
- [ ] Thermal via array under AMS1117 tab → GND plane

---

## 3. Wiring the Two MCUs Together

Connect these three wires before applying any power to the board.

```
ESP32-S3 GPIO 17 (TX)  ──────────→  STM32 PA10 (RX)
ESP32-S3 GPIO 18 (RX)  ←──────────  STM32 PA9  (TX)
ESP32-S3 GND           ───────────  STM32 GND
```

Both MCUs operate at 3.3 V logic — no level shifter is required. Baud rate on both sides is **115200 8N1**.

---

## 4. Artemis — STM32F103 Firmware Setup

### 4.1 Tools required

| Tool | Download |
|------|----------|
| STM32CubeIDE | https://www.st.com/en/development-tools/stm32cubeide.html |
| ST-Link V2 | Hardware programmer (~200 BDT clone) |

### 4.2 CubeMX peripheral configuration

Open STM32CubeIDE → New STM32 Project → select **STM32F103C8T6**.

**TIM1 — 50 kHz PWM on PA8:**

| Parameter | Value |
|-----------|-------|
| Channel 1 mode | PWM Generation CH1 |
| Prescaler | 0 |
| Counter Period (ARR) | 1439 |
| CH1 Output State | Enable |

**I2C1 — INA219:**

| Parameter | Value |
|-----------|-------|
| Speed Mode | Fast Mode |
| Clock Speed | 400 kHz |
| SDA | PB7 |
| SCL | PB6 |

**USART1 — Helios link:**

| Parameter | Value |
|-----------|-------|
| Baud Rate | 115200 |
| Word Length | 8 bits |
| Parity | None |
| Stop Bits | 1 |
| TX | PA9 |
| RX | PA10 |

**Clock tree:** Set HCLK to **72 MHz** via PLL from 8 MHz HSE crystal.

### 4.3 Adding the firmware file

Copy `artemis_stm32f103.c` into `Core/Src/` in your CubeMX project.

In the CubeMX-generated `main.c`, make the following additions:

```c
/* USER CODE BEGIN Includes */
extern void Artemis_Init(void);
extern void Artemis_Tick(void);
/* USER CODE END Includes */
```

```c
/* USER CODE BEGIN 2 */
Artemis_Init();
/* USER CODE END 2 */
```

```c
while (1)
{
  /* USER CODE BEGIN 3 */
  Artemis_Tick();
  /* USER CODE END 3 */
}
```

### 4.4 Flashing via ST-Link V2

```
ST-Link V2    Blue Pill
──────────    ─────────
SWDIO    →    PA13
SWCLK    →    PA14
GND      →    GND
3.3V     →    3.3V  (power from ST-Link during flash only)
```

In STM32CubeIDE: **Run → Debug → Resume**, or use STM32CubeProgrammer to write the `.bin` directly.

### 4.5 Verify Artemis is running

Open a serial terminal at **115200** on PA9/PA10. With 12 V applied and PV connected, you should see telemetry frames at 100 ms intervals:

```
ART:V=12.54,I=2.341,D=0.712,S=0,G=623.4
```

| Field | Meaning |
|-------|---------|
| V | Battery bus voltage (V) |
| I | Battery current (A) |
| D | PWM duty cycle [0–1] |
| S | Charge state: 0=BULK, 1=ABSORPTION, 2=FLOAT |
| G | Irradiance estimate from I_pv (W/m²) |

Also confirm **50 kHz square wave** on PA8 with an oscilloscope.

---

## 5. Helios — ESP32-S3 Firmware Setup

### 5.1 Tools required

| Tool | Download |
|------|----------|
| VS Code | https://code.visualstudio.com |
| PlatformIO extension | https://platformio.org/install/ide?install=vscode |
| USB-C cable | For flashing (DevKit-C has onboard USB-UART) |

### 5.2 Create the PlatformIO project

In VS Code: **PlatformIO → New Project**

- Name: `helios`
- Board: `Espressif ESP32-S3-DevKitC-1`
- Framework: `Arduino`

Replace the generated `platformio.ini` with:

```ini
[env:esp32s3]
platform   = espressif32
board      = esp32s3devkitc1
framework  = arduino
lib_deps   =
    bblanchon/ArduinoJson @ ^7.0.0
    esp32-camera
board_build.partitions = default_ffat.csv
board_build.flash_mode = qio
board_build.psram_type = opi
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=3
monitor_speed = 115200
```

### 5.3 Adding the firmware file

Copy `helios_esp32s3.cpp` into the `src/` folder of your project. Delete the default `main.cpp` — the firmware already contains `setup()` and `loop()`.

### 5.4 Flash

```
PlatformIO → Project Tasks → esp32s3 → General → Upload
```

No external programmer needed — the DevKit-C flashes over USB-C.

### 5.5 Verify Helios is running

Open **PlatformIO Serial Monitor** at 115200. Expected boot output:

```
[HELIOS] Booting Helios-Artemis
[HELIOS] TSL2591 OK
[HELIOS] OV2640 OK
[HELIOS] LSTM weights loaded from SPIFFS
[HELIOS] SD card OK
[HELIOS] AP: Helios-MPPT, IP: 192.168.4.1
[HELIOS] Init complete — entering main loop
```

If TSL2591 or OV2640 shows failure, check I²C wiring and camera ribbon before proceeding.

---

## 6. SPIFFS Filesystem Preparation

Helios loads LSTM weights from SPIFFS at boot. You must upload the filesystem image before first run.

### 6.1 Create the data folder

In your PlatformIO project root, create a folder named `data/`.

### 6.2 Add the weights file

**First deployment (no pre-trained weights):** Create `data/lstm_weights.json` as an empty object:

```json
{}
```

The device will boot in pass-through mode (LSTM output = measured G) until the first TF.js training session completes after 24 hours of data collection.

**Pre-trained weights:** If you have trained a model offline in Python/Keras, export it to the firmware JSON schema (keys: `lstm`, `gain`, each containing `Wf`, `Wi`, `Wc`, `Wo`, `bf`, `bi`, `bc`, `bo`, `Wd`, `bd`) and save it as `data/lstm_weights.json`.

### 6.3 Upload the filesystem image

```
PlatformIO → Project Tasks → esp32s3 → Platform → Upload Filesystem Image
```

This must be done **once before the first firmware flash**, and again any time you manually update the weights file.

### 6.4 Offline TF.js (optional)

The dashboard loads TF.js from `cdn.jsdelivr.net`. If the connecting device has no internet access (AP-only), serve TF.js locally:

1. Download `tf.min.js` from `https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@4.17.0/dist/tf.min.js`
2. Place it in `data/tfjs.min.js`
3. Upload filesystem image again
4. Add this route in `web_server_init()` in `helios_esp32s3.cpp`:
   ```cpp
   g_server.serveStatic("/tfjs.min.js", SPIFFS, "/tfjs.min.js");
   ```
5. Change the `<script src>` in `DASHBOARD_HTML` from the CDN URL to `/tfjs.min.js`

---

## 7. Seven-Step System Commissioning Test

Follow this sequence exactly as specified in the PCB fabrication notes. Do **not** skip steps.

**Step 1 — 3.3 V rail only**
Disconnect 12 V. Measure: +3.30 V ±0.05 V at ESP32-S3 VCC and STM32 VCC pins.

**Step 2 — I²C scan**
From Helios serial monitor, trigger an I²C scan. Expected devices:

```
0x29  →  TSL2591 (irradiance sensor)
0x40  →  INA219  (current/voltage sense)
```

Both must respond. If 0x40 is missing, check R2/R3 pull-ups and INA219 address solder jumpers.

**Step 3 — Artemis PWM**
Flash Artemis firmware via ST-Link V2. Attach oscilloscope probe to PA8. Confirm **50 kHz square wave**. Duty cycle should be at minimum (≈5%) with no PV connected.

**Step 4 — Helios boot and dashboard**
Flash Helios firmware via USB-C. Confirm boot log in serial monitor. Connect a phone or laptop to WiFi **Helios-MPPT** (password: `sylhet2026`). Open `http://192.168.4.1`. Dashboard should load and show live telemetry.

**Step 5 — 12 V supply, no PV/battery**
Connect 12 V supply with a 1 A current limit. Verify:
- TC4420 is receiving gate pulses (probe TC4420 OUT pin)
- AMS1117 stays cool to touch
- No unexpected current draw

**Step 6 — PV panel connection**
Connect PV panel with a 1 A bench-supply current limit. Verify:
- INA219 reports non-zero voltage and current in dashboard
- Artemis telemetry shows increasing duty cycle
- Buck output voltage appears at J2

**Step 7 — Full system test**
Connect 12 V / 7 Ah SLA battery. Monitor via serial log and dashboard. You should observe the charge state sequence over time:

```
S=0  →  BULK        (constant current, 6 A limit)
S=1  →  ABSORPTION  (constant voltage, 14.7 V)
S=2  →  FLOAT       (trickle, 13.8 V)
```

The transition from BULK to ABSORPTION occurs when V_bat reaches **14.7 V**. Absorption ends when I_bat drops below **0.5 A**. If the battery is already charged, you may see it jump directly to S=1 or S=2.

---

## 8. Live Dashboard Usage

With any device connected to the **Helios-MPPT** WiFi AP, open `http://192.168.4.1`.

### Telemetry cards

| Card | Source | Description |
|------|--------|-------------|
| G_meas | TSL2591 + OV2640 blend | Measured irradiance (W/m²) |
| G_pred | LSTM inference | Predicted irradiance 30 min ahead (W/m²) |
| V_bat | INA219 via Artemis | Battery bus voltage (V) |
| I_bat | INA219 via Artemis | Battery charging current (A) |
| V_MPP_pred | Analytical model | Predicted MPP voltage (V) |
| α blend | Gain scheduler | LSTM blend weight applied to P&O |
| η_MPPT | Calculated | Estimated MPPT efficiency (%) |
| Charge State | Artemis FSM | BULK / ABSORPTION / FLOAT |
| Train Samples | SPIFFS count | Minute-resolution samples collected |
| Data Ready | Threshold flag | YES when ≥ 1440 samples exist |

### API endpoints

| Endpoint | Method | Returns |
|----------|--------|---------|
| `/api/status` | GET | JSON snapshot of all telemetry fields |
| `/api/train_data` | GET | Raw `train_buf.csv` for TF.js training |
| `/api/weights` | GET | Current `lstm_weights.json` from SPIFFS |
| `/api/weights` | POST | Upload new weights JSON; device reloads immediately |

---

## 9. TF.js On-Device Retraining

After **24 hours of operation**, 1440 training samples accumulate in SPIFFS. The system auto-triggers retraining at the 24-hour mark, or you can start it manually.

### Manual trigger

1. Connect to **Helios-MPPT** WiFi
2. Open `http://192.168.4.1`
3. Confirm **Data Ready = YES** in the telemetry cards
4. Click **▶ Train Now**

### What happens

The dashboard fetches `train_buf.csv` from `/api/train_data`, downsamples it to hourly averages, builds 24-step LSTM sequences, and trains two TF.js models entirely in the browser:

- **Irradiance forecaster:** LSTM(32) → Dense(1), 40 epochs, Adam lr=0.002
- **Gain scheduler:** LSTM(4) → Dense(1), 20 epochs

Training takes approximately **2–5 minutes** on a mid-range laptop. The progress bar and per-epoch loss are shown in real time.

### Deploying the new weights

When training completes, click **↑ Deploy Weights**. The browser POSTs the serialised weight JSON to `/api/weights`. The device:
1. Writes the new `lstm_weights.json` to SPIFFS
2. Reloads weights into the C++ LSTM engine immediately (no reboot)
3. Clears the training buffer for the next 24-hour cycle

### Warm-start behaviour

Each retraining session automatically downloads the current weights from `/api/weights` and warm-starts from them before fitting. This means the model improves incrementally with each daily cycle rather than retraining from scratch.

### Automatic 24-hour cycle

The `RETRAIN_INTERVAL_MS` (86,400,000 ms) timer in the firmware sets `auto_retrain: true` in `/api/status` once 24 hours have elapsed and data is ready. The dashboard polls this field every second and calls `startTraining()` automatically — no manual interaction needed after the first deployment.

---

## 10. UART Frame Reference

### Artemis → Helios (100 ms interval)

```
ART:V=12.54,I=2.341,D=0.712,S=0,G=623.4\r\n
```

| Field | Type | Description |
|-------|------|-------------|
| V | float, 2 dp | Battery bus voltage (V) |
| I | float, 3 dp | Battery current (A), positive = charging |
| D | float, 3 dp | PWM duty cycle [0.050 – 0.950] |
| S | int | Charge state: 0=BULK, 1=ABSORPTION, 2=FLOAT |
| G | float, 1 dp | Irradiance estimate from I_pv (W/m²) |

### Helios → Artemis (100 ms interval)

```
HEL:VP=17.23,GP=745.0,AL=0.32\r\n
```

| Field | Type | Description |
|-------|------|-------------|
| VP | float, 2 dp | Predicted V_MPP (V) |
| GP | float, 1 dp | Predicted irradiance (W/m²) |
| AL | float, 2 dp | Blend weight α [0.05 – 0.55] |

---

## 11. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| TSL2591 not found on I²C scan | Pull-up resistors missing or wrong address | Check R2/R3 (4.7 kΩ to 3.3 V) on SDA/SCL; TSL2591 addr is fixed at 0x29 |
| INA219 not found (0x40) | Address solder jumper or Kelvin wiring issue | Verify INA219 A0/A1 jumpers are open (default 0x40); check 4-wire Kelvin layout for R1 |
| PA8 shows no PWM | TIM1 not started or ARR wrong | Confirm ARR=1439, Prescaler=0, HCLK=72 MHz; check `HAL_TIM_PWM_Start` is called in `Artemis_Init` |
| Dashboard doesn't load | WiFi AP not started | Check serial log for `AP: Helios-MPPT` line; confirm `web_server_init()` runs without error |
| ART frames not appearing on Helios serial | UART wiring reversed | Swap GPIO17/18 connections; verify common GND |
| SPIFFS mount failed | Filesystem not uploaded | Run PlatformIO → Upload Filesystem Image before flashing firmware |
| Charge state stuck at S=0 | V_bat never reaches 14.7 V | Check that buck is regulating; verify L1 is not saturating (I_sat ≥ 8 A required) |
| TF.js training fails — "not enough data" | < 1440 samples in buffer | Wait for more data; check that per-minute append timer is firing (60,000 ms) |
| TF.js CDN fails to load | No internet on connected device | Serve TF.js locally from SPIFFS (see Section 6.4) |
| AMS1117 hot to touch | Excessive current draw on 3.3 V rail | Check for short on 3.3 V bus; AMS1117 rated 1 A — verify all MCU bypass caps are populated |

---

## 12. File Index

| File | MCU | Purpose |
|------|-----|---------|
| `artemis_stm32f103.c` | STM32F103C8T6 | VS-P&O MPPT, CC/CV/Float FSM, INA219 driver, 50 kHz PWM, UART link |
| `helios_esp32s3.cpp` | ESP32-S3 | LSTM inference, TSL2591 driver, TF.js dashboard, SD logging, UART link |
| `data/lstm_weights.json` | ESP32-S3 SPIFFS | LSTM weight store — empty `{}` for first deploy, replaced by TF.js after training |

---

*Helios-Artemis MPPT · PCB Rev 1.0 · Sylhet Deployment 2026*
