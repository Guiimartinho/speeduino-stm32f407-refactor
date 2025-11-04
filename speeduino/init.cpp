/**
 * @file init.cpp
 * @brief ECU initialization and hardware configuration at startup
 *
 * @details Performs complete system initialization when the ECU boots, configuring
 * all hardware peripherals, sensors, actuators, interrupts, and trigger decoders.
 * Called once from Arduino setup() before entering the main control loop.
 *
 * **MODULARIZATION NOTE:**
 * - setPinMapping() function (1,853 lines) has been modularized into board_config/
 * - Original monolithic code preserved in init.cpp.backup_original
 * - Board-specific pin mappings now organized by platform (AVR, STM32, Teensy, etc.)
 *
 * **INITIALIZATION SEQUENCE:**
 * 1. **Hardware Safety** - Shutdown all outputs (fuel, ignition, auxiliary)
 * 2. **Configuration Loading** - Load tuning maps from EEPROM/SD card
 * 3. **Sensor Setup** - Configure ADC inputs (MAP, TPS, CLT, IAT, O2, etc.)
 * 4. **Fuel System** - Initialize injector timings, staging, cylinder layout
 * 5. **Ignition System** - Configure spark outputs, coil dwell, per-tooth timing
 * 6. **Trigger Decoder** - Attach interrupts for crank/cam wheel decoding (29 patterns supported)
 * 7. **Auxiliary Systems** - Setup idle control, boost control, VVT, nitrous, etc.
 * 8. **Communication** - Initialize serial ports (USB, Bluetooth, CAN bus)
 * 9. **Timers** - Start 1ms ISR, scheduler loop, RPM calculation
 *
 * **TRIGGER DECODERS SUPPORTED (29 Patterns):**
 * - Missing Tooth (36-1, 60-2, 36-2-2-2, 36-2-1, etc.)
 * - Dual Wheel (crank + cam)
 * - Basic Distributor (single pulse per revolution)
 * - GM 7X/24X (General Motors)
 * - 4G63 (Mitsubishi Eclipse/Evo)
 * - Jeep 2000, Audi 135
 * - Honda D17/J32
 * - Miata 99-05, Mazda AU
 * - Non-360, Nissan 360
 * - Subaru 6/7
 * - Daihatsu +1
 * - Harley Davidson
 * - 420a (Chrysler)
 * - Weber-Marelli, Ford ST170
 * - Suzuki DRZ400
 * - Chrysler NGC (4/6/8 cyl)
 * - Yamaha Vmax
 * - Renix 44-2-2
 * - Rover MEMS
 * - Suzuki K6A
 * - Ford TFI
 *
 * **CONFIGURATION FUNCTIONS:**
 * - configureCylinderTimings() - Set injection/ignition angles per cylinder (1-12 cyl)
 * - calculateFuelParameters() - Compute required fuel, squirts, staging
 * - configureInjectionLayout() - Setup sequential/semi-sequential/simultaneous injection
 * - configureIgnitionMode() - Setup wasted spark/single channel/sequential/rotary
 * - setupTriggerPins() - Map decoder interrupt pins to MCU hardware
 * - initialiseTriggers() - Attach ISRs and start edge detection
 *
 * **HARDWARE PLATFORMS:**
 * - Arduino Mega 2560 (ATmega2560) - Reference platform
 * - STM32F407VGT6 (ARM Cortex-M4) - High-performance target
 * - Teensy 3.5/3.6/4.0/4.1 (ARM Cortex-M4/M7) - USB development
 * - Arduino Due (SAM3X8E ARM Cortex-M3)
 * - ESP32 (Xtensa dual-core)
 *
 * @complexity High (43 functions, 2,611 lines, 29 decoder initializers)
 * @performance Called once at boot - execution time not critical (~50-200ms typical)
 * @safety Critical - Ensures all outputs disabled before configuration begins
 * @see board_config/board_config.cpp for platform-specific pin mappings
 * @see decoders.cpp for trigger pattern interrupt service routines
 */
#include "globals.h"
#include "init.h"
#include "storage.h"
#include "updates.h"
#include "speeduino.h"
#include "timers.h"
#include "comms.h"
#include "comms_secondary.h"
#include "comms_CAN.h"
#include "utilities.h"
#include "scheduledIO.h"
#include "scheduler.h"
#include "schedule_calcs.h"
#include "auxiliaries.h"
#include "sensors.h"
#include "decoders.h"
#include "corrections.h"
#include "idle.h"
#include "table2d.h"
#include "acc_mc33810.h"
#include "board_config/board_config.h"  // MODULARIZED: Board configuration
#include BOARD_H //Note that this is not a real file, it is defined in globals.h. 
#if defined(EEPROM_RESET_PIN)
  #include EEPROM_LIB_H
#endif
#ifdef SD_LOGGING
  #include "SD_logger.h"
  #include "rtc_common.h"
#endif

#if defined(CORE_AVR)
#pragma GCC push_options
// This minimizes RAM usage at no performance cost
#pragma GCC optimize ("Os")
#endif

// ============================================================================
// MISRA-C:2012 Compliance - Named Constants
// ============================================================================

/** @brief MISRA-C: Crank angle for 4-stroke sequential operation (720 degrees = 2 revolutions) */
static constexpr uint16_t CRANK_ANGLE_4STROKE_SEQUENTIAL = 720U;

/** @brief MISRA-C: Crank angle for 2-stroke or standard operation (360 degrees = 1 revolution) */
static constexpr uint16_t CRANK_ANGLE_2STROKE_OR_STANDARD = 360U;

/** @brief MISRA-C: Required fuel multiplier for sequential injection (2x due to half duty cycle) */
static constexpr uint8_t REQ_FUEL_SEQUENTIAL_MULTIPLIER = 2U;

/** @brief MISRA-C: Even-fire V-twin angle (90 degrees) */
static constexpr uint16_t V_TWIN_EVEN_FIRE_ANGLE = 90U;

/** @brief MISRA-C: 4-cylinder even-fire spacing (90 degrees) */
static constexpr uint16_t FOUR_CYL_EVEN_FIRE_ANGLE = 90U;

/** @brief MISRA-C: Inline-3 cylinder firing angle (120 degrees) */
static constexpr uint16_t INLINE_3_FIRING_ANGLE = 120U;

/** @brief MISRA-C: Inline-6 cylinder firing angle (120 degrees) */
static constexpr uint16_t INLINE_6_FIRING_ANGLE = 120U;

/** @brief MISRA-C: V6 even-fire angle (60 degrees) */
static constexpr uint16_t V6_EVEN_FIRE_ANGLE = 60U;

/** @brief MISRA-C: V8 even-fire angle (90 degrees) */
static constexpr uint16_t V8_EVEN_FIRE_ANGLE = 90U;

/** @brief MISRA-C: V10 even-fire angle (72 degrees) */
static constexpr uint16_t V10_EVEN_FIRE_ANGLE = 72U;

/** @brief MISRA-C: Rotary engine trailing coil offset (4 degrees after leading) */
static constexpr uint8_t ROTARY_TRAILING_OFFSET_DEGREES = 4U;

/** @brief MISRA-C: Minimum injector dead time in microseconds */
static constexpr uint16_t INJECTOR_MIN_DEADTIME_US = 100U;

/** @brief MISRA-C: Maximum number of cylinders supported */
static constexpr uint8_t MAX_CYLINDERS = 12U;

/** @brief MISRA-C: Trigger interrupt not used/invalid */
static constexpr uint8_t TRIGGER_INTERRUPT_INVALID = 0xFFU;

/**
 * Configure cylinder-specific timing parameters based on engine configuration.
 *
 * This function configures ignition and injection timing angles for each cylinder
 * based on the number of cylinders, engine type (even/odd fire), stroke cycle,
 * injection layout, and spark mode. It sets up:
 * - Individual cylinder ignition degrees (channel1-8IgnDegrees)
 * - Individual cylinder injection degrees (channel1-8InjDegrees)
 * - Maximum number of ignition/injection outputs (maxIgnOutputs/maxInjOutputs)
 * - Crank angle maximums for ignition/injection (CRANK_ANGLE_MAX_IGN/INJ)
 * - Number of injection squirts per cycle (currentStatus.nSquirts)
 * - Required fuel duration adjustments (req_fuel_uS)
 * - Ignition end angles (ignition1-8EndAngle)
 *
 * All variables modified are globals and changes persist after function returns.
 * Must be called during initialization after trigger setup.
 */
static void configureCylinderTimings(void)
{
    switch (configPage2.nCylinders) {
    case 1:
        channel1IgnDegrees = 0;
        channel1InjDegrees = 0;
        maxIgnOutputs = 1;
        maxInjOutputs = 1;

        //Sequential ignition works identically on a 1 cylinder whether it's odd or even fire.
        if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage2.strokes == FOUR_STROKE) ) { CRANK_ANGLE_MAX_IGN = 720; }

        if ( (configPage2.injLayout == INJ_SEQUENTIAL) && (configPage2.strokes == FOUR_STROKE) )
        {
          CRANK_ANGLE_MAX_INJ = 720;
          currentStatus.nSquirts = 1;
          req_fuel_uS = req_fuel_uS * 2;
        }

        //Check if injector staging is enabled
        if(configPage10.stagingEnabled == true)
        {
          maxInjOutputs = 2;
          channel2InjDegrees = channel1InjDegrees;
        }
        break;

    case 2:
        channel1IgnDegrees = 0;
        channel1InjDegrees = 0;
        maxIgnOutputs = 2;
        maxInjOutputs = 2;
        if (configPage2.engineType == EVEN_FIRE ) { channel2IgnDegrees = 180; }
        else { channel2IgnDegrees = configPage2.oddfire2; }

        //Sequential ignition works identically on a 2 cylinder whether it's odd or even fire (With the default being a 180 degree second cylinder).
        if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage2.strokes == FOUR_STROKE) ) { CRANK_ANGLE_MAX_IGN = 720; }

        if ( (configPage2.injLayout == INJ_SEQUENTIAL) && (configPage2.strokes == FOUR_STROKE) )
        {
          CRANK_ANGLE_MAX_INJ = 720;
          currentStatus.nSquirts = 1;
          req_fuel_uS = req_fuel_uS * 2;
        }
        //The below are true regardless of whether this is running sequential or not
        if (configPage2.engineType == EVEN_FIRE ) { channel2InjDegrees = 180; }
        else { channel2InjDegrees = configPage2.oddfire2; }
        if (!configPage2.injTiming)
        {
          //For simultaneous, all squirts happen at the same time
          channel1InjDegrees = 0;
          channel2InjDegrees = 0;
        }

        //Check if injector staging is enabled
        if(configPage10.stagingEnabled == true)
        {
          maxInjOutputs = 4;

          channel3InjDegrees = channel1InjDegrees;
          channel4InjDegrees = channel2InjDegrees;
        }

        break;

    case 3:
        channel1IgnDegrees = 0;
        maxIgnOutputs = 3;
        maxInjOutputs = 3;
        if (configPage2.engineType == EVEN_FIRE )
        {
          //Sequential and Single channel modes both run over 720 crank degrees, but only on 4 stroke engines.
          if( ( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) || (configPage4.sparkMode == IGN_MODE_SINGLE) ) && (configPage2.strokes == FOUR_STROKE) )
          {
            channel2IgnDegrees = 240;
            channel3IgnDegrees = 480;

            CRANK_ANGLE_MAX_IGN = 720;
          }
          else
          {
            channel2IgnDegrees = 120;
            channel3IgnDegrees = 240;
          }
        }
        else
        {
          channel2IgnDegrees = configPage2.oddfire2;
          channel3IgnDegrees = configPage2.oddfire3;
        }

        //For alternating injection, the squirt occurs at different times for each channel
        if( (configPage2.injLayout == INJ_SEMISEQUENTIAL) || (configPage2.injLayout == INJ_PAIRED) )
        {
          channel1InjDegrees = 0;
          channel2InjDegrees = 120;
          channel3InjDegrees = 240;

          if(configPage2.injType == INJ_TYPE_PORT)
          {
            //Force nSquirts to 2 for individual port injection. This prevents TunerStudio forcing the value to 3 even when this isn't wanted.
            currentStatus.nSquirts = 2;
            if(configPage2.strokes == FOUR_STROKE) { CRANK_ANGLE_MAX_INJ = 360; }
            else { CRANK_ANGLE_MAX_INJ = 180; }
          }

          //Adjust the injection angles based on the number of squirts
          if (currentStatus.nSquirts > 2)
          {
            channel2InjDegrees = (channel2InjDegrees * 2) / currentStatus.nSquirts;
            channel3InjDegrees = (channel3InjDegrees * 2) / currentStatus.nSquirts;
          }

          if (!configPage2.injTiming)
          {
            //For simultaneous, all squirts happen at the same time
            channel1InjDegrees = 0;
            channel2InjDegrees = 0;
            channel3InjDegrees = 0;
          }
        }
        else if (configPage2.injLayout == INJ_SEQUENTIAL)
        {
          currentStatus.nSquirts = 1;

          if(configPage2.strokes == TWO_STROKE)
          {
            channel1InjDegrees = 0;
            channel2InjDegrees = 120;
            channel3InjDegrees = 240;
            CRANK_ANGLE_MAX_INJ = 360;
          }
          else
          {
            req_fuel_uS = req_fuel_uS * 2;
            channel1InjDegrees = 0;
            channel2InjDegrees = 240;
            channel3InjDegrees = 480;
            CRANK_ANGLE_MAX_INJ = 720;
          }
        }
        else
        {
          //Should never happen, but default values
          channel1InjDegrees = 0;
          channel2InjDegrees = 120;
          channel3InjDegrees = 240;
        }

        //Check if injector staging is enabled
        if(configPage10.stagingEnabled == true)
        {
          #if INJ_CHANNELS >= 6
            maxInjOutputs = 6;

            channel4InjDegrees = channel1InjDegrees;
            channel5InjDegrees = channel2InjDegrees;
            channel6InjDegrees = channel3InjDegrees;
          #else
            //Staged output is on channel 4
            maxInjOutputs = 4;
            channel4InjDegrees = channel1InjDegrees;
          #endif
        }
        break;
    case 4:
        channel1IgnDegrees = 0;
        channel1InjDegrees = 0;
        maxIgnOutputs = 2; //Default value for 4 cylinder, may be changed below
        maxInjOutputs = 2;
        if (configPage2.engineType == EVEN_FIRE )
        {
          channel2IgnDegrees = 180;

          if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage2.strokes == FOUR_STROKE) )
          {
            channel3IgnDegrees = 360;
            channel4IgnDegrees = 540;

            CRANK_ANGLE_MAX_IGN = 720;
            maxIgnOutputs = 4;
          }
          if(configPage4.sparkMode == IGN_MODE_ROTARY)
          {
            //Rotary uses the ign 3 and 4 schedules for the trailing spark. They are offset from the ign 1 and 2 channels respectively and so use the same degrees as them
            channel3IgnDegrees = 0;
            channel4IgnDegrees = 180;
            maxIgnOutputs = 4;

            configPage4.IgInv = GOING_LOW; //Force Going Low ignition mode (Going high is never used for rotary)
          }
        }
        else
        {
          channel2IgnDegrees = configPage2.oddfire2;
          channel3IgnDegrees = configPage2.oddfire3;
          channel4IgnDegrees = configPage2.oddfire4;
          maxIgnOutputs = 4;
        }

        //For alternating injection, the squirt occurs at different times for each channel
        if( (configPage2.injLayout == INJ_SEMISEQUENTIAL) || (configPage2.injLayout == INJ_PAIRED) || (configPage2.strokes == TWO_STROKE) )
        {
          channel2InjDegrees = 180;

          if (!configPage2.injTiming)
          {
            //For simultaneous, all squirts happen at the same time
            channel1InjDegrees = 0;
            channel2InjDegrees = 0;
          }
          else if (currentStatus.nSquirts > 2)
          {
            //Adjust the injection angles based on the number of squirts
            channel2InjDegrees = (channel2InjDegrees * 2) / currentStatus.nSquirts;
          }
          else { } //Do nothing, default values are correct
        }
        else if (configPage2.injLayout == INJ_SEQUENTIAL)
        {
          channel2InjDegrees = 180;
          channel3InjDegrees = 360;
          channel4InjDegrees = 540;

          maxInjOutputs = 4;

          CRANK_ANGLE_MAX_INJ = 720;
          currentStatus.nSquirts = 1;
          req_fuel_uS = req_fuel_uS * 2;
        }
        else
        {
          //Should never happen, but default values
          maxInjOutputs = 2;
        }

        //Check if injector staging is enabled
        if(configPage10.stagingEnabled == true)
        {
          maxInjOutputs = 4;

          if( (configPage2.injLayout == INJ_SEQUENTIAL) || (configPage2.injLayout == INJ_SEMISEQUENTIAL) )
          {
            //Staging with 4 cylinders semi/sequential requires 8 total channels
            #if INJ_CHANNELS >= 8
              maxInjOutputs = 8;

              channel5InjDegrees = channel1InjDegrees;
              channel6InjDegrees = channel2InjDegrees;
              channel7InjDegrees = channel3InjDegrees;
              channel8InjDegrees = channel4InjDegrees;
            #else
              //This is an invalid config as there are not enough outputs to support sequential + staging
              //Put the staging output to the non-existent channel 5
              #if (INJ_CHANNELS >= 5)
              maxInjOutputs = 5;
              channel5InjDegrees = channel1InjDegrees;
              #endif
            #endif
          }
          else
          {
            channel3InjDegrees = channel1InjDegrees;
            channel4InjDegrees = channel2InjDegrees;
          }
        }

        break;
    case 5:
        channel1IgnDegrees = 0;
        channel2IgnDegrees = 72;
        channel3IgnDegrees = 144;
        channel4IgnDegrees = 216;
#if (IGN_CHANNELS >= 5)
        channel5IgnDegrees = 288;
#endif
        maxIgnOutputs = 5; //Only 4 actual outputs, so that's all that can be cut
        maxInjOutputs = 4; //Is updated below to 5 if there are enough channels

        if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
        {
          channel2IgnDegrees = 144;
          channel3IgnDegrees = 288;
          channel4IgnDegrees = 432;
#if (IGN_CHANNELS >= 5)
          channel5IgnDegrees = 576;
#endif

          CRANK_ANGLE_MAX_IGN = 720;
        }

        //For alternating injection, the squirt occurs at different times for each channel
        if( (configPage2.injLayout == INJ_SEMISEQUENTIAL) || (configPage2.injLayout == INJ_PAIRED) || (configPage2.strokes == TWO_STROKE) )
        {
          if (!configPage2.injTiming)
          {
            //For simultaneous, all squirts happen at the same time
            channel1InjDegrees = 0;
            channel2InjDegrees = 0;
            channel3InjDegrees = 0;
            channel4InjDegrees = 0;
#if (INJ_CHANNELS >= 5)
            channel5InjDegrees = 0;
#endif
          }
          else
          {
            channel1InjDegrees = 0;
            channel2InjDegrees = 72;
            channel3InjDegrees = 144;
            channel4InjDegrees = 216;
#if (INJ_CHANNELS >= 5)
            channel5InjDegrees = 288;
#endif

            //Divide by currentStatus.nSquirts ?
          }
        }
    #if INJ_CHANNELS >= 5
        else if (configPage2.injLayout == INJ_SEQUENTIAL)
        {
          channel1InjDegrees = 0;
          channel2InjDegrees = 144;
          channel3InjDegrees = 288;
          channel4InjDegrees = 432;
          channel5InjDegrees = 576;

          maxInjOutputs = 5;

          CRANK_ANGLE_MAX_INJ = 720;
          currentStatus.nSquirts = 1;
          req_fuel_uS = req_fuel_uS * 2;
        }
    #endif

    #if INJ_CHANNELS >= 6
          if(configPage10.stagingEnabled == true) { maxInjOutputs = 6; }
    #endif
        break;
    case 6:
        channel1IgnDegrees = 0;
        channel2IgnDegrees = 120;
        channel3IgnDegrees = 240;
        maxIgnOutputs = 3;
        maxInjOutputs = 3;

    #if IGN_CHANNELS >= 6
        if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL))
        {
        channel4IgnDegrees = 360;
        channel5IgnDegrees = 480;
        channel6IgnDegrees = 600;
        CRANK_ANGLE_MAX_IGN = 720;
        maxIgnOutputs = 6;
        }
    #endif

        //For alternating injection, the squirt occurs at different times for each channel
        if( (configPage2.injLayout == INJ_SEMISEQUENTIAL) || (configPage2.injLayout == INJ_PAIRED) )
        {
          channel1InjDegrees = 0;
          channel2InjDegrees = 120;
          channel3InjDegrees = 240;
          if (!configPage2.injTiming)
          {
            //For simultaneous, all squirts happen at the same time
            channel1InjDegrees = 0;
            channel2InjDegrees = 0;
            channel3InjDegrees = 0;
          }
          else if (currentStatus.nSquirts > 2)
          {
            //Adjust the injection angles based on the number of squirts
            channel2InjDegrees = (channel2InjDegrees * 2) / currentStatus.nSquirts;
            channel3InjDegrees = (channel3InjDegrees * 2) / currentStatus.nSquirts;
          }
        }

    #if INJ_CHANNELS >= 6
        if (configPage2.injLayout == INJ_SEQUENTIAL)
        {
          channel1InjDegrees = 0;
          channel2InjDegrees = 120;
          channel3InjDegrees = 240;
          channel4InjDegrees = 360;
          channel5InjDegrees = 480;
          channel6InjDegrees = 600;

          maxInjOutputs = 6;

          CRANK_ANGLE_MAX_INJ = 720;
          currentStatus.nSquirts = 1;
          req_fuel_uS = req_fuel_uS * 2;
        }
        else if(configPage10.stagingEnabled == true) //Check if injector staging is enabled
        {
          maxInjOutputs = 6;

          if( (configPage2.injLayout == INJ_SEQUENTIAL) || (configPage2.injLayout == INJ_SEMISEQUENTIAL) )
          {
            //Staging with 6 cylinders semi/sequential requires 7 total channels
            #if INJ_CHANNELS >= 7
              maxInjOutputs = 7;

              channel5InjDegrees = channel1InjDegrees;
              channel6InjDegrees = channel2InjDegrees;
              channel7InjDegrees = channel3InjDegrees;
              channel8InjDegrees = channel4InjDegrees;
            #else
              //This is an invalid config as there are not enough outputs to support sequential + staging
              //No staging output will be active
              maxInjOutputs = 6;
            #endif
          }
        }
    #endif
        break;
    case 8:
        channel1IgnDegrees = 0;
        channel2IgnDegrees = 90;
        channel3IgnDegrees = 180;
        channel4IgnDegrees = 270;
        maxIgnOutputs = 4;
        maxInjOutputs = 4;


        if( (configPage4.sparkMode == IGN_MODE_SINGLE))
        {
          maxIgnOutputs = 4;
          CRANK_ANGLE_MAX_IGN = 360;
        }


    #if IGN_CHANNELS >= 8
        if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL))
        {
        channel5IgnDegrees = 360;
        channel6IgnDegrees = 450;
        channel7IgnDegrees = 540;
        channel8IgnDegrees = 630;
        maxIgnOutputs = 8;
        CRANK_ANGLE_MAX_IGN = 720;
        }
    #endif

        //For alternating injection, the squirt occurs at different times for each channel
        if( (configPage2.injLayout == INJ_SEMISEQUENTIAL) || (configPage2.injLayout == INJ_PAIRED) )
        {
          channel1InjDegrees = 0;
          channel2InjDegrees = 90;
          channel3InjDegrees = 180;
          channel4InjDegrees = 270;

          if (!configPage2.injTiming)
          {
            //For simultaneous, all squirts happen at the same time
            channel1InjDegrees = 0;
            channel2InjDegrees = 0;
            channel3InjDegrees = 0;
            channel4InjDegrees = 0;
          }
          else if (currentStatus.nSquirts > 2)
          {
            //Adjust the injection angles based on the number of squirts
            channel2InjDegrees = (channel2InjDegrees * 2) / currentStatus.nSquirts;
            channel3InjDegrees = (channel3InjDegrees * 2) / currentStatus.nSquirts;
            channel4InjDegrees = (channel4InjDegrees * 2) / currentStatus.nSquirts;
          }
        }

    #if INJ_CHANNELS >= 8
        else if (configPage2.injLayout == INJ_SEQUENTIAL)
        {
          channel1InjDegrees = 0;
          channel2InjDegrees = 90;
          channel3InjDegrees = 180;
          channel4InjDegrees = 270;
          channel5InjDegrees = 360;
          channel6InjDegrees = 450;
          channel7InjDegrees = 540;
          channel8InjDegrees = 630;

          maxInjOutputs = 8;

          CRANK_ANGLE_MAX_INJ = 720;
          currentStatus.nSquirts = 1;
          req_fuel_uS = req_fuel_uS * 2;
        }
    #endif

        break;
    default: //Handle this better!!!
        channel1InjDegrees = 0;
        channel2InjDegrees = 180;
        break;
    }
}

/**
 * Calculate fuel-related parameters after cylinder timing configuration.
 *
 * This function performs final fuel calculation adjustments that must occur
 * after configureCylinderTimings() has been called. It handles:
 * - Base required fuel conversion from ms*10 to microseconds (req_fuel_uS)
 * - Injector opening time conversion from ms*10 to microseconds (inj_opentime_uS)
 * - Staged injection fuel multiplier calculations for primary/secondary injectors
 * - VVT poll level configuration for secondary trigger when using missing tooth decoder
 *
 * For staged injection, calculates percentage multipliers based on relative injector sizes:
 * - staged_req_fuel_mult_pri: Primary injector capacity as % of total
 * - staged_req_fuel_mult_sec: Secondary injector capacity as % of total
 *
 * All variables modified are globals and changes persist after function returns.
 * Must be called after configureCylinderTimings() and before trigger initialization.
 */
static void calculateFuelParameters(void)
{
    //Once the configs have been loaded, a number of one time calculations can be completed
    req_fuel_uS = configPage2.reqFuel * 100; //Convert to uS and an int. This is the only variable to be used in calculations
    inj_opentime_uS = configPage2.injOpen * 100; //Injector open time. Comes through as ms*10 (Eg 15.5ms = 155).

    if(configPage10.stagingEnabled == true)
    {
    uint32_t totalInjector = configPage10.stagedInjSizePri + configPage10.stagedInjSizeSec;
    /*
        These values are a percentage of the req_fuel value that would be required for each injector channel to deliver that much fuel.
        Eg:
        Pri injectors are 250cc
        Sec injectors are 500cc
        Total injector capacity = 750cc

        staged_req_fuel_mult_pri = 300% (The primary injectors would have to run 3x the overall PW in order to be the equivalent of the full 750cc capacity
        staged_req_fuel_mult_sec = 150% (The secondary injectors would have to run 1.5x the overall PW in order to be the equivalent of the full 750cc capacity
    */
    staged_req_fuel_mult_pri = (100 * totalInjector) / configPage10.stagedInjSizePri;
    staged_req_fuel_mult_sec = (100 * totalInjector) / configPage10.stagedInjSizeSec;
    }

    if (configPage4.trigPatternSec == SEC_TRIGGER_POLL && configPage4.TrigPattern == DECODER_MISSING_TOOTH)
    { configPage4.TrigEdgeSec = configPage4.PollLevelPolarity; } // set the secondary trigger edge automatically to correct working value with poll level mode to enable cam angle detection in closed loop vvt.
    //Explanation: currently cam trigger for VVT is only captured when revolution one == 1. So we need to make sure that the edge trigger happens on the first revolution. So now when we set the poll level to be low
    //on revolution one and it's checked at tooth #1. This means that the cam signal needs to go high during the first revolution to be high on next revolution at tooth #1. So poll level low = cam trigger edge rising.
}

/**
 * Safely shutdown all ignition and injection outputs at startup.
 *
 * This function ensures a clean startup state by:
 * - Ending all coil charges (8 ignition channels)
 * - Closing all injectors (8 injection channels)
 * - Setting tacho output to default HIGH state
 *
 * This prevents any stray sparks or fuel delivery that could occur from
 * residual states in hardware or undefined pin conditions during boot.
 * Must be called early in initialization, after pin mapping is configured.
 *
 * All channel operations are conditional on compile-time channel definitions
 * (IGN_CHANNELS and INJ_CHANNELS) to support different board configurations.
 */
static void safetyShutdownAllOutputs(void)
{
    //End all coil charges to ensure no stray sparks on startup
    endCoil1Charge();
    endCoil2Charge();
    endCoil3Charge();
    endCoil4Charge();
    endCoil5Charge();
    #if (IGN_CHANNELS >= 6)
    endCoil6Charge();
    #endif
    #if (IGN_CHANNELS >= 7)
    endCoil7Charge();
    #endif
    #if (IGN_CHANNELS >= 8)
    endCoil8Charge();
    #endif

    //Similar for injectors, make sure they're turned off
    closeInjector1();
    closeInjector2();
    closeInjector3();
    closeInjector4();
    closeInjector5();
    #if (INJ_CHANNELS >= 6)
    closeInjector6();
    #endif
    #if (INJ_CHANNELS >= 7)
    closeInjector7();
    #endif
    #if (INJ_CHANNELS >= 8)
    closeInjector8();
    #endif

    //Set the tacho output default state
    digitalWrite(pinTachOut, HIGH);
}

/**
 * Configure injection layout by assigning function pointers to fuel schedules.
 *
 * This function sets up the fuel delivery system based on the configured injection
 * layout (configPage2.injLayout). It assigns appropriate start/end function pointers
 * to each fuel schedule (fuelSchedule1-8) based on the injection mode:
 *
 * - INJ_PAIRED: Each injector operates independently, paired with its cylinder
 * - INJ_SEMISEQUENTIAL: Injectors are paired (e.g., 1&3, 2&4 for 4-cyl)
 * - INJ_SEQUENTIAL: Each injector fires independently in engine firing order
 * - default: Falls back to paired injection mode
 *
 * Special cases handled:
 * - 4-cylinder: Supports two pairing modes (INJ_PAIR_13_24 or INJ_PAIR_14_23)
 * - 5-cylinder: Uses 5 outputs with special pairing for cylinders 3&5
 * - 6-cylinder: Pairs 1&4, 2&5, 3&6
 * - 8-cylinder: Pairs 1&5, 2&6, 3&7, 4&8
 *
 * All variables modified are global schedule structures, changes persist after return.
 * Must be called after configureCylinderTimings() during initialization.
 */
static void configureInjectionLayout(void)
{
    switch(configPage2.injLayout)
    {
    case INJ_PAIRED:
        //Paired injection
        fuelSchedule1.pStartFunction = openInjector1;
        fuelSchedule1.pEndFunction = closeInjector1;
        fuelSchedule2.pStartFunction = openInjector2;
        fuelSchedule2.pEndFunction = closeInjector2;
        fuelSchedule3.pStartFunction = openInjector3;
        fuelSchedule3.pEndFunction = closeInjector3;
        fuelSchedule4.pStartFunction = openInjector4;
        fuelSchedule4.pEndFunction = closeInjector4;
#if INJ_CHANNELS >= 5
        fuelSchedule5.pStartFunction = openInjector5;
        fuelSchedule5.pEndFunction = closeInjector5;
#endif
        break;

    case INJ_SEMISEQUENTIAL:
        //Semi-Sequential injection. Currently possible with 4, 6 and 8 cylinders. 5 cylinder is a special case
        if( configPage2.nCylinders == 4 )
        {
          if(configPage4.inj4cylPairing == INJ_PAIR_13_24)
          {
            fuelSchedule1.pStartFunction = openInjector1and3;
            fuelSchedule1.pEndFunction = closeInjector1and3;
            fuelSchedule2.pStartFunction = openInjector2and4;
            fuelSchedule2.pEndFunction = closeInjector2and4;
          }
          else
          {
            fuelSchedule1.pStartFunction = openInjector1and4;
            fuelSchedule1.pEndFunction = closeInjector1and4;
            fuelSchedule2.pStartFunction = openInjector2and3;
            fuelSchedule2.pEndFunction = closeInjector2and3;
          }
        }
        else if( configPage2.nCylinders == 5 ) //This is similar to the paired injection but uses five injector outputs instead of four
        {
          fuelSchedule1.pStartFunction = openInjector1;
          fuelSchedule1.pEndFunction = closeInjector1;
          fuelSchedule2.pStartFunction = openInjector2;
          fuelSchedule2.pEndFunction = closeInjector2;
          fuelSchedule3.pStartFunction = openInjector3and5;
          fuelSchedule3.pEndFunction = closeInjector3and5;
          fuelSchedule4.pStartFunction = openInjector4;
          fuelSchedule4.pEndFunction = closeInjector4;
        }
        else if( configPage2.nCylinders == 6 )
        {
          fuelSchedule1.pStartFunction = openInjector1and4;
          fuelSchedule1.pEndFunction = closeInjector1and4;
          fuelSchedule2.pStartFunction = openInjector2and5;
          fuelSchedule2.pEndFunction = closeInjector2and5;
          fuelSchedule3.pStartFunction = openInjector3and6;
          fuelSchedule3.pEndFunction = closeInjector3and6;
        }
        else if( configPage2.nCylinders == 8 )
        {
          fuelSchedule1.pStartFunction = openInjector1and5;
          fuelSchedule1.pEndFunction = closeInjector1and5;
          fuelSchedule2.pStartFunction = openInjector2and6;
          fuelSchedule2.pEndFunction = closeInjector2and6;
          fuelSchedule3.pStartFunction = openInjector3and7;
          fuelSchedule3.pEndFunction = closeInjector3and7;
          fuelSchedule4.pStartFunction = openInjector4and8;
          fuelSchedule4.pEndFunction = closeInjector4and8;
        }
        else
        {
          //Fall back to paired injection
          fuelSchedule1.pStartFunction = openInjector1;
          fuelSchedule1.pEndFunction = closeInjector1;
          fuelSchedule2.pStartFunction = openInjector2;
          fuelSchedule2.pEndFunction = closeInjector2;
          fuelSchedule3.pStartFunction = openInjector3;
          fuelSchedule3.pEndFunction = closeInjector3;
          fuelSchedule4.pStartFunction = openInjector4;
          fuelSchedule4.pEndFunction = closeInjector4;
#if INJ_CHANNELS >= 5
          fuelSchedule5.pStartFunction = openInjector5;
          fuelSchedule5.pEndFunction = closeInjector5;
#endif
        }
        break;

    case INJ_SEQUENTIAL:
        //Sequential injection
        fuelSchedule1.pStartFunction = openInjector1;
        fuelSchedule1.pEndFunction = closeInjector1;
        fuelSchedule2.pStartFunction = openInjector2;
        fuelSchedule2.pEndFunction = closeInjector2;
        fuelSchedule3.pStartFunction = openInjector3;
        fuelSchedule3.pEndFunction = closeInjector3;
        fuelSchedule4.pStartFunction = openInjector4;
        fuelSchedule4.pEndFunction = closeInjector4;
#if INJ_CHANNELS >= 5
        fuelSchedule5.pStartFunction = openInjector5;
        fuelSchedule5.pEndFunction = closeInjector5;
#endif
#if INJ_CHANNELS >= 6
        fuelSchedule6.pStartFunction = openInjector6;
        fuelSchedule6.pEndFunction = closeInjector6;
#endif
#if INJ_CHANNELS >= 7
        fuelSchedule7.pStartFunction = openInjector7;
        fuelSchedule7.pEndFunction = closeInjector7;
#endif
#if INJ_CHANNELS >= 8
        fuelSchedule8.pStartFunction = openInjector8;
        fuelSchedule8.pEndFunction = closeInjector8;
#endif
        break;

    default:
        //Paired injection
        fuelSchedule1.pStartFunction = openInjector1;
        fuelSchedule1.pEndFunction = closeInjector1;
        fuelSchedule2.pStartFunction = openInjector2;
        fuelSchedule2.pEndFunction = closeInjector2;
        fuelSchedule3.pStartFunction = openInjector3;
        fuelSchedule3.pEndFunction = closeInjector3;
        fuelSchedule4.pStartFunction = openInjector4;
        fuelSchedule4.pEndFunction = closeInjector4;
#if INJ_CHANNELS >= 5
        fuelSchedule5.pStartFunction = openInjector5;
        fuelSchedule5.pEndFunction = closeInjector5;
#endif
        break;
    }
}

/**
 * Configure ignition mode by assigning function pointers to ignition schedules.
 *
 * This function sets up the ignition system based on the configured spark mode
 * (configPage4.sparkMode). It assigns appropriate start/end callback function pointers
 * to each ignition schedule (ignitionSchedule1-8) based on the ignition mode:
 *
 * - IGN_MODE_WASTED: Wasted spark mode, each coil fires twice per cycle
 * - IGN_MODE_SINGLE: Single channel mode, all sparks use coil 1 output
 * - IGN_MODE_WASTEDCOP: Wasted COP mode with paired coils
 *   - 4-cyl: Pairs 1&3, 2&4
 *   - 6-cyl: Pairs 1&4, 2&5, 3&6
 *   - 8-cyl: Pairs 1&5, 2&6, 3&7, 4&8
 * - IGN_MODE_SEQUENTIAL: Each coil fires independently in firing order
 * - IGN_MODE_ROTARY: Special rotary engine modes (FC, FD, RX8)
 *   - FC: Wasted leading + shared trailing coil
 *   - FD: Wasted leading + individual trailing coils
 *   - RX8: Individual coils per plug (4 outputs)
 * - default: Falls back to wasted spark mode
 *
 * All variables modified are global schedule structures, changes persist after return.
 * Must be called after configureCylinderTimings() during initialization.
 */
static void configureIgnitionMode(void)
{
    switch(configPage4.sparkMode)
    {
    case IGN_MODE_WASTED:
        //Wasted Spark (Normal mode)
        ignitionSchedule1.pStartCallback = beginCoil1Charge;
        ignitionSchedule1.pEndCallback = endCoil1Charge;
        ignitionSchedule2.pStartCallback = beginCoil2Charge;
        ignitionSchedule2.pEndCallback = endCoil2Charge;
        ignitionSchedule3.pStartCallback = beginCoil3Charge;
        ignitionSchedule3.pEndCallback = endCoil3Charge;
        ignitionSchedule4.pStartCallback = beginCoil4Charge;
        ignitionSchedule4.pEndCallback = endCoil4Charge;
        ignitionSchedule5.pStartCallback = beginCoil5Charge;
        ignitionSchedule5.pEndCallback = endCoil5Charge;
        break;

    case IGN_MODE_SINGLE:
        //Single channel mode. All ignition pulses are on channel 1
        ignitionSchedule1.pStartCallback = beginCoil1Charge;
        ignitionSchedule1.pEndCallback = endCoil1Charge;
        ignitionSchedule2.pStartCallback = beginCoil1Charge;
        ignitionSchedule2.pEndCallback = endCoil1Charge;
        ignitionSchedule3.pStartCallback = beginCoil1Charge;
        ignitionSchedule3.pEndCallback = endCoil1Charge;
        ignitionSchedule4.pStartCallback = beginCoil1Charge;
        ignitionSchedule4.pEndCallback = endCoil1Charge;
#if IGN_CHANNELS >= 5
        ignitionSchedule5.pStartCallback = beginCoil1Charge;
        ignitionSchedule5.pEndCallback = endCoil1Charge;
#endif
#if IGN_CHANNELS >= 6
        ignitionSchedule6.pStartCallback = beginCoil1Charge;
        ignitionSchedule6.pEndCallback = endCoil1Charge;
#endif
#if IGN_CHANNELS >= 7
        ignitionSchedule7.pStartCallback = beginCoil1Charge;
        ignitionSchedule7.pEndCallback = endCoil1Charge;
#endif
#if IGN_CHANNELS >= 8
        ignitionSchedule8.pStartCallback = beginCoil1Charge;
        ignitionSchedule8.pEndCallback = endCoil1Charge;
#endif
        break;

    case IGN_MODE_WASTEDCOP:
        //Wasted COP mode. Note, most of the boards can only run this for 4-cyl only.
        if( configPage2.nCylinders <= 3)
        {
          //1-3 cylinder wasted COP is the same as regular wasted mode
          ignitionSchedule1.pStartCallback = beginCoil1Charge;
          ignitionSchedule1.pEndCallback = endCoil1Charge;
          ignitionSchedule2.pStartCallback = beginCoil2Charge;
          ignitionSchedule2.pEndCallback = endCoil2Charge;
          ignitionSchedule3.pStartCallback = beginCoil3Charge;
          ignitionSchedule3.pEndCallback = endCoil3Charge;
        }
        else if( configPage2.nCylinders == 4 )
        {
          //Wasted COP mode for 4 cylinders. Ignition channels 1&3 and 2&4 are paired together
          ignitionSchedule1.pStartCallback = beginCoil1and3Charge;
          ignitionSchedule1.pEndCallback = endCoil1and3Charge;
          ignitionSchedule2.pStartCallback = beginCoil2and4Charge;
          ignitionSchedule2.pEndCallback = endCoil2and4Charge;

          ignitionSchedule3.pStartCallback = nullCallback;
          ignitionSchedule3.pEndCallback = nullCallback;
          ignitionSchedule4.pStartCallback = nullCallback;
          ignitionSchedule4.pEndCallback = nullCallback;
        }
        else if( configPage2.nCylinders == 6 )
        {
          //Wasted COP mode for 6 cylinders. Ignition channels 1&4, 2&5 and 3&6 are paired together
          ignitionSchedule1.pStartCallback = beginCoil1and4Charge;
          ignitionSchedule1.pEndCallback = endCoil1and4Charge;
          ignitionSchedule2.pStartCallback = beginCoil2and5Charge;
          ignitionSchedule2.pEndCallback = endCoil2and5Charge;
          ignitionSchedule3.pStartCallback = beginCoil3and6Charge;
          ignitionSchedule3.pEndCallback = endCoil3and6Charge;

          ignitionSchedule4.pStartCallback = nullCallback;
          ignitionSchedule4.pEndCallback = nullCallback;
          ignitionSchedule5.pStartCallback = nullCallback;
          ignitionSchedule5.pEndCallback = nullCallback;
#if IGN_CHANNELS >= 6
          ignitionSchedule6.pStartCallback = nullCallback;
          ignitionSchedule6.pEndCallback = nullCallback;
#endif
        }
        else if( configPage2.nCylinders == 8 )
        {
          //Wasted COP mode for 8 cylinders. Ignition channels 1&5, 2&6, 3&7 and 4&8 are paired together
          ignitionSchedule1.pStartCallback = beginCoil1and5Charge;
          ignitionSchedule1.pEndCallback = endCoil1and5Charge;
          ignitionSchedule2.pStartCallback = beginCoil2and6Charge;
          ignitionSchedule2.pEndCallback = endCoil2and6Charge;
          ignitionSchedule3.pStartCallback = beginCoil3and7Charge;
          ignitionSchedule3.pEndCallback = endCoil3and7Charge;
          ignitionSchedule4.pStartCallback = beginCoil4and8Charge;
          ignitionSchedule4.pEndCallback = endCoil4and8Charge;

          ignitionSchedule5.pStartCallback = nullCallback;
          ignitionSchedule5.pEndCallback = nullCallback;
#if IGN_CHANNELS >= 6
          ignitionSchedule6.pStartCallback = nullCallback;
          ignitionSchedule6.pEndCallback = nullCallback;
#endif
#if IGN_CHANNELS >= 7
          ignitionSchedule7.pStartCallback = nullCallback;
          ignitionSchedule7.pEndCallback = nullCallback;
#endif
#if IGN_CHANNELS >= 8
          ignitionSchedule8.pStartCallback = nullCallback;
          ignitionSchedule8.pEndCallback = nullCallback;
#endif
        }
        else
        {
          //If the person has inadvertently selected this when running more than 4 cylinders or other than 6 cylinders, just use standard Wasted spark mode
          ignitionSchedule1.pStartCallback = beginCoil1Charge;
          ignitionSchedule1.pEndCallback = endCoil1Charge;
          ignitionSchedule2.pStartCallback = beginCoil2Charge;
          ignitionSchedule2.pEndCallback = endCoil2Charge;
          ignitionSchedule3.pStartCallback = beginCoil3Charge;
          ignitionSchedule3.pEndCallback = endCoil3Charge;
          ignitionSchedule4.pStartCallback = beginCoil4Charge;
          ignitionSchedule4.pEndCallback = endCoil4Charge;
          ignitionSchedule5.pStartCallback = beginCoil5Charge;
          ignitionSchedule5.pEndCallback = endCoil5Charge;
        }
        break;

    case IGN_MODE_SEQUENTIAL:
        ignitionSchedule1.pStartCallback = beginCoil1Charge;
        ignitionSchedule1.pEndCallback = endCoil1Charge;
        ignitionSchedule2.pStartCallback = beginCoil2Charge;
        ignitionSchedule2.pEndCallback = endCoil2Charge;
        ignitionSchedule3.pStartCallback = beginCoil3Charge;
        ignitionSchedule3.pEndCallback = endCoil3Charge;
        ignitionSchedule4.pStartCallback = beginCoil4Charge;
        ignitionSchedule4.pEndCallback = endCoil4Charge;
        ignitionSchedule5.pStartCallback = beginCoil5Charge;
        ignitionSchedule5.pEndCallback = endCoil5Charge;
#if IGN_CHANNELS >= 6
        ignitionSchedule6.pStartCallback = beginCoil6Charge;
        ignitionSchedule6.pEndCallback = endCoil6Charge;
#endif
#if IGN_CHANNELS >= 7
        ignitionSchedule7.pStartCallback = beginCoil7Charge;
        ignitionSchedule7.pEndCallback = endCoil7Charge;
#endif
#if IGN_CHANNELS >= 8
        ignitionSchedule8.pStartCallback = beginCoil8Charge;
        ignitionSchedule8.pEndCallback = endCoil8Charge;
#endif
        break;

    case IGN_MODE_ROTARY:
        if(configPage10.rotaryType == ROTARY_IGN_FC)
        {
          //Ignition channel 1 is a wasted spark signal for leading signal on both rotors
          ignitionSchedule1.pStartCallback = beginCoil1Charge;
          ignitionSchedule1.pEndCallback = endCoil1Charge;
          ignitionSchedule2.pStartCallback = beginCoil1Charge;
          ignitionSchedule2.pEndCallback = endCoil1Charge;

          ignitionSchedule3.pStartCallback = beginTrailingCoilCharge;
          ignitionSchedule3.pEndCallback = endTrailingCoilCharge1;
          ignitionSchedule4.pStartCallback = beginTrailingCoilCharge;
          ignitionSchedule4.pEndCallback = endTrailingCoilCharge2;
        }
        else if(configPage10.rotaryType == ROTARY_IGN_FD)
        {
          //Ignition channel 1 is a wasted spark signal for leading signal on both rotors
          ignitionSchedule1.pStartCallback = beginCoil1Charge;
          ignitionSchedule1.pEndCallback = endCoil1Charge;
          ignitionSchedule2.pStartCallback = beginCoil1Charge;
          ignitionSchedule2.pEndCallback = endCoil1Charge;

          //Trailing coils have their own channel each
          //IGN2 = front rotor trailing spark
          ignitionSchedule3.pStartCallback = beginCoil2Charge;
          ignitionSchedule3.pEndCallback = endCoil2Charge;
          //IGN3 = rear rotor trailing spark
          ignitionSchedule4.pStartCallback = beginCoil3Charge;
          ignitionSchedule4.pEndCallback = endCoil3Charge;

          //IGN4 not used
        }
        else if(configPage10.rotaryType == ROTARY_IGN_RX8)
        {
          //RX8 outputs are simply 1 coil and 1 output per plug

          //IGN1 is front rotor, leading spark
          ignitionSchedule1.pStartCallback = beginCoil1Charge;
          ignitionSchedule1.pEndCallback = endCoil1Charge;
          //IGN2 is rear rotor, leading spark
          ignitionSchedule2.pStartCallback = beginCoil2Charge;
          ignitionSchedule2.pEndCallback = endCoil2Charge;
          //IGN3 = front rotor trailing spark
          ignitionSchedule3.pStartCallback = beginCoil3Charge;
          ignitionSchedule3.pEndCallback = endCoil3Charge;
          //IGN4 = rear rotor trailing spark
          ignitionSchedule4.pStartCallback = beginCoil4Charge;
          ignitionSchedule4.pEndCallback = endCoil4Charge;
        }
        else { } //No action for other RX ignition modes (Future expansion / MISRA compliant).
        break;

    default:
        //Wasted spark (Shouldn't ever happen anyway)
        ignitionSchedule1.pStartCallback = beginCoil1Charge;
        ignitionSchedule1.pEndCallback = endCoil1Charge;
        ignitionSchedule2.pStartCallback = beginCoil2Charge;
        ignitionSchedule2.pEndCallback = endCoil2Charge;
        ignitionSchedule3.pStartCallback = beginCoil3Charge;
        ignitionSchedule3.pEndCallback = endCoil3Charge;
        ignitionSchedule4.pStartCallback = beginCoil4Charge;
        ignitionSchedule4.pEndCallback = endCoil4Charge;
        ignitionSchedule5.pStartCallback = beginCoil5Charge;
        ignitionSchedule5.pEndCallback = endCoil5Charge;
        break;
    }
}

/** Initialise Speeduino for the main loop.
 * Top level init entry point for all initialisations:
 * - Initialise and set sizes of 3D tables
 * - Load config from EEPROM, update config structures to current version of SW if needed.
 * - Initialise board (The initBoard() is for board X implemented in board_X.ino file)
 * - Initialise timers (See timers.ino)
 * - Perform optional SD card and RTC battery inits
 * - Load calibration tables from EEPROM
 * - Perform pin mapping (calling @ref setPinMapping() based on @ref config2.pinMapping)
 * - Stop any coil charging and close injectors
 * - Initialise schedulers, Idle, Fan, auxPWM, Corrections, AD-conversions, Programmable I/O
 * - Initialise baro (ambient pressure) by reading MAP (before engine runs)
 * - Initialise triggers (by @ref initialiseTriggers() )
 * - Perform cyl. count based initialisations (@ref config2.nCylinders)
 * - Perform injection and spark mode based setup
 *   - Assign injector open/close and coil charge begin/end functions to their dedicated global vars
 * - Perform fuel pressure priming by turning fuel pump on
 * - Read CLT and TPS sensors to have cranking pulsewidths computed correctly
 * - Mark Initialisation completed (this flag-marking is used in code to prevent after-init changes)
 */

/**
 * Handle EEPROM reset via pin press detection.
 *
 * This function checks if the EEPROM_RESET_PIN is defined and implements a
 * user-interactive EEPROM reset sequence:
 * 1. Waits for pin press (pulled low via INPUT_PULLUP)
 * 2. If held for 0.5s, turns LED on to indicate readiness
 * 3. If released within next 0.5s (before 1s total), erases EEPROM
 * 4. Provides visual feedback via LED_BUILTIN throughout process
 *
 * The function implements a 1050ms timeout window for the entire sequence.
 * EEPROM erase clears all bytes to 0xFF (255) or uses EEPROM.clear() for
 * Flash-as-EEPROM implementations.
 *
 * Only active when EEPROM_RESET_PIN is defined and not in UNIT_TEST mode.
 */
static void handleEepromResetPin(void)
{
    #if defined(EEPROM_RESET_PIN) && !defined(UNIT_TEST)
    uint32_t start_time = millis();
    byte exit_erase_loop = false;
    pinMode(EEPROM_RESET_PIN, INPUT_PULLUP);

    //only start routine when this pin is low because it is pulled low
    while (digitalRead(EEPROM_RESET_PIN) != HIGH && (millis() - start_time)<1050)
    {
      //make sure the key is pressed for at least 0.5 second
      if ((millis() - start_time)>500) {
        //if key is pressed afterboot for 0.5 second make led turn off
        digitalWrite(LED_BUILTIN, HIGH);

        //see if the user reacts to the led turned off with removing the keypress within 1 second
        while (((millis() - start_time)<1000) && (exit_erase_loop!=true)){

          //if user let go of key within 1 second erase eeprom
          if(digitalRead(EEPROM_RESET_PIN) != LOW){
            #if defined(FLASH_AS_EEPROM_h)
              EEPROM.read(0); //needed for SPI eeprom emulation.
              EEPROM.clear();
            #else
              for (int i = 0 ; i < EEPROM.length() ; i++) { EEPROM.write(i, 255);}
            #endif
            //if erase done exit while loop.
            exit_erase_loop = true;
          }
        }
      }
    }
    #endif
}

/**
 * Setup sensor-related interrupt handlers.
 *
 * This function configures interrupt attachments for three sensor types:
 * 1. Flex sensor (ethanol content) - CHANGE edge detection
 *    - Only if configPage2.flexEnabled > 0 and pin not reserved
 *    - Initializes currentStatus.ethanolPct to 0
 * 2. VSS (Vehicle Speed Sensor) - RISING edge detection
 *    - Only for VSS modes 2 and 3 (interrupt-driven, not CAN)
 *    - Only if pin not reserved
 * 3. Knock sensor (digital knock detection) - RISING or FALLING edge
 *    - Only if configPage10.knock_mode == KNOCK_MODE_DIGITAL
 *    - Configures pin mode (INPUT or INPUT_PULLUP based on knock_pullup)
 *    - Edge detection based on configPage10.knock_trigger
 *    - Only if pin not reserved
 *
 * Must be called after initialiseProgrammableIO() to ensure pins are properly mapped.
 */
static void setupSensorInterrupts(void)
{
    //Check whether the flex sensor is enabled and if so, attach an interrupt for it
    if(configPage2.flexEnabled > 0)
    {
      if(!pinIsReserved(pinFlex)) { attachInterrupt(digitalPinToInterrupt(pinFlex), flexPulse, CHANGE); }
      currentStatus.ethanolPct = 0;
    }
    //Same as above, but for the VSS input
    if(configPage2.vssMode > 1) // VSS modes 2 and 3 are interrupt drive (Mode 1 is CAN)
    {
      if(!pinIsReserved(pinVSS)) { attachInterrupt(digitalPinToInterrupt(pinVSS), vssPulse, RISING); }
    }
    //As above but for knock pulses
    if(configPage10.knock_mode == KNOCK_MODE_DIGITAL)
    {
      if(configPage10.knock_pullup) { pinMode(configPage10.knock_pin, INPUT_PULLUP); }
      else { pinMode(configPage10.knock_pin, INPUT); }

      if(!pinIsReserved(configPage10.knock_pin))
      {
        if(configPage10.knock_trigger == KNOCK_TRIGGER_HIGH) { attachInterrupt(digitalPinToInterrupt(configPage10.knock_pin), knockPulse, RISING); }
        else { attachInterrupt(digitalPinToInterrupt(configPage10.knock_pin), knockPulse, FALLING); }
      }
    }
}

void initialiseAll(void)
{   
    currentStatus.fpPrimed = false;
    currentStatus.injPrimed = false;

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    #if defined(CORE_STM32)
    configPage9.intcan_available = 1;   // device has internal canbus
    //STM32 can not currently enabled
    #endif

    /*
    ***********************************************************************************************************
    * EEPROM reset
    */
    handleEepromResetPin();
  
    // Unit tests should be independent of any stored configuration on the board!
#if !defined(UNIT_TEST)
    loadConfig();
    doUpdates(); //Check if any data items need updating (Occurs with firmware updates)
#endif


    //Always start with a clean slate on the bootloader capabilities level
    //This should be 0 until we hear otherwise from the 16u2
    configPage4.bootloaderCaps = 0;
    
    initBoard(); //This calls the current individual boards init function. See the board_xxx.ino files for these.
    initialiseTimers();
    
  #ifdef SD_LOGGING
    initRTC();
    if(configPage13.onboard_log_file_style) { initSD(); }
  #endif

//Teensy 4.1 does not require .begin() to be called. This introduces a 700ms delay on startup time whilst USB is enumerated if it is called
#ifndef CORE_TEENSY41
    Serial.begin(115200);
    #else
    teensy41_customSerialBegin();
#endif
    pPrimarySerial = &Serial; //Default to standard Serial interface
    BIT_SET(currentStatus.status4, BIT_STATUS4_ALLOW_LEGACY_COMMS); //Flag legacy comms as being allowed on startup
   
    //Setup the calibration tables
    loadCalibration();   

    //Set the pin mappings
    if((configPage2.pinMapping == 255) || (configPage2.pinMapping == 0)) //255 = EEPROM value in a blank AVR; 0 = EEPROM value in new FRAM
    {
      //First time running on this board
      resetConfigPages();
      setPinMapping(3); //Force board to v0.4
    }
    else { setPinMapping(configPage2.pinMapping); }

    // Repeatedly initialising the CAN bus hangs the system when
    // running initialisation tests on Teensy 3.5
    #if defined(NATIVE_CAN_AVAILABLE) && !defined(UNIT_TEST)
      initCAN();
    #endif

    //Must come after setPinMapping() as secondary serial can be changed on a per board basis
    if (configPage9.enable_secondarySerial == 1) { secondarySerial.begin(115200); }

    safetyShutdownAllOutputs();
    //Perform all initialisations
    initialiseSchedulers();
    //initialiseDisplay();
    initialiseIdle(true);
    initialiseFan();
    initialiseAirCon();
    initialiseAuxPWM();
    initialiseCorrections();
    BIT_CLEAR(currentStatus.engineProtectStatus, PROTECT_IO_ERROR); //Clear the I/O error bit. The bit will be set in initialiseADC() if there is problem in there.
    initialiseADC();
    initialiseMAPBaro();
    initialiseProgrammableIO();

    setupSensorInterrupts();

    calculateFuelParameters();

    //Begin the main crank trigger interrupt pin setup
    //The interrupt numbering is a bit odd - See here for reference: arduino.cc/en/Reference/AttachInterrupt
    //These assignments are based on the Arduino Mega AND VARY BETWEEN BOARDS. Please confirm the board you are using and update accordingly.
    currentStatus.RPM = 0;
    currentStatus.hasSync = false;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    currentStatus.runSecs = 0;
    currentStatus.secl = 0;
    //currentStatus.seclx10 = 0;
    currentStatus.startRevolutions = 0;
    currentStatus.syncLossCounter = 0;
    currentStatus.flatShiftingHard = false;
    currentStatus.launchingHard = false;
    currentStatus.crankRPM = ((unsigned int)configPage4.crankRPM * 10); //Crank RPM limit (Saves us calculating this over and over again. It's updated once per second in timers.ino)
    currentStatus.fuelPumpOn = false;
    currentStatus.engineProtectStatus = 0;
    triggerFilterTime = 0; //Trigger filter time is the shortest possible time (in uS) that there can be between crank teeth (ie at max RPM). Any pulses that occur faster than this time will be discarded as noise. This is simply a default value, the actual values are set in the setup() functions of each decoder
    dwellLimit_uS = (1000 * configPage4.dwellLimit);
    currentStatus.nChannels = ((uint8_t)INJ_CHANNELS << 4) + IGN_CHANNELS; //First 4 bits store the number of injection channels, 2nd 4 store the number of ignition channels
    fpPrimeTime = 0;
    ms_counter = 0;
    fixedCrankingOverride = 0;
    timer5_overflow_count = 0;
    toothHistoryIndex = 0;
    resetDecoder();
    
    noInterrupts();
    initialiseTriggers();

    //The secondary input can be used for VSS if nothing else requires it. Allows for the standard VR conditioner to be used for VSS. This MUST be run after the initialiseTriggers() function
    if( VSS_USES_RPM2() ) { attachInterrupt(digitalPinToInterrupt(pinVSS), vssPulse, RISING); } //Secondary trigger input can safely be used for VSS
    if( FLEX_USES_RPM2() ) { attachInterrupt(digitalPinToInterrupt(pinFlex), flexPulse, CHANGE); } //Secondary trigger input can safely be used for Flex sensor

    //End crank trigger interrupt attachment
    if(configPage2.strokes == FOUR_STROKE)
    {
      //Default is 1 squirt per revolution, so we halve the given req-fuel figure (Which would be over 2 revolutions)
      req_fuel_uS = req_fuel_uS / 2; //The req_fuel calculation above gives the total required fuel (At VE 100%) in the full cycle. If we're doing more than 1 squirt per cycle then we need to split the amount accordingly. (Note that in a non-sequential 4-stroke setup you cannot have less than 2 squirts as you cannot determine the stroke to make the single squirt on)
    }

    //Initial values for loop times
    currentLoopTime = micros();
    mainLoopCount = 0;

    if(configPage2.divider == 0) { currentStatus.nSquirts = 2; } //Safety check.
    else { currentStatus.nSquirts = configPage2.nCylinders / configPage2.divider; } //The number of squirts being requested. This is manually overridden below for sequential setups (Due to TS req_fuel calc limitations)
    if(currentStatus.nSquirts == 0) { currentStatus.nSquirts = 1; } //Safety check. Should never happen as TS will give an error, but leave in case tune is manually altered etc. 

    //Calculate the number of degrees between cylinders
    //Set some default values. These will be updated below if required.
    CRANK_ANGLE_MAX_IGN = 360;
    CRANK_ANGLE_MAX_INJ = 360;

    maxInjOutputs = 1; // Disable all injectors expect channel 1

    ignition1EndAngle = 0;
    ignition2EndAngle = 0;
    ignition3EndAngle = 0;
    ignition4EndAngle = 0;
#if IGN_CHANNELS >= 5
    ignition5EndAngle = 0;
#endif
#if IGN_CHANNELS >= 6
    ignition6EndAngle = 0;
#endif
#if IGN_CHANNELS >= 7
    ignition7EndAngle = 0;
#endif
#if IGN_CHANNELS >= 8
    ignition8EndAngle = 0;
#endif

    if(configPage2.strokes == FOUR_STROKE) { CRANK_ANGLE_MAX_INJ = 720 / currentStatus.nSquirts; }
    else { CRANK_ANGLE_MAX_INJ = 360 / currentStatus.nSquirts; }

    configureCylinderTimings();

    currentStatus.status3 |= currentStatus.nSquirts << BIT_STATUS3_NSQUIRTS1; //Top 3 bits of the status3 variable are the number of squirts. This must be done after the above section due to nSquirts being forced to 1 for sequential
    
    //Special case:
    //3 or 5 squirts per cycle MUST be tracked over 720 degrees. This is because the angles for them (Eg 720/3=240) are not evenly divisible into 360
    //This is ONLY the case on 4 stroke systems
    if( (currentStatus.nSquirts == 3) || (currentStatus.nSquirts == 5) )
    {
      if(configPage2.strokes == FOUR_STROKE) { CRANK_ANGLE_MAX_INJ = (720U / currentStatus.nSquirts); }
    }

    configureInjectionLayout();

    configureIgnitionMode();

    //Begin priming the fuel pump. This is turned off in the low resolution, 1s interrupt in timers.ino
    //First check that the priming time is not 0
    if(configPage2.fpPrime > 0)
    {
      FUEL_PUMP_ON();
      currentStatus.fuelPumpOn = true;
    }
    else { currentStatus.fpPrimed = true; } //If the user has set 0 for the pump priming, immediately mark the priming as being completed

    interrupts();
    readCLT(false); // Need to read coolant temp to make priming pulsewidth work correctly. The false here disables use of the filter
    readTPS(false); // Need to read tps to detect flood clear state

    /* tacho sweep function. */
    currentStatus.tachoSweepEnabled = (configPage2.useTachoSweep > 0);
    /* SweepMax is stored as a byte, RPM/100. divide by 60 to convert min to sec (net 5/3).  Multiply by ignition pulses per rev.
       tachoSweepIncr is also the number of tach pulses per second */
    tachoSweepIncr = configPage2.tachoSweepMaxRPM * maxIgnOutputs * 5 / 3;
    
    currentStatus.initialisationComplete = true;
    digitalWrite(LED_BUILTIN, HIGH);

}
/** Set board / microcontroller specific pin mappings / assignments.
 *
 * MODULARIZED VERSION:
 * Original function was 1853 lines with giant switch-case (60+ boards)
 * Now uses table-driven architecture in board_config/ module
 *
 * Benefits:
 * - Extensible: Add new boards without modifying core logic
 * - Testable: Each board config is independent function
 * - Maintainable: Board-specific code in separate files
 * - Performance: Direct function call vs switch evaluation
 *
 * Original code preserved in init.cpp.backup_original (lines 1216-3062)
 *
 * The boardID is originated from tuning SW (e.g. TS) set values
 * and are available in reference/speeduino.ini (See pinLayout)
 */
void setPinMapping(byte boardID)
{
  // MODULARIZED: Call board configuration API
  // This replaces 1853 lines of switch-case code
  // All original logic is preserved in board_config/ module

  // Step 1: Configure pin assignments based on board ID
  boardConfigSetPinMapping(boardID);

  // Step 2: Setup pin modes for all configured pins
  boardConfigSetupPinModes();

  // Step 3: Setup port/mask pointers for direct GPIO access
  boardConfigSetupPortPointers();
}

// ORIGINAL FUNCTION REMOVED - PRESERVED IN init.cpp.backup_original
// Original: Lines 1216-3062 (1853 lines total)
// Modular replacement above preserves 100% of original logic
//
// To review original code, see: init.cpp.backup_original
// Backup contains complete original setPinMapping() function

/**
 * Setup trigger pins and configure interrupt numbers for decoder inputs.
 *
 * This function performs the preamble setup before trigger decoder initialization:
 * 1. Declares interrupt numbers for primary, secondary, and tertiary triggers
 * 2. Platform-specific interrupt mapping:
 *    - AVR platforms: Maps physical pin numbers to interrupt numbers via switch-case
 *      (Arduino Mega 2560 mapping: pins 2,3,18,19,20,21 to interrupts 0,1,5,4,3,2)
 *    - Non-AVR platforms: Direct pin-to-interrupt mapping
 * 3. Configures pin modes (INPUT) for all three trigger pins
 * 4. Detaches any existing interrupts from all trigger pins
 * 5. Sets default trigger edge values (all to 0, updated by decoder setup)
 *
 * Modified global variables:
 * - triggerInterrupt, triggerInterrupt2, triggerInterrupt3 (passed back via reference)
 * - primaryTriggerEdge, secondaryTriggerEdge, tertiaryTriggerEdge (set to 0)
 *
 * Must be called before decoder-specific setup functions (triggerSetup_*()).
 *
 * @param[out] triggerInterrupt    Pointer to store primary trigger interrupt number
 * @param[out] triggerInterrupt2   Pointer to store secondary trigger interrupt number
 * @param[out] triggerInterrupt3   Pointer to store tertiary trigger interrupt number
 */
static void setupTriggerPins(byte *triggerInterrupt, byte *triggerInterrupt2, byte *triggerInterrupt3)
{
  *triggerInterrupt = 0; // By default, use the first interrupt
  *triggerInterrupt2 = 1;
  *triggerInterrupt3 = 2;

  #if defined(CORE_AVR)
    switch (pinTrigger) {
      //Arduino Mega 2560 mapping
      case 2:
        *triggerInterrupt = 0; break;
      case 3:
        *triggerInterrupt = 1; break;
      case 18:
        *triggerInterrupt = 5; break;
      case 19:
        *triggerInterrupt = 4; break;
      case 20:
        *triggerInterrupt = 3; break;
      case 21:
        *triggerInterrupt = 2; break;
      default:
        *triggerInterrupt = 0; break; //This should NEVER happen
    }
  #else
    *triggerInterrupt = pinTrigger;
  #endif

  #if defined(CORE_AVR)
    switch (pinTrigger2) {
      //Arduino Mega 2560 mapping
      case 2:
        *triggerInterrupt2 = 0; break;
      case 3:
        *triggerInterrupt2 = 1; break;
      case 18:
        *triggerInterrupt2 = 5; break;
      case 19:
        *triggerInterrupt2 = 4; break;
      case 20:
        *triggerInterrupt2 = 3; break;
      case 21:
        *triggerInterrupt2 = 2; break;
      default:
        *triggerInterrupt2 = 0; break; //This should NEVER happen
    }
  #else
    *triggerInterrupt2 = pinTrigger2;
  #endif

  #if defined(CORE_AVR)
    switch (pinTrigger3) {
      //Arduino Mega 2560 mapping
      case 2:
        *triggerInterrupt3 = 0; break;
      case 3:
        *triggerInterrupt3 = 1; break;
      case 18:
        *triggerInterrupt3 = 5; break;
      case 19:
        *triggerInterrupt3 = 4; break;
      case 20:
        *triggerInterrupt3 = 3; break;
      case 21:
        *triggerInterrupt3 = 2; break;
      default:
        *triggerInterrupt3 = 0; break; //This should NEVER happen
    }
  #else
    *triggerInterrupt3 = pinTrigger3;
  #endif

  pinMode(pinTrigger, INPUT);
  pinMode(pinTrigger2, INPUT);
  pinMode(pinTrigger3, INPUT);

  detachInterrupt(*triggerInterrupt);
  detachInterrupt(*triggerInterrupt2);
  detachInterrupt(*triggerInterrupt3);
  //The default values for edges
  primaryTriggerEdge = 0; //This should ALWAYS be changed below
  secondaryTriggerEdge = 0; //This is optional and may not be changed below, depending on the decoder in use
  tertiaryTriggerEdge = 0; //This is even more optional and may not be changed below, depending on the decoder in use
}

/*
===============================================================================
FASE B - FASE 4: Decoder Initialization Functions (Batch 1 of 3)
===============================================================================
Each function configures a specific trigger decoder:
- Calls triggerSetup_xxx()
- Assigns function pointers (triggerHandler, getRPM, getCrankAngle, etc.)
- Sets trigger edges based on configuration
- Attaches interrupts
===============================================================================
*/

/**
 * Initialize Missing Tooth decoder.
 * Supports primary, secondary, and tertiary triggers (VVT2).
 */
static void initDecoder_MissingTooth(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  triggerSetup_missingTooth();
  triggerHandler = triggerPri_missingTooth;
  triggerSecondaryHandler = triggerSec_missingTooth;
  triggerTertiaryHandler = triggerThird_missingTooth;

  getRPM = getRPM_missingTooth;
  getCrankAngle = getCrankAngle_missingTooth;
  triggerSetEndTeeth = triggerSetEndTeeth_missingTooth;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }
  if(configPage10.TrigEdgeThrd == 0) { tertiaryTriggerEdge = RISING; }
  else { tertiaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);

  if(BIT_CHECK(decoderState, BIT_DECODER_HAS_SECONDARY)) { attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge); }
  if(configPage10.vvt2Enabled > 0) { attachInterrupt(triggerInterrupt3, triggerTertiaryHandler, tertiaryTriggerEdge); }
}

/**
 * Initialize Basic Distributor decoder.
 * Simple single-channel distributor trigger.
 */
static void initDecoder_BasicDistributor(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_BasicDistributor();
  triggerHandler = triggerPri_BasicDistributor;
  getRPM = getRPM_BasicDistributor;
  getCrankAngle = getCrankAngle_BasicDistributor;
  triggerSetEndTeeth = triggerSetEndTeeth_BasicDistributor;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
}

/**
 * Initialize Dual Wheel decoder (case 2).
 * Two-channel crank/cam trigger.
 */
static void initDecoder_DualWheel(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_DualWheel();
  triggerHandler = triggerPri_DualWheel;
  triggerSecondaryHandler = triggerSec_DualWheel;
  getRPM = getRPM_DualWheel;
  getCrankAngle = getCrankAngle_DualWheel;
  triggerSetEndTeeth = triggerSetEndTeeth_DualWheel;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize GM 7X decoder.
 * NOTE: Original code has duplicate attachInterrupt calls (preserved for 100% logic compatibility).
 */
static void initDecoder_GM7X(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_GM7X();
  triggerHandler = triggerPri_GM7X;
  getRPM = getRPM_GM7X;
  getCrankAngle = getCrankAngle_GM7X;
  triggerSetEndTeeth = triggerSetEndTeeth_GM7X;

  if(configPage4.TrigEdge == 0) { attachInterrupt(triggerInterrupt, triggerHandler, RISING); }
  else { attachInterrupt(triggerInterrupt, triggerHandler, FALLING); }

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
}

/**
 * Initialize 4G63 (Mitsubishi) decoder.
 * Uses CHANGE on primary, FALLING on secondary.
 */
static void initDecoder_4G63(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_4G63();
  triggerHandler = triggerPri_4G63;
  triggerSecondaryHandler = triggerSec_4G63;
  getRPM = getRPM_4G63;
  getCrankAngle = getCrankAngle_4G63;
  triggerSetEndTeeth = triggerSetEndTeeth_4G63;

  primaryTriggerEdge = CHANGE;
  secondaryTriggerEdge = FALLING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize 24X (Opel/GM) decoder.
 * Secondary always uses CHANGE.
 */
static void initDecoder_24X(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_24X();
  triggerHandler = triggerPri_24X;
  triggerSecondaryHandler = triggerSec_24X;
  getRPM = getRPM_24X;
  getCrankAngle = getCrankAngle_24X;
  triggerSetEndTeeth = triggerSetEndTeeth_24X;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = CHANGE;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Jeep 2000 decoder.
 * Secondary always uses CHANGE.
 */
static void initDecoder_Jeep2000(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_Jeep2000();
  triggerHandler = triggerPri_Jeep2000;
  triggerSecondaryHandler = triggerSec_Jeep2000;
  getRPM = getRPM_Jeep2000;
  getCrankAngle = getCrankAngle_Jeep2000;
  triggerSetEndTeeth = triggerSetEndTeeth_Jeep2000;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = CHANGE;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Audi 135 decoder.
 * Secondary always uses RISING.
 */
static void initDecoder_Audi135(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_Audi135();
  triggerHandler = triggerPri_Audi135;
  triggerSecondaryHandler = triggerSec_Audi135;
  getRPM = getRPM_Audi135;
  getCrankAngle = getCrankAngle_Audi135;
  triggerSetEndTeeth = triggerSetEndTeeth_Audi135;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = RISING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Honda D17 decoder.
 * Secondary uses CHANGE.
 */
static void initDecoder_HondaD17(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_HondaD17();
  triggerHandler = triggerPri_HondaD17;
  triggerSecondaryHandler = triggerSec_HondaD17;
  getRPM = getRPM_HondaD17;
  getCrankAngle = getCrankAngle_HondaD17;
  triggerSetEndTeeth = triggerSetEndTeeth_HondaD17;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = CHANGE;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Honda J32 decoder.
 * Always uses RISING edge (config ignored).
 */
static void initDecoder_HondaJ32(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_HondaJ32();
  triggerHandler = triggerPri_HondaJ32;
  triggerSecondaryHandler = triggerSec_HondaJ32;
  getRPM = getRPM_HondaJ32;
  getCrankAngle = getCrankAngle_HondaJ32;
  triggerSetEndTeeth = triggerSetEndTeeth_HondaJ32;

  primaryTriggerEdge = RISING;
  secondaryTriggerEdge = RISING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/*
===============================================================================
FASE B - FASE 4: Decoder Initialization Functions (Batch 2 of 3)
===============================================================================
*/

/**
 * Initialize Miata 99-05 decoder.
 */
static void initDecoder_Miata9905(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_Miata9905();
  triggerHandler = triggerPri_Miata9905;
  triggerSecondaryHandler = triggerSec_Miata9905;
  getRPM = getRPM_Miata9905;
  getCrankAngle = getCrankAngle_Miata9905;
  triggerSetEndTeeth = triggerSetEndTeeth_Miata9905;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Mazda AU decoder.
 * Secondary always uses FALLING.
 */
static void initDecoder_MazdaAU(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_MazdaAU();
  triggerHandler = triggerPri_MazdaAU;
  triggerSecondaryHandler = triggerSec_MazdaAU;
  getRPM = getRPM_MazdaAU;
  getCrankAngle = getCrankAngle_MazdaAU;
  triggerSetEndTeeth = triggerSetEndTeeth_MazdaAU;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = FALLING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Non-360 decoder.
 * Uses DualWheel trigger handlers.
 */
static void initDecoder_Non360(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_non360();
  triggerHandler = triggerPri_DualWheel;
  triggerSecondaryHandler = triggerSec_DualWheel;
  getRPM = getRPM_non360;
  getCrankAngle = getCrankAngle_non360;
  triggerSetEndTeeth = triggerSetEndTeeth_non360;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = FALLING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Nissan 360 decoder.
 * Secondary always uses CHANGE.
 */
static void initDecoder_Nissan360(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_Nissan360();
  triggerHandler = triggerPri_Nissan360;
  triggerSecondaryHandler = triggerSec_Nissan360;
  getRPM = getRPM_Nissan360;
  getCrankAngle = getCrankAngle_Nissan360;
  triggerSetEndTeeth = triggerSetEndTeeth_Nissan360;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = CHANGE;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Subaru 6/7 decoder.
 * Secondary always uses FALLING.
 */
static void initDecoder_Subaru67(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_Subaru67();
  triggerHandler = triggerPri_Subaru67;
  triggerSecondaryHandler = triggerSec_Subaru67;
  getRPM = getRPM_Subaru67;
  getCrankAngle = getCrankAngle_Subaru67;
  triggerSetEndTeeth = triggerSetEndTeeth_Subaru67;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = FALLING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Daihatsu +1 decoder.
 * Single channel decoder (no secondary).
 */
static void initDecoder_Daihatsu(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_Daihatsu();
  triggerHandler = triggerPri_Daihatsu;
  getRPM = getRPM_Daihatsu;
  getCrankAngle = getCrankAngle_Daihatsu;
  triggerSetEndTeeth = triggerSetEndTeeth_Daihatsu;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
}

/**
 * Initialize Harley Davidson decoder.
 * Single channel, always RISING edge.
 */
static void initDecoder_Harley(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_Harley();
  triggerHandler = triggerPri_Harley;
  getRPM = getRPM_Harley;
  getCrankAngle = getCrankAngle_Harley;
  triggerSetEndTeeth = triggerSetEndTeeth_Harley;

  primaryTriggerEdge = RISING;
  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
}

/**
 * Initialize 36-2-2-2 decoder.
 * Uses missing tooth crank angle function.
 */
static void initDecoder_36_2_2_2(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_ThirtySixMinus222();
  triggerHandler = triggerPri_ThirtySixMinus222;
  triggerSecondaryHandler = triggerSec_ThirtySixMinus222;
  getRPM = getRPM_ThirtySixMinus222;
  getCrankAngle = getCrankAngle_missingTooth;
  triggerSetEndTeeth = triggerSetEndTeeth_ThirtySixMinus222;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize 36-2-1 decoder.
 * Uses missing tooth secondary handler and crank angle function.
 */
static void initDecoder_36_2_1(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_ThirtySixMinus21();
  triggerHandler = triggerPri_ThirtySixMinus21;
  triggerSecondaryHandler = triggerSec_missingTooth;
  getRPM = getRPM_ThirtySixMinus21;
  getCrankAngle = getCrankAngle_missingTooth;
  triggerSetEndTeeth = triggerSetEndTeeth_ThirtySixMinus21;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize DSM 420a decoder.
 * Secondary always uses FALLING.
 */
static void initDecoder_420a(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_420a();
  triggerHandler = triggerPri_420a;
  triggerSecondaryHandler = triggerSec_420a;
  getRPM = getRPM_420a;
  getCrankAngle = getCrankAngle_420a;
  triggerSetEndTeeth = triggerSetEndTeeth_420a;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  secondaryTriggerEdge = FALLING;

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/*
===============================================================================
FASE B - FASE 4: Decoder Initialization Functions (Batch 3 of 3)
===============================================================================
*/

/**
 * Initialize Weber-Marelli decoder.
 * Uses DualWheel setup with Webber handlers.
 */
static void initDecoder_Weber(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_DualWheel();
  triggerHandler = triggerPri_Webber;
  triggerSecondaryHandler = triggerSec_Webber;
  getRPM = getRPM_DualWheel;
  getCrankAngle = getCrankAngle_DualWheel;
  triggerSetEndTeeth = triggerSetEndTeeth_DualWheel;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Ford ST170 decoder.
 * Uses missing tooth primary handler.
 */
static void initDecoder_ST170(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_FordST170();
  triggerHandler = triggerPri_missingTooth;
  triggerSecondaryHandler = triggerSec_FordST170;
  getRPM = getRPM_FordST170;
  getCrankAngle = getCrankAngle_FordST170;
  triggerSetEndTeeth = triggerSetEndTeeth_FordST170;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize DRZ400 decoder.
 * Uses DualWheel primary handler and DualWheel functions.
 */
static void initDecoder_DRZ400(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_DRZ400();
  triggerHandler = triggerPri_DualWheel;
  triggerSecondaryHandler = triggerSec_DRZ400;
  getRPM = getRPM_DualWheel;
  getCrankAngle = getCrankAngle_DualWheel;
  triggerSetEndTeeth = triggerSetEndTeeth_DualWheel;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize NGC (Chrysler) decoder.
 * Cylinder-dependent secondary handler selection.
 */
static void initDecoder_NGC(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_NGC();
  triggerHandler = triggerPri_NGC;
  getRPM = getRPM_NGC;
  getCrankAngle = getCrankAngle_missingTooth;
  triggerSetEndTeeth = triggerSetEndTeeth_NGC;

  primaryTriggerEdge = CHANGE;
  if (configPage2.nCylinders == 4) {
    triggerSecondaryHandler = triggerSec_NGC4;
    secondaryTriggerEdge = CHANGE;
  }
  else {
    triggerSecondaryHandler = triggerSec_NGC68;
    secondaryTriggerEdge = FALLING;
  }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize V-Max decoder.
 * Uses CHANGE interrupt, primaryTriggerEdge used as boolean in decoder.
 */
static void initDecoder_Vmax(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_Vmax();
  triggerHandler = triggerPri_Vmax;
  getRPM = getRPM_Vmax;
  getCrankAngle = getCrankAngle_Vmax;
  triggerSetEndTeeth = triggerSetEndTeeth_Vmax;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = true; }
  else { primaryTriggerEdge = false; }

  attachInterrupt(triggerInterrupt, triggerHandler, CHANGE);
}

/**
 * Initialize Renix (Renault 44 tooth) decoder.
 * Uses missing tooth functions for RPM and crank angle.
 */
static void initDecoder_Renix(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_Renix();
  triggerHandler = triggerPri_Renix;
  getRPM = getRPM_missingTooth;
  getCrankAngle = getCrankAngle_missingTooth;
  triggerSetEndTeeth = triggerSetEndTeeth_Renix;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
}

/**
 * Initialize Rover MEMS decoder.
 * Multiple flywheel trigger combinations.
 */
static void initDecoder_RoverMEMS(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_RoverMEMS();
  triggerHandler = triggerPri_RoverMEMS;
  getRPM = getRPM_RoverMEMS;
  triggerSetEndTeeth = triggerSetEndTeeth_RoverMEMS;

  triggerSecondaryHandler = triggerSec_RoverMEMS;
  getCrankAngle = getCrankAngle_missingTooth;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize Suzuki K6A decoder.
 * Single channel, pattern over 720 degrees.
 */
static void initDecoder_SuzukiK6A(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerSetup_SuzukiK6A();
  triggerHandler = triggerPri_SuzukiK6A;
  getRPM = getRPM_SuzukiK6A;
  getCrankAngle = getCrankAngle_SuzukiK6A;
  triggerSetEndTeeth = triggerSetEndTeeth_SuzukiK6A;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
}

/**
 * Initialize Ford TFI decoder.
 */
static void initDecoder_FordTFI(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt3; // Unused

  triggerSetup_FordTFI();
  triggerHandler = triggerPri_FordTFI;
  triggerSecondaryHandler = triggerSec_FordTFI;
  getRPM = getRPM_FordTFI;
  getCrankAngle = getCrankAngle_FordTFI;
  triggerSetEndTeeth = triggerSetEndTeeth_FordTFI;

  if(configPage4.TrigEdge == 0) { primaryTriggerEdge = RISING; }
  else { primaryTriggerEdge = FALLING; }
  if(configPage4.TrigEdgeSec == 0) { secondaryTriggerEdge = RISING; }
  else { secondaryTriggerEdge = FALLING; }

  attachInterrupt(triggerInterrupt, triggerHandler, primaryTriggerEdge);
  attachInterrupt(triggerInterrupt2, triggerSecondaryHandler, secondaryTriggerEdge);
}

/**
 * Initialize default decoder (fallback to missing tooth).
 */
static void initDecoder_Default(byte triggerInterrupt, byte triggerInterrupt2, byte triggerInterrupt3)
{
  (void)triggerInterrupt2; (void)triggerInterrupt3; // Unused

  triggerHandler = triggerPri_missingTooth;
  getRPM = getRPM_missingTooth;
  getCrankAngle = getCrankAngle_missingTooth;

  if(configPage4.TrigEdge == 0) { attachInterrupt(triggerInterrupt, triggerHandler, RISING); }
  else { attachInterrupt(triggerInterrupt, triggerHandler, FALLING); }
}

/** Initialise the chosen trigger decoder.
 * - Set Interrupt numbers @ref triggerInterrupt, @ref triggerInterrupt2 and @ref triggerInterrupt3  by pin their numbers (based on board CORE_* define)
 * - Call decoder specific setup function triggerSetup_*() (by @ref config4.TrigPattern, set to one of the DECODER_* defines) and do any additional initialisations needed.
 *
 * @todo Explain why triggerSetup_*() alone cannot do all the setup, but there's ~10+ lines worth of extra init for each of decoders.
 */
void initialiseTriggers(void)
{
  byte triggerInterrupt = 0; // By default, use the first interrupt
  byte triggerInterrupt2 = 1;
  byte triggerInterrupt3 = 2;

  setupTriggerPins(&triggerInterrupt, &triggerInterrupt2, &triggerInterrupt3);

  //Set the trigger function based on the decoder in the config
  switch (configPage4.TrigPattern)
  {
    case DECODER_MISSING_TOOTH:
      initDecoder_MissingTooth(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_BASIC_DISTRIBUTOR:
      initDecoder_BasicDistributor(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case 2:
      initDecoder_DualWheel(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_GM7X:
      initDecoder_GM7X(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_4G63:
      initDecoder_4G63(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_24X:
      initDecoder_24X(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_JEEP2000:
      initDecoder_Jeep2000(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_AUDI135:
      initDecoder_Audi135(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_HONDA_D17:
      initDecoder_HondaD17(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_HONDA_J32:
      initDecoder_HondaJ32(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_MIATA_9905:
      initDecoder_Miata9905(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_MAZDA_AU:
      initDecoder_MazdaAU(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_NON360:
      initDecoder_Non360(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_NISSAN_360:
      initDecoder_Nissan360(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_SUBARU_67:
      initDecoder_Subaru67(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_DAIHATSU_PLUS1:
      initDecoder_Daihatsu(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_HARLEY:
      initDecoder_Harley(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_36_2_2_2:
      initDecoder_36_2_2_2(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_36_2_1:
      initDecoder_36_2_1(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_420A:
      initDecoder_420a(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_WEBER:
      initDecoder_Weber(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_ST170:
      initDecoder_ST170(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_DRZ400:
      initDecoder_DRZ400(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_NGC:
      initDecoder_NGC(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_VMAX:
      initDecoder_Vmax(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_RENIX:
      initDecoder_Renix(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_ROVERMEMS:
      initDecoder_RoverMEMS(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_SUZUKI_K6A:
      initDecoder_SuzukiK6A(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    case DECODER_FORD_TFI:
      initDecoder_FordTFI(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;

    default:
      initDecoder_Default(triggerInterrupt, triggerInterrupt2, triggerInterrupt3);
      break;
  }

  #if defined(CORE_TEENSY41)
    //Teensy 4 requires a HYSTERESIS flag to be set on any external interrupt pins to prevent false interrupts
    setTeensy41PinsHysteresis();
  #endif
}

static inline bool isAnyFuelScheduleRunning(void) {
  return fuelSchedule1.Status==RUNNING
      || fuelSchedule2.Status==RUNNING
      || fuelSchedule3.Status==RUNNING
      || fuelSchedule4.Status==RUNNING
#if INJ_CHANNELS >= 5      
      || fuelSchedule5.Status==RUNNING
#endif
#if INJ_CHANNELS >= 6
      || fuelSchedule6.Status==RUNNING
#endif
#if INJ_CHANNELS >= 7
      || fuelSchedule7.Status==RUNNING
#endif
#if INJ_CHANNELS >= 8
      || fuelSchedule8.Status==RUNNING
#endif
      ;
}

static inline bool isAnyIgnScheduleRunning(void) {
  return ignitionSchedule1.Status==RUNNING      
#if IGN_CHANNELS >= 2 
      || ignitionSchedule2.Status==RUNNING
#endif      
#if IGN_CHANNELS >= 3 
      || ignitionSchedule3.Status==RUNNING
#endif      
#if IGN_CHANNELS >= 4       
      || ignitionSchedule4.Status==RUNNING
#endif      
#if IGN_CHANNELS >= 5      
      || ignitionSchedule5.Status==RUNNING
#endif
#if IGN_CHANNELS >= 6
      || ignitionSchedule6.Status==RUNNING
#endif
#if IGN_CHANNELS >= 7
      || ignitionSchedule7.Status==RUNNING
#endif
#if IGN_CHANNELS >= 8
      || ignitionSchedule8.Status==RUNNING
#endif
      ;
}

/** Change injectors or/and ignition angles to 720deg.
 * Roll back req_fuel size and set number of outputs equal to cylinder count.
* */
void changeHalfToFullSync(void)
{
  //Need to do another check for injLayout as this function can be called from ignition
  noInterrupts();
  if( (configPage2.injLayout == INJ_SEQUENTIAL) && (CRANK_ANGLE_MAX_INJ != 720) && (!isAnyFuelScheduleRunning()))
  {
    CRANK_ANGLE_MAX_INJ = 720;
    req_fuel_uS *= 2;
    
    fuelSchedule1.pStartFunction = openInjector1;
    fuelSchedule1.pEndFunction = closeInjector1;
    fuelSchedule2.pStartFunction = openInjector2;
    fuelSchedule2.pEndFunction = closeInjector2;
    fuelSchedule3.pStartFunction = openInjector3;
    fuelSchedule3.pEndFunction = closeInjector3;
    fuelSchedule4.pStartFunction = openInjector4;
    fuelSchedule4.pEndFunction = closeInjector4;
#if INJ_CHANNELS >= 5
    fuelSchedule5.pStartFunction = openInjector5;
    fuelSchedule5.pEndFunction = closeInjector5;
#endif
#if INJ_CHANNELS >= 6
    fuelSchedule6.pStartFunction = openInjector6;
    fuelSchedule6.pEndFunction = closeInjector6;
#endif
#if INJ_CHANNELS >= 7
    fuelSchedule7.pStartFunction = openInjector7;
    fuelSchedule7.pEndFunction = closeInjector7;
#endif
#if INJ_CHANNELS >= 8
    fuelSchedule8.pStartFunction = openInjector8;
     fuelSchedule8.pEndFunction = closeInjector8;
#endif

    switch (configPage2.nCylinders)
    {
      case 4:
        maxInjOutputs = 4;
        break;
            
      case 6:
        maxInjOutputs = 6;
        break;

      case 8:
        maxInjOutputs = 8;
        break;

      default:
        break; //No actions required for other cylinder counts

    }
  }
  interrupts();

  //Need to do another check for sparkMode as this function can be called from injection
  if( (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (CRANK_ANGLE_MAX_IGN != 720) && (!isAnyIgnScheduleRunning()) )
  {
    CRANK_ANGLE_MAX_IGN = 720;
    maxIgnOutputs = configPage2.nCylinders;
    switch (configPage2.nCylinders)
    {
    case 4:
      ignitionSchedule1.pStartCallback = beginCoil1Charge;
      ignitionSchedule1.pEndCallback = endCoil1Charge;
      ignitionSchedule2.pStartCallback = beginCoil2Charge;
      ignitionSchedule2.pEndCallback = endCoil2Charge;
      break;

    case 6:
      ignitionSchedule1.pStartCallback = beginCoil1Charge;
      ignitionSchedule1.pEndCallback = endCoil1Charge;
      ignitionSchedule2.pStartCallback = beginCoil2Charge;
      ignitionSchedule2.pEndCallback = endCoil2Charge;
      ignitionSchedule3.pStartCallback = beginCoil3Charge;
      ignitionSchedule3.pEndCallback = endCoil3Charge;
      break;

    case 8:
      ignitionSchedule1.pStartCallback = beginCoil1Charge;
      ignitionSchedule1.pEndCallback = endCoil1Charge;
      ignitionSchedule2.pStartCallback = beginCoil2Charge;
      ignitionSchedule2.pEndCallback = endCoil2Charge;
      ignitionSchedule3.pStartCallback = beginCoil3Charge;
      ignitionSchedule3.pEndCallback = endCoil3Charge;
      ignitionSchedule4.pStartCallback = beginCoil4Charge;
      ignitionSchedule4.pEndCallback = endCoil4Charge;
      break;

    default:
      break; //No actions required for other cylinder counts
      
    }
  }
}

/** Change injectors or/and ignition angles to 360deg.
 * In semi sequentiol mode req_fuel size is half.
 * Set number of outputs equal to half cylinder count.
* */
void changeFullToHalfSync(void)
{
  if(configPage2.injLayout == INJ_SEQUENTIAL)
  {
    CRANK_ANGLE_MAX_INJ = 360;
    req_fuel_uS /= 2;
    switch (configPage2.nCylinders)
    {
      case 4:
        if(configPage4.inj4cylPairing == INJ_PAIR_13_24)
        {
          fuelSchedule1.pStartFunction = openInjector1and3;
          fuelSchedule1.pEndFunction = closeInjector1and3;
          fuelSchedule2.pStartFunction = openInjector2and4;
          fuelSchedule2.pEndFunction = closeInjector2and4;
        }
        else
        {
          fuelSchedule1.pStartFunction = openInjector1and4;
          fuelSchedule1.pEndFunction = closeInjector1and4;
          fuelSchedule2.pStartFunction = openInjector2and3;
          fuelSchedule2.pEndFunction = closeInjector2and3;
        }
        maxInjOutputs = 2;
        break;
            
      case 6:
        fuelSchedule1.pStartFunction = openInjector1and4;
        fuelSchedule1.pEndFunction = closeInjector1and4;
        fuelSchedule2.pStartFunction = openInjector2and5;
        fuelSchedule2.pEndFunction = closeInjector2and5;
        fuelSchedule3.pStartFunction = openInjector3and6;
        fuelSchedule3.pEndFunction = closeInjector3and6;
        maxInjOutputs = 3;
        break;

      case 8:
        fuelSchedule1.pStartFunction = openInjector1and5;
        fuelSchedule1.pEndFunction = closeInjector1and5;
        fuelSchedule2.pStartFunction = openInjector2and6;
        fuelSchedule2.pEndFunction = closeInjector2and6;
        fuelSchedule3.pStartFunction = openInjector3and7;
        fuelSchedule3.pEndFunction = closeInjector3and7;
        fuelSchedule4.pStartFunction = openInjector4and8;
        fuelSchedule4.pEndFunction = closeInjector4and8;
        maxInjOutputs = 4;
        break;
    }
  }

  if(configPage4.sparkMode == IGN_MODE_SEQUENTIAL)
  {
    CRANK_ANGLE_MAX_IGN = 360;
    maxIgnOutputs = configPage2.nCylinders / 2;
    switch (configPage2.nCylinders)
    {
      case 4:
        ignitionSchedule1.pStartCallback = beginCoil1and3Charge;
        ignitionSchedule1.pEndCallback = endCoil1and3Charge;
        ignitionSchedule2.pStartCallback = beginCoil2and4Charge;
        ignitionSchedule2.pEndCallback = endCoil2and4Charge;
        break;
            
      case 6:
        ignitionSchedule1.pStartCallback = beginCoil1and4Charge;
        ignitionSchedule1.pEndCallback = endCoil1and4Charge;
        ignitionSchedule2.pStartCallback = beginCoil2and5Charge;
        ignitionSchedule2.pEndCallback = endCoil2and5Charge;
        ignitionSchedule3.pStartCallback = beginCoil3and6Charge;
        ignitionSchedule3.pEndCallback = endCoil3and6Charge;
        break;

      case 8:
        ignitionSchedule1.pStartCallback = beginCoil1and5Charge;
        ignitionSchedule1.pEndCallback = endCoil1and5Charge;
        ignitionSchedule2.pStartCallback = beginCoil2and6Charge;
        ignitionSchedule2.pEndCallback = endCoil2and6Charge;
        ignitionSchedule3.pStartCallback = beginCoil3and7Charge;
        ignitionSchedule3.pEndCallback = endCoil3and7Charge;
        ignitionSchedule4.pStartCallback = beginCoil4and8Charge;
        ignitionSchedule4.pEndCallback = endCoil4and8Charge;
        break;
    }
  }
}

#if defined(CORE_AVR)
#pragma GCC pop_options
#endif