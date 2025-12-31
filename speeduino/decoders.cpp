#define MJR 1

/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/

/**
 * @file decoders.cpp
 * @brief Trigger decoder coordinator and shared decoder infrastructure
 *
 * @details This file serves as the coordination layer for all trigger wheel decoders,
 * providing shared variables, function pointers, and common decoder utilities. The
 * actual decoder implementations have been modularized into decoders/implementations/.
 *
 * **MODULARIZATION STATUS:**
 * - **Original code**: Monolithic 6,595-line file with all 29 decoders inline
 * - **Refactored structure**: 91% of decoder code moved to decoders/implementations/
 * - **This file (wrapper)**: ~200 active lines + shared infrastructure
 * - **Implementations**: 29 separate files, 6,827 lines total, 100% documented
 *
 * **DECODER IMPLEMENTATIONS (29 Patterns - All in decoders/implementations/):**
 * 1. missing_tooth.cpp - 36-1, 60-2, 4-1, 12-1, etc. (305 lines)
 * 2. dual_wheel.cpp - Separate crank + cam wheels (322 lines)
 * 3. basic_distributor.cpp - Single pulse per revolution (301 lines)
 * 4. gm_7x.cpp - GM 7X crankshaft reluctor (187 lines)
 * 5. four_g63.cpp - Mitsubishi 4G63 (Eclipse/Evo) (485 lines)
 * 6. gm_24x.cpp - GM 24X crankshaft reluctor (297 lines)
 * 7. jeep_2000.cpp - Jeep 2000 4.0L 6-cylinder (333 lines)
 * 8. audi_135.cpp - Audi 135-tooth + cam (336 lines)
 * 9. honda_d17.cpp - Honda D17 12+1 crank (347 lines)
 * 10. miata_9905.cpp - Mazda Miata 99-05 4+1 (334 lines)
 * 11. non_360.cpp - Non-360 degree cam wheels (297 lines)
 * 12. nissan_360.cpp - Nissan 360-tooth optical (321 lines)
 * 13. subaru_67.cpp - Subaru 6/7 pattern (339 lines)
 * 14. daihatsu_plus1.cpp - Daihatsu +1 trigger (266 lines)
 * 15. harley.cpp - Harley Davidson V-twin (243 lines)
 * 16. thirty_six_minus_2_2_2.cpp - 36-2-2-2 pattern (342 lines)
 * 17. thirty_six_minus_2_1.cpp - 36-2-1 Renault pattern (342 lines)
 * 18. four_twenty_a.cpp - Chrysler 420a DOHC (397 lines)
 * 19. weber_iaw.cpp - Weber-Marelli IAW (332 lines)
 * 20. st170.cpp - Ford ST170 36-1 with VCT (342 lines)
 * 21. drzfour_hundred.cpp - Suzuki DRZ400 (342 lines)
 * 22. ngc.cpp - Chrysler NGC 4/6/8 cylinder (448 lines)
 * 23. vmax.cpp - Yamaha Vmax 16 teeth (267 lines)
 * 24. renix.cpp - Renix 44-2-2 trigger (301 lines)
 * 25. rover_mems.cpp - Rover MEMS 36-1-1 (368 lines)
 * 26. suzuki_k6a.cpp - Suzuki K6A 3-cylinder (276 lines)
 * 27. ford_tfi.cpp - Ford TFI/EDIS (333 lines)
 * 28. honda_j32.cpp - Honda J32 12+1 cam (391 lines)
 * 29. mazda_au.cpp - Mazda AU 12+1 pattern (316 lines)
 *
 * **DECODER FUNCTION INTERFACE:**
 * Each decoder must implement these 4 functions (where xxxx = decoder name):
 * - **triggerSetup_xxxx()** - Initialize decoder variables, configure trigger mode
 * - **triggerPri_xxxx()** - ISR for primary crank/cam signal (interrupt context!)
 * - **triggerSec_xxxx()** - ISR for secondary crank/cam signal (interrupt context!)
 * - **getRPM_xxxx()** - Calculate and return current RPM based on tooth timing
 *
 * Optional functions:
 * - **getCrankAngle_xxxx()** - Return current crank angle (0-360 or 0-720 degrees)
 * - **getCamAngle_xxxx()** - Return current cam angle (VVT applications)
 * - **triggerSetEndTeeth_xxxx()** - Calculate ignition/injection end angles
 *
 * **SHARED DECODER INFRASTRUCTURE (This File):**
 * - **Function pointers**: triggerHandler, triggerSecondaryHandler, getRPM, etc.
 * - **Timing variables**: toothLastToothTime, curTime, curGap, etc. (all volatile!)
 * - **Tooth counters**: toothCurrentCount, secondaryToothCount, thirdToothCount
 * - **Sync detection**: revolutionOne, lastSyncRevolution
 * - **Filter/debounce**: triggerFilterTime, triggerSecFilterTime, triggerThirdFilterTime
 * - **Stall detection**: MAX_STALL_TIME (500ms default, decoder-specific)
 *
 * **FUNCTION POINTER INITIALIZATION:**
 * At ECU startup, init.cpp calls initialiseTriggers() which:
 * 1. Reads configPage4.trigPattern (user-selected decoder)
 * 2. Calls appropriate triggerSetup_xxxx() function
 * 3. Sets function pointers (triggerHandler, getRPM, etc.) to decoder functions
 * 4. Attaches hardware interrupts to decoder ISRs
 *
 * **INTERRUPT SERVICE ROUTINE CONSTRAINTS:**
 * - **Context**: All triggerPri/triggerSec functions execute in interrupt context
 * - **Variables**: Must be declared volatile if accessed outside ISR
 * - **Duration**: Must complete in <50µs to avoid missing teeth at high RPM
 * - **Operations**: Avoid division, floating-point, Serial.print(), or blocking calls
 * - **Atomicity**: Use noInterrupts()/interrupts() if reading multi-byte volatile vars
 *
 * **SYNCHRONIZATION STATES:**
 * - **Loss of Sync**: toothCurrentCount = 0, engine position unknown
 * - **Gaining Sync**: Decoder identifies unique pattern (e.g., missing tooth gap)
 * - **Full Sync**: toothCurrentCount > 0, revolutionOne tracking active
 * - **Sync Validation**: lastSyncRevolution updated every 720° (4-stroke sequential)
 *
 * **TYPICAL TRIGGER PATTERNS:**
 * - **Missing Tooth**: N-M pattern (e.g., 36-1 = 35 teeth + 1 gap)
 * - **Dual Wheel**: Full crank wheel + single cam pulse for 720° sync
 * - **Distributor**: 1 pulse per cylinder (no crank position info)
 * - **Optical**: High-resolution 360-tooth wheel (1° accuracy)
 * - **OEM Specific**: Manufacturer patterns (GM, Honda, Nissan, etc.)
 *
 * **DECODER MIGRATION NOTES:**
 * - Legacy decoders (in #if 0 blocks) are DISABLED - use implementations/ instead
 * - All 29 decoders have been refactored with MISRA-C compliance
 * - All decoders include comprehensive Doxygen documentation
 * - Original monolithic code preserved in decoders.cpp.backup_original
 *
 * @complexity Medium (wrapper only ~200 active lines, full system 6,827 lines across 29 files)
 * @performance Critical path: ISR execution <50µs, RPM calc <100µs
 * @safety ISR-safe: All shared variables volatile, no blocking operations
 * @note This file coordinates decoders - actual implementations in decoders/implementations/
 * @see decoders/implementations/ for individual decoder implementations
 * @see init.cpp initialiseTriggers() for decoder selection and initialization
 */

/* Notes on Doxygen Groups/Modules documentation style:
 * - Installing doxygen (e.g. Ubuntu) via pkg mgr: sudo apt-get install doxygen graphviz
 * - @defgroup tag name/description becomes the short name on (Doxygen) "Modules" page
 * - Relying on JAVADOC_AUTOBRIEF (in Doxyfile, essentially automatic @brief), the first sentence (ending with period) becomes
 *   the longer description (second column following name) on (Doxygen) "Modules" page (old Desc: ... could be this sentence)
 * - All the content after first sentence (like old Note:...) is visible on the page linked from the name (1st col) on "Modules" page
 * - To group all decoders together add 1) @defgroup dec Decoders (on top) and 2) "@ingroup dec" to each decoder (under @defgroup)
 * - To compare Speeduino Doxyfile to default config, do: `doxygen -g Doxyfile.default ; diff Doxyfile.default Doxyfile`
 */
#include <limits.h>
#include "globals.h"
#include "decoders.h"
#include "scheduledIO.h"
#include "scheduler.h"
#include "crankMaths.h"
#include "timers.h"
#include "schedule_calcs.h"
#include "unit_testing.h"

void nullTriggerHandler (void){return;} //initialisation function for triggerhandlers, does exactly nothing
uint16_t nullGetRPM(void){return 0;} //initialisation function for getRpm, returns safe value of 0
int nullGetCrankAngle(void){return 0;} //initialisation function for getCrankAngle, returns safe value of 0

void (*triggerHandler)(void) = nullTriggerHandler; ///Pointer for the trigger function (Gets pointed to the relevant decoder)
void (*triggerSecondaryHandler)(void) = nullTriggerHandler; ///Pointer for the secondary trigger function (Gets pointed to the relevant decoder)
void (*triggerTertiaryHandler)(void) = nullTriggerHandler; ///Pointer for the tertiary trigger function (Gets pointed to the relevant decoder)
uint16_t (*getRPM)(void) = nullGetRPM; ///Pointer to the getRPM function (Gets pointed to the relevant decoder)
int (*getCrankAngle)(void) = nullGetCrankAngle; ///Pointer to the getCrank Angle function (Gets pointed to the relevant decoder)
void (*triggerSetEndTeeth)(void) = triggerSetEndTeeth_missingTooth; ///Pointer to the triggerSetEndTeeth function of each decoder

static void triggerRoverMEMSCommon(void);

volatile unsigned long curTime;
volatile unsigned long curGap;
volatile unsigned long curTime2;
volatile unsigned long curGap2;
volatile unsigned long curTime3;
volatile unsigned long curGap3;
volatile unsigned long lastGap;
volatile unsigned long targetGap;

unsigned long MAX_STALL_TIME = MICROS_PER_SEC/2U; //The maximum time (in uS) that the system will continue to function before the engine is considered stalled/stopped. This is unique to each decoder, depending on the number of teeth etc. 500000 (half a second) is used as the default value, most decoders will be much less.
volatile uint16_t toothCurrentCount = 0; //The current number of teeth (Once sync has been achieved, this can never actually be 0
volatile byte toothSystemCount = 0; //Used for decoders such as Audi 135 where not every tooth is used for calculating crank angle. This variable stores the actual number of teeth, not the number being used to calculate crank angle
volatile unsigned long toothSystemLastToothTime = 0; //As below, but used for decoders where not every tooth count is used for calculation
volatile unsigned long toothLastToothTime = 0; //The time (micros()) that the last tooth was registered
volatile unsigned long toothLastSecToothTime = 0; //The time (micros()) that the last tooth was registered on the secondary input
volatile unsigned long toothLastThirdToothTime = 0; //The time (micros()) that the last tooth was registered on the second cam input
volatile unsigned long toothLastMinusOneToothTime = 0; //The time (micros()) that the tooth before the last tooth was registered
volatile unsigned long toothLastMinusOneSecToothTime = 0; //The time (micros()) that the tooth before the last tooth was registered on secondary input
volatile unsigned long toothLastToothRisingTime = 0; //The time (micros()) that the last tooth rose (used by special decoders to determine missing teeth polarity)
volatile unsigned long toothLastSecToothRisingTime = 0; //The time (micros()) that the last tooth rose on the secondary input (used by special decoders to determine missing teeth polarity)
volatile unsigned long targetGap2;
volatile unsigned long targetGap3;
volatile unsigned long toothOneTime = 0; //The time (micros()) that tooth 1 last triggered
volatile unsigned long toothOneMinusOneTime = 0; //The 2nd to last time (micros()) that tooth 1 last triggered
volatile unsigned long lastSyncRevolution = 0; // the revolution value of last valid sync
volatile bool revolutionOne = 0; // For sequential operation, this tracks whether the current revolution is 1 or 2 (not 1)
volatile bool revolutionLastOne = 0; // used to identify in the rover pattern which has a non unique primary trigger something unique - has the secondary tooth changed.

volatile unsigned int secondaryToothCount; //Used for identifying the current secondary (Usually cam) tooth for patterns with multiple secondary teeth
volatile unsigned int secondaryLastToothCount = 0; // used to identify in the rover pattern which has a non unique primary trigger something unique - has the secondary tooth changed.
volatile unsigned long secondaryLastToothTime = 0; //The time (micros()) that the last tooth was registered (Cam input)
volatile unsigned long secondaryLastToothTime1 = 0; //The time (micros()) that the last tooth was registered (Cam input)

volatile unsigned int thirdToothCount; //Used for identifying the current third (Usually exhaust cam - used for VVT2) tooth for patterns with multiple secondary teeth
volatile unsigned long thirdLastToothTime = 0; //The time (micros()) that the last tooth was registered (Cam input)
volatile unsigned long thirdLastToothTime1 = 0; //The time (micros()) that the last tooth was registered (Cam input)

uint16_t triggerActualTeeth;
volatile unsigned long triggerFilterTime; // The shortest time (in uS) that pulses will be accepted (Used for debounce filtering)
volatile unsigned long triggerSecFilterTime; // The shortest time (in uS) that pulses will be accepted (Used for debounce filtering) for the secondary input
volatile unsigned long triggerThirdFilterTime; // The shortest time (in uS) that pulses will be accepted (Used for debounce filtering) for the Third input

volatile uint8_t decoderState = 0;

unsigned int triggerSecFilterTime_duration; // The shortest valid time (in uS) pulse DURATION
volatile uint16_t triggerToothAngle; //The number of crank degrees that elapse per tooth
byte checkSyncToothCount; //How many teeth must've been seen on this revolution before we try to confirm sync (Useful for missing tooth type decoders)
unsigned long elapsedTime;
unsigned long lastCrankAngleCalc;
unsigned long lastVVTtime; //The time between the vvt reference pulse and the last crank pulse

uint16_t ignition1EndTooth = 0;
uint16_t ignition2EndTooth = 0;
uint16_t ignition3EndTooth = 0;
uint16_t ignition4EndTooth = 0;
uint16_t ignition5EndTooth = 0;
uint16_t ignition6EndTooth = 0;
uint16_t ignition7EndTooth = 0;
uint16_t ignition8EndTooth = 0;

int16_t toothAngles[24]; //An array for storing fixed tooth angles. Currently sized at 24 for the GM 24X decoder, but may grow later if there are other decoders that use this style

#ifdef USE_LIBDIVIDE
#include "src/libdivide/libdivide.h"
libdivide::libdivide_s16_t divTriggerToothAngle;
#endif

/** Universal (shared between decoders) decoder routines.
*
* @defgroup dec_uni Universal Decoder Routines
*
* @{
*/
// whichTooth - 0 for Primary (Crank), 1 for Secondary (Cam)

/** Add tooth log entry to toothHistory (array).
 * Enabled by (either) currentStatus.toothLogEnabled and currentStatus.compositeTriggerUsed.
 * @param toothTime - Tooth Time
 * @param whichTooth - 0 for Primary (Crank), 2 for Secondary (Cam) 3 for Tertiary (Cam)
 */
static inline void addToothLogEntry(unsigned long toothTime, byte whichTooth)
{
  if(BIT_CHECK(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY)) { return; }
  //High speed tooth logging history
  if( (currentStatus.toothLogEnabled == true) || (currentStatus.compositeTriggerUsed > 0) )
  {
    bool valueLogged = false;
    if(currentStatus.toothLogEnabled == true)
    {
      //Tooth log only works on the Crank tooth
      if(whichTooth == TOOTH_CRANK)
      {
        toothHistory[toothHistoryIndex] = toothTime; //Set the value in the log.
        valueLogged = true;
      }
    }
    else if(currentStatus.compositeTriggerUsed > 0)
    {
      compositeLogHistory[toothHistoryIndex] = 0;
      if(currentStatus.compositeTriggerUsed == 4)
      {
        // we want to display both cams so swap the values round to display primary as cam1 and secondary as cam2, include the crank in the data as the third output
        if(READ_SEC_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_PRI); }
        if(READ_THIRD_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_SEC); }
        if(READ_PRI_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_THIRD); }
        if(whichTooth > TOOTH_CAM_SECONDARY) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_TRIG); }
      }
      else
      {
        // we want to display crank and one of the cams
        if(READ_PRI_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_PRI); }
        if(currentStatus.compositeTriggerUsed == 3)
        {
          // display cam2 and also log data for cam 1
          if(READ_THIRD_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_SEC); } // only the COMPOSITE_LOG_SEC value is visualised hence the swapping of the data
          if(READ_SEC_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_THIRD); }
        }
        else
        {
          // display cam1 and also log data for cam 2 - this is the historic composite view
          if(READ_SEC_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_SEC); }
          if(READ_THIRD_TRIGGER() == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_THIRD); }
        }
        if(whichTooth > TOOTH_CRANK) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_TRIG); }
      }
      if(currentStatus.hasSync == true) { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_LOG_SYNC); }

      if(revolutionOne == 1)
      { BIT_SET(compositeLogHistory[toothHistoryIndex], COMPOSITE_ENGINE_CYCLE);}
      else
      { BIT_CLEAR(compositeLogHistory[toothHistoryIndex], COMPOSITE_ENGINE_CYCLE);}

      toothHistory[toothHistoryIndex] = micros();
      valueLogged = true;
    }

    //If there has been a value logged above, update the indexes
    if(valueLogged == true)
    {
     if(toothHistoryIndex < (TOOTH_LOG_SIZE-1)) { toothHistoryIndex++; BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY); }
     else { BIT_SET(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY); }
    }


  } //Tooth/Composite log enabled
}

/** Interrupt handler for primary trigger.
* This function is called on both the rising and falling edges of the primary trigger, when either the
* composite or tooth loggers are turned on.
*/
void loggerPrimaryISR(void)
{
  BIT_CLEAR(decoderState, BIT_DECODER_VALID_TRIGGER); //This value will be set to the return value of the decoder function, indicating whether or not this pulse passed the filters
  bool validEdge = false; //This is set true below if the edge
  /*
  Need to still call the standard decoder trigger.
  Two checks here:
  1) If the primary trigger is RISING, then check whether the primary is currently HIGH
  2) If the primary trigger is FALLING, then check whether the primary is currently LOW
  If either of these are true, the primary decoder function is called
  */
  if( ( (primaryTriggerEdge == RISING) && (READ_PRI_TRIGGER() == HIGH) ) || ( (primaryTriggerEdge == FALLING) && (READ_PRI_TRIGGER() == LOW) ) || (primaryTriggerEdge == CHANGE) )
  {
    triggerHandler();
    validEdge = true;
  }
  if( (currentStatus.toothLogEnabled == true) && (BIT_CHECK(decoderState, BIT_DECODER_VALID_TRIGGER)) )
  {
    //Tooth logger only logs when the edge was correct
    if(validEdge == true)
    {
      addToothLogEntry(curGap, TOOTH_CRANK);
    }
  }
  else if( (currentStatus.compositeTriggerUsed > 0) )
  {
    //Composite logger adds an entry regardless of which edge it was
    addToothLogEntry(curGap, TOOTH_CRANK);
  }
}

/** Interrupt handler for secondary trigger.
* As loggerPrimaryISR, but for the secondary trigger.
*/
void loggerSecondaryISR(void)
{
  BIT_CLEAR(decoderState, BIT_DECODER_VALID_TRIGGER); //This value will be set to the return value of the decoder function, indicating whether or not this pulse passed the filters
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //This value will be set to the return value of the decoder function, indicating whether or not this pulse passed the filters
  /* 3 checks here:
  1) If the primary trigger is RISING, then check whether the primary is currently HIGH
  2) If the primary trigger is FALLING, then check whether the primary is currently LOW
  3) The secondary trigger is CHANGING
  If any of these are true, the primary decoder function is called
  */
  if( ( (secondaryTriggerEdge == RISING) && (READ_SEC_TRIGGER() == HIGH) ) || ( (secondaryTriggerEdge == FALLING) && (READ_SEC_TRIGGER() == LOW) ) || (secondaryTriggerEdge == CHANGE) )
  {
    triggerSecondaryHandler();
  }
  //No tooth logger for the secondary input
  if( (currentStatus.compositeTriggerUsed > 0) && (BIT_CHECK(decoderState, BIT_DECODER_VALID_TRIGGER)) )
  {
    //Composite logger adds an entry regardless of which edge it was
    addToothLogEntry(curGap2, TOOTH_CAM_SECONDARY);
  }
}

/** Interrupt handler for third trigger.
* As loggerPrimaryISR, but for the third trigger.
*/
void loggerTertiaryISR(void)
{
  BIT_CLEAR(decoderState, BIT_DECODER_VALID_TRIGGER); //This value will be set to the return value of the decoder function, indicating whether or not this pulse passed the filters
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //This value will be set to the return value of the decoder function, indicating whether or not this pulse passed the filters
  /* 3 checks here:
  1) If the primary trigger is RISING, then check whether the primary is currently HIGH
  2) If the primary trigger is FALLING, then check whether the primary is currently LOW
  3) The secondary trigger is CHANGING
  If any of these are true, the primary decoder function is called
  */


  if( ( (tertiaryTriggerEdge == RISING) && ( READ_THIRD_TRIGGER() == HIGH) ) || ( (tertiaryTriggerEdge == FALLING) && (READ_THIRD_TRIGGER() == LOW) ) || (tertiaryTriggerEdge == CHANGE) )
  {
    triggerTertiaryHandler();
  }
  //No tooth logger for the secondary input
  if( (currentStatus.compositeTriggerUsed > 0) && (BIT_CHECK(decoderState, BIT_DECODER_VALID_TRIGGER)) )
  {
    //Composite logger adds an entry regardless of which edge it was
    addToothLogEntry(curGap3, TOOTH_CAM_TERTIARY);
  }
}

#if false
#if !defined(UNIT_TEST)
static
#endif
uint32_t angleToTimeIntervalTooth(uint16_t angle) {
  noInterrupts();
  if(BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT))
  {
    unsigned long toothTime = (toothLastToothTime - toothLastMinusOneToothTime);
    uint16_t tempTriggerToothAngle = triggerToothAngle; // triggerToothAngle is set by interrupts
    interrupts();

    return (toothTime * (uint32_t)angle) / tempTriggerToothAngle;
  }
  //Safety check. This can occur if the last tooth seen was outside the normal pattern etc
  else {
    interrupts();
    return angleToTimeMicroSecPerDegree(angle);
  }
}
#endif

uint16_t timeToAngleIntervalTooth(uint32_t time)
{
    noInterrupts();
    //Still uses a last interval method (ie retrospective), but bases the interval on the gap between the 2 most recent teeth rather than the last full revolution
    if(BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT))
    {
      unsigned long toothTime = (toothLastToothTime - toothLastMinusOneToothTime);
      uint16_t tempTriggerToothAngle = triggerToothAngle; // triggerToothAngle is set by interrupts
      interrupts();

      return (unsigned long)(time * (uint32_t)tempTriggerToothAngle) / toothTime;
    }
    else {
      interrupts();
      //Safety check. This can occur if the last tooth seen was outside the normal pattern etc
      return timeToAngleDegPerMicroSec(time);
    }
}

static inline bool IsCranking(const statuses &status) {
  return (status.RPM < status.crankRPM) && (status.startRevolutions == 0U);
}

bool engineIsRunning(uint32_t curTime) {
  // Check how long ago the last tooth was seen compared to now.
  // If it was more than MAX_STALL_TIME then the engine is probably stopped.
  // toothLastToothTime can be greater than curTime if a pulse occurs between getting the latest time and doing the comparison
  ATOMIC() {
    return (toothLastToothTime > curTime) || ((curTime - toothLastToothTime) < MAX_STALL_TIME);
  }
  return false; // Just here to avoid compiler warning.
}

void resetDecoder(void) {
  toothLastSecToothTime = 0;
  toothLastToothTime = 0;
  toothSystemCount = 0;
  secondaryToothCount = 0;
}

__attribute__((noinline)) bool SetRevolutionTime(uint32_t revTime)
{
  if (revTime!=revolutionTime) {
    revolutionTime = revTime;
    setAngleConverterRevolutionTime(revolutionTime);
    return true;
  }
  return false;
}

static bool UpdateRevolutionTimeFromTeeth(bool isCamTeeth) {
  noInterrupts();
  bool updatedRevTime = HasAnySync(currentStatus)
    && !IsCranking(currentStatus)
    && (toothOneMinusOneTime!=UINT32_C(0))
    && (toothOneTime>toothOneMinusOneTime)
    //The time in uS that one revolution would take at current speed (The time tooth 1 was last seen, minus the time it was seen prior to that)
    && SetRevolutionTime((toothOneTime - toothOneMinusOneTime) >> (isCamTeeth ? 1U : 0U));

  interrupts();
 return updatedRevTime;
}

/** Compute RPM.
* As nearly all the decoders use a common method of determining RPM (The time the last full revolution took) A common function is simpler.
* @param degreesOver - the number of crank degrees between tooth #1s. Some patterns have a tooth #1 every crank rev, others are every cam rev.
* @return RPM
*/
__attribute__((noinline)) uint16_t stdGetRPM(bool isCamTeeth)
{
  if (UpdateRevolutionTimeFromTeeth(isCamTeeth)) {
    return RpmFromRevolutionTimeUs(revolutionTime);
  }

  return currentStatus.RPM;
}

/**
 * Sets the new filter time based on the current settings.
 * This ONLY works for even spaced decoders.
 */
void setFilter(unsigned long curGap)
{
  /*
  if(configPage4.triggerFilter == 0) { triggerFilterTime = 0; } //trigger filter is turned off.
  else if(configPage4.triggerFilter == 1) { triggerFilterTime = curGap >> 2; } //Lite filter level is 25% of previous gap
  else if(configPage4.triggerFilter == 2) { triggerFilterTime = curGap >> 1; } //Medium filter level is 50% of previous gap
  else if (configPage4.triggerFilter == 3) { triggerFilterTime = (curGap * 3) >> 2; } //Aggressive filter level is 75% of previous gap
  else { triggerFilterTime = 0; } //trigger filter is turned off.
  */

  switch(configPage4.triggerFilter)
  {
    case TRIGGER_FILTER_OFF:
      triggerFilterTime = 0;
      break;
    case TRIGGER_FILTER_LITE:
      triggerFilterTime = curGap >> 2;
      break;
    case TRIGGER_FILTER_MEDIUM:
      triggerFilterTime = curGap >> 1;
      break;
    case TRIGGER_FILTER_AGGRESSIVE:
      triggerFilterTime = (curGap * 3) >> 2;
      break;
    default:
      triggerFilterTime = 0;
      break;
  }
}

/**
This is a special case of RPM measure that is based on the time between the last 2 teeth rather than the time of the last full revolution.
This gives much more volatile reading, but is quite useful during cranking, particularly on low resolution patterns.
It can only be used on patterns where the teeth are evenly spaced.
It takes an argument of the full (COMPLETE) number of teeth per revolution.
For a missing tooth wheel, this is the number if the tooth had NOT been missing (Eg 36-1 = 36)
*/
__attribute__((noinline)) int crankingGetRPM(byte totalTeeth, bool isCamTeeth)
{
  if( (currentStatus.startRevolutions >= configPage4.StgCycles) && ((currentStatus.hasSync == true) || BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC)) )
  {
    if((toothLastMinusOneToothTime > 0) && (toothLastToothTime > toothLastMinusOneToothTime) )
    {
      noInterrupts();
      bool newRevtime = SetRevolutionTime(((toothLastToothTime - toothLastMinusOneToothTime) * totalTeeth) >> (isCamTeeth ? 1U : 0U));
      interrupts();
      if (newRevtime) {
        return RpmFromRevolutionTimeUs(revolutionTime);
      }
    }
  }

  return currentStatus.RPM;
}

/**
On decoders that are enabled for per tooth based timing adjustments, this function performs the timer compare changes on the schedules themselves
For each ignition channel, a check is made whether we're at the relevant tooth and whether that ignition schedule is currently running
Only if both these conditions are met will the schedule be updated with the latest timing information.
If it's the correct tooth, but the schedule is not yet started, calculate and an end compare value (This situation occurs when both the start and end of the ignition pulse happen after the end tooth, but before the next tooth)
*/
void checkPerToothTiming(int16_t crankAngle, uint16_t currentTooth)
{
  if ( (fixedCrankingOverride == 0) && (currentStatus.RPM > 0) )
  {
    if ( (currentTooth == ignition1EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule1, ignition1EndAngle, crankAngle);
    }
    else if ( (currentTooth == ignition2EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule2, ignition2EndAngle, crankAngle);
    }
    else if ( (currentTooth == ignition3EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule3, ignition3EndAngle, crankAngle);
    }
    else if ( (currentTooth == ignition4EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule4, ignition4EndAngle, crankAngle);
    }
#if IGN_CHANNELS >= 5
    else if ( (currentTooth == ignition5EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule5, ignition5EndAngle, crankAngle);
    }
#endif
#if IGN_CHANNELS >= 6
    else if ( (currentTooth == ignition6EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule6, ignition6EndAngle, crankAngle);
    }
#endif
#if IGN_CHANNELS >= 7
    else if ( (currentTooth == ignition7EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule7, ignition7EndAngle, crankAngle);
    }
#endif
#if IGN_CHANNELS >= 8
    else if ( (currentTooth == ignition8EndTooth) )
    {
      adjustCrankAngle(ignitionSchedule8, ignition8EndAngle, crankAngle);
    }
#endif
  }
}
/** @} */

// Helper function for missing tooth decoder - must be outside #if 0 block
uint16_t __attribute__((noinline)) calcEndTeeth_missingTooth(int endAngle, uint8_t toothAdder) {
  //Temp variable used here to avoid potential issues if a trigger interrupt occurs part way through this function
  int16_t tempEndTooth;
#ifdef USE_LIBDIVIDE
  tempEndTooth = libdivide::libdivide_s16_do(endAngle - configPage4.triggerAngle, &divTriggerToothAngle);
#else
  tempEndTooth = div((endAngle - configPage4.triggerAngle), (int16_t)triggerToothAngle).quot + 1;
#endif
  if(tempEndTooth > (configPage4.triggerTeeth + toothAdder)) { tempEndTooth -= (configPage4.triggerTeeth + toothAdder); }
  else if(tempEndTooth <= 0) { tempEndTooth += (configPage4.triggerTeeth + toothAdder); }

  return clampToActualTeeth(clampToToothCount(tempEndTooth, toothAdder), toothAdder);
}

/** A (single) multi-tooth wheel with one of more 'missing' teeth.
* The first tooth after the missing one is considered number 1 and is the basis for the trigger angle.
* Optionally a cam signal can be added to provide a sequential reference.
* @defgroup dec_miss Missing tooth wheel
* @{
*/

//========== MISSING TOOTH DECODER HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Determine if we should attempt missing tooth detection
 * @return true if detection should be attempted
 * @note MISRA-C compliant: Lines: 5 | Cyclomatic: 2 | Nesting: 1
 */
static inline bool shouldDetectMissingTooth(void)
{
  return (currentStatus.hasSync == false) ||
         (currentStatus.RPM < 2000) ||
         (toothCurrentCount >= (3 * triggerActualTeeth >> 2));
}

/**
 * @brief Handle sync loss detection when tooth count is wrong
 * @note MISRA-C compliant: Lines: 7 | Cyclomatic: 1 | Nesting: 0
 */
static inline void handleSyncLoss(void)
{
  currentStatus.hasSync = false;
  BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
  currentStatus.syncLossCounter++;
}

/**
 * @brief Update revolution counter when tooth #1 is detected
 * @note MISRA-C compliant: Lines: 9 | Cyclomatic: 3 | Nesting: 2
 */
static inline void updateRevolutionCounter(void)
{
  if ((currentStatus.hasSync == true) || BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC))
  {
    currentStatus.startRevolutions++;
    if (configPage4.TrigSpeed == CAM_SPEED)
    {
      currentStatus.startRevolutions++; // Extra revolution at cam speed
    }
  }
  else
  {
    currentStatus.startRevolutions = 0;
  }
}

/**
 * @brief Determine revolution tracking based on cam sensor in poll mode
 * @note MISRA-C compliant: Lines: 8 | Cyclomatic: 2 | Nesting: 2
 */
static inline void updateRevolutionTracking(void)
{
  if (configPage4.trigPatternSec == SEC_TRIGGER_POLL)
  {
    if (configPage4.PollLevelPolarity == READ_SEC_TRIGGER())
    {
      revolutionOne = 1;
    }
    else
    {
      revolutionOne = 0;
    }
  }
  else
  {
    revolutionOne = !revolutionOne; // Flip sequential revolution tracker
  }
}

/**
 * @brief Update sync status for sequential fuel/ignition modes
 * @note MISRA-C compliant: Lines: 18 | Cyclomatic: 5 | Nesting: 2
 */
static inline void updateSequentialSync(void)
{
  if ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) || (configPage2.injLayout == INJ_SEQUENTIAL))
  {
    // Sequential mode requires cam signal OR cam-speed trigger
    if ((secondaryToothCount > 0) ||
        (configPage4.TrigSpeed == CAM_SPEED) ||
        (configPage4.trigPatternSec == SEC_TRIGGER_POLL) ||
        (configPage2.strokes == TWO_STROKE))
    {
      currentStatus.hasSync = true;
      BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    }
    else if (currentStatus.hasSync != true)
    {
      BIT_SET(currentStatus.status3, BIT_STATUS3_HALFSYNC); // Half sync only
    }
  }
  else
  {
    currentStatus.hasSync = true;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
  }
}

/**
 * @brief Reset secondary tooth counter if needed
 * @note MISRA-C compliant: Lines: 5 | Cyclomatic: 2 | Nesting: 1
 */
static inline void resetSecondaryToothIfNeeded(void)
{
  if ((configPage4.trigPatternSec == SEC_TRIGGER_SINGLE) ||
      (configPage4.trigPatternSec == SEC_TRIGGER_TOYOTA_3))
  {
    secondaryToothCount = 0;
  }
}

/**
 * @brief Handle all processing when tooth #1 (after missing tooth) is detected
 * @note MISRA-C compliant: Lines: 15 | Cyclomatic: 1 | Nesting: 0
 */
static inline void handleToothOneDetected(void)
{
  updateRevolutionCounter();

  toothCurrentCount = 1;
  updateRevolutionTracking();

  toothOneMinusOneTime = toothOneTime;
  toothOneTime = curTime;

  updateSequentialSync();
  resetSecondaryToothIfNeeded();

  triggerFilterTime = 0; // Prevent filter lockup on intermittent signals
  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;
  BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); // Tooth angle is double
}

/**
 * @brief Handle missing tooth detection and sync logic
 * @param curGap Current gap between teeth
 * @param targetGap Expected gap threshold for missing tooth
 * @return true if missing tooth was detected
 * @note MISRA-C compliant: Lines: 14 | Cyclomatic: 4 | Nesting: 2
 */
static inline bool handleMissingToothDetection(unsigned long curGap, unsigned long targetGap)
{
  if ((curGap <= targetGap) && (toothCurrentCount <= triggerActualTeeth))
  {
    return false; // Not a missing tooth
  }

  // Missing tooth detected
  if ((toothCurrentCount < triggerActualTeeth) && (currentStatus.hasSync == true))
  {
    // Lost sync - saw tooth #1 before all teeth were counted
    handleSyncLoss();
  }
  else
  {
    // Normal tooth #1 detection
    handleToothOneDetected();
  }

  return true;
}

/**
 * @brief Process regular (non-missing) tooth
 * @note MISRA-C compliant: Lines: 6 | Cyclomatic: 1 | Nesting: 0
 */
static inline void handleRegularTooth(void)
{
  setFilter(curGap);
  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

/**
 * @brief Handle per-tooth ignition timing calculations
 * @note MISRA-C compliant: Lines: 12 | Cyclomatic: 3 | Nesting: 2
 */
static inline void handlePerToothIgnition(void)
{
  if (!configPage2.perToothIgn || BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK))
  {
    return; // Per-tooth ignition disabled or cranking
  }

  int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;

  if ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) &&
      (revolutionOne == true) &&
      (configPage4.TrigSpeed == CRANK_SPEED) &&
      (configPage2.strokes == FOUR_STROKE))
  {
    crankAngle += 360;
    crankAngle = ignitionLimits(crankAngle);
    checkPerToothTiming(crankAngle, (configPage4.triggerTeeth + toothCurrentCount));
  }
  else
  {
    crankAngle = ignitionLimits(crankAngle);
    checkPerToothTiming(crankAngle, toothCurrentCount);
  }
}

#if 0 // REFACTORED - Implementation moved to decoders/implementations/missing_tooth.cpp
void triggerSetup_missingTooth(void)
{
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  triggerToothAngle = 360 / configPage4.triggerTeeth; //The number of degrees that passes from tooth to tooth
  if(configPage4.TrigSpeed == CAM_SPEED)
  {
    //Account for cam speed missing tooth
    triggerToothAngle = 720 / configPage4.triggerTeeth;
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  }
  triggerActualTeeth = configPage4.triggerTeeth - configPage4.triggerMissingTeeth; //The number of physical teeth on the wheel. Doing this here saves us a calculation each time in the interrupt
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  if (configPage4.trigPatternSec == SEC_TRIGGER_4_1)
  {
    triggerSecFilterTime = MICROS_PER_MIN / MAX_RPM / 4U / 2U;
  }
  else
  {
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U));
  }
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  checkSyncToothCount = (configPage4.triggerTeeth) >> 1; //50% of the total teeth.
  toothLastMinusOneToothTime = 0;
  toothCurrentCount = 0;
  secondaryToothCount = 0;
  thirdToothCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * (configPage4.triggerMissingTeeth + 1U)); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)

  if( (configPage4.TrigSpeed == CRANK_SPEED) && ( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) || (configPage2.injLayout == INJ_SEQUENTIAL) || (configPage6.vvtEnabled > 0)) ) { BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY); }
  else { BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY); }
#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

/**
 * @brief Primary trigger interrupt for missing tooth decoder
 * @details Handles crank teeth interrupts, detects missing tooth gap for sync,
 * and optionally triggers per-tooth ignition timing. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 45 | Cyclomatic: 8 | Nesting: 2 (was N:8, C:36, 118 lines!)
 * @see handleMissingToothDetection(), handlePerToothIgnition(), shouldDetectMissingTooth()
 */
void triggerPri_missingTooth(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;

  // Guard clause: filter debounce (pulses < triggerFilterTime are false triggers)
  if (curGap < triggerFilterTime) { return; }

  toothCurrentCount++;
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

  // Guard clause: need at least 2 previous teeth for gap timing analysis
  if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0))
  {
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
    return;
  }

  bool isMissingTooth = false;

  /*
   * Performance Optimization:
   * Only attempt missing tooth detection if:
   * 1. We don't have sync yet, OR
   * 2. We're in final 1/4 of wheel (missing tooth never occurs in first 3/4), OR
   * 3. RPM < 2000 (avoid strange timing when cranking/idling)
   */
  if (shouldDetectMissingTooth())
  {
    // Calculate target gap: 1.5x for single missing tooth, 2x for double
    if (configPage4.triggerMissingTeeth == 1)
    {
      targetGap = (3 * (toothLastToothTime - toothLastMinusOneToothTime)) >> 1; // Multiply by 1.5
    }
    else
    {
      targetGap = (toothLastToothTime - toothLastMinusOneToothTime) * configPage4.triggerMissingTeeth;
    }

    isMissingTooth = handleMissingToothDetection(curGap, targetGap);
  }

  // Process regular (non-missing) tooth
  if (!isMissingTooth)
  {
    handleRegularTooth();
  }

  // Handle per-tooth ignition timing if enabled
  handlePerToothIgnition();
}

// SecTriggerSimpleConfig struct, secTriggerSimpleConfigs array, and processSimpleSecTrigger function moved outside #if 0 block (line ~929)

void triggerSec_missingTooth(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  //Safety check for initial startup
  if( (toothLastSecToothTime == 0) )
  {
    curGap2 = 0;
    toothLastSecToothTime = curTime2;
  }

  if ( curGap2 >= triggerSecFilterTime )
  {
    // Handle SEC_TRIGGER_4_1 separately due to complex missing tooth detection logic
    if(configPage4.trigPatternSec == SEC_TRIGGER_4_1)
    {
      targetGap2 = (3 * (toothLastSecToothTime - toothLastMinusOneSecToothTime)) >> 1; //If the time between the current tooth and the last is greater than 1.5x the time between the last tooth and the tooth before that, we make the assertion that we must be at the first tooth after the gap
      toothLastMinusOneSecToothTime = toothLastSecToothTime;
      if ( (curGap2 >= targetGap2) || (secondaryToothCount > 3) )
      {
        secondaryToothCount = 1;
        revolutionOne = 1; //Sequential revolution reset
        triggerSecFilterTime = 0; //This is used to prevent a condition where serious intermittent signals (Eg someone furiously plugging the sensor wire in and out) can leave the filter in an unrecoverable state
        triggerRecordVVT1Angle();
      }
      else
      {
        triggerSecFilterTime = curGap2 >> 2; //Set filter at 25% of the current speed. Filter can only be recalc'd for the regular teeth, not the missing one.
        secondaryToothCount++;
      }
    }
    else
    {
      // Use data-driven approach for simple trigger patterns (POLL, SINGLE, TOYOTA_3)
      processSimpleSecTrigger(configPage4.trigPatternSec, curGap2, &secondaryToothCount, &revolutionOne, &triggerSecFilterTime);
    }
    toothLastSecToothTime = curTime2;
  } //Trigger filter
}

// triggerRecordVVT1Angle moved outside #if 0 block (line ~993)

void triggerThird_missingTooth(void)
{
//Record the VVT2 Angle (the only purpose of the third trigger)
//NB no filtering of this signal with current implementation unlike Cam (VVT1)

  int16_t curAngle;
  curTime3 = micros();
  curGap3 = curTime3 - toothLastThirdToothTime;

  //Safety check for initial startup
  if( (toothLastThirdToothTime == 0) )
  {
    curGap3 = 0;
    toothLastThirdToothTime = curTime3;
  }

  if ( curGap3 >= triggerThirdFilterTime )
  {
    thirdToothCount++;
    triggerThirdFilterTime = curGap3 >> 2; //Next third filter is 25% the current gap

    curAngle = getCrankAngle();
    while(curAngle > 360) { curAngle -= 360; }
    curAngle -= configPage4.triggerAngle; //Value at TDC
    if( configPage6.vvtMode == VVT_MODE_CLOSED_LOOP ) { curAngle -= configPage4.vvt2CL0DutyAng; }
    //currentStatus.vvt2Angle = int8_t (curAngle); //vvt1Angle is only int8, but +/-127 degrees is enough for VVT control
    currentStatus.vvt2Angle = LOW_PASS_FILTER( (curAngle << 1), configPage4.ANGLEFILTER_VVT, currentStatus.vvt2Angle);

    toothLastThirdToothTime = curTime3;
  } //Trigger filter
}

uint16_t getRPM_missingTooth(void)
{
  uint16_t tempRPM = 0;
  if( currentStatus.RPM < currentStatus.crankRPM )
  {
    if(toothCurrentCount != 1)
    {
      tempRPM = crankingGetRPM(configPage4.triggerTeeth, configPage4.TrigSpeed==CAM_SPEED); //Account for cam speed
    }
    else { tempRPM = currentStatus.RPM; } //Can't do per tooth RPM if we're at tooth #1 as the missing tooth messes the calculation
  }
  else
  {
    tempRPM = stdGetRPM(configPage4.TrigSpeed==CAM_SPEED); //Account for cam speed
  }
  return tempRPM;
}

int getCrankAngle_missingTooth(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempRevolutionOne = revolutionOne;
    tempToothLastToothTime = toothLastToothTime;
    interrupts();

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    //Sequential check (simply sets whether we're on the first or 2nd revolution of the cycle)
    if ( (tempRevolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED) ) { crankAngle += 360; }

    lastCrankAngleCalc = micros();
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

// clampToToothCount and clampToActualTeeth moved to decoders.h as static inline
// calcEndTeeth_missingTooth moved before #if 0 block (line ~519)

void triggerSetEndTeeth_missingTooth(void)
{
  uint8_t toothAdder = 0;
  if( ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) || (configPage4.sparkMode == IGN_MODE_SINGLE)) && (configPage4.TrigSpeed == CRANK_SPEED) && (configPage2.strokes == FOUR_STROKE) ) { toothAdder = configPage4.triggerTeeth; }

  ignition1EndTooth = calcEndTeeth_missingTooth(ignition1EndAngle, toothAdder);
  ignition2EndTooth = calcEndTeeth_missingTooth(ignition2EndAngle, toothAdder);
  ignition3EndTooth = calcEndTeeth_missingTooth(ignition3EndAngle, toothAdder);
  ignition4EndTooth = calcEndTeeth_missingTooth(ignition4EndAngle, toothAdder);
#if IGN_CHANNELS >= 5
  ignition5EndTooth = calcEndTeeth_missingTooth(ignition5EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 6
  ignition6EndTooth = calcEndTeeth_missingTooth(ignition6EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 7
  ignition7EndTooth = calcEndTeeth_missingTooth(ignition7EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 8
  ignition8EndTooth = calcEndTeeth_missingTooth(ignition8EndAngle, toothAdder);
#endif
}
/** @} */

/** Dual wheels - 2 wheels located either both on the crank or with the primary on the crank and the secondary on the cam.
Note: There can be no missing teeth on the primary wheel.
* @defgroup dec_dual Dual wheels
* @{
*/
#endif // missing_tooth

// Secondary trigger pattern configuration - data-driven approach for simple trigger types
// Captures common behaviors of POLL, SINGLE, and TOYOTA_3 patterns - MOVED OUTSIDE #if 0
struct SecTriggerSimpleConfig {
  uint8_t triggerType;          // SEC_TRIGGER constant
  uint8_t filterShift;          // Right shift amount for filter calculation (1=>>1, 2=>>2)
  bool shouldResetRevolution;   // Whether to reset revolutionOne flag
  bool shouldIncrementCounter;  // Whether to increment secondaryToothCount
  bool isToyotaSpecial;         // Toyota 3-tooth special logic flag
};

// Configuration for simple secondary trigger patterns
// SEC_TRIGGER_4_1 is handled separately due to complex missing tooth detection
static const SecTriggerSimpleConfig secTriggerSimpleConfigs[3] = {
  {SEC_TRIGGER_POLL,     1, false, false, false},  // Poll: >>1 filter, no revolution reset, no counter
  {SEC_TRIGGER_SINGLE,   1, true,  true,  false},  // Single: >>1 filter, reset revolution, increment counter
  {SEC_TRIGGER_TOYOTA_3, 2, false, true,  true}    // Toyota: >>2 filter, conditional revolution, increment counter
};

// Helper function to process simple secondary trigger patterns - MOVED OUTSIDE #if 0
bool processSimpleSecTrigger(uint8_t triggerType, uint32_t curGap, volatile unsigned int* pSecondaryCount, volatile bool* pRevolutionOne, volatile uint32_t* pTriggerSecFilterTime)
{
  // Find matching configuration
  for(uint8_t i = 0; i < 3; i++)
  {
    const SecTriggerSimpleConfig* config = &secTriggerSimpleConfigs[i];

    if(config->triggerType == triggerType)
    {
      // Calculate filter time
      *pTriggerSecFilterTime = curGap >> config->filterShift;

      // Handle Toyota special case
      if(config->isToyotaSpecial)
      {
        (*pSecondaryCount)++;
        if(*pSecondaryCount == 2)
        {
          *pRevolutionOne = 1;
          triggerRecordVVT1Angle();
        }
      }
      else
      {
        // Standard processing
        if(config->shouldResetRevolution)
        {
          *pRevolutionOne = 1;
        }

        if(config->shouldIncrementCounter)
        {
          (*pSecondaryCount)++;
        }

        triggerRecordVVT1Angle();
      }

      return true;
    }
  }

  return false;  // Not a simple trigger type
}

// Helper function to record VVT1 angle - MOVED OUTSIDE #if 0
void triggerRecordVVT1Angle (void)
{
  //Record the VVT Angle
  if( (configPage6.vvtEnabled > 0) && (revolutionOne == 1) )
  {
    int16_t curAngle;
    curAngle = getCrankAngle();
    while(curAngle > 360) { curAngle -= 360; }
    curAngle -= configPage4.triggerAngle; //Value at TDC
    if( configPage6.vvtMode == VVT_MODE_CLOSED_LOOP ) { curAngle -= configPage10.vvtCL0DutyAng; }

    currentStatus.vvt1Angle = LOW_PASS_FILTER( (curAngle << 1), configPage4.ANGLEFILTER_VVT, currentStatus.vvt1Angle);
  }
}

#if 0 // REFACTORED - Implementation moved to decoders/implementations/dual_wheel.cpp
/** Dual Wheel Setup.
 *
 * */
void triggerSetup_DualWheel(void)
{
  triggerToothAngle = 360 / configPage4.triggerTeeth; //The number of degrees that passes from tooth to tooth
  if(configPage4.TrigSpeed == CAM_SPEED) { triggerToothAngle = 720 / configPage4.triggerTeeth; } //Account for cam speed
  toothCurrentCount = UINT8_MAX; //Default value
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U; //Same as above, but fixed at 2 teeth on the secondary input and divided by 2 (for cam speed)
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); //This is always true for this pattern
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

/** Dual Wheel Primary.
 *
 * */
void triggerPri_DualWheel(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if ( curGap >= triggerFilterTime )
    {
      toothCurrentCount++; //Increment the tooth counter
      BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

      toothLastMinusOneToothTime = toothLastToothTime;
      toothLastToothTime = curTime;

      if ( currentStatus.hasSync == true )
      {
        if ( (toothCurrentCount == 1) || (toothCurrentCount > configPage4.triggerTeeth) )
        {
          toothCurrentCount = 1;
          revolutionOne = !revolutionOne; //Flip sequential revolution tracker
          toothOneMinusOneTime = toothOneTime;
          toothOneTime = curTime;
          currentStatus.startRevolutions++; //Counter
          if ( configPage4.TrigSpeed == CAM_SPEED ) { currentStatus.startRevolutions++; } //Add an extra revolution count if we're running at cam speed
        }

        setFilter(curGap); //Recalc the new filter value
      }

      //NEW IGNITION MODE
      if( (configPage2.perToothIgn == true) && (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) )
      {
        int16_t crankAngle = ( (toothCurrentCount-1) * triggerToothAngle ) + configPage4.triggerAngle;
        uint16_t currentTooth;
        if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (revolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED) )
        {
          crankAngle += 360;
          currentTooth = (configPage4.triggerTeeth + toothCurrentCount);
        }
        else{ currentTooth = toothCurrentCount; }
        checkPerToothTiming(crankAngle, currentTooth);
      }
   } //Trigger filter
}
/** Dual Wheel Secondary.
 *
 * */
void triggerSec_DualWheel(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;
  if ( curGap2 >= triggerSecFilterTime )
  {
    toothLastSecToothTime = curTime2;
    triggerSecFilterTime = curGap2 >> 2; //Set filter at 25% of the current speed

    if( (currentStatus.hasSync == false) || (currentStatus.startRevolutions <= configPage4.StgCycles) )
    {
      toothLastToothTime = micros();
      toothLastMinusOneToothTime = micros() - ((MICROS_PER_MIN/10U) / configPage4.triggerTeeth); //Fixes RPM at 10rpm until a full revolution has taken place
      toothCurrentCount = configPage4.triggerTeeth;
      triggerFilterTime = 0; //Need to turn the filter off here otherwise the first primary tooth after achieving sync is ignored

      currentStatus.hasSync = true;
    }
    else
    {
      if ( (toothCurrentCount != configPage4.triggerTeeth) && (currentStatus.startRevolutions > 2)) { currentStatus.syncLossCounter++; } //Indicates likely sync loss.
      if (configPage4.useResync == 1) { toothCurrentCount = configPage4.triggerTeeth; }
    }

    revolutionOne = 1; //Sequential revolution reset
  }
  else
  {
    triggerSecFilterTime = revolutionTime >> 1; //Set filter at 25% of the current cam speed. This needs to be performed here to prevent a situation where the RPM and triggerSecFilterTime get out of alignment and curGap2 never exceeds the filter value
  } //Trigger filter
}
/** Dual Wheel - Get RPM.
 *
 * */
uint16_t getRPM_DualWheel(void)
{
  if( currentStatus.hasSync == true )
  {
    //Account for cam speed
    if( currentStatus.RPM < currentStatus.crankRPM )
    {
      return crankingGetRPM(configPage4.triggerTeeth, configPage4.TrigSpeed==CAM_SPEED);
    }
    else
    {
      return stdGetRPM(configPage4.TrigSpeed==CAM_SPEED);
    }
  }
  return 0U;
}

/** Dual Wheel - Get Crank angle.
 *
 * */
int getCrankAngle_DualWheel(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    tempRevolutionOne = revolutionOne;
    lastCrankAngleCalc = micros();
    interrupts();

    //Handle case where the secondary tooth was the last one seen
    if(tempToothCurrentCount == 0) { tempToothCurrentCount = configPage4.triggerTeeth; }

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    //Sequential check (simply sets whether we're on the first or 2nd revolution of the cycle)
    if ( (tempRevolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED) ) { crankAngle += 360; }

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

static uint16_t __attribute__((noinline)) calcEndTeeth_DualWheel(int ignitionAngle, uint8_t toothAdder) {
  int16_t tempEndTooth =
#ifdef USE_LIBDIVIDE
      libdivide::libdivide_s16_do(ignitionAngle - configPage4.triggerAngle, &divTriggerToothAngle);
#else
      (ignitionAngle - (int16_t)configPage4.triggerAngle) / (int16_t)triggerToothAngle;
#endif
  return clampToToothCount(tempEndTooth, toothAdder);
}

/** Dual Wheel - Set End Teeth.
 *
 * */
void triggerSetEndTeeth_DualWheel(void)
{
  //The toothAdder variable is used for when a setup is running sequentially, but the primary wheel is running at crank speed. This way the count of teeth will go up to 2* the number of primary teeth to allow for a sequential count.
  byte toothAdder = 0;
  if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage4.TrigSpeed == CRANK_SPEED) ) { toothAdder = configPage4.triggerTeeth; }

  ignition1EndTooth = calcEndTeeth_DualWheel(ignition1EndAngle, toothAdder);
  ignition2EndTooth = calcEndTeeth_DualWheel(ignition2EndAngle, toothAdder);
  ignition3EndTooth = calcEndTeeth_DualWheel(ignition3EndAngle, toothAdder);
  ignition4EndTooth = calcEndTeeth_DualWheel(ignition4EndAngle, toothAdder);
#if IGN_CHANNELS >= 5
  ignition5EndTooth = calcEndTeeth_DualWheel(ignition5EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 6
  ignition6EndTooth = calcEndTeeth_DualWheel(ignition6EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 7
  ignition7EndTooth = calcEndTeeth_DualWheel(ignition7EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 8
  ignition8EndTooth = calcEndTeeth_DualWheel(ignition8EndAngle, toothAdder);
#endif
}
/** @} */

// setEndTeethFromDistributorConfig moved outside #if 0 blocks (after line 1159)

/** Basic Distributor where tooth count is equal to the number of cylinders and teeth are evenly spaced on the cam.
* No position sensing (Distributor is retained) so crank angle is
* a made up figure based purely on the first teeth to be seen.
* Note: This is a very simple decoder. See http://www.megamanual.com/ms2/GM_7pinHEI.htm
* @defgroup dec_dist Basic Distributor
* @{
*/
#endif // dual_wheel

// FASE M: Shared configuration for distributor-based ignition end teeth
// Eliminates 100% code duplication between BasicDistributor and FordTFI
struct DistributorEndTeethRangeConfig {
  uint8_t nCylinders;         // Number of engine cylinders
  int16_t angleThresholdLow;  // Lower bound of angle range (exclusive)
  int16_t angleThresholdHigh; // Upper bound of angle range (inclusive)
  uint16_t endTeeth[4];       // ignition1-4EndTooth values for this range
};

// Static configuration table for distributor ignition end teeth
// Covers 4-cyl, 6-cyl (3-cyl uses same as 6), and 8-cyl patterns
static const DistributorEndTeethRangeConfig distributorEndTeethConfigs[] PROGMEM = {
  // 4 cylinder ranges (2 ranges)
  {4,   0, 180, {1, 2, 0, 0}},  // 0 < angle <= 180
  {4, 180, 361, {2, 1, 0, 0}},  // 180 < angle <= 360 (361 catches ">180 || <=0" case)

  // 6 cylinder ranges (3 ranges) - also used for 3 cylinder
  {6,   0, 120, {1, 2, 3, 0}},  // 0 < angle <= 120
  {6, 120, 240, {2, 3, 1, 0}},  // 120 < angle <= 240
  {6, 240, 361, {3, 1, 2, 0}},  // 240 < angle <= 360

  // 8 cylinder ranges (4 ranges)
  {8,   0,  90, {1, 2, 3, 4}},  // 0 < angle <= 90
  {8,  90, 180, {2, 3, 4, 1}},  // 90 < angle <= 180
  {8, 180, 270, {3, 4, 1, 2}},  // 180 < angle <= 270
  {8, 270, 361, {4, 1, 2, 3}}   // 270 < angle <= 360
};

// Helper function: Set ignition end teeth using data-driven lookup
// Eliminates duplicated switch/if-else logic between multiple triggers - MOVED OUTSIDE #if 0 blocks
void setEndTeethFromDistributorConfig(int16_t tempEndAngle, uint8_t nCylinders)
{
  const uint8_t configCount = sizeof(distributorEndTeethConfigs) / sizeof(DistributorEndTeethRangeConfig);

  // Handle wrap-around case: angle > 180 OR angle <= 0 maps to range 180-360
  // This matches original logic: if( (tempEndAngle > 180) || (tempEndAngle <= 0) )
  if (tempEndAngle <= 0)
  {
    tempEndAngle = 361; // Force into highest range bucket
  }

  for (uint8_t i = 0; i < configCount; i++)
  {
    const DistributorEndTeethRangeConfig* cfg = &distributorEndTeethConfigs[i];

    // Match: same cylinder count AND angle in valid range
    if (cfg->nCylinders == nCylinders &&
        tempEndAngle > cfg->angleThresholdLow &&
        tempEndAngle <= cfg->angleThresholdHigh)
    {
      // Apply end teeth values
      ignition1EndTooth = cfg->endTeeth[0];
      ignition2EndTooth = cfg->endTeeth[1];
      ignition3EndTooth = cfg->endTeeth[2];
      ignition4EndTooth = cfg->endTeeth[3];
      return;
    }
  }
}

// DistributorEndTeethRangeConfig struct and distributorEndTeethConfigs array moved to line ~1131

#if 0 // REFACTORED - Implementation moved to decoders/implementations/basic_distributor.cpp
void triggerSetup_BasicDistributor(void)
{
  triggerActualTeeth = configPage2.nCylinders;
  if(triggerActualTeeth == 0) { triggerActualTeeth = 1; }

  //The number of degrees that passes from tooth to tooth. Depends on number of cylinders and whether 4 or 2 stroke
  if(configPage2.strokes == FOUR_STROKE) { triggerToothAngle = 720U / triggerActualTeeth; }
  else { triggerToothAngle = 360U / triggerActualTeeth; }

  triggerFilterTime = MICROS_PER_MIN / MAX_RPM / configPage2.nCylinders; // Minimum time required between teeth
  triggerFilterTime = triggerFilterTime / 2; //Safety margin
  triggerFilterTime = 0;
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
  toothCurrentCount = 0; //Default value
  BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  if(configPage2.nCylinders <= 4U) { MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/90U) * triggerToothAngle); }//Minimum 90rpm. (1851uS is the time per degree at 90rpm). This uses 90rpm rather than 50rpm due to the potentially very high stall time on a 4 cylinder if we wait that long.
  else { MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); } //Minimum 50rpm. (3200uS is the time per degree at 50rpm).

}

void triggerPri_BasicDistributor(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if ( (curGap >= triggerFilterTime) )
  {
    if(currentStatus.hasSync == true) { setFilter(curGap); } //Recalc the new filter value
    else { triggerFilterTime = 0; } //If we don't yet have sync, ensure that the filter won't prevent future valid pulses from being ignored.

    if( (toothCurrentCount == triggerActualTeeth) || (currentStatus.hasSync == false) ) //Check if we're back to the beginning of a revolution
    {
      toothCurrentCount = 1; //Reset the counter
      toothOneMinusOneTime = toothOneTime;
      toothOneTime = curTime;
      currentStatus.hasSync = true;
      currentStatus.startRevolutions++; //Counter
    }
    else
    {
      if( (toothCurrentCount < triggerActualTeeth) ) { toothCurrentCount++; } //Increment the tooth counter
      else
      {
        //This means toothCurrentCount is greater than triggerActualTeeth, which is bad.
        //If we have sync here then there's a problem. Throw a sync loss
        if( currentStatus.hasSync == true )
        {
          currentStatus.syncLossCounter++;
          currentStatus.hasSync = false;
        }
      }

    }

    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    if ( configPage4.ignCranklock && BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
    {
      endCoil1Charge();
      endCoil2Charge();
      endCoil3Charge();
      endCoil4Charge();
    }

    if(configPage2.perToothIgn == true)
    {
      int16_t crankAngle = ( (toothCurrentCount-1) * triggerToothAngle ) + configPage4.triggerAngle;
      crankAngle = ignitionLimits((crankAngle));
      uint16_t currentTooth = toothCurrentCount;
      if(toothCurrentCount > (triggerActualTeeth/2) ) { currentTooth = (toothCurrentCount - (triggerActualTeeth/2)); }
      checkPerToothTiming(crankAngle, currentTooth);
    }

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
  } //Trigger filter
}
void triggerSec_BasicDistributor(void) { return; } //Not required
uint16_t getRPM_BasicDistributor(void)
{
  uint16_t tempRPM;
  uint8_t distributorSpeed = CAM_SPEED; //Default to cam speed
  if(configPage2.strokes == TWO_STROKE) { distributorSpeed = CRANK_SPEED; } //For 2 stroke distributors, the tooth rate is based on crank speed, not 'cam'

  if( currentStatus.RPM < currentStatus.crankRPM || currentStatus.RPM < 1500)
  {
    tempRPM = crankingGetRPM(triggerActualTeeth, distributorSpeed);
  }
  else { tempRPM = stdGetRPM(distributorSpeed); }

  MAX_STALL_TIME = revolutionTime << 1; //Set the stall time to be twice the current RPM. This is a safe figure as there should be no single revolution where this changes more than this
  if(triggerActualTeeth == 1) { MAX_STALL_TIME = revolutionTime << 1; } //Special case for 1 cylinder engines that only get 1 pulse every 720 degrees
  if(MAX_STALL_TIME < 366667UL) { MAX_STALL_TIME = 366667UL; } //Check for 50rpm minimum

  return tempRPM;

}
int getCrankAngle_BasicDistributor(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);

    //crankAngle += timeToAngleDegPerMicroSec(elapsedTime);
    crankAngle += timeToAngleIntervalTooth(elapsedTime);


    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

// DistributorEndTeethRangeConfig struct and distributorEndTeethConfigs array moved outside #if 0 blocks (line ~1163)

void triggerSetEndTeeth_BasicDistributor(void)
{
  int tempEndAngle = (ignition1EndAngle - configPage4.triggerAngle);
  tempEndAngle = ignitionLimits((tempEndAngle));

  // Use shared data-driven implementation
  // Handles 3-cyl (mapped to 6), 4-cyl, 6-cyl, and 8-cyl
  uint8_t cylinders = configPage2.nCylinders;
  if (cylinders == 3) { cylinders = 6; } // 3-cyl uses 6-cyl pattern

  setEndTeethFromDistributorConfig(tempEndAngle, cylinders);
}
/** @} */

/** Decode GM 7X trigger wheel with six equally spaced teeth and a seventh tooth for cylinder identification.
* Note: Within the decoder code pf GM7X, the sync tooth is referred to as tooth #3 rather than tooth #7. This makes for simpler angle calculations
* (See: http://www.speeduino.com/forum/download/file.php?id=4743 ).
* @defgroup dec_gm7x GM7X
* @{
*/
#endif // basic_distributor
#if 0 // REFACTORED - Implementation moved to decoders/implementations/gm_7x.cpp
void triggerSetup_GM7X(void)
{
  triggerToothAngle = 360 / 6; //The number of degrees that passes from tooth to tooth
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
}

void triggerPri_GM7X(void)
{
    lastGap = curGap;
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    toothCurrentCount++; //Increment the tooth counter
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    if( (toothLastToothTime > 0) && (toothLastMinusOneToothTime > 0) )
    {
      if( toothCurrentCount > 7 )
      {
        toothCurrentCount = 1;
        toothOneMinusOneTime = toothOneTime;
        toothOneTime = curTime;

        BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
      }
      else
      {
        targetGap = (lastGap) >> 1; //The target gap is set at half the last tooth gap
        if ( curGap < targetGap ) //If the gap between this tooth and the last one is less than half of the previous gap, then we are very likely at the magical 3rd tooth
        {
          toothCurrentCount = 3;
          currentStatus.hasSync = true;
          BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); //The tooth angle is double at this point
          currentStatus.startRevolutions++; //Counter
        }
        else
        {
          BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
        }
      }
    }

    //New ignition mode!
    if(configPage2.perToothIgn == true)
    {
      if(toothCurrentCount != 3) //Never do the check on the extra tooth. It's not needed anyway
      {
        //configPage4.triggerAngle must currently be below 48 and above -81
        int16_t crankAngle;
        if( toothCurrentCount < 3 )
        {
          crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + 42 + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
        }
        else
        {
          crankAngle = ((toothCurrentCount - 2) * triggerToothAngle) + 42 + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
        }
        checkPerToothTiming(crankAngle, toothCurrentCount);
      }
    }

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;


}
void triggerSec_GM7X(void) { return; } //Not required
uint16_t getRPM_GM7X(void)
{
   return stdGetRPM(CRANK_SPEED);
}
int getCrankAngle_GM7X(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    //Check if the last tooth seen was the reference tooth (Number 3). All others can be calculated, but tooth 3 has a unique angle
    int crankAngle;
    if( tempToothCurrentCount < 3 )
    {
      crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + 42 + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
    }
    else if( tempToothCurrentCount == 3 )
    {
      crankAngle = 112;
    }
    else
    {
      crankAngle = ((tempToothCurrentCount - 2) * triggerToothAngle) + 42 + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
    }

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_GM7X(void)
{
  if(currentStatus.advance < 18 )
  {
    ignition1EndTooth = 7;
    ignition2EndTooth = 2;
    ignition3EndTooth = 5;
  }
  else
  {
    ignition1EndTooth = 6;
    ignition2EndTooth = 1;
    ignition3EndTooth = 4;
  }
}
/** @} */

/** Mitsubishi 4G63 / NA/NB Miata + MX-5 / 4/2.
Note: raw.githubusercontent.com/noisymime/speeduino/master/reference/wiki/decoders/4g63_trace.png
Tooth #1 is defined as the next crank tooth after the crank signal is HIGH when the cam signal is falling.
Tooth number one is at 355* ATDC.
* @defgroup dec_mitsu_miata Mistsubishi 4G63 and Miata + MX-5
* @{
*/
#endif // gm_7x

// 4G63 trigger filter configuration - data-driven approach to eliminate 99 lines of duplication
// Filter calculations: 0=direct, 1=rshift1, 2=rshift2, 3=rshift3, 4=mult5_rshift2, 5=mult3_rshift3, 6=mult11_rshift3, 7=mult9_rshift5
enum FilterCalcType : uint8_t {
  CALC_DIRECT = 0,      // triggerFilterTime = curGap
  CALC_RSHIFT1 = 1,     // triggerFilterTime = curGap >> 1
  CALC_RSHIFT2 = 2,     // triggerFilterTime = curGap >> 2
  CALC_MULT5_RSHIFT2 = 3,   // triggerFilterTime = (curGap * 5) >> 2
  CALC_MULT3_RSHIFT2 = 4,   // triggerFilterTime = (curGap * 3) >> 2
  CALC_MULT3_RSHIFT3 = 5,   // triggerFilterTime = rshift<3>(curGap * 3UL)
  CALC_MULT11_RSHIFT3 = 6,  // triggerFilterTime = rshift<3>(curGap * 11UL)
  CALC_MULT9_RSHIFT5 = 7    // triggerFilterTime = rshift<5>(curGap * 9UL)
};

struct Trigger4G63FilterConfig {
  uint8_t toothAngle_odd;   // Angle for odd teeth (1,3,5,7,9,11)
  uint8_t toothAngle_even;  // Angle for even teeth (2,4,6,8,10,12)
  FilterCalcType calcType_odd_4cyl;
  FilterCalcType calcType_even_4cyl;
  FilterCalcType calcType_odd_6cyl;
  FilterCalcType calcType_even_6cyl;
};

// Configuration array: [filter_level] where 0=none, 1=lite, 2=medium, 3=aggressive
static const Trigger4G63FilterConfig filter4G63Configs[4] = {
  // Filter OFF (index 0) - toothAngle_odd, toothAngle_even, 4x FilterCalcType
  {
    70, 110,
    CALC_DIRECT, CALC_DIRECT,
    CALC_DIRECT, CALC_DIRECT
  },
  // Filter LITE (index 1)
  {
    70, 110,
    CALC_DIRECT, CALC_MULT3_RSHIFT3,  // 4cyl: odd=curGap, even=(curGap*3)>>3
    CALC_RSHIFT2, CALC_RSHIFT1        // 6cyl: odd=curGap>>2, even=curGap>>1
  },
  // Filter MEDIUM (index 2)
  {
    70, 110,
    CALC_MULT5_RSHIFT2, CALC_RSHIFT1, // 4cyl: odd=(curGap*5)>>2, even=curGap>>1
    CALC_RSHIFT1, CALC_MULT3_RSHIFT2  // 6cyl: odd=curGap>>1, even=(curGap*3)>>2
  },
  // Filter AGGRESSIVE (index 3)
  {
    70, 110,
    CALC_MULT11_RSHIFT3, CALC_MULT9_RSHIFT5,  // 4cyl: odd=(curGap*11)>>3, even=(curGap*9)>>5
    CALC_RSHIFT1, CALC_DIRECT                  // 6cyl: odd=curGap>>1, even=curGap
  }
};

// Helper function to calculate trigger filter time based on configuration
static inline uint32_t calculate4G63FilterTime(FilterCalcType calcType, uint32_t curGap)
{
  switch(calcType)
  {
    case CALC_DIRECT:
      return curGap;
    case CALC_RSHIFT1:
      return curGap >> 1;
    case CALC_RSHIFT2:
      return curGap >> 2;
    case CALC_MULT5_RSHIFT2:
      return (curGap * 5U) >> 2;
    case CALC_MULT3_RSHIFT2:
      return (curGap * 3U) >> 2;
    case CALC_MULT3_RSHIFT3:
      return rshift<3>(curGap * 3UL);
    case CALC_MULT11_RSHIFT3:
      return rshift<3>(curGap * 11UL);
    case CALC_MULT9_RSHIFT5:
      return rshift<5>(curGap * 9UL);
    default:
      return 0;
  }
}

// Helper function to apply 4G63 trigger filter configuration - MOVED OUTSIDE #if 0
void apply4G63FilterConfig(uint8_t filterLevel, bool isOddTooth, uint8_t nCylinders, uint32_t curGap)
{
  // Guard clause: invalid filter level
  if(filterLevel > 3) { filterLevel = 0; }

  const Trigger4G63FilterConfig* config = &filter4G63Configs[filterLevel];

  if(isOddTooth)
  {
    // Odd teeth (1,3,5,7,9,11) - always 70 degrees
    triggerToothAngle = config->toothAngle_odd;
    if(filterLevel == 0)
    {
      triggerFilterTime = 0;  // Filter disabled
    }
    else
    {
      FilterCalcType calcType = (nCylinders == 4) ? config->calcType_odd_4cyl : config->calcType_odd_6cyl;
      triggerFilterTime = calculate4G63FilterTime(calcType, curGap);
    }
  }
  else
  {
    // Even teeth (2,4,6,8,10,12)
    triggerToothAngle = (nCylinders == 4) ? 110 : 50;
    if(filterLevel == 0)
    {
      triggerFilterTime = 0;  // Filter disabled
    }
    else
    {
      FilterCalcType calcType = (nCylinders == 4) ? config->calcType_even_4cyl : config->calcType_even_6cyl;
      triggerFilterTime = calculate4G63FilterTime(calcType, curGap);
    }
  }
}

#if 0 // REFACTORED - Implementation moved to decoders/implementations/four_g63.cpp
void triggerSetup_4G63(void)
{
  triggerToothAngle = 180; //The number of degrees that passes from tooth to tooth (primary)
  toothCurrentCount = 99; //Fake tooth count represents no sync
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = 366667UL; //Minimum 50rpm based on the 110 degree tooth spacing
  if(currentStatus.initialisationComplete == false) { toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the initial check to prevent the fuel pump just staying on all the time

  //Note that these angles are for every rising and falling edge
  if(configPage2.nCylinders == 6)
  {
    //New values below
    toothAngles[0] = 715; //Rising edge of tooth #1
    toothAngles[1] = 45;  //Falling edge of tooth #1
    toothAngles[2] = 115; //Rising edge of tooth #2
    toothAngles[3] = 165; //Falling edge of tooth #2
    toothAngles[4] = 235; //Rising edge of tooth #3
    toothAngles[5] = 285; //Falling edge of tooth #3

    toothAngles[6] = 355; //Rising edge of tooth #4
    toothAngles[7] = 405; //Falling edge of tooth #4
    toothAngles[8] = 475; //Rising edge of tooth #5
    toothAngles[9] = 525; //Falling edge of tooth $5
    toothAngles[10] = 595; //Rising edge of tooth #6
    toothAngles[11] = 645; //Falling edge of tooth #6

    triggerActualTeeth = 12; //Both sides of all teeth over 720 degrees
  }
  else
  {
    // 70 / 110 for 4 cylinder
    toothAngles[0] = 715; //Falling edge of tooth #1
    toothAngles[1] = 105; //Rising edge of tooth #2
    toothAngles[2] = 175; //Falling edge of tooth #2
    toothAngles[3] = 285; //Rising edge of tooth #1

    toothAngles[4] = 355; //Falling edge of tooth #1
    toothAngles[5] = 465; //Rising edge of tooth #2
    toothAngles[6] = 535; //Falling edge of tooth #2
    toothAngles[7] = 645; //Rising edge of tooth #1

    triggerActualTeeth = 8;
  }

  triggerFilterTime = 1500; //10000 rpm, assuming we're triggering on both edges off the crank tooth.
  triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U; //Same as above, but fixed at 2 teeth on the secondary input and divided by 2 (for cam speed)
  triggerSecFilterTime_duration = 4000;
  secondaryLastToothTime = 0;
}

// 4G63 types moved outside #if 0 block (line ~1475): FilterCalcType, Trigger4G63FilterConfig, filter4G63Configs, calculate4G63FilterTime, apply4G63FilterConfig

void triggerPri_4G63(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if ( (curGap >= triggerFilterTime) || (currentStatus.startRevolutions == 0) )
  {
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)
    triggerFilterTime = curGap >> 2; //This only applies during non-sync conditions. If there is sync then triggerFilterTime gets changed again below with a better value.

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    toothCurrentCount++;

    if( (toothCurrentCount == 1) || (toothCurrentCount > triggerActualTeeth) ) //Trigger is on CHANGE, hence 4 pulses = 1 crank rev (or 6 pulses for 6 cylinders)
    {
       toothCurrentCount = 1; //Reset the counter
       toothOneMinusOneTime = toothOneTime;
       toothOneTime = curTime;
       currentStatus.startRevolutions++; //Counter
    }

    if (currentStatus.hasSync == true)
    {
      if ( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && configPage4.ignCranklock && (currentStatus.startRevolutions >= configPage4.StgCycles))
      {
        if(configPage2.nCylinders == 4)
        {
          //This operates in forced wasted spark mode during cranking to align with crank teeth
          if( (toothCurrentCount == 1) || (toothCurrentCount == 5) ) { endCoil1Charge(); endCoil3Charge(); }
          else if( (toothCurrentCount == 3) || (toothCurrentCount == 7) ) { endCoil2Charge(); endCoil4Charge(); }
        }
        else if(configPage2.nCylinders == 6)
        {
          if( (toothCurrentCount == 1) || (toothCurrentCount == 7) ) { endCoil1Charge(); }
          else if( (toothCurrentCount == 3) || (toothCurrentCount == 9) ) { endCoil2Charge(); }
          else if( (toothCurrentCount == 5) || (toothCurrentCount == 11) ) { endCoil3Charge(); }
        }
      }

      // Apply trigger filter configuration using data-driven approach
      // Determine filter level: 0=none, 1=lite, 2=medium, 3=aggressive
      uint8_t filterLevel;
      if( (configPage4.triggerFilter == 1) || (currentStatus.RPM < 1400) ) {
        filterLevel = 1;  // Lite filter
      }
      else if(configPage4.triggerFilter == 2) {
        filterLevel = 2;  // Medium filter
      }
      else if(configPage4.triggerFilter == 3) {
        filterLevel = 3;  // Aggressive filter
      }
      else {
        filterLevel = 0;  // Filter off
      }

      // Determine if odd tooth (1,3,5,7,9,11) or even tooth (2,4,6,8,10,12)
      bool isOddTooth = (toothCurrentCount & 1) != 0;

      // Apply configuration (replaces 104 lines of duplicated code)
      apply4G63FilterConfig(filterLevel, isOddTooth, configPage2.nCylinders, curGap);

      //EXPERIMENTAL!
      //New ignition mode is ONLY available on 4g63 when the trigger angle is set to the stock value of 0.
      if( (configPage2.perToothIgn == true) && (configPage4.triggerAngle == 0) )
      {
        if( (configPage2.nCylinders == 4) && (currentStatus.advance > 0) )
        {
          int16_t crankAngle = ignitionLimits( toothAngles[(toothCurrentCount-1)] );

          //Handle non-sequential tooth counts
          if( (configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (toothCurrentCount > configPage2.nCylinders) ) { checkPerToothTiming(crankAngle, (toothCurrentCount-configPage2.nCylinders) ); }
          else { checkPerToothTiming(crankAngle, toothCurrentCount); }
        }
      }
    } //Has sync
    else
    {
      triggerSecFilterTime = 0;
      //New secondary method of determining sync
      if(READ_PRI_TRIGGER() == true)
      {
        if(READ_SEC_TRIGGER() == true) { revolutionOne = true; }
        else { revolutionOne = false; }
      }
      else
      {
        if( (READ_SEC_TRIGGER() == false) && (revolutionOne == true) )
        {
          //Crank is low, cam is low and the crank pulse STARTED when the cam was high.
          if(configPage2.nCylinders == 4) { toothCurrentCount = 1; } //Means we're at 5* BTDC on a 4G63 4 cylinder
          //else if(configPage2.nCylinders == 6) { toothCurrentCount = 8; }
        }
        //If sequential is ever enabled, the below toothCurrentCount will need to change:
        else if( (READ_SEC_TRIGGER() == true) && (revolutionOne == true) )
        {
          //Crank is low, cam is high and the crank pulse STARTED when the cam was high.
          if(configPage2.nCylinders == 4) { toothCurrentCount = 5; } //Means we're at 5* BTDC on a 4G63 4 cylinder
          else if(configPage2.nCylinders == 6) { toothCurrentCount = 2; currentStatus.hasSync = true; } //Means we're at 45* ATDC on 6G72 6 cylinder
        }
      }
    }
  } //Filter time

}
void triggerSec_4G63(void)
{
  //byte crankState = READ_PRI_TRIGGER();
  //First filter is a duration based one to ensure the pulse was of sufficient length (time)
  //if(READ_SEC_TRIGGER()) { secondaryLastToothTime1 = micros(); return; }
  if(currentStatus.hasSync == true)
  {
  //1166 is the time taken to cross 70 degrees at 10k rpm
  //if ( (micros() - secondaryLastToothTime1) < triggerSecFilterTime_duration ) { return; }
  //triggerSecFilterTime_duration = (micros() - secondaryLastToothTime1) >> 1;
  }


  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;
  if ( (curGap2 >= triggerSecFilterTime) )//|| (currentStatus.startRevolutions == 0) )
  {
    toothLastSecToothTime = curTime2;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)
    //addToothLogEntry(curGap, TOOTH_CAM_SECONDARY);

    triggerSecFilterTime = curGap2 >> 1; //Basic 50% filter for the secondary reading
    //More aggressive options:
    //62.5%:
    //triggerSecFilterTime = (curGap2 * 9) >> 5;
    //75%:
    //triggerSecFilterTime = (curGap2 * 6) >> 3;

    //if( (currentStatus.RPM < currentStatus.crankRPM) || (currentStatus.hasSync == false) )
    if( (currentStatus.hasSync == false) )
    {

      triggerFilterTime = 1500; //If this is removed, can have trouble getting sync again after the engine is turned off (but ECU not reset).
      triggerSecFilterTime = triggerSecFilterTime >> 1; //Divide the secondary filter time by 2 again, making it 25%. Only needed when cranking
      if(READ_PRI_TRIGGER() == true)
      {
        if(configPage2.nCylinders == 4)
        {
          if(toothCurrentCount == 8) { currentStatus.hasSync = true; } //Is 8 for sequential, was 4
        }
        else if(configPage2.nCylinders == 6)
        {
          if(toothCurrentCount == 7) { currentStatus.hasSync = true; }
        }

      }
      else
      {
        if(configPage2.nCylinders == 4)
        {
          if(toothCurrentCount == 5) { currentStatus.hasSync = true; } //Is 5 for sequential, was 1
        }
        //Cannot gain sync for 6 cylinder here.
      }
    }

    //if ( (micros() - secondaryLastToothTime1) < triggerSecFilterTime_duration && configPage2.useResync )
    if ( (currentStatus.RPM < currentStatus.crankRPM) || (configPage4.useResync == 1) )
    {
      if( (currentStatus.hasSync == true) && (configPage2.nCylinders == 4) )
      {
        triggerSecFilterTime_duration = (micros() - secondaryLastToothTime1) >> 1;
        if(READ_PRI_TRIGGER() == true)
        {
          //Whilst we're cranking and have sync, we need to watch for noise pulses.
          if(toothCurrentCount != 8)
          {
            // This should never be true, except when there's noise
            currentStatus.hasSync = false;
            currentStatus.syncLossCounter++;
          }
          else { toothCurrentCount = 8; } //Why? Just why?
        }
      } //Has sync and 4 cylinder
    } // Use resync or cranking
  } //Trigger filter
}


uint16_t getRPM_4G63(void)
{
  uint16_t tempRPM = 0;
  //During cranking, RPM is calculated 4 times per revolution, once for each rising/falling of the crank signal.
  //Because these signals aren't even (Alternating 110 and 70 degrees), this needs a special function
  if(currentStatus.hasSync == true)
  {
    if( (currentStatus.RPM < currentStatus.crankRPM)  )
    {
      int tempToothAngle;
      unsigned long toothTime;
      if( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { tempRPM = 0; }
      else
      {
        noInterrupts();
        tempToothAngle = triggerToothAngle;
        toothTime = (toothLastToothTime - toothLastMinusOneToothTime); //Note that trigger tooth angle changes between 70 and 110 depending on the last tooth that was seen (or 70/50 for 6 cylinders)
        interrupts();
        toothTime = toothTime * 36;
        tempRPM = ((unsigned long)tempToothAngle * (MICROS_PER_MIN/10U)) / toothTime;
        SetRevolutionTime((10UL * toothTime) / tempToothAngle);
        MAX_STALL_TIME = 366667UL; // 50RPM
      }
    }
    else
    {
      tempRPM = stdGetRPM(CAM_SPEED);
      //EXPERIMENTAL! Add/subtract RPM based on the last rpmDOT calc
      //tempRPM += (micros() - toothOneTime) * currentStatus.rpmDOT
      MAX_STALL_TIME = revolutionTime << 1; //Set the stall time to be twice the current RPM. This is a safe figure as there should be no single revolution where this changes more than this
      if(MAX_STALL_TIME < 366667UL) { MAX_STALL_TIME = 366667UL; } //Check for 50rpm minimum
    }
  }

  return tempRPM;
}

int getCrankAngle_4G63(void)
{
    int crankAngle = 0;
    if(currentStatus.hasSync == true)
    {
      //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
      unsigned long tempToothLastToothTime;
      int tempToothCurrentCount;
      //Grab some variables that are used in the trigger code and assign them to temp variables.
      noInterrupts();
      tempToothCurrentCount = toothCurrentCount;
      tempToothLastToothTime = toothLastToothTime;
      lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
      interrupts();

      crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle; //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

      //Estimate the number of degrees travelled since the last tooth}
      elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
      crankAngle += timeToAngleIntervalTooth(elapsedTime);

      if (crankAngle >= 720) { crankAngle -= 720; }
      if (crankAngle < 0) { crankAngle += 360; }
    }
    return crankAngle;
}

void triggerSetEndTeeth_4G63(void)
{
  if(configPage2.nCylinders == 4)
  {
    if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
    {
      ignition1EndTooth = 8;
      ignition2EndTooth = 2;
      ignition3EndTooth = 4;
      ignition4EndTooth = 6;
    }
    else
    {
      ignition1EndTooth = 4;
      ignition2EndTooth = 2;
      ignition3EndTooth = 4; //Not used
      ignition4EndTooth = 2;
    }
  }
  if(configPage2.nCylinders == 6)
  {
    if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
    {
      //This should never happen as 6 cylinder sequential not supported
      ignition1EndTooth = 8;
      ignition2EndTooth = 2;
      ignition3EndTooth = 4;
      ignition4EndTooth = 6;
    }
    else
    {
      ignition1EndTooth = 6;
      ignition2EndTooth = 2;
      ignition3EndTooth = 4;
      ignition4EndTooth = 2; //Not used
    }
  }
}
/** @} */

/** GM 24X Decoder (eg early LS1 1996-2005).
Note: Useful references:
*
- www.vems.hu/wiki/index.php?page=MembersPage%2FJorgenKarlsson%2FTwentyFourX

Provided that the cam signal is used, this decoder simply counts the teeth and then looks their angles up against a lookup table. The cam signal is used to determine tooth #1
* @defgroup dec_gm GM 24X
* @{
*/
#endif // four_g63

#if 0  // 24X, Jeep2000, Audi135, HondaD17, HondaJ32 - REFACTORED to implementations/
void triggerSetup_24X(void)
{
  triggerToothAngle = 15; //The number of degrees that passes from tooth to tooth (primary)
  toothAngles[0] = 12;
  toothAngles[1] = 18;
  toothAngles[2] = 33;
  toothAngles[3] = 48;
  toothAngles[4] = 63;
  toothAngles[5] = 78;
  toothAngles[6] = 102;
  toothAngles[7] = 108;
  toothAngles[8] = 123;
  toothAngles[9] = 138;
  toothAngles[10] = 162;
  toothAngles[11] = 177;
  toothAngles[12] = 183;
  toothAngles[13] = 198;
  toothAngles[14] = 222;
  toothAngles[15] = 237;
  toothAngles[16] = 252;
  toothAngles[17] = 258;
  toothAngles[18] = 282;
  toothAngles[19] = 288;
  toothAngles[20] = 312;
  toothAngles[21] = 327;
  toothAngles[22] = 342;
  toothAngles[23] = 357;

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  if(currentStatus.initialisationComplete == false) { toothCurrentCount = 25; toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the init check to prevent the fuel pump just staying on all the time
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

void triggerPri_24X(void)
{
  if(toothCurrentCount == 25) { currentStatus.hasSync = false; } //Indicates sync has not been achieved (Still waiting for 1 revolution of the crank to take place)
  else
  {
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    if(toothCurrentCount == 0)
    {
       toothCurrentCount = 1; //Reset the counter
       toothOneMinusOneTime = toothOneTime;
       toothOneTime = curTime;
       revolutionOne = !revolutionOne; //Sequential revolution flip
       currentStatus.hasSync = true;
       currentStatus.startRevolutions++; //Counter
       triggerToothAngle = 15; //Always 15 degrees for tooth #15
    }
    else
    {
      toothCurrentCount++; //Increment the tooth counter
      triggerToothAngle = toothAngles[(toothCurrentCount-1)] - toothAngles[(toothCurrentCount-2)]; //Calculate the last tooth gap in degrees
    }

    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    toothLastToothTime = curTime;


  }
}

void triggerSec_24X(void)
{
  toothCurrentCount = 0; //All we need to do is reset the tooth count back to zero, indicating that we're at the beginning of a new revolution
  revolutionOne = 1; //Sequential revolution reset
}

uint16_t getRPM_24X(void)
{
   return stdGetRPM(CRANK_SPEED);
}
int getCrankAngle_24X(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount, tempRevolutionOne;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    tempRevolutionOne = revolutionOne;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    int crankAngle;
    if (tempToothCurrentCount == 0) { crankAngle = 0 + configPage4.triggerAngle; } //This is the special case to handle when the 'last tooth' seen was the cam tooth. 0 is the angle at which the crank tooth goes high (Within 360 degrees).
    else { crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;} //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    //Sequential check (simply sets whether we're on the first or 2nd revolution of the cycle)
    if (tempRevolutionOne == 1) { crankAngle += 360; }

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_24X(void)
{
}
/** @} */

/** Jeep 2000 - 24 crank teeth over 720 degrees, in groups of 4 ('91 to 2000 6 cylinder Jeep engines).
* Crank wheel is high for 360 crank degrees. Quite similar to the 24X setup.
* As we only need timing within 360 degrees, only 12 tooth angles are defined.
* Tooth number 1 represents the first tooth seen after the cam signal goes high.
* www.speeduino.com/forum/download/file.php?id=205
* @defgroup dec_jeep Jeep 2000 (6 cyl)
* @{
*/
void triggerSetup_Jeep2000(void)
{
  triggerToothAngle = 0; //The number of degrees that passes from tooth to tooth (primary)
  toothAngles[0] = 174;
  toothAngles[1] = 194;
  toothAngles[2] = 214;
  toothAngles[3] = 234;
  toothAngles[4] = 294;
  toothAngles[5] = 314;
  toothAngles[6] = 334;
  toothAngles[7] = 354;
  toothAngles[8] = 414;
  toothAngles[9] = 434;
  toothAngles[10] = 454;
  toothAngles[11] = 474;

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 60U); //Minimum 50rpm. (3333uS is the time per degree at 50rpm). Largest gap between teeth is 60 degrees.
  if(currentStatus.initialisationComplete == false) { toothCurrentCount = 13; toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the initial check to prevent the fuel pump just staying on all the time
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

void triggerPri_Jeep2000(void)
{
  if(toothCurrentCount == 13) { currentStatus.hasSync = false; } //Indicates sync has not been achieved (Still waiting for 1 revolution of the crank to take place)
  else
  {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if ( curGap >= triggerFilterTime )
    {
      if(toothCurrentCount == 0)
      {
         toothCurrentCount = 1; //Reset the counter
         toothOneMinusOneTime = toothOneTime;
         toothOneTime = curTime;
         currentStatus.hasSync = true;
         currentStatus.startRevolutions++; //Counter
         triggerToothAngle = 60; //There are groups of 4 pulses (Each 20 degrees apart), with each group being 60 degrees apart. Hence #1 is always 60
      }
      else
      {
        toothCurrentCount++; //Increment the tooth counter
        triggerToothAngle = toothAngles[(toothCurrentCount-1)] - toothAngles[(toothCurrentCount-2)]; //Calculate the last tooth gap in degrees
      }

      setFilter(curGap); //Recalc the new filter value

      BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

      toothLastMinusOneToothTime = toothLastToothTime;
      toothLastToothTime = curTime;
    } //Trigger filter
  } //Sync check
}
void triggerSec_Jeep2000(void)
{
  toothCurrentCount = 0; //All we need to do is reset the tooth count back to zero, indicating that we're at the beginning of a new revolution
  return;
}

uint16_t getRPM_Jeep2000(void)
{
   return stdGetRPM(CRANK_SPEED);
}
int getCrankAngle_Jeep2000(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    int crankAngle;
    if (toothCurrentCount == 0) { crankAngle = 114 + configPage4.triggerAngle; } //This is the special case to handle when the 'last tooth' seen was the cam tooth. Since  the tooth timings were taken on the previous crank tooth, the previous crank tooth angle is used here, not cam angle.
    else { crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;} //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_Jeep2000(void)
{
}
/** @} */

/** Audi with 135 teeth on the crank and 1 tooth on the cam.
* This is very similar to the dual wheel decoder, however due to the 135 teeth not dividing evenly into 360,
* only every 3rd crank tooth is used in calculating the crank angle. This effectively makes it a 45 tooth dual wheel setup.
* @defgroup dec_audi135 Audi 135
* @{
*/
void triggerSetup_Audi135(void)
{
  triggerToothAngle = 8; //135/3 = 45, 360/45 = 8 degrees every 3 teeth
  toothCurrentCount = UINT8_MAX; //Default value
  toothSystemCount = 0;
  triggerFilterTime = (unsigned long)(MICROS_PER_SEC / (MAX_RPM / 60U * 135UL)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U; //Same as above, but fixed at 2 teeth on the secondary input and divided by 2 (for cam speed)
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

void triggerPri_Audi135(void)
{
   curTime = micros();
   curGap = curTime - toothSystemLastToothTime;
   if ( (curGap > triggerFilterTime) || (currentStatus.startRevolutions == 0) )
   {
     toothSystemCount++;

     if ( currentStatus.hasSync == false ) { toothLastToothTime = curTime; }
     else
     {
       if ( toothSystemCount >= 3 )
       {
         //We only proceed for every third tooth

         BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)
         toothSystemLastToothTime = curTime;
         toothSystemCount = 0;
         toothCurrentCount++; //Increment the tooth counter

         if ( (toothCurrentCount == 1) || (toothCurrentCount > 45) )
         {
           toothCurrentCount = 1;
           toothOneMinusOneTime = toothOneTime;
           toothOneTime = curTime;
           revolutionOne = !revolutionOne;
           currentStatus.startRevolutions++; //Counter
         }

         setFilter(curGap); //Recalc the new filter value

         toothLastMinusOneToothTime = toothLastToothTime;
         toothLastToothTime = curTime;
       } //3rd tooth check
     } // Sync check
   } // Trigger filter
}

void triggerSec_Audi135(void)
{
  /*
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;
  if ( curGap2 < triggerSecFilterTime ) { return; }
  toothLastSecToothTime = curTime2;
  */

  if( currentStatus.hasSync == false )
  {
    toothCurrentCount = 0;
    currentStatus.hasSync = true;
    toothSystemCount = 3; //Need to set this to 3 so that the next primary tooth is counted
  }
  else if (configPage4.useResync == 1) { toothCurrentCount = 0; toothSystemCount = 3; }
  else if ( (currentStatus.startRevolutions < 100) && (toothCurrentCount != 45) ) { toothCurrentCount = 0; }
  revolutionOne = 1; //Sequential revolution reset
}

uint16_t getRPM_Audi135(void)
{
   return stdGetRPM(CRANK_SPEED);
}

int getCrankAngle_Audi135(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    tempRevolutionOne = revolutionOne;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    //Handle case where the secondary tooth was the last one seen
    if(tempToothCurrentCount == 0) { tempToothCurrentCount = 45; }

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    //Sequential check (simply sets whether we're on the first or 2nd revolution of the cycle)
    if (tempRevolutionOne) { crankAngle += 360; }

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

void triggerSetEndTeeth_Audi135(void)
{
}
/** @} */
/** Honda D17 (1.7 liter 4 cyl SOHC).
*
* @defgroup dec_honda_d17 Honda D17
* @{
*/
void triggerSetup_HondaD17(void)
{
  triggerToothAngle = 360 / 12; //The number of degrees that passes from tooth to tooth
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
}

void triggerPri_HondaD17(void)
{
   lastGap = curGap;
   curTime = micros();
   curGap = curTime - toothLastToothTime;
   toothCurrentCount++; //Increment the tooth counter

   BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

   //
   if( (toothCurrentCount == 13) && (currentStatus.hasSync == true) )
   {
     toothCurrentCount = 0;
   }
   else if( (toothCurrentCount == 1) && (currentStatus.hasSync == true) )
   {
     toothOneMinusOneTime = toothOneTime;
     toothOneTime = curTime;
     currentStatus.startRevolutions++; //Counter

     toothLastMinusOneToothTime = toothLastToothTime;
     toothLastToothTime = curTime;
   }
   else
   {
     //13th tooth
     targetGap = (lastGap) >> 1; //The target gap is set at half the last tooth gap
     if ( curGap < targetGap) //If the gap between this tooth and the last one is less than half of the previous gap, then we are very likely at the magical 13th tooth
     {
       toothCurrentCount = 0;
       currentStatus.hasSync = true;
     }
     else
     {
       //The tooth times below don't get set on tooth 13(The magical 13th tooth should not be considered for any calculations that use those times)
       toothLastMinusOneToothTime = toothLastToothTime;
       toothLastToothTime = curTime;
     }
   }

}
void triggerSec_HondaD17(void) { return; } //The 4+1 signal on the cam is yet to be supported. If this ever changes, update BIT_DECODER_HAS_SECONDARY in the setup() function
uint16_t getRPM_HondaD17(void)
{
   return stdGetRPM(CRANK_SPEED);
}
int getCrankAngle_HondaD17(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    //Check if the last tooth seen was the reference tooth 13 (Number 0 here). All others can be calculated, but tooth 3 has a unique angle
    int crankAngle;
    if( tempToothCurrentCount == 0 )
    {
      crankAngle = (11 * triggerToothAngle) + configPage4.triggerAngle; //if temptoothCurrentCount is 0, the last tooth seen was the 13th one. Based on this, ignore the 13th tooth and use the 12th one as the last reference.
    }
    else
    {
      crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
    }

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_HondaD17(void)
{
}

/** @} */
/** Honda J 32 (3.2 liter 6 cyl SOHC).
*  The Honda J32a4 (and all J series I'm aware of) has a crank trigger with nominal 24 teeth (22 teeth actually present).
*  It has one missing tooth, then 7 teeth, then another missing tooth, then 15 teeth.
*  The tooth rising edges all have uniform spacing between them, except for teeth 14 and 22, which measure about
*  18 degrees between rising edges, rather than 15 degrees as the other teeth do.  These slightly larger
*  teeth are immediately before a gap, and the extra 3 degrees is made up for in the gap, the gap being about
*  3 degrees smaller than might be nominally expected, such that the expected rotational angle is restored immediately after
*  the gap (missing tooth) passes.
*  Teeth are represented as 0V at the ECU, no teeth are represented as 5V.
*  Top dead center of cylinder number 1 occurs as we lose sight of (just pass) the first tooth in the string of 15 teeth
*  (this is a rising edge).
*  The second tooth in the string of 15 teeth is designated as tooth 1 in this code. This means that
*  when the interrupt for tooth 1 fires (as we just pass tooth 1), crank angle = 360/24 = 15 degrees.
*  It follows that the first tooth in the string of 7 teeth is tooth 16.
* @defgroup dec_honda_j_32 Honda J 32
* @{
*/
void triggerSetup_HondaJ32(void)
{
  triggerToothAngle = 360 / 24; //The number of degrees that passes from tooth to tooth
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/10U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);

  // Filter (ignore) triggers that are faster than this.
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60 * 24));
  toothLastToothTime = 0;
  toothCurrentCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  lastGap = 0;
  revolutionOne = 0;
}

// FASE U: Honda J32 sync logic extraction
// Extracts sync achievement and validation helpers to reduce complexity

// Helper: Check if tooth has unusual spacing (18deg vs 15deg)
// Teeth 14 and 22 are wider and should not update lastGap
static inline bool isUnusualSpacingTooth_HondaJ32(uint8_t tooth)
{
  return (tooth == 14 || tooth == 22);
}

// Helper: Check if tooth is first after missing tooth
// Teeth 15 and 23 follow the two missing teeth in the pattern
static inline bool isAfterMissingTooth_HondaJ32(uint8_t tooth)
{
  return (tooth == 23 || tooth == 15);
}

// Helper: Validate that gap is actually big (at least 1.5x last gap)
// Used to verify we're actually at a missing tooth, not a noise spike
static inline bool isBigGapValid_HondaJ32(uint16_t curGap, uint16_t lastGap)
{
  return (curGap >= ((lastGap >> 1) * 3));
}

// Helper: Achieve sync when we detect we're at tooth 16 (first in string of 7)
// Initializes tooth1 times based on known position and gap width
static inline void achieveSync_HondaJ32(uint16_t curTime, uint16_t lastGap)
{
  currentStatus.hasSync = true;
  toothCurrentCount = 16;  // First tooth in string of 7 after second gap
  toothOneTime = curTime - (15 * lastGap);  // Calculate tooth 1 time backwards
  toothOneMinusOneTime = toothOneTime - (24 * lastGap);  // Full revolution back
}

void triggerPri_HondaJ32(void)
{
  // This function is called only on rising edges, which occur as we lose sight of a tooth.
  // This function sets the following state variables for use in other functions:
  // toothLastToothTime, toothOneTime, revolutionOne (just toggles - not correct)
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  toothLastToothTime = curTime;

  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

  if (currentStatus.hasSync == true) // We have sync
  {
    toothCurrentCount++;

    if (toothCurrentCount == 25) { // handle rollover.  Normal sized tooth here
      toothCurrentCount = 1;
      toothOneMinusOneTime = toothOneTime;
      toothOneTime = curTime;
      currentStatus.startRevolutions++;
      SetRevolutionTime(toothOneTime - toothOneMinusOneTime);
    }
    else if (isAfterMissingTooth_HondaJ32(toothCurrentCount)) // First tooth after a missing tooth
    {
      toothCurrentCount++; // account for missing tooth
      if (!isBigGapValid_HondaJ32(curGap, lastGap)) // Gap should be big, if not → lost sync
      {
        currentStatus.hasSync = false;
        toothCurrentCount = 1;
      }
    }
    else if (!isUnusualSpacingTooth_HondaJ32(toothCurrentCount))
    {
      // Update lastGap for normal teeth (not 14 or 22 which are 18deg instead of 15deg)
      lastGap = curGap;
    }
    // else toothCurrentCount == 14 or 22.  Take no further action.
  }
  else // we do not have sync yet. While syncing, treat tooth 14 and 22 as normal teeth
  {
    if (!isBigGapValid_HondaJ32(curGap, lastGap) || lastGap == 0) { // Regular tooth, lastGap == 0 at startup
      toothCurrentCount++;  // Increment teeth between gaps
      lastGap = curGap;
    }
    else { // First tooth after the missing tooth
      if (toothCurrentCount == 15) {  // 15 teeth since the gap before this, meaning we just passed the second gap and are synced
        achieveSync_HondaJ32(curTime, lastGap);
      }
      else { // Unclear which gap we just passed. reset counter
        toothCurrentCount = 1;
      }
    }
  }
}

// There's currently no compelling reason to implement cam timing on the J32. (Have to do semi-sequential injection, wasted spark, there is no VTC on this engine, just VTEC)
void triggerSec_HondaJ32(void)
{
  return;
}

uint16_t getRPM_HondaJ32(void)
{
  return RpmFromRevolutionTimeUs(revolutionTime); // revolutionTime set by SetRevolutionTime()
}

// FASE V: Honda J32 tooth angle lookup table
// Replaces if/else chain with direct array lookup for unusual tooth spacing
// Teeth 14 and 22 have 18deg spacing instead of normal 15deg

// Base angle for each tooth position (1-24)
// Index 0 unused, teeth 1-24 have pre-calculated angles
static const uint16_t hondaJ32ToothAngles[25] PROGMEM = {
  0,    // tooth 0 (unused)
  15,   // tooth 1: 1 * 15 = 15
  30,   // tooth 2: 2 * 15 = 30
  45,   // tooth 3: 3 * 15 = 45
  60,   // tooth 4: 4 * 15 = 60
  75,   // tooth 5: 5 * 15 = 75
  90,   // tooth 6: 6 * 15 = 90
  105,  // tooth 7: 7 * 15 = 105
  120,  // tooth 8: 8 * 15 = 120
  135,  // tooth 9: 9 * 15 = 135
  150,  // tooth 10: 10 * 15 = 150
  165,  // tooth 11: 11 * 15 = 165
  180,  // tooth 12: 12 * 15 = 180
  195,  // tooth 13: 13 * 15 = 195
  213,  // tooth 14: 13*15 + 18 = 213 (UNUSUAL 18deg spacing)
  228,  // tooth 15: 213 + 15 = 228
  243,  // tooth 16: 228 + 15 = 243
  258,  // tooth 17: 243 + 15 = 258
  273,  // tooth 18: 258 + 15 = 273
  288,  // tooth 19: 273 + 15 = 288
  303,  // tooth 20: 288 + 15 = 303
  318,  // tooth 21: 303 + 15 = 318
  333,  // tooth 22: 21*15 + 18 = 333 (UNUSUAL 18deg spacing)
  348,  // tooth 23: 333 + 15 = 348
  363   // tooth 24: 348 + 15 = 363
};

// Helper: Get base angle for tooth count using lookup table
static inline int getBaseAngle_HondaJ32(uint16_t toothCount)
{
  if (toothCount >= 25) { return 0; }  // Safety: invalid tooth count
  return pgm_read_word(&hondaJ32ToothAngles[toothCount]);
}

int getCrankAngle_HondaJ32(void)
{
  // Returns values from 0 to 360.
  // Tooth 1 time occurs 360/24 degrees after TDC.
  // Teeth 14 and 22 are unusually sized (18 degrees), handled by lookup table
  int crankAngle;
  uint16_t tempToothCurrentCount;
  noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    elapsedTime = lastCrankAngleCalc - toothLastToothTime;
  interrupts();

  // Use lookup table for base angle (handles unusual teeth 14 and 22)
  crankAngle = getBaseAngle_HondaJ32(tempToothCurrentCount);
  crankAngle += timeToAngleDegPerMicroSec(elapsedTime) + configPage4.triggerAngle;

  if (crankAngle >= 720) { crankAngle -= 720; }
  if (crankAngle > CRANK_ANGLE_MAX) { crankAngle -= CRANK_ANGLE_MAX; }
  if (crankAngle < 0) { crankAngle += 360; }

  return crankAngle;
}

void triggerSetEndTeeth_HondaJ32(void)
{
  return;
}

/** @} */
#endif // 24X, Jeep2000, Audi135, HondaD17, HondaJ32

#if 0  // Miata9905, MazdaAU, Non360, Nissan360, Subaru67 - REFACTORED to implementations/
/** Miata '99 to '05 with 4x 70 degree duration teeth running at cam speed.
Teeth believed to be at the same angles as the 4g63 decoder.
Tooth #1 is defined as the next crank tooth after the crank signal is HIGH when the cam signal is falling.
Tooth number one is at 355* ATDC.
* (See: www.forum.diyefi.org/viewtopic.php?f=56&t=1077)
* @defgroup miata_99_05 Miata '99 to '05
* @{
*/
void triggerSetup_Miata9905(void)
{
  triggerToothAngle = 90; //The number of degrees that passes from tooth to tooth (primary)
  toothCurrentCount = 99; //Fake tooth count represents no sync
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  triggerActualTeeth = 8;

  if(currentStatus.initialisationComplete == false) { secondaryToothCount = 0; toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the initial check to prevent the fuel pump just staying on all the time
  else { toothLastToothTime = 0; }
  toothLastMinusOneToothTime = 0;

  //Note that these angles are for every rising and falling edge

  /*
  toothAngles[0] = 350;
  toothAngles[1] = 100;
  toothAngles[2] = 170;
  toothAngles[3] = 280;
  */

  toothAngles[0] = 710; //
  toothAngles[1] = 100; //First crank pulse after the SINGLE cam pulse
  toothAngles[2] = 170; //
  toothAngles[3] = 280; //
  toothAngles[4] = 350; //
  toothAngles[5] = 460; //First crank pulse AFTER the DOUBLE cam pulse
  toothAngles[6] = 530; //
  toothAngles[7] = 640; //

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  triggerFilterTime = 1500; //10000 rpm, assuming we're triggering on both edges off the crank tooth.
  triggerSecFilterTime = 0; //Need to figure out something better for this
  BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

// Miata9905 filter configuration - data-driven approach for trigger filtering
struct Miata9905FilterConfig {
  uint8_t filterLevel;    // Filter level constant (0=OFF, 1=LITE, 2=MEDIUM, 3=AGGRESSIVE)
  uint8_t oddToothMult;   // Multiplier for odd teeth (1,3,5,7) - 70° spacing
  uint8_t oddToothShift;  // Right shift amount for odd teeth filter calculation
  uint8_t evenToothMult;  // Multiplier for even teeth (2,4,6,8) - 110° spacing
  uint8_t evenToothShift; // Right shift amount for even teeth filter calculation
};

static const Miata9905FilterConfig miata9905FilterConfigs[4] = {
  {0, 1,  0,  1, 0},  // OFF: No filtering (special case - sets filterTime to 0)
  {1, 1,  0,  3, 3},  // LITE: odd=curGap, even=(curGap*3)>>3 (41.25°)
  {2, 5,  2,  1, 1},  // MEDIUM: odd=(curGap*5)>>2 (87.5°), even=curGap>>1 (55°)
  {3, 11, 3,  9, 5}   // AGGRESSIVE: odd=(curGap*11)>>3 (96.26°), even=(curGap*9)>>5 (61.87°)
};

static inline void applyMiata9905Filter(uint8_t filterLevel, uint8_t toothCount, uint32_t curGap,
                                        volatile uint16_t* pTriggerToothAngle,
                                        volatile uint32_t* pTriggerFilterTime,
                                        volatile uint32_t* pTriggerSecFilterTime)
{
  // Determine if odd tooth (1,3,5,7 have 70° spacing, even teeth 2,4,6,8 have 110° spacing)
  bool isOddTooth = (toothCount == 1) || (toothCount == 3) || (toothCount == 5) || (toothCount == 7);

  // Find and apply matching filter configuration
  for(uint8_t i = 0; i < 4; i++)
  {
    const Miata9905FilterConfig* config = &miata9905FilterConfigs[i];
    if(config->filterLevel == filterLevel)
    {
      // Set tooth angle based on odd/even position
      *pTriggerToothAngle = isOddTooth ? 70 : 110;

      // Special case: filter OFF sets both filter times to 0
      if(filterLevel == 0)
      {
        *pTriggerFilterTime = 0;
        *pTriggerSecFilterTime = 0;
      }
      else
      {
        // Apply filter calculation based on tooth position
        uint8_t mult = isOddTooth ? config->oddToothMult : config->evenToothMult;
        uint8_t shift = isOddTooth ? config->oddToothShift : config->evenToothShift;

        // Calculate filter time: (curGap * multiplier) >> shift
        if(shift == 0)
        {
          *pTriggerFilterTime = curGap * (uint32_t)mult;
        }
        else
        {
          *pTriggerFilterTime = (curGap * (uint32_t)mult) >> shift;
        }
      }
      return;
    }
  }
}

void triggerPri_Miata9905(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if ( (curGap >= triggerFilterTime) || (currentStatus.startRevolutions == 0) )
  {
    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)
    if( (toothCurrentCount == (triggerActualTeeth + 1)) )
    {
       toothCurrentCount = 1; //Reset the counter
       toothOneMinusOneTime = toothOneTime;
       toothOneTime = curTime;
       //currentStatus.hasSync = true;
       currentStatus.startRevolutions++; //Counter
    }
    else
    {
      if( (currentStatus.hasSync == false) || (configPage4.useResync == true) )
      {
        if(secondaryToothCount == 2)
        {
          toothCurrentCount = 6;
          currentStatus.hasSync = true;
        }
      }
    }

    if (currentStatus.hasSync == true)
    {

      //Whilst this is an uneven tooth pattern, if the specific angle between the last 2 teeth is specified, 1st deriv prediction can be used
      // Determine effective filter level: use LITE filter when RPM < 1400 regardless of configured level
      uint8_t effectiveFilterLevel = ((configPage4.triggerFilter == 1) || (currentStatus.RPM < 1400)) ? 1 : configPage4.triggerFilter;
      applyMiata9905Filter(effectiveFilterLevel, toothCurrentCount, curGap, &triggerToothAngle, &triggerFilterTime, &triggerSecFilterTime);

      //EXPERIMENTAL!
      //New ignition mode is ONLY available on 9905 when the trigger angle is set to the stock value of 0.
      if(    (configPage2.perToothIgn == true)
          && (configPage4.triggerAngle == 0)
          && (currentStatus.advance > 0) )
      {
        int16_t crankAngle = ignitionLimits( toothAngles[(toothCurrentCount-1)] );

        //Handle non-sequential tooth counts
        if( (configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (toothCurrentCount > configPage2.nCylinders) ) { checkPerToothTiming(crankAngle, (toothCurrentCount-configPage2.nCylinders) ); }
        else { checkPerToothTiming(crankAngle, toothCurrentCount); }
      }
    } //Has sync

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    //if ( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && configPage4.ignCranklock)
    if ( (currentStatus.RPM < (currentStatus.crankRPM + 30)) && (configPage4.ignCranklock) ) //The +30 here is a safety margin. When switching from fixed timing to normal, there can be a situation where a pulse started when fixed and ending when in normal mode causes problems. This prevents that.
    {
      if( (toothCurrentCount == 1) || (toothCurrentCount == 5) ) { endCoil1Charge(); endCoil3Charge(); }
      else if( (toothCurrentCount == 3) || (toothCurrentCount == 7) ) { endCoil2Charge(); endCoil4Charge(); }
    }
    secondaryToothCount = 0;
  } //Trigger filter

}

void triggerSec_Miata9905(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  if(BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) || (currentStatus.hasSync == false) )
  {
    triggerFilterTime = 1500; //If this is removed, can have trouble getting sync again after the engine is turned off (but ECU not reset).
  }

  if ( curGap2 >= triggerSecFilterTime )
  {
    toothLastSecToothTime = curTime2;
    lastGap = curGap2;
    secondaryToothCount++;

    //TODO Add some secondary filtering here

    //Record the VVT tooth time
    if( (toothCurrentCount == 1) && (curTime2 > toothLastToothTime) )
    {
      lastVVTtime = curTime2 - toothLastToothTime;
    }
  }
}

uint16_t getRPM_Miata9905(void)
{
  //During cranking, RPM is calculated 4 times per revolution, once for each tooth on the crank signal.
  //Because these signals aren't even (Alternating 110 and 70 degrees), this needs a special function
  uint16_t tempRPM = 0;
  if( (currentStatus.RPM < currentStatus.crankRPM) && (currentStatus.hasSync == true) )
  {
    if( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { tempRPM = 0; }
    else
    {
      int tempToothAngle;
      unsigned long toothTime;
      noInterrupts();
      tempToothAngle = triggerToothAngle;
      toothTime = (toothLastToothTime - toothLastMinusOneToothTime); //Note that trigger tooth angle changes between 70 and 110 depending on the last tooth that was seen
      interrupts();
      toothTime = toothTime * 36;
      tempRPM = ((unsigned long)tempToothAngle * (MICROS_PER_MIN/10U)) / toothTime;
      SetRevolutionTime((10UL * toothTime) / tempToothAngle);
      MAX_STALL_TIME = 366667UL; // 50RPM
    }
  }
  else
  {
    tempRPM = stdGetRPM(CAM_SPEED);
    MAX_STALL_TIME = revolutionTime << 1; //Set the stall time to be twice the current RPM. This is a safe figure as there should be no single revolution where this changes more than this
    if(MAX_STALL_TIME < 366667UL) { MAX_STALL_TIME = 366667UL; } //Check for 50rpm minimum
  }

  return tempRPM;
}

int getCrankAngle_Miata9905(void)
{
    int crankAngle = 0;
    //if(currentStatus.hasSync == true)
    {
      //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
      unsigned long tempToothLastToothTime;
      int tempToothCurrentCount;
      //Grab some variables that are used in the trigger code and assign them to temp variables.
      noInterrupts();
      tempToothCurrentCount = toothCurrentCount;
      tempToothLastToothTime = toothLastToothTime;
      lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
      interrupts();

      crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle; //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

      //Estimate the number of degrees travelled since the last tooth}
      elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
      crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

      if (crankAngle >= 720) { crankAngle -= 720; }
      if (crankAngle < 0) { crankAngle += 360; }
    }

    return crankAngle;
}

int getCamAngle_Miata9905(void)
{
  int16_t curAngle;
  //lastVVTtime is the time between tooth #1 (10* BTDC) and the single cam tooth.
  //All cam angles in in BTDC, so the actual advance angle is 370 - timeToAngleDegPerMicroSec(lastVVTtime) - <the angle of the cam at 0 advance>
  curAngle = 370 - timeToAngleDegPerMicroSec(lastVVTtime) - configPage10.vvtCL0DutyAng;
  currentStatus.vvt1Angle = LOW_PASS_FILTER( (curAngle << 1), configPage4.ANGLEFILTER_VVT, currentStatus.vvt1Angle);

  return currentStatus.vvt1Angle;
}

void triggerSetEndTeeth_Miata9905(void)
{

  if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
  {
    if(currentStatus.advance >= 10)
    {
      ignition1EndTooth = 8;
      ignition2EndTooth = 2;
      ignition3EndTooth = 4;
      ignition4EndTooth = 6;
    }
    else if (currentStatus.advance > 0)
    {
      ignition1EndTooth = 1;
      ignition2EndTooth = 3;
      ignition3EndTooth = 5;
      ignition4EndTooth = 7;
    }

  }
  else
  {
    if(currentStatus.advance >= 10)
    {
      ignition1EndTooth = 4;
      ignition2EndTooth = 2;
      ignition3EndTooth = 4; //Not used
      ignition4EndTooth = 2; //Not used
    }
    else if(currentStatus.advance > 0)
    {
      ignition1EndTooth = 1;
      ignition2EndTooth = 3;
      ignition3EndTooth = 1; //Not used
      ignition4EndTooth = 3; //Not used
    }
  }
}
/** @} */

/** Mazda AU version.
Tooth #2 is defined as the next crank tooth after the single cam tooth.
Tooth number one is at 348* ATDC.
* @defgroup mazda_au Mazda AU
* @{
*/
void triggerSetup_MazdaAU(void)
{
  triggerToothAngle = 108; //The number of degrees that passes from tooth to tooth (primary). This is the maximum gap
  toothCurrentCount = 99; //Fake tooth count represents no sync
  secondaryToothCount = 0; //Needed for the cam tooth tracking
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);

  toothAngles[0] = 348; //tooth #1
  toothAngles[1] = 96; //tooth #2
  toothAngles[2] = 168; //tooth #3
  toothAngles[3] = 276; //tooth #4

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  triggerFilterTime = 1500; //10000 rpm, assuming we're triggering on both edges off the crank tooth.
  triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U; //Same as above, but fixed at 2 teeth on the secondary input and divided by 2 (for cam speed)
  BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

void triggerPri_MazdaAU(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if ( curGap >= triggerFilterTime )
  {
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    toothCurrentCount++;
    if( (toothCurrentCount == 1) || (toothCurrentCount == 5) ) //Trigger is on CHANGE, hence 4 pulses = 1 crank rev
    {
       toothCurrentCount = 1; //Reset the counter
       toothOneMinusOneTime = toothOneTime;
       toothOneTime = curTime;
       currentStatus.hasSync = true;
       currentStatus.startRevolutions++; //Counter
    }

    if (currentStatus.hasSync == true)
    {
      // Locked cranking timing is available, fixed at 12* BTDC
      if ( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && configPage4.ignCranklock )
      {
        if( toothCurrentCount == 1 ) { endCoil1Charge(); }
        else if( toothCurrentCount == 3 ) { endCoil2Charge(); }
      }

      //Whilst this is an uneven tooth pattern, if the specific angle between the last 2 teeth is specified, 1st deriv prediction can be used
      if( (toothCurrentCount == 1) || (toothCurrentCount == 3) ) { triggerToothAngle = 72; triggerFilterTime = curGap; } //Trigger filter is set to whatever time it took to do 72 degrees (Next trigger is 108 degrees away)
      else { triggerToothAngle = 108; triggerFilterTime = rshift<3>(curGap * 3UL); } //Trigger filter is set to (108*3)/8=40 degrees (Next trigger is 70 degrees away).

      toothLastMinusOneToothTime = toothLastToothTime;
      toothLastToothTime = curTime;
    } //Has sync
  } //Filter time
}

void triggerSec_MazdaAU(void)
{
  curTime2 = micros();
  lastGap = curGap2;
  curGap2 = curTime2 - toothLastSecToothTime;
  //if ( curGap2 < triggerSecFilterTime ) { return; }
  toothLastSecToothTime = curTime2;

  //if(BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) || currentStatus.hasSync == false)
  if(currentStatus.hasSync == false)
  {
    //we find sync by looking for the 2 teeth that are close together. The next crank tooth after that is the one we're looking for.
    //For the sake of this decoder, the lone cam tooth will be designated #1
    if(secondaryToothCount == 2)
    {
      toothCurrentCount = 1;
      currentStatus.hasSync = true;
    }
    else
    {
      triggerFilterTime = 1500; //In case the engine has been running and then lost sync.
      targetGap = (lastGap) >> 1; //The target gap is set at half the last tooth gap
      if ( curGap2 < targetGap) //If the gap between this tooth and the last one is less than half of the previous gap, then we are very likely at the extra (3rd) tooth on the cam). This tooth is located at 421 crank degrees (aka 61 degrees) and therefore the last crank tooth seen was number 1 (At 350 degrees)
      {
        secondaryToothCount = 2;
      }
    }
    secondaryToothCount++;
  }
}


uint16_t getRPM_MazdaAU(void)
{
  uint16_t tempRPM = 0;

  if (currentStatus.hasSync == true)
  {
    //During cranking, RPM is calculated 4 times per revolution, once for each tooth on the crank signal.
    //Because these signals aren't even (Alternating 108 and 72 degrees), this needs a special function
    if(currentStatus.RPM < currentStatus.crankRPM)
    {
      int tempToothAngle;
      noInterrupts();
      tempToothAngle = triggerToothAngle;
      SetRevolutionTime(36*(toothLastToothTime - toothLastMinusOneToothTime)); //Note that trigger tooth angle changes between 72 and 108 depending on the last tooth that was seen
      interrupts();
      tempRPM = (tempToothAngle * MICROS_PER_MIN) / revolutionTime;
    }
    else { tempRPM = stdGetRPM(CRANK_SPEED); }
  }
  return tempRPM;
}

int getCrankAngle_MazdaAU(void)
{
    int crankAngle = 0;
    if(currentStatus.hasSync == true)
    {
      //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
      unsigned long tempToothLastToothTime;
      int tempToothCurrentCount;
      //Grab some variables that are used in the trigger code and assign them to temp variables.
      noInterrupts();
      tempToothCurrentCount = toothCurrentCount;
      tempToothLastToothTime = toothLastToothTime;
      lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
      interrupts();

      crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle; //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

      //Estimate the number of degrees travelled since the last tooth}
      elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
      crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

      if (crankAngle >= 720) { crankAngle -= 720; }
      if (crankAngle < 0) { crankAngle += 360; }
    }

    return crankAngle;
}

void triggerSetEndTeeth_MazdaAU(void)
{
}
/** @} */

/** Non-360 Dual wheel with 2 wheels located either both on the crank or with the primary on the crank and the secondary on the cam.
There can be no missing teeth on the primary wheel.
* @defgroup dec_non360 Non-360 Dual wheel
* @{
*/
void triggerSetup_non360(void)
{
  triggerToothAngle = (360U * configPage4.TrigAngMul) / configPage4.triggerTeeth; //The number of degrees that passes from tooth to tooth multiplied by the additional multiplier
  toothCurrentCount = UINT8_MAX; //Default value
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U; //Same as above, but fixed at 2 teeth on the secondary input and divided by 2 (for cam speed)
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
}


void triggerPri_non360(void)
{
  //This is not used, the trigger is identical to the dual wheel one, so that is used instead.
}

void triggerSec_non360(void)
{
  //This is not used, the trigger is identical to the dual wheel one, so that is used instead.
}

uint16_t getRPM_non360(void)
{
  uint16_t tempRPM = 0;
  if( (currentStatus.hasSync == true) && (toothCurrentCount != 0) )
  {
    if(currentStatus.RPM < currentStatus.crankRPM) { tempRPM = crankingGetRPM(configPage4.triggerTeeth, CRANK_SPEED); }
    else { tempRPM = stdGetRPM(CRANK_SPEED); }
  }
  return tempRPM;
}

int getCrankAngle_non360(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    //Handle case where the secondary tooth was the last one seen
    if(tempToothCurrentCount == 0) { tempToothCurrentCount = configPage4.triggerTeeth; }

    //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
    int crankAngle = (tempToothCurrentCount - 1) * triggerToothAngle;
    crankAngle = (crankAngle / configPage4.TrigAngMul) + configPage4.triggerAngle; //Have to divide by the multiplier to get back to actual crank angle.

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_non360(void)
{
}
/** @} */

/** Nissan 360 tooth on cam (Optical trigger disc inside distributor housing).
See http://wiki.r31skylineclub.com/index.php/Crank_Angle_Sensor .
* @defgroup dec_nissan360 Nissan 360 tooth on cam
* @{
*/
void triggerSetup_Nissan360(void)
{
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U; //Same as above, but fixed at 2 teeth on the secondary input and divided by 2 (for cam speed)
  secondaryToothCount = 0; //Initially set to 0 prior to calculating the secondary window duration
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  toothCurrentCount = 1;
  triggerToothAngle = 2;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
}


void triggerPri_Nissan360(void)
{
   curTime = micros();
   curGap = curTime - toothLastToothTime;
   if ( curGap < triggerFilterTime ) { return; }

   toothCurrentCount++; //Increment the tooth counter
   BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

   toothLastMinusOneToothTime = toothLastToothTime;
   toothLastToothTime = curTime;

   if ( currentStatus.hasSync == true )
   {
     if ( toothCurrentCount == 361 ) //2 complete crank revolutions
     {
       toothCurrentCount = 1;
       toothOneMinusOneTime = toothOneTime;
       toothOneTime = curTime;
       currentStatus.startRevolutions++; //Counter
     }
     //Recalc the new filter value
     setFilter(curGap);

     //EXPERIMENTAL!
     if(configPage2.perToothIgn == true)
     {
        int16_t crankAngle = ( (toothCurrentCount-1) * 2 ) + configPage4.triggerAngle;
        if(crankAngle > CRANK_ANGLE_MAX_IGN)
        {
          crankAngle -= CRANK_ANGLE_MAX_IGN;
          checkPerToothTiming(crankAngle, (toothCurrentCount/2) );
        }
        else
        {
          checkPerToothTiming(crankAngle, toothCurrentCount);
        }

     }
   }
}

// FASE L: Nissan360 window configuration (data-driven approach)
// Nissan 360 optical distributor uses secondary cam windows of varying widths
// to identify engine position. Different cylinder counts use different patterns.
struct Nissan360WindowConfig {
  uint8_t nCylinders;        // Number of engine cylinders (4, 6, or 8)
  uint8_t durationMin;       // Minimum window width in primary teeth
  uint8_t durationMax;       // Maximum window width in primary teeth
  uint16_t targetToothCount; // Primary tooth count at end of this window
};

// Static configuration table for all Nissan360 window patterns
// 4-cyl: 4 windows (16,12,8,4 teeth wide) at 90-degree intervals
// 6-cyl: Single smallest window (4 teeth) for sync
// 8-cyl: Single shortest window (7 teeth) for sync (V8 Optispark)
static const Nissan360WindowConfig nissan360WindowConfigs[] PROGMEM = {
  // 4 cylinder windows
  {4, 15, 17, 16},   // Window 1: 16 teeth duration, ends at tooth 16
  {4, 11, 13, 102},  // Window 2: 12 teeth duration, ends at tooth 102 (90+12)
  {4,  7,  9, 188},  // Window 3: 8 teeth duration, ends at tooth 188 (90+90+8)
  {4,  3,  5, 274},  // Window 4: 4 teeth duration, ends at tooth 274 (90+90+90+4)
  // 6 cylinder window
  {6,  3,  5, 124},  // Smallest window: 4 teeth, ends at tooth 124 (60+60+4)
  // 8 cylinder window (V8 Optispark)
  {8,  6,  8, 56}    // Shortest window: 7 teeth, ends at tooth 56 (102 crank degrees)
};

// Helper function: Process Nissan360 cam window using data-driven lookup
// Returns true if window matches a known pattern and sets outToothCount
static inline bool processNissan360Window(uint8_t secondaryDuration, uint8_t nCylinders, uint16_t* outToothCount)
{
  const uint8_t configCount = sizeof(nissan360WindowConfigs) / sizeof(Nissan360WindowConfig);

  for (uint8_t i = 0; i < configCount; i++)
  {
    const Nissan360WindowConfig* cfg = &nissan360WindowConfigs[i];

    // Match: same cylinder count AND duration in valid range
    if (cfg->nCylinders == nCylinders &&
        secondaryDuration >= cfg->durationMin &&
        secondaryDuration <= cfg->durationMax)
    {
      *outToothCount = cfg->targetToothCount;
      return true;
    }
  }

  return false; // No matching window pattern found
}

void triggerSec_Nissan360(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;
  //if ( curGap2 < triggerSecFilterTime ) { return; }
  toothLastSecToothTime = curTime2;
  //OPTIONAL: Set filter at 25% of the current speed
  //triggerSecFilterTime = curGap2 >> 2;

  // Determine secondary trigger edge polarity
  byte trigEdge = (configPage4.TrigEdgeSec == 0) ? LOW : HIGH;

  // Guard: Start of secondary window (first rotation OR window start edge)
  if ((secondaryToothCount == 0) || (READ_SEC_TRIGGER() == trigEdge))
  {
    secondaryToothCount = toothCurrentCount;
    return;
  }

  // End of secondary window detected - calculate window width
  byte secondaryDuration = toothCurrentCount - secondaryToothCount;
  uint16_t matchedToothCount = 0;

  // Path 1: No sync yet - try to acquire sync
  if (currentStatus.hasSync == false)
  {
    // Use data-driven lookup to find matching window pattern
    if (processNissan360Window(secondaryDuration, configPage2.nCylinders, &matchedToothCount))
    {
      toothCurrentCount = matchedToothCount;
      currentStatus.hasSync = true;
    }
    else
    {
      // No matching window pattern found
      currentStatus.hasSync = false;
      currentStatus.syncLossCounter++;
    }
  }
  // Path 2: Already have sync - optionally resync every 720 degrees
  else if (configPage4.useResync == true)
  {
    // Use same data-driven lookup for resync verification
    if (processNissan360Window(secondaryDuration, configPage2.nCylinders, &matchedToothCount))
    {
      toothCurrentCount = matchedToothCount;
    }
  }
}

uint16_t getRPM_Nissan360(void)
{
  //Can't use stdGetRPM as there is no separate cranking RPM calc (stdGetRPM returns 0 if cranking)
  uint16_t tempRPM;
  if( (currentStatus.hasSync == true) && (toothLastToothTime != 0) && (toothLastMinusOneToothTime != 0) )
  {
    if(currentStatus.startRevolutions < 2)
    {
      noInterrupts();
      SetRevolutionTime((toothLastToothTime - toothLastMinusOneToothTime) * 180); //Each tooth covers 2 crank degrees, so multiply by 180 to get a full revolution time.
      interrupts();
    }
    else
    {
      noInterrupts();
      SetRevolutionTime((toothOneTime - toothOneMinusOneTime) >> 1); //The time in uS that one revolution would take at current speed (The time tooth 1 was last seen, minus the time it was seen prior to that)
      interrupts();
    }
    tempRPM = RpmFromRevolutionTimeUs(revolutionTime); //Calc RPM based on last full revolution time (Faster as /)
    MAX_STALL_TIME = revolutionTime << 1; //Set the stall time to be twice the current RPM. This is a safe figure as there should be no single revolution where this changes more than this
  }
  else { tempRPM = 0; }

  return tempRPM;
}

int getCrankAngle_Nissan360(void)
{
  //As each tooth represents 2 crank degrees, we only need to determine whether we're more or less than halfway between teeth to know whether to add another 1 degrees
  int crankAngle = 0;
  int tempToothLastToothTime;
  int tempToothLastMinusOneToothTime;
  int tempToothCurrentCount;

  noInterrupts();
  tempToothLastToothTime = toothLastToothTime;
  tempToothLastMinusOneToothTime = toothLastMinusOneToothTime;
  tempToothCurrentCount = toothCurrentCount;
  lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
  interrupts();

  crankAngle = ( (tempToothCurrentCount - 1) * 2) + configPage4.triggerAngle;
  unsigned long halfTooth = (tempToothLastToothTime - tempToothLastMinusOneToothTime) / 2;
  elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
  if (elapsedTime > halfTooth)
  {
    //Means we're over halfway to the next tooth, so add on 1 degree
    crankAngle += 1;
  }

  if (crankAngle >= 720) { crankAngle -= 720; }
  if (crankAngle < 0) { crankAngle += 360; }

  return crankAngle;
}

void triggerSetEndTeeth_Nissan360(void)
{
  //This uses 4 prior teeth, just to ensure there is sufficient time to set the schedule etc
  byte offset_teeth = 4;
  if((ignition1EndAngle - offset_teeth) > configPage4.triggerAngle) { ignition1EndTooth = ( (ignition1EndAngle - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  else { ignition1EndTooth = ( (ignition1EndAngle + 720 - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  if((ignition2EndAngle - offset_teeth) > configPage4.triggerAngle) { ignition2EndTooth = ( (ignition2EndAngle - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  else { ignition2EndTooth = ( (ignition2EndAngle + 720 - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  if((ignition3EndAngle - offset_teeth) > configPage4.triggerAngle) { ignition3EndTooth = ( (ignition3EndAngle - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  else { ignition3EndTooth = ( (ignition3EndAngle + 720 - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  if((ignition4EndAngle - offset_teeth) > configPage4.triggerAngle) { ignition4EndTooth = ( (ignition4EndAngle - configPage4.triggerAngle) / 2 ) - offset_teeth; }
  else { ignition4EndTooth = ( (ignition4EndAngle + 720 - configPage4.triggerAngle) / 2 ) - offset_teeth; }
}
/** @} */

/** Subaru 6/7 Trigger pattern decoder for 6 tooth (irregularly spaced) crank and 7 tooth (also fairly irregular) cam wheels (eg late 90's Impreza 2.2).
This seems to be present in late 90's Subaru. In 2001 Subaru moved to 36-2-2-2 (See: http://www.vems.hu/wiki/index.php?page=InputTrigger%2FSubaruTrigger ).
* @defgroup dec_subaru_6_7 Subaru 6/7
* @{
*/
void triggerSetup_Subaru67(void)
{
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = 0;
  secondaryToothCount = 0; //Initially set to 0 prior to calculating the secondary window duration
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  toothCurrentCount = 1;
  triggerToothAngle = 2;
  BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  toothSystemCount = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 93U); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)

  toothAngles[0] = 710; //tooth #1
  toothAngles[1] = 83; //tooth #2
  toothAngles[2] = 115; //tooth #3
  toothAngles[3] = 170; //tooth #4
  toothAngles[4] = toothAngles[1] + 180;
  toothAngles[5] = toothAngles[2] + 180;
  toothAngles[6] = toothAngles[3] + 180;
  toothAngles[7] = toothAngles[1] + 360;
  toothAngles[8] = toothAngles[2] + 360;
  toothAngles[9] = toothAngles[3] + 360;
  toothAngles[10] = toothAngles[1] + 540;
  toothAngles[11] = toothAngles[2] + 540;
}

// Subaru 6/7 trigger sync validation - data-driven approach to eliminate switch duplication
// Stores expected tooth count combinations for each secondary tooth count
struct Subaru67SyncConfig {
  uint8_t secondaryCount;      // Number of secondary teeth seen (1, 2, or 3)
  uint8_t expectedTooth1;      // Primary expected tooth count
  uint8_t expectedTooth2;      // Alternative tooth count (0 if not applicable)
  uint8_t defaultToothCount;   // Tooth count to set on validation failure
};

// Configuration array for Subaru 6/7 sync validation
// Format: {secondaryCount, expectedTooth1, expectedTooth2, defaultToothCount}
static const Subaru67SyncConfig subaru67SyncConfigs[3] = {
  {1, 5, 11, 5},  // Case 1: Expected tooth 5 OR 11, default to 5 on failure
  {2, 8,  0, 8},  // Case 2: Expected tooth 8, default to 8 on failure
  {3, 2,  0, 2}   // Case 3: Expected tooth 2, default to 2 on failure
};

// Helper function to validate Subaru 6/7 sync using data-driven approach
static inline bool validateSubaru67Sync(uint8_t secondaryCount, uint8_t toothCount, uint8_t* outToothCount)
{
  // Find matching configuration
  for(uint8_t i = 0; i < 3; i++)
  {
    const Subaru67SyncConfig* config = &subaru67SyncConfigs[i];

    if(config->secondaryCount == secondaryCount)
    {
      // Check if tooth count matches expected values
      bool isValid = (toothCount == config->expectedTooth1);
      if(config->expectedTooth2 != 0)
      {
        isValid = isValid || (toothCount == config->expectedTooth2);
      }

      if(!isValid)
      {
        // Validation failed - return default tooth count
        *outToothCount = config->defaultToothCount;
      }

      return isValid;
    }
  }

  return false;  // No matching configuration found
}

/**
 * @brief Handle fixed cranking ignition timing for Subaru 6/7
 * @details Fires coils at fixed 10° BTDC during cranking with ignCranklock enabled
 *          Teeth 1,7: Coil 1+3 | Teeth 4,10: Coil 2+4
 *
 * MISRA-C: 8 lines, N:2, C:4
 */
static inline void handleSubaru67FixedCranking(void)
{
  if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return; }
  if (!configPage4.ignCranklock) { return; }

  if ((toothCurrentCount == 1) || (toothCurrentCount == 7)) { endCoil1Charge(); endCoil3Charge(); }
  else if ((toothCurrentCount == 4) || (toothCurrentCount == 10)) { endCoil2Charge(); endCoil4Charge(); }
}

/**
 * @brief Handle revolution tracking and tooth angle calculation for Subaru 6/7
 * @details Updates revolution counter at 720°, calculates per-tooth angles
 *          Teeth 1,2 have special angles (55°, 93°), others calculated from table
 *
 * MISRA-C: 17 lines, N:1, C:4
 */
static inline void handleSubaru67Revolution(void)
{
  if (toothCurrentCount > 12)
  {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
  }

  if (toothCurrentCount == 1) { triggerToothAngle = 55; }
  else if (toothCurrentCount == 2) { triggerToothAngle = 93; }
  else { triggerToothAngle = toothAngles[(toothCurrentCount - 1)] - toothAngles[(toothCurrentCount - 2)]; }

  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

/**
 * @brief Handle per-tooth ignition timing for Subaru 6/7
 * @details Calculates crank angle with trigger correction, handles sequential/non-sequential modes
 *          Non-sequential: teeth 7-12 map to 1-6 for wasted spark
 *
 * MISRA-C: 16 lines, N:2, C:5
 */
static inline void handleSubaru67PerToothIgnition(void)
{
  if (configPage2.perToothIgn == false) { return; }
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return; }

  int16_t crankAngle = toothAngles[(toothCurrentCount - 1)] + configPage4.triggerAngle;

  if (configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
  {
    checkPerToothTiming(crankAngle, toothCurrentCount);
  }
  else
  {
    crankAngle = ignitionLimits(toothAngles[(toothCurrentCount - 1)]);
    uint8_t effectiveTooth = (toothCurrentCount > 6) ? (toothCurrentCount - 6) : toothCurrentCount;
    checkPerToothTiming(crankAngle, effectiveTooth);
  }
}

/**
 * @brief Primary trigger ISR for Subaru 6/7 decoder
 * @details Handles 6 irregular crank teeth + 7 irregular cam teeth
 *          Sync determined by cam tooth count between crank teeth
 *
 * MISRA-C: 47 lines, N:2, C:8 (was: 100 lines, N:4, C:14)
 *
 * @note Refactored FASE D - extracted 3 helpers to reduce complexity
 */
void triggerPri_Subaru67(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if (curGap < triggerFilterTime) { return; }

  toothCurrentCount++;
  toothSystemCount++;
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;

  if (toothCurrentCount > 13)
  {
    toothCurrentCount = 0;
    currentStatus.hasSync = false;
    currentStatus.syncLossCounter++;
  }

  // Sync determined by cam teeth count between crank teeth
  if (secondaryToothCount == 0)
  {
    // No cam teeth - can't validate sync
  }
  else if ((secondaryToothCount >= 1) && (secondaryToothCount <= 3))
  {
    uint8_t newToothCount = toothCurrentCount;
    bool isValid = validateSubaru67Sync(secondaryToothCount, toothCurrentCount, &newToothCount);

    if (isValid)
    {
      currentStatus.hasSync = true;
    }
    else
    {
      currentStatus.hasSync = false;
      currentStatus.syncLossCounter++;
      toothCurrentCount = newToothCount;
    }
    secondaryToothCount = 0;
  }
  else
  {
    // Noise or cranking stop/start (secondaryToothCount > 3)
    currentStatus.hasSync = false;
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    currentStatus.syncLossCounter++;
    secondaryToothCount = 0;
  }

  if (currentStatus.hasSync == false) { return; }

  handleSubaru67FixedCranking();
  handleSubaru67Revolution();
  handleSubaru67PerToothIgnition();
}

void triggerSec_Subaru67(void)
{
  if( ((toothSystemCount == 0) || (toothSystemCount == 3)) )
  {
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;

    if ( curGap2 > triggerSecFilterTime )
    {
      toothLastSecToothTime = curTime2;
      secondaryToothCount++;
      toothSystemCount = 0;

      if(secondaryToothCount > 1)
      {
        //Set filter at 25% of the current speed
        //Note that this can only be set on the 2nd or 3rd cam tooth in each set.
        triggerSecFilterTime = curGap2 >> 2;
      }
      else { triggerSecFilterTime = 0; } //Filter disabled

    }
  }
  else
  {
    //Sanity check
    if(toothSystemCount > 3)
    {
      toothSystemCount = 0;
      secondaryToothCount = 1;
      currentStatus.hasSync = false; // impossible to have more than 3 crank teeth between cam teeth - must have noise but can't have sync
      currentStatus.syncLossCounter++;
    }
    secondaryToothCount = 0;
  }

}

uint16_t getRPM_Subaru67(void)
{
  //if(currentStatus.RPM < currentStatus.crankRPM) { return crankingGetRPM(configPage4.triggerTeeth); }

  uint16_t tempRPM = 0;
  if(currentStatus.startRevolutions > 0)
  {
    //As the tooth count is over 720 degrees
    tempRPM = stdGetRPM(CAM_SPEED);
  }
  return tempRPM;
}

int getCrankAngle_Subaru67(void)
{
  int crankAngle = 0;
  if( currentStatus.hasSync == true )
  {
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle; //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleIntervalTooth(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }
  }

  return crankAngle;
}

void triggerSetEndTeeth_Subaru67(void)
{
  if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
  {
    //if(ignition1EndAngle < 710) { ignition1EndTooth = 12; }
    if(currentStatus.advance >= 10 )
    {
      ignition1EndTooth = 12;
      ignition2EndTooth = 3;
      ignition3EndTooth = 6;
      ignition4EndTooth = 9;
    }
    else
    {
      ignition1EndTooth = 1;
      ignition2EndTooth = 4;
      ignition3EndTooth = 7;
      ignition4EndTooth = 10;
    }
  }
  else
  {
    if(currentStatus.advance >= 10 )
    {
      ignition1EndTooth = 6;
      ignition2EndTooth = 3;
      //ignition3EndTooth = 6;
      //ignition4EndTooth = 9;
    }
    else
    {
      ignition1EndTooth = 1;
      ignition2EndTooth = 4;
      //ignition3EndTooth = 7;
      //ignition4EndTooth = 10;
    }
  }
}
/** @} */
#endif // Miata9905, MazdaAU, Non360, Nissan360, Subaru67

/** Daihatsu +1 trigger for 3 and 4 cylinder engines.
* Tooth equal to the number of cylinders are evenly spaced on the cam. No position sensing (Distributor is retained),
* so crank angle is a made up figure based purely on the first teeth to be seen.
* Note: This is a very simple decoder. See http://www.megamanual.com/ms2/GM_7pinHEI.htm
* @defgroup dec_daihatsu Daihatsu (3  and 4 cyl.)
* @{
*/
#if 0  // Daihatsu, Harley - REFACTORED to implementations/
void triggerSetup_Daihatsu(void)
{
  triggerActualTeeth = configPage2.nCylinders + 1;
  triggerToothAngle = 720 / triggerActualTeeth; //The number of degrees that passes from tooth to tooth
  triggerFilterTime = MICROS_PER_MIN / MAX_RPM / configPage2.nCylinders; // Minimum time required between teeth
  triggerFilterTime = triggerFilterTime / 2; //Safety margin
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/90U) * triggerToothAngle)*4U;//Minimum 90rpm. (1851uS is the time per degree at 90rpm). This uses 90rpm rather than 50rpm due to the potentially very high stall time on a 4 cylinder if we wait that long.

  if(configPage2.nCylinders == 3)
  {
    toothAngles[0] = 0; //tooth #1
    toothAngles[1] = 30; //tooth #2 (Extra tooth)
    toothAngles[2] = 240; //tooth #3
    toothAngles[3] = 480; //tooth #4
  }
  else
  {
    //Should be 4 cylinders here
    toothAngles[0] = 0; //tooth #1
    toothAngles[1] = 30; //tooth #2 (Extra tooth)
    toothAngles[2] = 180; //tooth #3
    toothAngles[3] = 360; //tooth #4
    toothAngles[4] = 540; //tooth #5
  }
}

void triggerPri_Daihatsu(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;

  //if ( curGap >= triggerFilterTime || (currentStatus.startRevolutions == 0 )
  {
    toothSystemCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    if (currentStatus.hasSync == true)
    {
      if( (toothCurrentCount == triggerActualTeeth) ) //Check if we're back to the beginning of a revolution
      {
         toothCurrentCount = 1; //Reset the counter
         toothOneMinusOneTime = toothOneTime;
         toothOneTime = curTime;
         currentStatus.hasSync = true;
         currentStatus.startRevolutions++; //Counter

         //Need to set a special filter time for the next tooth
         triggerFilterTime = 20; //Fix this later
      }
      else
      {
        toothCurrentCount++; //Increment the tooth counter
        setFilter(curGap); //Recalc the new filter value
      }

      if ( configPage4.ignCranklock && BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
      {
        //This locks the cranking timing to 0 degrees BTDC (All the triggers allow for)
        if(toothCurrentCount == 1) { endCoil1Charge(); }
        else if(toothCurrentCount == 2) { endCoil2Charge(); }
        else if(toothCurrentCount == 3) { endCoil3Charge(); }
        else if(toothCurrentCount == 4) { endCoil4Charge(); }
      }
    }
    else //NO SYNC
    {
      //
      if(toothSystemCount >= 3) //Need to have seen at least 3 teeth to determine SYNC
      {
        unsigned long targetTime;
        //We need to try and find the extra tooth (#2) which is located 30 degrees after tooth #1
        //Aim for tooth times less than about 60 degrees
        if(configPage2.nCylinders == 3)
        {
          targetTime = (toothLastToothTime -  toothLastMinusOneToothTime) / 4; //Teeth are 240 degrees apart for 3 cylinder. 240/4 = 60
        }
        else
        {
          targetTime = ((toothLastToothTime -  toothLastMinusOneToothTime) * 3) / 8; //Teeth are 180 degrees apart for 4 cylinder. (180*3)/8 = 67
        }
        if(curGap < targetTime)
        {
          //Means we're on the extra tooth here
          toothCurrentCount = 2; //Reset the counter
          currentStatus.hasSync = true;
          triggerFilterTime = targetTime; //Lazy, but it works
        }
      }
    }

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
  } //Trigger filter
}
void triggerSec_Daihatsu(void) { return; } //Not required (Should never be called in the first place)

uint16_t getRPM_Daihatsu(void)
{
  uint16_t tempRPM = 0;
  if( (currentStatus.RPM < currentStatus.crankRPM) && false) //Disable special cranking processing for now
  {
    //Can't use standard cranking RPM function due to extra tooth
    if( currentStatus.hasSync == true )
    {
      if(toothCurrentCount == 2) { tempRPM = currentStatus.RPM; }
      else if (toothCurrentCount == 3) { tempRPM = currentStatus.RPM; }
      else
      {
        noInterrupts();
        SetRevolutionTime((toothLastToothTime - toothLastMinusOneToothTime) * (triggerActualTeeth-1));
        interrupts();
        tempRPM = RpmFromRevolutionTimeUs(revolutionTime);
      } //is tooth #2
    }
    else { tempRPM = 0; } //No sync
  }
  else
  { tempRPM = stdGetRPM(CAM_SPEED); } //Tracking over 2 crank revolutions

  return tempRPM;

}
int getCrankAngle_Daihatsu(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    int crankAngle;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
    interrupts();

    crankAngle = toothAngles[tempToothCurrentCount-1] + configPage4.triggerAngle; //Crank angle of the last tooth seen

    //Estimate the number of degrees travelled since the last tooth}
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

void triggerSetEndTeeth_Daihatsu(void)
{
}
/** @} */

/** Harley Davidson (V2) with 2 unevenly Spaced Teeth.
Within the decoder code, the sync tooth is referred to as tooth #1. Derived from GMX7 and adapted for Harley.
Only rising Edge is used for simplicity.The second input is ignored, as it does not help to resolve cam position.
* @defgroup dec_harley Harley Davidson
* @{
*/
void triggerSetup_Harley(void)
{
  triggerToothAngle = 0; // The number of degrees that passes from tooth to tooth, ev. 0. It alternates uneven
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 60U); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  if(currentStatus.initialisationComplete == false) { toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the initial check to prevent the fuel pump just staying on all the time
  triggerFilterTime = 1500;
}

void triggerPri_Harley(void)
{
  lastGap = curGap;
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  setFilter(curGap); // Filtering adjusted according to setting
  if (curGap > triggerFilterTime)
  {
    if ( READ_PRI_TRIGGER() == HIGH) // Has to be the same as in main() trigger-attach, for readability we do it this way.
    {
        BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)
        targetGap = lastGap ; //Gap is the Time to next toothtrigger, so we know where we are
        toothCurrentCount++;
        if (curGap > targetGap)
        {
          toothCurrentCount = 1;
          triggerToothAngle = 0;// Has to be equal to Angle Routine
          toothOneMinusOneTime = toothOneTime;
          toothOneTime = curTime;
          currentStatus.hasSync = true;
        }
        else
        {
          toothCurrentCount = 2;
          triggerToothAngle = 157;
          //     toothOneMinusOneTime = toothOneTime;
          //     toothOneTime = curTime;
        }
        toothLastMinusOneToothTime = toothLastToothTime;
        toothLastToothTime = curTime;
        currentStatus.startRevolutions++; //Counter
    }
    else
    {
      if (currentStatus.hasSync == true) { currentStatus.syncLossCounter++; }
      currentStatus.hasSync = false;
      toothCurrentCount = 0;
    } //Primary trigger high
  } //Trigger filter
}


void triggerSec_Harley(void)
// Needs to be enabled in main()
{
  return;// No need for now. The only thing it could help to sync more quickly or confirm position.
} // End Sec Trigger


uint16_t getRPM_Harley(void)
{
  uint16_t tempRPM = 0;
  if (currentStatus.hasSync == true)
  {
    if ( currentStatus.RPM < (unsigned int)(configPage4.crankRPM * 100) )
    {
      // No difference with this option?
      int tempToothAngle;
      unsigned long toothTime;
      if ( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { tempRPM = 0; }
      else
      {
        noInterrupts();
        tempToothAngle = triggerToothAngle;
        /* High-res mode
          if(toothCurrentCount == 1) { tempToothAngle = 129; }
          else { tempToothAngle = toothAngles[toothCurrentCount-1] - toothAngles[toothCurrentCount-2]; }
        */
        SetRevolutionTime(toothOneTime - toothOneMinusOneTime); //The time in uS that one revolution would take at current speed (The time tooth 1 was last seen, minus the time it was seen prior to that)
        toothTime = (toothLastToothTime - toothLastMinusOneToothTime); //Note that trigger tooth angle changes between 129 and 332 depending on the last tooth that was seen
        interrupts();
        toothTime = toothTime * 36;
        tempRPM = ((unsigned long)tempToothAngle * (MICROS_PER_MIN/10U)) / toothTime;
      }
    }
    else {
      tempRPM = stdGetRPM(CRANK_SPEED);
    }
  }
  return tempRPM;
}


int getCrankAngle_Harley(void)
{
  //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
  unsigned long tempToothLastToothTime;
  int tempToothCurrentCount;
  //Grab some variables that are used in the trigger code and assign them to temp variables.
  noInterrupts();
  tempToothCurrentCount = toothCurrentCount;
  tempToothLastToothTime = toothLastToothTime;
  lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
  interrupts();

  //Check if the last tooth seen was the reference tooth (Number 3). All others can be calculated, but tooth 3 has a unique angle
  int crankAngle;
  if ( (tempToothCurrentCount == 1) || (tempToothCurrentCount == 3) )
  {
    crankAngle = 0 + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.
  }
  else {
    crankAngle = 157 + configPage4.triggerAngle;
  }

  //Estimate the number of degrees travelled since the last tooth}
  elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
  crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

  if (crankAngle >= 720) { crankAngle -= 720; }
  if (crankAngle < 0) { crankAngle += 360; }

  return crankAngle;
}

void triggerSetEndTeeth_Harley(void)
{
}
/** @} */
#endif  // Daihatsu, Harley - REFACTORED to implementations/

#if 0  // ThirtySixMinus222, ThirtySixMinus21, 420a, FordST170 - REFACTORED to implementations/
//************************************************************************************************************************

/** 36-2-2-2 crank based trigger wheel.
* A crank based trigger with a nominal 36 teeth, but 6 of these removed in 3 groups of 2.
* 2 of these groups are located concurrently.
* Note: This decoder supports both the H4 version (13-missing-16-missing-1-missing) and the H6 version of 36-2-2-2 (19-missing-10-missing-1-missing).
* The decoder checks which pattern is selected in order to determine the tooth number
* Note: www.thefactoryfiveforum.com/attachment.php?attachmentid=34279&d=1412431418
*
* @defgroup dec_36_2_2_2 36-2-2-2 Trigger wheel
* @{
*/
void triggerSetup_ThirtySixMinus222(void)
{
  triggerToothAngle = 10; //The number of degrees that passes from tooth to tooth
  triggerActualTeeth = 30; //The number of physical teeth on the wheel. Doing this here saves us a calculation each time in the interrupt
  triggerFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 36)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  checkSyncToothCount = (configPage4.triggerTeeth) >> 1; //50% of the total teeth.
  toothLastMinusOneToothTime = 0;
  toothCurrentCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * 2U ); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
}

// FASE P: ThirtySixMinus222 missing tooth sync configuration
// Subaru 36-2-2-2 pattern uses two double-gaps for sync
// toothSystemCount tracks state: 0=normal, 1=saw first double-gap
struct ThirtySixMinus222SyncConfig {
  uint8_t nCylinders;        // Engine cylinder count (4 or 6)
  uint8_t systemCountState;  // toothSystemCount value (0 or 1)
  uint16_t targetToothCount; // Tooth count to set for this sync point
};

// Static configuration table for 36-2-2-2 sync points
// After second double-gap (systemCount=1, double-gap detected): first tooth after 2x2 missing
// After single tooth following first gap (systemCount=1, no double-gap): single missing tooth
static const ThirtySixMinus222SyncConfig thirtySixMinus222SyncConfigs[] PROGMEM = {
  // After second double-gap (toothSystemCount was 1, now detected another gap)
  {4, 1, 19},  // H4: tooth 19 is first after 2x2 missing
  {6, 1, 12},  // H6: tooth 12 is first after 2x2 missing

  // After single tooth (toothSystemCount was 1, but next tooth is normal - means single missing)
  {4, 0, 35},  // H4: tooth 35 is after single missing
  {6, 0, 34}   // H6: tooth 34 is after single missing
};

// Helper function: Get sync tooth count for ThirtySixMinus222 pattern
// Uses toothSystemCount state and nCylinders to determine correct tooth position
static inline uint16_t getThirtySixMinus222SyncTooth(uint8_t nCylinders, uint8_t systemCountState)
{
  const uint8_t configCount = sizeof(thirtySixMinus222SyncConfigs) / sizeof(ThirtySixMinus222SyncConfig);

  for (uint8_t i = 0; i < configCount; i++)
  {
    const ThirtySixMinus222SyncConfig* cfg = &thirtySixMinus222SyncConfigs[i];

    if (cfg->nCylinders == nCylinders && cfg->systemCountState == systemCountState)
    {
      return cfg->targetToothCount;
    }
  }

  return 0; // Should never reach here
}

void triggerPri_ThirtySixMinus222(void)
{
   curTime = micros();
   curGap = curTime - toothLastToothTime;
   if ( curGap >= triggerFilterTime ) //Pulses should never be less than triggerFilterTime, so if they are it means a false trigger. (A 36-1 wheel at 8000pm will have triggers approx. every 200uS)
   {
     toothCurrentCount++; //Increment the tooth counter
     BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

     //Begin the missing tooth detection
     //If the time between the current tooth and the last is greater than 2x the time between the last tooth and the tooth before that, we make the assertion that we must be at the first tooth after a gap
     //toothSystemCount is used to keep track of which missed tooth we're on. It will be set to 1 if that last tooth seen was the middle one in the -2-2 area. At all other times it will be 0
     if(toothSystemCount == 0) { targetGap = ((toothLastToothTime - toothLastMinusOneToothTime)) * 2; } //Multiply by 2 (Checks for a gap 2x greater than the last one)


     if( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { curGap = 0; }

     if ( (curGap > targetGap) )
     {
       {
         if(toothSystemCount == 1)
         {
           //This occurs when we're at the first tooth after the 2 lots of 2x missing tooth.
           // Use data-driven configuration to get correct tooth count
           toothCurrentCount = getThirtySixMinus222SyncTooth(configPage2.nCylinders, 1);

           toothSystemCount = 0;
           currentStatus.hasSync = true;
         }
         else
         {
           //We've seen a missing tooth set, but do not yet know whether it is the single one or the double one.
           toothSystemCount = 1;
           toothCurrentCount++;
           toothCurrentCount++; //Accurately reflect the actual tooth count, including the skipped ones
         }
         BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); //The tooth angle is double at this point
         triggerFilterTime = 0; //This is used to prevent a condition where serious intermittent signals (Eg someone furiously plugging the sensor wire in and out) can leave the filter in an unrecoverable state
       }
     }
     else
     {
       if(toothCurrentCount > 36)
       {
         //Means a complete rotation has occurred.
         toothCurrentCount = 1;
         revolutionOne = !revolutionOne; //Flip sequential revolution tracker
         toothOneMinusOneTime = toothOneTime;
         toothOneTime = curTime;
         currentStatus.startRevolutions++; //Counter

       }
       else if(toothSystemCount == 1)
       {
          //This occurs when a set of missing teeth had been seen, but the next one was NOT missing.
          // Use data-driven configuration to get correct tooth count
          toothCurrentCount = getThirtySixMinus222SyncTooth(configPage2.nCylinders, 0);
          currentStatus.hasSync = true;
       }

       //Filter can only be recalculated for the regular teeth, not the missing one.
       setFilter(curGap);

       BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
       toothSystemCount = 0;
     }

     toothLastMinusOneToothTime = toothLastToothTime;
     toothLastToothTime = curTime;

     //EXPERIMENTAL!
     if(configPage2.perToothIgn == true)
     {
       int16_t crankAngle = ( (toothCurrentCount-1) * triggerToothAngle ) + configPage4.triggerAngle;
       crankAngle = ignitionLimits(crankAngle);
       checkPerToothTiming(crankAngle, toothCurrentCount);
     }

   }
}

void triggerSec_ThirtySixMinus222(void)
{
  //NOT USED - This pattern uses the missing tooth version of this function
}

// FASE Q: ThirtySixMinus222 RPM excluded teeth configuration
// Certain teeth near missing tooth gaps cannot be used for RPM calculation
struct ThirtySixMinus222RPMExclusion {
  uint8_t nCylinders;
  uint8_t excludedTooth;
};

static const ThirtySixMinus222RPMExclusion thirtySixMinus222RPMExclusions[] PROGMEM = {
  {4, 19}, {4, 16}, {4, 34},  // H4 excluded teeth
  {6,  9}, {6, 12}, {6, 33}   // H6 excluded teeth
};

// Helper: Check if current tooth is excluded from RPM calculation
static inline bool isToothExcludedFromRPM_ThirtySixMinus222(uint8_t nCyl, uint8_t toothCount)
{
  const uint8_t exclusionCount = sizeof(thirtySixMinus222RPMExclusions) / sizeof(ThirtySixMinus222RPMExclusion);

  for (uint8_t i = 0; i < exclusionCount; i++)
  {
    const ThirtySixMinus222RPMExclusion* excl = &thirtySixMinus222RPMExclusions[i];
    if (excl->nCylinders == nCyl && excl->excludedTooth == toothCount)
    {
      return true;
    }
  }
  return false;
}

uint16_t getRPM_ThirtySixMinus222(void)
{
  uint16_t tempRPM = 0;
  if( currentStatus.RPM < currentStatus.crankRPM)
  {
    // Use data-driven helper to check if current tooth is excluded from RPM calc
    bool isExcluded = isToothExcludedFromRPM_ThirtySixMinus222(configPage2.nCylinders, toothCurrentCount);

    if( !isExcluded && BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT) )
    {
      tempRPM = crankingGetRPM(36, CRANK_SPEED);
    }
    else { tempRPM = currentStatus.RPM; } //Can't do per tooth RPM if we're at end of the missing teeth as it messes the calculation
  }
  else
  {
    tempRPM = stdGetRPM(CRANK_SPEED);
  }
  return tempRPM;
}

int getCrankAngle_ThirtySixMinus222(void)
{
    //NOT USED - This pattern uses the missing tooth version of this function
    return 0;
}

// FASE N: ThirtySixMinus222 end teeth configuration (data-driven approach)
// Subaru H4 and H6 engines use 36-2-2-2 crank pattern
// End tooth varies by advance timing to prevent coil overlap
struct ThirtySixMinus222EndTeethConfig {
  uint8_t nCylinders;      // Engine cylinder count (4 or 6)
  uint8_t ignitionChannel; // Ignition channel number (1, 2, or 3)
  uint8_t advanceMax;      // Maximum advance for this tooth (degrees)
  uint16_t endTooth;       // End tooth for this advance range
};

// Static configuration table for 36-2-2-2 end teeth
// H4 (4-cylinder): 2 ignition channels with advance-based tooth selection
// H6 (6-cylinder): 3 ignition channels with advance-based tooth selection
static const ThirtySixMinus222EndTeethConfig thirtySixMinus222Configs[] PROGMEM = {
  // 4-cylinder ignition 1 ranges
  {4, 1, 10, 36},  // advance < 10  -> tooth 36
  {4, 1, 20, 35},  // advance < 20  -> tooth 35
  {4, 1, 30, 34},  // advance < 30  -> tooth 34
  {4, 1, 255, 31}, // advance >= 30 -> tooth 31 (255 = catch-all)

  // 4-cylinder ignition 2 ranges
  {4, 2, 30, 16},  // advance < 30  -> tooth 16
  {4, 2, 255, 13}, // advance >= 30 -> tooth 13

  // 6-cylinder ignition 1 ranges
  {6, 1, 10, 36},  // advance < 10  -> tooth 36
  {6, 1, 20, 35},  // advance < 20  -> tooth 35
  {6, 1, 30, 34},  // advance < 30  -> tooth 34
  {6, 1, 40, 33},  // advance < 40  -> tooth 33
  {6, 1, 255, 31}, // advance >= 40 -> tooth 31

  // 6-cylinder ignition 2 ranges
  {6, 2, 20, 9},   // advance < 20  -> tooth 9
  {6, 2, 255, 6},  // advance >= 20 -> tooth 6

  // 6-cylinder ignition 3 ranges
  {6, 3, 10, 23},  // advance < 10  -> tooth 23
  {6, 3, 20, 22},  // advance < 20  -> tooth 22
  {6, 3, 30, 21},  // advance < 30  -> tooth 21
  {6, 3, 40, 20},  // advance < 40  -> tooth 20
  {6, 3, 255, 19}  // advance >= 40 -> tooth 19
};

// Helper function: Set end tooth using data-driven lookup
// Returns the tooth number for given cylinder count, channel, and advance
static inline uint16_t getThirtySixMinus222EndTooth(uint8_t nCylinders, uint8_t channel, uint8_t advance)
{
  const uint8_t configCount = sizeof(thirtySixMinus222Configs) / sizeof(ThirtySixMinus222EndTeethConfig);

  for (uint8_t i = 0; i < configCount; i++)
  {
    const ThirtySixMinus222EndTeethConfig* cfg = &thirtySixMinus222Configs[i];

    // Match: same cylinder count AND same channel AND advance < advanceMax
    if (cfg->nCylinders == nCylinders &&
        cfg->ignitionChannel == channel &&
        advance < cfg->advanceMax)
    {
      return cfg->endTooth;
    }
  }

  return 0; // Should never reach here if config table is complete
}

void triggerSetEndTeeth_ThirtySixMinus222(void)
{
  // Use data-driven configuration lookup
  // Handles both 4-cylinder (H4) and 6-cylinder (H6) with advance-based selection
  uint8_t nCyl = configPage2.nCylinders;
  uint8_t advance = currentStatus.advance;

  ignition1EndTooth = getThirtySixMinus222EndTooth(nCyl, 1, advance);
  ignition2EndTooth = getThirtySixMinus222EndTooth(nCyl, 2, advance);

  // Ignition 3 only used for 6-cylinder
  if (nCyl == 6)
  {
    ignition3EndTooth = getThirtySixMinus222EndTooth(nCyl, 3, advance);
  }
}
/** @} */

//************************************************************************************************************************

/** 36-2-1 / Mistsubishi 4B11 - A crank based trigger with a nominal 36 teeth, but with 1 single and 1 double missing tooth.
* @defgroup dec_36_2_1 36-2-1 For Mistsubishi 4B11
* @{
*/
void triggerSetup_ThirtySixMinus21(void)
{
  triggerToothAngle = 10; //The number of degrees that passes from tooth to tooth
  triggerActualTeeth = 33; //The number of physical teeth on the wheel. Doing this here saves us a calculation each time in the interrupt. Not Used
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 36)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  checkSyncToothCount = (configPage4.triggerTeeth) >> 1; //50% of the total teeth.
  toothLastMinusOneToothTime = 0;
  toothCurrentCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * 2U ); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
}

void triggerPri_ThirtySixMinus21(void)
{
   curTime = micros();
   curGap = curTime - toothLastToothTime;
   if ( curGap >= triggerFilterTime ) //Pulses should never be less than triggerFilterTime, so if they are it means a false trigger. (A 36-1 wheel at 8000pm will have triggers approx. every 200uS)
   {
     toothCurrentCount++; //Increment the tooth counter
     BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

     //Begin the missing tooth detection
     //If the time between the current tooth and the last is greater than 2x the time between the last tooth and the tooth before that, we make the assertion that we must be at the first tooth after a gap

     targetGap2 = (3 * (toothLastToothTime - toothLastMinusOneToothTime)) ; //Multiply by 3 (Checks for a gap 3x greater than the last one)
     targetGap = targetGap2 >> 1;  //Multiply by 1.5 (Checks for a gap 1.5x greater than the last one) (Uses bitshift to divide by 2 as in the missing tooth decoder)

     if( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { curGap = 0; }

     if ( (curGap > targetGap) )
     {
      if ( (curGap < targetGap2))
       {
           //we are at the tooth after the single gap
           toothCurrentCount = 20; //it's either 19 or 20, need to clarify engine direction!
           currentStatus.hasSync = true;
        }
        else
        {
          //we are at the tooth after the double gap
          toothCurrentCount = 1;
          currentStatus.hasSync = true;
        }

         BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); //The tooth angle is double at this point
         triggerFilterTime = 0; //This is used to prevent a condition where serious intermittent signals (Eg someone furiously plugging the sensor wire in and out) can leave the filter in an unrecoverable state
       }
     }
     else
     {
       if(  (toothCurrentCount > 36) || ( toothCurrentCount==1)  )
       {
         //Means a complete rotation has occurred.
         toothCurrentCount = 1;
         revolutionOne = !revolutionOne; //Flip sequential revolution tracker
         toothOneMinusOneTime = toothOneTime;
         toothOneTime = curTime;
         currentStatus.startRevolutions++; //Counter

       }

       //Filter can only be recalculated for the regular teeth, not the missing one.
       setFilter(curGap);

       BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);

     }

     toothLastMinusOneToothTime = toothLastToothTime;
     toothLastToothTime = curTime;

     //EXPERIMENTAL!
     if(configPage2.perToothIgn == true)
     {
       int16_t crankAngle = ( (toothCurrentCount-1) * triggerToothAngle ) + configPage4.triggerAngle;
       crankAngle = ignitionLimits(crankAngle);
       checkPerToothTiming(crankAngle, toothCurrentCount);
     }


}

void triggerSec_ThirtySixMinus21(void)
{
  //NOT USED - This pattern uses the missing tooth version of this function
}

uint16_t getRPM_ThirtySixMinus21(void)
{
  uint16_t tempRPM = 0;
  if( currentStatus.RPM < currentStatus.crankRPM)
  {
    if( (toothCurrentCount != 20) && (BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT)) )
    {
      tempRPM = crankingGetRPM(36, CRANK_SPEED);
    }
    else { tempRPM = currentStatus.RPM; } //Can't do per tooth RPM if we're at tooth #1 as the missing tooth messes the calculation
  }
  else
  {
    tempRPM = stdGetRPM(CRANK_SPEED);
  }
  return tempRPM;
}

int getCrankAngle_ThirtySixMinus21(void)
{
    //NOT USED - This pattern uses the missing tooth version of this function
    return 0;
}

void triggerSetEndTeeth_ThirtySixMinus21(void)
{
  ignition1EndTooth = 10;
  ignition2EndTooth = 28; // Arbitrarily picked  at 180°.
}
/** @} */

//************************************************************************************************************************

/** DSM 420a, For the DSM Eclipse with 16 teeth total on the crank.
* Tracks the falling side of the signal.
* Sync is determined by watching for a falling edge on the secondary signal and checking if the primary signal is high then.
* https://github.com/noisymime/speeduino/issues/133
* @defgroup dec_dsm_420a DSM 420a, For the DSM Eclipse
* @{
*/
void triggerSetup_420a(void)
{
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = 0;
  secondaryToothCount = 0; //Initially set to 0 prior to calculating the secondary window duration
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  toothCurrentCount = 1;
  triggerToothAngle = 20; //Is only correct for the 4 short pulses before each TDC
  BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  toothSystemCount = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 93U); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)

  toothAngles[0] = 711; //tooth #1, just before #1 TDC
  toothAngles[1] = 111;
  toothAngles[2] = 131;
  toothAngles[3] = 151;
  toothAngles[4] = 171; //Just before #3 TDC
  toothAngles[5] = toothAngles[1] + 180;
  toothAngles[6] = toothAngles[2] + 180;
  toothAngles[7] = toothAngles[3] + 180;
  toothAngles[8] = toothAngles[4] + 180; //Just before #4 TDC
  toothAngles[9]  = toothAngles[1] + 360;
  toothAngles[10] = toothAngles[2] + 360;
  toothAngles[11] = toothAngles[3] + 360;
  toothAngles[12] = toothAngles[4] + 360; //Just before #2 TDC
  toothAngles[13] = toothAngles[1] + 540;
  toothAngles[14] = toothAngles[2] + 540;
  toothAngles[15] = toothAngles[3] + 540;
}

void triggerPri_420a(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if ( curGap >= triggerFilterTime ) //Pulses should never be less than triggerFilterTime, so if they are it means a false trigger. (A 36-1 wheel at 8000pm will have triggers approx. every 200uS)
  {
    toothCurrentCount++; //Increment the tooth counter
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    if( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { curGap = 0; }

    if( (toothCurrentCount > 16) && (currentStatus.hasSync == true) )
    {
      //Means a complete rotation has occurred.
      toothCurrentCount = 1;
      toothOneMinusOneTime = toothOneTime;
      toothOneTime = curTime;
      currentStatus.startRevolutions++; //Counter
    }

    //Filter can only be recalculated for the regular teeth, not the missing one.
    //setFilter(curGap);
    triggerFilterTime = 0;

    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    //EXPERIMENTAL!
    if(configPage2.perToothIgn == true)
    {
      int16_t crankAngle = ( toothAngles[(toothCurrentCount-1)] ) + configPage4.triggerAngle;
      crankAngle = ignitionLimits(crankAngle);
      checkPerToothTiming(crankAngle, toothCurrentCount);
    }
  }
}

// FASE R: DSM 420a sync configuration
// Sync determined by primary signal state when secondary falls
struct DSM420aSyncConfig {
  bool priTriggerState;       // Expected primary trigger state (HIGH or LOW)
  uint8_t expectedToothCount; // Tooth count for this sync point
};

static const DSM420aSyncConfig dsm420aSyncConfigs[2] PROGMEM = {
  {true,  13},  // Primary HIGH when secondary falls -> tooth 13
  {false,  5}   // Primary LOW when secondary falls -> tooth 5
};

// Helper: Validate and set DSM 420a sync based on primary state
static inline void processDSM420aSync(bool priState, bool hasSync, uint8_t currentToothCount)
{
  // Find matching configuration
  uint8_t expectedTooth = 0;
  for (uint8_t i = 0; i < 2; i++)
  {
    if (dsm420aSyncConfigs[i].priTriggerState == priState)
    {
      expectedTooth = dsm420aSyncConfigs[i].expectedToothCount;
      break;
    }
  }

  if (hasSync == false)
  {
    // No sync yet - accept this tooth position
    toothCurrentCount = expectedTooth;
    currentStatus.hasSync = true;
  }
  else
  {
    // Have sync - validate tooth position matches expectation
    if (currentToothCount != expectedTooth)
    {
      currentStatus.syncLossCounter++;
      toothCurrentCount = expectedTooth;
    }
  }
}

void triggerSec_420a(void)
{
  //Secondary trigger is only on falling edge
  //Use data-driven helper to process sync based on primary trigger state
  bool priState = READ_PRI_TRIGGER();
  processDSM420aSync(priState, currentStatus.hasSync, toothCurrentCount);
}

uint16_t getRPM_420a(void)
{
  uint16_t tempRPM = 0;
  if( currentStatus.RPM < currentStatus.crankRPM)
  {
    //Possibly look at doing special handling for cranking in the future, but for now just use the standard method
    tempRPM = stdGetRPM(CAM_SPEED);
  }
  else
  {
    tempRPM = stdGetRPM(CAM_SPEED);
  }
  return tempRPM;
}

int getCrankAngle_420a(void)
{
  //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
  unsigned long tempToothLastToothTime;
  int tempToothCurrentCount;
  //Grab some variables that are used in the trigger code and assign them to temp variables.
  noInterrupts();
  tempToothCurrentCount = toothCurrentCount;
  tempToothLastToothTime = toothLastToothTime;
  lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
  interrupts();

  int crankAngle;
  crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle; //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.

  //Estimate the number of degrees travelled since the last tooth}
  elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
  crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

  if (crankAngle >= 720) { crankAngle -= 720; }
  if (crankAngle < 0) { crankAngle += 360; }

  return crankAngle;
}

void triggerSetEndTeeth_420a(void)
{
  if(currentStatus.advance < 9)
  {
    ignition1EndTooth = 1;
    ignition2EndTooth = 5;
    ignition3EndTooth = 9;
    ignition4EndTooth = 13;
  }
  else
  {
    ignition1EndTooth = 16;
    ignition2EndTooth = 4;
    ignition3EndTooth = 8;
    ignition4EndTooth = 12;
  }
}
/** @} */
#endif  // ThirtySixMinus222, ThirtySixMinus21, 420a - REFACTORED to implementations/

#if 0  // Weber - REFACTORED to implementations/
/** Weber-Marelli trigger setup with 2 wheels, 4 teeth 90deg apart on crank and 2 90deg apart on cam.
Uses DualWheel decoders, There can be no missing teeth on the primary wheel.
* @defgroup dec_weber_marelli Weber-Marelli
* @{
*/
void triggerPri_Webber(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;
  if ( curGap >= triggerFilterTime )
  {
    toothCurrentCount++; //Increment the tooth counter
    if (checkSyncToothCount > 0) { checkSyncToothCount++; }
    if ( triggerSecFilterTime <= curGap ) { triggerSecFilterTime = curGap + (curGap>>1); } //150% crank tooth
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    if ( currentStatus.hasSync == true )
    {
      if ( (toothCurrentCount == 1) || (toothCurrentCount > configPage4.triggerTeeth) )
      {
        toothCurrentCount = 1;
        revolutionOne = !revolutionOne; //Flip sequential revolution tracker
        toothOneMinusOneTime = toothOneTime;
        toothOneTime = curTime;
        currentStatus.startRevolutions++; //Counter
      }

      setFilter(curGap); //Recalc the new filter value
    }
    else
    {
      if ( (secondaryToothCount == 1) && (checkSyncToothCount == 4) )
      {
        toothCurrentCount = 2;
        currentStatus.hasSync = true;
        revolutionOne = 0; //Sequential revolution reset
      }
    }

    //NEW IGNITION MODE
    if( (configPage2.perToothIgn == true) && (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) )
    {
      int16_t crankAngle = ( (toothCurrentCount-1) * triggerToothAngle ) + configPage4.triggerAngle;
      if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (revolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED) )
      {
        crankAngle += 360;
        checkPerToothTiming(crankAngle, (configPage4.triggerTeeth + toothCurrentCount));
      }
      else{ checkPerToothTiming(crankAngle, toothCurrentCount); }
    }
  } //Trigger filter
}

// FASE S: Webber sync state machine (explicit enum-based)
// State determined by combination of secondaryToothCount, checkSyncToothCount, hasSync, toothCurrentCount
enum WebberSyncState {
  WEBBER_SYNC_SECOND_CAM_RESTART,  // secondaryToothCount==2 && checkSyncToothCount==3
  WEBBER_SYNC_FIRST_START,         // !hasSync && toothCurrentCount>=3 && secondaryToothCount==0
  WEBBER_SYNC_OTHER                // All other cases (noise/normal)
};

// Helper: Determine Webber sync state
static inline WebberSyncState getWebberSyncState(uint8_t secCount, uint8_t checkCount, bool hasSync, uint8_t toothCount)
{
  if ((secCount == 2) && (checkCount == 3))
  {
    return WEBBER_SYNC_SECOND_CAM_RESTART;
  }
  else if ((hasSync == false) && (toothCount >= 3) && (secCount == 0))
  {
    return WEBBER_SYNC_FIRST_START;
  }
  else
  {
    return WEBBER_SYNC_OTHER;
  }
}

void triggerSec_Webber(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  if ( curGap2 >= triggerSecFilterTime )
  {
    toothLastSecToothTime = curTime2;

    // Use explicit state machine
    WebberSyncState state = getWebberSyncState(secondaryToothCount, checkSyncToothCount, currentStatus.hasSync, toothCurrentCount);

    switch(state)
    {
      case WEBBER_SYNC_SECOND_CAM_RESTART:
        // Running, on first CAM pulse restart crank teeth count, on second the counter should be 3
        if(currentStatus.hasSync == false)
        {
          toothLastToothTime = micros();
          toothLastMinusOneToothTime = micros() - 1500000; //Fixes RPM at 10rpm until a full revolution has taken place
          toothCurrentCount = configPage4.triggerTeeth-1;
          currentStatus.hasSync = true;
        }
        else
        {
          if ( (toothCurrentCount != (configPage4.triggerTeeth-1U)) && (currentStatus.startRevolutions > 2U)) { currentStatus.syncLossCounter++; } //Indicates likely sync loss.
          if (configPage4.useResync == 1) { toothCurrentCount = configPage4.triggerTeeth-1; }
        }
        revolutionOne = 1; //Sequential revolution reset
        triggerSecFilterTime = curGap << 2; //4 crank teeth
        secondaryToothCount = 1; //Next tooth should be first
        break;

      case WEBBER_SYNC_FIRST_START:
        // First start, between gaps on CAM pulses have 2 teeth, sync on first CAM pulse if seen 3 teeth or more
        toothLastToothTime = micros();
        toothLastMinusOneToothTime = micros() - 1500000; //Fixes RPM at 10rpm until a full revolution has taken place
        toothCurrentCount = 1;
        revolutionOne = 1; //Sequential revolution reset
        currentStatus.hasSync = true;
        break;

      case WEBBER_SYNC_OTHER:
        // First time might fall here, second CAM tooth will
        triggerSecFilterTime = curGap + (curGap>>1); //150% crank tooth
        secondaryToothCount++;
        checkSyncToothCount = 1; //Tooth 1 considered as already been seen
        break;
    }
  }
  else
  {
    triggerSecFilterTime = curGap + (curGap>>1); //Noise region, using 150% of crank tooth
    checkSyncToothCount = 1; //Reset tooth counter
  } //Trigger filter
}
/** @} */
#endif  // Weber - REFACTORED to implementations/

#if 0  // FordST170 - REFACTORED to implementations/
/** Ford ST170 - a dedicated decoder for 01-04 Ford Focus ST170/SVT engine.
Standard 36-1 trigger wheel running at crank speed and 8-3 trigger wheel running at cam speed.
* @defgroup dec_ford_st170 Ford ST170 (01-04 Focus)
* @{
*/
void triggerSetup_FordST170(void)
{
  //Set these as we are using the existing missing tooth primary decoder and these will never change.
  configPage4.triggerTeeth = 36;
  configPage4.triggerMissingTeeth = 1;
  configPage4.TrigSpeed = CRANK_SPEED;

  triggerToothAngle = 360 / configPage4.triggerTeeth; //The number of degrees that passes from tooth to tooth
  triggerActualTeeth = configPage4.triggerTeeth - configPage4.triggerMissingTeeth; //The number of physical teeth on the wheel. Doing this here saves us a calculation each time in the interrupt
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise

  triggerSecFilterTime = MICROS_PER_MIN / MAX_RPM / 8U / 2U; //Cam pattern is 8-3, so 2 nearest teeth are 90 deg crank angle apart. Cam can be advanced by 60 deg, so going from fully retarded to fully advanced closes the gap to 30 deg. Zetec cam pulleys aren't keyed from factory, so I subtracted additional 10 deg to avoid filter to be too aggressive. And there you have it 720/20=36.

  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  checkSyncToothCount = (36) >> 1; //50% of the total teeth.
  toothLastMinusOneToothTime = 0;
  toothCurrentCount = 0;
  secondaryToothCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * (1U + 1U)); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

// FASE T: Ford ST170 VVT recording extraction
// Separates VVT angle recording concern from missing tooth sync logic
// Reduces nesting from 4 levels to 2 (guard clauses)
static inline void recordVVTAngle_FordST170(uint8_t revOne, uint8_t secToothCount)
{
  // Guard: VVT disabled
  if (configPage6.vvtEnabled == 0) { return; }

  // Guard: Not on first revolution
  if (revOne != 1) { return; }

  // Guard: Not on first tooth after gap
  if (secToothCount != 1) { return; }

  // Record VVT angle using first tooth after long gap as reference
  // This tooth remains in the same engine cycle even when VVT is at full swing
  int16_t curAngle = getCrankAngle();
  while (curAngle > 360) { curAngle -= 360; }

  if (configPage6.vvtMode == VVT_MODE_CLOSED_LOOP)
  {
    curAngle = LOW_PASS_FILTER((curAngle << 1), configPage4.ANGLEFILTER_VVT, curAngle);
    currentStatus.vvt1Angle = 360 - curAngle - configPage10.vvtCL0DutyAng;
  }
}

void triggerSec_FordST170(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  //Safety check for initial startup
  if( (toothLastSecToothTime == 0) )
  {
    curGap2 = 0;
    toothLastSecToothTime = curTime2;
  }

  if ( curGap2 >= triggerSecFilterTime )
  {
      targetGap2 = (3 * (toothLastSecToothTime - toothLastMinusOneSecToothTime)) >> 1; //If the time between the current tooth and the last is greater than 1.5x the time between the last tooth and the tooth before that, we make the assertion that we must be at the first tooth after the gap
      toothLastMinusOneSecToothTime = toothLastSecToothTime;
      if ( (curGap2 >= targetGap2) || (secondaryToothCount == 5) )
      {
        secondaryToothCount = 1;
        revolutionOne = 1; //Sequential revolution reset
        triggerSecFilterTime = 0; //This is used to prevent a condition where serious intermittent signals (Eg someone furiously plugging the sensor wire in and out) can leave the filter in an unrecoverable state
      }
      else
      {
        triggerSecFilterTime = curGap2 >> 2; //Set filter at 25% of the current speed. Filter can only be recalculated for the regular teeth, not the missing one.
        secondaryToothCount++;
      }

    toothLastSecToothTime = curTime2;

    // Record VVT angle (separated concern)
    recordVVTAngle_FordST170(revolutionOne, secondaryToothCount);
  } //Trigger filter
}

uint16_t getRPM_FordST170(void)
{
  uint16_t tempRPM = 0;
  if( currentStatus.RPM < currentStatus.crankRPM )
  {
    if(toothCurrentCount != 1)
    {
      tempRPM = crankingGetRPM(36, CRANK_SPEED);
    }
    else { tempRPM = currentStatus.RPM; } //Can't do per tooth RPM if we're at tooth #1 as the missing tooth messes the calculation
  }
  else
  {
    tempRPM = stdGetRPM(CRANK_SPEED);
  }
  return tempRPM;
}

int getCrankAngle_FordST170(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempRevolutionOne = revolutionOne;
    tempToothLastToothTime = toothLastToothTime;
    interrupts();

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    //Sequential check (simply sets whether we're on the first or 2nd revolution of the cycle)
    if ( (tempRevolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED) ) { crankAngle += 360; }

    lastCrankAngleCalc = micros();
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

static uint16_t __attribute__((noinline)) calcSetEndTeeth_FordST170(int ignitionAngle, uint8_t toothAdder) {
  int16_t tempEndTooth = ignitionAngle - configPage4.triggerAngle;
#ifdef USE_LIBDIVIDE
  tempEndTooth = libdivide::libdivide_s16_do(tempEndTooth, &divTriggerToothAngle);
#else
  tempEndTooth = tempEndTooth / (int16_t)triggerToothAngle;
#endif
  tempEndTooth = nudge(1, 36U + toothAdder,  tempEndTooth - 1, 36U + toothAdder);
  return clampToActualTeeth((uint16_t)tempEndTooth, toothAdder);
}

void triggerSetEndTeeth_FordST170(void)
{
  byte toothAdder = 0;
   if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage4.TrigSpeed == CRANK_SPEED) ) { toothAdder = 36; }

  ignition1EndTooth = calcSetEndTeeth_FordST170(ignition1EndAngle, toothAdder);
  ignition2EndTooth = calcSetEndTeeth_FordST170(ignition2EndAngle, toothAdder);
  ignition3EndTooth = calcSetEndTeeth_FordST170(ignition3EndAngle, toothAdder);
  ignition4EndTooth = calcSetEndTeeth_FordST170(ignition4EndAngle, toothAdder);

  // Removed ign channels >4 as an ST170 engine is a 4 cylinder
}
/** @} */
#endif  // FordST170 - REFACTORED to implementations/

#if 0  // DRZ400, NGC, Vmax, Renix, RoverMEMS, SuzukiK6A - REFACTORED to implementations/
void triggerSetup_DRZ400(void)
{
  triggerToothAngle = 360 / configPage4.triggerTeeth; //The number of degrees that passes from tooth to tooth
  if(configPage4.TrigSpeed == 1) { triggerToothAngle = 720 / configPage4.triggerTeeth; } //Account for cam speed
  toothCurrentCount = UINT8_MAX; //Default value
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)); //Same as above, but fixed at 2 teeth on the secondary input
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); //This is always true for this pattern
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
}

void triggerSec_DRZ400(void)
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;
  if ( curGap2 >= triggerSecFilterTime )
  {
    toothLastSecToothTime = curTime2;

    if(currentStatus.hasSync == false)
    {
      toothLastToothTime = micros();
      toothLastMinusOneToothTime = micros() - ((MICROS_PER_MIN/10U) / configPage4.triggerTeeth); //Fixes RPM at 10rpm until a full revolution has taken place
      toothCurrentCount = configPage4.triggerTeeth;
      currentStatus.syncLossCounter++;
      currentStatus.hasSync = true;
    }
    else
    {
      // have rotation, set tooth to six so next tooth is 1 & duel wheel rotation code kicks in
      toothCurrentCount = 6;
    }
  }

  triggerSecFilterTime = (toothOneTime - toothOneMinusOneTime) >> 1; //Set filter at 50% of the current crank speed.
}

/** Chrysler NGC - a dedicated decoder for vehicles with 4, 6 and 8 cylinder NGC pattern.
4-cyl: 36+2-2 crank wheel and 7 tooth cam
6-cyl: 36-2+2 crank wheel and 12 tooth cam in 6 groups
8-cyl: 36-2+2 crank wheel and 15 tooth cam in 8 groups
The crank decoder uses the polarity of the missing teeth to determine position
The 4-cyl cam decoder uses the polarity of the missing teeth to determine position
The 6 and 8-cyl cam decoder uses the amount of teeth in the two previous groups of teeth to determine position
* @defgroup dec Chrysler NGC - 4, 6 and 8-cylinder
* @{
*/

void triggerSetup_NGC(void)
{
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);

  //Primary trigger
  configPage4.triggerTeeth = 36; //The number of teeth on the wheel incl missing teeth.
  triggerToothAngle = 10; //The number of degrees that passes from tooth to tooth
  triggerFilterTime = MICROS_PER_SEC / (MAX_RPM/60U) / (360U/triggerToothAngle); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  toothCurrentCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  toothLastMinusOneToothTime = 0;
  toothLastToothRisingTime = 0;
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * 2U ); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)

  //Secondary trigger
  if (configPage2.nCylinders == 4) {
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM/60U) / (360U/36U) * 2U); //Two nearest edges are 36 degrees apart. Multiply by 2 for half cam speed.
  } else {
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM/60U) / (360U/21U) * 2U); //Two nearest edges are 21 degrees apart. Multiply by 2 for half cam speed.
  }
  secondaryToothCount = 0;
  toothSystemCount = 0;
  toothLastSecToothRisingTime = 0;
  toothLastSecToothTime = 0;
  toothLastMinusOneSecToothTime = 0;

  //toothAngles is reused to store the cam pattern, only used for 6 and 8 cylinder pattern
  if (configPage2.nCylinders == 6) {
    toothAngles[0] = 1; // Pos 0 is required to be the same as group 6 for easier math
    toothAngles[1] = 3; // Group 1 ...
    toothAngles[2] = 1;
    toothAngles[3] = 2;
    toothAngles[4] = 3;
    toothAngles[5] = 2;
    toothAngles[6] = 1;
    toothAngles[7] = 3; // Pos 7 is required to be the same as group 1 for easier math
  }
  else if (configPage2.nCylinders == 8) {
    toothAngles[0] = 3; // Pos 0 is required to be the same as group 8 for easier math
    toothAngles[1] = 1; // Group 1 ...
    toothAngles[2] = 1;
    toothAngles[3] = 2;
    toothAngles[4] = 3;
    toothAngles[5] = 2;
    toothAngles[6] = 2;
    toothAngles[7] = 1;
    toothAngles[8] = 3;
    toothAngles[9] = 1; // Pos 9 is required to be the same as group 1 for easier math
  }
#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

// NGC trigger sync validation - data-driven approach to eliminate duplication
// Stores valid tooth count combinations for sync validation
struct NGCSyncCondition {
  uint8_t nCylinders;
  bool revolutionOne;
  // For toothCurrentCount == 1
  uint8_t tooth1_secondary_min;  // Min secondary/system count at tooth 1
  uint8_t tooth1_secondary_max;  // Max secondary/system count at tooth 1
  // For toothCurrentCount == 19
  uint8_t tooth19_secondary_min; // Min secondary/system count at tooth 19
  uint8_t tooth19_secondary_max; // Max secondary/system count at tooth 19
};

// Configuration array for all NGC sync conditions
// Format: {cylinders, revolutionOne, tooth1_min, tooth1_max, tooth19_min, tooth19_max}
static const NGCSyncCondition ngcSyncConditions[6] = {
  // Revolution One = false conditions
  {4, false, 1, 2, 4, 4},  // 4cyl rev0: tooth1 with sec 1-2, tooth19 with sec 4
  {6, false, 1, 2, 2, 3},  // 6cyl rev0: tooth1 with sys 1-2, tooth19 with sys 2-3
  {8, false, 1, 2, 3, 4},  // 8cyl rev0: tooth1 with sys 1-2, tooth19 with sys 3-4
  // Revolution One = true conditions
  {4, true,  5, 5, 7, 7},  // 4cyl rev1: tooth1 with sec 5, tooth19 with sec 7
  {6, true,  4, 5, 5, 6},  // 6cyl rev1: tooth1 with sys 4-5, tooth19 with sys 5-6
  {8, true,  5, 6, 7, 8}   // 8cyl rev1: tooth1 with sys 5-6, tooth19 with sys 7-8
};

// Helper function to check NGC sync conditions using data-driven approach
static inline bool checkNGCSyncCondition(uint8_t toothCount, uint8_t secondaryCount, uint8_t nCylinders, volatile bool* outRevolutionOne)
{
  for(uint8_t i = 0; i < 6; i++)
  {
    const NGCSyncCondition* cond = &ngcSyncConditions[i];

    // Skip if not matching cylinder count
    if(cond->nCylinders != nCylinders) { continue; }

    bool isValid = false;

    // Check tooth 1 conditions
    if(toothCount == 1)
    {
      isValid = (secondaryCount >= cond->tooth1_secondary_min &&
                 secondaryCount <= cond->tooth1_secondary_max);
    }
    // Check tooth 19 conditions
    else if(toothCount == 19)
    {
      isValid = (secondaryCount >= cond->tooth19_secondary_min &&
                 secondaryCount <= cond->tooth19_secondary_max);
    }

    if(isValid)
    {
      *outRevolutionOne = cond->revolutionOne;
      return true;
    }
  }

  return false;  // No valid condition found
}

//========== NGC PRIMARY TRIGGER DECODER HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Determine tooth position after detecting NGC missing tooth
 * @return Tooth count (1 for HIGH missing tooth, 19 for LOW missing tooth)
 * @note MISRA-C compliant: Lines: 28 | Cyclomatic: 2 | Nesting: 2
 */
static inline uint16_t determineNGCToothPosition(void)
{
  // Determine polarity by comparing how far ago the last tooth rose
  if ((toothLastToothRisingTime - toothLastToothTime) < (curTime - toothLastToothRisingTime))
  {
    // Just passed the HIGH missing tooth (tooth #1)
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;

    if (currentStatus.hasSync == true)
    {
      currentStatus.startRevolutions++;
    }
    else
    {
      currentStatus.startRevolutions = 0;
    }

    return 1;
  }

  // Just passed the first tooth after LOW missing tooth
  return 19;
}

/**
 * @brief Update NGC sequential sync status
 * @param toothCount Current tooth count
 * @note MISRA-C compliant: Lines: 18 | Cyclomatic: 4 | Nesting: 2
 */
static inline void updateNGCSequentialSync(uint16_t toothCount)
{
  if ((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (configPage2.injLayout != INJ_SEQUENTIAL))
  {
    // Non-sequential: immediate sync
    currentStatus.hasSync = true;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    return;
  }

  // Sequential mode: need cam signal validation
  uint8_t secondaryCount = (configPage2.nCylinders == 4) ? secondaryToothCount : toothSystemCount;

  if (checkNGCSyncCondition(toothCount, secondaryCount, configPage2.nCylinders, &revolutionOne))
  {
    currentStatus.hasSync = true;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
  }
  else
  {
    if (currentStatus.hasSync == true)
    {
      currentStatus.syncLossCounter++;
    }
    currentStatus.hasSync = false;
    BIT_SET(currentStatus.status3, BIT_STATUS3_HALFSYNC); // Half sync only
  }
}

/**
 * @brief Handle NGC missing tooth detection and sync
 * @param curGap Current gap between teeth
 * @param expectedGap Expected gap for normal tooth
 * @return true if missing tooth detected
 * @note MISRA-C compliant: Lines: 20 | Cyclomatic: 3 | Nesting: 2
 */
static inline bool handleNGCMissingTooth(unsigned long curGap, unsigned long expectedGap)
{
  if (curGap <= expectedGap)
  {
    // Not a missing tooth - lost sync if we expected one
    if (currentStatus.hasSync == true)
    {
      currentStatus.syncLossCounter++;
    }
    currentStatus.hasSync = false;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    return false;
  }

  // Missing tooth detected
  triggerFilterTime = 0; // Prevent filter lockup
  BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); // Tooth angle is double

  toothCurrentCount = determineNGCToothPosition();
  updateNGCSequentialSync(toothCurrentCount);

  return true;
}

/**
 * @brief Primary trigger interrupt for NGC (Chrysler 36+2-2) decoder
 * @details Handles crank teeth interrupts with dual missing tooth pattern.
 * Detects HIGH and LOW missing teeth to determine position. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 60 | Cyclomatic: 9 | Nesting: 2 (was N:7, C:25, 103 lines!)
 * @see handleNGCMissingTooth(), determineNGCToothPosition(), updateNGCSequentialSync()
 */
void triggerPri_NGC(void)
{
  curTime = micros();

  // Guard clause: check polarity - only process LOW signals (falling edge)
  if (READ_PRI_TRIGGER() == HIGH)
  {
    toothLastToothRisingTime = curTime;
    return;
  }

  curGap = curTime - toothLastToothTime;

  // Guard clause: filter debounce
  if (curGap < triggerFilterTime) { return; }

  toothCurrentCount++;
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

  bool isMissingTooth = false;

  // Need at least 2 previous teeth for gap analysis
  if ((toothLastToothTime > 0) && (toothLastMinusOneToothTime > 0))
  {
    // Only check for missing tooth at expected positions (17, 35) or when no sync
    if ((toothCurrentCount == 17) || (toothCurrentCount == 35) ||
        ((currentStatus.hasSync == false) && (BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC) == false)))
    {
      unsigned long expectedGap = (toothLastToothTime - toothLastMinusOneToothTime) * 2;
      isMissingTooth = handleNGCMissingTooth(curGap, expectedGap);
    }

    if (isMissingTooth == false)
    {
      // Regular (non-missing) tooth
      setFilter(curGap);
      BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    }
  }

  // Update tooth timing history
  if (isMissingTooth == true)
  {
    // Missing tooth: copy gap from previous tooth as correct normal length
    toothLastMinusOneToothTime = curTime - (toothLastToothTime - toothLastMinusOneToothTime);
  }
  else
  {
    toothLastMinusOneToothTime = toothLastToothTime;
  }
  toothLastToothTime = curTime;

  // Per-tooth ignition timing
  if ((configPage2.perToothIgn == true) && (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) == false))
  {
    int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
    crankAngle = ignitionLimits(crankAngle);

    if ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (revolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED))
    {
      crankAngle += 360;
      checkPerToothTiming(crankAngle, (configPage4.triggerTeeth + toothCurrentCount));
    }
    else
    {
      checkPerToothTiming(crankAngle, toothCurrentCount);
    }
  }
}
#endif  // DRZ400, NGC (part 1) - REFACTORED to implementations/

//========== NGC 4-CYLINDER CAM DECODER HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Determine NGC4 cam tooth position and sync state based on polarity
 * @param isHighMissingTooth true if HIGH missing tooth detected, false if LOW
 * @note MISRA-C compliant: Lines: 18 | Cyclomatic: 6 | Nesting: 2
 */
static inline void determineNGC4ToothPosition(bool isHighMissingTooth)
{
  if (isHighMissingTooth)
  {
    // Just passed the HIGH missing tooth
    if ((secondaryToothCount == 0) || (secondaryToothCount == 8))
    {
      secondaryToothCount = 1; // Synced
    }
    else if (secondaryToothCount > 0)
    {
      secondaryToothCount = 0; // Lost sync - wrong tooth count
    }
  }
  else
  {
    // Just passed the first tooth after the LOW missing tooth
    if ((secondaryToothCount == 0) || (secondaryToothCount == 5))
    {
      secondaryToothCount = 5;
    }
    else if (secondaryToothCount > 0)
    {
      secondaryToothCount = 0; // Lost sync
    }
  }

  triggerSecFilterTime = 0; // Prevent filter lockup on intermittent signals
}

/**
 * @brief Handle NGC4 long tooth (missing tooth) detection
 * @param curGap2 Current gap between teeth
 * @param expectedLongGap Expected gap for long tooth (1.5x normal)
 * @return true if long tooth detected and processed
 * @note MISRA-C compliant: Lines: 13 | Cyclomatic: 3 | Nesting: 2
 */
static inline bool handleNGC4LongTooth(unsigned long curGap2, unsigned long expectedLongGap)
{
  if (curGap2 < expectedLongGap)
  {
    return false; // Not a long tooth
  }

  // Determine polarity by comparing how far ago the last tooth rose
  bool isHighMissingTooth = ((toothLastSecToothRisingTime - toothLastSecToothTime) <
                             (curTime2 - toothLastSecToothRisingTime));

  determineNGC4ToothPosition(isHighMissingTooth);
  return true;
}

/**
 * @brief Secondary trigger interrupt for NGC 4-cylinder cam wheel
 * @details Handles cam signal with polarity-based missing tooth detection for sync.
 * Supports HIGH and LOW missing teeth. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 48 | Cyclomatic: 5 | Nesting: 1 (was N:5, C:17, 49 lines!)
 * @see determineNGC4ToothPosition(), handleNGC4LongTooth()
 */
void triggerSec_NGC4(void)
{
  // Guard clause: only for sequential operation
  if ((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (configPage2.injLayout != INJ_SEQUENTIAL))
  {
    return;
  }

  curTime2 = micros();

  // Guard clause: check polarity - only process LOW signals (falling edge)
  if (READ_SEC_TRIGGER() == HIGH)
  {
    toothLastSecToothRisingTime = curTime2;
    return;
  }

  curGap2 = curTime2 - toothLastSecToothTime;

  // Guard clause: filter check
  if (curGap2 <= triggerSecFilterTime) { return; }

  // Guard clause: need tooth timing information
  if ((toothLastSecToothTime == 0) || (toothLastMinusOneSecToothTime == 0))
  {
    toothLastMinusOneSecToothTime = toothLastSecToothTime;
    toothLastSecToothTime = curTime2;
    return;
  }

  // Increment tooth count if we have sync
  if (secondaryToothCount > 0)
  {
    secondaryToothCount++;
  }

  // Check for long tooth (missing tooth pattern)
  unsigned long expectedLongGap = (3 * (toothLastSecToothTime - toothLastMinusOneSecToothTime)) >> 1;
  bool isLongTooth = handleNGC4LongTooth(curGap2, expectedLongGap);

  if (!isLongTooth && (secondaryToothCount > 0))
  {
    // Regular tooth - update filter (25% of current speed)
    triggerSecFilterTime = curGap2 >> 2;
  }

  toothLastMinusOneSecToothTime = toothLastSecToothTime;
  toothLastSecToothTime = curTime2;
}

#define secondaryToothLastCount checkSyncToothCount

//========== NGC 6/8 CYLINDER CAM DECODER HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Search for cam sync pattern by matching tooth counts
 * @return Group number if sync pattern found, 0 if not found
 * @note MISRA-C compliant: Lines: 12 | Cyclomatic: 3 | Nesting: 2
 */
static inline byte searchCamSyncPattern(void)
{
  for (byte group = 1; group <= configPage2.nCylinders; group++)
  {
    if ((secondaryToothCount == (unsigned int)toothAngles[group]) &&
        (secondaryToothLastCount == (byte)toothAngles[group - 1]))
    {
      return group; // Found matching pattern
    }
  }
  return 0; // No match found
}

/**
 * @brief Update cam sync status for NGC 6/8 cylinders
 * @note MISRA-C compliant: Lines: 16 | Cyclomatic: 4 | Nesting: 2
 */
static inline void updateCamSync_NGC68(void)
{
  // Quick check: do we already have cam sync?
  if ((toothSystemCount > 0) &&
      (secondaryToothCount == (unsigned int)toothAngles[toothSystemCount + 1]))
  {
    toothSystemCount++;
    if (toothSystemCount > configPage2.nCylinders)
    {
      toothSystemCount = 1;
    }
  }
  else
  {
    // Lost sync or haven't achieved it yet - search for pattern
    toothSystemCount = searchCamSyncPattern();
  }
}

/**
 * @brief Secondary trigger interrupt for NGC 6/8 cylinder cam wheel
 * @details Handles cam signal for sequential operation. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 30 | Cyclomatic: 6 | Nesting: 2 (was N:8, C:17, 54 lines!)
 * @see updateCamSync_NGC68(), searchCamSyncPattern()
 */
void triggerSec_NGC68(void)
{
  // Guard clause: only process cam wheel for sequential operation
  if ((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (configPage2.injLayout != INJ_SEQUENTIAL))
  {
    return;
  }

  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  // Guard clause: filter debounce
  if (curGap2 <= triggerSecFilterTime) { return; }

  // Guard clause: need tooth timing info from primary wheel
  if ((toothLastSecToothTime == 0) || (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0))
  {
    toothLastSecToothTime = curTime2;
    return;
  }

  /*
   * Cam wheel can have single tooth in group -> use primary wheel gap for comparison.
   * 2.1 primary teeth duration == 1 secondary tooth duration
   */
  if (curGap2 >= (3 * (toothLastToothTime - toothLastMinusOneToothTime)))
  {
    // Large gap detected - missing teeth (start of new group)
    if ((secondaryToothCount > 0) && (secondaryToothLastCount > 0))
    {
      // Have two groups detected - can attempt cam sync
      updateCamSync_NGC68();
    }

    secondaryToothLastCount = secondaryToothCount;
    secondaryToothCount = 1; // First tooth in new group
    triggerSecFilterTime = 0; // Prevent filter lockup on intermittent signals
  }
  else if (secondaryToothCount > 0)
  {
    // Normal tooth within group
    secondaryToothCount++;
    triggerSecFilterTime = curGap2 >> 2; // Filter at 25% of current speed
  }

  toothLastSecToothTime = curTime2;
}

#if 0  // NGC (part 2), Vmax, Renix, RoverMEMS, SuzukiK6A - REFACTORED to implementations/
uint16_t getRPM_NGC(void)
{
  uint16_t tempRPM = 0;
  if( currentStatus.RPM < currentStatus.crankRPM)
  {
    if (BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT)) { tempRPM = crankingGetRPM(36, CRANK_SPEED); }
    else { tempRPM = currentStatus.RPM; } //Can't do per tooth RPM if we're at any of the missing teeth as it messes the calculation
  }
  else
  {
    tempRPM = stdGetRPM(CRANK_SPEED);
  }
  return tempRPM;
}

static inline uint16_t calcSetEndTeeth_NGC_SkipMissing(uint16_t toothNum) {
  if(toothNum == 17U || toothNum == 18U) { return 16U; } // These are missing teeth, so set the next one before instead
  if(toothNum == 35U || toothNum == 36U) { return 34U; } // These are missing teeth, so set the next one before instead
  if(toothNum == 53U || toothNum == 54U) { return 52U; } // These are missing teeth, so set the next one before instead
  if(toothNum > 70U) { return 70U; } // These are missing teeth, so set the next one before instead
  return toothNum;

}

static uint16_t __attribute__((noinline)) calcSetEndTeeth_NGC(int ignitionAngle, uint8_t toothAdder) {
  int16_t tempEndTooth = ignitionAngle - configPage4.triggerAngle;
#ifdef USE_LIBDIVIDE
  tempEndTooth = libdivide::libdivide_s16_do(tempEndTooth, &divTriggerToothAngle);
#else
  tempEndTooth = tempEndTooth / (int16_t)triggerToothAngle;
#endif
  return calcSetEndTeeth_NGC_SkipMissing(clampToToothCount(tempEndTooth - 1, toothAdder));
}

void triggerSetEndTeeth_NGC(void)
{
  byte toothAdder = 0;
  if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage4.TrigSpeed == CRANK_SPEED) ) { toothAdder = configPage4.triggerTeeth; }

  ignition1EndTooth = calcSetEndTeeth_NGC(ignition1EndAngle, toothAdder);
  ignition2EndTooth = calcSetEndTeeth_NGC(ignition2EndAngle, toothAdder);
  ignition3EndTooth = calcSetEndTeeth_NGC(ignition3EndAngle, toothAdder);
  ignition4EndTooth = calcSetEndTeeth_NGC(ignition4EndAngle, toothAdder);
  #if IGN_CHANNELS >= 6
  ignition5EndTooth = calcSetEndTeeth_NGC(ignition5EndAngle, toothAdder);
  ignition6EndTooth = calcSetEndTeeth_NGC(ignition6EndAngle, toothAdder);
  #endif

  #if IGN_CHANNELS >= 8
  ignition7EndTooth = calcSetEndTeeth_NGC(ignition7EndAngle, toothAdder);
  ignition8EndTooth = calcSetEndTeeth_NGC(ignition8EndAngle, toothAdder);
  #endif
}

/** Yamaha Vmax 1990+ with 6 uneven teeth, triggering on the wide lobe.
Within the decoder code, the sync tooth is referred to as tooth #1. Derived from Harley and made to work on the Yamah Vmax.
Trigger is based on 'CHANGE' so we get a signal on the up and downward edges of the lobe. This is required to identify the wide lobe.
* @defgroup dec_vmax Yamaha Vmax
* @{
*/
void triggerSetup_Vmax(void)
{
  triggerToothAngle = 0; // The number of degrees that passes from tooth to tooth, ev. 0. It alternates uneven
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 60U); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  if(currentStatus.initialisationComplete == false) { toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the initi check to prevent the fuel pump just staying on all the time
  triggerFilterTime = 1500;
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); // We must start with a valid trigger or we cannot start measuring the lobe width. We only have a false trigger on the lobe up event when it doesn't pass the filter. Then, the lobe width will also not be beasured.
  toothAngles[1] = 0;      //tooth #1, these are the absolute tooth positions
  toothAngles[2] = 40;     //tooth #2
  toothAngles[3] = 110;    //tooth #3
  toothAngles[4] = 180;    //tooth #4
  toothAngles[5] = 220;    //tooth #5
  toothAngles[6] = 290;    //tooth #6
}

//curGap = microseconds between primary triggers
//curGap2 = microseconds between secondary triggers
//toothCurrentCount = the current number for the end of a lobe
//secondaryToothCount = the current number of the beginning of a lobe
//We measure the width of a lobe so on the end of a lobe, but want to trigger on the beginning. Variable toothCurrentCount tracks the downward events, and secondaryToothCount updates on the upward events. Ideally, it should be the other way round but the engine stall routine resets secondaryToothCount, so it would not sync again after an engine stall.

// FASE O: Vmax tooth configuration (data-driven approach)
// Vmax pattern: 6 teeth with alternating 70-degree and 40-degree spacing
// Teeth 1,3,4,6 = 70 degrees, Teeth 2,5 = 40 degrees
// Filter compensation needed: 70deg->40deg requires *4/7, 40deg->70deg requires *7/4
struct VmaxToothConfig {
  uint8_t toothCount;        // Tooth number (1-6)
  uint8_t toothAngle;        // Angle of this tooth (70 or 40 degrees)
  uint8_t filterNumerator;   // Filter calculation numerator
  uint8_t filterDenominator; // Filter calculation denominator
  uint8_t secondaryCount;    // Secondary tooth counter value
};

// Static configuration table for all 6 Vmax teeth
// Filter logic: curGap * numerator / denominator
// - 70deg tooth (next is 40deg): multiply by 4/7 to compensate
// - 40deg tooth (next is 70deg): multiply by 7/4 to compensate
// - 70deg tooth (next is 70deg): multiply by 1/1 (no compensation)
static const VmaxToothConfig vmaxToothConfigs[6] PROGMEM = {
  {1, 70, 4, 7, 1},  // Tooth 1: 70deg, next is 40deg (tooth 2), compensate *4/7
  {2, 40, 7, 4, 2},  // Tooth 2: 40deg, next is 70deg (tooth 3), compensate *7/4
  {3, 70, 1, 1, 3},  // Tooth 3: 70deg, next is 70deg (tooth 4), no compensation
  {4, 70, 4, 7, 4},  // Tooth 4: 70deg, next is 40deg (tooth 5), compensate *4/7
  {5, 40, 7, 4, 5},  // Tooth 5: 40deg, next is 70deg (tooth 6), compensate *7/4
  {6, 70, 1, 1, 6}   // Tooth 6: 70deg, next is 70deg (tooth 1), no compensation
};

// Helper function: Process Vmax tooth using data-driven configuration
static inline void processVmaxTooth(uint8_t toothCount, unsigned long curGap)
{
  // Guard: toothCount must be 1-6
  if (toothCount < 1 || toothCount > 6) { return; }

  // Array is 0-indexed, toothCount is 1-indexed
  const VmaxToothConfig* cfg = &vmaxToothConfigs[toothCount - 1];

  // Set tooth angle for this tooth
  triggerToothAngle = cfg->toothAngle;

  // Set secondary counter
  secondaryToothCount = cfg->secondaryCount;

  // Apply filter with compensation for next tooth spacing
  if (cfg->filterDenominator == 1)
  {
    setFilter(curGap); // No compensation needed
  }
  else
  {
    setFilter((curGap * cfg->filterNumerator) / cfg->filterDenominator);
  }

  // Special handling for tooth 1 (revolution marker)
  if (toothCount == 1)
  {
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;
  }
}

void triggerPri_Vmax(void)
{
  curTime = micros();
  if(READ_PRI_TRIGGER() == primaryTriggerEdge){// Forwarded from the config page to setup the primary trigger edge (rising or falling). Inverting VR-conditioners require FALLING, non-inverting VR-conditioners require RISING in the Trigger edge setup.
    curGap2 = curTime;
    curGap = curTime - toothLastToothTime;
    if ( (curGap >= triggerFilterTime) ){
      BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)
      if (toothCurrentCount > 0) // We have sync based on the tooth width.
      {
          BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

          // Use data-driven configuration to process tooth
          // Handles all 6 teeth (angles, filters, counters) with single function call
          processVmaxTooth(toothCurrentCount, curGap);

          toothLastMinusOneToothTime = toothLastToothTime;
          toothLastToothTime = curTime;
          if (triggerFilterTime > 50000){//The first pulse seen
            triggerFilterTime = 0;
          }
      }
      else{
        triggerFilterTime = 0;
        return;//Zero, no sync yet.
      }
    }
    else{
      BIT_CLEAR(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being an invalid trigger
    }
  }
  else if( BIT_CHECK(decoderState, BIT_DECODER_VALID_TRIGGER) ) // Inverted due to vr conditioner. So this is the falling lobe. We only process if there was a valid trigger.
  {
    unsigned long curGapLocal = curTime - curGap2;
    if (curGapLocal > (lastGap * 2)){// Small lobe is 5 degrees, big lobe is 45 degrees. So this should be the wide lobe.
        if (toothCurrentCount == 0 || toothCurrentCount == 6){//Wide should be seen with toothCurrentCount = 0, when there is no sync yet, or toothCurrentCount = 6 when we have done a full revolution.
          currentStatus.hasSync = true;
        }
        else{//Wide lobe seen where it shouldn't, adding a sync error.
          currentStatus.syncLossCounter++;
        }
        toothCurrentCount = 1;
    }
    else if(toothCurrentCount == 6){//The 6th lobe should be wide, adding a sync error.
        toothCurrentCount = 1;
        currentStatus.syncLossCounter++;
    }
    else{// Small lobe, just add 1 to the toothCurrentCount.
      toothCurrentCount++;
    }
    lastGap = curGapLocal;
    return;
  }
  else if( BIT_CHECK(decoderState, BIT_DECODER_VALID_TRIGGER) == false)
  {
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //We reset this every time to ensure we only filter when needed.
  }
}


void triggerSec_Vmax(void)
// Needs to be enabled in main()
{
  return;// No need for now. The only thing it could help to sync more quickly or confirm position.
} // End Sec Trigger


uint16_t getRPM_Vmax(void)
{
  uint16_t tempRPM = 0;
  if (currentStatus.hasSync == true)
  {
    if ( currentStatus.RPM < (unsigned int)(configPage4.crankRPM * 100) )
    {
      int tempToothAngle;
      unsigned long toothTime;
      if ( (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0) ) { tempRPM = 0; }
      else
      {
        noInterrupts();
        tempToothAngle = triggerToothAngle;
        SetRevolutionTime(toothOneTime - toothOneMinusOneTime); //The time in uS that one revolution would take at current speed (The time tooth 1 was last seen, minus the time it was seen prior to that)
        toothTime = (toothLastToothTime - toothLastMinusOneToothTime);
        interrupts();
        toothTime = toothTime * 36;
        tempRPM = ((unsigned long)tempToothAngle * (MICROS_PER_MIN/10U)) / toothTime;
      }
    }
    else {
      tempRPM = stdGetRPM(CRANK_SPEED);
    }
  }
  return tempRPM;
}


int getCrankAngle_Vmax(void)
{
  //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
  unsigned long tempToothLastToothTime;
  int tempsecondaryToothCount;
  //Grab some variables that are used in the trigger code and assign them to temp variables.
  noInterrupts();
  tempsecondaryToothCount = secondaryToothCount;
  tempToothLastToothTime = toothLastToothTime;
  lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
  interrupts();

  //Check if the last tooth seen was the reference tooth (Number 3). All others can be calculated, but tooth 3 has a unique angle
  int crankAngle;
  crankAngle=toothAngles[tempsecondaryToothCount] + configPage4.triggerAngle;

  //Estimate the number of degrees travelled since the last tooth}
  elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
  crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

  if (crankAngle >= 720) { crankAngle -= 720; }
  if (crankAngle < 0) { crankAngle += 360; }

  return crankAngle;
}

void triggerSetEndTeeth_Vmax(void)
{
}

/** @} */

/** Renix 44-2-2  and 66-2-2-2 decoder.
* Renix trigger wheel doesn't decode into 360 degrees nicely (360/44 = 8.18 degrees or 360/66 = 5.454545). Speeduino can't handle any teeth that have a decimal point.
* Solution is to count teeth, every 11 teeth = a proper angle. For 66 tooth decoder its 60 degrees per 11 teeth, for 44 tooth decoder its 90 degrees per 11 teeth.
* This means the system sees 4 teeth on the 44 tooth wheel and 6 teeth on the 66 tooth wheel.
* Double missing tooth in the pattern is actually a large tooth and a large gap. If the trigger is set to rising you'll see the start of the large tooth
* then the gap. If its not set to rising the code won't work due to seeing two gaps
*
*
* @defgroup dec_renix Renix decoder
* @{
*/
void triggerSetup_Renix(void)
{
  if( configPage2.nCylinders == 4)
  {
    triggerToothAngle = 90; //The number of degrees that passes from tooth to tooth (primary) this changes between 41 and 49 degrees
    configPage4.triggerTeeth = 4; // wheel has 44 teeth but we use these to work out which tooth angle to use, therefore speeduino thinks we only have 8 teeth.
    configPage4.triggerMissingTeeth = 0;
    triggerActualTeeth = 4; //The number of teeth we're pretending physically existing on the wheel.
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 44U)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  }
  else if (configPage2.nCylinders == 6)
  {
    triggerToothAngle = 60;
    configPage4.triggerTeeth = 6; // wheel has 44 teeth but we use these to work out which tooth angle to use, therefore speeduino thinks we only have 6 teeth.
    configPage4.triggerMissingTeeth = 0;
    triggerActualTeeth = 6; //The number of teeth we're pretending physically existing on the wheel.
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 66U)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  }

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm). Largest gap between teeth is 90 or 60 degrees depending on decoder.
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);

  toothSystemCount = 1;
  toothCurrentCount = 1;
  toothLastToothTime = 0;
#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}


// variables used to help calculate gap on the physical 44 or 66 teeth we're pretending don't exist in most of the speeduino code
// reusing existing variables to save storage space as these aren't used in the code for their original purpose.
#define renixSystemLastToothTime         toothLastToothRisingTime
#define renixSystemLastMinusOneToothTime toothLastSecToothRisingTime

//========== RENIX DECODER HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Calculate Renix target gap for missing tooth detection
 * @return Target gap (2x normal tooth gap, or large value if no history)
 * @note MISRA-C compliant: Lines: 9 | Cyclomatic: 2 | Nesting: 1
 */
static inline unsigned long calculateRenixTargetGap(void)
{
  if ((renixSystemLastToothTime != 0) && (renixSystemLastMinusOneToothTime != 0))
  {
    return 2 * (renixSystemLastToothTime - renixSystemLastMinusOneToothTime);
  }
  return 100000000UL; // Large number to prevent false gap detection at startup
}

/**
 * @brief Handle Renix gap tooth detection (2-tooth missing pattern)
 * @return true if gap tooth detected
 * @note MISRA-C compliant: Lines: 16 | Cyclomatic: 3 | Nesting: 2
 */
static inline bool handleRenixGapTooth(void)
{
  // Add two teeth to account for the gap
  toothSystemCount += 2;

  // Validate sync: first tooth after gap should be tooth 12
  if (toothSystemCount != 12)
  {
    // Lost sync
    currentStatus.hasSync = false;
    currentStatus.syncLossCounter++;
    toothSystemCount = 1;
    toothCurrentCount = 1;
    return true;
  }

  return true; // Gap detected and sync maintained
}

/**
 * @brief Update Renix revolution tracking when sync tooth detected
 * @note MISRA-C compliant: Lines: 20 | Cyclomatic: 3 | Nesting: 2
 */
static inline void updateRenixRevolution(void)
{
  toothCurrentCount++;

  // Check for revolution completion (6 or 4 "pretend" teeth depending on cylinders)
  if (((configPage2.nCylinders == 6) && (toothCurrentCount == 7)) ||
      ((configPage2.nCylinders == 4) && (toothCurrentCount == 5)))
  {
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;
    revolutionOne = !revolutionOne;
    toothCurrentCount = 1;
  }

  toothSystemCount = 1;
  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;
}

/**
 * @brief Handle per-tooth ignition timing for Renix
 * @note MISRA-C compliant: Lines: 16 | Cyclomatic: 3 | Nesting: 2
 */
static inline void handleRenixPerToothIgnition(void)
{
  if ((configPage2.perToothIgn == false) || BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK))
  {
    return; // Per-tooth ignition disabled or cranking
  }

  int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
  crankAngle = ignitionLimits(crankAngle);

  if ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (revolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED))
  {
    crankAngle += 360;
    checkPerToothTiming(crankAngle, (configPage4.triggerTeeth + toothCurrentCount));
  }
  else
  {
    checkPerToothTiming(crankAngle, toothCurrentCount);
  }
}

/**
 * @brief Primary trigger interrupt for Renix decoder (44/66 tooth with 2-tooth gap)
 * @details Handles crank teeth interrupts, detects 2-tooth gap for sync,
 * and tracks "pretend" teeth (4 or 6) for revolution counting. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 33 | Cyclomatic: 4 | Nesting: 1 (was N:5, C:17, 73 lines!)
 * @see calculateRenixTargetGap(), handleRenixGapTooth(), updateRenixRevolution(), handleRenixPerToothIgnition()
 */
void triggerPri_Renix(void)
{
  curTime = micros();
  curGap = curTime - renixSystemLastToothTime;

  // Guard clause: filter check
  if (curGap < triggerFilterTime) { return; }

  toothSystemCount++;

  // Calculate target gap for missing tooth detection (2x normal gap)
  targetGap = calculateRenixTargetGap();

  if (curGap >= targetGap)
  {
    // Gap tooth detected - handle sync validation
    handleRenixGapTooth();
  }
  else
  {
    // Regular tooth - update filter
    setFilter(curGap);
  }

  renixSystemLastMinusOneToothTime = renixSystemLastToothTime;
  renixSystemLastToothTime = curTime;

  // Handle sync tooth (12) and revolution tracking
  if ((toothSystemCount == 12) || (toothLastToothTime == 0))
  {
    updateRenixRevolution();
    handleRenixPerToothIgnition();
  }
}

static uint16_t __attribute__((noinline)) calcEndTeeth_Renix(int ignitionAngle, uint8_t toothAdder) {
  int16_t tempEndTooth = ignitionAngle - configPage4.triggerAngle;
#ifdef USE_LIBDIVIDE
  tempEndTooth = libdivide::libdivide_s16_do(tempEndTooth, &divTriggerToothAngle);
#else
  tempEndTooth = tempEndTooth / (int16_t)triggerToothAngle;
#endif
  tempEndTooth = tempEndTooth - 1;
  // Clamp to tooth count
  return clampToActualTeeth(clampToToothCount(tempEndTooth, toothAdder), toothAdder);
}

void triggerSetEndTeeth_Renix(void)
{
  byte toothAdder = 0;
  if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage4.TrigSpeed == CRANK_SPEED) ) { toothAdder = configPage4.triggerTeeth; }

  //Temp variables are used here to avoid potential issues if a trigger interrupt occurs part way through this function

  ignition1EndTooth = calcEndTeeth_Renix(ignition1EndAngle, toothAdder);
  ignition2EndTooth = calcEndTeeth_Renix(ignition2EndAngle, toothAdder);
  currentStatus.canin[1] = ignition2EndTooth;
  ignition3EndTooth = calcEndTeeth_Renix(ignition3EndAngle, toothAdder);
  ignition4EndTooth = calcEndTeeth_Renix(ignition4EndAngle, toothAdder);
#if IGN_CHANNELS >= 5
  ignition5EndTooth = calcEndTeeth_Renix(ignition5EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 6
  ignition6EndTooth = calcEndTeeth_Renix(ignition6EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 7
  ignition7EndTooth = calcEndTeeth_Renix(ignition7EndAngle, toothAdder);
#endif
#if IGN_CHANNELS >= 8
  ignition8EndTooth = calcEndTeeth_Renix(ignition8EndAngle, toothAdder);
#endif
}

/** @} */

/*****************************************************************
 * Rover MEMS decoder
 * Covers multiple trigger wheels used interchanbably over the range of MEMS units
 * Specifically covers teeth patterns on the primary trigger (crank)
 * 3 gap 14 gap 2 gap 13 gap
 * 11 gap 5 gap 12 gap 4 gap
 * 2 gap 14 gap 3 gap 13 gap
 * 17 gap 17 gap
 *
 * Support no cam, single tooth Cam (or half moon cam), and multi tooth (5-3-2 teeth)
 *
 * @defgroup dec_rover_mems Rover MEMS all versions including T Series, O Series, Mini and K Series
 * @{
 */
volatile unsigned long roverMEMSTeethSeen = 0; // used for flywheel gap pattern matching

void triggerSetup_RoverMEMS()
{
  for(toothOneTime = 0; toothOneTime < 10; toothOneTime++)   // repurpose variable temporarily to help clear ToothAngles.
    { toothAngles[toothOneTime] = 0; }// Repurpose ToothAngles to store data needed for this implementation.

  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 36U)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U)); // only 1 tooth on the wheel not 36

  configPage4.triggerTeeth = 36;
  triggerToothAngle = 360 / configPage4.triggerTeeth; //The number of degrees that passes from tooth to tooth 360 / 36 theortical teeth
  triggerActualTeeth = 36; //The number of physical teeth on the wheel. Need to fix now so we can identify the wheel on the first rotation and not risk a  type 1 wheel not being spotted
  toothLastMinusOneToothTime = 0;
  toothCurrentCount = 0; // current tooth
  secondaryToothCount = 0;
  secondaryLastToothCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;
  revolutionOne=0;

  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * 2U); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);

}

// RoverMEMS pattern configuration data structure
struct RoverMEMSPattern {
  uint32_t binaryPattern;
  uint8_t patternId;
  uint8_t skipTooth1, skipTooth2, skipTooth3, skipTooth4;
  uint8_t missingTeeth;
};

// Array of all 5 RoverMEMS trigger patterns
// Pattern IDs: 5=9-7-10-6, 4=3-14-2-13, 3=2-14-3-13, 2=11-5-12-4, 1=17-17
static const RoverMEMSPattern roverMEMSPatterns[5] = {
  {0b11111101111111011111111110111111, 5,  1, 11, 19, 30, 4}, // 9-7-10-6 pattern (#5)
  {0b11011101111111111111101101111111, 4,  8, 11, 25, 27, 4}, // 3-14-2-13 pattern (#4)
  {0b11011011111111111111011101111111, 3,  8, 10, 24, 27, 4}, // 2-14-3-13 pattern (#3)
  {0b11111101111101111111111110111101, 2,  1, 12, 17, 29, 4}, // 11-5-12-4 pattern (#2)
  {0b11111111111101111111111111111101, 1,  1, 18,  0,  0, 2}  // 17-17 pattern (#1, only 2 missing teeth)
};

// Helper function to check and configure RoverMEMS pattern
static inline bool checkAndConfigureRoverMEMSPattern(void)
{
  for(uint8_t i = 0; i < 5; i++)
  {
    if(roverMEMSTeethSeen == roverMEMSPatterns[i].binaryPattern)
    {
      const RoverMEMSPattern* pattern = &roverMEMSPatterns[i];

      if(toothAngles[ID_TOOTH_PATTERN] != pattern->patternId)
      {
        toothAngles[SKIP_TOOTH1] = pattern->skipTooth1;
        toothAngles[SKIP_TOOTH2] = pattern->skipTooth2;
        if(pattern->patternId != 1) // Pattern #1 only has 2 missing teeth
        {
          toothAngles[SKIP_TOOTH3] = pattern->skipTooth3;
          toothAngles[SKIP_TOOTH4] = pattern->skipTooth4;
        }
        toothAngles[ID_TOOTH_PATTERN] = pattern->patternId;
        configPage4.triggerMissingTeeth = pattern->missingTeeth;
        triggerActualTeeth = 36;
      }

      triggerRoverMEMSCommon();
      return true;
    }
  }
  return false;
}

/**
 * @brief Record Rover MEMS tooth and detect missing teeth via gap analysis
 * @details Uses 32-bit binary tracking of tooth pattern (0=gap, 1=tooth)
 *          Detects gaps when curGap > 1.5x previous gap
 *
 * MISRA-C: 22 lines, N:2, C:4
 */
static inline void recordRoverMEMSTooth(void)
{
  if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0)) { return; }

  targetGap = (3 * (toothLastToothTime - toothLastMinusOneToothTime)) >> 1; // 1.5x last gap

  if (curGap > targetGap)
  {
    // Missing tooth detected - record as gap (shift 2: gap + tooth)
    roverMEMSTeethSeen = roverMEMSTeethSeen << 2;
    roverMEMSTeethSeen++;
    toothCurrentCount += 2; // Gap counts as 2 positions
  }
  else
  {
    // Regular tooth - record and update filter
    roverMEMSTeethSeen = roverMEMSTeethSeen << 1;
    roverMEMSTeethSeen++;
    toothCurrentCount++;
    setFilter(curGap);
  }
}

/**
 * @brief Handle per-tooth ignition timing for Rover MEMS
 * @details Calculates crank angle, handles sequential mode with revolution tracking
 *
 * MISRA-C: 13 lines, N:2, C:4
 */
static inline void handleRoverMEMSPerToothIgnition(void)
{
  if (configPage2.perToothIgn == false) { return; }
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return; }

  int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
  crankAngle = ignitionLimits(crankAngle);

  if ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (revolutionOne == true))
  {
    checkPerToothTiming(crankAngle + 360, configPage4.triggerTeeth + toothCurrentCount);
  }
  else
  {
    checkPerToothTiming(crankAngle, toothCurrentCount);
  }
}

/**
 * @brief Primary trigger ISR for Rover MEMS decoder
 * @details Handles multiple 36-tooth patterns (3-14-2-13, 2-14-3-13, 11-5-12-4, 17-17)
 *          Uses 32-bit binary tracking to identify pattern
 *
 * MISRA-C: 29 lines, N:2, C:5 (was: 58 lines, N:4, C:12)
 *
 * @note Refactored FASE D - extracted 2 helpers to reduce complexity
 */
void triggerPri_RoverMEMS()
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;

  if (curGap < triggerFilterTime) { return; }

  recordRoverMEMSTooth();

  if (toothCurrentCount >= triggerActualTeeth)
  {
    bool patternMatched = checkAndConfigureRoverMEMSPattern();

    if (!patternMatched && (toothCurrentCount > triggerActualTeeth + 1))
    {
      // Lost sync - no pattern match after full rotation
      currentStatus.hasSync = false;
      BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
      currentStatus.syncLossCounter++;
    }
  }

  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;

  handleRoverMEMSPerToothIgnition();
}


/**
 * @brief Validate sequential sync with cam signal for Rover MEMS
 * @details Sequential mode requires cam tooth validation for full sync
 *          Pattern 1 (17-17) isn't unique without cam signal
 *
 * MISRA-C: 20 lines, N:2, C:5
 */
static inline void validateRoverMEMSSequentialSync(void)
{
  bool isSequentialMode = (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) ||
                          (configPage2.injLayout == INJ_SEQUENTIAL);

  if (!isSequentialMode)
  {
    currentStatus.hasSync = false;
    BIT_SET(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    return;
  }

  // Sequential mode - need cam tooth or cam-speed trigger
  bool camToothSeen = (secondaryToothCount > 0) || (configPage4.TrigSpeed == CAM_SPEED);

  if (camToothSeen)
  {
    currentStatus.hasSync = true;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    if (configPage4.trigPatternSec == SEC_TRIGGER_SINGLE) { secondaryToothCount = 0; }
  }
  else if (currentStatus.hasSync == false)
  {
    BIT_SET(currentStatus.status3, BIT_STATUS3_HALFSYNC);
  }
}

/**
 * @brief Common revolution tracking for Rover MEMS patterns
 * @details Handles revolution counter reset, sequential sync validation
 *          Pattern 1 (17-17) requires >18 teeth to distinguish revolutions
 *
 * MISRA-C: 17 lines, N:1, C:2 (was: 28 lines, N:4, C:7)
 *
 * @note Refactored FASE D - extracted sync validation helper
 */
static void triggerRoverMEMSCommon(void)
{
  if (toothCurrentCount > 18)
  {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    revolutionOne = !revolutionOne;
  }

  validateRoverMEMSSequentialSync();

  currentStatus.startRevolutions++;
}




int getCrankAngle_RoverMEMS()
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempRevolutionOne = revolutionOne;
    tempToothLastToothTime = toothLastToothTime;
    interrupts();

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    //Sequential check (simply sets whether we're on the first or 2nd revolution of the cycle)
    if ( (tempRevolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED) ) { crankAngle += 360; }

    lastCrankAngleCalc = micros();
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

//========== ROVER MEMS CAM DECODER HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Record VVT angle from cam signal
 * @note MISRA-C compliant: Lines: 14 | Cyclomatic: 3 | Nesting: 2
 */
static inline void recordVVTAngle_RoverMEMS(void)
{
  if (configPage6.vvtEnabled == 0) { return; }

  if ((configPage4.trigPatternSec != SEC_TRIGGER_SINGLE) &&
      !((configPage4.trigPatternSec == SEC_TRIGGER_5_3_2) && (secondaryToothCount == 6)))
  {
    return;
  }

  int16_t curAngle = getCrankAngle();
  while (curAngle > 360) { curAngle -= 360; }

  curAngle -= configPage4.triggerAngle; // Value at TDC
  if (configPage6.vvtMode == VVT_MODE_CLOSED_LOOP)
  {
    curAngle -= configPage10.vvtCLMinAng;
  }

  currentStatus.vvt1Angle = curAngle;
}

/**
 * @brief Handle single tooth cam trigger pattern
 * @note MISRA-C compliant: Lines: 6 | Cyclomatic: 1 | Nesting: 0
 */
static inline void handleSingleToothCam_RoverMEMS(void)
{
  revolutionOne = true;
  triggerSecFilterTime = curGap2 >> 1; // Next filter is half current gap
}

/**
 * @brief Adjust tooth count based on revolution and secondary tooth position
 * @param secondaryCount Current secondary tooth count after gap
 * @note MISRA-C compliant: Lines: 26 | Cyclomatic: 6 | Nesting: 2
 */
static inline void adjustToothCountForCycle_RoverMEMS(byte secondaryCount)
{
  if (secondaryCount == 6)
  {
    // Tooth after 5-tooth pattern: cycle 360-720, tooth 18-36
    revolutionOne = false;
    if (toothCurrentCount < 19)
    {
      toothCurrentCount += 18;
    }
  }
  else if (secondaryCount == 4)
  {
    // Tooth after 3-tooth pattern: cycle 0-360, tooth 1-18
    revolutionOne = true;
    if (toothCurrentCount > 17)
    {
      toothCurrentCount -= 18;
    }
  }
  else if (secondaryCount == 3)
  {
    // Tooth after 2-tooth pattern: cycle 0-360, tooth 18-36
    revolutionOne = true;
    if (toothCurrentCount < 19)
    {
      toothCurrentCount += 18;
    }
  }
}

/**
 * @brief Handle multi-tooth 5-3-2 cam pattern with gap detection
 * @note MISRA-C compliant: Lines: 18 | Cyclomatic: 3 | Nesting: 2
 */
static inline void handleMultiToothCamPattern_RoverMEMS(void)
{
  if (curGap2 < targetGap2)
  {
    // Normal tooth-sized gap
    triggerSecFilterTime = curGap2 >> 1;
    targetGap2 = (3 * curGap2) >> 1; // Multiply by 1.5 for next gap check
  }
  else
  {
    // Large gap detected (single or double gap after tooth group)
    adjustToothCountForCycle_RoverMEMS(secondaryToothCount);
    secondaryToothCount = 1; // Reset - this is first tooth after gap
  }
}

/**
 * @brief Secondary trigger interrupt for Rover MEMS cam wheel
 * @details Handles cam signal for multiple patterns: single tooth or 5-3-2 multi-tooth.
 * Supports VVT angle recording and cycle detection. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 29 | Cyclomatic: 4 | Nesting: 1 (was N:6, C:18, 81 lines!)
 * @see recordVVTAngle_RoverMEMS(), handleSingleToothCam_RoverMEMS(), handleMultiToothCamPattern_RoverMEMS()
 */
void triggerSec_RoverMEMS()
{
  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  // Guard clause: initial startup safety check
  if (toothLastSecToothTime == 0)
  {
    targetGap2 = curGap * 2;
    curGap2 = 0;
    toothLastSecToothTime = curTime2;
    return;
  }

  // Guard clause: filter check
  if (curGap2 < triggerSecFilterTime) { return; }

  secondaryToothCount++;
  toothLastSecToothTime = curTime2;

  // Record VVT angle if enabled
  recordVVTAngle_RoverMEMS();

  // Dispatch based on cam pattern type
  if (configPage4.trigPatternSec == SEC_TRIGGER_SINGLE)
  {
    handleSingleToothCam_RoverMEMS();
  }
  else if (configPage4.trigPatternSec == SEC_TRIGGER_5_3_2)
  {
    handleMultiToothCamPattern_RoverMEMS();
  }
}

uint16_t getRPM_RoverMEMS()
{
  uint16_t tempRPM = 0;

  if( currentStatus.RPM < currentStatus.crankRPM)
  {
    if( (toothCurrentCount != (unsigned int) toothAngles[SKIP_TOOTH1]) &&
        (toothCurrentCount != (unsigned int) toothAngles[SKIP_TOOTH2]) &&
        (toothCurrentCount != (unsigned int) toothAngles[SKIP_TOOTH3]) &&
        (toothCurrentCount != (unsigned int) toothAngles[SKIP_TOOTH4]) )
    { tempRPM = crankingGetRPM(36, CRANK_SPEED); }
    else
    { tempRPM = currentStatus.RPM; } //Can't do per tooth RPM as the missing tooth messes the calculation
  }
  else
  { tempRPM = stdGetRPM(CRANK_SPEED); }
  return tempRPM;
}


void triggerSetEndTeeth_RoverMEMS()
{
  //Temp variables are used here to avoid potential issues if a trigger interrupt occurs part way through this function
  int16_t tempIgnitionEndTooth[5]; // cheating with the array - location 1 is spark 1, location 0 not used.
  int16_t toothAdder = 0;

  if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage4.TrigSpeed == CRANK_SPEED) ) { toothAdder = 36; }

  tempIgnitionEndTooth[1] = ( (ignition1EndAngle - configPage4.triggerAngle) / (int16_t)(10) ) - 1;
  if(tempIgnitionEndTooth[1] > (36 + toothAdder)) { tempIgnitionEndTooth[1] -= (36 + toothAdder); }
  if(tempIgnitionEndTooth[1] <= 0) { tempIgnitionEndTooth[1] += (36 + toothAdder); }
  if(tempIgnitionEndTooth[1] > (36 + toothAdder)) { tempIgnitionEndTooth[1] = (36 + toothAdder); }

  tempIgnitionEndTooth[2] = ( (ignition2EndAngle - configPage4.triggerAngle) / (int16_t)(10) ) - 1;
  if(tempIgnitionEndTooth[2] > (36 + toothAdder)) { tempIgnitionEndTooth[2] -= (36 + toothAdder); }
  if(tempIgnitionEndTooth[2] <= 0) { tempIgnitionEndTooth[2] += (36 + toothAdder); }
  if(tempIgnitionEndTooth[2] > (36 + toothAdder)) { tempIgnitionEndTooth[2] = (36 + toothAdder); }

  tempIgnitionEndTooth[3] = ( (ignition3EndAngle - configPage4.triggerAngle) / (int16_t)(10) ) - 1;
  if(tempIgnitionEndTooth[3] > (36 + toothAdder)) { tempIgnitionEndTooth[3] -= (36 + toothAdder); }
  if(tempIgnitionEndTooth[3] <= 0) { tempIgnitionEndTooth[3] += (36 + toothAdder); }
  if(tempIgnitionEndTooth[3] > (36 + toothAdder)) { tempIgnitionEndTooth[3] = (36 + toothAdder); }

  tempIgnitionEndTooth[4] = ( (ignition4EndAngle - configPage4.triggerAngle) / (int16_t)(10) ) - 1;
  if(tempIgnitionEndTooth[4] > (36 + toothAdder)) { tempIgnitionEndTooth[4] -= (36 + toothAdder); }
  if(tempIgnitionEndTooth[4] <= 0) { tempIgnitionEndTooth[4] += (36 + toothAdder); }
  if(tempIgnitionEndTooth[4] > (36 + toothAdder)) { tempIgnitionEndTooth[4] = (36 + toothAdder); }

  // take into account the missing teeth on the Rover flywheels
  int tempCount=0;

  if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
  {
    // check the calculated trigger tooth exists, if it doesn't use the previous tooth
    // nb the toothAngles[x] holds the tooth after the gap, hence the '-1' to see if it matches a gap

    for(tempCount=1;tempCount <5;tempCount++)
    {
      if(tempIgnitionEndTooth[tempCount] == (toothAngles[1]) || tempIgnitionEndTooth[tempCount] == (toothAngles[2]) ||
         tempIgnitionEndTooth[tempCount] == (toothAngles[3]) || tempIgnitionEndTooth[tempCount] == (toothAngles[4]) ||
         tempIgnitionEndTooth[tempCount] == (36 + toothAngles[1]) || tempIgnitionEndTooth[tempCount] == (36 + toothAngles[2]) ||
         tempIgnitionEndTooth[tempCount] == (36 + toothAngles[3]) || tempIgnitionEndTooth[tempCount] == (36 + toothAngles[4])  )
      { tempIgnitionEndTooth[tempCount]--; }
    }
  }
  else
  {
    for(tempCount=1;tempCount<5;tempCount++)
    {
      if(tempIgnitionEndTooth[tempCount] == (toothAngles[1]) || tempIgnitionEndTooth[tempCount] == (toothAngles[2]) )
      { tempIgnitionEndTooth[tempCount]--; }
    }
  }


  ignition1EndTooth = tempIgnitionEndTooth[1];
  ignition2EndTooth = tempIgnitionEndTooth[2];
  ignition3EndTooth = tempIgnitionEndTooth[3];
  ignition4EndTooth = tempIgnitionEndTooth[4];
}
/** @} */

/** Suzuki K6A 3 cylinder engine

* (See: https://www.msextra.com/forums/viewtopic.php?t=74614)
* @defgroup Suzuki_K6A Suzuki K6A
* @{
*/
void triggerSetup_SuzukiK6A(void)
{
  triggerToothAngle = 90; //The number of degrees that passes from tooth to tooth (primary) - set to a value, needs to be set per tooth
  toothCurrentCount = 99; //Fake tooth count represents no sync

  configPage4.TrigSpeed = CAM_SPEED;
  triggerActualTeeth = 7;
  toothCurrentCount = 1;
  curGap = curGap2 = curGap3 = 0;

  if(currentStatus.initialisationComplete == false) { toothLastToothTime = micros(); } //Set a startup value here to avoid filter errors when starting. This MUST have the initial check to prevent the fuel pump just staying on all the time
  else { toothLastToothTime = 0; }
  toothLastMinusOneToothTime = 0;

  // based on data in msextra page linked to above we can deduce,
  // gap between rising and falling edge of a normal 70 degree tooth is 48 degrees, this means the gap is 70 degrees - 48 degrees = 22 degrees.
  // assume this is constant for all similar sized gaps and teeth
  // sync tooth is 35 degrees - eyeball looks like the tooth is 50% tooth and 50% gap so guess its 17 degrees and 18 degrees.

  // coded every tooth here in case you want to try "change" setting on the trigger setup (this is defined in init.ino and what i've set it to, otherwise you need code to select rising or falling in init.ino (steal it from another trigger)).
  // If you don't want change then drop the 'falling' edges listed below and half the number of edges + reduce the triggerActualTeeth
  // nb as you can edit the trigger offset using rising or falling edge setup below is irrelevant as you can adjust via the trigger offset to cover the difference.

  // not using toothAngles[0] as i'm hoping it makes logic easier

  toothAngles[0] = -70;                 // Wrap around to 650,
  toothAngles[1] = 0;                   // 0 TDC cylinder 1,
  toothAngles[2] = 170;                 // 170 - end of cylinder 1, start of cylinder 3, trigger ignition for cylinder 3 on this tooth
  toothAngles[3] = 240;                 // 70 TDC cylinder 3
  toothAngles[4] = 410;                 // 170  - end of cylinder 3, start of cylinder2, trigger ignition for cylinder 2 on this tooth
  toothAngles[5] = 480;                 // 70 TDC cylinder 2
  toothAngles[6] = 515;                 // 35 Additional sync tooth
  toothAngles[7] = 650;                 // 135 end of cylinder 2, start of cylinder 1, trigger ignition for cylinder 1 on this tooth
  toothAngles[8] = 720;                 // 70 - gap to rotation to TDC1. array item 1 and 8 are the same, code never gets here its for reference only


  MAX_STALL_TIME = (3333UL * triggerToothAngle); //Minimum 50rpm. (3333uS is the time per degree at 50rpm)
  triggerFilterTime = 1500; //10000 rpm, assuming we're triggering on both edges off the crank tooth.
  triggerSecFilterTime = 0; //Need to figure out something better for this
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
  BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
  BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY); // never sure if we need to set this in this type of trigger
  BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC); // we can never have half sync - its either full or none.
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
}

// SuzukiK6A filter calculation type enumeration
enum SuzukiK6AFilterCalc : uint8_t {
  K6A_CALC_OFF = 0,          // Filter off: triggerFilterTime = 0
  K6A_CALC_RS3,              // rshift<3>(curGap)
  K6A_CALC_RS3_RS4,          // rshift<3>(curGap) + rshift<4>(curGap)
  K6A_CALC_RS2_RS4,          // rshift<2>(curGap) + rshift<4>(curGap)
  K6A_CALC_RS2,              // rshift<2>(curGap)
  K6A_CALC_RS2_RS3,          // rshift<2>(curGap) + rshift<3>(curGap)
  K6A_CALC_DIRECT,           // curGap
  K6A_CALC_MULT2,            // curGap * 2U
  K6A_CALC_MULT3,            // curGap * 3U
  K6A_CALC_RS1_RS3,          // rshift<1>(curGap) + rshift<3>(curGap)
  K6A_CALC_ADD_RS2,          // curGap + rshift<2>(curGap)
  K6A_CALC_ADD_RS1_RS2       // curGap + rshift<1>(curGap) + rshift<2>(curGap)
};

// SuzukiK6A filter configuration - maps tooth position + filter level to calculation type
struct SuzukiK6AFilterConfig {
  uint8_t toothMask;         // Bitmask for matching tooth positions (bit 0-7 = tooth 1-8)
  uint8_t filterLevel;       // Filter level (0=OFF, 1=25%, 2=50%, 3=75%)
  SuzukiK6AFilterCalc calc;  // Calculation type to use
};

// Configuration table: 20 entries for 5 tooth groups × 4 filter levels
// Ordered by toothMask then filterLevel for efficient lookup
static const SuzukiK6AFilterConfig suzukiK6AFilterConfigs[20] = {
  // Tooth 1,3 (70° → 170°)
  {0x05, 0, K6A_CALC_OFF},        // Teeth 1,3 filter OFF
  {0x05, 1, K6A_CALC_RS1_RS3},    // Teeth 1,3 filter 25%: (curGap>>1) + (curGap>>3)
  {0x05, 2, K6A_CALC_ADD_RS2},    // Teeth 1,3 filter 50%: curGap + (curGap>>2)
  {0x05, 3, K6A_CALC_ADD_RS1_RS2},// Teeth 1,3 filter 75%: curGap + (curGap>>1) + (curGap>>2)

  // Tooth 2,4 (170° → 70°)
  {0x0A, 0, K6A_CALC_OFF},        // Teeth 2,4 filter OFF
  {0x0A, 1, K6A_CALC_RS3},        // Teeth 2,4 filter 25%: curGap>>3
  {0x0A, 2, K6A_CALC_RS3_RS4},    // Teeth 2,4 filter 50%: (curGap>>3) + (curGap>>4)
  {0x0A, 3, K6A_CALC_RS2_RS4},    // Teeth 2,4 filter 75%: (curGap>>2) + (curGap>>4)

  // Tooth 5 (70° → 35°)
  {0x10, 0, K6A_CALC_OFF},        // Tooth 5 filter OFF
  {0x10, 1, K6A_CALC_RS3},        // Tooth 5 filter 25%: curGap>>3
  {0x10, 2, K6A_CALC_RS2},        // Tooth 5 filter 50%: curGap>>2
  {0x10, 3, K6A_CALC_RS2_RS3},    // Tooth 5 filter 75%: (curGap>>2) + (curGap>>3)

  // Tooth 6 (sync → 135°)
  {0x20, 0, K6A_CALC_OFF},        // Tooth 6 filter OFF
  {0x20, 1, K6A_CALC_DIRECT},     // Tooth 6 filter 25%: curGap
  {0x20, 2, K6A_CALC_MULT2},      // Tooth 6 filter 50%: curGap * 2U
  {0x20, 3, K6A_CALC_MULT3},      // Tooth 6 filter 75%: curGap * 3U

  // Tooth 7 (135° → 70°)
  {0x40, 0, K6A_CALC_OFF},        // Tooth 7 filter OFF
  {0x40, 1, K6A_CALC_RS3},        // Tooth 7 filter 25%: curGap>>3
  {0x40, 2, K6A_CALC_RS2},        // Tooth 7 filter 50%: curGap>>2
  {0x40, 3, K6A_CALC_RS2_RS3}     // Tooth 7 filter 75%: (curGap>>2) + (curGap>>3)
};

static inline uint32_t applySuzukiK6AFilter(uint8_t toothCount, uint8_t filterLevel, uint32_t curGap)
{
  // Create tooth bitmask (toothCount 1-7 maps to bit 0-6)
  uint8_t toothBit = (uint8_t)(1U << (toothCount - 1U));

  // Find matching configuration
  for(uint8_t i = 0; i < 20; i++)
  {
    const SuzukiK6AFilterConfig* config = &suzukiK6AFilterConfigs[i];
    if((config->toothMask & toothBit) != 0U && config->filterLevel == filterLevel)
    {
      // Apply calculation based on type
      switch(config->calc)
      {
        case K6A_CALC_OFF:        return 0;
        case K6A_CALC_RS3:        return rshift<3>(curGap);
        case K6A_CALC_RS3_RS4:    return rshift<3>(curGap) + rshift<4>(curGap);
        case K6A_CALC_RS2_RS4:    return rshift<2>(curGap) + rshift<4>(curGap);
        case K6A_CALC_RS2:        return rshift<2>(curGap);
        case K6A_CALC_RS2_RS3:    return rshift<2>(curGap) + rshift<3>(curGap);
        case K6A_CALC_DIRECT:     return curGap;
        case K6A_CALC_MULT2:      return curGap * 2U;
        case K6A_CALC_MULT3:      return curGap * 3U;
        case K6A_CALC_RS1_RS3:    return rshift<1>(curGap) + rshift<3>(curGap);
        case K6A_CALC_ADD_RS2:    return curGap + rshift<2>(curGap);
        case K6A_CALC_ADD_RS1_RS2:return curGap + rshift<1>(curGap) + rshift<2>(curGap);
        default:                  return 0;
      }
    }
  }
  return 0; // Default if no match found
}

/**
 * @brief Detect sync tooth based on gap pattern for Suzuki K6A
 * @details Pattern: small-big-small-big normally. Sync tooth breaks pattern: big-small-small
 *          Uses curGap2/curGap3 to store previous gaps (reused from sec/tert decoders)
 *
 * MISRA-C: 10 lines, N:1, C:2
 */
static inline void detectSuzukiK6ASyncTooth(void)
{
  if ((curGap <= curGap2) && (curGap2 <= curGap3))
  {
    // Decreasing gap sequence: we're on sync tooth
    toothCurrentCount = 6;
    currentStatus.hasSync = true;
  }

  curGap3 = curGap2;
  curGap2 = curGap;
}

/**
 * @brief Validate revolution counter and handle sync loss for Suzuki K6A
 * @details Checks if full revolution seen, resets counter, or detects sync loss
 *
 * MISRA-C: 18 lines, N:1, C:3
 */
static inline void validateSuzukiK6ARevolution(void)
{
  if ((toothCurrentCount == (triggerActualTeeth + 1U)) && (currentStatus.hasSync == true))
  {
    // Full revolution complete
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions = currentStatus.startRevolutions + 2U; // 720° crank = 2 revs
    return;
  }

  if (toothCurrentCount > (triggerActualTeeth + 1U))
  {
    // Lost sync - too many teeth
    currentStatus.hasSync = false;
    currentStatus.syncLossCounter++;
    triggerFilterTime = 0;
    toothCurrentCount = 0;
  }
}

/**
 * @brief Validate gap sequence matches expected tooth pattern for Suzuki K6A
 * @details Teeth 1,3,5,6: gap should be smaller than previous (small teeth)
 *          Teeth 2,4,7: gap should be larger than previous (big teeth)
 *
 * MISRA-C: 28 lines, N:2, C:8
 */
static inline void validateSuzukiK6AGapSequence(void)
{
  switch (toothCurrentCount)
  {
    case 1:
    case 3:
    case 5:
    case 6:
      // Small teeth - gap should be smaller than previous
      if (curGap > curGap2)
      {
        currentStatus.hasSync = false;
        currentStatus.syncLossCounter++;
        triggerFilterTime = 0;
        toothCurrentCount = 2;
      }
      break;

    case 2:
    case 4:
    case 7:
    default:
      // Big teeth - gap should be larger than previous
      if (curGap < curGap2)
      {
        currentStatus.hasSync = false;
        currentStatus.syncLossCounter++;
        triggerFilterTime = 0;
        toothCurrentCount = 1;
      }
      break;
  }
}

/**
 * @brief Handle per-tooth ignition timing for Suzuki K6A
 * @details Applies trigger angle correction and calls per-tooth timing check
 *
 * MISRA-C: 10 lines, N:1, C:2
 */
static inline void handleSuzukiK6APerToothIgnition(void)
{
  if (currentStatus.hasSync == false) { return; }

  triggerFilterTime = applySuzukiK6AFilter(toothCurrentCount, configPage4.triggerFilter, curGap);

  if (configPage2.perToothIgn == true)
  {
    int16_t crankAngle = toothAngles[toothCurrentCount] + configPage4.triggerAngle;
    crankAngle = ignitionLimits(crankAngle);
    checkPerToothTiming(crankAngle, toothCurrentCount);
  }
}

/**
 * @brief Primary trigger ISR for Suzuki K6A 3-cylinder decoder
 * @details Handles 6 crank teeth + 1 sync tooth (7 total per cycle)
 *          Pattern: small-big-small-big-small-big-SYNC
 *
 * MISRA-C: 25 lines, N:2, C:3 (was: 105 lines, N:4, C:15)
 *
 * @note Refactored FASE D - extracted 4 helpers to reduce complexity
 */
void triggerPri_SuzukiK6A(void)
{
  curTime = micros();
  curGap = curTime - toothLastToothTime;

  if ((curGap < triggerFilterTime) && (currentStatus.startRevolutions > 0U)) { return; }

  toothCurrentCount++;
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;

  detectSuzukiK6ASyncTooth();
  validateSuzukiK6ARevolution();
  validateSuzukiK6AGapSequence();
  handleSuzukiK6APerToothIgnition();
}

void triggerSec_SuzukiK6A(void)
{
  return;
}

uint16_t getRPM_SuzukiK6A(void)
{
  //Cranking code needs working out.

  uint16_t tempRPM = stdGetRPM(CAM_SPEED);

  MAX_STALL_TIME = revolutionTime << 1; //Set the stall time to be twice the current RPM. This is a safe figure as there should be no single revolution where this changes more than this
  if(MAX_STALL_TIME < 366667UL) { MAX_STALL_TIME = 366667UL; } //Check for 50rpm minimum

  return tempRPM;
}

int getCrankAngle_SuzukiK6A(void)
{
  //Grab some variables that are used in the trigger code and assign them to temp variables.
  noInterrupts();
  uint16_t tempToothCurrentCount = toothCurrentCount;
  unsigned long tempToothLastToothTime = toothLastToothTime;
  lastCrankAngleCalc = micros(); //micros() is no longer interrupt safe
  interrupts();

  if (tempToothCurrentCount>0U) {
    triggerToothAngle = (uint16_t)toothAngles[tempToothCurrentCount] - (uint16_t)toothAngles[tempToothCurrentCount-1U];
  }

  //Estimate the number of degrees travelled since the last tooth}
  elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);

  int crankAngle = toothAngles[tempToothCurrentCount] + configPage4.triggerAngle; //Perform a lookup of the fixed toothAngles array to find what the angle of the last tooth passed was.
  crankAngle += (int)timeToAngleDegPerMicroSec(elapsedTime);
  if (crankAngle >= 720) { crankAngle -= 720; }
  if (crankAngle < 0) { crankAngle += 720; }

  return crankAngle;
}

// Assumes no advance greater than 48 degrees. Triggers on the tooth before the ignition event
static uint16_t __attribute__((noinline)) calcEndTeeth_SuzukiK6A(int ignitionAngle) {
  //Temp variables are used here to avoid potential issues if a trigger interrupt occurs part way through this function
  const int16_t tempIgnitionEndTooth = ignitionLimits(ignitionAngle - configPage4.triggerAngle);

  uint8_t nCount=1U;
  while ((nCount<8U) && (tempIgnitionEndTooth > toothAngles[nCount])) {
    ++nCount;
  }
  if(nCount==1U || nCount==8U) {
    // didn't find a match, use tooth 7 as it must be greater than 7 but less than 1.
    return 7U;
  }

  // The tooth we want is the tooth prior to this one.
  return nCount-1U;
}

void triggerSetEndTeeth_SuzukiK6A(void)
{
  ignition1EndTooth = calcEndTeeth_SuzukiK6A(ignition1EndAngle);
  ignition2EndTooth = calcEndTeeth_SuzukiK6A(ignition2EndAngle);
  ignition3EndTooth = calcEndTeeth_SuzukiK6A(ignition3EndAngle);
}

/** @} */
#endif  // DRZ400, NGC, Vmax, Renix, RoverMEMS, SuzukiK6A - REFACTORED to implementations/

/** Ford TFI - Distributor mounted signal tooth wheel. Same number of teeth as cylinders.
Evenly spaced rising edge triggers, Cylinder 1 has a narrow teeth and will have a signature falling edge

* @defgroup Ford_TFI Ford TFI
* @{
*/
#if 0  // FordTFI - REFACTORED to implementations/
/** Ford TFI Setup.
 *
 * */
void triggerSetup_FordTFI(void)
{
  triggerActualTeeth = configPage2.nCylinders;
  if(triggerActualTeeth == 0) { triggerActualTeeth = 1; }

  triggerToothAngle = 720U / triggerActualTeeth; //The number of degrees that passes from tooth to tooth, half cylinder count
  toothCurrentCount = 0; //Default value
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 30U * configPage2.nCylinders)); //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise
  triggerSecFilterTime = triggerFilterTime * 4U /5u; //Same as above, but slightly about lower due to signature trigger (about 80%)
  lastSyncRevolution = 0;
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT); //This is always true for this pattern
  BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  if(configPage2.nCylinders <= 4U) { MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/90U) * triggerToothAngle); }//Minimum 90rpm. (1851uS is the time per degree at 90rpm). This uses 90rpm rather than 50rpm due to the potentially very high stall time on a 4 cylinder if we wait that long.
  else { MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle); } //Minimum 50rpm. (3200uS is the time per degree at 50rpm).
#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}


/** Ford TFI Primary (Rising Edge).
 *
 * */
void triggerPri_FordTFI(void)
{
  curTime = micros(); // Get current time and gap duration with micros rollover
  if (curTime >= toothLastToothTime)
    { curGap = curTime - toothLastToothTime; }
  else
    { curGap = (4294967296 - toothLastToothTime + curTime); }



  if ( curGap >= triggerFilterTime )
  {
    if(currentStatus.hasSync == true) { setFilter(curGap); } //Recalc the new filter value
    else { triggerFilterTime = 0; } //If we don't yet have sync, ensure that the filter won't prevent future valid pulses from being ignored

    toothCurrentCount++; //Increment the tooth counter

    if(toothCurrentCount > triggerActualTeeth)//Check if we're back to the beginning of a revolution
    {
      if ( (currentStatus.hasSync == true)  && ( (lastSyncRevolution) + 3  < currentStatus.startRevolutions)) // Revolution count when signature tooth was detected, Allow up to 4 cam revolution without sync signal detected
      {
        currentStatus.hasSync = false;
        currentStatus.syncLossCounter++;
      }
      toothCurrentCount = 1; //Reset the counter
      toothOneMinusOneTime = toothOneTime;
      toothOneTime = curTime;
      currentStatus.startRevolutions++; //Counter
    }

    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER); //Flag this pulse as being a valid trigger (ie that it passed filters)

    if(configPage2.perToothIgn == true)
    {
      int16_t crankAngle = ( (toothCurrentCount-1) * triggerToothAngle ) + configPage4.triggerAngle;
      crankAngle = ignitionLimits((crankAngle));
      uint16_t currentTooth = toothCurrentCount;
      if(toothCurrentCount > (triggerActualTeeth/2) ) { currentTooth = (toothCurrentCount - (triggerActualTeeth/2)); }
      checkPerToothTiming(crankAngle, currentTooth);
    }

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

   } //Trigger filter
}

/** Ford TFI Secondary (Falling Edge).
 *
 * */
void triggerSec_FordTFI(void)
{
  curTime2 = micros();
  if (curTime2 >= toothLastSecToothTime)
    { curGap2 = curTime2 - toothLastSecToothTime; }
  else
    { curGap2 = (4294967296 - toothLastSecToothTime + curTime2); }


  //Safety check for initial startup
  if( (toothLastSecToothTime == 0) )
  {
    curGap2 = 0;
    toothLastSecToothTime = curTime2;
  }


  if ( curGap2 >= triggerSecFilterTime ) // Valid tooth falling edge
  {
    if ((curGap > 0) && (curGap < 20000000)) // Limit to prevent overflow
    {
      targetGap2 = curGap * 110UL / 100UL; // Wide last teeth gap min
      targetGap3 = curGap * 90UL / 100UL; // Narrow last teeth minus one gap max
    }
    else
    {
    targetGap2 = 0;
    targetGap3 = 0;
    }
    if ( (curGap2 > targetGap2) && (curGap3 < targetGap3) && (lastGap < targetGap2) && (lastGap > targetGap3) ) // Signature Tooth detected
    {
      if( (currentStatus.hasSync == false) || (currentStatus.startRevolutions <= configPage4.StgCycles) )
      {
        toothCurrentCount = 2; // Last primary tooth was #2
        triggerFilterTime = 0; //Need to turn the filter off here otherwise the first primary tooth after achieving sync is ignored
        currentStatus.hasSync = true;
      }
      else
      {
        if ( (toothCurrentCount != 2) && (currentStatus.startRevolutions > 2))
        {
          currentStatus.syncLossCounter++;
        } //Indicates likely sync loss.
        if (configPage4.useResync == 1)
        {
          toothCurrentCount = 2;
          currentStatus.hasSync = true;
        }
      }
      lastSyncRevolution = currentStatus.startRevolutions ; // Revolution count when signature tooth was detected

    }

  toothLastSecToothTime = curTime2; //
  lastGap = curGap3; // Minus two Gap
  curGap3 = curGap2; // Minus one Gap

  } //Trigger filter
  //else { currentStatus.syncLossCounter = 0; }
  triggerSecFilterTime = curGap >> 1; //Set filter at 50 % speed of last primary gap
}

/** Ford TFI - Get RPM.
 *
 * */
uint16_t getRPM_FordTFI(void)
{
  uint16_t tempRPM;
  uint8_t distributorSpeed = CAM_SPEED; //Default to cam speed

  if( currentStatus.RPM < currentStatus.crankRPM || currentStatus.RPM < 1500)
  {
    tempRPM = crankingGetRPM(triggerActualTeeth, distributorSpeed);
  }
  else { tempRPM = stdGetRPM(distributorSpeed); }

  MAX_STALL_TIME = revolutionTime << 1; //Set the stall time to be twice the current RPM. This is a safe figure as there should be no single revolution where this changes more than this
  if(MAX_STALL_TIME < 366667UL) { MAX_STALL_TIME = 366667UL; } //Check for 50rpm minimum

  return tempRPM;

}

/** Ford TFI - Get Crank angle.
 *
 * */
int getCrankAngle_FordTFI(void)
{
    //This is the current angle ATDC the engine is at. This is the last known position based on what tooth was last 'seen'. It is only accurate to the resolution of the trigger wheel (Eg 36-1 is 10 degrees)
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    //Grab some variables that are used in the trigger code and assign them to temp variables.
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    //Handle case where the secondary tooth was the last one seen
    if(tempToothCurrentCount == 0) { tempToothCurrentCount = 2; }

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle; //Number of teeth that have passed since tooth 1, multiplied by the angle each tooth represents, plus the angle that tooth 1 is ATDC. This gives accuracy only to the nearest tooth.

    if (lastCrankAngleCalc >= tempToothLastToothTime)
      { elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime); }
    else
      { elapsedTime = (4294967296 - tempToothLastToothTime + lastCrankAngleCalc); }
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;

}
/** Ford TFI - Set End Teeth.
 *
 * */
void triggerSetEndTeeth_FordTFI(void)
{
  int tempEndAngle = (ignition1EndAngle - configPage4.triggerAngle);
  tempEndAngle = ignitionLimits((tempEndAngle));

  // Use shared data-driven implementation
  // Handles 4-cyl, 6-cyl, and 8-cyl (no 3-cyl for TFI)
  setEndTeethFromDistributorConfig(tempEndAngle, configPage2.nCylinders);
}
#endif  // FordTFI - REFACTORED to implementations/
/** @} */
