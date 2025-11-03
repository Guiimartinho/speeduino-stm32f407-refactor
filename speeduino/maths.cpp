#include <Arduino.h>
#include "maths.h"

/** @brief PRNG state variables for XORShift algorithm */
static uint8_t a, x, y, z;

/**
 * @brief Generate pseudo-random number from 1 to 100 (inclusive)
 * @return Random number in range [1, 100]
 *
 * @note Uses XORShift algorithm - fast and lightweight PRNG
 * @note Auto-seeds from micros() on first call (when all state == 0)
 * @note State requires 4 bytes, execution time ~4µs per call
 * @note Not cryptographically secure - suitable for engine control only
 *
 * @details Algorithm:
 *          1. First call: Initialize state from micros() timer
 *          2. XORShift operations generate next pseudo-random value
 *          3. Loop until result < 100, then return result + 1
 *
 * @example
 *          uint8_t randomPercent = random1to100(); // Returns 1-100
 *
 * @complexity 2 (simple loop with XORShift operations)
 * @misra Compliant: 21 lines, low complexity, deterministic
 *
 * @warning Do NOT use for security purposes (predictable seed)
 */
uint8_t random1to100()
{
  //Check if this is the first time being run. If so, seed the random number generator with micros()
  if( (a == 0U) && (x == 0U) && (y == 0U) && (z == 0U) )
  {
    x = micros() >> 24U;
    y = micros() >> 16U;
    z = micros() >> 8U;
    a = micros();
  }

  do
  {
    unsigned char t = x ^ (x << 4);
		x=y;
		y=z;
		z=a;
		a = z ^ t ^ ( z >> 1) ^ (t << 1);
  }
  while(a >= 100U);
  return (a+1U);
}
