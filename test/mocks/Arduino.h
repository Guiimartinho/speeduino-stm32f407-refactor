#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

/**
 * @file Arduino.h
 * @brief Arduino API mock for native testing
 *
 * Provides minimal Arduino API implementation for unit testing without hardware.
 * Allows Speeduino code to compile and run on native platform (x86/x64).
 *
 * @note This is a mock implementation - does not interact with real hardware
 * @date 2025-11-05
 * @version 1.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPES
// ============================================================================

typedef uint8_t byte;
typedef bool boolean;

// ============================================================================
// CONSTANTS
// ============================================================================

// Digital pin states
#define HIGH 1
#define LOW 0

// Pin modes
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3

// Interrupt modes
#define RISING 1
#define FALLING 2
#define CHANGE 3

// Math constants
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

// Bit manipulation
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define bit(b) (1UL << (b))

// PROGMEM (no-op on native)
#define PROGMEM
#define PGM_P const char *
#define PSTR(s) (s)
#define F(string_literal) (string_literal)

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_float(addr) (*(const float *)(addr))
#define pgm_read_ptr(addr) (*(void * const *)(addr))

#define pgm_read_byte_near(addr) pgm_read_byte(addr)
#define pgm_read_word_near(addr) pgm_read_word(addr)
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#define pgm_read_float_near(addr) pgm_read_float(addr)
#define pgm_read_ptr_near(addr) pgm_read_ptr(addr)

// ============================================================================
// GLOBAL MOCK STATE (defined in arduino_mock.cpp)
// ============================================================================

extern uint32_t mock_micros_value;
extern uint32_t mock_millis_value;
extern uint8_t mock_digital_pins[256];
extern uint16_t mock_analog_pins[64];

// ============================================================================
// TIME FUNCTIONS
// ============================================================================

/**
 * @brief Returns number of microseconds since program start (mocked)
 * @return Current microseconds value from mock
 */
inline uint32_t micros(void) {
    return mock_micros_value;
}

/**
 * @brief Returns number of milliseconds since program start (mocked)
 * @return Current milliseconds value from mock
 */
inline uint32_t millis(void) {
    return mock_millis_value;
}

/**
 * @brief Delays execution for specified milliseconds (mocked - just advances time)
 * @param ms Milliseconds to delay
 */
inline void delay(uint32_t ms) {
    mock_millis_value += ms;
    mock_micros_value += (ms * 1000UL);
}

/**
 * @brief Delays execution for specified microseconds (mocked - just advances time)
 * @param us Microseconds to delay
 */
inline void delayMicroseconds(uint32_t us) {
    mock_micros_value += us;
}

// ============================================================================
// GPIO FUNCTIONS
// ============================================================================

/**
 * @brief Set pin mode (mocked - no-op)
 * @param pin Pin number
 * @param mode Pin mode (INPUT, OUTPUT, etc)
 */
inline void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin;
    (void)mode;
    // No-op in mock
}

/**
 * @brief Write digital value to pin (mocked - stores in array)
 * @param pin Pin number
 * @param val Value to write (HIGH or LOW)
 */
inline void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < 256) {
        mock_digital_pins[pin] = val;
    }
}

/**
 * @brief Read digital value from pin (mocked - reads from array)
 * @param pin Pin number
 * @return Value from mock array
 */
inline int digitalRead(uint8_t pin) {
    if (pin < 256) {
        return mock_digital_pins[pin];
    }
    return LOW;
}

/**
 * @brief Write analog value to pin (PWM) (mocked - stores in array)
 * @param pin Pin number
 * @param val Value to write (0-255)
 */
inline void analogWrite(uint8_t pin, int val) {
    if (pin < 64) {
        mock_analog_pins[pin] = (uint16_t)val;
    }
}

/**
 * @brief Read analog value from pin (ADC) (mocked - reads from array)
 * @param pin Pin number
 * @return Value from mock array (0-1023)
 */
inline int analogRead(uint8_t pin) {
    if (pin < 64) {
        return mock_analog_pins[pin];
    }
    return 0;
}

/**
 * @brief Set analog reference voltage (mocked - no-op)
 * @param mode Reference mode
 */
inline void analogReference(uint8_t mode) {
    (void)mode;
    // No-op in mock
}

// ============================================================================
// INTERRUPT FUNCTIONS
// ============================================================================

/**
 * @brief Disable interrupts (mocked - no-op)
 */
inline void noInterrupts(void) {
    // No-op in mock
}

/**
 * @brief Enable interrupts (mocked - no-op)
 */
inline void interrupts(void) {
    // No-op in mock
}

/**
 * @brief Attach interrupt handler (mocked - no-op)
 */
inline void attachInterrupt(uint8_t interrupt, void (*ISR)(void), int mode) {
    (void)interrupt;
    (void)ISR;
    (void)mode;
    // No-op in mock
}

/**
 * @brief Detach interrupt handler (mocked - no-op)
 */
inline void detachInterrupt(uint8_t interrupt) {
    (void)interrupt;
    // No-op in mock
}

// ============================================================================
// MATH FUNCTIONS
// ============================================================================

/**
 * @brief Map value from one range to another
 * @param x Value to map
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Mapped value
 */
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Constrain value to range
 * @param x Value to constrain
 * @param a Minimum value
 * @param b Maximum value
 * @return Constrained value
 */
inline long constrain(long x, long a, long b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

/**
 * @brief Return minimum of two values
 */
#define min(a,b) ((a)<(b)?(a):(b))

/**
 * @brief Return maximum of two values
 */
#define max(a,b) ((a)>(b)?(a):(b))

/**
 * @brief Return absolute value
 */
#define abs(x) ((x)>0?(x):-(x))

/**
 * @brief Square root
 */
#define sq(x) ((x)*(x))

// ============================================================================
// RANDOM FUNCTIONS
// ============================================================================

/**
 * @brief Seed random number generator (mocked)
 */
inline void randomSeed(unsigned long seed) {
    srand(seed);
}

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ ONLY - RANDOM FUNCTIONS (overloaded)
// ============================================================================

#ifdef __cplusplus
// Forward declare random functions (implemented in arduino_mock.cpp)
long random(long howbig);
long random(long howsmall, long howbig);

extern "C" {
#endif

// ============================================================================
// STRING FUNCTIONS
// ============================================================================

/**
 * @brief Convert integer to string
 */
inline char* itoa(int value, char* str, int base) {
    if (base == 10) {
        sprintf(str, "%d", value);
    } else if (base == 16) {
        sprintf(str, "%x", value);
    } else if (base == 8) {
        sprintf(str, "%o", value);
    } else if (base == 2) {
        int i = 0;
        unsigned int v = (unsigned int)value;
        do {
            str[i++] = (v & 1) ? '1' : '0';
            v >>= 1;
        } while (v);
        str[i] = '\0';
        // Reverse string
        for (int j = 0; j < i / 2; j++) {
            char tmp = str[j];
            str[j] = str[i - 1 - j];
            str[i - 1 - j] = tmp;
        }
    }
    return str;
}

/**
 * @brief Convert long to string
 */
inline char* ltoa(long value, char* str, int base) {
    return itoa((int)value, str, base);
}

/**
 * @brief Convert unsigned long to string
 */
inline char* ultoa(unsigned long value, char* str, int base) {
    if (base == 10) {
        sprintf(str, "%lu", value);
    } else if (base == 16) {
        sprintf(str, "%lx", value);
    }
    return str;
}

/**
 * @brief Convert double to string
 */
inline char* dtostrf(double val, signed char width, unsigned char prec, char* str) {
    sprintf(str, "%*.*f", width, prec, val);
    return str;
}

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ ONLY - Serial Mock
// ============================================================================

#ifdef __cplusplus

/**
 * @brief Mock Serial class for unit testing
 *
 * Provides no-op implementation of Serial interface.
 * Allows code using Serial to compile without errors.
 */
class SerialMock {
public:
    void begin(unsigned long baud) { (void)baud; }
    void begin(unsigned long baud, uint8_t config) { (void)baud; (void)config; }
    void end() {}

    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() {}

    size_t write(uint8_t c) { (void)c; return 1; }
    size_t write(const uint8_t *buffer, size_t size) { (void)buffer; (void)size; return size; }

    void print(const char* str) { (void)str; }
    void print(char c) { (void)c; }
    void print(int n) { (void)n; }
    void print(unsigned int n) { (void)n; }
    void print(long n) { (void)n; }
    void print(unsigned long n) { (void)n; }
    void print(double n, int digits = 2) { (void)n; (void)digits; }

    void println(const char* str) { (void)str; }
    void println(char c) { (void)c; }
    void println(int n) { (void)n; }
    void println(unsigned int n) { (void)n; }
    void println(long n) { (void)n; }
    void println(unsigned long n) { (void)n; }
    void println(double n, int digits = 2) { (void)n; (void)digits; }
    void println() {}

    operator bool() { return true; }
};

// Global Serial object
extern SerialMock Serial;
extern SerialMock Serial1;
extern SerialMock Serial2;
extern SerialMock Serial3;

/**
 * @brief Mock String class (minimal implementation)
 */
class String {
public:
    String() : buffer(nullptr), len(0) {}
    String(const char* str) : buffer(nullptr), len(0) {
        if (str) {
            len = strlen(str);
            buffer = (char*)malloc(len + 1);
            if (buffer) {
                strcpy(buffer, str);
            }
        }
    }
    String(int num) : buffer(nullptr), len(0) {
        char buf[16];
        sprintf(buf, "%d", num);
        len = strlen(buf);
        buffer = (char*)malloc(len + 1);
        if (buffer) {
            strcpy(buffer, buf);
        }
    }
    ~String() {
        if (buffer) free(buffer);
    }

    const char* c_str() const { return buffer ? buffer : ""; }
    size_t length() const { return len; }

private:
    char* buffer;
    size_t len;
};

#endif // __cplusplus

#endif // ARDUINO_MOCK_H
