# TunerStudio Configuration Guide - VW Gol Quadrado AP 1.8

## Overview

This guide covers the complete TunerStudio configuration for the VW Gol Quadrado with AP 1.8 engine, supporting both naturally aspirated and turbo versions. The SCG-ECU 2.0 provides full OBD-II compliance meeting PROCONVE L6 standards.

## Vehicle Specifications

### Engine Variants
| Parameter | AP 1.8 NA | AP 1.8 Turbo |
|-----------|-----------|--------------|
| Displacement | 1781cc | 1781cc |
| Cylinders | 4 inline | 4 inline |
| Compression | 9.0:1 | 7.5-8.5:1 |
| Fuel System | Sequential | Sequential |
| Ignition | Wasted spark | Wasted spark |
| Firing Order | 1-3-4-2 | 1-3-4-2 |

## TunerStudio Base Configuration

### Engine Constants (Page 1)

```
Cylinders: 4
Injector Staging: Simultaneous or Sequential
Engine Stroke: 4-Stroke
Trigger Pattern: Missing Tooth (60-2)
Ignition Mode: Wasted Spark
```

### Trigger Configuration

**Crankshaft (60-2 Pattern):**
```
Primary Trigger: Hall/VR Sensor
Trigger Wheel: 60-2
Trigger Edge: Rising
Primary Base Teeth: 1
```

**For distributor versions:**
```
Primary Trigger: Hall Distributor
Trigger Pattern: Dual Wheel
Secondary Trigger: Single Pulse (Cam)
```

### Fuel Settings

```
Injector Flow Rate: 180-220 cc/min (stock)
Fuel Pressure: 3.0 bar (300 kPa)
Stoichiometric AFR: 14.7
Required Fuel: Calculate per injector
```

**Turbo Version:**
```
Injector Flow Rate: 350-550 cc/min (upgraded)
Fuel Pressure: 3.0-4.0 bar
Boost Fuel Enrichment: Enable
```

## CAN Bus Configuration

### Settings Location
**TunerStudio Menu:** Settings → CAN Bus/OBD

### Enable CAN Communication
```
Enable CAN: Enabled
CAN Speed: 500 kbps
CAN Protocol: VAG (for Gol cluster compatibility)
OBD-II Support: Enabled
OBD Address: 0xE0 (default)
```

### Protocol Selection

For VW Gol Quadrado with OEM cluster:
```
Broadcast Protocol: VAG (option 2)
```

This enables:
- **0x280**: RPM broadcast (30Hz)
- **0x5A0**: Vehicle speed (30Hz)

## OBD-II Configuration

### Supported Modes

| Mode | Function | Status |
|------|----------|--------|
| 01 | Real-time data | Full support |
| 02 | Freeze frame | Full support |
| 03 | Read DTCs | Full support |
| 04 | Clear DTCs | Full support |
| 07 | Pending DTCs | Full support |
| 09 | VIN/ECU info | Full support |

### Available PIDs

**Essential PIDs for Gol AP 1.8:**
| PID | Description | Units |
|-----|-------------|-------|
| 0x05 | Coolant Temperature | °C |
| 0x06 | Short Term Fuel Trim B1 | % |
| 0x07 | Long Term Fuel Trim B1 | % |
| 0x0B | MAP Pressure | kPa |
| 0x0C | Engine RPM | rpm |
| 0x0D | Vehicle Speed | km/h |
| 0x0E | Timing Advance | ° |
| 0x0F | Intake Air Temp | °C |
| 0x11 | Throttle Position | % |
| 0x21 | Distance with MIL | km |
| 0x33 | Barometric Pressure | kPa |
| 0x46 | Ambient Temperature | °C |
| 0x5C | Oil Temperature | °C |

### VIN Configuration

Navigate to: **Settings → OBD-II → Vehicle Info**

```
VIN: [17 characters]
Example: 9BWZZZ377VT000001
         │└─ Brazil (9B = Volkswagen Brazil)
```

## DTC (Diagnostic Trouble Codes)

### Common DTCs for AP 1.8

**Sensor Faults:**
- P0105-P0108: MAP sensor
- P0110-P0113: IAT sensor
- P0115-P0118: CLT sensor
- P0120-P0123: TPS sensor
- P0335-P0336: Crankshaft sensor

**Fuel System:**
- P0171: System too lean
- P0172: System too rich
- P0300: Random misfire
- P0301-P0304: Cylinder misfire

**VW Manufacturer Codes:**
- P1127: LTFT too rich
- P1128: LTFT too lean
- P1176: O2 correction limit

### DTC Monitoring in TunerStudio

**Menu:** Diagnostics → CEL/MIL Status

Shows:
- Active DTCs (confirmed)
- Pending DTCs
- Freeze frame data
- MIL lamp status

## Sensor Calibration

### Coolant Temperature (CLT)

**Stock VW Sensor (Green):**
```
Bias Resistor: 2490 ohms
Calibration Table:
  -40°C: ~100,000 ohms
    0°C: ~7,000 ohms
   20°C: ~2,500 ohms
   80°C: ~300 ohms
  120°C: ~80 ohms
```

### Intake Air Temperature (IAT)

**Stock VW Sensor:**
```
Bias Resistor: 2490 ohms
Same curve as CLT
```

### Throttle Position Sensor (TPS)

```
Closed: 0.5V (calibrate to 0%)
WOT: 4.5V (calibrate to 100%)
```

## Fuel Table (VE)

### Base VE Table - AP 1.8 NA

| RPM \ MAP | 20 | 40 | 60 | 80 | 100 |
|-----------|----|----|----|----|-----|
| 1000 | 25 | 32 | 40 | 48 | 55 |
| 2000 | 30 | 38 | 48 | 58 | 68 |
| 3000 | 35 | 45 | 55 | 68 | 78 |
| 4000 | 38 | 48 | 60 | 72 | 82 |
| 5000 | 40 | 50 | 62 | 75 | 85 |
| 6000 | 38 | 48 | 58 | 72 | 82 |

### Turbo Considerations

For turbo versions, extend MAP axis:
- Include 120, 140, 160, 180 kPa columns
- Add boost enrichment table
- Enable boost cut protection

## Ignition Table

### Base Ignition - AP 1.8 NA

| RPM \ MAP | 20 | 40 | 60 | 80 | 100 |
|-----------|----|----|----|----|-----|
| 1000 | 12 | 15 | 15 | 12 | 10 |
| 2000 | 18 | 22 | 22 | 18 | 15 |
| 3000 | 24 | 28 | 28 | 24 | 20 |
| 4000 | 28 | 32 | 32 | 28 | 24 |
| 5000 | 30 | 34 | 32 | 28 | 24 |
| 6000 | 30 | 34 | 30 | 26 | 22 |

### Turbo Ignition Safety

```
Boost Retard: -2° per 10 kPa above 100
Knock Retard: -3° per event
Maximum Retard: -10°
Recovery Rate: 0.5° per second
```

## PROCONVE L6 Compliance

### Required Features
- [x] Catalyst monitoring (P0420)
- [x] O2 sensor monitoring (P0130-P0141)
- [x] Misfire detection (P0300-P0304)
- [x] EVAP system (P0440-P0446)
- [x] Fuel system monitoring (P0171-P0172)
- [x] OBD-II Mode 01-09 support

### Readiness Monitors

Configure in: **Settings → OBD-II → Readiness**

Enable monitors:
- Catalyst
- Heated Catalyst
- EVAP System
- Secondary Air
- A/C Refrigerant
- O2 Sensor
- O2 Sensor Heater
- EGR System

## Wiring Notes

### ECU Connector Pinout (STM32F407)

| Function | Pin | Notes |
|----------|-----|-------|
| CAN High | PD0 | 120Ω termination |
| CAN Low | PD1 | 120Ω termination |
| TPS | A0 | 0-5V analog |
| CLT | A1 | NTC sensor |
| IAT | A2 | NTC sensor |
| MAP | A3 | 0-5V sensor |
| O2 | A8 | Narrowband/Wideband |
| Crank | B6 | Hall/VR sensor |

## Troubleshooting

### No CAN Communication
1. Verify CAN enabled in settings
2. Check 120Ω termination resistors
3. Verify CAN H/L not swapped
4. Check CAN speed (500 kbps)

### No OBD-II Response
1. Verify OBD address (0xE0)
2. Check CAN bus for activity
3. Ensure ECU is powered with engine running

### Sensor Reading Errors
1. Verify bias resistor values
2. Check calibration tables
3. Inspect wiring for open/short

---

**Version:** 1.0
**Date:** 2026-01-12
**Compatible with:** SCG-ECU 2.0 / Speeduino firmware
