# GITHUB WORKFLOWS REFERENCE - SCG-ECU 2.0
## CI/CD Architecture and Documentation

**Project:** SCG-ECU 2.0 - STM32F407VGT6 8x8
**Date:** 29/10/2025
**Version:** 1.0

---

## TABLE OF CONTENTS

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Active Workflows](#3-active-workflows)
4. [Configuration Files](#4-configuration-files)
5. [Archived Workflows](#5-archived-workflows)
6. [Usage Guide](#6-usage-guide)
7. [Maintenance](#7-maintenance)
8. [Migration from Speeduino](#8-migration-from-speeduino)

---

## 1. OVERVIEW

### 1.1 Purpose

This document provides complete technical reference for all GitHub Actions workflows in the SCG-ECU 2.0 project. The CI/CD infrastructure has been simplified and adapted from the original Speeduino project to focus exclusively on STM32F407VGT6 platform.

### 1.2 Design Principles

**SIMPLIFICATION:**
- Single platform focus (STM32F407VGT6)
- Consolidated workflows (12 -> 4)
- Self-contained (no external servers)

**QUALITY:**
- MISRA C:2012 compliance tracking
- Memory usage monitoring
- Automated documentation

**AUTOMATION:**
- Build on every push/PR
- Automatic releases on tags
- Documentation deployment

### 1.3 Workflow Summary

| Workflow | Purpose | Trigger | Critical |
|----------|---------|---------|----------|
| `ci.yml` | Build, test, quality | Push/PR | YES |
| `release.yml` | Automated releases | Tags | YES |
| `misra.yml` | MISRA compliance | Push/PR | YES |
| `doxygen.yml` | Documentation | Push | NO |

---

## 2. ARCHITECTURE

### 2.1 Directory Structure

```
.github/
├── CODEOWNERS              # Code ownership (Guiimartinho)
├── dependabot.yml          # Dependency updates
├── FUNDING.yml             # GitHub Sponsors (optional)
└── workflows/
    ├── ci.yml              # Main CI/CD workflow
    ├── release.yml         # Release automation
    ├── misra.yml           # MISRA compliance
    ├── doxygen.yml         # Documentation
    ├── codespell-ignored-words.txt
    └── archived/           # Old Speeduino workflows
        ├── README.md
        ├── build-firmware.yml
        ├── codespell.yml
        ├── create-release.yml
        ├── pr-memory-deltas-*.yml
        ├── sim-unit-tests.yml
        ├── unit-tests.yml
        ├── upload-ini.yml
        └── validate-ini.yml
```

### 2.2 Workflow Dependencies

```
ci.yml
├── Job 1: build
│   └── Artifact: firmware-stm32f407-{sha}
├── Job 2: memory-report (PRs only)
│   └── Artifact: memory-report-{sha}
├── Job 3: code-quality
│   ├── Codespell
│   └── INI validation
└── Job 4: build-summary (needs: build, code-quality)

release.yml (tags only)
├── Job 1: build-release
│   └── Artifact: firmware-{version}
└── Job 2: create-release (needs: build-release)
    └── GitHub Release

misra.yml
└── Job: misra-scan
    └── Artifact: misra-results-{sha}

doxygen.yml
├── Job 1: build-docs
│   └── Artifact: doxygen-docs-{sha}
└── Job 2: deploy-pages (needs: build-docs)
    └── GitHub Pages
```

### 2.3 Consolidation Mapping

**Original Speeduino (12 workflows) -> SCG-ECU 2.0 (4 workflows):**

```
build-firmware.yml        ]
codespell.yml             ]
validate-ini.yml          ] --> ci.yml (consolidated)
pr-memory-deltas-*.yml    ]

create-release.yml        --> release.yml (simplified)

misra.yml                 --> misra.yml (adapted)

doxygen.yml               --> doxygen.yml (adapted)

upload-ini.yml            --> REMOVED (no HyperTuner)
unit-tests.yml            --> REMOVED (no remote hardware)
sim-unit-tests.yml        --> REMOVED (future: integrate to ci.yml)
```

---

## 3. ACTIVE WORKFLOWS

### 3.1 ci.yml - Main CI/CD

**File:** `.github/workflows/ci.yml`
**Purpose:** Consolidated CI/CD for build, quality checks, and memory tracking

#### Triggers

```yaml
on:
  push:
    branches: [ main, master, '*-fixes' ]
  pull_request:
    branches: [ main, master ]
  workflow_dispatch:
```

#### Jobs

**JOB 1: build**
- Runs on: `ubuntu-latest`
- Steps:
  1. Checkout code
  2. Cache pip + PlatformIO
  3. Setup Python 3.10
  4. Install PlatformIO
  5. Build STM32F407 firmware (`black_F407VE-EEPROM-SPI`)
  6. Check build size
  7. Upload firmware artifact (30 days retention)
  8. Upload build info (7 days retention)

**Output:**
- `firmware-stm32f407-{sha}.bin`
- `firmware-stm32f407-{sha}.elf`
- `.map` files

**JOB 2: memory-report** (PRs only)
- Runs on: `ubuntu-latest`
- Condition: `if: github.event_name == 'pull_request'`
- Steps:
  1. Checkout code
  2. Compile with memory tracking (arduino/compile-sketches)
  3. Upload memory report artifact
  4. Report size deltas to PR comment

**Output:**
- PR comment with RAM/Flash deltas

**JOB 3: code-quality**
- Runs on: `ubuntu-latest`
- Steps:
  1. Checkout code
  2. Install codespell
  3. Update dictionaries
  4. Run codespell on `./speeduino/`
  5. Validate TunerStudio INI

**Output:**
- Codespell report (continue-on-error)
- INI validation result

**JOB 4: build-summary**
- Runs on: `ubuntu-latest`
- Needs: `[build, code-quality]`
- Condition: `if: always()`
- Steps:
  1. Check job results
  2. Fail if build failed
  3. Warn if quality failed (non-blocking)

#### Environment Variables

```yaml
env:
  PIO_ENV: black_F407VE-EEPROM-SPI
  SKETCHES_REPORTS_PATH: sketches-reports
```

#### Key Features

- **Single Platform:** STM32F407 only
- **Memory Tracking:** Automatic delta reports on PRs
- **Code Quality:** Spelling + INI validation
- **Fast Builds:** PlatformIO caching

---

### 3.2 release.yml - Automated Releases

**File:** `.github/workflows/release.yml`
**Purpose:** Automated firmware releases when version tags are pushed

#### Triggers

```yaml
on:
  push:
    tags:
      - 'v*'  # v1.0.0, v2.1.3, etc.
```

#### Jobs

**JOB 1: build-release**
- Runs on: `ubuntu-latest`
- Steps:
  1. Checkout code
  2. Cache pip + PlatformIO
  3. Setup Python 3.10
  4. Install PlatformIO
  5. Build STM32F407 firmware
  6. Extract version from tag
  7. Rename firmware with version
  8. Upload artifacts (90 days retention)

**Output:**
- `firmware-scu-ecu-2.0-{version}.bin`
- `firmware-scu-ecu-2.0-{version}.elf`

**JOB 2: create-release**
- Runs on: `ubuntu-latest`
- Needs: `build-release`
- Steps:
  1. Checkout code (full history)
  2. Extract version
  3. Download firmware artifacts
  4. Build changelog (mikepenz/release-changelog-builder)
  5. Create release notes
  6. Create GitHub Release
  7. Print summary

**Release Assets:**
- `firmware-scu-ecu-2.0-{version}.bin`
- `firmware-scu-ecu-2.0-{version}.elf`
- `speeduino.ini`

**Changelog Categories:**
- Features (labels: feature, enhancement)
- Bug Fixes (labels: bug, fix)
- Documentation (labels: documentation, docs)
- Other Changes

#### Key Features

- **Automatic Triggering:** Push tag -> build -> release
- **Versioned Binaries:** Firmware named with version
- **Rich Changelogs:** Auto-generated from commits
- **Installation Instructions:** Included in release notes

#### Usage

```bash
# Create and push a tag
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0

# Workflow automatically:
# 1. Builds firmware
# 2. Creates GitHub Release
# 3. Uploads binaries
```

---

### 3.3 misra.yml - MISRA Compliance

**File:** `.github/workflows/misra.yml`
**Purpose:** MISRA C:2012 compliance scanning for automotive safety

#### Triggers

```yaml
on:
  push:
    branches: [ master, main ]
  pull_request:
    branches: [ master, main ]
  workflow_dispatch:
```

#### Jobs

**JOB: misra-scan**
- Runs on: `ubuntu-22.04`
- Steps:
  1. Checkout code
  2. Install cppcheck + dos2unix
  3. Verify cppcheck version
  4. Run MISRA scan (misra/check_misra.sh)
  5. Extract violation count
  6. Create summary
  7. Upload results (90 days retention)
  8. Comment on PR (if PR)
  9. Check violation threshold (fail if > 1000)

**Output:**
- `misra-results-{sha}/results.txt`
- `misra-results-{sha}/error_count.txt`
- `misra-summary.md`
- PR comment (if PR)

#### Threshold

```bash
THRESHOLD=1000  # Adjust as needed
```

Workflow fails if violations exceed threshold.

#### Key Features

- **Automotive Standards:** MISRA C:2012 compliance
- **PR Comments:** Violation count posted to PRs
- **Threshold Enforcement:** Configurable failure threshold
- **Artifact Retention:** 90 days for analysis

#### Adapted from Speeduino

**Removed:**
- Speeduino Gist dependency
- Dynamic badge (can be created manually)

**Added:**
- PR comments
- Threshold check
- Better summary

---

### 3.4 doxygen.yml - Documentation

**File:** `.github/workflows/doxygen.yml`
**Purpose:** Automatic Doxygen documentation generation and deployment

#### Triggers

```yaml
on:
  push:
    branches: [ master, main ]
  workflow_dispatch:
```

#### Permissions

```yaml
permissions:
  contents: read
  pages: write
  id-token: write
```

#### Jobs

**JOB 1: build-docs**
- Runs on: `ubuntu-latest`
- Steps:
  1. Checkout code
  2. Build Doxygen (mattnotmitt/doxygen-action)
  3. Verify HTML output
  4. Upload documentation artifact (30 days retention)

**Output:**
- `doxygen-docs-{sha}/` (HTML documentation)

**JOB 2: deploy-pages**
- Runs on: `ubuntu-latest`
- Needs: `build-docs`
- Condition: `if: github.event_name != 'pull_request'`
- Environment: `github-pages`
- Steps:
  1. Download docs artifact
  2. Setup GitHub Pages
  3. Upload to Pages
  4. Deploy to Pages
  5. Print URL

**Output:**
- GitHub Pages site (https://{user}.github.io/{repo}/)

#### Key Features

- **Automatic Updates:** Every push to master/main
- **GitHub Pages:** No separate repository needed
- **Artifact Backup:** 30 days retention
- **URL Display:** Deployment URL in logs

#### Adapted from Speeduino

**Changed:**
- Uses GitHub Pages (not separate repo)
- Simplified deployment
- No external secrets needed

**Requirement:**
Enable GitHub Pages in repository settings:
- Settings -> Pages
- Source: GitHub Actions

---

## 4. CONFIGURATION FILES

### 4.1 CODEOWNERS

**File:** `.github/CODEOWNERS`
**Purpose:** Define code ownership for automatic PR review requests

```
# SCG-ECU 2.0 - Global owner
* @Guiimartinho

# Original Speeduino owners (for reference)
# * @noisymime
# /speeduino/board_stm32* @VitorBoss
```

**Usage:**
- All PRs automatically request review from @Guiimartinho
- Can add specific owners for subdirectories

---

### 4.2 dependabot.yml

**File:** `.github/dependabot.yml`
**Purpose:** Automatic dependency updates for GitHub Actions

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "daily"
```

**Behavior:**
- Checks daily for GitHub Actions updates
- Creates PRs automatically
- Example: `actions/checkout@v3` -> `actions/checkout@v4`

**Benefits:**
- Security patches
- New features
- No manual checking

---

### 4.3 FUNDING.yml

**File:** `.github/FUNDING.yml`
**Purpose:** GitHub Sponsors button (optional)

```yaml
# SCG-ECU 2.0 Funding
# Original Speeduino project funding (for reference)
# github: noisymime

# Uncomment below if you want to enable GitHub Sponsors
# github: Guiimartinho
```

**Current State:** Disabled (all commented)

**To Enable:**
1. Uncomment `github: Guiimartinho`
2. Setup GitHub Sponsors for your account
3. Commit changes

---

### 4.4 codespell-ignored-words.txt

**File:** `.github/workflows/codespell-ignored-words.txt`
**Purpose:** Words to ignore in spelling check

```
fram      # FRAM memory
iterm     # Terminal
ntegral   # Integral (common typo)
numer     # Numerator
numers    # Numerators
wel       # WEL (Write Enable Latch)
```

**Add Technical Terms:**
```
ecu
misra
stm32
pwm
dwell
```

---

## 5. ARCHIVED WORKFLOWS

### 5.1 Overview

Original Speeduino workflows moved to `.github/workflows/archived/`:

```
archived/
├── README.md
├── build-firmware.yml
├── codespell.yml
├── create-release.yml
├── pr-memory-deltas-generate.yml
├── pr-memory-deltas-report.yaml
├── sim-unit-tests.yml
├── unit-tests.yml
├── upload-ini.yml
└── validate-ini.yml
```

### 5.2 Reason for Archiving

| Workflow | Replaced By | Reason |
|----------|-------------|--------|
| build-firmware.yml | ci.yml | Multi-platform -> Single platform |
| codespell.yml | ci.yml | Consolidated into code-quality job |
| validate-ini.yml | ci.yml | Consolidated into code-quality job |
| pr-memory-deltas-*.yml | ci.yml | Consolidated into memory-report job |
| create-release.yml | release.yml | Removed server uploads |
| upload-ini.yml | REMOVED | No HyperTuner integration |
| unit-tests.yml | REMOVED | No remote hardware |
| sim-unit-tests.yml | REMOVED | Future: add to ci.yml |

### 5.3 Restoration

If needed:

```bash
# Restore a workflow
cp .github/workflows/archived/build-firmware.yml .github/workflows/

# Edit for SCG-ECU 2.0
# - Update branches (master -> main)
# - Remove Speeduino-specific steps
# - Update secrets

# Test
git commit -m "Restore workflow X"
git push
```

---

## 6. USAGE GUIDE

### 6.1 Development Workflow

**1. Create Feature Branch**
```bash
git checkout -b feature/new-module
# Make changes
git commit -m "feat: add new module"
git push origin feature/new-module
```

**2. Create Pull Request**
- GitHub -> New Pull Request
- Triggers: `ci.yml` (all jobs + memory-report)
- Check: Build status, quality, memory deltas

**3. Review**
- Review PR comments (memory usage)
- Fix any build/quality failures
- Merge when green

**4. Master/Main Push**
- Triggers: `ci.yml`, `misra.yml`, `doxygen.yml`
- Documentation automatically updates
- MISRA report generated

### 6.2 Release Workflow

**1. Ensure Clean State**
```bash
git checkout main
git pull
# Verify CI passed
```

**2. Create Tag**
```bash
# Semantic versioning
git tag -a v1.0.0 -m "Release v1.0.0 - Initial release"
git tag -a v1.1.0 -m "Release v1.1.0 - Bug fixes"
git tag -a v2.0.0 -m "Release v2.0.0 - Major update"
```

**3. Push Tag**
```bash
git push origin v1.0.0
```

**4. Workflow Automatically:**
- Builds firmware
- Generates changelog
- Creates GitHub Release
- Uploads binaries

**5. Verify Release**
- GitHub -> Releases
- Check firmware binaries
- Test installation

### 6.3 Manual Workflow Trigger

**Via GitHub UI:**
1. GitHub -> Actions
2. Select workflow (ci.yml, doxygen.yml, etc.)
3. Run workflow -> Run on branch

**Via gh CLI:**
```bash
# Install gh
gh workflow run ci.yml --ref main
gh workflow run doxygen.yml
```

### 6.4 Monitoring

**GitHub Actions Tab:**
- All workflow runs
- Logs for debugging
- Artifact downloads

**Artifacts:**
```bash
# Download via gh CLI
gh run download <run-id>

# Or via GitHub UI
Actions -> Workflow Run -> Artifacts
```

**Notifications:**
- Email on failure (default)
- Configure in Settings -> Notifications

---

## 7. MAINTENANCE

### 7.1 Updating Workflows

**Best Practices:**
1. Test in feature branch first
2. Use `workflow_dispatch` for manual testing
3. Monitor first run carefully
4. Document changes in commit message

**Example:**
```yaml
# Test change
on:
  workflow_dispatch:  # Add temporarily
  push:
    branches: [ feature/test-workflow ]

# After testing, update to:
on:
  push:
    branches: [ main, master ]
  pull_request:
    branches: [ main, master ]
```

### 7.2 Action Updates

**Dependabot PRs:**
- Review changelog
- Test in staging if critical
- Merge when confident

**Manual Updates:**
```yaml
# Before
- uses: actions/checkout@v3

# After (check latest)
- uses: actions/checkout@v4
```

### 7.3 Troubleshooting

**Build Fails:**
```bash
# Check logs
gh run view <run-id> --log

# Re-run
gh run rerun <run-id>

# Debug locally
platformio run -e black_F407VE-EEPROM-SPI
```

**Memory Report Missing:**
- Check PR exists (memory-report only runs on PRs)
- Verify arduino/compile-sketches step passed
- Check artifacts

**MISRA Threshold:**
```yaml
# Adjust in misra.yml
THRESHOLD=1000  # Increase if needed
```

**Doxygen Deployment:**
- Enable GitHub Pages (Settings -> Pages)
- Check permissions (pages: write)
- Verify Doxyfile exists

---

## 8. MIGRATION FROM SPEEDUINO

### 8.1 Changes Made

**Removed:**
- Multi-platform builds (AVR, Teensy, other STM32)
- Speeduino server uploads
- Speeduino Gist (MISRA badge)
- Speeduino doxygen repo
- Discord notifications
- HyperTuner uploads
- Remote hardware testing

**Simplified:**
- 12 workflows -> 4 workflows
- Consolidated CI/CD in ci.yml
- GitHub Pages for docs (same repo)
- Self-contained (no external services)

**Added:**
- PR memory delta comments
- MISRA threshold enforcement
- Automatic changelog generation
- Versioned firmware binaries

### 8.2 Platform Changes

**Before (Speeduino):**
```yaml
- megaatmega2560
- megaatmega2560-6-3
- megaatmega2560-8-1
- teensy35
- teensy36
- teensy41
- black_F407VE
- BlackPill_F401CC
- BlackPill_F411CE_USB
- black_F407VE-EEPROM-SRAM
- black_F407VE-EEPROM-SPI
- black_F407VE-EEPROM-FRAM
```

**After (SCG-ECU 2.0):**
```yaml
- black_F407VE-EEPROM-SPI  # ONLY
```

### 8.3 Secrets Migration

**No Longer Needed:**
- `WEB_PWD` (Speeduino server)
- `GH_DOXYGEN` (separate repo)
- `DISCORD_WEBHOOK`
- `MISRA_GIST`
- `HYPER_TUNER_*`
- `PLATFORMIO_AUTH_TOKEN` (remote hardware)

**Still Used:**
- `GITHUB_TOKEN` (automatic, no setup needed)

### 8.4 Branch Changes

**Speeduino:**
```yaml
branches: [ master ]
```

**SCG-ECU 2.0:**
```yaml
branches: [ main, master ]  # Support both
```

---

## APPENDIX A: WORKFLOW REFERENCE CARD

```
WORKFLOW          TRIGGER         JOBS    ARTIFACTS          CRITICAL
========================================================================
ci.yml            Push/PR         4       firmware, memory   YES
release.yml       Tags            2       versioned fw       YES
misra.yml         Push/PR         1       misra results      YES
doxygen.yml       Push            2       HTML docs          NO

TOTAL WORKFLOWS: 4
TOTAL JOBS: 9
PLATFORMS: 1 (STM32F407VGT6)
```

---

## APPENDIX B: GITHUB PAGES SETUP

**Enable GitHub Pages:**

1. Repository Settings
2. Pages (left sidebar)
3. Source: GitHub Actions
4. Save

**Access:**
- URL: https://{user}.github.io/{repo}/
- Updates automatically on push to main/master

---

## APPENDIX C: QUICK REFERENCE

**Common Commands:**
```bash
# Trigger CI manually
gh workflow run ci.yml

# Create release
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0

# Download artifacts
gh run download <run-id>

# View workflow logs
gh run view <run-id> --log

# List workflows
gh workflow list

# Check workflow status
gh run list --workflow=ci.yml
```

**Important Files:**
```
.github/workflows/ci.yml          # Main CI/CD
.github/workflows/release.yml     # Releases
.github/workflows/misra.yml       # MISRA compliance
.github/workflows/doxygen.yml     # Documentation
.github/CODEOWNERS                # Code ownership
.github/dependabot.yml            # Dependency updates
```

---

## REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-10-29 | Initial documentation - structured refactoring adaptation complete |

---

**END OF DOCUMENT**
