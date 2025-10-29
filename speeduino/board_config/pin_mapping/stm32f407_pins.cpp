/**
 * @file stm32f407_pins.cpp
 * @brief STM32F407 pin mapping implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * PRESERVES 100% LOGIC from init.cpp case 60 (lines 2443-2622)
 * Original code migrated without modifications to behavior
 *
 * This module configures ALL pin assignments for the STM32F407 Black F407VE board:
 * - Pin definitions for experimental board Tjeerd
 * - Black F407VE wiki.stm32duino.com/index.php?title=STM32F407
 * - https://github.com/Tjeerdie/SPECTRE/tree/master/SPECTRE_V0.5
 */

#include "stm32f407_pins.h"
#include "../../globals.h"

void stm32f407ConfigurePins(void)
{
  #if defined(STM32F407xx)
    //Pin definitions for experimental board Tjeerd
    //Black F407VE wiki.stm32duino.com/index.php?title=STM32F407
    //https://github.com/Tjeerdie/SPECTRE/tree/master/SPECTRE_V0.5

    //******************************************
    //******** PORTA CONNECTIONS ***************
    //******************************************
    // = PA0; //Wakeup ADC123
    // = PA1; //ADC123
    // = PA2; //ADC123
    // = PA3; //ADC123
    // = PA4; //ADC12
    // = PA5; //ADC12
    // = PA6; //ADC12 LED_BUILTIN_1
    // = PA7; //ADC12 LED_BUILTIN_2
    pinCoil3 = PA8;
    // = PA9;  //TXD1=Bluetooth module
    // = PA10; //RXD1=Bluetooth module
    // = PA11; //(DO NOT USE FOR SPEEDUINO) USB
    // = PA12; //(DO NOT USE FOR SPEEDUINO) USB
    // = PA13;  //(DO NOT USE FOR SPEEDUINO) NOT ON GPIO - DEBUG ST-LINK
    // = PA14;  //(DO NOT USE FOR SPEEDUINO) NOT ON GPIO - DEBUG ST-LINK
    // = PA15;  //(DO NOT USE FOR SPEEDUINO) NOT ON GPIO - DEBUG ST-LINK

    //******************************************
    //******** PORTB CONNECTIONS ***************
    //******************************************
    // = PB0;  //(DO NOT USE FOR SPEEDUINO) ADC123 - SPI FLASH CHIP CS pin
    pinBaro = PB1; //ADC12
    // = PB2;  //(DO NOT USE FOR SPEEDUINO) BOOT1
    // = PB3;  //(DO NOT USE FOR SPEEDUINO) SPI1_SCK FLASH CHIP
    // = PB4;  //(DO NOT USE FOR SPEEDUINO) SPI1_MISO FLASH CHIP
    // = PB5;  //(DO NOT USE FOR SPEEDUINO) SPI1_MOSI FLASH CHIP
    // = PB6;  //NRF_CE
    pinCoil6 = PB7;  //NRF_CS
    // = PB8;  //NRF_IRQ
    pinCoil2 = PB9; //
    // = PB9;  //
    // = PB10; //TXD3
    // = PB11; //RXD3
    // = PB12; //
    // = PB13;  //SPI2_SCK
    // = PB14;  //SPI2_MISO
    // = PB15;  //SPI2_MOSI

    //******************************************
    //******** PORTC CONNECTIONS ***************
    //******************************************
    pinIAT = PC0; //ADC123
    pinTPS = PC1; //ADC123
    pinMAP = PC2; //ADC123
    pinCLT = PC3; //ADC123
    pinO2 = PC4; //ADC12
    pinBat = PC5;  //ADC12
    pinBoost = PC6; //
    pinIdle1 = PC7; //
    // = PC8;  //(DO NOT USE FOR SPEEDUINO) - SDIO_D0
    // = PC9;  //(DO NOT USE FOR SPEEDUINO) - SDIO_D1
    // = PC10;  //(DO NOT USE FOR SPEEDUINO) - SDIO_D2
    // = PC11;  //(DO NOT USE FOR SPEEDUINO) - SDIO_D3
    // = PC12;  //(DO NOT USE FOR SPEEDUINO) - SDIO_SCK
    pinTachOut = PC13; //
    // = PC14;  //(DO NOT USE FOR SPEEDUINO) - OSC32_IN
    // = PC15;  //(DO NOT USE FOR SPEEDUINO) - OSC32_OUT

    //******************************************
    //******** PORTD CONNECTIONS ***************
    //******************************************
    // = PD0;  //CANRX
    // = PD1;  //CANTX
    // = PD2;  //(DO NOT USE FOR SPEEDUINO) - SDIO_CMD
    pinIdle2 = PD3; //
    // = PD4;  //
    pinFlex = PD4;
    // = PD5; //TXD2
    // = PD6;  //RXD2
    pinCoil1 = PD7; //
    // = PD7;  //
    // = PD8;  //
    pinCoil5 = PD9;//
    pinCoil4 = PD10;//
    // = PD11;  //
    pinInjector1 = PD12; //
    pinInjector2 = PD13; //
    pinInjector3 = PD14; //
    pinInjector4 = PD15; //

    //******************************************
    //******** PORTE CONNECTIONS ***************
    //******************************************
    pinTrigger = PE0; //
    pinTrigger2 = PE1; //
    pinStepperEnable = PE2; //
    pinFuelPump = PE3; //ONBOARD KEY1
    // = PE4;  //ONBOARD KEY2
    pinStepperStep = PE5; //
    pinFan = PE6; //
    pinStepperDir = PE7; //
    // = PE8;  //
    pinInjector5 = PE9; //
    // = PE10;  //
    pinInjector6 = PE11; //
    // = PE12; //
    pinInjector8 = PE13; //
    pinInjector7 = PE14; //
    // = PE15;  //

  #elif (defined(STM32F411xE) || defined(STM32F401xC))
    //pins PA12, PA11 are used for USB or CAN couldn't be used for GPIO
    //PB2 can't be used as input because is BOOT pin
    pinInjector1 = PB7; //Output pin injector 1 is on
    pinInjector2 = PB6; //Output pin injector 2 is on
    pinInjector3 = PB5; //Output pin injector 3 is on
    pinInjector4 = PB4; //Output pin injector 4 is on
    pinCoil1 = PB9; //Pin for coil 1
    pinCoil2 = PB8; //Pin for coil 2
    pinCoil3 = PB3; //Pin for coil 3
    pinCoil4 = PA15; //Pin for coil 4
    pinTPS = A2;//TPS input pin
    pinMAP = A3; //MAP sensor pin
    pinIAT = A0; //IAT sensor pin
    pinCLT = A1; //CLS sensor pin
    pinO2 = A8; //O2 Sensor pin
    pinBat = A4; //Battery reference voltage pin
    pinBaro = pinMAP;
    pinTachOut = PB1; //Tacho output pin  (Goes to ULN2803)
    pinIdle1 = PB2; //Single wire idle control
    pinIdle2 = PB10; //2 wire idle control
    pinBoost = PA6; //Boost control
    pinStepperDir = PB10; //Direction pin  for DRV8825 driver
    pinStepperStep = PB2; //Step pin for DRV8825 driver
    pinFuelPump = PA8; //Fuel pump output
    pinFan = PA5; //Pin for the fan output (Goes to ULN2803)

    //external interrupt enabled pins
    pinFlex = PC14; // Flex sensor (Must be external interrupt enabled)
    pinTrigger = PC13; //The CAS pin also led pin so bad idea
    pinTrigger2 = PC15; //The Cam Sensor pin

  #elif defined(CORE_STM32)
    //blue pill wiki.stm32duino.com/index.php?title=Blue_Pill
    //Maple mini wiki.stm32duino.com/index.php?title=Maple_Mini
    //pins PA12, PA11 are used for USB or CAN couldn't be used for GPIO
    //PB2 can't be used as input because is BOOT pin
    pinInjector1 = PB7; //Output pin injector 1 is on
    pinInjector2 = PB6; //Output pin injector 2 is on
    pinInjector3 = PB5; //Output pin injector 3 is on
    pinInjector4 = PB4; //Output pin injector 4 is on
    pinCoil1 = PB3; //Pin for coil 1
    pinCoil2 = PA15; //Pin for coil 2
    pinCoil3 = PA14; //Pin for coil 3
    pinCoil4 = PA9; //Pin for coil 4
    pinCoil5 = PA8; //Pin for coil 5
    pinTPS = A0; //TPS input pin
    pinMAP = A1; //MAP sensor pin
    pinIAT = A2; //IAT sensor pin
    pinCLT = A3; //CLS sensor pin
    pinO2 = A4; //O2 Sensor pin
    pinBat = A5; //Battery reference voltage pin
    pinBaro = pinMAP;
    pinIdle1 = PB2; //Single wire idle control
    pinIdle2 = PA2; //2 wire idle control
    pinBoost = PA1; //Boost control
    pinVVT_1 = PA0; //Default VVT output
    pinVVT_2 = PA2; //Default VVT2 output
    pinStepperDir = PC15; //Direction pin  for DRV8825 driver
    pinStepperStep = PC14; //Step pin for DRV8825 driver
    pinStepperEnable = PC13; //Enable pin for DRV8825
    pinDisplayReset = PB2; // OLED reset pin
    pinFan = PB1; //Pin for the fan output
    pinFuelPump = PB11; //Fuel pump output
    pinTachOut = PB10; //Tacho output pin
    //external interrupt enabled pins
    pinFlex = PB8; // Flex sensor (Must be external interrupt enabled)
    pinTrigger = PA10; //The CAS pin
    pinTrigger2 = PA13; //The Cam Sensor pin

  #endif
}
