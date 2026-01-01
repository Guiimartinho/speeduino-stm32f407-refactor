# SCG-ECU 2.0 - Refactored Speeduino for STM32F407VGT6

<div align="center">

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/Platform-STM32F407VGT6-green.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html)
[![PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange.svg)](https://platformio.org/)
[![MISRA C:2012](https://img.shields.io/badge/MISRA_C:2012-100%25_Compliant-purple.svg)](#misra-c2012-compliance)
[![Tests](https://img.shields.io/badge/Tests-737_Passing-brightgreen.svg)](#test-coverage)
[![Build Status](https://img.shields.io/badge/Build-Passing-success.svg)](#build-status)
[![Code Quality](https://img.shields.io/badge/Nesting_Depth-Max_3-blue.svg)](#code-quality)

**A professional-grade, MISRA-compliant refactoring of Speeduino firmware**
**optimized for STM32F407VGT6 with 8x8 configuration**

[Features](#key-features) •
[Comparison](#comparison-with-original-speeduino) •
[Getting Started](#getting-started) •
[Documentation](#documentation) •
[Credits](#credits--acknowledgments)

</div>

---

## Overview

**SCG-ECU 2.0** is a **complete modular refactoring** of the original [Speeduino](https://github.com/noisymime/speeduino) open-source engine management firmware. This project transforms the monolithic codebase into a clean, testable, and maintainable architecture specifically optimized for the **STM32F407VGT6** platform with **8x8 configuration** (8 independent injectors + 8 ignition channels).

### What Makes This Different

This is **NOT** just a port of Speeduino. This project implements:

- **432+ nesting depth violations fixed** (MISRA C:2012 Rule 15.6)
- **50+ modular components** from monolithic files
- **737+ unit tests** with comprehensive coverage
- **12 pre-commit QA hooks** for automated quality assurance
- **Interface-based architecture** for testability
- **Vehicle-specific documentation** (BMW, VW)

---

## Key Features

### ✅ MISRA C:2012 Compliance

| Metric | Original Speeduino | SCG-ECU 2.0 |
|--------|-------------------|-------------|
| Nesting Depth Violations | **432+** | **0** |
| Maximum Nesting Depth | 8 levels | **3 levels** |
| MISRA C:2012 Rule 15.6 | ✗ FAIL | **✓ PASS** |
| Cyclomatic Complexity | High (~80 avg) | **< 10 (avg 3-5)** |
| Function Size | 100+ lines | **< 50 lines** |

### ✅ Modular Architecture

| Original File | Violations | Refactored Structure |
|---------------|------------|---------------------|
| `auxiliaries.cpp` (1276 lines) | 122 | → 9 focused modules |
| `decoders.cpp` (6236 lines) | Many | → 31 decoder implementations |
| `corrections.cpp` (1128 lines) | 70 | → 3 specialized modules |
| `idle.cpp` (821 lines) | 71 | → Clean guard clauses |
| `init.cpp` (3717 lines) | 35 | → Modular with helpers |

### ✅ Comprehensive Testing

```
Test Suites:     27 total
Test Cases:      737 passing
Pass Rate:       100%
Duration:        ~33 seconds
```

### ✅ Pre-commit Quality Assurance

- Nesting depth checker (max 3 levels)
- Spelling verification
- Trailing whitespace removal
- Mixed line ending fixes
- Large file detection
- Secret scanning
- Build verification
- Unit test execution

---

## Comparison with Original Speeduino

### Code Quality Metrics

```
┌─────────────────────────────────────┬───────────────┬───────────────┐
│ METRIC                              │ ORIGINAL      │ SCG-ECU 2.0   │
├─────────────────────────────────────┼───────────────┼───────────────┤
│ Nesting depth violations            │ 432+          │ 0             │
│ Maximum nesting depth               │ 8             │ 3             │
│ MISRA C:2012 Rule 15.6              │ ✗ FAIL        │ ✓ PASS        │
│ Monolithic files (>1000 lines)      │ 5+            │ 0             │
│ Test files                          │ 57            │ 73            │
│ Test cases                          │ ~300          │ 737+          │
│ Pre-commit QA hooks                 │ None          │ 12 hooks      │
│ Modular architecture                │ No            │ Yes           │
│ Interface patterns                  │ No            │ Yes           │
│ Board-specific isolation            │ Partial       │ Complete      │
└─────────────────────────────────────┴───────────────┴───────────────┘
```

### Original vs Refactored Code Example

```cpp
// ORIGINAL Speeduino - Deep nesting (depth 8)
void someFunction() {
  if (condition1) {
    if (condition2) {
      if (condition3) {
        if (condition4) {
          if (condition5) {
            // 5+ levels deep - hard to read/maintain
          }
        }
      }
    }
  }
}

// SCG-ECU 2.0 - Guard clauses (max depth 3)
void someFunction() {
  if (!condition1) { return; }
  if (!condition2) { return; }
  if (!condition3) { return; }

  // Clean, flat logic
  processCondition4And5();
}
```

---

## Hardware Specifications

### STM32F407VGT6 Configuration

```
Microcontroller: STM32F407VGT6 (ARM Cortex-M4F)
├── Clock:       168 MHz
├── Flash:       1 MB (512KB usable)
├── RAM:         192 KB (131KB usable)
├── FPU:         Hardware floating-point
└── DSP:         SIMD instructions

8x8 Configuration:
├── Injection:   8 independent channels (high-side drivers)
├── Ignition:    8 independent channels (low-side drivers)
├── ADC:         16 channels 12-bit with DMA
├── Timers:      14 timers (TIM1-TIM14) dedicated
└── CAN:         2x CAN 2.0B native
```

### Exclusive SCG-ECU 2.0 Features

| Feature | Description |
|---------|-------------|
| **LED Button System** | 3-button interface with visual LED feedback |
| **SPI EEPROM Support** | External flash storage for configurations |
| **Board Registry Pattern** | Multi-board support architecture |
| **Modular Auxiliary System** | Coordinator pattern for auxiliary controls |
| **Vehicle Documentation** | BMW E46 M54B30, VW Gol AP 1.8 guides |

---

## Build Status

### Current Build (2026-01-01)

```
Environment:     black_F407VE-EEPROM-SPI
Build:           ✓ SUCCESS (zero warnings)
Flash:           196,924 bytes (37.6% of 524KB)
RAM:             21,480 bytes (16.4% of 131KB)
MISRA C:2012:    100% compliant (project code)
Tests:           737 passing (100%)
Status:          Production Ready
```

### Available Build Environments

| Environment | Description |
|-------------|-------------|
| `black_F407VE` | Base STM32F407 Black Board |
| `black_F407VE-EEPROM-SPI` | With SPI EEPROM support |
| `black_F407VE-EEPROM-SRAM` | With SRAM EEPROM emulation |
| `black_F407VE-EEPROM-FRAM` | With FRAM support |
| `native` | Native tests (PC) |

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) CLI or VS Code + PlatformIO IDE
- Git
- ST-Link programmer or USB DFU bootloader

### Build & Flash

```bash
# Clone the repository
git clone https://github.com/Guiimartinho/speeduino-stm32f407-refactor.git
cd speeduino-stm32f407-refactor/firmware/speeduino

# Build
platformio run -e black_F407VE-EEPROM-SPI

# Upload to board
platformio run -e black_F407VE-EEPROM-SPI --target upload

# Run native tests
platformio test -e native
```

### Quick Verification

```bash
# Check nesting depth compliance
python scripts/hooks/check_nesting.py --all --summary-only

# Run unit tests
platformio test -e native

# Build and verify
platformio run -e black_F407VE-EEPROM-SPI
```

---

## Test Coverage

### Test Suite Summary

| Test Suite | Tests | Status |
|------------|-------|--------|
| test_auxiliaries | 50 | ✓ PASSED |
| test_corrections_massive | 36 | ✓ PASSED |
| test_decoders_massive | 24 | ✓ PASSED |
| test_engineProtection_massive | 18 | ✓ PASSED |
| test_idle_massive | 20 | ✓ PASSED |
| test_sensors_massive | 25 | ✓ PASSED |
| test_scheduling_massive | 30 | ✓ PASSED |
| test_storage | 15 | ✓ PASSED |
| test_logger | 12 | ✓ PASSED |
| test_utilities | 30 | ✓ PASSED |
| ... and 17 more | 477+ | ✓ PASSED |
| **TOTAL** | **737** | **100%** |

---

## Documentation

### Project Structure

```
docs/
├── guides/                    # Development guides
│   ├── contributing.md
│   └── PROJECT_PROGRESS_MASTER.md
├── reference/                 # Technical references
│   ├── 01_PROJETO_SCG_ECU_MASTER_REFERENCE.md
│   └── ...
├── reports/                   # Phase completion reports
├── bmw/                       # BMW E46 M54B30 documentation
└── vw/                        # VW Gol AP 1.8 documentation
```

### Key Documents

- **[Master Reference](docs/reference/01_PROJETO_SCG_ECU_MASTER_REFERENCE.md)** - Complete project overview
- **[Technical Requirements](docs/reference/02_REQUISITOS_TECNICOS.md)** - Mandatory standards
- **[Pinout Documentation](docs/reports/17_PINOUT_COMPLETO_SCG_ECU.md)** - Pin mapping

---

## Credits & Acknowledgments

### Original Speeduino Project

This project is built upon the excellent foundation of the **Speeduino** open-source engine management system:

- **Project:** [Speeduino](https://speeduino.com) - Open Source Engine Management
- **Creator:** **Josh Stewart** ([@noisymime](https://github.com/noisymime))
- **Repository:** [github.com/noisymime/speeduino](https://github.com/noisymime/speeduino)
- **License:** GNU General Public License v3.0

> *"A massive thank you to Josh Stewart and the entire Speeduino community for creating and maintaining an exceptional open-source ECU platform that has enabled countless enthusiasts to build their own engine management systems."*

### Hardware Design

**SCG-ECU 2.0 Board:**
- **Designer:** **[@dvjcodec](https://github.com/dvjcodec)**
- **Repository:** [github.com/dvjcodec/SCG-ECU-2.0-STM32F407-8x8](https://github.com/dvjcodec/SCG-ECU-2.0-STM32F407-8x8)
- **Features:** STM32F407VGT6 + 8x8 outputs + Internal WBO (SLC Free 2.0)

### Related STM32 Projects

- **SPECTRE Project** by [@Tjeerdie](https://github.com/Tjeerdie/SPECTRE) - STM32F407 Speeduino PCB
- **STM32_mega** by [@pazi88](https://github.com/pazi88/STM32_mega) - Arduino Mega replacement for Speeduino

### Community

Special thanks to the entire **Speeduino community** on the [Speeduino Forum](https://speeduino.com/forum/) for their continuous support, testing, and feedback.

---

## Roadmap

### Completed ✓

- [x] Complete modularization (50+ modules)
- [x] MISRA C:2012 compliance (100% project code)
- [x] 737+ unit tests with 100% pass rate
- [x] Pre-commit QA hooks (12 automated checks)
- [x] Vehicle-specific documentation (BMW, VW)
- [x] Interface-based architecture
- [x] Board registry pattern

### In Progress

- [ ] Hardware-In-Loop (HIL) testing
- [ ] CI/CD Pipeline (GitHub Actions)

### Planned

- [ ] Code coverage reports (target: 80%+)
- [ ] Additional vehicle profiles
- [ ] FreeRTOS migration (optional)

---

## License

This project is licensed under the **GNU General Public License v3.0** - compatible with the original Speeduino license.

See [LICENSE](LICENSE) for details.

---

## Safety Warning

> ⚠️ **IMPORTANT**: This is an experimental engine management system.
>
> - **NOT** certified for public road use
> - **NOT** suitable for safety-critical applications without extensive testing
> - **NO** warranty of any kind
> - Intended for **racing, off-road, and experimental use only**
> - Professional installation and tuning recommended

---

## Contributing

Contributions are welcome! Please read our [Contributing Guide](docs/guides/contributing.md) before submitting pull requests.

### Quality Requirements

All contributions must:
- Pass nesting depth check (max 3 levels)
- Pass all unit tests
- Follow MISRA C:2012 guidelines
- Include appropriate documentation

---

<div align="center">

**Last Updated:** 2026-01-01
**Version:** 2.0.0 - Production Ready

[![Stars](https://img.shields.io/github/stars/Guiimartinho/speeduino-stm32f407-refactor?style=social)](https://github.com/Guiimartinho/speeduino-stm32f407-refactor)
[![Forks](https://img.shields.io/github/forks/Guiimartinho/speeduino-stm32f407-refactor?style=social)](https://github.com/Guiimartinho/speeduino-stm32f407-refactor/fork)

*Built with ❤️ for the open-source automotive community*

</div>
