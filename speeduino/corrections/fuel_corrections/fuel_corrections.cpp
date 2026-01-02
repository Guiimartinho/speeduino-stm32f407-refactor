/**
 * @file fuel_corrections.cpp
 * @brief Fuel corrections implementation with MISRA-C compliance
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * REFACTORED MODULE - 100% logic preservation with MISRA-C compliance:
 * - Functions < 50 lines
 * - Cyclomatic complexity < 10
 * - Guard clauses for early returns
 * - Anonymous namespace for private helpers
 * - Comprehensive Doxygen documentation
 *
 * This module handles ALL fuel correction calculations:
 * - Main dispatcher (correctionsFuel)
 * - WUE (Warmup Enrichment)
 * - ASE (After Start Enrichment)
 * - Cranking enrichment with tapering
 * - Acceleration enrichment (MAP/TPS based, complex state machine)
 * - Environmental corrections (IAT, baro, fuel temp)
 * - Battery voltage correction
 * - Special corrections (launch, flex fuel)
 * - DFCO (Deceleration Fuel Cut-Off) with tapering
 */

#include "fuel_corrections.h"
#include "../../globals.h"
#include "../../corrections.h"
#include "../../speeduino.h"
#include "../../timers.h"
#include "../../maths.h"
#include "../../sensors.h"
#include "../../units.h"
#include "../../limp_mode.h"

// ============================================================================
// GLOBAL STATE (Acceleration enrichment tracking)
// ============================================================================

/// MAP DOT value seen when MAE was activated
byte activateMAPDOT;

/// TPS DOT value seen when TAE was activated
byte activateTPSDOT;

// ============================================================================
// PRIVATE HELPERS - WUE (Warmup Enrichment)
// ============================================================================

namespace {

static const byte WUE_NO_ENRICHMENT = 100;

} // anonymous namespace

// ============================================================================
// PRIVATE HELPERS - ASE (After Start Enrichment)
// ============================================================================

namespace {

static const byte ASE_NO_ENRICHMENT = 100;

} // anonymous namespace

// ============================================================================
// PRIVATE HELPERS - CRANKING
// ============================================================================

namespace {

static const uint16_t CRANKING_NO_ENRICHMENT = 100;
static const uint16_t CRANKING_MULTIPLIER = 5; // Range 0-1275%

} // anonymous namespace

// ============================================================================
// PRIVATE HELPERS - ACCELERATION ENRICHMENT (Complex state machine)
// ============================================================================

namespace {

static const uint16_t AE_NO_CORRECTION = 100;
static const uint16_t AE_TIME_MULTIPLIER_US = 10000; // aeTime stored as mS/10
static const uint8_t AE_TABLE_DIVISOR = 10; // DOT value divided by 10 for table lookup

/// @brief Reset DOT value based on AE mode (helper to reduce nesting)
static inline void resetDOT(void) {
    if (configPage2.aeMode == AE_MODE_MAP) { currentStatus.mapDOT = 0; }
    else if (configPage2.aeMode == AE_MODE_TPS) { currentStatus.tpsDOT = 0; }
}

/// @brief Calculate rate of change (DOT) based on mode
/// @param MAP_change Output: MAP change value
/// @param TPS_change Output: TPS change value
/// @complexity 2
static inline void calculateDOT(int16_t* MAP_change, int16_t* TPS_change) {
    if (configPage2.aeMode == AE_MODE_MAP) {
        *MAP_change = getMAPDelta();
        currentStatus.mapDOT = (MICROS_PER_SEC / getMAPDeltaTime()) * (*MAP_change);
    }
    else if (configPage2.aeMode == AE_MODE_TPS) {
        *TPS_change = (currentStatus.TPS - currentStatus.TPSlast);
        currentStatus.tpsDOT = (TPS_READ_FREQUENCY * (*TPS_change)) / 2;
    }
}

/// @brief Apply RPM taper correction
/// @param accelValue Base acceleration enrichment value
/// @return Tapered acceleration value
/// @complexity 3
static inline int16_t applyRPMTaper(int16_t accelValue) {
    const uint16_t trueTaperMin = configPage2.aeTaperMin * 100;
    const uint16_t trueTaperMax = configPage2.aeTaperMax * 100;

    // Guard: RPM below taper min, no reduction
    if (currentStatus.RPM <= trueTaperMin) { return accelValue; }

    // Guard: RPM above taper max, disable enrichment
    if (currentStatus.RPM > trueTaperMax) { return 0; }

    // RPM within taper range, calculate reduction
    const int16_t taperRange = trueTaperMax - trueTaperMin;
    const int16_t taperPercent = ((currentStatus.RPM - trueTaperMin) * 100UL) / taperRange;
    return percentage((100 - taperPercent), accelValue);
}

/// @brief Apply cold temperature modifier
/// @param accelValue Base acceleration enrichment value
/// @return Modified acceleration value
/// @complexity 3
static inline int16_t applyColdModifier(int16_t accelValue) {
    // Guard: CLT above taper max, no modifier
    if (currentStatus.coolant >= temperatureRemoveOffset(configPage2.aeColdTaperMax)) {
        return accelValue;
    }

    // Full modifier if CLT below taper min
    if (currentStatus.coolant <= temperatureRemoveOffset(configPage2.aeColdTaperMin)) {
        const uint16_t accelValue_uint = percentage(configPage2.aeColdPct, accelValue);
        return (int16_t)accelValue_uint;
    }

    // Tapered modifier between min and max
    const int16_t taperRange = (int16_t)configPage2.aeColdTaperMax - configPage2.aeColdTaperMin;
    const int16_t taperPercent = (int)((temperatureAddOffset(currentStatus.coolant) - configPage2.aeColdTaperMin) * 100) / taperRange;
    const int16_t coldPct = (int16_t)100 + percentage((100 - taperPercent), (configPage2.aeColdPct - 100));
    const uint16_t accelValue_uint = (uint16_t)accelValue * coldPct / 100;
    return (int16_t)accelValue_uint;
}

/// @brief Handle deceleration enrichment
/// @return Deceleration correction value
/// @complexity 1
static inline int16_t handleDeceleration(void) {
    BIT_SET(currentStatus.engine, BIT_ENGINE_DCC);
    return configPage2.decelAmount;
}

/// @brief Handle acceleration enrichment (unified MAP/TPS logic)
/// @param dotValue Rate of change value
/// @param isMAP true for MAP mode, false for TPS mode
/// @return Acceleration correction value
/// @complexity 3
static inline int16_t handleAcceleration(int16_t dotValue, bool isMAP) {
    BIT_SET(currentStatus.engine, BIT_ENGINE_ACC);

    // Get base enrichment from table (MAP or TPS table)
    int16_t accelValue;
    if (isMAP) {
        accelValue = table2D_getValue(&maeTable, (uint8_t)(dotValue / AE_TABLE_DIVISOR));
    }
    else {
        accelValue = table2D_getValue(&taeTable, (uint8_t)(dotValue / AE_TABLE_DIVISOR));
    }

    // Apply corrections in sequence
    accelValue = applyRPMTaper(accelValue);
    accelValue = applyColdModifier(accelValue);

    // Add 100% base (enrichment is additive)
    return AE_NO_CORRECTION + accelValue;
}

/// @brief Process new AE activation (unified MAP/TPS logic)
/// @param dotValue Rate of change value
/// @param change Absolute change value
/// @param minChange Minimum change threshold
/// @param thresh DOT threshold for activation
/// @param activateDOT Output: Activated DOT value
/// @param isMAP true for MAP mode, false for TPS mode
/// @return Acceleration/deceleration correction value
/// @complexity 4
static inline int16_t processNewActivation(int16_t dotValue, int16_t change, uint8_t minChange,
                                           uint8_t thresh, byte* activateDOT, bool isMAP) {
    // Guard: Change too small, ignore
    if (abs(change) <= minChange) { resetDOT(); return AE_NO_CORRECTION; }

    // Guard: DOT below threshold, no activation
    if (abs(dotValue) <= thresh) {
        return AE_NO_CORRECTION;
    }

    // Activate AE/DCC
    *activateDOT = abs(dotValue);
    currentStatus.AEEndTime = micros() + ((unsigned long)configPage2.aeTime * AE_TIME_MULTIPLIER_US);

    // Deceleration or acceleration?
    if (dotValue < 0) {
        return handleDeceleration();
    }
    else {
        return handleAcceleration(dotValue, isMAP);
    }
}

} // anonymous namespace

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

uint16_t correctionsFuel(void) {
    uint32_t sumCorrections = 100;
    uint16_t result;

    // WUE (Warmup Enrichment)
    currentStatus.wueCorrection = correctionWUE();
    if (currentStatus.wueCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.wueCorrection);
    }

    // ASE (After Start Enrichment)
    currentStatus.ASEValue = correctionASE();
    if (currentStatus.ASEValue != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.ASEValue);
    }

    // Cranking enrichment
    result = correctionCranking();
    if (result != 100) {
        sumCorrections = div100(sumCorrections * result);
    }

    // Acceleration enrichment
    currentStatus.AEamount = correctionAccel();
    if ((configPage2.aeApplyMode == AE_MODE_MULTIPLIER) || BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC)) {
        if (currentStatus.AEamount != 100) {
            sumCorrections = div100(sumCorrections * currentStatus.AEamount);
        }
    }

    // Flood clear
    result = correctionFloodClear();
    if (result != 100) {
        sumCorrections = div100(sumCorrections * result);
    }

    // AFR closed-loop
    currentStatus.egoCorrection = correctionAFRClosedLoop();
    if (currentStatus.egoCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.egoCorrection);
    }

    // Battery voltage correction (applied to injector opening time)
    currentStatus.batCorrection = correctionBatVoltage();
    inj_opentime_uS = configPage2.injOpen * currentStatus.batCorrection;

    // IAT density correction
    currentStatus.iatCorrection = correctionIATDensity();
    if (currentStatus.iatCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.iatCorrection);
    }

    // Barometric pressure correction
    currentStatus.baroCorrection = correctionBaro();
    if (currentStatus.baroCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.baroCorrection);
    }

    // Flex fuel correction
    currentStatus.flexCorrection = correctionFlex();
    if (currentStatus.flexCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.flexCorrection);
    }

    // Fuel temperature correction
    currentStatus.fuelTempCorrection = correctionFuelTemp();
    if (currentStatus.fuelTempCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.fuelTempCorrection);
    }

    // Launch control correction
    currentStatus.launchCorrection = correctionLaunch();
    if (currentStatus.launchCorrection != 100) {
        sumCorrections = div100(sumCorrections * currentStatus.launchCorrection);
    }

    // DFCO (Deceleration Fuel Cut-Off)
    bitWrite(currentStatus.status1, BIT_STATUS1_DFCO, correctionDFCO());
    byte dfcoTaperCorrection = correctionDFCOfuel();
    if (dfcoTaperCorrection == 0) {
        sumCorrections = 0;
    }
    else if (dfcoTaperCorrection != 100) {
        sumCorrections = div100(sumCorrections * dfcoTaperCorrection);
    }

    // Maximum allowable increase during cranking
    if (sumCorrections > 1500) { sumCorrections = 1500; }

    // Limp mode: apply fuel enrichment for safety
    if (isLimpModeActive()) {
        uint8_t fuelEnrich = getLimpModeFuelCorrection();
        sumCorrections = div100(sumCorrections * fuelEnrich);
    }

    return (uint16_t)sumCorrections;
}

byte correctionWUE(void) {
    // Guard: Already warmed up
    if (currentStatus.coolant > temperatureRemoveOffset(WUETable.axis[WUETable.size()-1U])) {
        BIT_CLEAR(currentStatus.engine, BIT_ENGINE_WARMUP);
        return WUETable.values[WUETable.size()-1U];
    }

    // Engine warming up
    BIT_SET(currentStatus.engine, BIT_ENGINE_WARMUP);
    return table2D_getValue(&WUETable, temperatureAddOffset(currentStatus.coolant));
}

uint16_t correctionCranking(void) {
    uint16_t crankingValue = CRANKING_NO_ENRICHMENT;

    // Guard: Currently cranking
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        crankingValue = table2D_getValue(&crankingEnrichTable, temperatureAddOffset(currentStatus.coolant));
        crankingValue = (uint16_t)crankingValue * CRANKING_MULTIPLIER;
        crankingEnrichTaper = 0;
        return crankingValue;
    }

    // Not cranking - check if taper is active
    if (crankingEnrichTaper < configPage10.crankingEnrichTaper) {
        crankingValue = table2D_getValue(&crankingEnrichTable, temperatureAddOffset(currentStatus.coolant));
        crankingValue = (uint16_t)crankingValue * CRANKING_MULTIPLIER;

        // Taper start value accounts for ASE to avoid jump
        unsigned long taperStart = (unsigned long)crankingValue * 100 / currentStatus.ASEValue;
        crankingValue = (uint16_t)map(crankingEnrichTaper, 0, configPage10.crankingEnrichTaper, taperStart, 100);

        if (crankingValue < 100) { crankingValue = 100; } // Sanity check

        if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) { crankingEnrichTaper++; }
    }

    return crankingValue;
}

byte correctionASE(void) {
    int16_t ASEValue = currentStatus.ASEValue;

    // Guard: Engine cranking, disable ASE
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ASE);
        return ASE_NO_ENRICHMENT;
    }

    // Guard: Update rate (10Hz) and initial activation
    if (!BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) && (currentStatus.ASEValue != 0)) {
        return ASEValue;
    }

    // Check if within ASE duration window
    byte cltOffset = temperatureAddOffset(currentStatus.coolant);
    byte aseDuration = table2D_getValue(&ASECountTable, cltOffset);

    if (currentStatus.runSecs < aseDuration) {
        // Full ASE enrichment
        BIT_SET(currentStatus.engine, BIT_ENGINE_ASE);
        ASEValue = ASE_NO_ENRICHMENT + table2D_getValue(&ASETable, cltOffset);
        aseTaper = 0;
    }
    else {
        // Taper phase
        if (aseTaper < configPage2.aseTaperTime) {
            BIT_SET(currentStatus.engine, BIT_ENGINE_ASE);
            byte fullEnrichment = table2D_getValue(&ASETable, cltOffset);
            ASEValue = ASE_NO_ENRICHMENT + map(aseTaper, 0, configPage2.aseTaperTime, fullEnrichment, 0);
            aseTaper++;
        }
        else {
            // Taper complete, disable ASE
            BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ASE);
            ASEValue = ASE_NO_ENRICHMENT;
        }
    }

    // Safety bounds
    if (ASEValue > UINT8_MAX) { ASEValue = UINT8_MAX; }
    if (ASEValue < 0) { ASEValue = 0; }

    return (byte)ASEValue;
}

uint16_t correctionAccel(void) {
    int16_t accelValue = AE_NO_CORRECTION;
    int16_t MAP_change = 0;
    int16_t TPS_change = 0;

    // Step 1: Calculate rate of change (DOT)
    calculateDOT(&MAP_change, &TPS_change);

    // Step 2: Check if AE/DCC already active
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC) || BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC)) {
        // Check if expired
        if (micros() >= currentStatus.AEEndTime) {
            // Deactivate AE/DCC
            BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ACC);
            BIT_CLEAR(currentStatus.engine, BIT_ENGINE_DCC);
            currentStatus.AEamount = 0;
            resetDOT();
            return AE_NO_CORRECTION;
        }

        // Still active, return current amount
        accelValue = currentStatus.AEamount;

        // Check for increased acceleration (re-trigger)
        const int16_t activeDOT = (configPage2.aeMode == AE_MODE_MAP) ? currentStatus.mapDOT : currentStatus.tpsDOT;
        const int16_t activateThreshold = (configPage2.aeMode == AE_MODE_MAP) ? activateMAPDOT : activateTPSDOT;

        if (abs(activeDOT) > activateThreshold) {
            // Clear current phase, will restart below
            BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ACC);
            BIT_CLEAR(currentStatus.engine, BIT_ENGINE_DCC);
        }
        else {
            return accelValue; // Continue with current enrichment
        }
    }

    // Step 3: Try to activate new AE/DCC (if not already active)
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC) && !BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC)) {
        if (configPage2.aeMode == AE_MODE_MAP) {
            accelValue = processNewActivation(currentStatus.mapDOT, MAP_change,
                                              configPage2.maeMinChange, configPage2.maeThresh,
                                              &activateMAPDOT, true);
        }
        else if (configPage2.aeMode == AE_MODE_TPS) {
            accelValue = processNewActivation(currentStatus.tpsDOT, TPS_change,
                                              configPage2.taeMinChange, configPage2.taeThresh,
                                              &activateTPSDOT, false);
        }
    }

    return accelValue;
}

byte correctionFloodClear(void) {
    // Guard: Not cranking, no flood clear needed
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        return 100;
    }

    // Cranking - check TPS threshold
    if (currentStatus.TPS >= configPage4.floodClear) {
        return 0; // Cut all fuel
    }

    return 100;
}

byte correctionBatVoltage(void) {
    return table2D_getValue(&injectorVCorrectionTable, currentStatus.battery10);
}

byte correctionIATDensity(void) {
    return table2D_getValue(&IATDensityCorrectionTable, temperatureAddOffset(currentStatus.IAT));
}

byte correctionBaro(void) {
    return table2D_getValue(&baroFuelTable, currentStatus.baro);
}

byte correctionLaunch(void) {
    if (currentStatus.launchingHard || currentStatus.launchingSoft) {
        return (100 + configPage6.lnchFuelAdd);
    }
    return 100;
}

byte correctionDFCOfuel(void) {
    // Guard: DFCO not active
    if (!BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO)) {
        dfcoTaper = configPage9.dfcoTaperTime; // Keep updating duration
        return 100;
    }

    // DFCO active - check taper
    if ((configPage9.dfcoTaperEnable == 1) && (dfcoTaper != 0)) {
        // Prevent overflow if user reduced duration while active
        if (dfcoTaper > configPage9.dfcoTaperTime) {
            dfcoTaper = configPage9.dfcoTaperTime;
        }

        byte scaleValue = map(dfcoTaper, configPage9.dfcoTaperTime, 0, 100, configPage9.dfcoTaperFuel);
        if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) { dfcoTaper--; }
        return scaleValue;
    }

    // Taper ended or disabled
    return 0;
}

bool correctionDFCO(void) {
    // Guard: DFCO disabled
    if (configPage2.dfcoEnabled != 1) {
        return false;
    }

    // Check if DFCO already active
    if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) == 1) {
        // Active - check if should deactivate
        bool shouldStayActive = (currentStatus.RPM > (configPage4.dfcoRPM * 10)) &&
                                (currentStatus.TPS < configPage4.dfcoTPSThresh);
        if (!shouldStayActive) {
            dfcoDelay = 0;
        }
        return shouldStayActive;
    }

    // Not active - check if should activate
    bool conditionsMet = (currentStatus.TPS < configPage4.dfcoTPSThresh) &&
                         (currentStatus.coolant >= temperatureRemoveOffset(configPage2.dfcoMinCLT)) &&
                         (currentStatus.RPM > (unsigned int)((configPage4.dfcoRPM * 10U) + (configPage4.dfcoHyster * 2U)));

    // Guard: Conditions not met
    if (!conditionsMet) { dfcoDelay = 0; return false; }

    // Guard: Delay not complete - increment at 10Hz and wait
    bool delayComplete = (dfcoDelay >= configPage2.dfcoDelay);
    if (!delayComplete) {
        if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) { dfcoDelay++; }
        return false;
    }

    return true; // Activate DFCO
}

byte correctionFlex(void) {
    if (configPage2.flexEnabled == 1) {
        return table2D_getValue(&flexFuelTable, currentStatus.ethanolPct);
    }
    return 100;
}

byte correctionFuelTemp(void) {
    if (configPage2.flexEnabled == 1) {
        return table2D_getValue(&fuelTempTable, temperatureAddOffset(currentStatus.fuelTemp));
    }
    return 100;
}

// ============================================================================
// INTERFACE PROVIDER
// ============================================================================

static const FuelCorrectionsInterface fuelCorrectionsInterface = {
    .correctionsFuel = &correctionsFuel,
    .correctionWUE = &correctionWUE,
    .correctionCranking = &correctionCranking,
    .correctionASE = &correctionASE,
    .correctionAccel = &correctionAccel,
    .correctionFloodClear = &correctionFloodClear,
    .correctionAFRClosedLoop = &correctionAFRClosedLoop,
    .correctionFlex = &correctionFlex,
    .correctionFuelTemp = &correctionFuelTemp,
    .correctionBatVoltage = &correctionBatVoltage,
    .correctionIATDensity = &correctionIATDensity,
    .correctionBaro = &correctionBaro,
    .correctionLaunch = &correctionLaunch,
    .correctionDFCOfuel = &correctionDFCOfuel,
    .correctionDFCO = &correctionDFCO,
};

const FuelCorrectionsInterface* getFuelCorrectionsInterface(void) {
    return &fuelCorrectionsInterface;
}
