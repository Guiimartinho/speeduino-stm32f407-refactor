/**
 * @file test_main_native.cpp
 * @brief Native test runner for test_tables
 *
 * Provides a proper main() function for native tests, replacing the
 * Arduino-style setup()/loop() structure.
 */

#if defined(NATIVE_BUILD) || defined(UNIT_TEST)

#include <unity.h>
#include "tests_tables.h"
#include "test_table2d.h"

int main(int argc, char **argv) {
    UNITY_BEGIN();

    testTables();
    testTable2d();

    return UNITY_END();
}

#endif // NATIVE_BUILD || UNIT_TEST
