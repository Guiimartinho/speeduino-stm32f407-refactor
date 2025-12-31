#ifndef STM32OFFICIAL_H
#define STM32OFFICIAL_H
#include <Arduino.h>
#if defined(STM32_CORE_VERSION_MAJOR)
#include <HardwareTimer.h>
#include <HardwareSerial.h>
#include "STM32RTC.h"
#include <SPI.h>

#ifndef PLATFORMIO
  #ifndef USBCON
    #error "USBCON must be defined in boards.txt"
  #endif
  #ifndef USBD_USE_CDC
    #error "USBD_USE_CDC must be defined in boards.txt"
  #endif
#endif

#if defined(STM32F1)
  #include "stm32f1xx_ll_tim.h"
#elif defined(STM32F3)
  #include "stm32f3xx_ll_tim.h"
#elif defined(STM32F4)
  #include "stm32f4xx_ll_tim.h"
  #if defined(STM32F407xx) && !defined(HAL_CAN_MODULE_ENABLED)
    #warning "CAN module is not enabled. Internal CAN will NOT be available"
  #endif
#else /*Default should be STM32F4*/
  #include "stm32f4xx_ll_tim.h"
#endif
/*
***********************************************************************************************************
* General
*/
#define COMPARE_TYPE uint16_t
#define SERIAL_BUFFER_SIZE 517 //Size of the serial buffer used by new comms protocol. For SD transfers this must be at least 512 + 1 (flag) + 4 (sector)
#define FPU_MAX_SIZE 32 //Size of the FPU buffer. 0 means no FPU.
#define TIMER_RESOLUTION 4

//Select one for EEPROM,the default is EEPROM emulation on internal flash.
//#define SRAM_AS_EEPROM /*Use 4K battery backed SRAM, requires a 3V continuous source (like battery) connected to Vbat pin */
//#define USE_SPI_EEPROM PB0 /*Use M25Qxx SPI flash on BlackF407VE*/
//#define FRAM_AS_EEPROM /*Use FRAM like FM25xxx, MB85RSxxx or any SPI compatible */

#ifndef word
  #define word(h, l) ((h << 8) | l) //word() function not defined for this platform in the main library
#endif

#if defined(ARDUINO_BLUEPILL_F103C8) || defined(ARDUINO_BLUEPILL_F103CB) \
  || defined(ARDUINO_BLACKPILL_F401CC) || defined(ARDUINO_BLACKPILL_F411CE)
  //STM32 Pill boards
  #ifndef NUM_DIGITAL_PINS
    #define NUM_DIGITAL_PINS 35
  #endif
  #ifndef LED_BUILTIN
    #define LED_BUILTIN PB1 //Maple Mini
  #endif
#elif defined(STM32F407xx)
  #ifndef NUM_DIGITAL_PINS
    #define NUM_DIGITAL_PINS 75
  #endif
#endif

//Specific mode for Bluepill due to its small flash size. This disables a number of strings from being compiled into the flash
#if defined(MCU_STM32F103C8) || defined(MCU_STM32F103CB)
  #define SMALL_FLASH_MODE
#endif

#define BOARD_MAX_DIGITAL_PINS NUM_DIGITAL_PINS
#define BOARD_MAX_IO_PINS NUM_DIGITAL_PINS
#if __GNUC__ < 7 //Already included on GCC 7
extern "C" char* sbrk(int incr); //Used to freeRam
#endif
#ifndef digitalPinToInterrupt
inline uint32_t  digitalPinToInterrupt(uint32_t Interrupt_pin) { return Interrupt_pin; } //This isn't included in the stm32duino libs (yet)
#endif

#if defined(USER_BTN)
  #define EEPROM_RESET_PIN USER_BTN //onboard key0 for black STM32F407 boards and blackpills, keep pressed during boot to reset eeprom
#endif

#if defined(STM32F407xx)
  //Comment out this to disable SD logging for STM32 if needed. Currently SD logging for STM32 is experimental feature for F407.
  #define SD_LOGGING
#endif

#if defined(SD_LOGGING)
  #define RTC_ENABLED
  //SD logging with STM32 uses SD card in SPI mode, because used SD library doesn't support SDIO implementation. By default SPI3 is used that uses same pins as SDIO also, but in different order.
  extern SPIClass SD_SPI; //SPI3_MOSI, SPI3_MISO, SPI3_SCK
  #define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(50), &SD_SPI)
  //Alternatively same SPI bus can be used as there is for SPI flash. But this is not recommended due to slower speed and other possible problems.
  //#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(50), &SPI_for_flash)
#endif

//When building for Black board Serial1 is instantiated,building generic STM32F4x7 has serial2 and serial 1 must be done here
#if SERIAL_UART_INSTANCE==2
HardwareSerial Serial1(PA10, PA9);
#endif

extern STM32RTC& rtc;

void initBoard();
uint16_t freeRam();
void doSystemReset();
void jumpToBootloader();
uint8_t getSystemTemp();

#if defined(ARDUINO_BLUEPILL_F103C8) || defined(ARDUINO_BLUEPILL_F103CB) \
 || defined(ARDUINO_BLACKPILL_F401CC) || defined(ARDUINO_BLACKPILL_F411CE)
  #define pinIsReserved(pin)  ( ((pin) == PA11) || ((pin) == PA12) || ((pin) == PC14) || ((pin) == PC15) )

  #ifndef PB11 //Hack for F4 BlackPills
    #define PB11 PB10
  #endif
  //Hack to allow compilation on small STM boards
  #ifndef A10
    #define A10  PA0
    #define A11  PA1
    #define A12  PA2
    #define A13  PA3
    #define A14  PA4
    #define A15  PA5
  #endif
#else
  #ifdef USE_SPI_EEPROM
    #define pinIsReserved(pin)  ( ((pin) == PA11) || ((pin) == PA12) || ((pin) == PB3) || ((pin) == PB4) || ((pin) == PB5) || ((pin) == USE_SPI_EEPROM) ) //Forbidden pins like USB
  #else
    #define pinIsReserved(pin)  ( ((pin) == PA11) || ((pin) == PA12) || ((pin) == PB3) || ((pin) == PB4) || ((pin) == PB5) || ((pin) == PB0) ) //Forbidden pins like USB
  #endif
#endif

#define PWM_FAN_AVAILABLE

#ifndef LED_BUILTIN
  #define LED_BUILTIN PA7
#endif

/*
***********************************************************************************************************
* EEPROM emulation
*/
#if defined(SRAM_AS_EEPROM)
    #define EEPROM_LIB_H "src/BackupSram/BackupSramAsEEPROM.h"
    typedef uint16_t eeprom_address_t;
    #include EEPROM_LIB_H
    extern BackupSramAsEEPROM EEPROM;

#elif defined(USE_SPI_EEPROM)
    #define EEPROM_LIB_H "src/SPIAsEEPROM/SPIAsEEPROM.h"
    typedef uint16_t eeprom_address_t;
    #include EEPROM_LIB_H
    extern SPIClass SPI_for_flash; //SPI1_MOSI, SPI1_MISO, SPI1_SCK

    //windbond W25Q16 SPI flash EEPROM emulation
    extern EEPROM_Emulation_Config EmulatedEEPROMMconfig;
    extern Flash_SPI_Config SPIconfig;
    extern SPI_EEPROM_Class EEPROM;

#elif defined(FRAM_AS_EEPROM) //https://github.com/VitorBoss/FRAM
    #define EEPROM_LIB_H "src/FRAM/Fram.h"
    typedef uint16_t eeprom_address_t;
    #include EEPROM_LIB_H
    #if defined(STM32F407xx)
      extern FramClass EEPROM; /*(mosi, miso, sclk, ssel, clockspeed) 31/01/2020*/
    #else
      extern FramClass EEPROM; //Blue/Black Pills
    #endif

#else //default case, internal flash as EEPROM
  #define EEPROM_LIB_H "src/SPIAsEEPROM/SPIAsEEPROM.h"
  typedef uint16_t eeprom_address_t;
  #include EEPROM_LIB_H
    extern InternalSTM32F4_EEPROM_Class EEPROM;
  #if defined(STM32F401xC)
    #define SMALL_FLASH_MODE
  #endif
#endif


#define RTC_LIB_H "STM32RTC.h"

/*
***********************************************************************************************************
* Schedules
* Timers Table for STM32F1
*   TIMER1    TIMER2    TIMER3    TIMER4
* 1 - FAN   1 - INJ1  1 - IGN1  1 - oneMSInterval
* 2 - BOOST 2 - INJ2  2 - IGN2  2 -
* 3 - VVT   3 - INJ3  3 - IGN3  3 -
* 4 - IDLE  4 - INJ4  4 - IGN4  4 -
*
* Timers Table for STM32F4 (STANDARD)
*   TIMER1  |  TIMER2  |  TIMER3  |  TIMER4  |  TIMER5  |  TIMER11
* 1 - FAN   |1 - IGN1  |1 - INJ1  |1 - IGN5  |1 - INJ5  |1 - oneMSInterval
* 2 - BOOST |2 - IGN2  |2 - INJ2  |2 - IGN6  |2 - INJ6  |
* 3 - VVT   |3 - IGN3  |3 - INJ3  |3 - IGN7  |3 - INJ7  |
* 4 - IDLE  |4 - IGN4  |4 - INJ4  |4 - IGN8  |4 - INJ8  |
*
* Timers Table for STM32F4 (SCG-ECU 2.0 CUSTOM)
*   TIMER1  |  TIMER4  |  TIMER12  |  TIMER13 (Software PWM)  |  TIMER11
* 1 - INJ1  |1 - IGN1  |1 - IGN4   |   IGN5 (PD8 - Software)  |1 - oneMSInterval
* 2 - INJ2  |2 - IGN2  |2 - IGN3   |   IGN7 (PD11 - Software) |
* 3 - INJ3  |3 - IGN6  |           |                          |
* 4 - INJ4  |4 - IGN8  |           |                          |
* 5 - INJ5  |          |           |                          |
* 6 - INJ6  |          |           |                          |
* 7 - INJ7  |          |           |                          |
* 8 - INJ8  |          |           |                          |
*
* NOTE: SCG-ECU 2.0 uses TIM1 for ALL injectors (PE8-15 pins)
*       IGN5 and IGN7 use software PWM (no hardware timer on PD8, PD11)
*/
#define MAX_TIMER_PERIOD 262140UL //The longest period of time (in uS) that the timer can permit (IN this case it is 65535 * 4, as each timer tick is 4uS)
#define uS_TO_TIMER_COMPARE(uS1) ((uS1) >> 2) //Converts a given number of uS into the required number of timer ticks until that time has passed

// ============================================================================
// SCG-ECU 2.0 CUSTOM TIMER ALLOCATION
// ============================================================================
#if defined(BOARD_SCG_ECU_20)

// --- INJECTORS (TIM1 - ALL 8 CHANNELS) ---
// PE8-15 pins are on TIM1, not TIM3/TIM5
#define FUEL1_COUNTER (TIM1)->CNT
#define FUEL2_COUNTER (TIM1)->CNT
#define FUEL3_COUNTER (TIM1)->CNT
#define FUEL4_COUNTER (TIM1)->CNT
#define FUEL5_COUNTER (TIM1)->CNT
#define FUEL6_COUNTER (TIM1)->CNT
#define FUEL7_COUNTER (TIM1)->CNT
#define FUEL8_COUNTER (TIM1)->CNT

#define FUEL1_COMPARE (TIM1)->CCR1  // PE15
#define FUEL2_COMPARE (TIM1)->CCR4  // PE14
#define FUEL3_COMPARE (TIM1)->CCR3  // PE13
#define FUEL4_COMPARE (TIM1)->CCR2  // PE12 (complementary CH2N)
#define FUEL5_COMPARE (TIM1)->CCR2  // PE11 (shares CCR2)
#define FUEL6_COMPARE (TIM1)->CCR2  // PE10 (complementary CH2N, shares CCR2)
#define FUEL7_COMPARE (TIM1)->CCR1  // PE9 (shares CCR1)
#define FUEL8_COMPARE (TIM1)->CCR1  // PE8 (complementary CH1N, shares CCR1)

// --- IGNITION (TIM4 + TIM12 + Software PWM) ---
// IGN1,2,6,8 on TIM4 (PD9-13)
// IGN3,4 on TIM12 (PB14-15)
// IGN5,7 on SOFTWARE PWM (PD8, PD11 - no hardware timer!)

#define IGN1_COUNTER  (TIM4)->CNT   // PD12
#define IGN2_COUNTER  (TIM4)->CNT   // PD13
#define IGN3_COUNTER  (TIM12)->CNT  // PB15
#define IGN4_COUNTER  (TIM12)->CNT  // PB14
// IGN5 uses software PWM (no hardware timer) - provide dummy macro for compilation
#define IGN5_COUNTER  (TIM4)->CNT   // Dummy (software PWM)
#define IGN6_COUNTER  (TIM4)->CNT   // PD9
// IGN7 uses software PWM (no hardware timer) - provide dummy macro for compilation
#define IGN7_COUNTER  (TIM4)->CNT   // Dummy (software PWM)
#define IGN8_COUNTER  (TIM4)->CNT   // PD10

#define IGN1_COMPARE (TIM4)->CCR1   // PD12
#define IGN2_COMPARE (TIM4)->CCR2   // PD13
#define IGN3_COMPARE (TIM12)->CCR2  // PB15
#define IGN4_COMPARE (TIM12)->CCR1  // PB14
// IGN5 uses software PWM (no CCR register) - provide dummy macro for compilation
#define IGN5_COMPARE (TIM4)->CCR1   // Dummy (software PWM)
#define IGN6_COMPARE (TIM4)->CCR1   // PD9 (shares CCR1 with IGN1)
// IGN7 uses software PWM (no CCR register) - provide dummy macro for compilation
#define IGN7_COMPARE (TIM4)->CCR2   // Dummy (software PWM)
#define IGN8_COMPARE (TIM4)->CCR2   // PD10 (shares CCR2 with IGN2)

// --- TIMER ENABLE/DISABLE FUNCTIONS ---
// FUEL channels on TIM1
static inline void FUEL1_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC1; (TIM1)->DIER |= TIM_DIER_CC1IE;}
static inline void FUEL2_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC4; (TIM1)->DIER |= TIM_DIER_CC4IE;}
static inline void FUEL3_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC3; (TIM1)->DIER |= TIM_DIER_CC3IE;}
static inline void FUEL4_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC2; (TIM1)->DIER |= TIM_DIER_CC2IE;}
static inline void FUEL5_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC2; (TIM1)->DIER |= TIM_DIER_CC2IE;}
static inline void FUEL6_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC2; (TIM1)->DIER |= TIM_DIER_CC2IE;}
static inline void FUEL7_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC1; (TIM1)->DIER |= TIM_DIER_CC1IE;}
static inline void FUEL8_TIMER_ENABLE(void) {(TIM1)->CR1 |= TIM_CR1_CEN; (TIM1)->SR = ~TIM_FLAG_CC1; (TIM1)->DIER |= TIM_DIER_CC1IE;}

static inline void FUEL1_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC1IE;}
static inline void FUEL2_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC4IE;}
static inline void FUEL3_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC3IE;}
static inline void FUEL4_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC2IE;}
static inline void FUEL5_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC2IE;}
static inline void FUEL6_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC2IE;}
static inline void FUEL7_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC1IE;}
static inline void FUEL8_TIMER_DISABLE(void) {(TIM1)->DIER &= ~TIM_DIER_CC1IE;}

// IGN1,2,6,8 on TIM4
static inline void IGN1_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC1; (TIM4)->DIER |= TIM_DIER_CC1IE;}
static inline void IGN2_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC2; (TIM4)->DIER |= TIM_DIER_CC2IE;}
static inline void IGN6_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC1; (TIM4)->DIER |= TIM_DIER_CC1IE;}
static inline void IGN8_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC2; (TIM4)->DIER |= TIM_DIER_CC2IE;}

static inline void IGN1_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC1IE;}
static inline void IGN2_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC2IE;}
static inline void IGN6_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC1IE;}
static inline void IGN8_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC2IE;}

// IGN3,4 on TIM12
static inline void IGN3_TIMER_ENABLE(void)  {(TIM12)->CR1 |= TIM_CR1_CEN; (TIM12)->SR = ~TIM_FLAG_CC2; (TIM12)->DIER |= TIM_DIER_CC2IE;}
static inline void IGN4_TIMER_ENABLE(void)  {(TIM12)->CR1 |= TIM_CR1_CEN; (TIM12)->SR = ~TIM_FLAG_CC1; (TIM12)->DIER |= TIM_DIER_CC1IE;}

static inline void IGN3_TIMER_DISABLE(void)  {(TIM12)->DIER &= ~TIM_DIER_CC2IE;}
static inline void IGN4_TIMER_DISABLE(void)  {(TIM12)->DIER &= ~TIM_DIER_CC1IE;}

// IGN5,7 use SOFTWARE PWM - no hardware timer
// These are handled by software_pwm_ignition.cpp via ignition scheduler
// We provide stub functions to maintain API compatibility
static inline void IGN5_TIMER_ENABLE(void)  { /* Software PWM - no hardware timer */ }
static inline void IGN7_TIMER_ENABLE(void)  { /* Software PWM - no hardware timer */ }
static inline void IGN5_TIMER_DISABLE(void) { /* Software PWM - no hardware timer */ }
static inline void IGN7_TIMER_DISABLE(void) { /* Software PWM - no hardware timer */ }

#else
// ============================================================================
// STANDARD STM32F4 TIMER ALLOCATION (Original Speeduino)
// ============================================================================

#define FUEL1_COUNTER (TIM3)->CNT
#define FUEL2_COUNTER (TIM3)->CNT
#define FUEL3_COUNTER (TIM3)->CNT
#define FUEL4_COUNTER (TIM3)->CNT

#define FUEL1_COMPARE (TIM3)->CCR1
#define FUEL2_COMPARE (TIM3)->CCR2
#define FUEL3_COMPARE (TIM3)->CCR3
#define FUEL4_COMPARE (TIM3)->CCR4

#define IGN1_COUNTER  (TIM2)->CNT
#define IGN2_COUNTER  (TIM2)->CNT
#define IGN3_COUNTER  (TIM2)->CNT
#define IGN4_COUNTER  (TIM2)->CNT

#define IGN1_COMPARE (TIM2)->CCR1
#define IGN2_COMPARE (TIM2)->CCR2
#define IGN3_COMPARE (TIM2)->CCR3
#define IGN4_COMPARE (TIM2)->CCR4

#define FUEL5_COUNTER (TIM5)->CNT
#define FUEL6_COUNTER (TIM5)->CNT
#define FUEL7_COUNTER (TIM5)->CNT
#define FUEL8_COUNTER (TIM5)->CNT

#define FUEL5_COMPARE (TIM5)->CCR1
#define FUEL6_COMPARE (TIM5)->CCR2
#define FUEL7_COMPARE (TIM5)->CCR3
#define FUEL8_COMPARE (TIM5)->CCR4

#define IGN5_COUNTER  (TIM4)->CNT
#define IGN6_COUNTER  (TIM4)->CNT
#define IGN7_COUNTER  (TIM4)->CNT
#define IGN8_COUNTER  (TIM4)->CNT

#define IGN5_COMPARE (TIM4)->CCR1
#define IGN6_COMPARE (TIM4)->CCR2
#define IGN7_COMPARE (TIM4)->CCR3
#define IGN8_COMPARE (TIM4)->CCR4


static inline void FUEL1_TIMER_ENABLE(void) {(TIM3)->CR1 |= TIM_CR1_CEN; (TIM3)->SR = ~TIM_FLAG_CC1; (TIM3)->DIER |= TIM_DIER_CC1IE;}
static inline void FUEL2_TIMER_ENABLE(void) {(TIM3)->CR1 |= TIM_CR1_CEN; (TIM3)->SR = ~TIM_FLAG_CC2; (TIM3)->DIER |= TIM_DIER_CC2IE;}
static inline void FUEL3_TIMER_ENABLE(void) {(TIM3)->CR1 |= TIM_CR1_CEN; (TIM3)->SR = ~TIM_FLAG_CC3; (TIM3)->DIER |= TIM_DIER_CC3IE;}
static inline void FUEL4_TIMER_ENABLE(void) {(TIM3)->CR1 |= TIM_CR1_CEN; (TIM3)->SR = ~TIM_FLAG_CC4; (TIM3)->DIER |= TIM_DIER_CC4IE;}

static inline void FUEL1_TIMER_DISABLE(void) {(TIM3)->DIER &= ~TIM_DIER_CC1IE;}
static inline void FUEL2_TIMER_DISABLE(void) {(TIM3)->DIER &= ~TIM_DIER_CC2IE;}
static inline void FUEL3_TIMER_DISABLE(void) {(TIM3)->DIER &= ~TIM_DIER_CC3IE;}
static inline void FUEL4_TIMER_DISABLE(void) {(TIM3)->DIER &= ~TIM_DIER_CC4IE;}

static inline void IGN1_TIMER_ENABLE(void)  {(TIM2)->CR1 |= TIM_CR1_CEN; (TIM2)->SR = ~TIM_FLAG_CC1; (TIM2)->DIER |= TIM_DIER_CC1IE;}
static inline void IGN2_TIMER_ENABLE(void)  {(TIM2)->CR1 |= TIM_CR1_CEN; (TIM2)->SR = ~TIM_FLAG_CC2; (TIM2)->DIER |= TIM_DIER_CC2IE;}
static inline void IGN3_TIMER_ENABLE(void)  {(TIM2)->CR1 |= TIM_CR1_CEN; (TIM2)->SR = ~TIM_FLAG_CC3; (TIM2)->DIER |= TIM_DIER_CC3IE;}
static inline void IGN4_TIMER_ENABLE(void)  {(TIM2)->CR1 |= TIM_CR1_CEN; (TIM2)->SR = ~TIM_FLAG_CC4; (TIM2)->DIER |= TIM_DIER_CC4IE;}

static inline void IGN1_TIMER_DISABLE(void)  {(TIM2)->DIER &= ~TIM_DIER_CC1IE;}
static inline void IGN2_TIMER_DISABLE(void)  {(TIM2)->DIER &= ~TIM_DIER_CC2IE;}
static inline void IGN3_TIMER_DISABLE(void)  {(TIM2)->DIER &= ~TIM_DIER_CC3IE;}
static inline void IGN4_TIMER_DISABLE(void)  {(TIM2)->DIER &= ~TIM_DIER_CC4IE;}

static inline void FUEL5_TIMER_ENABLE(void) {(TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->SR = ~TIM_FLAG_CC1; (TIM5)->DIER |= TIM_DIER_CC1IE;}
static inline void FUEL6_TIMER_ENABLE(void) {(TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->SR = ~TIM_FLAG_CC2; (TIM5)->DIER |= TIM_DIER_CC2IE;}
static inline void FUEL7_TIMER_ENABLE(void) {(TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->SR = ~TIM_FLAG_CC3; (TIM5)->DIER |= TIM_DIER_CC3IE;}
static inline void FUEL8_TIMER_ENABLE(void) {(TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->CR1 |= TIM_CR1_CEN; (TIM5)->SR = ~TIM_FLAG_CC4; (TIM5)->DIER |= TIM_DIER_CC4IE;}

static inline void FUEL5_TIMER_DISABLE(void) {(TIM5)->DIER &= ~TIM_DIER_CC1IE;}
static inline void FUEL6_TIMER_DISABLE(void) {(TIM5)->DIER &= ~TIM_DIER_CC2IE;}
static inline void FUEL7_TIMER_DISABLE(void) {(TIM5)->DIER &= ~TIM_DIER_CC3IE;}
static inline void FUEL8_TIMER_DISABLE(void) {(TIM5)->DIER &= ~TIM_DIER_CC4IE;}

static inline void IGN5_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC1; (TIM4)->DIER |= TIM_DIER_CC1IE;}
static inline void IGN6_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC2; (TIM4)->DIER |= TIM_DIER_CC2IE;}
static inline void IGN7_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC3; (TIM4)->DIER |= TIM_DIER_CC3IE;}
static inline void IGN8_TIMER_ENABLE(void)  {(TIM4)->CR1 |= TIM_CR1_CEN; (TIM4)->SR = ~TIM_FLAG_CC4; (TIM4)->DIER |= TIM_DIER_CC4IE;}

static inline void IGN5_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC1IE;}
static inline void IGN6_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC2IE;}
static inline void IGN7_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC3IE;}
static inline void IGN8_TIMER_DISABLE(void)  {(TIM4)->DIER &= ~TIM_DIER_CC4IE;}

#endif // BOARD_SCG_ECU_20

/*
***********************************************************************************************************
* Auxiliaries
*/
#define ENABLE_BOOST_TIMER()  (TIM1)->SR = ~TIM_FLAG_CC2; (TIM1)->DIER |= TIM_DIER_CC2IE; (TIM1)->CR1 |= TIM_CR1_CEN;
#define DISABLE_BOOST_TIMER() (TIM1)->DIER &= ~TIM_DIER_CC2IE

#define ENABLE_VVT_TIMER()    (TIM1)->SR = ~TIM_FLAG_CC3; (TIM1)->DIER |= TIM_DIER_CC3IE; (TIM1)->CR1 |= TIM_CR1_CEN;
#define DISABLE_VVT_TIMER()   (TIM1)->DIER &= ~TIM_DIER_CC3IE

#define ENABLE_FAN_TIMER()  (TIM1)->SR = ~TIM_FLAG_CC1; (TIM1)->DIER |= TIM_DIER_CC1IE; (TIM1)->CR1 |= TIM_CR1_CEN;
#define DISABLE_FAN_TIMER() (TIM1)->DIER &= ~TIM_DIER_CC1IE

#define BOOST_TIMER_COMPARE   (TIM1)->CCR2
#define BOOST_TIMER_COUNTER   (TIM1)->CNT
#define VVT_TIMER_COMPARE     (TIM1)->CCR3
#define VVT_TIMER_COUNTER     (TIM1)->CNT
#define FAN_TIMER_COMPARE     (TIM1)->CCR1
#define FAN_TIMER_COUNTER     (TIM1)->CNT

/*
***********************************************************************************************************
* Idle
*/
#define IDLE_COUNTER   (TIM1)->CNT
#define IDLE_COMPARE   (TIM1)->CCR4

#define IDLE_TIMER_ENABLE()  (TIM1)->SR = ~TIM_FLAG_CC4; (TIM1)->DIER |= TIM_DIER_CC4IE; (TIM1)->CR1 |= TIM_CR1_CEN;
#define IDLE_TIMER_DISABLE() (TIM1)->DIER &= ~TIM_DIER_CC4IE

/*
***********************************************************************************************************
* Timers
*/

extern HardwareTimer Timer1;
extern HardwareTimer Timer2;
extern HardwareTimer Timer3;
extern HardwareTimer Timer4;
#if !defined(ARDUINO_BLUEPILL_F103C8) && !defined(ARDUINO_BLUEPILL_F103CB) //F103 just have 4 timers
extern HardwareTimer Timer5;
#if defined(TIM11)
extern HardwareTimer Timer11;
#elif defined(TIM7)
extern HardwareTimer Timer11;
#endif
#endif

#if ((STM32_CORE_VERSION_MINOR<=8) & (STM32_CORE_VERSION_MAJOR==1))
void oneMSInterval(HardwareTimer*);
void boostInterrupt(HardwareTimer*);
void fuelSchedule1Interrupt(HardwareTimer*);
void fuelSchedule2Interrupt(HardwareTimer*);
void fuelSchedule3Interrupt(HardwareTimer*);
void fuelSchedule4Interrupt(HardwareTimer*);
#if (INJ_CHANNELS >= 5)
void fuelSchedule5Interrupt(HardwareTimer*);
#endif
#if (INJ_CHANNELS >= 6)
void fuelSchedule6Interrupt(HardwareTimer*);
#endif
#if (INJ_CHANNELS >= 7)
void fuelSchedule7Interrupt(HardwareTimer*);
#endif
#if (INJ_CHANNELS >= 8)
void fuelSchedule8Interrupt(HardwareTimer*);
#endif
void idleInterrupt(HardwareTimer*);
void vvtInterrupt(HardwareTimer*);
void fanInterrupt(HardwareTimer*);
void ignitionSchedule1Interrupt(HardwareTimer*);
void ignitionSchedule2Interrupt(HardwareTimer*);
void ignitionSchedule3Interrupt(HardwareTimer*);
void ignitionSchedule4Interrupt(HardwareTimer*);
#if (IGN_CHANNELS >= 5)
void ignitionSchedule5Interrupt(HardwareTimer*);
#endif
#if (IGN_CHANNELS >= 6)
void ignitionSchedule6Interrupt(HardwareTimer*);
#endif
#if (IGN_CHANNELS >= 7)
void ignitionSchedule7Interrupt(HardwareTimer*);
#endif
#if (IGN_CHANNELS >= 8)
void ignitionSchedule8Interrupt(HardwareTimer*);
#endif
#endif //End core<=1.8

/*
***********************************************************************************************************
* CAN / Second serial
*/
#if defined(HAL_CAN_MODULE_ENABLED)
#define NATIVE_CAN_AVAILABLE
#include <src/STM32_CAN/STM32_CAN.h>
//This activates CAN1 interface on STM32, but it's named as Can0, because that's how Teensy implementation is done
extern STM32_CAN Can0;
#endif

#if defined(STM32GENERIC) // STM32GENERIC core
  #define SECONDARY_SERIAL_T SerialUART
#else //libmaple core aka STM32DUINO
  #define SECONDARY_SERIAL_T HardwareSerial
#endif

#endif //CORE_STM32
#endif //STM32_H
