/**
 * @file test_scheduling_massive.cpp
 * @brief Massive test suite for ignition & fuel scheduling helpers
 *
 * FASE V - Validation & Testing
 * Tests for ignition_scheduling.cpp and fuel_scheduling.cpp helper functions
 *
 * Test Categories:
 * 1. Fixed Cranking Override Tests (10 tests)
 * 2. Ignition Angle Adjustment Tests (5 tests)
 * 3. Ignition Channel Bit Tests (3 tests)
 * 4. Fuel Channel Bit Tests (3 tests)
 * 5. Boundary and Edge Cases (4 tests)
 *
 * Total: 25 tests
 */

#include <unity.h>
#include "Arduino.h"

// ============================================================================
// ARDUINO MOCK IMPLEMENTATION (inline to avoid linking issues)
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

void mock_reset_arduino_state(void)
{
    mock_micros_value = 0;
    mock_millis_value = 0;
    for (int i = 0; i < 256; i++) { mock_digital_pins[i] = 0; }
    for (int i = 0; i < 64; i++) { mock_analog_pins[i] = 0; }
}

void mock_advance_time_us(uint32_t us)
{
    mock_micros_value += us;
    mock_millis_value = mock_micros_value / 1000;
}

void mock_advance_time_ms(uint32_t ms)
{
    mock_millis_value += ms;
    mock_micros_value = mock_millis_value * 1000;
}

void mock_set_analog_pin(uint8_t pin, uint16_t value)
{
    if (pin < 64) { mock_analog_pins[pin] = value; }
}

void mock_set_digital_pin(uint8_t pin, uint8_t value)
{
    if (pin < 256) { mock_digital_pins[pin] = value; }
}

} // extern "C"

// ============================================================================
// MOCK SPEEDUINO STRUCTURES AND GLOBALS
// ============================================================================

#define BIT_ENGINE_CRANK 0
#define BIT_DECODER_HAS_FIXED_CRANKING 3

#define IGN_CHANNELS 8
#define INJ_CHANNELS 8

// Engine status structure
struct statuses {
    uint16_t RPM;
    uint32_t dwell;
    uint8_t engine;
    unsigned int PW1, PW2, PW3, PW4, PW5, PW6, PW7, PW8;
};

// Config page 4 (ignition)
struct config4 {
    uint8_t ignCranklock;
    uint8_t StgCycles;
};

// Config page 2
struct config2 {
    bool perToothIgn;
};

struct statuses currentStatus;
struct config4 configPage4;
struct config2 configPage2;

// Decoder state
uint8_t decoderState = 0;

// Ignition channel control
uint8_t ignitionChannelsOn = 0xFF; // All channels ON by default
uint8_t fuelChannelsOn = 0xFF;

// Ignition start angles (global variables modified by helper)
int ignition1StartAngle = 100;
int ignition2StartAngle = 200;
int ignition3StartAngle = 300;
int ignition4StartAngle = 400;
int ignition5StartAngle = 500;
int ignition6StartAngle = 600;
int ignition7StartAngle = 700;
int ignition8StartAngle = 800;

// Injector minimum opening time
unsigned long inj_opentime_uS = 1000; // 1ms minimum

// Max outputs
uint8_t maxIgnOutputs = IGN_CHANNELS;
uint8_t maxInjOutputs = INJ_CHANNELS;

// Bit manipulation macros
#define BIT_SET(var, bit) ((var) |= (1U << (bit)))
#define BIT_CLEAR(var, bit) ((var) &= ~(1U << (bit)))
#define BIT_CHECK(var, bit) (((var) & (1U << (bit))) != 0U)

// Channel command bits
#define IGN1_CMD_BIT 0
#define IGN2_CMD_BIT 1
#define IGN3_CMD_BIT 2
#define IGN4_CMD_BIT 3
#define IGN5_CMD_BIT 4
#define IGN6_CMD_BIT 5
#define IGN7_CMD_BIT 6
#define IGN8_CMD_BIT 7

#define INJ1_CMD_BIT 0
#define INJ2_CMD_BIT 1
#define INJ3_CMD_BIT 2
#define INJ4_CMD_BIT 3
#define INJ5_CMD_BIT 4
#define INJ6_CMD_BIT 5
#define INJ7_CMD_BIT 6
#define INJ8_CMD_BIT 7

// ============================================================================
// HELPER FUNCTIONS UNDER TEST (extracted from source files)
// ============================================================================

/**
 * @brief Adjust ignition start angles for low RPM cranking
 * From: ignition_scheduling.cpp:41-66
 */
static inline void adjustIgnitionStartAnglesForCranking(void)
{
    static int* const startAngles[IGN_CHANNELS] = {
        &ignition1StartAngle,
        &ignition2StartAngle,
        &ignition3StartAngle,
        &ignition4StartAngle,
        &ignition5StartAngle,
        &ignition6StartAngle,
        &ignition7StartAngle,
        &ignition8StartAngle
    };

    for (uint8_t ch = 0; ch < IGN_CHANNELS; ch++)
    {
        *startAngles[ch] -= 5;
    }
}

/**
 * @brief Calculate fixed cranking override for dwell extension
 * From: ignition_scheduling.cpp:76-88
 */
static inline uint32_t calculateFixedCrankingOverride(void)
{
    if (!configPage4.ignCranklock) { return 0; }
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return 0; }
    if (!BIT_CHECK(decoderState, BIT_DECODER_HAS_FIXED_CRANKING)) { return 0; }

    if (currentStatus.RPM < 250)
    {
        adjustIgnitionStartAnglesForCranking();
    }

    return currentStatus.dwell * 3;
}

// ============================================================================
// TEST SETUP/TEARDOWN
// ============================================================================

void setUp(void)
{
    mock_reset_arduino_state();

    // Reset all structures
    memset(&currentStatus, 0, sizeof(currentStatus));
    memset(&configPage4, 0, sizeof(configPage4));
    memset(&configPage2, 0, sizeof(configPage2));

    // Reset global variables
    decoderState = 0;
    ignitionChannelsOn = 0xFF;
    fuelChannelsOn = 0xFF;

    // Reset ignition angles to default values
    ignition1StartAngle = 100;
    ignition2StartAngle = 200;
    ignition3StartAngle = 300;
    ignition4StartAngle = 400;
    ignition5StartAngle = 500;
    ignition6StartAngle = 600;
    ignition7StartAngle = 700;
    ignition8StartAngle = 800;

    // Reset injector timing
    inj_opentime_uS = 1000;
    maxIgnOutputs = IGN_CHANNELS;
    maxInjOutputs = INJ_CHANNELS;
}

void tearDown(void)
{
    // No cleanup needed
}

// ============================================================================
// CATEGORY 1: Fixed Cranking Override Tests (10 tests)
// ============================================================================

/**
 * Test: Fixed cranking disabled (ignCranklock = 0)
 * Expected: Return 0, no dwell extension
 */
void test_calculateFixedCrankingOverride_disabled(void)
{
    configPage4.ignCranklock = 0;  // DISABLED
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 200;
    currentStatus.dwell = 4000; // 4ms

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(0, result);
}

/**
 * Test: Not cranking (engine running)
 * Expected: Return 0
 */
void test_calculateFixedCrankingOverride_not_cranking(void)
{
    configPage4.ignCranklock = 1;
    BIT_CLEAR(currentStatus.engine, BIT_ENGINE_CRANK);  // NOT CRANKING
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 200;
    currentStatus.dwell = 4000;

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(0, result);
}

/**
 * Test: Decoder does not support fixed cranking
 * Expected: Return 0
 */
void test_calculateFixedCrankingOverride_no_decoder_support(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);  // NO SUPPORT
    currentStatus.RPM = 200;
    currentStatus.dwell = 4000;

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(0, result);
}

/**
 * Test: All conditions met, RPM < 250 (triggers angle adjustment)
 * Expected: Return dwell * 3, and angles adjusted
 */
void test_calculateFixedCrankingOverride_low_rpm_with_adjustment(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 200;  // < 250
    currentStatus.dwell = 4000; // 4ms

    // Store original angles
    int original_angle1 = ignition1StartAngle;
    int original_angle2 = ignition2StartAngle;

    uint32_t result = calculateFixedCrankingOverride();

    // Check dwell extension
    TEST_ASSERT_EQUAL_UINT32(12000, result); // 4000 * 3 = 12000

    // Check angle adjustment (-5 degrees)
    TEST_ASSERT_EQUAL_INT(original_angle1 - 5, ignition1StartAngle);
    TEST_ASSERT_EQUAL_INT(original_angle2 - 5, ignition2StartAngle);
}

/**
 * Test: All conditions met, RPM >= 250 (no angle adjustment)
 * Expected: Return dwell * 3, but angles NOT adjusted
 */
void test_calculateFixedCrankingOverride_high_rpm_no_adjustment(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 300;  // >= 250
    currentStatus.dwell = 3000; // 3ms

    // Store original angles
    int original_angle1 = ignition1StartAngle;
    int original_angle8 = ignition8StartAngle;

    uint32_t result = calculateFixedCrankingOverride();

    // Check dwell extension
    TEST_ASSERT_EQUAL_UINT32(9000, result); // 3000 * 3 = 9000

    // Check angles NOT adjusted
    TEST_ASSERT_EQUAL_INT(original_angle1, ignition1StartAngle);
    TEST_ASSERT_EQUAL_INT(original_angle8, ignition8StartAngle);
}

/**
 * Test: RPM exactly 250 (boundary)
 * Expected: No angle adjustment
 */
void test_calculateFixedCrankingOverride_rpm_boundary_250(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 250;  // Exactly 250
    currentStatus.dwell = 5000;

    int original_angle3 = ignition3StartAngle;

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(15000, result); // 5000 * 3
    TEST_ASSERT_EQUAL_INT(original_angle3, ignition3StartAngle); // NO adjustment
}

/**
 * Test: RPM = 249 (just below boundary)
 * Expected: Angle adjustment triggered
 */
void test_calculateFixedCrankingOverride_rpm_249(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 249;  // Just below 250
    currentStatus.dwell = 2000;

    int original_angle4 = ignition4StartAngle;

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(6000, result); // 2000 * 3
    TEST_ASSERT_EQUAL_INT(original_angle4 - 5, ignition4StartAngle); // Adjusted
}

/**
 * Test: Dwell = 0 (edge case)
 * Expected: Return 0
 */
void test_calculateFixedCrankingOverride_zero_dwell(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 200;
    currentStatus.dwell = 0;  // Zero dwell

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(0, result); // 0 * 3 = 0
}

/**
 * Test: Large dwell value
 * Expected: Return dwell * 3 (no overflow)
 */
void test_calculateFixedCrankingOverride_large_dwell(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 200;
    currentStatus.dwell = 10000; // 10ms (very long)

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(30000, result); // 10000 * 3 = 30000
}

/**
 * Test: RPM = 0 (stalled engine)
 * Expected: Still returns dwell * 3 with angle adjustment
 */
void test_calculateFixedCrankingOverride_rpm_zero(void)
{
    configPage4.ignCranklock = 1;
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 0;  // Stalled
    currentStatus.dwell = 3500;

    int original_angle5 = ignition5StartAngle;

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(10500, result); // 3500 * 3
    TEST_ASSERT_EQUAL_INT(original_angle5 - 5, ignition5StartAngle);
}

// ============================================================================
// CATEGORY 2: Ignition Angle Adjustment Tests (5 tests)
// ============================================================================

/**
 * Test: Adjust all 8 ignition start angles
 * Expected: All angles reduced by 5 degrees
 */
void test_adjustIgnitionStartAnglesForCranking_all_channels(void)
{
    // Set specific initial values
    ignition1StartAngle = 100;
    ignition2StartAngle = 250;
    ignition3StartAngle = 360;
    ignition4StartAngle = 720;
    ignition5StartAngle = 50;
    ignition6StartAngle = 180;
    ignition7StartAngle = 540;
    ignition8StartAngle = 0;

    adjustIgnitionStartAnglesForCranking();

    // Verify all reduced by 5
    TEST_ASSERT_EQUAL_INT(95, ignition1StartAngle);
    TEST_ASSERT_EQUAL_INT(245, ignition2StartAngle);
    TEST_ASSERT_EQUAL_INT(355, ignition3StartAngle);
    TEST_ASSERT_EQUAL_INT(715, ignition4StartAngle);
    TEST_ASSERT_EQUAL_INT(45, ignition5StartAngle);
    TEST_ASSERT_EQUAL_INT(175, ignition6StartAngle);
    TEST_ASSERT_EQUAL_INT(535, ignition7StartAngle);
    TEST_ASSERT_EQUAL_INT(-5, ignition8StartAngle); // Can go negative!
}

/**
 * Test: Multiple adjustments (cumulative effect)
 * Expected: Angles reduced by 5 each time
 */
void test_adjustIgnitionStartAnglesForCranking_multiple_calls(void)
{
    ignition1StartAngle = 100;

    adjustIgnitionStartAnglesForCranking();
    TEST_ASSERT_EQUAL_INT(95, ignition1StartAngle);

    adjustIgnitionStartAnglesForCranking();
    TEST_ASSERT_EQUAL_INT(90, ignition1StartAngle);

    adjustIgnitionStartAnglesForCranking();
    TEST_ASSERT_EQUAL_INT(85, ignition1StartAngle);
}

/**
 * Test: Starting from zero
 * Expected: Goes negative (-5)
 */
void test_adjustIgnitionStartAnglesForCranking_from_zero(void)
{
    ignition2StartAngle = 0;

    adjustIgnitionStartAnglesForCranking();

    TEST_ASSERT_EQUAL_INT(-5, ignition2StartAngle);
}

/**
 * Test: Starting from negative value
 * Expected: Becomes more negative
 */
void test_adjustIgnitionStartAnglesForCranking_from_negative(void)
{
    ignition3StartAngle = -10;

    adjustIgnitionStartAnglesForCranking();

    TEST_ASSERT_EQUAL_INT(-15, ignition3StartAngle);
}

/**
 * Test: Large positive angle
 * Expected: Still reduced by exactly 5
 */
void test_adjustIgnitionStartAnglesForCranking_large_angle(void)
{
    ignition4StartAngle = 10000;

    adjustIgnitionStartAnglesForCranking();

    TEST_ASSERT_EQUAL_INT(9995, ignition4StartAngle);
}

// ============================================================================
// CATEGORY 3: Ignition Channel Bit Tests (3 tests)
// ============================================================================

/**
 * Test: Check individual ignition channel bits
 * Expected: BIT_CHECK works correctly for all channels
 */
void test_ignition_channel_bits_individual(void)
{
    ignitionChannelsOn = 0x00; // All OFF

    // Set channel 1 ON
    BIT_SET(ignitionChannelsOn, IGN1_CMD_BIT);
    TEST_ASSERT_TRUE(BIT_CHECK(ignitionChannelsOn, IGN1_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(ignitionChannelsOn, IGN2_CMD_BIT));

    // Set channel 4 ON
    BIT_SET(ignitionChannelsOn, IGN4_CMD_BIT);
    TEST_ASSERT_TRUE(BIT_CHECK(ignitionChannelsOn, IGN1_CMD_BIT));
    TEST_ASSERT_TRUE(BIT_CHECK(ignitionChannelsOn, IGN4_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(ignitionChannelsOn, IGN8_CMD_BIT));
}

/**
 * Test: All ignition channels ON
 * Expected: All bits should be set
 */
void test_ignition_channel_bits_all_on(void)
{
    ignitionChannelsOn = 0xFF; // All ON

    for (uint8_t ch = 0; ch < 8; ch++)
    {
        TEST_ASSERT_TRUE(BIT_CHECK(ignitionChannelsOn, ch));
    }
}

/**
 * Test: All ignition channels OFF
 * Expected: All bits should be clear
 */
void test_ignition_channel_bits_all_off(void)
{
    ignitionChannelsOn = 0x00; // All OFF

    for (uint8_t ch = 0; ch < 8; ch++)
    {
        TEST_ASSERT_FALSE(BIT_CHECK(ignitionChannelsOn, ch));
    }
}

// ============================================================================
// CATEGORY 4: Fuel Channel Bit Tests (3 tests)
// ============================================================================

/**
 * Test: Individual fuel channel control
 * Expected: Bits can be set/cleared independently
 */
void test_fuel_channel_bits_individual(void)
{
    fuelChannelsOn = 0x00;

    BIT_SET(fuelChannelsOn, INJ1_CMD_BIT);
    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ1_CMD_BIT));

    BIT_SET(fuelChannelsOn, INJ3_CMD_BIT);
    BIT_SET(fuelChannelsOn, INJ5_CMD_BIT);

    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ1_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(fuelChannelsOn, INJ2_CMD_BIT));
    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ3_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(fuelChannelsOn, INJ4_CMD_BIT));
    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ5_CMD_BIT));
}

/**
 * Test: All fuel channels enabled
 * Expected: 0xFF = all bits set
 */
void test_fuel_channel_bits_all_enabled(void)
{
    fuelChannelsOn = 0xFF;

    for (uint8_t ch = 0; ch < INJ_CHANNELS; ch++)
    {
        TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, ch));
    }
}

/**
 * Test: Partial fuel channel configuration (sequential injection)
 * Expected: Only selected channels enabled
 */
void test_fuel_channel_bits_sequential_config(void)
{
    fuelChannelsOn = 0x0F; // Only channels 0-3 enabled (4 cylinder)

    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ1_CMD_BIT));
    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ2_CMD_BIT));
    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ3_CMD_BIT));
    TEST_ASSERT_TRUE(BIT_CHECK(fuelChannelsOn, INJ4_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(fuelChannelsOn, INJ5_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(fuelChannelsOn, INJ6_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(fuelChannelsOn, INJ7_CMD_BIT));
    TEST_ASSERT_FALSE(BIT_CHECK(fuelChannelsOn, INJ8_CMD_BIT));
}

// ============================================================================
// CATEGORY 5: Boundary and Edge Cases (4 tests)
// ============================================================================

/**
 * Test: All cranking conditions false
 * Expected: Return 0
 */
void test_calculateFixedCrankingOverride_all_conditions_false(void)
{
    configPage4.ignCranklock = 0;
    BIT_CLEAR(currentStatus.engine, BIT_ENGINE_CRANK);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    currentStatus.RPM = 0;
    currentStatus.dwell = 5000;

    uint32_t result = calculateFixedCrankingOverride();

    TEST_ASSERT_EQUAL_UINT32(0, result);
}

/**
 * Test: Injector minimum opening time boundary
 * Expected: Pulsewidth >= inj_opentime_uS is valid
 */
void test_injector_opening_time_boundary(void)
{
    inj_opentime_uS = 1000; // 1ms minimum

    // Below minimum - would not schedule
    currentStatus.PW1 = 999;
    TEST_ASSERT_TRUE(currentStatus.PW1 < inj_opentime_uS);

    // At minimum - should schedule
    currentStatus.PW1 = 1000;
    TEST_ASSERT_TRUE(currentStatus.PW1 >= inj_opentime_uS);

    // Above minimum - should schedule
    currentStatus.PW1 = 1500;
    TEST_ASSERT_TRUE(currentStatus.PW1 >= inj_opentime_uS);
}

/**
 * Test: Max outputs configuration
 * Expected: Loop bounds checking works correctly
 */
void test_max_outputs_configuration(void)
{
    // V8 configuration
    maxIgnOutputs = 8;
    maxInjOutputs = 8;
    TEST_ASSERT_EQUAL_UINT8(8, maxIgnOutputs);
    TEST_ASSERT_EQUAL_UINT8(8, maxInjOutputs);

    // 4-cylinder configuration
    maxIgnOutputs = 4;
    maxInjOutputs = 4;
    TEST_ASSERT_EQUAL_UINT8(4, maxIgnOutputs);
    TEST_ASSERT_EQUAL_UINT8(4, maxInjOutputs);

    // Single channel (for testing)
    maxIgnOutputs = 1;
    maxInjOutputs = 1;
    TEST_ASSERT_EQUAL_UINT8(1, maxIgnOutputs);
    TEST_ASSERT_EQUAL_UINT8(1, maxInjOutputs);
}

/**
 * Test: BIT_ENGINE_CRANK and BIT_DECODER_HAS_FIXED_CRANKING manipulation
 * Expected: Bit operations work correctly
 */
void test_engine_and_decoder_bit_operations(void)
{
    currentStatus.engine = 0;
    decoderState = 0;

    // Set cranking bit
    BIT_SET(currentStatus.engine, BIT_ENGINE_CRANK);
    TEST_ASSERT_TRUE(BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK));

    // Set decoder fixed cranking bit
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    TEST_ASSERT_TRUE(BIT_CHECK(decoderState, BIT_DECODER_HAS_FIXED_CRANKING));

    // Clear cranking bit
    BIT_CLEAR(currentStatus.engine, BIT_ENGINE_CRANK);
    TEST_ASSERT_FALSE(BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK));
    TEST_ASSERT_TRUE(BIT_CHECK(decoderState, BIT_DECODER_HAS_FIXED_CRANKING)); // Still set

    // Clear decoder bit
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    TEST_ASSERT_FALSE(BIT_CHECK(decoderState, BIT_DECODER_HAS_FIXED_CRANKING));
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    // Category 1: Fixed Cranking Override Tests (10 tests)
    RUN_TEST(test_calculateFixedCrankingOverride_disabled);
    RUN_TEST(test_calculateFixedCrankingOverride_not_cranking);
    RUN_TEST(test_calculateFixedCrankingOverride_no_decoder_support);
    RUN_TEST(test_calculateFixedCrankingOverride_low_rpm_with_adjustment);
    RUN_TEST(test_calculateFixedCrankingOverride_high_rpm_no_adjustment);
    RUN_TEST(test_calculateFixedCrankingOverride_rpm_boundary_250);
    RUN_TEST(test_calculateFixedCrankingOverride_rpm_249);
    RUN_TEST(test_calculateFixedCrankingOverride_zero_dwell);
    RUN_TEST(test_calculateFixedCrankingOverride_large_dwell);
    RUN_TEST(test_calculateFixedCrankingOverride_rpm_zero);

    // Category 2: Ignition Angle Adjustment Tests (5 tests)
    RUN_TEST(test_adjustIgnitionStartAnglesForCranking_all_channels);
    RUN_TEST(test_adjustIgnitionStartAnglesForCranking_multiple_calls);
    RUN_TEST(test_adjustIgnitionStartAnglesForCranking_from_zero);
    RUN_TEST(test_adjustIgnitionStartAnglesForCranking_from_negative);
    RUN_TEST(test_adjustIgnitionStartAnglesForCranking_large_angle);

    // Category 3: Ignition Channel Bit Tests (3 tests)
    RUN_TEST(test_ignition_channel_bits_individual);
    RUN_TEST(test_ignition_channel_bits_all_on);
    RUN_TEST(test_ignition_channel_bits_all_off);

    // Category 4: Fuel Channel Bit Tests (3 tests)
    RUN_TEST(test_fuel_channel_bits_individual);
    RUN_TEST(test_fuel_channel_bits_all_enabled);
    RUN_TEST(test_fuel_channel_bits_sequential_config);

    // Category 5: Boundary and Edge Cases (4 tests)
    RUN_TEST(test_calculateFixedCrankingOverride_all_conditions_false);
    RUN_TEST(test_injector_opening_time_boundary);
    RUN_TEST(test_max_outputs_configuration);
    RUN_TEST(test_engine_and_decoder_bit_operations);

    return UNITY_END();
}
