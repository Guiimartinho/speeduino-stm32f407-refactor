# Archived Workflows - SCG-ECU 2.0

This directory contains workflows from the original Speeduino project that have been **archived** as part of the SCG-ECU 2.0 simplification and adaptation process.

## Why These Were Archived

The SCG-ECU 2.0 project is a **single-platform** fork focused exclusively on **STM32F407VGT6**. The original Speeduino workflows were designed for multi-platform support (AVR, Teensy, multiple STM32 variants) and included external integrations not needed for this project.

## Archived Workflows

### Replaced by `ci.yml` (Consolidated CI/CD)

| Workflow | Reason for Archiving |
|----------|---------------------|
| `build-firmware.yml` | Replaced by `ci.yml` - simplified to STM32F407 only |
| `codespell.yml` | Integrated into `ci.yml` code-quality job |
| `validate-ini.yml` | Integrated into `ci.yml` code-quality job |
| `pr-memory-deltas-generate.yml` | Integrated into `ci.yml` memory-report job |
| `pr-memory-deltas-report.yaml` | Integrated into `ci.yml` memory-report job |

### Replaced by `release.yml` (Simplified Release)

| Workflow | Reason for Archiving |
|----------|---------------------|
| `create-release.yml` | Replaced by `release.yml` - removed Speeduino server uploads |

### Removed (Not Applicable for SCG-ECU 2.0)

| Workflow | Reason for Archiving |
|----------|---------------------|
| `upload-ini.yml` | HyperTuner integration not used in SCG-ECU 2.0 |
| `unit-tests.yml` | Remote hardware testing not available for SCG-ECU 2.0 |
| `sim-unit-tests.yml` | Simulator tests not implemented yet (future: add to `ci.yml`) |

## Active Workflows

The following workflows are **ACTIVE** in SCG-ECU 2.0:

### Primary Workflows
- **`ci.yml`** - Main CI/CD (build, quality, memory tracking)
- **`release.yml`** - Automated releases
- **`misra.yml`** - MISRA C:2012 compliance (critical for ECU)
- **`doxygen.yml`** - Documentation generation

## Consolidation Benefits

**Before (Speeduino):**
- 12 separate workflows
- Multi-platform complexity
- External server dependencies
- Fragmented CI/CD logic

**After (SCG-ECU 2.0):**
- 4 focused workflows
- Single platform (STM32F407)
- Self-contained (GitHub only)
- Unified CI/CD in `ci.yml`

## Restoration

If you need to restore any of these workflows:

1. Copy from `archived/` back to `workflows/`
2. Update for SCG-ECU 2.0 specifics (platform, branches, secrets)
3. Test thoroughly before committing

## Reference

For details on the new workflow structure, see:
- `docs/GITHUB_WORKFLOWS_REFERENCE.md` (complete technical documentation)
- `.github/workflows/ci.yml` (consolidated CI/CD)
- `.github/workflows/release.yml` (simplified releases)

---

**Archived:** 2025-10-29
**Project:** SCG-ECU 2.0 - STM32F407VGT6 8x8
**Methodology:** Structured Modularization
