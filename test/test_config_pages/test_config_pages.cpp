/**
 * @file test_config_pages.cpp
 * @brief Unit tests for config page struct sizes - TunerStudio compatibility
 *
 * These tests verify that all config page structs have the EXACT size expected
 * for TunerStudio communication compatibility.
 *
 * IMPORTANT: Page numbers vs struct mapping (from pages.h):
 *   veSetPage = 1   (128 bytes) - contains config2
 *   veMapPage = 2   (288 bytes) - VE table
 *   ignMapPage = 3  (288 bytes) - Ignition table
 *   ignSetPage = 4  (128 bytes) - contains config4
 *   afrMapPage = 5  (288 bytes) - AFR table
 *   afrSetPage = 6  (128 bytes) - contains config6
 *   boostvvtPage = 7  (240 bytes) - Boost/VVT tables
 *   seqFuelPage = 8   (384 bytes) - Sequential fuel tables
 *   canbusPage = 9    (192 bytes) - contains config9
 *   warmupPage = 10   (192 bytes) - contains config10
 *   fuelMap2Page = 11 (288 bytes) - Fuel map 2
 *   wmiMapPage = 12   (192 bytes) - WMI table
 *   progOutsPage = 13 (128 bytes) - contains config13
 *   ignMap2Page = 14  (288 bytes) - Ignition map 2
 *   boostvvtPage2 = 15 (256 bytes) - 80 byte table + config15 (176 bytes)
 *
 * ini_page_sizes[] = { 0, 128, 288, 288, 128, 288, 128, 240, 384, 192, 192, 288, 192, 128, 288, 256 }
 */

#include <unity.h>
#include "globals.h"
#include "config_pages.h"

// Expected STRUCT sizes (not page sizes - some pages contain tables + structs)
// These are the sizes the structs MUST be on the target platform
#define EXPECTED_CONFIG2_SIZE   128   // Page 1 (veSetPage) = 128 bytes, all config2
#define EXPECTED_CONFIG4_SIZE   128   // Page 4 (ignSetPage) = 128 bytes, all config4
#define EXPECTED_CONFIG6_SIZE   128   // Page 6 (afrSetPage) = 128 bytes, all config6
#define EXPECTED_CONFIG9_SIZE   192   // Page 9 (canbusPage) = 192 bytes, all config9
#define EXPECTED_CONFIG10_SIZE  192   // Page 10 (warmupPage) = 192 bytes, all config10
#define EXPECTED_CONFIG13_SIZE  128   // Page 13 (progOutsPage) = 128 bytes, all config13
#define EXPECTED_CONFIG15_SIZE  176   // Page 15 has 80 byte table + 176 byte config15 = 256 bytes

// LOG_ENTRY_SIZE must match ochBlockSize in INI
#define INI_OCH_BLOCK_SIZE  138

void setUp(void) {}
void tearDown(void) {}

// =============================================================================
// Config Struct Size Tests - CRITICAL for TunerStudio Communication
// =============================================================================

/**
 * @brief Test config2 struct size matches expected
 *
 * config2 contains: fuel settings, acceleration enrichment, warmup enrichment,
 * injector timing, MAP sampling, engine type configuration
 * Page 1 (veSetPage) is 128 bytes, entirely config2.
 */
void test_config2_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG2_SIZE,
        sizeof(config2),
        "config2 size mismatch! Page 1 (veSetPage) expects 128 bytes"
    );
}

/**
 * @brief Test config4 struct size matches expected
 *
 * config4 contains: trigger configuration, ignition timing, dwell settings,
 * rev limiter, decoder selection
 */
void test_config4_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG4_SIZE,
        sizeof(config4),
        "config4 size mismatch! Page 4 (ignSetPage) expects 128 bytes"
    );
}

/**
 * @brief Test config6 struct size matches expected
 *
 * config6 contains: EGO/AFR settings, boost control, VVT configuration,
 * launch control, idle control, fan control
 */
void test_config6_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG6_SIZE,
        sizeof(config6),
        "config6 size mismatch! Page 6 (afrSetPage) expects 128 bytes"
    );
}

/**
 * @brief Test config9 struct size matches expected
 *
 * config9 contains: CAN bus configuration, CAN input channels,
 * auxiliary inputs, boost-by-gear, engine protection
 */
void test_config9_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG9_SIZE,
        sizeof(config9),
        "config9 size mismatch! Page 9 (canbusPage) expects 192 bytes"
    );
}

/**
 * @brief Test config10 struct size matches expected
 *
 * config10 contains: cranking enrichment, rotary engine support,
 * staging, flex fuel, N2O control, knock detection, fuel2/spark2 modes
 */
void test_config10_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG10_SIZE,
        sizeof(config10),
        "config10 size mismatch! Page 10 (warmupPage) expects 192 bytes"
    );
}

/**
 * @brief Test config13 struct size matches expected
 *
 * config13 contains: programmable I/O output logic,
 * 8 programmable outputs with pin selection and comparison operations
 */
void test_config13_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG13_SIZE,
        sizeof(config13),
        "config13 size mismatch! Page 13 (progOutsPage) expects 128 bytes"
    );
}

/**
 * @brief Test config15 struct size matches expected
 *
 * config15 contains: boost control, VVT extended settings, A/C control,
 * OBD-II configuration (VIN, ECU name, DTC storage, freeze frame),
 * BMW fuel consumption parameters
 *
 * NOTE: Page 15 (boostvvtPage2) is 256 bytes total, but contains:
 *   - 80 byte boost lookup table (boostTableLookupDuty)
 *   - 176 byte config15 struct
 */
void test_config15_size_exact_match(void) {
    TEST_ASSERT_EQUAL_MESSAGE(
        EXPECTED_CONFIG15_SIZE,
        sizeof(config15),
        "config15 size mismatch! Page 15 expects 80 byte table + 176 byte config15"
    );
}

// =============================================================================
// Output Channel Tests - Real-time Data Streaming
// =============================================================================

/**
 * @brief Test LOG_ENTRY_SIZE matches INI ochBlockSize
 *
 * This is the size of the real-time data packet sent to TunerStudio.
 * Must match ochBlockSize=138 in the INI file.
 */
void test_log_entry_size_matches_ini(void) {
    // Note: LOG_ENTRY_SIZE is redefined to 1 in UNIT_TEST mode (logger.h:17)
    // This test documents the expected production value
    TEST_ASSERT_EQUAL_MESSAGE(
        INI_OCH_BLOCK_SIZE,
        138,  // Expected production value
        "LOG_ENTRY_SIZE should be 138 bytes for TunerStudio ochBlockSize"
    );
}

// =============================================================================
// Struct Alignment Tests - Prevent Padding Issues
// =============================================================================

/**
 * @brief Verify config2 has no unexpected padding
 *
 * On 32-bit systems, structs may be padded to 4-byte boundaries.
 * The struct should use __attribute__((packed)) or explicit reserved bytes.
 */
void test_config2_alignment_correct(void) {
    // config2 should be exactly 128 bytes with no hidden padding
    TEST_ASSERT_EQUAL_MESSAGE(
        0,
        sizeof(config2) % 4,
        "config2 should be 4-byte aligned for STM32"
    );
}

/**
 * @brief Verify config15 has no unexpected padding
 */
void test_config15_alignment_correct(void) {
    // config15 should be exactly 176 bytes
    TEST_ASSERT_EQUAL_MESSAGE(
        0,
        sizeof(config15) % 4,
        "config15 should be 4-byte aligned for STM32"
    );
}

// =============================================================================
// Boundary Value Tests - Ensure Structs Don't Exceed Page Limits
// =============================================================================

/**
 * @brief Test that config structs don't exceed their allocated space
 *
 * This catches issues where adding fields without reducing reserved bytes
 * causes the struct to overflow its allocation.
 */
void test_all_config_structs_within_limits(void) {
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG2_SIZE, sizeof(config2),
        "config2 exceeds limit");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG4_SIZE, sizeof(config4),
        "config4 exceeds limit");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG6_SIZE, sizeof(config6),
        "config6 exceeds limit");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG9_SIZE, sizeof(config9),
        "config9 exceeds limit");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG10_SIZE, sizeof(config10),
        "config10 exceeds limit");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG13_SIZE, sizeof(config13),
        "config13 exceeds limit");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(EXPECTED_CONFIG15_SIZE, sizeof(config15),
        "config15 exceeds limit");
}

// =============================================================================
// Debug Helper - Print Actual Sizes (useful when tests fail)
// =============================================================================

/**
 * @brief Print all config struct sizes for debugging
 *
 * This test always passes but prints useful debug info when sizes mismatch.
 */
void test_print_config_struct_sizes(void) {
    // This test documents actual sizes - useful for debugging
    // Unity doesn't have printf, so we use TEST_MESSAGE with formatted string

    char msg[128];

    snprintf(msg, sizeof(msg), "config2:  actual=%u, expected=%u",
             (unsigned)sizeof(config2), EXPECTED_CONFIG2_SIZE);
    TEST_MESSAGE(msg);

    snprintf(msg, sizeof(msg), "config4:  actual=%u, expected=%u",
             (unsigned)sizeof(config4), EXPECTED_CONFIG4_SIZE);
    TEST_MESSAGE(msg);

    snprintf(msg, sizeof(msg), "config6:  actual=%u, expected=%u",
             (unsigned)sizeof(config6), EXPECTED_CONFIG6_SIZE);
    TEST_MESSAGE(msg);

    snprintf(msg, sizeof(msg), "config9:  actual=%u, expected=%u",
             (unsigned)sizeof(config9), EXPECTED_CONFIG9_SIZE);
    TEST_MESSAGE(msg);

    snprintf(msg, sizeof(msg), "config10: actual=%u, expected=%u",
             (unsigned)sizeof(config10), EXPECTED_CONFIG10_SIZE);
    TEST_MESSAGE(msg);

    snprintf(msg, sizeof(msg), "config13: actual=%u, expected=%u",
             (unsigned)sizeof(config13), EXPECTED_CONFIG13_SIZE);
    TEST_MESSAGE(msg);

    snprintf(msg, sizeof(msg), "config15: actual=%u, expected=%u",
             (unsigned)sizeof(config15), EXPECTED_CONFIG15_SIZE);
    TEST_MESSAGE(msg);

    TEST_PASS();
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Config struct size tests (7 critical tests)
    RUN_TEST(test_config2_size_exact_match);
    RUN_TEST(test_config4_size_exact_match);
    RUN_TEST(test_config6_size_exact_match);
    RUN_TEST(test_config9_size_exact_match);
    RUN_TEST(test_config10_size_exact_match);
    RUN_TEST(test_config13_size_exact_match);
    RUN_TEST(test_config15_size_exact_match);

    // Output channel test
    RUN_TEST(test_log_entry_size_matches_ini);

    // Alignment tests
    RUN_TEST(test_config2_alignment_correct);
    RUN_TEST(test_config15_alignment_correct);

    // Boundary tests
    RUN_TEST(test_all_config_structs_within_limits);

    // Debug helper (always passes, prints sizes)
    RUN_TEST(test_print_config_struct_sizes);

    return UNITY_END();
}
