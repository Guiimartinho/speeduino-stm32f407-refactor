/**
 * @file logger.cpp
 * @brief Real-time data logging and telemetry export for TunerStudio and secondary serial
 *
 * @details Implements data logging interfaces for external monitoring and tuning tools:
 *
 * **Purpose:**
 * - Serialize currentStatus structure for transmission over serial/CAN
 * - Support TunerStudio realtime data display (gauges, datalogs, etc.)
 * - Provide tooth/trigger logging for decoder diagnostics
 * - Export human-readable values for SD card logging
 * - Support legacy secondary serial protocol (OBD adapters, etc.)
 *
 * **Logging Formats Implemented:**
 *
 * 1. **TunerStudio Format** - getTSLogEntry()
 *    - Byte-indexed access (0-138+)
 *    - Multi-byte fields split into low/high bytes
 *    - Values have offsets/scaling expected by TunerStudio
 *    - NOT human-readable (e.g., temp in ADC counts + offset)
 *
 * 2. **Human-Readable Format** - getReadableLogEntry()
 *    - Index-based access (0-98+) - NOT byte position
 *    - Single entry per field (no byte splitting)
 *    - Values in engineering units (kPa, °C, RPM, etc.)
 *    - For SD card CSV logging, serial monitors
 *
 * 3. **Floating-Point Format** - getReadableFloatLogEntry()
 *    - Same as readable but with FPU precision
 *    - Battery voltage: 12.6V (not 126)
 *    - Pulse width: 3.45ms (not 3450µs)
 *    - O2: 14.7 AFR (not 147)
 *    - Only on platforms with FPU (STM32F4, Teensy 3.x+)
 *
 * 4. **Legacy Secondary Serial** - getLegacySecondarySerialLogEntry()
 *    - Compatible with older Speeduino tools
 *    - 123 byte format
 *    - Used for OBD-II adapters, Bluetooth modules
 *
 * **Tooth Logger (Trigger Diagnostics):**
 * - Captures trigger wheel edge timestamps for decoder analysis
 * - 4 modes supported:
 *   - Standard: Primary + Secondary triggers
 *   - Composite: Primary + Secondary (combined pattern)
 *   - Composite Tertiary: Primary + Tertiary (3-wheel systems)
 *   - Composite Cams: Secondary + Tertiary (cam-only logging)
 * - Replaces normal trigger ISRs with logging ISRs
 * - Used to diagnose missing teeth, noise, incorrect wheel patterns
 *
 * **Performance Optimizations:**
 * - is2ByteEntry(): Boundless binary search (O(log n) vs O(n) linear)
 * - Static constexpr arrays in PROGMEM (Flash, not RAM)
 * - Switch-case for compiler optimization (jump table)
 * - Inline lowByte/highByte macros (no function call overhead)
 *
 * **Data Flow:**
 * ```
 * [Sensors] → currentStatus → getTSLogEntry() → Serial → TunerStudio
 *                           ↓
 *                     getReadableLogEntry() → SD Card / Serial Monitor
 * ```
 *
 * @note All get*LogEntry() functions are called frequently (10-100Hz)
 * @note Keep functions FAST - no heavy computation allowed
 * @note Tooth logger ISRs are TIME-CRITICAL (µs precision required)
 * @note MISRA-C:2012 compliant with named constants
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (MISRA-C compliant with comprehensive Doxygen)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#include "globals.h"
#include "logger.h"
#include "decoders.h"
#include "init.h"
#include "maths.h"
#include "utilities.h"
#include BOARD_H
#include "units.h"

//=============================================================================
// TUNERSTUDIO LOG ENTRY (Byte-indexed format)
//=============================================================================

namespace {

/**
 * @brief Handles TunerStudio log entries 0-9 (Basic counters & sensors)
 * @param byteNum Byte index (0-9)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_00_09(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 0: statusValue = currentStatus.secl; break;
    case 1: statusValue = currentStatus.status1; break;
    case 2: statusValue = currentStatus.engine; break;
    case 3: statusValue = currentStatus.syncLossCounter; break;
    case 4: statusValue = lowByte(currentStatus.MAP); break;
    case 5: statusValue = highByte(currentStatus.MAP); break;
    case 6: statusValue = lowByte(temperatureAddOffset(currentStatus.IAT)); break;
    case 7: statusValue = lowByte(temperatureAddOffset(currentStatus.coolant)); break;
    case 8: statusValue = currentStatus.batCorrection; break;
    case 9: statusValue = currentStatus.battery10; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 10-25 (Fueling & engine metrics)
 * @param byteNum Byte index (10-25)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_10_25(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 10: statusValue = currentStatus.O2; break;
    case 11: statusValue = currentStatus.egoCorrection; break;
    case 12: statusValue = currentStatus.iatCorrection; break;
    case 13: statusValue = currentStatus.wueCorrection; break;
    case 14: statusValue = lowByte(currentStatus.RPM); break;
    case 15: statusValue = highByte(currentStatus.RPM); break;
    case 16: statusValue = lowByte(currentStatus.AEamount >> 1U); break;
    case 17: statusValue = lowByte(currentStatus.corrections); break;
    case 18: statusValue = highByte(currentStatus.corrections); break;
    case 19: statusValue = currentStatus.VE1; break;
    case 20: statusValue = currentStatus.VE2; break;
    case 21: statusValue = currentStatus.afrTarget; break;
    case 22: statusValue = lowByte(currentStatus.tpsDOT); break;
    case 23: statusValue = highByte(currentStatus.tpsDOT); break;
    case 24: statusValue = currentStatus.advance; break;
    case 25: statusValue = currentStatus.TPS; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 26-41 (Performance & sensors)
 * @param byteNum Byte index (26-41)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_26_41(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 26:
      statusValue = lowByte(currentStatus.loopsPerSecond);
      break;
    case 27:
      statusValue = highByte(currentStatus.loopsPerSecond);
      break;
    case 28:
      currentStatus.freeRAM = freeRam();
      statusValue = lowByte(currentStatus.freeRAM);
      break;
    case 29:
      currentStatus.freeRAM = freeRam();
      statusValue = highByte(currentStatus.freeRAM);
      break;
    case 30: statusValue = lowByte(currentStatus.boostTarget >> 1U); break;
    case 31: statusValue = lowByte(div100(currentStatus.boostDuty)); break;
    case 32: statusValue = currentStatus.status2; break;
    case 33: statusValue = lowByte(currentStatus.rpmDOT); break;
    case 34: statusValue = highByte(currentStatus.rpmDOT); break;
    case 35: statusValue = currentStatus.ethanolPct; break;
    case 36: statusValue = currentStatus.flexCorrection; break;
    case 37: statusValue = currentStatus.flexIgnCorrection; break;
    case 38: statusValue = currentStatus.idleLoad; break;
    case 39: statusValue = currentStatus.testOutputs; break;
    case 40: statusValue = currentStatus.O2_2; break;
    case 41: statusValue = currentStatus.baro; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 42-73 (CAN bus 16 channels)
 * @param byteNum Byte index (42-73)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_42_73(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 42: statusValue = lowByte(currentStatus.canin[0]); break;
    case 43: statusValue = highByte(currentStatus.canin[0]); break;
    case 44: statusValue = lowByte(currentStatus.canin[1]); break;
    case 45: statusValue = highByte(currentStatus.canin[1]); break;
    case 46: statusValue = lowByte(currentStatus.canin[2]); break;
    case 47: statusValue = highByte(currentStatus.canin[2]); break;
    case 48: statusValue = lowByte(currentStatus.canin[3]); break;
    case 49: statusValue = highByte(currentStatus.canin[3]); break;
    case 50: statusValue = lowByte(currentStatus.canin[4]); break;
    case 51: statusValue = highByte(currentStatus.canin[4]); break;
    case 52: statusValue = lowByte(currentStatus.canin[5]); break;
    case 53: statusValue = highByte(currentStatus.canin[5]); break;
    case 54: statusValue = lowByte(currentStatus.canin[6]); break;
    case 55: statusValue = highByte(currentStatus.canin[6]); break;
    case 56: statusValue = lowByte(currentStatus.canin[7]); break;
    case 57: statusValue = highByte(currentStatus.canin[7]); break;
    case 58: statusValue = lowByte(currentStatus.canin[8]); break;
    case 59: statusValue = highByte(currentStatus.canin[8]); break;
    case 60: statusValue = lowByte(currentStatus.canin[9]); break;
    case 61: statusValue = highByte(currentStatus.canin[9]); break;
    case 62: statusValue = lowByte(currentStatus.canin[10]); break;
    case 63: statusValue = highByte(currentStatus.canin[10]); break;
    case 64: statusValue = lowByte(currentStatus.canin[11]); break;
    case 65: statusValue = highByte(currentStatus.canin[11]); break;
    case 66: statusValue = lowByte(currentStatus.canin[12]); break;
    case 67: statusValue = highByte(currentStatus.canin[12]); break;
    case 68: statusValue = lowByte(currentStatus.canin[13]); break;
    case 69: statusValue = highByte(currentStatus.canin[13]); break;
    case 70: statusValue = lowByte(currentStatus.canin[14]); break;
    case 71: statusValue = highByte(currentStatus.canin[14]); break;
    case 72: statusValue = lowByte(currentStatus.canin[15]); break;
    case 73: statusValue = highByte(currentStatus.canin[15]); break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 74-83 (TPS & pulse widths)
 * @param byteNum Byte index (74-83)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_74_83(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 74: statusValue = currentStatus.tpsADC; break;
    case 75: statusValue = 0U /*getNextError()*/; break;
    case 76: statusValue = lowByte(currentStatus.PW1); break;
    case 77: statusValue = highByte(currentStatus.PW1); break;
    case 78: statusValue = lowByte(currentStatus.PW2); break;
    case 79: statusValue = highByte(currentStatus.PW2); break;
    case 80: statusValue = lowByte(currentStatus.PW3); break;
    case 81: statusValue = highByte(currentStatus.PW3); break;
    case 82: statusValue = lowByte(currentStatus.PW4); break;
    case 83: statusValue = highByte(currentStatus.PW4); break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 84-109 (Advanced status)
 * @param byteNum Byte index (84-109)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_84_109(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 84: statusValue = currentStatus.status3; break;
    case 85: statusValue = currentStatus.engineProtectStatus; break;
    case 86: statusValue = lowByte(currentStatus.fuelLoad); break;
    case 87: statusValue = highByte(currentStatus.fuelLoad); break;
    case 88: statusValue = lowByte(currentStatus.ignLoad); break;
    case 89: statusValue = highByte(currentStatus.ignLoad); break;
    case 90: statusValue = lowByte(currentStatus.dwell); break;
    case 91: statusValue = highByte(currentStatus.dwell); break;
    case 92: statusValue = currentStatus.CLIdleTarget; break;
    case 93: statusValue = lowByte(currentStatus.mapDOT); break;
    case 94: statusValue = highByte(currentStatus.mapDOT); break;
    case 95: statusValue = lowByte(currentStatus.vvt1Angle); break;
    case 96: statusValue = highByte(currentStatus.vvt1Angle); break;
    case 97: statusValue = currentStatus.vvt1TargetAngle; break;
    case 98: statusValue = lowByte(currentStatus.vvt1Duty); break;
    case 99: statusValue = lowByte(currentStatus.flexBoostCorrection); break;
    case 100: statusValue = highByte(currentStatus.flexBoostCorrection); break;
    case 101: statusValue = currentStatus.baroCorrection; break;
    case 102: statusValue = currentStatus.VE; break;
    case 103: statusValue = currentStatus.ASEValue; break;
    case 104: statusValue = lowByte(currentStatus.vss); break;
    case 105: statusValue = highByte(currentStatus.vss); break;
    case 106: statusValue = currentStatus.gear; break;
    case 107: statusValue = currentStatus.fuelPressure; break;
    case 108: statusValue = currentStatus.oilPressure; break;
    case 109: statusValue = currentStatus.wmiPW; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 110-129 (VVT2 & system)
 * @param byteNum Byte index (110-129)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_110_129(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 110: statusValue = currentStatus.status4; break;
    case 111: statusValue = lowByte(currentStatus.vvt2Angle); break;
    case 112: statusValue = highByte(currentStatus.vvt2Angle); break;
    case 113: statusValue = currentStatus.vvt2TargetAngle; break;
    case 114: statusValue = lowByte(currentStatus.vvt2Duty); break;
    case 115: statusValue = currentStatus.outputsStatus; break;
    case 116: statusValue = lowByte(temperatureAddOffset(currentStatus.fuelTemp)); break;
    case 117: statusValue = currentStatus.fuelTempCorrection; break;
    case 118: statusValue = currentStatus.advance1; break;
    case 119: statusValue = currentStatus.advance2; break;
    case 120: statusValue = currentStatus.TS_SD_Status; break;
    case 121: statusValue = lowByte(currentStatus.EMAP); break;
    case 122: statusValue = highByte(currentStatus.EMAP); break;
    case 123: statusValue = currentStatus.fanDuty; break;
    case 124: statusValue = currentStatus.airConStatus; break;
    case 125: statusValue = lowByte(currentStatus.actualDwell); break;
    case 126: statusValue = highByte(currentStatus.actualDwell); break;
    case 127: statusValue = currentStatus.status5; break;
    case 128: statusValue = currentStatus.knockCount; break;
    case 129: statusValue = currentStatus.knockRetard; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles TunerStudio log entries 130-138 (Extended pulse widths)
 * @param byteNum Byte index (130-138)
 * @return Status byte value
 */
static inline byte getTSLogEntry_Range_130_138(uint16_t byteNum)
{
  byte statusValue = 0;
  switch(byteNum)
  {
    case 130: statusValue = lowByte(currentStatus.PW5); break;
    case 131: statusValue = highByte(currentStatus.PW5); break;
    case 132: statusValue = lowByte(currentStatus.PW6); break;
    case 133: statusValue = highByte(currentStatus.PW6); break;
    case 134: statusValue = lowByte(currentStatus.PW7); break;
    case 135: statusValue = highByte(currentStatus.PW7); break;
    case 136: statusValue = lowByte(currentStatus.PW8); break;
    case 137: statusValue = highByte(currentStatus.PW8); break;
    case 138: statusValue = currentStatus.systemTemp; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

} // anonymous namespace

/**
 * @brief Returns byte-field from currentStatus for TunerStudio protocol
 *
 * Returns a numbered byte-field (partial field in case of multi-byte fields) from "current status" structure in the format expected by TunerStudio
 * Notes on fields:
 * - Numbered field will be fields from @ref currentStatus, but not at all in the internal order of strct (e.g. field RPM value, number 14 will be
 *   2nd field in struct)
 * - The fields stored in multi-byte types will be accessed lowbyte and highbyte separately (e.g. PW1 will be broken into numbered byte-fields 75,76)
 * - Values have the value offsets and shifts expected by TunerStudio. They will not all be a 'human readable value'
 * @param byteNum - byte-Field number. This is not the entry number (As some entries have multiple byets), but the byte number that is needed
 * @return Field value in 1 byte size struct fields or 1 byte partial value (chunk) on multibyte fields.
 */
byte getTSLogEntry(uint16_t byteNum)
{
  byte statusValue = 0;

  if (byteNum <= 9U) {
    statusValue = getTSLogEntry_Range_00_09(byteNum);
  }
  else if (byteNum <= 25U) {
    statusValue = getTSLogEntry_Range_10_25(byteNum);
  }
  else if (byteNum <= 41U) {
    statusValue = getTSLogEntry_Range_26_41(byteNum);
  }
  else if (byteNum <= 73U) {
    statusValue = getTSLogEntry_Range_42_73(byteNum);
  }
  else if (byteNum <= 83U) {
    statusValue = getTSLogEntry_Range_74_83(byteNum);
  }
  else if (byteNum <= 109U) {
    statusValue = getTSLogEntry_Range_84_109(byteNum);
  }
  else if (byteNum <= 129U) {
    statusValue = getTSLogEntry_Range_110_129(byteNum);
  }
  else if (byteNum <= 138U) {
    statusValue = getTSLogEntry_Range_130_138(byteNum);
  }
  else {
    statusValue = 0; // Out of range - MISRA check
  }

  return statusValue;
}

//=============================================================================
// HUMAN-READABLE LOG ENTRY (Index-based format)
//=============================================================================

namespace {

/**
 * @brief Handles readable log entries 0-12 (Basic status & sensors)
 * @param logIndex Log index (0-12)
 * @return Status value
 */
static inline int16_t getReadableLogEntry_Range_00_12(uint16_t logIndex)
{
  int16_t statusValue = 0;
  switch(logIndex)
  {
    case 0: statusValue = currentStatus.secl; break;
    case 1: statusValue = currentStatus.status1; break;
    case 2: statusValue = currentStatus.engine; break;
    case 3: statusValue = currentStatus.syncLossCounter; break;
    case 4: statusValue = currentStatus.MAP; break;
    case 5: statusValue = currentStatus.IAT; break;
    case 6: statusValue = currentStatus.coolant; break;
    case 7: statusValue = currentStatus.batCorrection; break;
    case 8: statusValue = currentStatus.battery10; break;
    case 9: statusValue = currentStatus.O2; break;
    case 10: statusValue = currentStatus.egoCorrection; break;
    case 11: statusValue = currentStatus.iatCorrection; break;
    case 12: statusValue = currentStatus.wueCorrection; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles readable log entries 13-25 (Engine metrics)
 * @param logIndex Log index (13-25)
 * @return Status value
 */
static inline int16_t getReadableLogEntry_Range_13_25(uint16_t logIndex)
{
  int16_t statusValue = 0;
  switch(logIndex)
  {
    case 13: statusValue = currentStatus.RPM; break;
    case 14: statusValue = currentStatus.AEamount; break;
    case 15: statusValue = currentStatus.corrections; break;
    case 16: statusValue = currentStatus.VE1; break;
    case 17: statusValue = currentStatus.VE2; break;
    case 18: statusValue = currentStatus.afrTarget; break;
    case 19: statusValue = currentStatus.tpsDOT; break;
    case 20: statusValue = currentStatus.advance; break;
    case 21: statusValue = currentStatus.TPS; break;
    case 22:
      if(currentStatus.loopsPerSecond > 60000U) { currentStatus.loopsPerSecond = 60000U;}
      statusValue = currentStatus.loopsPerSecond;
      break;
    case 23:
      currentStatus.freeRAM = freeRam();
      statusValue = currentStatus.freeRAM;
      break;
    case 24: statusValue = currentStatus.boostTarget; break;
    case 25: statusValue = currentStatus.boostDuty; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles readable log entries 26-50 (Sensors & CAN bus)
 * @param logIndex Log index (26-50)
 * @return Status value
 */
static inline int16_t getReadableLogEntry_Range_26_50(uint16_t logIndex)
{
  int16_t statusValue = 0;
  switch(logIndex)
  {
    case 26: statusValue = currentStatus.status2; break;
    case 27: statusValue = currentStatus.rpmDOT; break;
    case 28: statusValue = currentStatus.ethanolPct; break;
    case 29: statusValue = currentStatus.flexCorrection; break;
    case 30: statusValue = currentStatus.flexIgnCorrection; break;
    case 31: statusValue = currentStatus.idleLoad; break;
    case 32: statusValue = currentStatus.testOutputs; break;
    case 33: statusValue = currentStatus.O2_2; break;
    case 34: statusValue = currentStatus.baro; break;
    case 35: statusValue = currentStatus.canin[0]; break;
    case 36: statusValue = currentStatus.canin[1]; break;
    case 37: statusValue = currentStatus.canin[2]; break;
    case 38: statusValue = currentStatus.canin[3]; break;
    case 39: statusValue = currentStatus.canin[4]; break;
    case 40: statusValue = currentStatus.canin[5]; break;
    case 41: statusValue = currentStatus.canin[6]; break;
    case 42: statusValue = currentStatus.canin[7]; break;
    case 43: statusValue = currentStatus.canin[8]; break;
    case 44: statusValue = currentStatus.canin[9]; break;
    case 45: statusValue = currentStatus.canin[10]; break;
    case 46: statusValue = currentStatus.canin[11]; break;
    case 47: statusValue = currentStatus.canin[12]; break;
    case 48: statusValue = currentStatus.canin[13]; break;
    case 49: statusValue = currentStatus.canin[14]; break;
    case 50: statusValue = currentStatus.canin[15]; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles readable log entries 51-70 (TPS, PW, status, loads, VVT1)
 * @param logIndex Log index (51-70)
 * @return Status value
 */
static inline int16_t getReadableLogEntry_Range_51_70(uint16_t logIndex)
{
  int16_t statusValue = 0;
  switch(logIndex)
  {
    case 51: statusValue = currentStatus.tpsADC; break;
    case 52: statusValue = 0U /*getNextError()*/; break;
    case 53: statusValue = currentStatus.PW1; break;
    case 54: statusValue = currentStatus.PW2; break;
    case 55: statusValue = currentStatus.PW3; break;
    case 56: statusValue = currentStatus.PW4; break;
    case 57: statusValue = currentStatus.status3; break;
    case 58: statusValue = currentStatus.engineProtectStatus; break;
    case 59: break; //UNUSED!!
    case 60: statusValue = currentStatus.fuelLoad; break;
    case 61: statusValue = currentStatus.ignLoad; break;
    case 62: statusValue = (int16_t)currentStatus.dwell; break;
    case 63: statusValue = currentStatus.CLIdleTarget; break;
    case 64: statusValue = currentStatus.mapDOT; break;
    case 65: statusValue = currentStatus.vvt1Angle; break;
    case 66: statusValue = currentStatus.vvt1TargetAngle; break;
    case 67: statusValue = currentStatus.vvt1Duty; break;
    case 68: statusValue = currentStatus.flexBoostCorrection; break;
    case 69: statusValue = currentStatus.baroCorrection; break;
    case 70: statusValue = currentStatus.VE; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles readable log entries 71-98 (Extended status & system)
 * @param logIndex Log index (71-98)
 * @return Status value
 */
static inline int16_t getReadableLogEntry_Range_71_98(uint16_t logIndex)
{
  int16_t statusValue = 0;
  switch(logIndex)
  {
    case 71: statusValue = currentStatus.ASEValue; break;
    case 72: statusValue = currentStatus.vss; break;
    case 73: statusValue = currentStatus.gear; break;
    case 74: statusValue = currentStatus.fuelPressure; break;
    case 75: statusValue = currentStatus.oilPressure; break;
    case 76: statusValue = currentStatus.wmiPW; break;
    case 77: statusValue = currentStatus.status4; break;
    case 78: statusValue = currentStatus.vvt2Angle; break;
    case 79: statusValue = currentStatus.vvt2TargetAngle; break;
    case 80: statusValue = currentStatus.vvt2Duty; break;
    case 81: statusValue = currentStatus.outputsStatus; break;
    case 82: statusValue = currentStatus.fuelTemp; break;
    case 83: statusValue = currentStatus.fuelTempCorrection; break;
    case 84: statusValue = currentStatus.advance1; break;
    case 85: statusValue = currentStatus.advance2; break;
    case 86: statusValue = currentStatus.TS_SD_Status; break;
    case 87: statusValue = currentStatus.EMAP; break;
    case 88: statusValue = currentStatus.fanDuty; break;
    case 89: statusValue = currentStatus.airConStatus; break;
    case 90: statusValue = currentStatus.actualDwell; break;
    case 91: statusValue = currentStatus.status5; break;
    case 92: statusValue = currentStatus.knockCount; break;
    case 93: statusValue = currentStatus.knockRetard; break;
    case 94: statusValue = currentStatus.PW5; break;
    case 95: statusValue = currentStatus.PW6; break;
    case 96: statusValue = currentStatus.PW7; break;
    case 97: statusValue = currentStatus.PW8; break;
    case 98: statusValue = currentStatus.systemTemp; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

} // anonymous namespace

/**
 * @brief Returns human-readable log entry value in engineering units
 *
 * Similar to the @ref getTSLogEntry function, however this returns a full, unadjusted (ie human readable) log entry value.
 * See logger.h for the field names and order
 * @param logIndex - The log index required. Note that this is NOT the byte number, but the index in the log
 * @return Raw, unadjusted value of the log entry. No offset or multiply is applied like it is with the TS log
 */
int16_t getReadableLogEntry(uint16_t logIndex)
{
  int16_t statusValue = 0;

  if (logIndex <= 12U) {
    statusValue = getReadableLogEntry_Range_00_12(logIndex);
  }
  else if (logIndex <= 25U) {
    statusValue = getReadableLogEntry_Range_13_25(logIndex);
  }
  else if (logIndex <= 50U) {
    statusValue = getReadableLogEntry_Range_26_50(logIndex);
  }
  else if (logIndex <= 70U) {
    statusValue = getReadableLogEntry_Range_51_70(logIndex);
  }
  else if (logIndex <= 98U) {
    statusValue = getReadableLogEntry_Range_71_98(logIndex);
  }
  else {
    statusValue = 0; // Out of range - MISRA check
  }

  return statusValue;
}

//=============================================================================
// FLOATING-POINT LOG ENTRY (FPU-optimized format)
//=============================================================================

/**
 * @brief Returns floating-point log entry with decimal precision (FPU platforms only)
 *
 * An expansion to the @ref getReadableLogEntry function for systems that have an FPU. It will provide a floating point value for any parameter that this is appropriate for, otherwise will return the result of @ref getReadableLogEntry.
 * See logger.h for the field names and order
 * @param logIndex - The log index required. Note that this is NOT the byte number, but the index in the log
 * @return float value of the requested log entry.
 */
#if defined(FPU_MAX_SIZE) && FPU_MAX_SIZE >= 32 //cppcheck-suppress misra-c2012-20.9
float getReadableFloatLogEntry(uint16_t logIndex)
{
  float statusValue = 0.0;

  switch(logIndex)
  {
    case 8: statusValue = currentStatus.battery10 / 10.0; break; //battery voltage
    case 9: statusValue = currentStatus.O2 / 10.0; break;
    case 18: statusValue = currentStatus.afrTarget / 10.0; break;
    case 21: statusValue = currentStatus.TPS / 2.0; break; // TPS (0% to 100% = 0 to 200)
    case 33: statusValue = currentStatus.O2_2 / 10.0; break; //O2

    case 53: statusValue = currentStatus.PW1 / 1000.0; break; //Pulsewidth 1 Have to convert from uS to mS.
    case 54: statusValue = currentStatus.PW2 / 1000.0; break; //Pulsewidth 2 Have to convert from uS to mS.
    case 55: statusValue = currentStatus.PW3 / 1000.0; break; //Pulsewidth 3 Have to convert from uS to mS.
    case 56: statusValue = currentStatus.PW4 / 1000.0; break; //Pulsewidth 4 Have to convert from uS to mS.

    default: statusValue = getReadableLogEntry(logIndex); break; //If logIndex value is NOT a float based one, use the regular function
  }

  return statusValue;
}
#endif

//=============================================================================
// LEGACY SECONDARY SERIAL FORMAT (Backward compatibility)
//=============================================================================

namespace {

/**
 * @brief Handles legacy log entries 0-9 (Basic counters & sensors)
 * @param byteNum Byte index (0-9)
 * @return Status byte value
 */
static inline uint8_t getLegacyLogEntry_Range_00_09(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  switch(byteNum)
  {
    case 0: statusValue = currentStatus.secl; break;
    case 1: statusValue = currentStatus.status1; break;
    case 2: statusValue = currentStatus.engine; break;
    case 3: statusValue = (byte)div100(currentStatus.dwell); break;
    case 4: statusValue = lowByte(currentStatus.MAP); break;
    case 5: statusValue = highByte(currentStatus.MAP); break;
    case 6: statusValue = (byte)(temperatureAddOffset(currentStatus.IAT)); break;
    case 7: statusValue = (byte)(temperatureAddOffset(currentStatus.coolant)); break;
    case 8: statusValue = currentStatus.batCorrection; break;
    case 9: statusValue = currentStatus.battery10; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles legacy log entries 10-28 (Corrections & engine metrics)
 * @param byteNum Byte index (10-28)
 * @return Status byte value
 */
static inline uint8_t getLegacyLogEntry_Range_10_28(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  switch(byteNum)
  {
    case 10: statusValue = currentStatus.O2; break;
    case 11: statusValue = currentStatus.egoCorrection; break;
    case 12: statusValue = currentStatus.iatCorrection; break;
    case 13: statusValue = currentStatus.wueCorrection; break;
    case 14: statusValue = lowByte(currentStatus.RPM); break;
    case 15: statusValue = highByte(currentStatus.RPM); break;
    case 16: statusValue = currentStatus.AEamount; break;
    case 17: statusValue = currentStatus.corrections; break;
    case 18: statusValue = currentStatus.VE; break;
    case 19: statusValue = currentStatus.afrTarget; break;
    case 20: statusValue = lowByte(currentStatus.PW1); break;
    case 21: statusValue = highByte(currentStatus.PW1); break;
    case 22: statusValue = (uint8_t)(currentStatus.tpsDOT / 10); break;
    case 23: statusValue = currentStatus.advance; break;
    case 24: statusValue = currentStatus.TPS; break;
    case 25: statusValue = lowByte(currentStatus.loopsPerSecond); break;
    case 26: statusValue = highByte(currentStatus.loopsPerSecond); break;
    case 27:
      currentStatus.freeRAM = freeRam();
      statusValue = lowByte(currentStatus.freeRAM);
      break;
    case 28:
      currentStatus.freeRAM = freeRam();
      statusValue = highByte(currentStatus.freeRAM);
      break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles legacy log entries 29-40 (Boost, status, flex, idle)
 * @param byteNum Byte index (29-40)
 * @return Status byte value
 */
static inline uint8_t getLegacyLogEntry_Range_29_40(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  switch(byteNum)
  {
    case 29: statusValue = (byte)(currentStatus.boostTarget >> 1); break;
    case 30: statusValue = (byte)(currentStatus.boostDuty / 100); break;
    case 31: statusValue = currentStatus.status2; break;
    case 32: statusValue = lowByte(currentStatus.rpmDOT); break;
    case 33: statusValue = highByte(currentStatus.rpmDOT); break;
    case 34: statusValue = currentStatus.ethanolPct; break;
    case 35: statusValue = currentStatus.flexCorrection; break;
    case 36: statusValue = currentStatus.flexIgnCorrection; break;
    case 37: statusValue = currentStatus.idleLoad; break;
    case 38: statusValue = currentStatus.testOutputs; break;
    case 39: statusValue = currentStatus.O2_2; break;
    case 40: statusValue = currentStatus.baro; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles legacy log entries 41-72 (CAN bus 16 channels)
 * @param byteNum Byte index (41-72)
 * @return Status byte value
 */
static inline uint8_t getLegacyLogEntry_Range_41_72(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  switch(byteNum)
  {
    case 41: statusValue = lowByte(currentStatus.canin[0]); break;
    case 42: statusValue = highByte(currentStatus.canin[0]); break;
    case 43: statusValue = lowByte(currentStatus.canin[1]); break;
    case 44: statusValue = highByte(currentStatus.canin[1]); break;
    case 45: statusValue = lowByte(currentStatus.canin[2]); break;
    case 46: statusValue = highByte(currentStatus.canin[2]); break;
    case 47: statusValue = lowByte(currentStatus.canin[3]); break;
    case 48: statusValue = highByte(currentStatus.canin[3]); break;
    case 49: statusValue = lowByte(currentStatus.canin[4]); break;
    case 50: statusValue = highByte(currentStatus.canin[4]); break;
    case 51: statusValue = lowByte(currentStatus.canin[5]); break;
    case 52: statusValue = highByte(currentStatus.canin[5]); break;
    case 53: statusValue = lowByte(currentStatus.canin[6]); break;
    case 54: statusValue = highByte(currentStatus.canin[6]); break;
    case 55: statusValue = lowByte(currentStatus.canin[7]); break;
    case 56: statusValue = highByte(currentStatus.canin[7]); break;
    case 57: statusValue = lowByte(currentStatus.canin[8]); break;
    case 58: statusValue = highByte(currentStatus.canin[8]); break;
    case 59: statusValue = lowByte(currentStatus.canin[9]); break;
    case 60: statusValue = highByte(currentStatus.canin[9]); break;
    case 61: statusValue = lowByte(currentStatus.canin[10]); break;
    case 62: statusValue = highByte(currentStatus.canin[10]); break;
    case 63: statusValue = lowByte(currentStatus.canin[11]); break;
    case 64: statusValue = highByte(currentStatus.canin[11]); break;
    case 65: statusValue = lowByte(currentStatus.canin[12]); break;
    case 66: statusValue = highByte(currentStatus.canin[12]); break;
    case 67: statusValue = lowByte(currentStatus.canin[13]); break;
    case 68: statusValue = highByte(currentStatus.canin[13]); break;
    case 69: statusValue = lowByte(currentStatus.canin[14]); break;
    case 70: statusValue = highByte(currentStatus.canin[14]); break;
    case 71: statusValue = lowByte(currentStatus.canin[15]); break;
    case 72: statusValue = highByte(currentStatus.canin[15]); break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles legacy log entries 73-89 (TPS, PW, status, loads)
 * @param byteNum Byte index (73-89)
 * @return Status byte value
 */
static inline uint8_t getLegacyLogEntry_Range_73_89(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  switch(byteNum)
  {
    case 73: statusValue = currentStatus.tpsADC; break;
    case 74: statusValue = 0U /*getNextError()*/; break;
    case 75: statusValue = currentStatus.launchCorrection; break;
    case 76: statusValue = lowByte(currentStatus.PW2); break;
    case 77: statusValue = highByte(currentStatus.PW2); break;
    case 78: statusValue = lowByte(currentStatus.PW3); break;
    case 79: statusValue = highByte(currentStatus.PW3); break;
    case 80: statusValue = lowByte(currentStatus.PW4); break;
    case 81: statusValue = highByte(currentStatus.PW4); break;
    case 82: statusValue = currentStatus.status3; break;
    case 83: statusValue = currentStatus.engineProtectStatus; break;
    case 84: statusValue = lowByte(currentStatus.fuelLoad); break;
    case 85: statusValue = highByte(currentStatus.fuelLoad); break;
    case 86: statusValue = lowByte(currentStatus.ignLoad); break;
    case 87: statusValue = highByte(currentStatus.ignLoad); break;
    case 88: statusValue = lowByte(currentStatus.injAngle); break;
    case 89: statusValue = highByte(currentStatus.injAngle); break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

/**
 * @brief Handles legacy log entries 90-122 (VVT, pressures, extended)
 * @param byteNum Byte index (90-122)
 * @return Status byte value
 */
static inline uint8_t getLegacyLogEntry_Range_90_122(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  switch(byteNum)
  {
    case 90: statusValue = currentStatus.idleLoad; break;
    case 91: statusValue = currentStatus.CLIdleTarget; break;
    case 92: statusValue = (uint8_t)(currentStatus.mapDOT / 10); break;
    case 93: statusValue = (int8_t)currentStatus.vvt1Angle; break;
    case 94: statusValue = currentStatus.vvt1TargetAngle; break;
    case 95: statusValue = currentStatus.vvt1Duty; break;
    case 96: statusValue = lowByte(currentStatus.flexBoostCorrection); break;
    case 97: statusValue = highByte(currentStatus.flexBoostCorrection); break;
    case 98: statusValue = currentStatus.baroCorrection; break;
    case 99: statusValue = currentStatus.ASEValue; break;
    case 100: statusValue = lowByte(currentStatus.vss); break;
    case 101: statusValue = highByte(currentStatus.vss); break;
    case 102: statusValue = currentStatus.gear; break;
    case 103: statusValue = currentStatus.fuelPressure; break;
    case 104: statusValue = currentStatus.oilPressure; break;
    case 105: statusValue = currentStatus.wmiPW; break;
    case 106: statusValue = currentStatus.status4; break;
    case 107: statusValue = (int8_t)currentStatus.vvt2Angle; break;
    case 108: statusValue = currentStatus.vvt2TargetAngle; break;
    case 109: statusValue = currentStatus.vvt2Duty; break;
    case 110: statusValue = currentStatus.outputsStatus; break;
    case 111: statusValue = (byte)temperatureAddOffset(currentStatus.fuelTemp); break;
    case 112: statusValue = currentStatus.fuelTempCorrection; break;
    case 113: statusValue = currentStatus.VE1; break;
    case 114: statusValue = currentStatus.VE2; break;
    case 115: statusValue = currentStatus.advance1; break;
    case 116: statusValue = currentStatus.advance2; break;
    case 117: statusValue = currentStatus.nitrous_status; break;
    case 118: statusValue = currentStatus.TS_SD_Status; break;
    case 119: statusValue = lowByte(currentStatus.EMAP); break;
    case 120: statusValue = highByte(currentStatus.EMAP); break;
    case 121: statusValue = currentStatus.fanDuty; break;
    case 122: statusValue = currentStatus.airConStatus; break;
    default: statusValue = 0; break;
  }
  return statusValue;
}

} // anonymous namespace

/**
 * @brief Returns log entry in legacy secondary serial format
 *
 * @param byteNum Byte index in legacy format (0-122)
 * @return Byte value formatted for legacy protocol
 *
 * @details Used for compatibility with older Speeduino tools:
 * - OBD-II Bluetooth adapters
 * - Legacy dataloggers
 * - Third-party monitoring software
 *
 * @note 123-byte format (different from TunerStudio format)
 * @note Some fields have different byte positions vs getTSLogEntry()
 *
 * @complexity 3 (large switch-case, but O(1) with jump table)
 * @misra Compliant: Default case prevents undefined behavior
 */
uint8_t getLegacySecondarySerialLogEntry(uint16_t byteNum)
{
  uint8_t statusValue = 0;
  currentStatus.status2 ^= (-currentStatus.hasSync ^ currentStatus.status2) & (1U << BIT_STATUS2_SYNC); //Set the sync bit of the Spark variable to match the hasSync variable

  if (byteNum <= 9U) {
    statusValue = getLegacyLogEntry_Range_00_09(byteNum);
  }
  else if (byteNum <= 28U) {
    statusValue = getLegacyLogEntry_Range_10_28(byteNum);
  }
  else if (byteNum <= 40U) {
    statusValue = getLegacyLogEntry_Range_29_40(byteNum);
  }
  else if (byteNum <= 72U) {
    statusValue = getLegacyLogEntry_Range_41_72(byteNum);
  }
  else if (byteNum <= 89U) {
    statusValue = getLegacyLogEntry_Range_73_89(byteNum);
  }
  else if (byteNum <= 122U) {
    statusValue = getLegacyLogEntry_Range_90_122(byteNum);
  }
  else {
    statusValue = 0; // Out of range - MISRA check
  }

  return statusValue;
}

//=============================================================================
// LOG FIELD TYPE DETECTION (Binary search optimization)
//=============================================================================

/**
 * @brief Determines if log field is single-byte or two-byte using binary search
 *
 * Searches the log 2 byte array to determine whether a given index is a regular single byte or a 2 byte field.
 * Uses a boundless binary search for improved performance, but requires the fsIntIndex to remain in order
 *
 * @param key - Index in the log array to check
 * @return True if the index is a 2 byte log field. False if it is a single byte
 */
bool is2ByteEntry(uint8_t key)
{
  // This array indicates which index values from the log are 2 byte values
  // This array MUST remain in ascending order
  // !!!! WARNING: If any value above 255 is required in this array, changes MUST be made to is2ByteEntry() function !!!!
  static constexpr byte PROGMEM fsIntIndex[] = {4, 14, 17, 22, 26, 28, 33, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 76, 78, 80, 82, 86, 88, 90, 93, 95, 99, 104, 111, 121, 125, 130, 132, 134, 136 };

  unsigned int bot = 0U;
  unsigned int mid = _countof(fsIntIndex);

  while (mid > 1U)
  {
    if (key >= pgm_read_byte( &fsIntIndex[bot + mid / 2U]) )
    {
      bot += mid++ / 2U;
    }
    mid /= 2U;
  }

  return key == pgm_read_byte(&fsIntIndex[bot]);
}

//=============================================================================
// TOOTH LOGGER (Trigger wheel diagnostics)
//=============================================================================

/**
 * @brief Start tooth logger for primary+secondary trigger diagnostics
 *
 * @details Replaces normal trigger ISRs with logging versions that capture:
 * - Timestamp of each trigger edge
 * - Edge type (rising/falling)
 * - Trigger source (primary/secondary)
 *
 * Used for:
 * - Diagnosing missing teeth
 * - Verifying trigger wheel pattern
 * - Detecting electrical noise
 * - Confirming decoder operation
 *
 * @note Disables normal spark/fuel operation during logging
 * @note Limited buffer size - only captures ~1-2 engine revolutions
 * @note Results viewed in TunerStudio → Tools → Tooth Logger
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void startToothLogger(void)
{
  currentStatus.toothLogEnabled = true;
  currentStatus.compositeTriggerUsed = 0U; //Safety first (Should never be required)
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  toothHistoryIndex = 0U;

  //Disconnect the standard interrupt and add the logger version
  detachInterrupt( digitalPinToInterrupt(pinTrigger) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger), loggerPrimaryISR, CHANGE );

  if(VSS_USES_RPM2() != true)
  {
    detachInterrupt( digitalPinToInterrupt(pinTrigger2) );
    attachInterrupt( digitalPinToInterrupt(pinTrigger2), loggerSecondaryISR, CHANGE );
  }

}

/**
 * @brief Stop tooth logger and restore normal trigger ISRs
 *
 * @details Reconnects standard trigger handlers to resume normal operation.
 * Engine will restart spark/fuel scheduling after this call.
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void stopToothLogger(void)
{
  currentStatus.toothLogEnabled = false;

  //Disconnect the logger interrupts and attach the normal ones
  detachInterrupt( digitalPinToInterrupt(pinTrigger) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger), triggerHandler, primaryTriggerEdge );

  if(VSS_USES_RPM2() != true)
  {
    detachInterrupt( digitalPinToInterrupt(pinTrigger2) );
    attachInterrupt( digitalPinToInterrupt(pinTrigger2), triggerSecondaryHandler, secondaryTriggerEdge );
  }
}

/**
 * @brief Start composite trigger logger (combined primary+secondary pattern)
 *
 * @details For decoders that use composite trigger patterns (e.g., 36-1+1, GM 7X).
 * Logs both wheels as single combined pattern for pattern analysis.
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void startCompositeLogger(void)
{
  currentStatus.compositeTriggerUsed = 2U;
  currentStatus.toothLogEnabled = false; //Safety first (Should never be required)
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  toothHistoryIndex = 0U;

  //Disconnect the standard interrupt and add the logger version
  detachInterrupt( digitalPinToInterrupt(pinTrigger) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger), loggerPrimaryISR, CHANGE );

  if( (VSS_USES_RPM2() != true) && (FLEX_USES_RPM2() != true) )
  {
    detachInterrupt( digitalPinToInterrupt(pinTrigger2) );
    attachInterrupt( digitalPinToInterrupt(pinTrigger2), loggerSecondaryISR, CHANGE );
  }
}

/**
 * @brief Stop composite trigger logger and restore normal ISRs
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void stopCompositeLogger(void)
{
  currentStatus.compositeTriggerUsed = 0U;

  //Disconnect the logger interrupts and attach the normal ones
  detachInterrupt( digitalPinToInterrupt(pinTrigger) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger), triggerHandler, primaryTriggerEdge );

  if( (VSS_USES_RPM2() != true) && (FLEX_USES_RPM2() != true) )
  {
    detachInterrupt( digitalPinToInterrupt(pinTrigger2) );
    attachInterrupt( digitalPinToInterrupt(pinTrigger2), triggerSecondaryHandler, secondaryTriggerEdge );
  }
}

/**
 * @brief Start composite logger with tertiary trigger (3-wheel systems)
 *
 * @details For engines with 3 trigger inputs (e.g., some Toyota, Nissan VQ).
 * Logs primary + tertiary trigger patterns.
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void startCompositeLoggerTertiary(void)
{
  currentStatus.compositeTriggerUsed = 3U;
  currentStatus.toothLogEnabled = false; //Safety first (Should never be required)
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  toothHistoryIndex = 0U;

  //Disconnect the standard interrupt and add the logger version
  detachInterrupt( digitalPinToInterrupt(pinTrigger) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger), loggerPrimaryISR, CHANGE );

  detachInterrupt( digitalPinToInterrupt(pinTrigger3) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger3), loggerTertiaryISR, CHANGE );
}

/**
 * @brief Stop tertiary composite logger and restore normal ISRs
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void stopCompositeLoggerTertiary(void)
{
  currentStatus.compositeTriggerUsed = 0;

  //Disconnect the logger interrupts and attach the normal ones
  detachInterrupt( digitalPinToInterrupt(pinTrigger) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger), triggerHandler, primaryTriggerEdge );

  detachInterrupt( digitalPinToInterrupt(pinTrigger3) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger3), triggerTertiaryHandler, tertiaryTriggerEdge );
}

/**
 * @brief Start composite logger for camshaft position sensors only
 *
 * @details Logs secondary + tertiary triggers (camshaft sensors).
 * Used when diagnosing cam sync issues without crank trigger.
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void startCompositeLoggerCams(void)
{
  currentStatus.compositeTriggerUsed = 4;
  currentStatus.toothLogEnabled = false; //Safety first (Should never be required)
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  toothHistoryIndex = 0;

  //Disconnect the standard interrupt and add the logger version
  if( (VSS_USES_RPM2() != true) && (FLEX_USES_RPM2() != true) )
  {
    detachInterrupt( digitalPinToInterrupt(pinTrigger2) );
    attachInterrupt( digitalPinToInterrupt(pinTrigger2), loggerSecondaryISR, CHANGE );
  }

  detachInterrupt( digitalPinToInterrupt(pinTrigger3) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger3), loggerTertiaryISR, CHANGE );
}

/**
 * @brief Stop camshaft composite logger and restore normal ISRs
 *
 * @complexity 2 (ISR detach/attach operations)
 * @misra Compliant: Proper ISR handling
 */
void stopCompositeLoggerCams(void)
{
  currentStatus.compositeTriggerUsed = false;

  //Disconnect the logger interrupts and attach the normal ones
  if( (VSS_USES_RPM2() != true) && (FLEX_USES_RPM2() != true) )
  {
    detachInterrupt( digitalPinToInterrupt(pinTrigger2) );
    attachInterrupt( digitalPinToInterrupt(pinTrigger2), triggerSecondaryHandler, secondaryTriggerEdge );
  }

  detachInterrupt( digitalPinToInterrupt(pinTrigger3) );
  attachInterrupt( digitalPinToInterrupt(pinTrigger3), triggerTertiaryHandler, tertiaryTriggerEdge );
}
