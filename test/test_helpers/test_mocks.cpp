/**
 * @file test_mocks.cpp
 * @brief Implementation of mock objects for native unit tests
 */

#include "test_mocks.h"

// ============================================================================
// STREAM MOCKS
// ============================================================================

class MockStream : public Stream {
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t) override { return 1; }
};

static MockStream mockSerialInstance;
Stream* pPrimarySerial = &mockSerialInstance;
Stream* pSecondarySerial = &mockSerialInstance;

// ============================================================================
// CONFIG PAGE MOCKS
// ============================================================================

mockConfigPage13 configPage13 = {{0}};

// ============================================================================
// EEPROM MOCK
// ============================================================================

MockEEPROM EEPROM;
