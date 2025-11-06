/**
 * @file test_common_sources.cpp
 * @brief Include source implementations for native tests
 */

#if defined(NATIVE_BUILD) || defined(UNIT_TEST)

// Mock implementations
#include "../mocks/arduino_mock.cpp"

// Math utilities
#include "../../speeduino/maths.cpp"

// Include crankMaths (has angleToTimeMicroSecPerDegree)
#include "../../speeduino/crankMaths.cpp"

// Stub variables for decoder functions
uint32_t revolutionTime = 0;

// Stub for SetRevolutionTime (from decoders.cpp - too many hardware deps)
bool SetRevolutionTime(uint32_t revTime) {
    // Stub implementation for tests
    revolutionTime = revTime;
    return true;
}

#endif // NATIVE_BUILD || UNIT_TEST
