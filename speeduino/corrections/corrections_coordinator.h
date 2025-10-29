/**
 * @file corrections_coordinator.h
 * @brief Corrections coordinator - main API for corrections management
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module orchestrates all correction calculations for fuel, ignition, and dwell.
 * Replaces direct function calls with modular architecture.
 *
 * Original API preserved:
 * - Maintains compatibility with existing code
 * - Same function names and signatures
 * - 100% behavioral equivalence
 *
 * @note This is the ONLY corrections module that should be included by speeduino.ino
 */

#ifndef CORRECTIONS_COORDINATOR_H
#define CORRECTIONS_COORDINATOR_H

#include "../globals.h"

/**
 * @brief Initialize corrections system
 * @details Sets up all correction subsystems (fuel, ignition, dwell, AFR)
 * @post Corrections ready for operation
 * @note Called once during ECU initialization
 * @note Thread-safe: Called only from init sequence
 */
void correctionsCoordinatorInitialize(void);

// ============================================================================
// FUEL CORRECTIONS API
// ============================================================================

/**
 * @brief Main fuel corrections pipeline
 * @return Corrected fuel value (percentage, 0-65535 representing 0-655.35%)
 * @note Called every injection cycle
 * @note Applies: WUE, ASE, cranking, accel, environmental, battery, special, DFCO
 */
uint16_t correctionsCoordinatorFuel(void);

/**
 * @brief Cranking enrichment correction
 * @return Cranking enrichment value (percentage)
 * @note Applied only during cranking
 */
uint16_t correctionsCoordinatorCranking(void);

/**
 * @brief Acceleration enrichment correction
 * @return Acceleration enrichment value (percentage)
 * @note CRITICAL: Complex state machine with 195 lines
 * @note Handles TPS-based and MAP-based AE
 */
uint16_t correctionsCoordinatorAccel(void);

/**
 * @brief Deceleration Fuel Cut-Off (DFCO) check
 * @return true if fuel should be cut (DFCO active), false otherwise
 * @note Safety-critical: Prevents overrun and improves economy
 */
bool correctionsCoordinatorDFCO(void);

// ============================================================================
// IGNITION CORRECTIONS API
// ============================================================================

/**
 * @brief Main ignition corrections pipeline
 * @param base_advance Base ignition advance in degrees
 * @return Corrected ignition advance in degrees
 * @note Called every ignition cycle
 * @note Applies: environmental, timing, protection, special corrections
 */
int8_t correctionsCoordinatorIgn(int8_t base_advance);

/**
 * @brief Fixed timing correction (overrides all other corrections)
 * @param advance Current advance value
 * @return Fixed timing value if enabled, otherwise unchanged
 */
int8_t correctionsCoordinatorFixedTiming(int8_t advance);

/**
 * @brief Cranking fixed timing correction
 * @param advance Current advance value
 * @return Cranking timing if cranking, otherwise unchanged
 */
int8_t correctionsCoordinatorCrankingFixedTiming(int8_t advance);

/**
 * @brief Flex fuel timing correction
 * @param advance Current advance value
 * @return Advance adjusted for ethanol content
 */
int8_t correctionsCoordinatorFlexTiming(int8_t advance);

/**
 * @brief Water/Methanol injection timing correction
 * @param advance Current advance value
 * @return Advance adjusted for WMI
 */
int8_t correctionsCoordinatorWMITiming(int8_t advance);

/**
 * @brief IAT retard correction
 * @param advance Current advance value
 * @return Advance reduced for high inlet temps
 */
int8_t correctionsCoordinatorIATretard(int8_t advance);

/**
 * @brief CLT advance correction
 * @param advance Current advance value
 * @return Advance adjusted for coolant temp
 */
int8_t correctionsCoordinatorCLTadvance(int8_t advance);

/**
 * @brief Idle advance correction
 * @param advance Current advance value
 * @return Advance adjusted for idle control
 */
int8_t correctionsCoordinatorIdleAdvance(int8_t advance);

/**
 * @brief Soft rev limit timing retard
 * @param advance Current advance value
 * @return Advance reduced for rev limiter
 */
int8_t correctionsCoordinatorSoftRevLimit(int8_t advance);

/**
 * @brief Nitrous timing retard
 * @param advance Current advance value
 * @return Advance reduced for nitrous safety
 */
int8_t correctionsCoordinatorNitrous(int8_t advance);

/**
 * @brief Soft launch timing retard
 * @param advance Current advance value
 * @return Advance reduced for launch control
 */
int8_t correctionsCoordinatorSoftLaunch(int8_t advance);

/**
 * @brief Soft flat shift timing retard
 * @param advance Current advance value
 * @return Advance reduced for flat shift
 */
int8_t correctionsCoordinatorSoftFlatShift(int8_t advance);

/**
 * @brief Knock timing correction
 * @param advance Current advance value
 * @return Advance reduced for knock protection
 * @note Safety-critical: Prevents engine damage
 */
int8_t correctionsCoordinatorKnockTiming(int8_t advance);

/**
 * @brief DFCO ignition correction
 * @param advance Current advance value
 * @return Advance during DFCO condition
 */
int8_t correctionsCoordinatorDFCOignition(int8_t advance);

// ============================================================================
// DWELL CORRECTIONS API
// ============================================================================

/**
 * @brief Dwell time corrections
 * @param dwell Base dwell time in milliseconds
 * @return Corrected dwell time
 * @note Adjusts for battery voltage
 */
uint16_t correctionsCoordinatorDwell(uint16_t dwell);

// ============================================================================
// AFR CORRECTIONS API
// ============================================================================

/**
 * @brief Calculate target AFR
 * @param afrLookUpTable AFR table to use
 * @param current Current engine status
 * @param page2 Configuration page 2
 * @param page6 Configuration page 6
 * @return Target AFR value
 * @note Used for closed-loop control
 */
uint8_t correctionsCoordinatorCalculateAfrTarget(
    table3d16RpmLoad &afrLookUpTable,
    const statuses &current,
    const config2 &page2,
    const config6 &page6
);

/**
 * @brief Check if corrections coordinator is initialized
 * @return true if initialized, false otherwise
 */
bool correctionsCoordinatorIsInitialized(void);

/**
 * @brief Get corrections coordinator name (for debugging)
 * @return "Corrections Coordinator"
 */
const char* correctionsCoordinatorGetName(void);

#endif // CORRECTIONS_COORDINATOR_H
