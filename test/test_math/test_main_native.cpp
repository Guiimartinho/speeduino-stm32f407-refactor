/**
 * @file test_main_native.cpp
 * @brief Native test runner for test_math
 */

#if defined(NATIVE_BUILD) || defined(UNIT_TEST)

#include <unity.h>
#include "tests_crankmaths.h"
#include "test_fp_support.h"

// External test functions
extern void testPercent();
extern void testDivision();
extern void testBitShift();
extern void test_LOW_PASS_FILTER();
extern void test_fast_map();

int main(int argc, char **argv) {
    UNITY_BEGIN();

    testCrankMaths();
    testPercent();
    testDivision();
    testBitShift();
    test_LOW_PASS_FILTER();
    test_fast_map();

    return UNITY_END();
}

#endif // NATIVE_BUILD || UNIT_TEST
