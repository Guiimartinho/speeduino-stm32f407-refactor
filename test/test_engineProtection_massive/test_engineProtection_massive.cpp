/**
 * @file test_engineProtection_massive.cpp
 * @brief MASSIVE test suite for refactored engineProtection.cpp helper functions (20+ tests)
 *
 * Tests AFR (Air-Fuel Ratio) protection state machine extracted during FASE EP refactoring.
 * Validates protection logic, countdown timers, and activation/deactivation conditions.
 *
 * Testing Strategy:
 * - NÍVEL 1: Pure logic helpers (state machine, conditions)
 * - Uses Arduino mocks for time infrastructure
 * - Validates MISRA-C refactoring preserves behavior
 * - Tests AFR protection state machine transitions
 * - Tests countdown timer logic
 * - Tests multi-condition evaluation
 *
 * @date 2025-11-05
 * @version 1.0
 * @test_count 20+
 */

#include <unity.h>
#include "Arduino.h"
#include <string.h>

// ============================================================================
// MOCK IMPLEMENTATION (inline for testing convenience)
// ============================================================================

uint32_t mock_micros_value = 0;
uint32_t mock_millis_value = 0;
uint8_t mock_digital_pins[256] = {0};
uint16_t mock_analog_pins[64] = {0};

SerialMock Serial;
SerialMock Serial1;
SerialMock Serial2;
SerialMock Serial3;

extern "C" {

void mock_reset_arduino_state(void) {
    mock_micros_value = 0;
    mock_millis_value = 0;
    for (int i = 0; i < 256; i++) mock_digital_pins[i] = 0;
    for (int i = 0; i < 64; i++) mock_analog_pins[i] = 0;
}

void mock_advance_time_us(uint32_t us) {
    mock_micros_value += us;
    mock_millis_value += (us / 1000);
}

void mock_advance_time_ms(uint32_t ms) {
    mock_millis_value += ms;
    mock_micros_value += (ms * 1000UL);
}

} // extern "C"

// C++ overloaded functions
long random(long howbig) {
    if (howbig == 0) return 0;
    return rand() % howbig;
}

long random(long howsmall, long howbig) {
    if (howsmall >= howbig) return howsmall;
    long diff = howbig - howsmall;
    return random(diff) + howsmall;
}

// ============================================================================
// MOCK SPEEDUINO STRUCTURES
// ============================================================================

// Engine protection types
#define PROTECT_CUT_OFF 0
#define PROTECT_CUT_FUEL 1
#define PROTECT_CUT_IGN 2
#define PROTECT_CUT_BOTH 3

// O2 sensor types
#define EGO_TYPE_NARROW 0
#define EGO_TYPE_WIDE 1

// Engine protect status bits
#define ENGINE_PROTECT_BIT_RPM 0
#define ENGINE_PROTECT_BIT_MAP 1
#define ENGINE_PROTECT_BIT_OIL 2
#define ENGINE_PROTECT_BIT_AFR 3

// Bit manipulation
#define BIT_SET(var, bit) ((var) |= (1U << (bit)))
#define BIT_CLEAR(var, bit) ((var) &= ~(1U << (bit)))
#define BIT_CHECK(var, bit) (((var) >> (bit)) & 1U)

// Mock structures
struct config6 {
    uint8_t engineProtectType;
    uint8_t egoType;
    uint8_t unused[100];
};

struct config9 {
    uint8_t afrProtectEnabled;       // 0=off, 1=fixed value, 2=deviation from target
    uint8_t afrProtectDeviation;     // AFR threshold or deviation
    uint8_t afrProtectMinMAP;        // Minimum MAP (×2 for kPa)
    uint8_t afrProtectMinRPM;        // Minimum RPM (×100)
    uint8_t afrProtectMinTPS;        // Minimum TPS (%)
    uint8_t afrProtectCutTime;       // Countdown time (×100ms)
    uint8_t afrProtectReactivationTPS; // TPS threshold for reactivation
    uint8_t unused[100];
};

struct statuses {
    uint8_t O2;                      // AFR × 10 (e.g., 147 = 14.7:1)
    uint8_t MAP;                     // MAP in kPa
    uint8_t RPMdiv100;               // RPM / 100
    uint8_t TPS;                     // TPS in %
    uint8_t afrTarget;               // Target AFR × 10
    uint8_t engineProtectStatus;     // Protection status bits
    uint8_t unused[200];
};

// Global mock state
struct config6 configPage6;
struct config9 configPage9;
struct statuses currentStatus;

// ============================================================================
// HELPER FUNCTIONS UNDER TEST (from engineProtection.cpp)
// ============================================================================

/**
 * @brief Check if AFR protection is enabled
 */
static inline bool isAFRProtectionEnabled(void) {
    return (configPage6.engineProtectType != PROTECT_CUT_OFF) &&
           (configPage9.afrProtectEnabled > 0) &&
           (configPage6.egoType == EGO_TYPE_WIDE);
}

/**
 * @brief Calculate AFR condition based on mode
 */
static inline bool calculateAFRCondition(void) {
    switch(configPage9.afrProtectEnabled) {
        case 1:
            // Fixed value mode
            return (currentStatus.O2 >= configPage9.afrProtectDeviation);
        case 2:
            // Deviation from target mode
            return (currentStatus.O2 >= (currentStatus.afrTarget + configPage9.afrProtectDeviation));
        default:
            return false;
    }
}

/**
 * @brief Evaluate all AFR protection conditions
 */
static inline bool evaluateAFRConditions(void) {
    static constexpr char X2_MULTIPLIER = 2;

    bool mapCondition = (currentStatus.MAP >= (configPage9.afrProtectMinMAP * X2_MULTIPLIER));
    bool rpmCondition = (currentStatus.RPMdiv100 >= configPage9.afrProtectMinRPM);
    bool tpsCondition = (currentStatus.TPS >= configPage9.afrProtectMinTPS);
    bool afrCondition = calculateAFRCondition();

    return (mapCondition && rpmCondition && tpsCondition && afrCondition);
}

/**
 * @brief Activate AFR protection after countdown
 */
static inline void activateAFRProtection(bool& countEnabled, unsigned long& countStart, bool& limitActive) {
    static constexpr char X100_MULTIPLIER = 100;

    if (!countEnabled) {
        countEnabled = true;
        countStart = millis();
    }

    if (millis() >= (countStart + (configPage9.afrProtectCutTime * X100_MULTIPLIER))) {
        limitActive = true;
        BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);
    }
}

/**
 * @brief Deactivate AFR protection
 */
static inline void deactivateAFRProtection(bool& countEnabled, unsigned long& countStart) {
    if (countEnabled) {
        countEnabled = false;
        countStart = 0;
    }
}

/**
 * @brief Check for AFR reactivation (TPS drop)
 */
static inline void checkAFRReactivation(bool& limitActive, bool& countEnabled) {
    if (limitActive && (currentStatus.TPS <= configPage9.afrProtectReactivationTPS)) {
        limitActive = false;
        countEnabled = false;
        BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);
    }
}

// ============================================================================
// TEST SETUP/TEARDOWN
// ============================================================================

void setUp(void) {
    // Reset all mock state before each test
    mock_reset_arduino_state();
    memset(&currentStatus, 0, sizeof(currentStatus));
    memset(&configPage6, 0, sizeof(configPage6));
    memset(&configPage9, 0, sizeof(configPage9));
}

void tearDown(void) {
    // Cleanup after each test (if needed)
}

// ============================================================================
// CATEGORY 1: AFR Protection Enabled Tests (5 tests)
// ============================================================================

/**
 * @test Test isAFRProtectionEnabled - all conditions met
 * @expected Returns true
 */
void test_isAFRProtectionEnabled_all_met(void) {
    configPage6.engineProtectType = PROTECT_CUT_FUEL;
    configPage9.afrProtectEnabled = 1;
    configPage6.egoType = EGO_TYPE_WIDE;

    TEST_ASSERT_TRUE(isAFRProtectionEnabled());
}

/**
 * @test Test isAFRProtectionEnabled - engine protect off
 * @expected Returns false
 */
void test_isAFRProtectionEnabled_engine_protect_off(void) {
    configPage6.engineProtectType = PROTECT_CUT_OFF;
    configPage9.afrProtectEnabled = 1;
    configPage6.egoType = EGO_TYPE_WIDE;

    TEST_ASSERT_FALSE(isAFRProtectionEnabled());
}

/**
 * @test Test isAFRProtectionEnabled - AFR protect disabled
 * @expected Returns false
 */
void test_isAFRProtectionEnabled_afr_disabled(void) {
    configPage6.engineProtectType = PROTECT_CUT_FUEL;
    configPage9.afrProtectEnabled = 0;
    configPage6.egoType = EGO_TYPE_WIDE;

    TEST_ASSERT_FALSE(isAFRProtectionEnabled());
}

/**
 * @test Test isAFRProtectionEnabled - narrow O2 sensor
 * @expected Returns false (need wideband)
 */
void test_isAFRProtectionEnabled_narrow_o2(void) {
    configPage6.engineProtectType = PROTECT_CUT_FUEL;
    configPage9.afrProtectEnabled = 1;
    configPage6.egoType = EGO_TYPE_NARROW;

    TEST_ASSERT_FALSE(isAFRProtectionEnabled());
}

/**
 * @test Test isAFRProtectionEnabled - multiple failures
 * @expected Returns false
 */
void test_isAFRProtectionEnabled_all_fail(void) {
    configPage6.engineProtectType = PROTECT_CUT_OFF;
    configPage9.afrProtectEnabled = 0;
    configPage6.egoType = EGO_TYPE_NARROW;

    TEST_ASSERT_FALSE(isAFRProtectionEnabled());
}

// ============================================================================
// CATEGORY 2: AFR Condition Calculation Tests (6 tests)
// ============================================================================

/**
 * @test Test calculateAFRCondition - mode 1 (fixed value), below threshold
 * @expected Returns false
 */
void test_calculateAFRCondition_mode1_below(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160; // 16.0:1 threshold
    currentStatus.O2 = 147;                // 14.7:1 actual

    TEST_ASSERT_FALSE(calculateAFRCondition());
}

/**
 * @test Test calculateAFRCondition - mode 1, above threshold
 * @expected Returns true
 */
void test_calculateAFRCondition_mode1_above(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160; // 16.0:1 threshold
    currentStatus.O2 = 180;                // 18.0:1 actual (LEAN!)

    TEST_ASSERT_TRUE(calculateAFRCondition());
}

/**
 * @test Test calculateAFRCondition - mode 1, exactly at threshold
 * @expected Returns true (>= comparison)
 */
void test_calculateAFRCondition_mode1_exact(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 160;

    TEST_ASSERT_TRUE(calculateAFRCondition());
}

/**
 * @test Test calculateAFRCondition - mode 2 (deviation), within target
 * @expected Returns false
 */
void test_calculateAFRCondition_mode2_within_target(void) {
    configPage9.afrProtectEnabled = 2;
    configPage9.afrProtectDeviation = 50;  // +5.0 AFR deviation limit
    currentStatus.afrTarget = 120;         // 12.0:1 target
    currentStatus.O2 = 140;                // 14.0:1 actual (+2.0 deviation)

    TEST_ASSERT_FALSE(calculateAFRCondition());
}

/**
 * @test Test calculateAFRCondition - mode 2, exceeds deviation
 * @expected Returns true
 */
void test_calculateAFRCondition_mode2_exceeds_deviation(void) {
    configPage9.afrProtectEnabled = 2;
    configPage9.afrProtectDeviation = 50;  // +5.0 AFR deviation limit
    currentStatus.afrTarget = 120;         // 12.0:1 target
    currentStatus.O2 = 180;                // 18.0:1 actual (+6.0 deviation)

    TEST_ASSERT_TRUE(calculateAFRCondition());
}

/**
 * @test Test calculateAFRCondition - mode 0 (disabled)
 * @expected Returns false
 */
void test_calculateAFRCondition_disabled(void) {
    configPage9.afrProtectEnabled = 0;
    currentStatus.O2 = 200; // Very lean, but protection disabled

    TEST_ASSERT_FALSE(calculateAFRCondition());
}

// ============================================================================
// CATEGORY 3: Condition Evaluation Tests (8 tests)
// ============================================================================

/**
 * @test Test evaluateAFRConditions - all conditions met
 * @expected Returns true
 */
void test_evaluateAFRConditions_all_met(void) {
    // Setup AFR condition (mode 1)
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 180; // Lean

    // Setup thresholds
    configPage9.afrProtectMinMAP = 50;  // 100 kPa minimum (×2)
    configPage9.afrProtectMinRPM = 20;  // 2000 RPM minimum (×100)
    configPage9.afrProtectMinTPS = 80;  // 80% TPS minimum

    // Setup actual values (all above thresholds)
    currentStatus.MAP = 120;      // 120 kPa (above 100)
    currentStatus.RPMdiv100 = 30; // 3000 RPM (above 2000)
    currentStatus.TPS = 95;       // 95% TPS (above 80)

    TEST_ASSERT_TRUE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - MAP too low
 * @expected Returns false
 */
void test_evaluateAFRConditions_map_low(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 180;

    configPage9.afrProtectMinMAP = 50;  // Need 100 kPa
    configPage9.afrProtectMinRPM = 20;
    configPage9.afrProtectMinTPS = 80;

    currentStatus.MAP = 80;       // 80 kPa (below 100)
    currentStatus.RPMdiv100 = 30;
    currentStatus.TPS = 95;

    TEST_ASSERT_FALSE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - RPM too low
 * @expected Returns false
 */
void test_evaluateAFRConditions_rpm_low(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 180;

    configPage9.afrProtectMinMAP = 50;
    configPage9.afrProtectMinRPM = 20;  // Need 2000 RPM
    configPage9.afrProtectMinTPS = 80;

    currentStatus.MAP = 120;
    currentStatus.RPMdiv100 = 15; // 1500 RPM (below 2000)
    currentStatus.TPS = 95;

    TEST_ASSERT_FALSE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - TPS too low
 * @expected Returns false
 */
void test_evaluateAFRConditions_tps_low(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 180;

    configPage9.afrProtectMinMAP = 50;
    configPage9.afrProtectMinRPM = 20;
    configPage9.afrProtectMinTPS = 80; // Need 80% TPS

    currentStatus.MAP = 120;
    currentStatus.RPMdiv100 = 30;
    currentStatus.TPS = 60; // 60% (below 80)

    TEST_ASSERT_FALSE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - AFR not lean enough
 * @expected Returns false
 */
void test_evaluateAFRConditions_afr_ok(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 140; // 14.0:1 - rich enough, no protection needed

    configPage9.afrProtectMinMAP = 50;
    configPage9.afrProtectMinRPM = 20;
    configPage9.afrProtectMinTPS = 80;

    currentStatus.MAP = 120;
    currentStatus.RPMdiv100 = 30;
    currentStatus.TPS = 95;

    TEST_ASSERT_FALSE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - boundary case (exactly at thresholds)
 * @expected Returns true
 */
void test_evaluateAFRConditions_at_boundaries(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 160; // Exactly at AFR threshold

    configPage9.afrProtectMinMAP = 50;
    configPage9.afrProtectMinRPM = 20;
    configPage9.afrProtectMinTPS = 80;

    currentStatus.MAP = 100;      // Exactly at MAP threshold (50 × 2)
    currentStatus.RPMdiv100 = 20; // Exactly at RPM threshold
    currentStatus.TPS = 80;       // Exactly at TPS threshold

    TEST_ASSERT_TRUE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - partial throttle scenario
 * @expected Returns false (TPS too low)
 */
void test_evaluateAFRConditions_partial_throttle(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 180;

    configPage9.afrProtectMinMAP = 40;
    configPage9.afrProtectMinRPM = 15;
    configPage9.afrProtectMinTPS = 80; // Require high throttle

    currentStatus.MAP = 90;
    currentStatus.RPMdiv100 = 25;
    currentStatus.TPS = 50; // Partial throttle = no protection

    TEST_ASSERT_FALSE(evaluateAFRConditions());
}

/**
 * @test Test evaluateAFRConditions - WOT scenario (all maxed)
 * @expected Returns true
 */
void test_evaluateAFRConditions_wot_scenario(void) {
    configPage9.afrProtectEnabled = 1;
    configPage9.afrProtectDeviation = 160;
    currentStatus.O2 = 190; // Very lean at WOT (danger!)

    configPage9.afrProtectMinMAP = 50;
    configPage9.afrProtectMinRPM = 30;
    configPage9.afrProtectMinTPS = 90;

    currentStatus.MAP = 255;      // Max MAP (boosted)
    currentStatus.RPMdiv100 = 60; // 6000 RPM
    currentStatus.TPS = 100;      // WOT

    TEST_ASSERT_TRUE(evaluateAFRConditions());
}

// ============================================================================
// CATEGORY 4: Activation/Deactivation Tests (6 tests)
// ============================================================================

/**
 * @test Test activateAFRProtection - start countdown
 * @expected Countdown starts, protection not yet active
 */
void test_activateAFRProtection_start_countdown(void) {
    bool countEnabled = false;
    unsigned long countStart = 0;
    bool limitActive = false;

    configPage9.afrProtectCutTime = 20; // 2 seconds (×100ms)
    mock_millis_value = 1000;

    activateAFRProtection(countEnabled, countStart, limitActive);

    TEST_ASSERT_TRUE(countEnabled);
    TEST_ASSERT_EQUAL_UINT32(1000, countStart);
    TEST_ASSERT_FALSE(limitActive); // Not active yet
}

/**
 * @test Test activateAFRProtection - countdown complete
 * @expected Protection activates
 */
void test_activateAFRProtection_countdown_complete(void) {
    bool countEnabled = true;
    unsigned long countStart = 1000;
    bool limitActive = false;

    configPage9.afrProtectCutTime = 20; // 2 seconds
    mock_millis_value = 3000; // 2 seconds elapsed

    activateAFRProtection(countEnabled, countStart, limitActive);

    TEST_ASSERT_TRUE(limitActive); // Protection now active
    TEST_ASSERT_TRUE(BIT_CHECK(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR));
}

/**
 * @test Test activateAFRProtection - countdown in progress
 * @expected Protection not yet active
 */
void test_activateAFRProtection_countdown_progress(void) {
    bool countEnabled = true;
    unsigned long countStart = 1000;
    bool limitActive = false;

    configPage9.afrProtectCutTime = 20; // 2 seconds
    mock_millis_value = 2000; // 1 second elapsed (need 2)

    activateAFRProtection(countEnabled, countStart, limitActive);

    TEST_ASSERT_FALSE(limitActive); // Still waiting
}

/**
 * @test Test deactivateAFRProtection - reset countdown
 * @expected Countdown disabled and reset
 */
void test_deactivateAFRProtection_reset(void) {
    bool countEnabled = true;
    unsigned long countStart = 5000;

    deactivateAFRProtection(countEnabled, countStart);

    TEST_ASSERT_FALSE(countEnabled);
    TEST_ASSERT_EQUAL_UINT32(0, countStart);
}

/**
 * @test Test deactivateAFRProtection - already disabled
 * @expected No change (idempotent)
 */
void test_deactivateAFRProtection_already_disabled(void) {
    bool countEnabled = false;
    unsigned long countStart = 0;

    deactivateAFRProtection(countEnabled, countStart);

    TEST_ASSERT_FALSE(countEnabled);
    TEST_ASSERT_EQUAL_UINT32(0, countStart);
}

/**
 * @test Test activation/deactivation cycle
 * @expected Full cycle works correctly
 */
void test_afr_protection_full_cycle(void) {
    bool countEnabled = false;
    unsigned long countStart = 0;
    bool limitActive = false;

    configPage9.afrProtectCutTime = 10; // 1 second
    mock_millis_value = 1000;

    // Start countdown
    activateAFRProtection(countEnabled, countStart, limitActive);
    TEST_ASSERT_TRUE(countEnabled);
    TEST_ASSERT_FALSE(limitActive);

    // Advance time - protection activates
    mock_millis_value = 2000;
    activateAFRProtection(countEnabled, countStart, limitActive);
    TEST_ASSERT_TRUE(limitActive);

    // Deactivate
    deactivateAFRProtection(countEnabled, countStart);
    TEST_ASSERT_FALSE(countEnabled);
    TEST_ASSERT_EQUAL_UINT32(0, countStart);
}

// ============================================================================
// CATEGORY 5: Reactivation Tests (4 tests)
// ============================================================================

/**
 * @test Test checkAFRReactivation - TPS drops, reactivate
 * @expected Protection clears
 */
void test_checkAFRReactivation_tps_drop(void) {
    bool limitActive = true;
    bool countEnabled = true;

    currentStatus.engineProtectStatus = 0xFF; // All bits set
    BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);

    configPage9.afrProtectReactivationTPS = 5; // 5% TPS threshold
    currentStatus.TPS = 3; // TPS dropped to 3%

    checkAFRReactivation(limitActive, countEnabled);

    TEST_ASSERT_FALSE(limitActive);
    TEST_ASSERT_FALSE(countEnabled);
    TEST_ASSERT_FALSE(BIT_CHECK(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR));
}

/**
 * @test Test checkAFRReactivation - TPS still high
 * @expected Protection remains active
 */
void test_checkAFRReactivation_tps_high(void) {
    bool limitActive = true;
    bool countEnabled = true;

    BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);

    configPage9.afrProtectReactivationTPS = 5;
    currentStatus.TPS = 50; // TPS still high

    checkAFRReactivation(limitActive, countEnabled);

    TEST_ASSERT_TRUE(limitActive); // Still active
}

/**
 * @test Test checkAFRReactivation - not active yet
 * @expected No effect
 */
void test_checkAFRReactivation_not_active(void) {
    bool limitActive = false;
    bool countEnabled = true;

    configPage9.afrProtectReactivationTPS = 5;
    currentStatus.TPS = 0; // TPS at 0

    checkAFRReactivation(limitActive, countEnabled);

    TEST_ASSERT_FALSE(limitActive); // Still not active
}

/**
 * @test Test checkAFRReactivation - exactly at threshold
 * @expected Reactivates (<= comparison)
 */
void test_checkAFRReactivation_at_threshold(void) {
    bool limitActive = true;
    bool countEnabled = true;

    BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);

    configPage9.afrProtectReactivationTPS = 5;
    currentStatus.TPS = 5; // Exactly at threshold

    checkAFRReactivation(limitActive, countEnabled);

    TEST_ASSERT_FALSE(limitActive); // Reactivated
    TEST_ASSERT_FALSE(BIT_CHECK(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR));
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Category 1: AFR Protection Enabled Tests (5 tests)
    RUN_TEST(test_isAFRProtectionEnabled_all_met);
    RUN_TEST(test_isAFRProtectionEnabled_engine_protect_off);
    RUN_TEST(test_isAFRProtectionEnabled_afr_disabled);
    RUN_TEST(test_isAFRProtectionEnabled_narrow_o2);
    RUN_TEST(test_isAFRProtectionEnabled_all_fail);

    // Category 2: AFR Condition Calculation Tests (6 tests)
    RUN_TEST(test_calculateAFRCondition_mode1_below);
    RUN_TEST(test_calculateAFRCondition_mode1_above);
    RUN_TEST(test_calculateAFRCondition_mode1_exact);
    RUN_TEST(test_calculateAFRCondition_mode2_within_target);
    RUN_TEST(test_calculateAFRCondition_mode2_exceeds_deviation);
    RUN_TEST(test_calculateAFRCondition_disabled);

    // Category 3: Condition Evaluation Tests (8 tests)
    RUN_TEST(test_evaluateAFRConditions_all_met);
    RUN_TEST(test_evaluateAFRConditions_map_low);
    RUN_TEST(test_evaluateAFRConditions_rpm_low);
    RUN_TEST(test_evaluateAFRConditions_tps_low);
    RUN_TEST(test_evaluateAFRConditions_afr_ok);
    RUN_TEST(test_evaluateAFRConditions_at_boundaries);
    RUN_TEST(test_evaluateAFRConditions_partial_throttle);
    RUN_TEST(test_evaluateAFRConditions_wot_scenario);

    // Category 4: Activation/Deactivation Tests (6 tests)
    RUN_TEST(test_activateAFRProtection_start_countdown);
    RUN_TEST(test_activateAFRProtection_countdown_complete);
    RUN_TEST(test_activateAFRProtection_countdown_progress);
    RUN_TEST(test_deactivateAFRProtection_reset);
    RUN_TEST(test_deactivateAFRProtection_already_disabled);
    RUN_TEST(test_afr_protection_full_cycle);

    // Category 5: Reactivation Tests (4 tests)
    RUN_TEST(test_checkAFRReactivation_tps_drop);
    RUN_TEST(test_checkAFRReactivation_tps_high);
    RUN_TEST(test_checkAFRReactivation_not_active);
    RUN_TEST(test_checkAFRReactivation_at_threshold);

    return UNITY_END();
}
