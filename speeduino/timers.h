

/*
 * Low-frequency timer interrupt (1ms interval) for SCG-ECU 2.0
 *
 * Purpose:
 * Implementing a lower frequency interrupt loop to perform calculations that are needed less often,
 * some of which depend on time having passed (delta/time) to be meaningful.
 *
 * STM32F407 Implementation:
 * Uses TIM11 for 1ms system timer interrupt (oneMSInterval)
 * Timer resolution: 4us per tick (defined in board_stm32_official.h)
 */
#ifndef TIMERS_H
#define TIMERS_H

#include "globals.h"

// Cast compare values to actual timer overflow limit type
#define SET_COMPARE(compare, value) compare = (COMPARE_TYPE)(value)

// Tacho output macros (STM32 uses digitalWrite)
#define TACHO_PULSE_LOW()         (digitalWrite(pinTachOut, LOW))
#define TACHO_PULSE_HIGH()        (digitalWrite(pinTachOut, HIGH))

// Tacho output states
enum TachoOutputStatus {TACHO_INACTIVE, READY, ACTIVE};

extern volatile TachoOutputStatus tachoOutputFlag;
extern volatile uint16_t tachoSweepIncr;

#define TACHO_SWEEP_TIME_MS 1500
#define TACHO_SWEEP_RAMP_MS (TACHO_SWEEP_TIME_MS * 2 / 3)
#define MS_PER_SEC  1000

extern volatile unsigned int dwellLimit_uS;

// STM32 1ms interval timer callback (called by TIM11 ISR)
void oneMSInterval(void);
void initialiseTimers(void);

#endif // TIMERS_H
