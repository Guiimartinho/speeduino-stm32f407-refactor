# TunerStudio Configuration Guide - BMW E46 330i (M54B30)

## Overview

This guide covers complete ECU replacement configuration for the BMW E46 330i (2001) with M54B30 engine. The SCG-ECU 2.0 provides full PT-CAN integration allowing complete replacement of the original Siemens MS43/MS45 DME.

## Vehicle Specifications

### Engine: M54B30
| Parameter | Value |
|-----------|-------|
| Displacement | 2979cc |
| Cylinders | 6 inline |
| Compression | 10.2:1 |
| Power | 231 HP @ 5900 rpm |
| Torque | 300 Nm @ 3500 rpm |
| Fuel System | Sequential |
| Ignition | Individual coil-on-plug |
| Firing Order | 1-5-3-6-2-4 |
| VANOS | Dual (intake + exhaust) |

## TunerStudio Base Configuration

### Engine Constants (Page 1)

```
Cylinders: 6
Injector Staging: Sequential
Engine Stroke: 4-Stroke
Trigger Pattern: BMW (60-2 + Cam)
Ignition Mode: Coil-on-Plug (IGN_MODE_SEQUENTIAL)
Number of Ignition Outputs: 6
```

### Trigger Configuration

**Crankshaft (60-2):**
```
Primary Trigger: Hall Sensor
Trigger Wheel: 60-2
Trigger Edge: Falling
Primary Base Teeth: 1
Missing Teeth: 2
Trigger Angle: 0 (adjust with timing light)
```

**Camshaft (Single Pulse):**
```
Secondary Trigger: Hall Sensor
Cam Trigger Pattern: Single Tooth
Cam Edge: Rising
```

### Fuel Settings

```
Injector Flow Rate: 215 cc/min @ 3.5 bar (stock M54)
Fuel Pressure: 3.5 bar (350 kPa)
Stoichiometric AFR: 14.7
Required Fuel: Calculate for 6 cylinders
Injector Dead Time: 1.0ms @ 14V
```

**For upgraded injectors (turbo conversion):**
```
Injector Flow Rate: 550-750 cc/min
Fuel Pressure: 4.0 bar
```

## CAN Bus Configuration - CRITICAL

### BMW PT-CAN Settings

**TunerStudio Menu:** Settings → CAN Bus/OBD

```
Enable CAN: Enabled
CAN Speed: 500 kbps (PT-CAN standard)
CAN Protocol: BMW (option 1)
OBD-II Support: Enabled
OBD Address: 0xE0
```

### PT-CAN Message Configuration

When BMW protocol is selected, the ECU transmits:

| Message ID | Name | Frequency | Content |
|------------|------|-----------|---------|
| 0x316 | DME1 | 30 Hz | RPM, engine status |
| 0x329 | DME2 | 30 Hz | CLT, TPS, throttle |
| 0x545 | DME4 | 10 Hz | Fuel consumption, CEL, oil temp |

### PT-CAN RX Messages (Received)

The ECU automatically receives:

| Message ID | Source | Content |
|------------|--------|---------|
| 0x153 | ASC1 | Wheel speeds from DSC |
| 0x1F3 | EGS1 | Gear, transmission status |
| 0x1F5 | EGS2 | Torque request |
| 0x615 | ICL3 | Odometer reading |
| 0x0C8 | SAS | Steering angle |

## Instrument Cluster Integration

### What Works with SCG-ECU 2.0

| Function | Status | Notes |
|----------|--------|-------|
| Tachometer | ✅ Works | Via DME1 (0x316) |
| Coolant Temp Gauge | ✅ Works | Via DME2 (0x329) |
| CEL/MIL Lamp | ✅ Works | Via DME4 (0x545) |
| EML Lamp | ✅ Works | Via DME4 (0x545) |
| Overheat Warning | ✅ Works | Via DME4 (0x545) |
| Fuel Consumption | ✅ Works | Via DME4 (0x545) |
| Oil Temperature | ✅ Works | Via DME4 (0x545) |
| Speedometer | ✅ Works | Via ASC/wheel speed |

### Cluster Communication Notes

The E46 instrument cluster expects messages from the DME on PT-CAN. The SCG-ECU 2.0 fully emulates the Siemens DME communication protocol.

**Important:** The cluster calculates fuel consumption display from DME4 data. Ensure `injectorFlowRate` and `fuelDensity` are configured correctly in Page 15.

## OBD-II Configuration

### BMW-Specific PIDs

| PID | Description | BMW Use |
|-----|-------------|---------|
| 0x05 | Coolant Temperature | Engine temp gauge |
| 0x0C | Engine RPM | Tachometer |
| 0x0D | Vehicle Speed | Speedometer (via ASC) |
| 0x0E | Timing Advance | Diagnostic |
| 0x11 | Throttle Position | Diagnostic |
| 0x21 | Distance with MIL | Service indicator |
| 0x42 | Control Module Voltage | Battery monitoring |
| 0x5C | Engine Oil Temperature | Cluster display |

### VIN Configuration

Navigate to: **Settings → OBD-II → Vehicle Info**

```
VIN: [17 characters]
Example: WBAAV33401FU00001
         │ │  │││└─ Sequential number
         │ │  ││└── Model year (1=2001)
         │ │  │└─── Plant (F=Munich)
         │ │  └──── Check digit
         │ └─────── Series (AV3=E46 330i)
         └───────── BMW Germany
```

## Fuel Consumption Calculation

### Configuration for Accurate Display

Navigate to: **Settings → CAN Bus → BMW Config**

```
Injector Flow Rate: 215 cc/min (stock M54)
Fuel Density: 750 (for gasoline, × 1000)
```

**Formula used:**
```
Consumption (L/h) = (PW_µs × RPM × nCyl × FlowRate) / (2 × 60,000,000 × Density)
```

### Fuel Density Values
| Fuel Type | Density (kg/m³) | Config Value |
|-----------|-----------------|--------------|
| Gasoline | 0.750 | 750 |
| E10 | 0.755 | 755 |
| E85 | 0.785 | 785 |
| Diesel | 0.850 | 850 |

## VANOS Configuration

### Dual VANOS Setup

**Intake VANOS:**
```
VVT Mode: On/Off or PID Control
VVT Output Pin: Configured per board
VVT Advance Cold: 0°
VVT Advance Hot: 20-25°
VVT Load Threshold: 60 kPa
VVT RPM Threshold: 3000
```

**Exhaust VANOS:**
```
VVT2 Mode: On/Off or PID Control
VVT2 Output Pin: Configured per board
VVT2 Advance Cold: 0°
VVT2 Advance Hot: 15-20°
```

### VANOS Table Example

| RPM \ Load | 20% | 40% | 60% | 80% | 100% |
|------------|-----|-----|-----|-----|------|
| 1000 | 0 | 0 | 5 | 10 | 15 |
| 2000 | 0 | 5 | 15 | 20 | 22 |
| 3000 | 5 | 15 | 20 | 23 | 25 |
| 4000 | 10 | 18 | 22 | 25 | 25 |
| 5000 | 15 | 20 | 23 | 25 | 25 |
| 6000 | 15 | 20 | 22 | 24 | 24 |

## DTC (Diagnostic Trouble Codes)

### BMW-Specific DTCs

**Engine Codes:**
- P0171/P0172: System lean/rich Bank 1
- P0174/P0175: System lean/rich Bank 2
- P0300-P0306: Misfire detection
- P0335-P0338: Crankshaft sensor
- P0340-P0343: Camshaft sensor

**BMW Manufacturer Codes:**
- P1083: Mixture lean Bank 1
- P1084: Mixture rich Bank 1
- P1085: Mixture lean Bank 2
- P1086: Mixture rich Bank 2
- P1188: Fuel trim lean limit
- P1189: Fuel trim rich limit
- P1519: Idle valve stuck open
- P1520: Idle valve stuck closed
- P1550: Throttle actuator fault

### Reading DTCs with BMW Tools

The SCG-ECU 2.0 responds to:
- INPA/ISTA diagnostic software
- Standard OBD-II scanners
- BMW-specific diagnostic tools

## Sensor Calibration

### Coolant Temperature (M54)

**BMW Sensor (Blue Ring):**
```
Bias Resistor: 2490 ohms
Calibration:
  -40°C: 100,000Ω → ADC ~1020
   20°C: 2,500Ω  → ADC ~512
   80°C: 300Ω    → ADC ~112
  120°C: 80Ω     → ADC ~32
```

### Intake Air Temperature

**BMW MAF Integrated Sensor:**
```
Bias Resistor: 2490 ohms
Similar curve to CLT
```

### Throttle Position (ETB or Cable)

**Cable Throttle (pre-2003.5):**
```
Closed: 0.5V = 0%
WOT: 4.5V = 100%
```

**E-Throttle (2003.5+):**
```
Requires dual TPS
TPS1: 0.5-4.5V
TPS2: 4.5-0.5V (reverse)
```

## Fuel Table (VE) - M54B30

### Base VE Table

| RPM \ MAP | 20 | 40 | 60 | 80 | 100 |
|-----------|----|----|----|----|-----|
| 1000 | 28 | 35 | 42 | 50 | 58 |
| 2000 | 32 | 42 | 52 | 62 | 72 |
| 3000 | 36 | 48 | 58 | 70 | 80 |
| 4000 | 40 | 52 | 64 | 76 | 86 |
| 5000 | 42 | 55 | 68 | 80 | 90 |
| 6000 | 40 | 52 | 65 | 78 | 88 |
| 7000 | 38 | 50 | 62 | 74 | 84 |

## Ignition Table - M54B30

### Base Ignition

| RPM \ MAP | 20 | 40 | 60 | 80 | 100 |
|-----------|----|----|----|----|-----|
| 1000 | 15 | 18 | 18 | 15 | 12 |
| 2000 | 22 | 26 | 26 | 22 | 18 |
| 3000 | 28 | 32 | 32 | 28 | 24 |
| 4000 | 32 | 36 | 36 | 32 | 28 |
| 5000 | 34 | 38 | 36 | 32 | 28 |
| 6000 | 34 | 38 | 34 | 30 | 26 |
| 7000 | 32 | 36 | 32 | 28 | 24 |

### Knock Sensor Configuration

```
Knock Input: Enabled
Knock Threshold: 30%
Knock Retard: -3° per event
Maximum Retard: -12°
Recovery Rate: 0.5°/second
```

## DSC/ASC Integration

### Wheel Speed Input

The ECU receives wheel speeds from ASC1 (0x153):
- Front Left
- Front Right
- Rear Left
- Rear Right

**Vehicle Speed Calculation:**
```
VSS = Average of 4 wheel speeds
Stored in: currentStatus.vss
```

### Traction Control Support

The ECU can implement traction control using:
- Wheel speed differential detection
- Ignition cut (soft cut)
- Fuel cut (hard cut)

Configure in: **Settings → Engine Protection → Traction Control**

## Transmission Integration (EGS)

### Automatic Transmission

The ECU receives from EGS:
- Current gear (0=P, 1-6=gear, 7=R, 8=N)
- Torque request
- Shift status

**Gear Display:**
```
currentStatus.gear = Received gear position
```

### Manual Transmission

For manual gearbox, configure:
```
Gear Detection: VSS/RPM Ratio
```

Or use external gear position sensor.

## Wiring - E46 Specific

### DME Connector Pinout Reference

| Signal | OEM Pin | SCG-ECU Pin |
|--------|---------|-------------|
| CAN High | Pin 4 | PD0 |
| CAN Low | Pin 3 | PD1 |
| +12V Switched | Pin 1 | VIN |
| Ground | Pin 2 | GND |
| Crank Signal | Pin 38 | Crank Input |
| Cam Signal | Pin 39 | Cam Input |
| TPS | Pin 42 | Analog In |
| CLT | Pin 44 | Analog In |
| IAT | Pin 45 | Analog In |
| MAP | Pin 46 | Analog In |
| O2 B1S1 | Pin 50 | O2 Input |

### CAN Bus Termination

**Important:** E46 PT-CAN requires 120Ω termination.
- Check if vehicle has termination at cluster
- Add termination resistor if ECU is endpoint

## Troubleshooting

### Cluster Not Working

1. **No Tachometer:**
   - Verify CAN protocol = BMW
   - Check DME1 (0x316) transmission
   - Verify CAN speed = 500 kbps

2. **No Temp Gauge:**
   - Check DME2 (0x329) transmission
   - Verify CLT sensor calibration

3. **CEL Always On:**
   - Check for stored DTCs
   - Verify DME4 (0x545) status byte

### No Communication with Diagnostic Tools

1. Verify OBD-II enabled
2. Check CAN bus wiring
3. Confirm 0x7E8 response ID
4. Test with generic OBD scanner first

### Wrong Fuel Consumption Display

1. Check injector flow rate setting
2. Verify fuel density value
3. Confirm cylinder count = 6

### Vehicle Speed Issues

1. Check ASC1 (0x153) reception
2. Verify DSC module is communicating
3. Check wheel speed sensor wiring

## Performance Tuning Notes

### M54 Modifications

**Intake:**
- Stock airbox: Good flow
- Cold air intake: +5-10 HP
- Adjust IAT compensation

**Exhaust:**
- Headers: Adjust VE table
- Cat-back: Minor changes needed

**Forced Induction:**
- Supercharger/Turbo: Complete retune required
- Lower compression
- Upgrade injectors
- Enable boost control

---

**Version:** 1.0
**Date:** 2026-01-12
**Compatible with:** SCG-ECU 2.0 / Speeduino firmware
**Vehicle:** BMW E46 330i (2001) M54B30
