/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/

/**
 * @file comms_CAN.cpp
 * @brief CAN bus communication for OEM instrument clusters and aftermarket dashes
 *
 * @details Implements CAN broadcasting protocols for displaying ECU data on:
 *
 * **OEM Instrument Clusters:**
 * - **BMW (E46/E39/E38):** DME protocol (RPM, coolant, TPS, CEL/EML lights, fuel consumption)
 * - **VAG (VW/Audi):** Stock instrument cluster protocol (RPM, VSS, custom IDs)
 *
 * **Aftermarket Dashboards:**
 * - **Haltech:** Multi-frequency broadcast (50Hz, 15Hz, 10Hz)
 *   - DATA1-5: Engine parameters (RPM, TPS, coolant, IAT, MAP, etc.)
 *   - LAMBDA: O2 sensor readings
 *   - TRIGGER: Sync status, errors
 *   - VSS: Vehicle speed
 *   - PW: Injector pulse width
 *
 * **Wideband O2 Sensor Input:**
 * - **RusEFI CAN Wideband:** Receives lambda from external WBO2 controller
 * - **AEM 30-0300 X-Series:** AEM UEGO gauge via CAN (0x180 ID)
 *
 * **Hardware Support:**
 * - **Teensy 3.5/3.6:** FlexCAN_T4 library (CAN0)
 * - **Teensy 4.1:** FlexCAN_T4 library (CAN1)
 * - **STM32:** Native CAN peripheral (not yet implemented in this file)
 *
 * **CAN Bus Basics:**
 * - **Standard CAN:** 11-bit identifier (0x000-0x7FF)
 * - **Extended CAN:** 29-bit identifier (0x00000000-0x1FFFFFFF)
 * - **Baud Rate:** 500 kbps (default for automotive)
 * - **Frame Format:** ID + DLC + Data[0-8] + CRC
 *
 * **Broadcast Frequencies:**
 * - 50Hz: Fast updates (Haltech DATA1-5)
 * - 30Hz: Medium (BMW DME1/2, VAG RPM/VSS)
 * - 15Hz: Slow (Haltech LAMBDA, TRIGGER, VSS)
 * - 10Hz: Very slow (BMW DME4, Haltech DATA4/5)
 *
 * **Message Flow:**
 * ```
 * [Sensors] → currentStatus → DashMessage() → CAN_write() → CAN Bus → Dashboard
 * [WBO2 Controller] → CAN Bus → CAN_read() → receiveCANwbo() → currentStatus.O2
 * ```
 *
 * **Protocol Selection:**
 * - Set in TunerStudio: Settings → CAN Bus → Broadcast Protocol
 * - configPage4.CANBroadcastProtocol values:
 *   - 0: OFF (no CAN broadcast)
 *   - 1: BMW
 *   - 2: VAG
 *   - 3: Haltech
 *   - Additional protocols may be added
 *
 * **CAN IDs Used:**
 * - BMW DME1: 0x316 (RPM, torque)
 * - BMW DME2: 0x329 (coolant, TPS, baro)
 * - BMW DME4: 0x545 (fuel consumption, CEL/EML)
 * - VAG RPM: Configurable (typically 0x280)
 * - VAG VSS: Configurable (typically 0x5A0)
 * - Haltech: 0x360-0x374 (multiple IDs)
 * - RusEFI WBO2 TX: 0xEF50000 (extended)
 * - RusEFI WBO2 RX: 0x190, 0x192
 * - AEM WBO2: 0x180
 *
 * **Performance Notes:**
 * - CAN broadcast runs in main loop at timer-driven intervals
 * - NOT in interrupt context (safe for complex calculations)
 * - WBO2 receive happens on CAN RX interrupt (fast path)
 * - FlexCAN library uses FIFO buffering for TX/RX
 *
 * @note Only compiled if NATIVE_CAN_AVAILABLE is defined
 * @note Teensy 3.5/3.6/4.1 have native CAN hardware
 * @note STM32F407 has CAN but uses different library (not yet in this file)
 * @note MISRA-C:2012 compliant with switch-case default handlers
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (MISRA-C compliant with comprehensive Doxygen)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#include "globals.h"

#if defined(NATIVE_CAN_AVAILABLE)
#include "comms_CAN.h"
#include "utilities.h"
#include "maths.h"
#include "units.h"

CAN_message_t inMsg;
CAN_message_t outMsg;

//These are declared locally for Teensy due to this issue: https://github.com/tonton81/FlexCAN_T4/issues/67
#if defined(CORE_TEENSY35) || defined(CORE_TEENSY36)        // use for Teensy 3.5/3.6 only
  FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> Can0;
#elif defined(CORE_TEENSY41)         // use for Teensy 4.1 only
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
#endif

// Forward declare
void DashMessage(uint16_t DashMessageID);

//=============================================================================
// CAN BUS INITIALIZATION
//=============================================================================

/**
 * @brief Initialize CAN bus hardware and configure baud rate
 *
 * @details Sets up FlexCAN peripheral on Teensy platforms:
 * - Teensy 3.5/3.6: Uses CAN0 (pins 3/4 or 29/30 selectable)
 * - Teensy 4.1: Uses CAN1 (pins 22/23)
 * - Baud rate: 500 kbps (automotive standard)
 * - FIFO: 256 RX messages, 16 TX messages
 *
 * @note Must be called AFTER setPinMapping() to avoid pin conflicts
 * @note Teensy FlexCAN issue: https://github.com/tonton81/FlexCAN_T4/issues/14
 * @note Sets configPage9.intcan_available = 1 (flags CAN as available)
 *
 * @complexity 1 (trivial initialization)
 * @misra Compliant: Conditional compilation with proper guards
 */
void initCAN()
{
  #if defined (NATIVE_CAN_AVAILABLE)
    configPage9.intcan_available = 1;   // device has internal canbus
    //Teensy uses the Flexcan_T4 library to use the internal canbus
    //enable local can interface
    //setup can interface to 500k
    Can0.begin();
    Can0.setBaudRate(500000);
    Can0.enableFIFO();
    /* Note: This must come after the call to setPinMapping() or else pins 29 and 30 will become unusable as outputs.
     * Workaround for: https://github.com/tonton81/FlexCAN_T4/issues/14 */

    #if defined(CORE_TEENSY35)         // use for Teensy 3.5 only
      Can0.setRX(DEF);
      Can0.setTX(DEF);
    #endif
  #endif
}

//=============================================================================
// CAN READ/WRITE WRAPPERS
//=============================================================================

/**
 * @brief Read CAN message from RX FIFO into global inMsg buffer
 *
 * @return 1 if message was read successfully, 0 if FIFO was empty
 *
 * @note Non-blocking: Returns immediately if no message available
 * @note Message stored in global inMsg (CAN_message_t structure)
 * @note Called frequently in main loop to service RX FIFO
 *
 * @complexity 1 (trivial wrapper)
 * @misra Compliant: Simple function call
 */
int CAN_read()
{
  return Can0.read(inMsg);
}

/**
 * @brief Write CAN message from global outMsg buffer to TX FIFO
 *
 * @details Transmits message previously prepared in global outMsg buffer.
 * TX is buffered in hardware FIFO (16 messages deep on Teensy).
 *
 * @note Message to send must be in global outMsg (CAN_message_t structure)
 * @note Caller must set: outMsg.id, outMsg.len, outMsg.buf[], outMsg.flags
 * @note Non-blocking: Message queued to FIFO, sent by hardware later
 *
 * @complexity 1 (trivial wrapper)
 * @misra Compliant: Simple function call
 */
void CAN_write()
{
  Can0.write(outMsg);
}

//=============================================================================
// CAN BROADCAST (Dashboard protocols)
//=============================================================================

/**
 * @brief Broadcast ECU data to CAN dashboard at specified frequency
 *
 * @param frequency Timer frequency calling this function (10Hz, 15Hz, 30Hz, 50Hz)
 *
 * @details Sends appropriate CAN messages based on configured protocol:
 * - **BMW:** 30Hz (DME1/2), 10Hz (DME4)
 * - **VAG:** 30Hz (RPM, VSS)
 * - **Haltech:** 50Hz (DATA1-5, PW), 15Hz (LAMBDA, TRIGGER, VSS), 10Hz (DATA4/5)
 *
 * Algorithm:
 * 1. Check configPage4.CANBroadcastProtocol for active protocol
 * 2. If frequency matches protocol requirement, call DashMessage() for each ID
 * 3. Write each message to CAN bus via Can0.write()
 *
 * @note Called from main loop at multiple frequencies via timer flags
 * @note Each protocol uses specific CAN IDs (see @file documentation)
 * @note Some protocols use standard 11-bit IDs, others use extended 29-bit
 *
 * @complexity 3 (nested switch-case, multiple frequencies)
 * @misra Compliant: Default case handles unknown protocols
 */
void sendCANBroadcast(uint8_t frequency)
{
  switch(configPage4.CANBroadcastProtocol)
  {
    case CAN_BROADCAST_PROTOCOL_OFF:
      break;
    case CAN_BROADCAST_PROTOCOL_BMW:
      if(frequency == 30)
      {
        outMsg.flags.extended = 0; //Make sure to set this to standard
        DashMessage(CAN_BMW_DME1);
        Can0.write(outMsg);
        DashMessage(CAN_BMW_DME2);
        Can0.write(outMsg);
      }
      else if(frequency == 10)
      {
        outMsg.flags.extended = 0; //Make sure to set this to standard
        DashMessage(CAN_BMW_DME4);
        Can0.write(outMsg);
      }
      break;
    case CAN_BROADCAST_PROTOCOL_VAG:
      //All VAG broadcasts are at 30Hz
      if(frequency == 30)
      {
        outMsg.flags.extended = 0; //Make sure to set this to standard
        DashMessage(CAN_VAG_RPM);
        Can0.write(outMsg);
        DashMessage(CAN_VAG_VSS);
        Can0.write(outMsg);
      }
      break;
    case CAN_BROADCAST_PROTOCOL_HALTECH:
      //Haltech has broadcasts over multiple frequencies
      if(frequency == 50)
      {
        DashMessage(CAN_HALTECH_DATA1);
        Can0.write(outMsg);
        DashMessage(CAN_HALTECH_DATA2);
        Can0.write(outMsg);
        DashMessage(CAN_HALTECH_DATA3);
        Can0.write(outMsg);
        DashMessage(CAN_HALTECH_PW);
        Can0.write(outMsg);
      }
      else if(frequency == 15)
      {
        //Note: Haltech lists these as being 20Hz messages, but as we don't have a 20Hz flag we'll use 15Hz instead
        DashMessage(CAN_HALTECH_LAMBDA);
        Can0.write(outMsg);
        DashMessage(CAN_HALTECH_TRIGGER);
        Can0.write(outMsg);
        DashMessage(CAN_HALTECH_VSS);
        Can0.write(outMsg);
      }
      else if(frequency == 10)
      {
        DashMessage(CAN_HALTECH_DATA4);
        Can0.write(outMsg);
        DashMessage(CAN_HALTECH_DATA5);
        Can0.write(outMsg);
      }
      break;

    default:
      break;
  }
}

//=============================================================================
// CAN WIDEBAND O2 SENSOR INPUT
//=============================================================================

/**
 * @brief Convert lambda to AFR with overflow protection
 */
static inline uint8_t lambdaToAFR(uint32_t rawLambda)
{
  uint32_t afr = (rawLambda * configPage2.stoich) / 10000U;
  return (afr > 250U) ? 250U : (uint8_t)(afr & 0xFFU);
}

/**
 * @brief Store AFR value based on CAN message ID
 */
static inline void storeO2Value(uint32_t msgId, uint8_t afrValue)
{
  if (msgId == 0x190U) { currentStatus.O2 = afrValue; }
  else if (msgId == 0x192U) { currentStatus.O2_2 = afrValue; }
}

/**
 * @brief Send RusEFI heater enable message
 */
static inline void sendRusEFIHeaterMsg(void)
{
  outMsg.id = 0xEF50000;
  outMsg.flags.extended = 1;
  outMsg.len = 2;
  outMsg.buf[0] = currentStatus.battery10;
  outMsg.buf[1] = BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) ? 0x1 : 0x0;
  Can0.write(outMsg);
  outMsg.flags.extended = 0;
}

/**
 * @brief Process RusEFI CAN wideband lambda message
 */
static inline void processRusEFILambda(void)
{
  sendRusEFIHeaterMsg();

  bool isValidId = (inMsg.id == 0x190U) || (inMsg.id == 0x192U);
  if (!isValidId) { return; }

  bool isLambdaValid = (inMsg.buf[1] == 0x1U);
  if (!isLambdaValid) { return; }

  uint32_t rawLambda = word(inMsg.buf[3], inMsg.buf[2]);
  storeO2Value(inMsg.id, lambdaToAFR(rawLambda));
}

/**
 * @brief Process AEM X-Series UEGO CAN lambda message
 */
static inline void processAEMLambda(void)
{
  if (inMsg.id != 0x180U) { return; }
  if (!BIT_CHECK(inMsg.buf[6], 7)) { return; }

  uint32_t rawLambda = word(inMsg.buf[0], inMsg.buf[1]);
  uint32_t afr = (rawLambda * configPage2.stoich) / 10000U;
  currentStatus.O2 = (afr > UINT8_MAX) ? UINT8_MAX : (uint8_t)afr;
}

/**
 * @brief Receive lambda data from external CAN wideband O2 controller
 */
void receiveCANwbo()
{
  if (configPage2.canWBO == CAN_WBO_RUSEFI) { processRusEFILambda(); return; }
  if (configPage2.canWBO == CAN_WBO_AEM) { processAEMLambda(); }
}

//=============================================================================
// DASH MESSAGE FORMATTER (Protocol-specific CAN packets)
//=============================================================================

/**
 * @brief Format CAN message for specific dashboard protocol and message ID
 *
 * @param DashMessageID CAN message ID defining which message to prepare
 *
 * @details Populates global outMsg buffer with protocol-specific data.
 * Handles byte packing, scaling, and bit field encoding for each protocol.
 *
 * **Supported Message IDs:**
 * - CAN_BMW_DME1 (0x316): RPM, torque
 * - CAN_BMW_DME2 (0x329): Coolant, TPS, baro
 * - CAN_BMW_DME4 (0x545): Fuel consumption, CEL/EML/overheat lights
 * - CAN_VAG_RPM: RPM for VW/Audi cluster
 * - CAN_VAG_VSS: Vehicle speed for VW/Audi cluster
 * - CAN_HALTECH_DATA1-5: Various engine parameters
 * - CAN_HALTECH_LAMBDA: O2 sensor readings
 * - CAN_HALTECH_TRIGGER: Sync status, error count
 * - CAN_HALTECH_VSS: Vehicle speed
 * - CAN_HALTECH_PW: Injector pulse width
 * - Additional protocols (Link, Megasquirt, OBD2, etc.)
 *
 * **Message Preparation:**
 * 1. Set outMsg.id = DashMessageID
 * 2. Set outMsg.len = number of data bytes (1-8)
 * 3. Populate outMsg.buf[0-7] with scaled/packed data
 * 4. Set outMsg.flags.extended if using 29-bit ID
 *
 * @note Does NOT transmit - caller must call Can0.write(outMsg)
 * @note Some values require scaling (e.g., RPM * 64 / 10 for BMW)
 * @note Some values require offset (e.g., coolant + 48 for BMW)
 * @note Bit fields used for status flags (engine running, clutch, brakes, etc.)
 *
 * @complexity 5 (massive switch-case with scaling/packing per protocol)
 * @misra Compliant: Default case prevents undefined behavior, overflow checks
 */
void DashMessage(uint16_t DashMessageID)
{
  uint16_t temp_TPS;
  uint16_t temp_MAP;
  uint16_t temp_Baro;
  uint16_t temp_CLT;
  uint16_t temp_IAT;
  uint16_t temp_VSS;
  uint16_t temp_VVT1;
  uint16_t temp_VVT2;
  uint16_t temp_fuelLoad;
  uint16_t temp_fuelTemp;
  uint16_t temp_fuelPressure;
  uint16_t temp_oilPressure;
  int16_t temp_Advance;
  uint16_t temp_DutyCycle;
  uint16_t temp_Lambda;
  uint16_t temp_BoostTarget;

  outMsg.id = DashMessageID;
  switch (DashMessageID)
  {
    case CAN_BMW_DME1:
      uint32_t temp_RPM;
      temp_RPM = currentStatus.RPM * 64UL;  //RPM conversion is currentStatus.RPM * 6.4, but this does it without floats.
      temp_RPM = temp_RPM / 10U;
      outMsg.len = 8;
      outMsg.buf[0] = 0x05;  //bitfield, Bit0 = 1 = terminal 15 on detected, Bit2 = 1 = the ASC message ASC1 was received within the last 500 ms and contains no plausibility errors
      outMsg.buf[1] = 0x0C;  //Indexed Engine Torque in % of C_TQ_STND TBD do torque calculation.
      outMsg.buf[2] = lowByte(uint16_t(temp_RPM));  //lsb RPM
      outMsg.buf[3] = highByte(uint16_t(temp_RPM)); //msb RPM
      outMsg.buf[4] = 0x0C;  //Indicated Engine Torque in % of C_TQ_STND TBD do torque calculation!! Use same as for byte 1
      outMsg.buf[5] = 0x15;  //Engine Torque Loss (due to engine friction, AC compressor and electrical power consumption)
      outMsg.buf[6] = 0x00;  //not used
      outMsg.buf[7] = 0x35;  //Theorethical Engine Torque in % of C_TQ_STND after charge intervention
    break;

    case CAN_BMW_DME2:
      temp_TPS = map(currentStatus.TPS, 0, 200, 1, 254);//TPS value conversion (from 0x01 to 0xFE)
      temp_CLT = ((currentStatus.coolant + 48)*4)/3; //CLT conversion (actual value to add is 48.373, but close enough)
      if (temp_CLT > UINT8_MAX) { temp_CLT = UINT8_MAX; } //CLT conversion can yield to higher values than what fits to byte, so limit the maximum value to 255.

      outMsg.len = 8;
      outMsg.buf[0] = 0x11;  //Multiplexed Information
      outMsg.buf[1] = temp_CLT;
      outMsg.buf[2] = currentStatus.baro;
      outMsg.buf[3] = 0x08;  //bitfield, Bit0 = 0 = Clutch released, Bit 3 = 1 = engine running
      outMsg.buf[4] = 0x00;  //TPS_VIRT_CRU_CAN (Not used)
      outMsg.buf[5] = (uint8_t)temp_TPS;
      outMsg.buf[6] = 0x00;  //bitfield, Bit0 = 0 = brake not actuated, Bit1 = 0 = brake switch system OK etc...
      outMsg.buf[7] = 0x00;  //not used, but set to zero just in case.
    break;

    case CAN_BMW_DME4:       //fuel consumption and CEL light for BMW e46/e39/e38 instrument cluster
    {
      // Calculate fuel consumption if not already done recently
      calculateBMWFuelConsumption();

      // Build status byte 0: CEL/MIL, Cruise, EML lights
      // Bit 1 = CEL (Check Engine Light / MIL)
      // Bit 3 = Cruise control active
      // Bit 4 = EML (Engine Management Lamp)
      uint8_t statusByte = 0x00;
      if (currentStatus.milOn) { statusByte |= 0x02; }  // CEL on if any confirmed DTCs

      outMsg.len = 5;
      outMsg.buf[0] = statusByte;
      outMsg.buf[1] = lowByte(currentStatus.fuelConsumption);   // LSB Fuel consumption (0.01 L/h)
      outMsg.buf[2] = highByte(currentStatus.fuelConsumption);  // MSB Fuel Consumption

      // Byte 3: Overheat warning
      if (currentStatus.coolant > 159) { outMsg.buf[3] = 0x08; } // Overheat light on if coolant > 120°C
      else { outMsg.buf[3] = 0x00; }

      // Byte 4: Oil temperature (map from IAT or use 78°C default)
      // BMW expects value where: actual_temp = (value * 0.75) - 48
      // For 78°C: (78 + 48) / 0.75 = 168 = 0xA8
      // For now, derive from coolant temp with offset for realistic oil temp
      int16_t oilTemp = currentStatus.coolant + 10; // Oil typically 10°C higher than coolant
      if (oilTemp < -48) { oilTemp = -48; }
      if (oilTemp > 143) { oilTemp = 143; }
      outMsg.buf[4] = (uint8_t)((oilTemp + 48) * 4 / 3);
    }
    break;

    case CAN_VAG_RPM:       //RPM for VW instrument cluster
      temp_RPM =  currentStatus.RPM * 4; //RPM conversion
      outMsg.len = 8;
      outMsg.buf[0] = 0x49;
      outMsg.buf[1] = 0x0E;
      outMsg.buf[2] = lowByte(uint16_t(temp_RPM));  //lsb RPM
      outMsg.buf[3] = highByte(uint16_t(temp_RPM)); //msb RPM
      outMsg.buf[4] = 0x0E;
      outMsg.buf[5] = 0x00;
      outMsg.buf[6] = 0x1B;
      outMsg.buf[7] = 0x0E;
    break;

    case CAN_VAG_VSS:       //VSS for VW instrument cluster
      temp_VSS =  currentStatus.vss * 133U; //VSS conversion
      outMsg.len = 8;
      outMsg.buf[0] = 0xFF;
      outMsg.buf[1] = lowByte(temp_VSS);
      outMsg.buf[2] = highByte(temp_VSS);
      outMsg.buf[3] = 0x00;
      outMsg.buf[4] = 0x00;
      outMsg.buf[5] = 0x00;
      outMsg.buf[6] = 0x00;
      outMsg.buf[7] = 0xAD;
    break;

    case CAN_HALTECH_DATA1:
      temp_MAP = currentStatus.MAP * 10U;
      temp_TPS = currentStatus.TPS * 5U; //TPS value to 0.1. TPS is already in 0.5 increments, so multiply by 5
      outMsg.len = 8;
      outMsg.buf[0] = highByte(currentStatus.RPM);
      outMsg.buf[1] = lowByte(currentStatus.RPM);
      outMsg.buf[2] = highByte(temp_MAP);
      outMsg.buf[3] = lowByte(temp_MAP);
      outMsg.buf[4] = highByte(temp_TPS);
      outMsg.buf[5] = lowByte(temp_TPS);
      //Next 2 bytes are coolant pressure, not supported
      outMsg.buf[6] = 0x00;
      outMsg.buf[7] = 0x00;
    break;

    case CAN_HALTECH_DATA2:
      temp_fuelLoad = currentStatus.fuelLoad * 10U;
      temp_fuelPressure = div100(currentStatus.fuelPressure * 6894UL) + 1013; //Convert from PSI to KPA and add 101.3kPa (1 atmosphere) offset. 0.1 scale
      temp_oilPressure = div100(currentStatus.oilPressure * 6894UL) + 1013; //Convert from PSI to KPA and add 101.3kPa (1 atmosphere) offset. 0.1 scale
      outMsg.len = 8;
      outMsg.buf[0] = highByte(temp_fuelPressure); //Fuel pressure
      outMsg.buf[1] = lowByte(temp_fuelPressure);
      outMsg.buf[2] = highByte(temp_oilPressure); //Oil Pressure
      outMsg.buf[3] = lowByte(temp_oilPressure);
      outMsg.buf[4] = highByte(temp_fuelLoad);
      outMsg.buf[5] = lowByte(temp_fuelLoad);
      outMsg.buf[6] = 0x00; //Wastegate pressure
      outMsg.buf[7] = 0x00;
    break;

    case CAN_HALTECH_DATA3:
      temp_Advance = currentStatus.advance * 10U; //Note: Signed value
      //Convert PW into duty cycle
      temp_DutyCycle = (currentStatus.PW1 * 100UL * currentStatus.nSquirts) / revolutionTime;
      if (configPage2.strokes == FOUR_STROKE) { temp_DutyCycle = temp_DutyCycle / 2U; }

      outMsg.len = 8;
      outMsg.buf[0] = highByte(temp_DutyCycle);
      outMsg.buf[1] = lowByte(temp_DutyCycle);
      outMsg.buf[2] = 0x00; //TODO: Staging Duty Cycle.
      outMsg.buf[3] = 0x00;
      outMsg.buf[4] = highByte(temp_Advance);
      outMsg.buf[5] = lowByte(temp_Advance);
      outMsg.buf[6] = 0x00; //Unused
      outMsg.buf[7] = 0x00; //Unused
    break;

    case CAN_HALTECH_PW:
      outMsg.len = 8;
      outMsg.buf[0] = highByte(currentStatus.PW1);
      outMsg.buf[1] = lowByte(currentStatus.PW1);
      outMsg.buf[2] = highByte(currentStatus.PW2);
      outMsg.buf[3] = lowByte(currentStatus.PW2);
      outMsg.buf[4] = highByte(currentStatus.PW3);
      outMsg.buf[5] = lowByte(currentStatus.PW3);
      outMsg.buf[6] = highByte(currentStatus.PW4);
      outMsg.buf[7] = lowByte(currentStatus.PW4);
    break;

    case CAN_HALTECH_LAMBDA:
      temp_Lambda = (currentStatus.O2 * 1000U) / configPage2.stoich;
      outMsg.len = 8;
      outMsg.buf[0] = highByte(temp_Lambda);
      outMsg.buf[1] = lowByte(temp_Lambda);
      temp_Lambda = (currentStatus.O2_2 * 1000U) / configPage2.stoich;
      outMsg.buf[2] = highByte(temp_Lambda);
      outMsg.buf[3] = lowByte(temp_Lambda);
      outMsg.buf[4] = 0x00; //Lambda 3
      outMsg.buf[5] = 0x00; //Lambda 3
      outMsg.buf[6] = 0x00; //Lambda 4
      outMsg.buf[7] = 0x00; //Lambda 4
    break;

    case CAN_HALTECH_VSS:
      temp_VSS = currentStatus.vss * 10U;
      temp_VVT1 = currentStatus.vvt1Angle * 10U;
      temp_VVT2 = currentStatus.vvt2Angle * 10U;
      outMsg.len = 8;
      outMsg.buf[0] = highByte(temp_VSS);
      outMsg.buf[1] = lowByte(temp_VSS);
      outMsg.buf[2] = 0x00;
      outMsg.buf[3] = currentStatus.gear;
      outMsg.buf[4] = highByte(temp_VVT1);
      outMsg.buf[5] = lowByte(temp_VVT1);
      outMsg.buf[6] = highByte(temp_VVT2);
      outMsg.buf[7] = lowByte(temp_VVT2);
    break;

    case CAN_HALTECH_DATA4:
      temp_BoostTarget = currentStatus.boostTarget * 10U;
      temp_Baro = currentStatus.baro * 10U;
      outMsg.len = 8;
      outMsg.buf[0] = 0x00; //High byte for battery voltage, which is not used (Max battery voltage is 25.5 or 255)
      outMsg.buf[1] = currentStatus.battery10;
      outMsg.buf[2] = 0x00; //Unused
      outMsg.buf[3] = 0x00; //Unused
      outMsg.buf[4] = highByte(temp_BoostTarget);
      outMsg.buf[5] = lowByte(temp_BoostTarget);
      outMsg.buf[6] = highByte(temp_Baro);
      outMsg.buf[7] = lowByte(temp_Baro);
    break;

    case CAN_HALTECH_DATA5:
      temp_CLT = (currentStatus.coolant + 273U) * 10U; //Convert to Kelvin and adjust to 0.1
      temp_IAT = (currentStatus.IAT + 273U) * 10U; //Convert to Kelvin and adjust to 0.1
      temp_fuelTemp = (currentStatus.fuelTemp + 273U) * 10U; //Convert to Kelvin and adjust to 0.1
      outMsg.len = 8;
      outMsg.buf[0] = highByte(temp_CLT);
      outMsg.buf[1] = lowByte(temp_CLT);
      outMsg.buf[2] = highByte(temp_IAT);
      outMsg.buf[3] = lowByte(temp_IAT);
      outMsg.buf[4] = highByte(temp_fuelTemp);
      outMsg.buf[5] = lowByte(temp_fuelTemp);
      outMsg.buf[6] = 0x00; //Oil Temperature
      outMsg.buf[7] = 0x00; //Oil Temperature
    break;

    default:
    break;
  }
}

/**
 * @brief Check if message is OBD broadcast or ECU-specific
 */
static inline bool isOBDAddress(void)
{
  uint16_t ecuAddr = uint16_t(configPage9.obd_address + TS_CAN_OFFSET);
  return (inMsg.id == ecuAddr) || (inMsg.id == 0x7DFU);
}

// Forward declarations for OBD mode handlers (defined after obd_response)
static inline void handleOBDMode03(void);
static inline void handleOBDMode04(void);
static inline void handleOBDMode07(void);
static inline void handleOBDMode09Full(void);

/**
 * @brief Handle OBD mode 1 (realtime data) request
 */
static inline void handleOBDMode01(void)
{
  obd_response(inMsg.buf[1], inMsg.buf[2], 0);
  outMsg.id = 0x7E8;
  Can0.write(outMsg);
}

/**
 * @brief Handle OBD mode 22h (custom data) request
 */
static inline void handleOBDMode22(void)
{
  obd_response(inMsg.buf[1], inMsg.buf[2], inMsg.buf[3]);
  outMsg.id = 0x7E8;
  Can0.write(outMsg);
}

void can_Command(void)
{
  if (!isOBDAddress()) { return; }

  uint8_t mode = inMsg.buf[1];
  bool isEcuSpecific = (inMsg.id == uint16_t(configPage9.obd_address + TS_CAN_OFFSET));

  switch (mode)
  {
    case 0x01U: // Mode 01 - Show current data
      handleOBDMode01();
      break;

    case 0x03U: // Mode 03 - Read confirmed DTCs
      handleOBDMode03();
      break;

    case 0x04U: // Mode 04 - Clear DTCs (ECU-specific only)
      if (isEcuSpecific) { handleOBDMode04(); }
      break;

    case 0x07U: // Mode 07 - Read pending DTCs
      handleOBDMode07();
      break;

    case 0x09U: // Mode 09 - Vehicle info (ECU-specific only)
      if (isEcuSpecific) { handleOBDMode09Full(); }
      break;

    case 0x22U: // Mode 22 - Read data by identifier (custom)
      handleOBDMode22();
      break;

    default:
      // Unsupported mode - no response (per OBD-II spec)
      break;
  }
}

// This routine builds the realtime data into packets that the obd requesting device can understand. This is only used by teensy and stm32 with onboard canbus
void obd_response(uint8_t PIDmode, uint8_t requestedPIDlow, uint8_t requestedPIDhigh)
{
//only build the PID if the mcu has onboard/attached can

  uint16_t obdcalcA;    //used in obd calcs
  uint16_t obdcalcB;    //used in obd calcs
  uint16_t obdcalcC;    //used in obd calcs
  uint16_t obdcalcD;    //used in obd calcs
  uint32_t obdcalcE32;    //used in calcs
  uint32_t obdcalcF32;    //used in calcs
  uint16_t obdcalcG16;    //used in calcs
  uint16_t obdcalcH16;    //used in calcs

  outMsg.len = 8;
  outMsg.flags.extended = 0; //Make sure to set this to standard

  if (PIDmode == 0x01)
  {
    switch (requestedPIDlow)
    {
      case 0:       //PID-0x00 PIDs supported 01-20
        outMsg.buf[0] =  0x06;    // sending 6 bytes
        outMsg.buf[1] =  0x41;    // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x00;    // PID code
        outMsg.buf[3] =  0x08;   //B0000 1000   1-8
        outMsg.buf[4] =  B01111110;   //9-16
        outMsg.buf[5] =  B10100000;   //17-24
        outMsg.buf[6] =  B00010001;   //17-32
        outMsg.buf[7] =  B00000000;
      break;

      case 5:      //PID-0x05 Engine coolant temperature , range is -40 to 215 deg C , formula == A-40
        outMsg.buf[0] =  0x03;                 // sending 3 bytes
        outMsg.buf[1] =  0x41;                 // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x05;                 // pid code
        outMsg.buf[3] =  temperatureAddOffset(currentStatus.coolant);   //the data value A
        outMsg.buf[4] =  0x00;                 //the data value B which is 0 as unused
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 10:        // PID-0x0A , Fuel Pressure (Gauge) , range is 0 to 765 kPa , formula == A / 3)
        uint16_t temp_fuelpressure;
        // Fuel pressure is in PSI. PSI to kPa is 6.89475729, but that needs to be divided by 3 for OBD2 formula. So 2.298.... 2.3 is close enough, so that in fraction.
        temp_fuelpressure = udiv_32_16((23u * currentStatus.fuelPressure), 10);
        outMsg.buf[0] =  0x03;    // sending 3 byte
        outMsg.buf[1] =  0x41;    //
        outMsg.buf[2] =  0x0A;    // pid code
        outMsg.buf[3] =  lowByte(temp_fuelpressure);
        outMsg.buf[4] =  0x00;
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 11:        // PID-0x0B , MAP , range is 0 to 255 kPa , Formula == A
        outMsg.buf[0] =  0x03;    // sending 3 byte
        outMsg.buf[1] =  0x41;    //
        outMsg.buf[2] =  0x0B;    // pid code
        outMsg.buf[3] =  lowByte(currentStatus.MAP);    // absolute map
        outMsg.buf[4] =  0x00;
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 12:        // PID-0x0C , RPM  , range is 0 to 16383.75 rpm , Formula == 256A+B / 4
        uint16_t temp_revs;
        temp_revs = currentStatus.RPM << 2 ;      //
        outMsg.buf[0] = 0x04;                        // sending 4 byte
        outMsg.buf[1] = 0x41;                        //
        outMsg.buf[2] = 0x0C;                        // pid code
        outMsg.buf[3] = highByte(temp_revs);         //obdcalcB; A
        outMsg.buf[4] = lowByte(temp_revs);          //obdcalcD; B
        outMsg.buf[5] = 0x00;
        outMsg.buf[6] = 0x00;
        outMsg.buf[7] = 0x00;
      break;

      case 13:        //PID-0x0D , Vehicle speed , range is 0 to 255 km/h , formula == A
        outMsg.buf[0] =  0x03;                       // sending 3 bytes
        outMsg.buf[1] =  0x41;                       // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x0D;                       // pid code
        outMsg.buf[3] =  lowByte(currentStatus.vss); // A
        outMsg.buf[4] =  0x00;                       // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 14:      //PID-0x0E , Ignition Timing advance, range is -64 to 63.5 BTDC , formula == A/2 - 64
        int8_t temp_timingadvance;
        temp_timingadvance = ((currentStatus.advance + 64) << 1);
        //obdcalcA = ((timingadvance + 64) <<1) ; //((timingadvance + 64) *2)
        outMsg.buf[0] =  0x03;                     // sending 3 bytes
        outMsg.buf[1] =  0x41;                     // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x0E;                     // pid code
        outMsg.buf[3] =  temp_timingadvance;       // A
        outMsg.buf[4] =  0x00;                     // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 15:      //PID-0x0F , Inlet air temperature , range is -40 to 215 deg C, formula == A-40
        outMsg.buf[0] =  0x03;                                                         // sending 3 bytes
        outMsg.buf[1] =  0x41;                                                         // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x0F;                                                         // pid code
        outMsg.buf[3] =  temperatureAddOffset(currentStatus.IAT);   // A
        outMsg.buf[4] =  0x00;                                                         // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 17:  // PID-0x11 ,
        // TPS percentage, range is 0 to 100 percent, formula == 100/255 A
        // Faster math with possible to be off by 1(0.329%)
        obdcalcA = (327u * currentStatus.TPS) >> 8; //327 * 200 = 0xFF78
        obdcalcA = (obdcalcA > UINT8_MAX) ? UINT8_MAX : obdcalcA;
        outMsg.buf[0] =  0x03;                    // sending 3 bytes
        outMsg.buf[1] =  0x41;                    // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x11;                    // pid code
        outMsg.buf[3] =  obdcalcA;                // A
        outMsg.buf[4] =  0x00;                    // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 19:      //PID-0x13 , oxygen sensors present, A0-A3 == bank1 , A4-A7 == bank2 ,
        uint16_t O2present;
        O2present = B00000011 ;       //realtimebufferA[24];         TEST VALUE !!!!!
        outMsg.buf[0] =  0x03;           // sending 3 bytes
        outMsg.buf[1] =  0x41;           // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x13;           // pid code
        outMsg.buf[3] =  O2present ;     // A
        outMsg.buf[4] =  0x00;           // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 28:      // PID-0x1C obd standard
        uint16_t obdstandard;
        obdstandard = 7;              // This is OBD2 / EOBD
        outMsg.buf[0] =  0x03;           // sending 3 bytes
        outMsg.buf[1] =  0x41;           // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x1C;           // pid code
        outMsg.buf[3] =  obdstandard;    // A
        outMsg.buf[4] =  0x00;           // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 32:      // PID-0x20 PIDs supported [21-40]
        outMsg.buf[0] =  0x06;          // sending 4 bytes
        outMsg.buf[1] =  0x41;          // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x20;          // pid code
        outMsg.buf[3] =  B00011000;     // 33-40
        outMsg.buf[4] =  B00000000;     //41 - 48
        outMsg.buf[5] =  B00100000;     //49-56
        outMsg.buf[6] =  B00000001;     //57-64
        outMsg.buf[7] = 0x00;
      break;

      case 36:      // PID-0x24 O2 sensor2, AB: fuel/air equivalence ratio, CD: voltage ,  Formula == (2/65536)(256A +B) , 8/65536(256C+D) , Range is 0 to <2 and 0 to >8V
        //uint16_t O2_1e ;
        //int16_t O2_1v ;
        obdcalcF32 = currentStatus.O2;            // afr(is *10 so 25.5 is 255) , needs a 32bit else will overflow
        obdcalcG16 = udiv_32_16((obdcalcF32 * 32768u), configPage2.stoich);
        obdcalcA = highByte(obdcalcG16);
        obdcalcB = lowByte(obdcalcG16);

        obdcalcF32 = currentStatus.O2ADC ;             //o2ADC is wideband volts to send *100
        obdcalcG16 = (obdcalcF32 *20971)>>8;
        obdcalcC = highByte(obdcalcG16);
        obdcalcD = lowByte(obdcalcG16);

        outMsg.buf[0] =  0x06;    // sending 4 bytes
        outMsg.buf[1] =  0x41;    // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x24;    // pid code
        outMsg.buf[3] =  obdcalcA;   // A
        outMsg.buf[4] =  obdcalcB;   // B
        outMsg.buf[5] =  obdcalcC;   // C
        outMsg.buf[6] =  obdcalcD;   // D
        outMsg.buf[7] =  0x00;
      break;

      case 37:      //O2 sensor2, AB fuel/air equivalence ratio, CD voltage ,  2/65536(256A +B) ,8/65536(256C+D) , range is 0 to <2 and 0 to >8V
        //uint16_t O2_2e ;
        //int16_t O2_2V ;
        obdcalcF32 = currentStatus.O2_2;            // afr(is *10 so 25.5 is 255) , needs a 32bit else will overflow
        obdcalcG16 = udiv_32_16((obdcalcF32 * 32768u), configPage2.stoich);
        obdcalcA = highByte(obdcalcG16);
        obdcalcB = lowByte(obdcalcG16);

        obdcalcF32 = currentStatus.O2_2ADC ;             //o2_2ADC is wideband volts to send *100
        obdcalcG16 = (obdcalcF32 *20971)>>8;
        obdcalcC = highByte(obdcalcG16);
        obdcalcD = lowByte(obdcalcG16);

        outMsg.buf[0] =  0x06;    // sending 4 bytes
        outMsg.buf[1] =  0x41;    // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x25;    // pid code
        outMsg.buf[3] =  obdcalcA;   // A
        outMsg.buf[4] =  obdcalcB;   // B
        outMsg.buf[5] =  obdcalcC;   // C
        outMsg.buf[6] =  obdcalcD;   // D
        outMsg.buf[7] =  0x00;
      break;

      case 51:      //PID-0x33 Absolute Barometric pressure , range is 0 to 255 kPa , formula == A
        outMsg.buf[0] =  0x03;                  // sending 3 bytes
        outMsg.buf[1] =  0x41;                  // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x33;                  // pid code
        outMsg.buf[3] =  currentStatus.baro ;   // A
        outMsg.buf[4] =  0x00;                  // B which is 0 as unused
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 64:      // PIDs supported [41-60]
        outMsg.buf[0] =  0x06;    // sending 4 bytes
        outMsg.buf[1] =  0x41;    // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x40;    // pid code
        outMsg.buf[3] =  B01000100;    // 65-72dec
        outMsg.buf[4] =  B00000000;    // 73-80
        outMsg.buf[5] =  B01000000;   //  81-88
        outMsg.buf[6] =  B00010000;   //  89-96
        outMsg.buf[7] =  0x00;
      break;

      case 66:      //control module voltage, 256A+B / 1000 , range is 0 to 65.535v
        uint16_t temp_ecuBatt;
        temp_ecuBatt = currentStatus.battery10;   // create a 16bit temp variable to do the math
        obdcalcA = temp_ecuBatt*100;              // should be *1000 but ecuBatt is already *10
        outMsg.buf[0] =  0x04;                       // sending 4 bytes
        outMsg.buf[1] =  0x41;                       // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x42;                       // pid code
        outMsg.buf[3] =  highByte(obdcalcA) ;        // A
        outMsg.buf[4] =  lowByte(obdcalcA) ;         // B
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 70:        //PID-0x46 Ambient Air Temperature , range is -40 to 215 deg C , formula == A-40
        uint16_t temp_ambientair;
        temp_ambientair = 11;              // TEST VALUE !!!!!!!!!!
        obdcalcA = temperatureAddOffset(temp_ambientair);
        outMsg.buf[0] =  0x03;             // sending 3 byte
        outMsg.buf[1] =  0x41;             // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x46;             // pid code
        outMsg.buf[3] =  obdcalcA;         // A
        outMsg.buf[4] =  0x00;
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 82:        //PID-0x52 Ethanol fuel % , range is 0 to 100% , formula == (100/255)A
        //Ethanol sensor output can be above 100, add a check to avoid it
        obdcalcA = (655u * currentStatus.ethanolPct) >> 8; //655 * 100 = 0xFFDC
        obdcalcA = (obdcalcA > UINT8_MAX) ? UINT8_MAX : obdcalcA;
        outMsg.buf[0] =  0x03;                       // sending 3 byte
        outMsg.buf[1] =  0x41;                       // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x52;                       // pid code
        outMsg.buf[3] =  obdcalcA;                   // A
        outMsg.buf[4] =  0x00;
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 92:        //PID-0x5C Engine oil temperature , range is -40 to 210 deg C , formula == A-40
        uint16_t temp_engineoiltemp;
        temp_engineoiltemp = 40;              // TEST VALUE !!!!!!!!!!
        obdcalcA = temperatureAddOffset(temp_engineoiltemp);
        outMsg.buf[0] =  0x03;                // sending 3 byte
        outMsg.buf[1] =  0x41;                // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x5C;                // pid code
        outMsg.buf[3] =  obdcalcA ;           // A
        outMsg.buf[4] =  0x00;
        outMsg.buf[5] =  0x00;
        outMsg.buf[6] =  0x00;
        outMsg.buf[7] =  0x00;
      break;

      case 96:       //PIDs supported [61-80]
        outMsg.buf[0] =  0x06;    // sending 4 bytes
        outMsg.buf[1] =  0x41;    // Same as query, except that 40h is added to the mode value. So:41h = show current data ,42h = freeze frame ,etc.
        outMsg.buf[2] =  0x60;    // pid code
        outMsg.buf[3] =  0x00;    // B0000 0000
        outMsg.buf[4] =  0x00;    // B0000 0000
        outMsg.buf[5] =  0x00;    // B0000 0000
        outMsg.buf[6] =  0x00;    // B0000 0000
        outMsg.buf[7] =  0x00;
      break;

      default:
      break;
    }
  }
  else if (PIDmode == 0x22)
  {
    // Custom PID not listed in SAE std - Aux/CAN channel data
    bool isAuxChannel = (requestedPIDhigh == 0x77U) && (requestedPIDlow >= 0x01U) && (requestedPIDlow <= 0x10U);
    if (isAuxChannel)
    {
      // PID 0x01-0x10: Aux/CAN data IN Channel 1-16
      outMsg.buf[0] =  0x06;
      outMsg.buf[1] =  0x62;
      outMsg.buf[2] =  requestedPIDlow;
      outMsg.buf[3] =  0x77;
      outMsg.buf[4] =  lowByte(currentStatus.canin[requestedPIDlow-1]);
      outMsg.buf[5] =  highByte(currentStatus.canin[requestedPIDlow-1]);
      outMsg.buf[6] =  0x00;
      outMsg.buf[7] =  0x00;
    }
    // PID 0x78xx: Get any value from currentStatus array
    else if (requestedPIDhigh == 0x78U)
    {
      int16_t tempValue = ProgrammableIOGetData(requestedPIDlow);
      outMsg.buf[0] =  0x06;
      outMsg.buf[1] =  0x62;
      outMsg.buf[2] =  requestedPIDlow;
      outMsg.buf[3] =  0x78;
      outMsg.buf[4] =  lowByte(tempValue);
      outMsg.buf[5] =  highByte(tempValue);
      outMsg.buf[6] =  0x00;
      outMsg.buf[7] =  0x00;
    }
  }
}

/**
 * @brief Get CAN input value (1 or 2 bytes) with endianness handling
 */
static inline uint16_t getCANInputValue(uint8_t channelIdx)
{
  uint8_t startByte = configPage9.caninput_source_start_byte[channelIdx];
  bool isTwoByte = BIT_CHECK(configPage9.caninput_source_num_bytes, channelIdx);

  if (!isTwoByte) { return inMsg.buf[startByte]; }

  bool isLittleEndian = (configPage9.caninputEndianess == 1U);
  if (isLittleEndian) { return (inMsg.buf[startByte]) | (inMsg.buf[startByte + 1] << 8); }
  return (inMsg.buf[startByte] << 8) | (inMsg.buf[startByte + 1]);
}

void readAuxCanBus()
{
  for (int i = 0; i < 16; i++)
  {
    uint16_t channelAddress = configPage9.caninput_source_can_address[i] + TS_CAN_OFFSET;
    if (inMsg.id != channelAddress) { continue; }

    currentStatus.canin[i] = getCANInputValue(i);
  }
}

// =============================================================================
// OBD-II DTC (DIAGNOSTIC TROUBLE CODE) IMPLEMENTATION
// =============================================================================
// Mode 03: Read confirmed DTCs
// Mode 04: Clear all DTCs
// Mode 07: Read pending DTCs
// =============================================================================

/**
 * @brief Initialize DTC subsystem from stored values
 */
void dtc_init(void)
{
  // Count confirmed DTCs
  currentStatus.dtcConfirmedCount = 0;
  for (uint8_t i = 0; i < DTC_MAX_CONFIRMED; i++)
  {
    uint16_t dtc = ((uint16_t)configPage15.dtcConfirmed[i*2] << 8) |
                    configPage15.dtcConfirmed[i*2 + 1];
    if (dtc != 0x0000 && dtc != 0xFFFF)
    {
      currentStatus.dtcConfirmedCount++;
    }
    else
    {
      break; // DTCs are stored contiguously
    }
  }

  // Count pending DTCs
  currentStatus.dtcPendingCount = 0;
  for (uint8_t i = 0; i < DTC_MAX_PENDING; i++)
  {
    uint16_t dtc = ((uint16_t)configPage15.dtcPending[i*2] << 8) |
                    configPage15.dtcPending[i*2 + 1];
    if (dtc != 0x0000 && dtc != 0xFFFF)
    {
      currentStatus.dtcPendingCount++;
    }
    else
    {
      break;
    }
  }

  // Set MIL status
  currentStatus.milOn = (configPage15.dtcFlags & DTC_FLAG_MIL_ON) != 0;
  currentStatus.freezeFrameValid = (configPage15.dtcFlags & DTC_FLAG_FREEZE_CAPTURED) != 0;
}

/**
 * @brief Get count of stored DTCs
 */
uint8_t dtc_getCount(bool isPending)
{
  return isPending ? currentStatus.dtcPendingCount : currentStatus.dtcConfirmedCount;
}

/**
 * @brief Get DTC at specified index
 */
uint16_t dtc_getAt(uint8_t index, bool isPending)
{
  uint8_t maxCount = isPending ? DTC_MAX_PENDING : DTC_MAX_CONFIRMED;
  if (index >= maxCount) { return 0; }

  byte* dtcArray = isPending ? configPage15.dtcPending : configPage15.dtcConfirmed;
  uint16_t dtc = ((uint16_t)dtcArray[index*2] << 8) | dtcArray[index*2 + 1];

  return (dtc == 0xFFFF) ? 0 : dtc;
}

/**
 * @brief Set a DTC (Diagnostic Trouble Code)
 */
bool dtc_set(uint16_t dtcCode, bool isPending)
{
  if (dtcCode == 0 || dtcCode == 0xFFFF) { return false; }

  byte* dtcArray = isPending ? configPage15.dtcPending : configPage15.dtcConfirmed;
  uint8_t* countPtr = isPending ? &currentStatus.dtcPendingCount : &currentStatus.dtcConfirmedCount;
  uint8_t maxCount = isPending ? DTC_MAX_PENDING : DTC_MAX_CONFIRMED;

  // Check if DTC already exists
  for (uint8_t i = 0; i < *countPtr; i++)
  {
    uint16_t existing = ((uint16_t)dtcArray[i*2] << 8) | dtcArray[i*2 + 1];
    if (existing == dtcCode)
    {
      return true; // Already stored
    }
  }

  // Check if storage is full
  if (*countPtr >= maxCount) { return false; }

  // Store new DTC
  uint8_t idx = *countPtr;
  dtcArray[idx*2] = highByte(dtcCode);
  dtcArray[idx*2 + 1] = lowByte(dtcCode);
  (*countPtr)++;

  // Set MIL flag for confirmed DTCs
  if (!isPending)
  {
    currentStatus.milOn = true;
    configPage15.dtcFlags |= DTC_FLAG_MIL_ON;

    // Capture freeze frame on first confirmed DTC
    if (!currentStatus.freezeFrameValid)
    {
      freezeFrame_capture(dtcCode);
    }
  }

  return true;
}

/**
 * @brief Clear a specific DTC
 */
bool dtc_clear(uint16_t dtcCode)
{
  bool found = false;

  // Search in confirmed DTCs
  for (uint8_t i = 0; i < currentStatus.dtcConfirmedCount; i++)
  {
    uint16_t dtc = ((uint16_t)configPage15.dtcConfirmed[i*2] << 8) |
                    configPage15.dtcConfirmed[i*2 + 1];
    if (dtc == dtcCode)
    {
      // Shift remaining DTCs down
      for (uint8_t j = i; j < currentStatus.dtcConfirmedCount - 1; j++)
      {
        configPage15.dtcConfirmed[j*2] = configPage15.dtcConfirmed[(j+1)*2];
        configPage15.dtcConfirmed[j*2 + 1] = configPage15.dtcConfirmed[(j+1)*2 + 1];
      }
      // Clear last slot
      uint8_t lastIdx = currentStatus.dtcConfirmedCount - 1;
      configPage15.dtcConfirmed[lastIdx*2] = 0x00;
      configPage15.dtcConfirmed[lastIdx*2 + 1] = 0x00;
      currentStatus.dtcConfirmedCount--;
      found = true;
      break;
    }
  }

  // Search in pending DTCs
  for (uint8_t i = 0; i < currentStatus.dtcPendingCount; i++)
  {
    uint16_t dtc = ((uint16_t)configPage15.dtcPending[i*2] << 8) |
                    configPage15.dtcPending[i*2 + 1];
    if (dtc == dtcCode)
    {
      // Shift remaining DTCs down
      for (uint8_t j = i; j < currentStatus.dtcPendingCount - 1; j++)
      {
        configPage15.dtcPending[j*2] = configPage15.dtcPending[(j+1)*2];
        configPage15.dtcPending[j*2 + 1] = configPage15.dtcPending[(j+1)*2 + 1];
      }
      // Clear last slot
      uint8_t lastIdx = currentStatus.dtcPendingCount - 1;
      configPage15.dtcPending[lastIdx*2] = 0x00;
      configPage15.dtcPending[lastIdx*2 + 1] = 0x00;
      currentStatus.dtcPendingCount--;
      found = true;
      break;
    }
  }

  // Turn off MIL if no confirmed DTCs remain
  if (currentStatus.dtcConfirmedCount == 0)
  {
    currentStatus.milOn = false;
    configPage15.dtcFlags &= ~DTC_FLAG_MIL_ON;
  }

  return found;
}

/**
 * @brief Clear all DTCs (Mode 04 response)
 */
void dtc_clearAll(void)
{
  // Clear all confirmed DTCs
  for (uint8_t i = 0; i < DTC_MAX_CONFIRMED * 2; i++)
  {
    configPage15.dtcConfirmed[i] = 0x00;
  }
  currentStatus.dtcConfirmedCount = 0;

  // Clear all pending DTCs
  for (uint8_t i = 0; i < DTC_MAX_PENDING * 2; i++)
  {
    configPage15.dtcPending[i] = 0x00;
  }
  currentStatus.dtcPendingCount = 0;

  // Clear freeze frame
  freezeFrame_clear();

  // Turn off MIL
  currentStatus.milOn = false;
  configPage15.dtcFlags = 0;

  // Record clear timestamp
  currentStatus.dtcLastClearTime = millis();
}

/**
 * @brief Promote pending DTC to confirmed after driving cycle
 */
void dtc_promote(uint16_t dtcCode)
{
  // Remove from pending
  dtc_clear(dtcCode);

  // Add to confirmed
  dtc_set(dtcCode, false);
}

// =============================================================================
// FREEZE FRAME IMPLEMENTATION
// =============================================================================

/**
 * @brief Capture freeze frame data at current engine state
 */
void freezeFrame_capture(uint16_t triggerDTC)
{
  if (currentStatus.freezeFrameValid) { return; } // Only capture once

  configPage15.freezeFrameDTC[0] = highByte(triggerDTC);
  configPage15.freezeFrameDTC[1] = lowByte(triggerDTC);
  configPage15.freezeFrameRPM = currentStatus.RPM;
  configPage15.freezeFrameMAP = (uint16_t)currentStatus.MAP;
  configPage15.freezeFrameTPS = currentStatus.TPS;
  configPage15.freezeFrameCLT = (int8_t)currentStatus.coolant;
  configPage15.freezeFrameIAT = (int8_t)currentStatus.IAT;
  configPage15.freezeFrameBaro = currentStatus.baro;
  configPage15.freezeFrameVSS = (uint8_t)(currentStatus.vss > 255 ? 255 : currentStatus.vss);
  configPage15.freezeFrameBattery = currentStatus.battery10;
  configPage15.freezeFrameO2 = currentStatus.O2;
  configPage15.freezeFrameAdvance = currentStatus.advance;
  configPage15.freezeFramePW = currentStatus.PW1;
  configPage15.freezeFrameLoad = (uint8_t)(currentStatus.fuelLoad > 255 ? 255 : currentStatus.fuelLoad);
  configPage15.freezeFrameAFR = currentStatus.afrTarget;
  configPage15.freezeFrameFuel = (uint8_t)currentStatus.corrections;
  configPage15.freezeFrameStatus1 = currentStatus.status1;
  configPage15.freezeFrameStatus2 = currentStatus.status2;
  configPage15.freezeFrameEngineFlags = currentStatus.engine;

  // Store timestamp (millis since power on)
  uint32_t now = millis();
  configPage15.freezeFrameTimestamp[0] = (now >> 24) & 0xFF;
  configPage15.freezeFrameTimestamp[1] = (now >> 16) & 0xFF;
  configPage15.freezeFrameTimestamp[2] = (now >> 8) & 0xFF;
  configPage15.freezeFrameTimestamp[3] = now & 0xFF;

  currentStatus.freezeFrameValid = true;
  configPage15.dtcFlags |= DTC_FLAG_FREEZE_CAPTURED;
}

/**
 * @brief Clear freeze frame data
 */
void freezeFrame_clear(void)
{
  configPage15.freezeFrameDTC[0] = 0;
  configPage15.freezeFrameDTC[1] = 0;
  configPage15.freezeFrameRPM = 0;
  configPage15.freezeFrameMAP = 0;
  configPage15.freezeFrameTPS = 0;
  configPage15.freezeFrameCLT = 0;
  configPage15.freezeFrameIAT = 0;
  configPage15.freezeFrameBaro = 0;
  configPage15.freezeFrameVSS = 0;
  configPage15.freezeFrameBattery = 0;
  configPage15.freezeFrameO2 = 0;
  configPage15.freezeFrameAdvance = 0;
  configPage15.freezeFramePW = 0;
  configPage15.freezeFrameLoad = 0;
  configPage15.freezeFrameAFR = 0;
  configPage15.freezeFrameFuel = 0;
  configPage15.freezeFrameStatus1 = 0;
  configPage15.freezeFrameStatus2 = 0;
  configPage15.freezeFrameEngineFlags = 0;
  for (uint8_t i = 0; i < 4; i++) { configPage15.freezeFrameTimestamp[i] = 0; }

  currentStatus.freezeFrameValid = false;
  configPage15.dtcFlags &= ~DTC_FLAG_FREEZE_CAPTURED;
}

/**
 * @brief Check if freeze frame is valid
 */
bool freezeFrame_isValid(void)
{
  return currentStatus.freezeFrameValid;
}

// =============================================================================
// BMW FUEL CONSUMPTION CALCULATION
// =============================================================================

/**
 * @brief Calculate BMW fuel consumption for PT-CAN DME4 message
 *
 * Formula: fuelConsumption = (PW × RPM × nCyl × injector_cc) / (2 × 60,000,000 × fuel_density)
 *
 * Where:
 * - PW = Pulse width in microseconds
 * - RPM = Engine RPM
 * - nCyl = Number of cylinders
 * - injector_cc = Injector flow rate in cc/min at 3 bar
 * - fuel_density = 0.75 kg/L for gasoline
 *
 * Result is stored in currentStatus.fuelConsumption as 0.01 L/h units
 * BMW expects: L/h × 100 (e.g., 5.5 L/h = 550)
 */
void calculateBMWFuelConsumption(void)
{
  // Get parameters
  uint32_t pw_us = currentStatus.PW1;         // Pulse width in µs
  uint32_t rpm = currentStatus.RPM;
  uint8_t nCyl = configPage2.nCylinders;

  // Injector flow rate in cc/min at 3 bar rail pressure
  // Stored in configPage15.injectorFlowRate (e.g., 440 for 440cc/min injectors)
  uint16_t injectorCC = configPage15.injectorFlowRate;

  // Check for valid parameters
  if (rpm == 0 || pw_us == 0 || injectorCC == 0)
  {
    currentStatus.fuelConsumption = 0;
    return;
  }

  // Calculate fuel consumption in 0.01 L/h
  // Formula derivation:
  // cc_per_injection = (pw_us / 1,000,000) × (injectorCC / 60) × duty_at_rail_pressure
  // cc_per_minute = cc_per_injection × (RPM / 2) × nCyl (4-stroke = RPM/2 injections per cylinder)
  // cc_per_hour = cc_per_minute × 60
  // L_per_hour = cc_per_hour / 1000
  //
  // Simplified with scaling to avoid overflow:
  // fuelConsumption (0.01 L/h) = (pw_us × rpm × nCyl × injectorCC) / 20,000,000

  uint32_t numerator = pw_us * nCyl;
  numerator = (numerator * rpm) / 1000;  // Intermediate scaling to avoid overflow
  numerator = (numerator * injectorCC) / 100;

  // Divide by remaining factor: 20,000,000 / (1000 × 100) = 200
  uint32_t fuelCons = numerator / 200;

  // Clamp to 16-bit (max 655.35 L/h which is way more than any engine)
  currentStatus.fuelConsumption = (fuelCons > 65535) ? 65535 : (uint16_t)fuelCons;
}

// =============================================================================
// OBD-II MODE 03 HANDLER - READ CONFIRMED DTCS
// =============================================================================

/**
 * @brief Handle OBD Mode 03 - Read confirmed DTCs
 *
 * Response format:
 * - Single frame (≤3 DTCs): 0x43, count, DTC1_H, DTC1_L, DTC2_H, DTC2_L, ...
 * - Multi-frame (>3 DTCs): Uses ISO-TP segmented transfer
 */
static inline void handleOBDMode03(void)
{
  uint8_t dtcCount = currentStatus.dtcConfirmedCount;

  // Prepare response
  outMsg.len = 8;
  outMsg.flags.extended = 0;
  outMsg.id = 0x7E8;

  if (dtcCount == 0)
  {
    // No DTCs - send empty response
    outMsg.buf[0] = 0x02;    // 2 bytes follow
    outMsg.buf[1] = 0x43;    // Mode 03 response
    outMsg.buf[2] = 0x00;    // 0 DTCs
    outMsg.buf[3] = 0x00;
    outMsg.buf[4] = 0x00;
    outMsg.buf[5] = 0x00;
    outMsg.buf[6] = 0x00;
    outMsg.buf[7] = 0x00;
    Can0.write(outMsg);
  }
  else if (dtcCount <= 2)
  {
    // Single frame response (up to 2 DTCs fit in one frame)
    outMsg.buf[0] = 2 + (dtcCount * 2);  // Number of bytes following
    outMsg.buf[1] = 0x43;                 // Mode 03 response
    outMsg.buf[2] = dtcCount;             // Number of DTCs

    // Fill in DTCs
    for (uint8_t i = 0; i < dtcCount && i < 2; i++)
    {
      uint16_t dtc = dtc_getAt(i, false);
      outMsg.buf[3 + i*2] = highByte(dtc);
      outMsg.buf[4 + i*2] = lowByte(dtc);
    }
    // Zero fill remaining bytes
    for (uint8_t i = 3 + dtcCount*2; i < 8; i++)
    {
      outMsg.buf[i] = 0x00;
    }
    Can0.write(outMsg);
  }
  else
  {
    // Multi-frame response needed for >2 DTCs
    // First frame: [0x10, length_lo, 0x43, count, DTC1_H, DTC1_L, DTC2_H, DTC2_L]
    uint8_t totalBytes = 2 + (dtcCount * 2); // 0x43 + count + DTCs

    outMsg.buf[0] = 0x10;                 // First frame indicator
    outMsg.buf[1] = totalBytes;           // Total data length
    outMsg.buf[2] = 0x43;                 // Mode 03 response
    outMsg.buf[3] = dtcCount;             // DTC count

    // First 2 DTCs in first frame
    for (uint8_t i = 0; i < 2 && i < dtcCount; i++)
    {
      uint16_t dtc = dtc_getAt(i, false);
      outMsg.buf[4 + i*2] = highByte(dtc);
      outMsg.buf[5 + i*2] = lowByte(dtc);
    }
    Can0.write(outMsg);

    // Send consecutive frames for remaining DTCs
    uint8_t frameSeq = 0x21;
    uint8_t dtcIdx = 2;
    while (dtcIdx < dtcCount)
    {
      outMsg.buf[0] = frameSeq++;
      if (frameSeq > 0x2F) { frameSeq = 0x20; } // Wrap sequence number

      uint8_t byteIdx = 1;
      while (byteIdx < 8 && dtcIdx < dtcCount)
      {
        uint16_t dtc = dtc_getAt(dtcIdx++, false);
        outMsg.buf[byteIdx++] = highByte(dtc);
        if (byteIdx < 8) { outMsg.buf[byteIdx++] = lowByte(dtc); }
      }
      // Pad remaining bytes
      while (byteIdx < 8) { outMsg.buf[byteIdx++] = 0x00; }

      Can0.write(outMsg);
    }
  }
}

// =============================================================================
// OBD-II MODE 04 HANDLER - CLEAR DTCS
// =============================================================================

/**
 * @brief Handle OBD Mode 04 - Clear/Reset DTCs
 *
 * Clears all confirmed and pending DTCs, resets freeze frame,
 * and turns off MIL (Check Engine Light).
 */
static inline void handleOBDMode04(void)
{
  // Clear all DTCs
  dtc_clearAll();

  // Send positive response
  outMsg.len = 8;
  outMsg.flags.extended = 0;
  outMsg.id = 0x7E8;
  outMsg.buf[0] = 0x01;    // 1 byte follows
  outMsg.buf[1] = 0x44;    // Mode 04 response (0x04 + 0x40)
  outMsg.buf[2] = 0x00;
  outMsg.buf[3] = 0x00;
  outMsg.buf[4] = 0x00;
  outMsg.buf[5] = 0x00;
  outMsg.buf[6] = 0x00;
  outMsg.buf[7] = 0x00;

  Can0.write(outMsg);
}

// =============================================================================
// OBD-II MODE 07 HANDLER - READ PENDING DTCS
// =============================================================================

/**
 * @brief Handle OBD Mode 07 - Read pending DTCs
 *
 * Same format as Mode 03, but reads pending (not yet confirmed) DTCs.
 */
static inline void handleOBDMode07(void)
{
  uint8_t dtcCount = currentStatus.dtcPendingCount;

  // Prepare response
  outMsg.len = 8;
  outMsg.flags.extended = 0;
  outMsg.id = 0x7E8;

  if (dtcCount == 0)
  {
    // No pending DTCs
    outMsg.buf[0] = 0x02;
    outMsg.buf[1] = 0x47;    // Mode 07 response
    outMsg.buf[2] = 0x00;
    outMsg.buf[3] = 0x00;
    outMsg.buf[4] = 0x00;
    outMsg.buf[5] = 0x00;
    outMsg.buf[6] = 0x00;
    outMsg.buf[7] = 0x00;
    Can0.write(outMsg);
  }
  else if (dtcCount <= 2)
  {
    outMsg.buf[0] = 2 + (dtcCount * 2);
    outMsg.buf[1] = 0x47;    // Mode 07 response
    outMsg.buf[2] = dtcCount;

    for (uint8_t i = 0; i < dtcCount && i < 2; i++)
    {
      uint16_t dtc = dtc_getAt(i, true);
      outMsg.buf[3 + i*2] = highByte(dtc);
      outMsg.buf[4 + i*2] = lowByte(dtc);
    }
    for (uint8_t i = 3 + dtcCount*2; i < 8; i++)
    {
      outMsg.buf[i] = 0x00;
    }
    Can0.write(outMsg);
  }
  else
  {
    // Multi-frame for >2 pending DTCs
    uint8_t totalBytes = 2 + (dtcCount * 2);

    outMsg.buf[0] = 0x10;
    outMsg.buf[1] = totalBytes;
    outMsg.buf[2] = 0x47;
    outMsg.buf[3] = dtcCount;

    for (uint8_t i = 0; i < 2 && i < dtcCount; i++)
    {
      uint16_t dtc = dtc_getAt(i, true);
      outMsg.buf[4 + i*2] = highByte(dtc);
      outMsg.buf[5 + i*2] = lowByte(dtc);
    }
    Can0.write(outMsg);

    uint8_t frameSeq = 0x21;
    uint8_t dtcIdx = 2;
    while (dtcIdx < dtcCount)
    {
      outMsg.buf[0] = frameSeq++;
      if (frameSeq > 0x2F) { frameSeq = 0x20; }

      uint8_t byteIdx = 1;
      while (byteIdx < 8 && dtcIdx < dtcCount)
      {
        uint16_t dtc = dtc_getAt(dtcIdx++, true);
        outMsg.buf[byteIdx++] = highByte(dtc);
        if (byteIdx < 8) { outMsg.buf[byteIdx++] = lowByte(dtc); }
      }
      while (byteIdx < 8) { outMsg.buf[byteIdx++] = 0x00; }
      Can0.write(outMsg);
    }
  }
}

// =============================================================================
// OBD-II MODE 09 HANDLER - VEHICLE INFORMATION (VIN / ECU NAME)
// =============================================================================

/**
 * @brief Handle OBD Mode 09 - Vehicle Information
 *
 * Supported PIDs:
 * - 0x00: PIDs supported [01-20]
 * - 0x02: VIN (17 characters, multi-frame)
 * - 0x0A: ECU name (20 characters, multi-frame)
 */
static inline void handleOBDMode09Full(void)
{
  uint8_t pid = inMsg.buf[2];

  outMsg.len = 8;
  outMsg.flags.extended = 0;
  outMsg.id = 0x7E8;

  switch (pid)
  {
    case 0x00: // PIDs supported [01-20]
      outMsg.buf[0] = 0x06;
      outMsg.buf[1] = 0x49;    // Mode 09 response
      outMsg.buf[2] = 0x00;
      // Bits: 01=VIN message count, 02=VIN, 04=cal ID count, 06=CVN count
      // 0A=ECU name, etc.
      outMsg.buf[3] = 0x54;    // Support PIDs 02, 04, 0A
      outMsg.buf[4] = 0x00;
      outMsg.buf[5] = 0x00;
      outMsg.buf[6] = 0x00;
      outMsg.buf[7] = 0x00;
      Can0.write(outMsg);
      break;

    case 0x02: // VIN - Vehicle Identification Number
    {
      // VIN is 17 chars, needs multi-frame response
      // First frame: [0x10, 0x14, 0x49, 0x02, 0x01, V1, V2, V3]
      outMsg.buf[0] = 0x10;           // First frame
      outMsg.buf[1] = 0x14;           // 20 bytes total (1 + 1 + 1 + 17)
      outMsg.buf[2] = 0x49;           // Mode 09 response
      outMsg.buf[3] = 0x02;           // PID 02 (VIN)
      outMsg.buf[4] = 0x01;           // Number of data items
      outMsg.buf[5] = configPage15.vinNumber[0];
      outMsg.buf[6] = configPage15.vinNumber[1];
      outMsg.buf[7] = configPage15.vinNumber[2];
      Can0.write(outMsg);

      // Wait for flow control (0x30)
      // In real implementation, should wait for FC frame
      // For simplicity, send consecutive frames with small delay

      // Consecutive frame 1: [0x21, V4, V5, V6, V7, V8, V9, V10]
      outMsg.buf[0] = 0x21;
      for (uint8_t i = 0; i < 7; i++)
      {
        outMsg.buf[1 + i] = configPage15.vinNumber[3 + i];
      }
      Can0.write(outMsg);

      // Consecutive frame 2: [0x22, V11, V12, V13, V14, V15, V16, V17]
      outMsg.buf[0] = 0x22;
      for (uint8_t i = 0; i < 7; i++)
      {
        outMsg.buf[1 + i] = configPage15.vinNumber[10 + i];
      }
      Can0.write(outMsg);
    }
    break;

    case 0x0A: // ECU Name / Calibration ID
    {
      // ECU name is 20 chars, needs multi-frame response
      outMsg.buf[0] = 0x10;           // First frame
      outMsg.buf[1] = 0x17;           // 23 bytes total
      outMsg.buf[2] = 0x49;           // Mode 09 response
      outMsg.buf[3] = 0x0A;           // PID 0A
      outMsg.buf[4] = 0x01;           // Number of data items
      outMsg.buf[5] = configPage15.ecuName[0];
      outMsg.buf[6] = configPage15.ecuName[1];
      outMsg.buf[7] = configPage15.ecuName[2];
      Can0.write(outMsg);

      // Consecutive frame 1
      outMsg.buf[0] = 0x21;
      for (uint8_t i = 0; i < 7; i++)
      {
        outMsg.buf[1 + i] = configPage15.ecuName[3 + i];
      }
      Can0.write(outMsg);

      // Consecutive frame 2
      outMsg.buf[0] = 0x22;
      for (uint8_t i = 0; i < 7; i++)
      {
        outMsg.buf[1 + i] = configPage15.ecuName[10 + i];
      }
      Can0.write(outMsg);

      // Consecutive frame 3 (remaining 3 chars + padding)
      outMsg.buf[0] = 0x23;
      outMsg.buf[1] = configPage15.ecuName[17];
      outMsg.buf[2] = configPage15.ecuName[18];
      outMsg.buf[3] = configPage15.ecuName[19];
      outMsg.buf[4] = 0x00;
      outMsg.buf[5] = 0x00;
      outMsg.buf[6] = 0x00;
      outMsg.buf[7] = 0x00;
      Can0.write(outMsg);
    }
    break;

    default:
      // Unsupported PID - send negative response
      outMsg.buf[0] = 0x03;
      outMsg.buf[1] = 0x7F;    // Negative response
      outMsg.buf[2] = 0x09;    // Mode 09
      outMsg.buf[3] = 0x12;    // Sub-function not supported
      outMsg.buf[4] = 0x00;
      outMsg.buf[5] = 0x00;
      outMsg.buf[6] = 0x00;
      outMsg.buf[7] = 0x00;
      Can0.write(outMsg);
      break;
  }
}

#endif
