/**
 * @file schedule_calcs.hpp
 * @brief Inline timing calculation functions for fuel injection and ignition scheduling
 *
 * @note Functions with underscore prefix (_) are NOT part of the public API
 * @note All functions are inlined for performance-critical ECU operations
 *
 * @misra All functions comply with MISRA-C:2012 standards
 * @complexity Maximum complexity: 4 (calculateInjectorStartAngle)
 */

// Note that all functions with an underscore prefix are NOT part
// of the public API. They are only here so we can inline them.

#include "scheduler.h"
#include "crankMaths.h"
#include "maths.h"
#include "timers.h"

/**
 * @brief Calculate injector opening angle in crankshaft degrees
 * @param pwDegrees Injector pulse width in crank degrees
 * @param injChannelDegrees Cylinder TDC offset in degrees (0-720°)
 * @param injAngle Injection start angle relative to TDC (0-720°)
 * @return Injector start angle clamped to [0, CRANK_ANGLE_MAX_INJ]
 *
 * @note Handles pulse widths exceeding one engine cycle (multi-rotation)
 * @note Uses while loops to avoid underflow and clamp to valid range
 * @note CRANK_ANGLE_MAX_INJ ranges from 45° (8-cyl) to 720° (1-cyl)
 *
 * @details Algorithm:
 *          1. Calculate raw start angle: injAngle + channelDegrees
 *          2. Add CRANK_ANGLE_MAX_INJ until startAngle >= pwDegrees (avoid underflow)
 *          3. Subtract pwDegrees to get actual start angle
 *          4. Modulo by CRANK_ANGLE_MAX_INJ to clamp result
 *
 * @example 4-cylinder (CRANK_ANGLE_MAX_INJ=180°), pwDegrees=90°, injAngle=30°, channelDegrees=0°:
 *          startAngle = 30 + 0 = 30°
 *          30 < 90, so: startAngle = 30 + 180 = 210°
 *          startAngle = 210 - 90 = 120°
 *          120 < 180, return 120°
 *
 * @complexity 4 (2 while loops, bounds checking)
 * @misra Compliant: 6 effective lines, no deep nesting
 */
static inline uint16_t calculateInjectorStartAngle(uint16_t pwDegrees, int16_t injChannelDegrees, uint16_t injAngle)
{
  // 0<=injAngle<=720°
  // 0<=injChannelDegrees<=720°
  // 0<pwDegrees<=??? (could be many crank rotations in the worst case!)
  // 45<=CRANK_ANGLE_MAX_INJ<=720
  // (CRANK_ANGLE_MAX_INJ can be as small as 360/nCylinders. E.g. 45° for 8 cylinder)

  uint16_t startAngle = (uint16_t)injAngle + (uint16_t)injChannelDegrees;
  
  while (startAngle<pwDegrees) { startAngle = startAngle + (uint16_t)CRANK_ANGLE_MAX_INJ; } // Avoid underflow
  startAngle = startAngle - pwDegrees; // startAngle guaranteed to be >=0.
  while (startAngle>(uint16_t)CRANK_ANGLE_MAX_INJ) { startAngle = startAngle - (uint16_t)CRANK_ANGLE_MAX_INJ; } // Clamp to 0<=startAngle<=CRANK_ANGLE_MAX_INJ

  return startAngle;
}

/**
 * @brief Calculate timeout (in µs) until injector should open
 * @param schedule Reference to fuel schedule containing current status
 * @param openAngle Target angle for injector opening (degrees)
 * @param crankAngle Current crankshaft position (degrees)
 * @return Timeout in microseconds, or 0 if already past opening angle
 *
 * @note Returns 0 if schedule is OFF and angle has passed
 * @note Wraps around cycle boundary if schedule is RUNNING or OFF
 *
 * @details Behavior by status:
 *          - delta > 0: Direct conversion (angle ahead of current position)
 *          - delta <= 0 AND (RUNNING or OFF): Add CRANK_ANGLE_MAX_INJ and convert
 *          - delta <= 0 AND other states: Return 0 (missed window)
 *
 * @example At 6000 RPM (10ms/360°):
 *          openAngle=90°, crankAngle=30° → delta=60° → ~1666µs
 *          openAngle=30°, crankAngle=90° (RUNNING) → delta=-60° → wrap to 300° → ~8333µs
 *
 * @complexity 2 (conditional branches)
 * @misra Compliant: 8 effective lines, guard clauses pattern
 */
static inline uint32_t calculateInjectorTimeout(const FuelSchedule &schedule, int openAngle, int crankAngle)
{
  uint32_t tempTimeout = 0;
  int16_t delta = openAngle - crankAngle;

  if(delta > 0) { tempTimeout = angleToTimeMicroSecPerDegree((uint16_t)delta); }
  else if ( (schedule.Status == RUNNING) || (schedule.Status == OFF)) 
  {
    while(delta < 0) { delta += CRANK_ANGLE_MAX_INJ; }
    tempTimeout = angleToTimeMicroSecPerDegree((uint16_t)delta);
  }
  
  return tempTimeout;
}

/**
 * @brief Calculate ignition coil charge start and spark fire angles
 * @param dwellAngle Coil charging time in crank degrees (typically 10-30°)
 * @param channelIgnDegrees Cylinder TDC offset in degrees (0 or channel-specific)
 * @param advance Ignition timing advance in degrees BTDC (positive = advance)
 * @param[out] pEndAngle Pointer to store spark firing angle
 * @param[out] pStartAngle Pointer to store coil charging start angle
 *
 * @note Automatically wraps angles to stay within [0, CRANK_ANGLE_MAX_IGN]
 * @note If channelIgnDegrees==0, uses CRANK_ANGLE_MAX_IGN as base
 *
 * @details Calculation:
 *          1. endAngle = (channelIgnDegrees or CRANK_ANGLE_MAX_IGN) - advance
 *          2. Wrap endAngle if > CRANK_ANGLE_MAX_IGN
 *          3. startAngle = endAngle - dwellAngle
 *          4. Wrap startAngle if < 0
 *
 * @example 4-cylinder (CRANK_ANGLE_MAX_IGN=720°), channelIgnDegrees=180°, advance=10°, dwellAngle=20°:
 *          endAngle = 180 - 10 = 170° (spark at 10° BTDC for cylinder 2)
 *          startAngle = 170 - 20 = 150° (start charging coil at 30° BTDC)
 *
 * @complexity 2 (conditional wrapping)
 * @misra Compliant: 3 effective lines, output via pointers
 */
static inline void calculateIgnitionAngle(const uint16_t dwellAngle, const uint16_t channelIgnDegrees, int8_t advance, int *pEndAngle, int *pStartAngle)
{
  *pEndAngle = (int16_t)(channelIgnDegrees==0U ? (uint16_t)CRANK_ANGLE_MAX_IGN : channelIgnDegrees) - (int16_t)advance;
  if(*pEndAngle > CRANK_ANGLE_MAX_IGN) {*pEndAngle -= CRANK_ANGLE_MAX_IGN;}
  *pStartAngle = *pEndAngle - dwellAngle;
  if(*pStartAngle < 0) {*pStartAngle += CRANK_ANGLE_MAX_IGN;}
}

/**
 * @brief Calculate trailing spark timing for rotary engines (Mazda RX-7/RX-8)
 * @param dwellAngle Coil charging time in crank degrees
 * @param rotarySplitDegrees Split angle between leading and trailing plugs (typically 10-15°)
 * @param leadIgnitionAngle Pre-calculated leading spark angle
 * @param[out] pEndAngle Pointer to store trailing spark fire angle
 * @param[out] pStartAngle Pointer to store trailing coil charge start angle
 *
 * @note Rotary engines use dual spark plugs per rotor face for complete combustion
 * @note Trailing spark fires AFTER leading spark by rotarySplitDegrees
 * @note Wraps angles to stay within [0, CRANK_ANGLE_MAX_IGN]
 *
 * @details Calculation:
 *          1. endAngle = leadIgnitionAngle + rotarySplitDegrees
 *          2. startAngle = endAngle - dwellAngle
 *          3. Wrap both angles if outside valid range
 *
 * @example Mazda 13B: leadAngle=350°, split=12°, dwell=20°
 *          endAngle = 350 + 12 = 362°
 *          startAngle = 362 - 20 = 342°
 *          (Trailing fires 12° after leading)
 *
 * @complexity 1 (simple arithmetic with wrapping)
 * @misra Compliant: 3 effective lines, rotary-specific logic
 */
static inline void calculateIgnitionTrailingRotary(uint16_t dwellAngle, int rotarySplitDegrees, int leadIgnitionAngle, int *pEndAngle, int *pStartAngle)
{
  *pEndAngle = leadIgnitionAngle + rotarySplitDegrees;
  *pStartAngle = *pEndAngle - dwellAngle;
  if(*pStartAngle > CRANK_ANGLE_MAX_IGN) {*pStartAngle -= CRANK_ANGLE_MAX_IGN;}
  if(*pStartAngle < 0) {*pStartAngle += CRANK_ANGLE_MAX_IGN;}
}

/**
 * @brief Calculate timeout (in µs) until ignition coil charging should begin (PRIVATE)
 * @param schedule Reference to ignition schedule containing current status
 * @param startAngle Target angle for coil charge start (degrees)
 * @param crankAngle Current crankshaft position (degrees)
 * @return Timeout in microseconds, or 0 if window missed
 *
 * @note PRIVATE HELPER - Use calculateIgnitionTimeout() public wrapper instead
 * @note Returns 0 if angle has passed and schedule not RUNNING
 * @note Wraps delta angle only if RUNNING and within one cycle (-CRANK_ANGLE_MAX_IGN)
 *
 * @details Behavior:
 *          - delta >= 0: Direct conversion to time
 *          - delta < 0 AND RUNNING AND delta > -CRANK_ANGLE_MAX_IGN: Wrap and convert
 *          - Otherwise: Return 0 (missed window, abort scheduling)
 *
 * @example At 6000 RPM:
 *          startAngle=340°, crankAngle=350° → delta=-10°
 *          If RUNNING: delta = -10 + 720 = 710° → ~19722µs (next cycle)
 *          If not RUNNING: return 0 (abort)
 *
 * @complexity 2 (nested conditionals with guard clauses)
 * @misra Compliant: 8 effective lines, early return pattern
 */
static inline uint32_t _calculateIgnitionTimeout(const IgnitionSchedule &schedule, int16_t startAngle, int16_t crankAngle) 
{
  int16_t delta = startAngle - crankAngle;
  if (delta < 0)
  {
    if ((schedule.Status == RUNNING) && (delta>-CRANK_ANGLE_MAX_IGN)) 
    { 
      // Msut be >0
      delta = delta + CRANK_ANGLE_MAX_IGN; 
    }
    else
    {
      return 0U;
    }
  }

  return angleToTimeMicroSecPerDegree(delta);
}

/**
 * @brief Adjust angle relative to specific ignition channel TDC (PRIVATE)
 * @param angle Absolute crankshaft angle (degrees)
 * @param channelInjDegrees Channel-specific TDC offset (degrees)
 * @return Angle adjusted relative to channel TDC, wrapped to [0, CRANK_ANGLE_MAX_IGN]
 *
 * @note PRIVATE HELPER - Used by calculateIgnitionTimeout() wrapper
 * @note Converts absolute crank angle to channel-relative angle
 *
 * @details Calculation:
 *          1. Subtract channel offset from absolute angle
 *          2. If result < 0, add CRANK_ANGLE_MAX_IGN to wrap
 *          3. Return wrapped angle
 *
 * @example 4-cylinder engine, channel 2 at 180° TDC:
 *          angle=200° (absolute), channelInjDegrees=180°
 *          adjusted = 200 - 180 = 20° (20° ATDC for cylinder 2)
 *
 * @complexity 1 (simple arithmetic with wrap)
 * @misra Compliant: 4 effective lines, trivial helper
 */
static inline uint16_t _adjustToIgnChannel(int angle, int channelInjDegrees) 
{
  angle = angle - channelInjDegrees;
  if( angle < 0) { return angle + CRANK_ANGLE_MAX_IGN; }
  return angle;
}

/**
 * @brief Calculate timeout until ignition coil charging begins (PUBLIC WRAPPER)
 * @param schedule Reference to ignition schedule containing current status
 * @param startAngle Target angle for coil charge start (absolute degrees)
 * @param channelIgnDegrees Channel-specific TDC offset (0 for cylinder 1)
 * @param crankAngle Current crankshaft position (absolute degrees)
 * @return Timeout in microseconds, or 0 if window missed
 *
 * @note PUBLIC API - Handles both absolute (channelIgnDegrees==0) and channel-relative angles
 * @note Delegates to _calculateIgnitionTimeout() after coordinate transformation
 *
 * @details Behavior:
 *          - If channelIgnDegrees==0: Use absolute angles directly
 *          - Otherwise: Convert both angles to channel-relative coordinates via _adjustToIgnChannel()
 *
 * @example Cylinder 1 (absolute): channelIgnDegrees=0, startAngle=340°, crankAngle=350°
 *          → Direct call: _calculateIgnitionTimeout(schedule, 340, 350)
 *
 * @example Cylinder 2 (relative): channelIgnDegrees=180°, startAngle=340°, crankAngle=350°
 *          → Adjust: _adjustToIgnChannel(340, 180)=160°, _adjustToIgnChannel(350, 180)=170°
 *          → Call: _calculateIgnitionTimeout(schedule, 160, 170)
 *
 * @complexity 1 (simple delegation with optional coordinate transform)
 * @misra Compliant: 4 effective lines, clean abstraction
 */
static inline uint32_t calculateIgnitionTimeout(const IgnitionSchedule &schedule, int startAngle, int channelIgnDegrees, int crankAngle)
{
  if (channelIgnDegrees == 0) 
  {
      return _calculateIgnitionTimeout(schedule, startAngle, crankAngle);
  }
  return _calculateIgnitionTimeout(schedule, _adjustToIgnChannel(startAngle, channelIgnDegrees), _adjustToIgnChannel(crankAngle, channelIgnDegrees));
}

/** @brief Minimum engine revolutions before enabling endCompare scheduling */
#define MIN_CYCLES_FOR_ENDCOMPARE 6

/**
 * @brief Adjust ignition schedule compare register based on current crank position
 * @param[in,out] schedule Reference to ignition schedule to modify
 * @param endAngle Target angle for spark firing (degrees)
 * @param crankAngle Current crankshaft position (degrees)
 *
 * @note Called during sync/trigger events to update hardware timer compare values
 * @note Behavior depends on schedule status and engine start cycles
 *
 * @details Behavior by state:
 *          - Status==RUNNING: Immediately update hardware compare register (SET_COMPARE)
 *          - Status!=RUNNING AND startRevolutions>MIN_CYCLES_FOR_ENDCOMPARE:
 *            Set endCompare for future scheduling, mark endScheduleSetByDecoder=true
 *          - Otherwise: No action (engine still cranking)
 *
 * @note ignitionLimits() clamps angle delta to valid range
 * @note angleToTimeMicroSecPerDegree() converts angle to time
 * @note uS_TO_TIMER_COMPARE() converts microseconds to timer ticks
 *
 * @example At 6000 RPM, endAngle=10° BTDC, crankAngle=30° BTDC (delta=-20°):
 *          If RUNNING: SET_COMPARE immediately for -20° (wrapped)
 *          If cranking >6 revs: Set endCompare for future use
 *
 * @complexity 1 (simple conditional dispatch)
 * @misra Compliant: 6 effective lines, hardware timer interaction
 */
inline void adjustCrankAngle(IgnitionSchedule &schedule, int endAngle, int crankAngle) 
{
  if( (schedule.Status == RUNNING) ) { 
    SET_COMPARE(schedule.compare, schedule.counter + uS_TO_TIMER_COMPARE( angleToTimeMicroSecPerDegree( ignitionLimits( (endAngle - crankAngle) ) ) ) ); 
  }
  else if(currentStatus.startRevolutions > MIN_CYCLES_FOR_ENDCOMPARE) { 
    schedule.endCompare = schedule.counter + uS_TO_TIMER_COMPARE( angleToTimeMicroSecPerDegree( ignitionLimits( (endAngle - crankAngle) ) ) ); 
    schedule.endScheduleSetByDecoder = true; 
  }
}