/**
 * @file test_mocks.h
 * @brief Common mocks and definitions for native unit tests
 *
 * Provides platform-independent definitions for types, macros, and functions
 * that are normally provided by Arduino/STM32 libraries but are not available
 * in the native test environment.
 */

#ifndef TEST_MOCKS_H
#define TEST_MOCKS_H

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

// Arduino byte type (used extensively in Speeduino)
#ifndef byte
typedef uint8_t byte;
#endif

// ============================================================================
// ATOMIC OPERATIONS MOCK
// ============================================================================

// Mock atomic operations for native tests (single-threaded)
#define ATOMIC_BLOCK(type) for(int __atomic_dummy = 0; __atomic_dummy < 1; __atomic_dummy++)
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_FORCEON 1

// Alternative atomic macro names
#define ATOMIC() ATOMIC_BLOCK(ATOMIC_RESTORESTATE)

// ============================================================================
// BOARD DEFINITIONS MOCK
// ============================================================================

// Mock board definitions (normally from board_*.h files)
#ifndef BOARD_H
#define BOARD_H "test_mock_board.h"
#endif

// ============================================================================
// STREAM CLASS MOCK (for serial/CAN communication)
// ============================================================================

#ifndef Stream_h

class Stream {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            n += write(*buffer++);
        }
        return n;
    }
};

// Mock serial streams
extern Stream* pPrimarySerial;
extern Stream* pSecondarySerial;

#define Stream_h
#endif

// ============================================================================
// SIMPLYATOMIC MOCK
// ============================================================================

// Mock SimplyAtomic library functions (used in auxiliaries.cpp)
#define ATOMIC_BLOCK_START
#define ATOMIC_BLOCK_END

// ============================================================================
// GLOBAL CONFIG PAGES MOCK
// ============================================================================

// Mock config page structures (normally defined in globals.h)
#ifndef GLOBALS_H_MOCKED

struct mockConfigPage13 {
    byte outputPin[8];
};

extern mockConfigPage13 configPage13;

#define GLOBALS_H_MOCKED
#endif

// ============================================================================
// UTILITY MACROS
// ============================================================================

// Bit manipulation (if not already defined)
#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// Min/max (if not already defined)
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

// ============================================================================
// EEPROM MOCK
// ============================================================================

// Mock EEPROM class for storage tests
class MockEEPROM {
public:
    uint8_t read(int address) { return 0; }
    void write(int address, uint8_t value) {}
    void update(int address, uint8_t value) {}
    uint16_t length() { return 4096; }
};

extern MockEEPROM EEPROM;

#endif // TEST_MOCKS_H
