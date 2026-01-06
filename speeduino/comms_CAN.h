#ifndef COMMS_CAN_H
#define COMMS_CAN_H

//For BMW e46/e39/e38, rover and mini other CAN instrument clusters
#define CAN_BMW_ASC1 0x153 //Rx message from ACS unit that includes speed
#define CAN_BMW_DME1 0x316 //Tx message that includes RPM
#define CAN_BMW_DME2 0x329 //Tx message that includes CLT and TPS
#define CAN_BMW_DME4 0x545 //Tx message that includes CLT and TPS
#define CAN_BMW_ICL2 0x613
#define CAN_BMW_ICL3 0x615

//For VAG CAN instrument clusters
#define CAN_VAG_RPM 0x280
#define CAN_VAG_VSS 0x5A0

//For Haltech IC-7 and IC-10 digital dashes
#define CAN_HALTECH_DATA1   0x360 //RPM, MAP, TPS, Coolant Pressure. 50Hz
#define CAN_HALTECH_DATA2   0x361 //Fuel Pressure, Oil Pressure, Load, Wastegate Pressure. 50Hz
#define CAN_HALTECH_DATA3   0x362 //Advance, INJ Stage 1/2 duty cycles. 50Hz
#define CAN_HALTECH_PW      0x364 //Pulsewidth 1-4. 50Hz
#define CAN_HALTECH_LAMBDA  0x368 //Lambda 1-4. 20Hz
#define CAN_HALTECH_TRIGGER 0x369 //Trigger Counter, sync level, sync error count. 20Hz
#define CAN_HALTECH_VSS     0x370 //VSS, current gear and inlet cam angles. 20Hz
#define CAN_HALTECH_DATA4   0x372 //Baro, BatteryV, Target boost. 10Hz
#define CAN_HALTECH_DATA5   0x3E0 //IAT, CLT, Fuel Temp, Oil Temp. 10Hz

#define CAN_BROADCAST_PROTOCOL_OFF      0
#define CAN_BROADCAST_PROTOCOL_BMW      1
#define CAN_BROADCAST_PROTOCOL_VAG      2
#define CAN_BROADCAST_PROTOCOL_HALTECH  3


#define CAN_WBO_RUSEFI 1
#define CAN_WBO_AEM 2

#define TS_CAN_OFFSET 0x100

// =============================================================================
// OBD-II DTC (Diagnostic Trouble Code) Definitions
// =============================================================================

// DTC type prefixes (first 2 bits of high byte)
#define DTC_TYPE_P0     0x00    ///< Powertrain generic (P0xxx)
#define DTC_TYPE_P1     0x40    ///< Powertrain manufacturer (P1xxx)
#define DTC_TYPE_P2     0x80    ///< Powertrain generic (P2xxx)
#define DTC_TYPE_P3     0xC0    ///< Powertrain manufacturer (P3xxx)

// Common DTCs for engine management
#define DTC_P0105       0x0105  ///< MAP sensor circuit malfunction
#define DTC_P0107       0x0107  ///< MAP sensor low input
#define DTC_P0108       0x0108  ///< MAP sensor high input
#define DTC_P0110       0x0110  ///< IAT sensor circuit malfunction
#define DTC_P0112       0x0112  ///< IAT sensor low input
#define DTC_P0113       0x0113  ///< IAT sensor high input
#define DTC_P0115       0x0115  ///< CLT sensor circuit malfunction
#define DTC_P0117       0x0117  ///< CLT sensor low input
#define DTC_P0118       0x0118  ///< CLT sensor high input
#define DTC_P0120       0x0120  ///< TPS circuit malfunction
#define DTC_P0130       0x0130  ///< O2 sensor circuit malfunction
#define DTC_P0171       0x0171  ///< System too lean (Bank 1)
#define DTC_P0172       0x0172  ///< System too rich (Bank 1)
#define DTC_P0217       0x0217  ///< Engine overtemp condition
#define DTC_P0219       0x0219  ///< Engine overspeed condition
#define DTC_P0230       0x0230  ///< Fuel pump primary circuit malfunction
#define DTC_P0335       0x0335  ///< Crankshaft position sensor A circuit
#define DTC_P0336       0x0336  ///< Crankshaft position sensor A range/performance
#define DTC_P0340       0x0340  ///< Camshaft position sensor A circuit
#define DTC_P0520       0x0520  ///< Engine oil pressure sensor circuit
#define DTC_P0562       0x0562  ///< System voltage low
#define DTC_P0563       0x0563  ///< System voltage high
#define DTC_P1000       0x5000  ///< OBD system readiness not complete (manufacturer)

// Maximum DTCs that can be stored
#define DTC_MAX_CONFIRMED   10
#define DTC_MAX_PENDING     10

// DTC status flags
#define DTC_FLAG_MIL_ON             0x01    ///< MIL (CEL) lamp on
#define DTC_FLAG_FREEZE_CAPTURED    0x02    ///< Freeze frame data captured

#if defined(NATIVE_CAN_AVAILABLE)

// =============================================================================
// Core CAN Functions
// =============================================================================
void initCAN();
int CAN_read();
void CAN_write();
void sendCANBroadcast(uint8_t);
void receiveCANwbo();
void DashMessages(uint16_t DashMessageID);
void can_Command(void);
void obd_response(uint8_t therequestedPID , uint8_t therequestedPIDlow, uint8_t therequestedPIDhigh);
void readAuxCanBus();

// =============================================================================
// OBD-II DTC Functions (Mode 03/04/07)
// =============================================================================

/**
 * @brief Set a DTC (Diagnostic Trouble Code)
 * @param dtcCode 16-bit DTC code (e.g., 0x0105 for P0105)
 * @param isPending true for pending DTC (Mode 07), false for confirmed (Mode 03)
 * @return true if DTC was stored, false if storage full
 */
bool dtc_set(uint16_t dtcCode, bool isPending);

/**
 * @brief Clear a specific DTC
 * @param dtcCode DTC code to clear
 * @return true if DTC was found and cleared
 */
bool dtc_clear(uint16_t dtcCode);

/**
 * @brief Clear all DTCs (Mode 04 response)
 * Clears both confirmed and pending DTCs, resets freeze frame
 */
void dtc_clearAll(void);

/**
 * @brief Get count of stored DTCs
 * @param isPending true for pending DTCs, false for confirmed
 * @return Number of stored DTCs
 */
uint8_t dtc_getCount(bool isPending);

/**
 * @brief Get DTC at specified index
 * @param index DTC index (0-9)
 * @param isPending true for pending DTCs, false for confirmed
 * @return 16-bit DTC code, or 0 if invalid index
 */
uint16_t dtc_getAt(uint8_t index, bool isPending);

/**
 * @brief Initialize DTC subsystem from stored values
 */
void dtc_init(void);

/**
 * @brief Promote pending DTC to confirmed after driving cycle
 * @param dtcCode DTC to promote
 */
void dtc_promote(uint16_t dtcCode);

// =============================================================================
// Freeze Frame Functions
// =============================================================================

/**
 * @brief Capture freeze frame data at current engine state
 * @param triggerDTC The DTC that triggered the capture
 */
void freezeFrame_capture(uint16_t triggerDTC);

/**
 * @brief Clear freeze frame data
 */
void freezeFrame_clear(void);

/**
 * @brief Check if freeze frame is valid
 * @return true if freeze frame data exists
 */
bool freezeFrame_isValid(void);

// =============================================================================
// BMW Fuel Consumption Calculation
// =============================================================================

/**
 * @brief Calculate BMW fuel consumption for PT-CAN DME4 message
 * Formula: (PW_us × RPM × nCylinders × injector_cc) / (2 × 60,000,000 × fuel_density)
 * Result in 0.01 L/h units
 */
void calculateBMWFuelConsumption(void);

// =============================================================================
// External Variables
// =============================================================================

extern CAN_message_t outMsg;
extern CAN_message_t inMsg;

#endif // NATIVE_CAN_AVAILABLE
#endif // COMMS_CAN_H
