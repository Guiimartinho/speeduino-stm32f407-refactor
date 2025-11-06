/**
 * @file test_common_sources.cpp
 * @brief Include source implementations for native tests
 *
 * This file includes all required source implementations for test_tables.
 * By having a single file that includes all sources, we avoid multiple
 * definition errors while ensuring all needed functions are available.
 */

#if defined(NATIVE_BUILD) || defined(UNIT_TEST)

// Mock implementations
#include "../mocks/arduino_mock.cpp"

// Table operations
#include "../../speeduino/table2d.cpp"
#include "../../speeduino/table3d.cpp"
#include "../../speeduino/table3d_interpolate.cpp"

// Math utilities
#include "../../speeduino/maths.cpp"

#endif // NATIVE_BUILD || UNIT_TEST
