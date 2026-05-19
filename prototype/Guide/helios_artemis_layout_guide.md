# HELIOS-ARTEMIS MPPT — KiCad Layout & Routing Guide
**PCB Rev 1.0 · 140 × 110 mm · 2-Layer FR4 · Sylhet Deployment 2026**

---

## 0. Getting Started — Open the Template

1. Open `helios_artemis.kicad_pcb` in KiCad 7.x or 8.x (File → Open).
2. Open `helios_artemis.kicad_sch` in KiCad Schematic Editor.
3. In the PCB editor run **Tools → Update PCB from Schematic** to import all footprints.
4. Verify the netlist imports without errors before placing any component.

---

## 1. Layer Stack Reminder

| Layer | Role | Cu weight |
|-------|------|-----------|
| F.Cu (Top) | Signal routing, SMD pads, power traces | 35 µm (1 oz) |
| B.Cu (Bottom) | GND flood fill, return current paths, thermal relief | 35 µm (1 oz) |

**Substrate:** FR4, 1.6 mm finished thickness  
**Surface finish:** HASL lead-free  
**Solder mask:** Green LPI both sides  
**Silkscreen:** White epoxy top side only

---

## 2. Design Rule Checklist (set these in KiCad DRC before routing)

Open **File → Board Setup → Design Rules → Constraints**:

| Rule | Value |
|------|-------|
| Signal trace min width | 0.20 mm |
| Logic power trace (3.3V) min width | 1.00 mm |
| High-current trace min width | 1.50 mm |
| Signal–signal clearance | 0.20 mm |
| HV to logic clearance | 0.50 mm |
| Board edge clearance | 1.00 mm |
| Signal via drill / pad | 0.60 mm / 1.20 mm |
| Power via drill / pad | 1.00 mm / 1.80 mm |
| TH component min drill | 0.90 mm |
| TH component min annular ring | 0.40 mm |
| Via min annular ring | 0.25 mm |
| M3 mount hole | 3.20 mm NPTH |

Net classes are pre-configured in the `.kicad_pcb` file:
- **Default** → 0.25 mm traces (SDA, SCL, SPI, UART, PWM_50K, GATE, 3.3V)
- **Power_High** → 2.0 mm traces (PV_POS, PV_NEG, BAT_POS, SW_NODE, GND power)

---

## 3. Component Placement Order

Place in this sequence — power path first, then control, then passives.

### 3.1 Connectors (anchor points)
- **J1 (PV IN)** → top-left edge, ≥5 mm from board edge
- **J2 (BAT OUT)** → top-right edge (or same edge as J1), ≥5 mm from board edge
- Screw terminals must face outward for easy wiring access

### 3.2 Power Path (left/top zone)
Place in signal-flow order J1 → J2:

```
J1 ──► F1 ──► R1 ──► [C2] ──► Q1 ──► L1 ──► C1 ──► J2
                              │
                              D1 (freewheeling, Q1 source to GND)
```

- **F1 (5A fuse):** Immediately after J1 PV+ terminal
- **R1 (0.1Ω shunt):** After F1. Orient for Kelvin 4-wire: the two current-carrying pads handle power; the two sense pads connect ONLY to INA219 sense inputs — route sense traces separately on F.Cu, never in the power pour
- **C2 (1000µF bulk):** Within 10 mm of Q1 drain pad. Place before Q1, positive to PV rail
- **Q1 (IRFB4110 TO-220):** Heatsink tab faces outward or toward edge. Drain connects to SW_NODE, Source to GND (thermal vias — see §6)
- **D1 (SS34 SMA):** Cathode to SW_NODE, Anode to GND. Place within 5 mm of Q1 source pin — this is the freewheeling path during off-time
- **L1 (100µH TH):** One end to SW_NODE (Q1 drain side of loop), other end to output (C1 positive)
- **C1 (470µF output):** Positive to L1 output, negative to GND. Place close to J2

### 3.3 Gate Driver (centre-left zone)
- **U4 (TC4420 SO-8):** Place between STM32 PWM pin and Q1 gate
- **R4 (10Ω 0603):** Physically mount at TC4420 OUT pin — the pads of R4 should be touching the OUT trace with zero routing distance. No vias between R4 and Q1 gate
- **C6 (100nF bypass):** At TC4420 Vcc pin, within 2 mm

### 3.4 Current/Voltage Sense
- **U3 (INA219 breakout or SOIC-8):** Place near R1 shunt. Keep short sense traces
- **C5 (100nF bypass):** At U3 Vcc pin
- Connect INA219 SDA/SCL to shared I²C bus (SDA net, SCL net)
- INA219 I²C address: **0x40** (A0=GND, A1=GND by default)

### 3.5 LDO Regulator
- **U5 (AMS1117-3.3 SOT-223):** Place in lower-left area, clear of Q1/L1 heat
- **C3 (10µF tantalum):** Input cap — within 3 mm of IN pin
- **C4 (10µF tantalum):** Output cap — within 3 mm of OUT pin
- Thermal vias under SOT-223 tab (4×4 grid, 0.6 mm drill) connected to GND pour on B.Cu

### 3.6 MCUs (right zone)
- **U1 (ESP32-S3 DevKit-C):** Centre-right. USB connector faces board edge for programming access
- **U2 (STM32F103 LQFP-48):** Adjacent to U1. SWD header (SWDIO/SWDCLK/GND/3.3V) must be accessible — add 4-pin 2.54mm header near U2
- **C7, C8 (100nF bypass):** One at each MCU VCC pin, within 2 mm

### 3.7 I²C Pull-ups
- **R2, R3 (4.7kΩ 0603):** Place near ESP32 GPIO21 (SDA) and GPIO22 (SCL). Pull to 3.3V rail
- Note: The I²C bus is shared — ESP32 GPIO21/22 AND STM32 PB6/PB7 both connect to SDA/SCL. Only one set of pull-ups needed (R2/R3 is it)

### 3.8 Ambient Light Sensor
- **U6 (GY302):** Mount near board edge with unobstructed upward view of sky. Shield from direct IR/heat radiation from Q1 and L1. SDA/SCL connect to shared I²C bus. Address: **0x23**

### 3.9 SD Card Module
- **J3:** Place near ESP32 SPI pins (GPIO23=MOSI, GPIO19=MISO, GPIO18=SCK, GPIO5=CS)

---

## 4. Routing — Critical Nets First

### 4.1 Power Path (route FIRST, before any signal traces)
Trace width: **2.0 mm minimum**, go wider (2.5–3.0 mm) if space allows.

```
Net: PV_POS
J1.pin1 → F1.A → R1.pad1(current) → C2+ → Q1.Drain
Min width: 2.0mm · Via: 1.0mm drill / 1.8mm pad if layer change needed

Net: SW_NODE
Q1.Drain → D1.Cathode (short, direct)
Q1.Drain → L1.pin1
Min width: 2.0mm · Keep this loop area MINIMAL

Net: BAT_POS / output
L1.pin2 → C1+ → J2.pin1
Min width: 2.0mm

Net: GND (power returns)
Q1.Source → D1.Anode → C2– → C1– → J2.pin2
Use GND pour on B.Cu. Add power vias (1.0mm drill) at Q1 source, D1 anode
```

**Loop area rule:** The loop Q1.Drain → L1 → C1 → GND return must be as tight as possible. Large loops = radiated EMI at 50kHz. Keep the switching node (SW_NODE) physically compact.

### 4.2 Gate Drive Path
```
Net: PWM_50K
STM32 PA8 → TC4420 IN
Width: 0.25mm · Keep AWAY from SDA/SCL/SPI traces
Route on F.Cu, direct path, no unnecessary vias

Net: GATE
TC4420 OUT → R4.pad1 → R4.pad2 → Q1.Gate
Width: 0.25mm · ZERO vias in this path — all on same layer
R4 must be the first component the OUT trace hits
```

### 4.3 Kelvin Shunt (R1 sense lines)
```
Net: SHUNT_P (sense +)
R1.sense_pad_1 → U3.IN+ (INA219 pin 1)
Width: 0.25mm · Route on F.Cu away from power pours
NEVER share a trace segment with PV_POS net

Net: SHUNT_N (sense –)
R1.sense_pad_2 → U3.IN– (INA219 pin 2)
Width: 0.25mm · Same rules as SHUNT_P
```

### 4.4 I²C Bus
```
Nets: SDA, SCL
ESP32 GPIO21 → SDA bus → INA219.SDA + GY302.SDA
ESP32 GPIO22 → SCL bus → INA219.SCL + GY302.SCL
STM32 PB6 → SCL bus (reads INA219)
STM32 PB7 → SDA bus (reads INA219)

Width: 0.25mm
Route on F.Cu
Keep AWAY from SW_NODE copper and PWM_50K trace
Optional: add 0.25mm ground guard trace on each side of SDA/SCL if routing must pass near switching node
Pull-ups R2(SDA) R3(SCL) tie to +3.3V near ESP32
```

### 4.5 UART
```
Net: UART_TX
ESP32 GPIO17 (U1TX) → STM32 PA10 (USART1 RX)
115200 baud · 0.25mm trace

Net: UART_RX (optional, for STM32 → ESP32 future use)
STM32 PA9 → ESP32 GPIO18 (check pin assignment)
```

### 4.6 SPI (SD Card)
```
ESP32 GPIO23 → SPI_MOSI → J3
ESP32 GPIO19 ← SPI_MISO ← J3
ESP32 GPIO18 → SPI_SCK  → J3
ESP32 GPIO5  → SPI_CS   → J3
All 0.25mm · Route together as a bundle
```

### 4.7 Power Distribution (3.3V rail)
```
AMS1117 OUT → 3.3V trace → ESP32 3V3 + STM32 VCC
Width: 1.0mm minimum for the main trunk
Branch to each IC with 0.5mm trace
C5, C6, C7, C8 (100nF bypass) each connected directly at IC VCC pin
```

---

## 5. GND Plane (Bottom Copper)

1. In KiCad PCB editor: **Edit → Zones → Fill All Zones** (shortcut B)
2. GND zone covers entire B.Cu (pre-configured in template, polygon 0.5→139.5 × 0.5→109.5)
3. Connect all GND pads to B.Cu pour via vias where needed (0.6mm drill / 1.2mm pad signal, 1.0mm / 1.8mm power)
4. **Thermal relief:** KiCad auto-applies. For power components (Q1, D1, J1, J2) you may want solid connections — right-click zone → Zone Properties → set thermal relief only for SMD, solid for TH

### GND Via placement (minimum):
| Location | Via type |
|----------|----------|
| Q1 Source pad | 4× power via (1.0mm drill) |
| D1 Anode pad | 2× power via |
| C2 negative | 2× power via |
| C1 negative | 2× power via |
| Each decoupling cap GND pad | 1× signal via (0.6mm drill) |
| AMS1117 tab | thermal via array (see §6) |

---

## 6. Thermal Via Arrays

### Q1 (IRFB4110 TO-220) — Source pad
Place a 4×4 grid of thermal vias under/around Q1 source pad:
- Via drill: 0.6 mm, pad: 1.2 mm
- Grid spacing: 1.5 mm
- All connect to GND on B.Cu
- KiCad: use **Place → Via** and manually array, or use **Edit → Create Array**

### AMS1117-3.3 (SOT-223) — Exposed tab
Place a 3×3 grid of thermal vias under the SOT-223 tab pad:
- Via drill: 0.6 mm, pad: 1.2 mm
- Grid spacing: 1.2 mm
- Connect to GND pour on B.Cu

---

## 7. 3.3V Pour (Top Copper — MCU Zone)

Pre-configured in template: covers x=50→135, y=55→105 on F.Cu.
This gives the MCU zone a local 3.3V reference plane reducing impedance.
- **Do not extend into power section** (left portion of board — HV clearance risk)
- Fill after all signal routing is complete (shortcut **B**)

---

## 8. Decoupling Cap Placement Rules

Every IC must have a 100nF MLCC (0603) at its VCC pin:

| Ref | IC | Location note |
|-----|----|---------------|
| C5 | U3 INA219 | Within 2mm of VCC pin |
| C6 | U4 TC4420 | Within 2mm of VCC pin — 12V side |
| C7 | U1 ESP32-S3 | Within 2mm of 3V3 pin |
| C8 | U2 STM32F103 | Within 2mm of VCC pin |

**Routing rule:** The cap GND pad goes directly to a via to B.Cu GND — no long return path.

---

## 9. SWD Programming Header (add manually)

Add a 4-pin 2.54mm TH header near U2 (STM32):

| Pin | Signal |
|-----|--------|
| 1 | +3.3V |
| 2 | SWDIO (PA13) |
| 3 | SWDCLK (PA14) |
| 4 | GND |

Label on silkscreen. Used with ST-Link V2 for firmware flashing.

---

## 10. Silkscreen Rules

- All component references visible on F.SilkS, not overlapping pads or vias
- J1: label "PV IN +" and "PV IN –"
- J2: label "BAT OUT +" and "BAT OUT –"
- J3: label "SD MOSI MISO SCK CS GND 3V3"
- Q1: label "G D S" near pins
- Board title silk: pre-configured at Y=108 (bottom edge)
- Mark SW_NODE area with "⚠ HV SWITCHING NODE" on Cmts.User layer

---

## 11. DRC Run — Before Generating Gerbers

Run **Inspect → Design Rules Checker**. All of these must pass:

- ✅ No unconnected nets
- ✅ No clearance violations (0.2mm signal, 0.5mm HV-to-logic)
- ✅ No trace width violations (power nets ≥1.5mm)
- ✅ No via drill violations
- ✅ No courtyard overlaps
- ✅ No silkscreen on copper

Fix all errors. Warnings about NPTH mounting holes can be ignored.

---

## 12. Gerber Export (for JLCPCB/PCBWay)

**File → Fabrication Outputs → Gerbers:**

| Layer | Gerber file |
|-------|-------------|
| F.Cu | helios_artemis-F_Cu.gbr |
| B.Cu | helios_artemis-B_Cu.gbr |
| F.Mask | helios_artemis-F_Mask.gbr |
| B.Mask | helios_artemis-B_Mask.gbr |
| F.SilkS | helios_artemis-F_SilkS.gbr |
| Edge.Cuts | helios_artemis-Edge_Cuts.gbr |

**File → Fabrication Outputs → Drill Files:**
- Format: Excellon, Separate files for plated/NPTH
- Units: Millimetres

**Zip all files** → upload to JLCPCB or PCBWay.

### JLCPCB Order Settings:
| Field | Value |
|-------|-------|
| Board size | 140 × 110 mm |
| Layers | 2 |
| Thickness | 1.6 mm |
| FR4 TG | TG130 (standard) |
| Surface finish | HASL (lead-free) |
| Copper weight | 1 oz (35µm) both layers |
| Solder mask | Green |
| Silkscreen | White |
| Min hole size | 0.3 mm |
| IPC Class | Class 2 |
| Quantity | 5 (minimum sensible for prototyping) |

---

## 13. Assembly Sequence (after boards arrive)

Follow the test sequence from PCB Fabrication Notes:

1. Solder SMD passives first (C5–C8, R2–R4) — use reflow or hot air
2. Solder ICs: U3 (INA219), U4 (TC4420), U5 (AMS1117), U6 (GY302), U2 (STM32 LQFP-48 — fine pitch, flux + drag solder)
3. Solder TH components: R1 (shunt), L1, C1, C2, C3, C4
4. Solder connectors: J1, J2, J3, SWD header
5. Last: Q1 (TO-220) and D1 (SMA) — power components
6. Seat U1 (ESP32-S3 DevKit-C) — use female header strip so it's removable
7. Visual inspection under magnification before power-on
8. Follow 7-step power-up sequence from Fabrication Notes

---

## 14. Known Layout Pitfalls

| Risk | Mitigation |
|------|------------|
| Large switching loop (Q1–L1–C1) | Place C2 within 10mm of Q1 drain. Keep D1 tight to Q1 source |
| Gate ringing at 50kHz | R4 must be at TC4420 OUT — not mid-trace. No vias in gate path |
| INA219 sense error | Kelvin sense traces carry zero current. Route completely separate from PV_POS pour |
| I²C noise pickup from PWM | Route SDA/SCL on opposite side of board from PWM_50K trace. Ground guard if needed |
| GY302 thermal interference | Keep ≥20mm from Q1/L1. Mount at board edge with thermal isolation |
| AMS1117 instability | C3/C4 tantalum caps are mandatory. Ceramic-only will oscillate |
| STM32 LQFP-48 tombstoning | Pre-tin pads, use plenty of flux, drag solder method or hot air gun |

---

*HELIOS-ARTEMIS MPPT · Layout Guide Rev 1.0 · Sylhet Deployment 2026*
