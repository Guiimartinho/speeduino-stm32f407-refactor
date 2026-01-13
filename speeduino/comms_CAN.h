#ifndef COMMS_CAN_H
#define COMMS_CAN_H

// =============================================================================
// BMW E46/E39/E38 PT-CAN Messages (500 kbps)
// =============================================================================

// DME TX Messages (ECU -> Network)
#define CAN_BMW_DME1 0x316  ///< TX: RPM, status flags. 10ms cycle
#define CAN_BMW_DME2 0x329  ///< TX: CLT, TPS, pedal position. 10ms cycle
#define CAN_BMW_DME4 0x545  ///< TX: Fuel consumption, oil temp. 100ms cycle

// ASC/DSC RX Messages (ASC -> ECU)
#define CAN_BMW_ASC1 0x153  ///< RX: Wheel speeds from ASC/DSC unit
#define CAN_BMW_ASC2 0x1F0  ///< RX: Brake status, yaw rate

// EGS (Transmission) RX Messages (EGS -> ECU)
#define CAN_BMW_EGS1 0x1F3  ///< RX: Current gear, transmission status
#define CAN_BMW_EGS2 0x1F5  ///< RX: Torque request, shift status

// Instrument Cluster Messages
#define CAN_BMW_ICL2 0x613  ///< RX: Instrument cluster status, MIL request
#define CAN_BMW_ICL3 0x615  ///< RX: Odometer value

// Steering Angle Sensor
#define CAN_BMW_SAS  0x0C8  ///< RX: Steering angle for traction control

// Electronic Throttle
#define CAN_BMW_ETC1 0x0B4  ///< TX: Electronic throttle position
#define CAN_BMW_ETC2 0x0B5  ///< TX: Electronic throttle status

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

// =============================================================================
// Common DTCs for engine management (OBD-II Generic P0xxx)
// =============================================================================

// MAF/MAP Sensor (P0100-P0109)
#define DTC_P0100       0x0100  ///< MAF sensor circuit malfunction
#define DTC_P0101       0x0101  ///< MAF sensor range/performance
#define DTC_P0102       0x0102  ///< MAF sensor low input
#define DTC_P0103       0x0103  ///< MAF sensor high input
#define DTC_P0105       0x0105  ///< MAP sensor circuit malfunction
#define DTC_P0107       0x0107  ///< MAP sensor low input
#define DTC_P0108       0x0108  ///< MAP sensor high input

// IAT Sensor (P0110-P0119)
#define DTC_P0110       0x0110  ///< IAT sensor circuit malfunction
#define DTC_P0112       0x0112  ///< IAT sensor low input
#define DTC_P0113       0x0113  ///< IAT sensor high input

// CLT Sensor (P0115-P0119)
#define DTC_P0115       0x0115  ///< CLT sensor circuit malfunction
#define DTC_P0117       0x0117  ///< CLT sensor low input
#define DTC_P0118       0x0118  ///< CLT sensor high input

// TPS Sensor (P0120-P0129)
#define DTC_P0120       0x0120  ///< TPS circuit malfunction
#define DTC_P0121       0x0121  ///< TPS range/performance
#define DTC_P0122       0x0122  ///< TPS low input
#define DTC_P0123       0x0123  ///< TPS high input

// O2 Sensor Bank 1 (P0130-P0144)
#define DTC_P0130       0x0130  ///< O2 sensor circuit malfunction (Bank 1 Sensor 1)
#define DTC_P0131       0x0131  ///< O2 sensor low voltage (B1S1)
#define DTC_P0132       0x0132  ///< O2 sensor high voltage (B1S1)
#define DTC_P0133       0x0133  ///< O2 sensor slow response (B1S1)
#define DTC_P0134       0x0134  ///< O2 sensor no activity (B1S1)
#define DTC_P0135       0x0135  ///< O2 heater circuit malfunction (B1S1)
#define DTC_P0136       0x0136  ///< O2 sensor circuit malfunction (B1S2)
#define DTC_P0137       0x0137  ///< O2 sensor low voltage (B1S2)
#define DTC_P0138       0x0138  ///< O2 sensor high voltage (B1S2)
#define DTC_P0140       0x0140  ///< O2 sensor no activity (B1S2)
#define DTC_P0141       0x0141  ///< O2 heater circuit malfunction (B1S2)

// Fuel System (P0170-P0175)
#define DTC_P0171       0x0171  ///< System too lean (Bank 1)
#define DTC_P0172       0x0172  ///< System too rich (Bank 1)
#define DTC_P0174       0x0174  ///< System too lean (Bank 2)
#define DTC_P0175       0x0175  ///< System too rich (Bank 2)

// Oil Temperature Sensor (P0195-P0199) - Gol AP 1.8 / BMW
#define DTC_P0195       0x0195  ///< Engine oil temperature sensor malfunction
#define DTC_P0196       0x0196  ///< Engine oil temperature sensor range/performance
#define DTC_P0197       0x0197  ///< Engine oil temperature sensor low
#define DTC_P0198       0x0198  ///< Engine oil temperature sensor high

// Engine Overtemp/Overspeed (P0217-P0219)
#define DTC_P0217       0x0217  ///< Engine overtemp condition
#define DTC_P0218       0x0218  ///< Transmission fluid overtemp
#define DTC_P0219       0x0219  ///< Engine overspeed condition

// Fuel Pump (P0230-P0233)
#define DTC_P0230       0x0230  ///< Fuel pump primary circuit malfunction
#define DTC_P0231       0x0231  ///< Fuel pump secondary circuit low
#define DTC_P0232       0x0232  ///< Fuel pump secondary circuit high

// Misfire Detection (P0300-P0312) - Critical for Gol AP 1.8
#define DTC_P0300       0x0300  ///< Random/multiple cylinder misfire
#define DTC_P0301       0x0301  ///< Cylinder 1 misfire
#define DTC_P0302       0x0302  ///< Cylinder 2 misfire
#define DTC_P0303       0x0303  ///< Cylinder 3 misfire
#define DTC_P0304       0x0304  ///< Cylinder 4 misfire
#define DTC_P0305       0x0305  ///< Cylinder 5 misfire
#define DTC_P0306       0x0306  ///< Cylinder 6 misfire
#define DTC_P0307       0x0307  ///< Cylinder 7 misfire
#define DTC_P0308       0x0308  ///< Cylinder 8 misfire

// Crankshaft Position Sensor (P0335-P0339)
#define DTC_P0335       0x0335  ///< Crankshaft position sensor A circuit
#define DTC_P0336       0x0336  ///< Crankshaft position sensor A range/performance
#define DTC_P0337       0x0337  ///< Crankshaft position sensor A low input
#define DTC_P0338       0x0338  ///< Crankshaft position sensor A high input

// Camshaft Position Sensor (P0340-P0349)
#define DTC_P0340       0x0340  ///< Camshaft position sensor A circuit
#define DTC_P0341       0x0341  ///< Camshaft position sensor A range/performance
#define DTC_P0342       0x0342  ///< Camshaft position sensor A low input
#define DTC_P0343       0x0343  ///< Camshaft position sensor A high input

// Ignition System (P0350-P0362)
#define DTC_P0351       0x0351  ///< Ignition coil A primary/secondary circuit
#define DTC_P0352       0x0352  ///< Ignition coil B primary/secondary circuit
#define DTC_P0353       0x0353  ///< Ignition coil C primary/secondary circuit
#define DTC_P0354       0x0354  ///< Ignition coil D primary/secondary circuit

// Catalyst Efficiency (P0420-P0424) - PROCONVE L6
#define DTC_P0420       0x0420  ///< Catalyst system efficiency below threshold (B1)
#define DTC_P0421       0x0421  ///< Warm up catalyst efficiency below threshold (B1)

// EVAP System (P0440-P0456) - PROCONVE L6
#define DTC_P0440       0x0440  ///< EVAP system malfunction
#define DTC_P0441       0x0441  ///< EVAP system incorrect purge flow
#define DTC_P0442       0x0442  ///< EVAP system small leak detected
#define DTC_P0443       0x0443  ///< EVAP purge control valve circuit
#define DTC_P0444       0x0444  ///< EVAP purge control valve circuit open
#define DTC_P0445       0x0445  ///< EVAP purge control valve circuit shorted
#define DTC_P0446       0x0446  ///< EVAP vent control circuit

// Vehicle Speed Sensor (P0500-P0503)
#define DTC_P0500       0x0500  ///< Vehicle speed sensor malfunction
#define DTC_P0501       0x0501  ///< Vehicle speed sensor range/performance
#define DTC_P0502       0x0502  ///< Vehicle speed sensor low input
#define DTC_P0503       0x0503  ///< Vehicle speed sensor intermittent/high

// Idle Control System (P0505-P0509)
#define DTC_P0505       0x0505  ///< Idle control system malfunction
#define DTC_P0506       0x0506  ///< Idle control system RPM lower than expected
#define DTC_P0507       0x0507  ///< Idle control system RPM higher than expected

// Oil Pressure (P0520-P0524)
#define DTC_P0520       0x0520  ///< Engine oil pressure sensor circuit
#define DTC_P0521       0x0521  ///< Engine oil pressure sensor range/performance
#define DTC_P0522       0x0522  ///< Engine oil pressure sensor low
#define DTC_P0523       0x0523  ///< Engine oil pressure sensor high
#define DTC_P0524       0x0524  ///< Engine oil pressure too low

// System Voltage (P0560-P0563)
#define DTC_P0560       0x0560  ///< System voltage malfunction
#define DTC_P0562       0x0562  ///< System voltage low
#define DTC_P0563       0x0563  ///< System voltage high

// Knock Sensor (P0325-P0334)
#define DTC_P0325       0x0325  ///< Knock sensor 1 circuit malfunction (Bank 1)
#define DTC_P0326       0x0326  ///< Knock sensor 1 range/performance
#define DTC_P0327       0x0327  ///< Knock sensor 1 low input
#define DTC_P0328       0x0328  ///< Knock sensor 1 high input
#define DTC_P0330       0x0330  ///< Knock sensor 2 circuit malfunction (Bank 2)

// Fuel Injectors (P0200-P0214)
#define DTC_P0200       0x0200  ///< Injector circuit malfunction
#define DTC_P0201       0x0201  ///< Injector circuit malfunction - Cylinder 1
#define DTC_P0202       0x0202  ///< Injector circuit malfunction - Cylinder 2
#define DTC_P0203       0x0203  ///< Injector circuit malfunction - Cylinder 3
#define DTC_P0204       0x0204  ///< Injector circuit malfunction - Cylinder 4
#define DTC_P0205       0x0205  ///< Injector circuit malfunction - Cylinder 5
#define DTC_P0206       0x0206  ///< Injector circuit malfunction - Cylinder 6
#define DTC_P0207       0x0207  ///< Injector circuit malfunction - Cylinder 7
#define DTC_P0208       0x0208  ///< Injector circuit malfunction - Cylinder 8

// =============================================================================
// Manufacturer Specific DTCs (P1xxx) - BMW E46 / VW Gol
// =============================================================================
#define DTC_P1000       0x5000  ///< OBD system readiness not complete

// BMW E46 Specific
#define DTC_P1083       0x5083  ///< BMW: Fuel control mixture lean (Bank 1)
#define DTC_P1084       0x5084  ///< BMW: Fuel control mixture rich (Bank 1)
#define DTC_P1085       0x5085  ///< BMW: Fuel control mixture lean (Bank 2)
#define DTC_P1086       0x5086  ///< BMW: Fuel control mixture rich (Bank 2)
#define DTC_P1188       0x5188  ///< BMW: Fuel system trim at lean limit
#define DTC_P1189       0x5189  ///< BMW: Fuel system trim at rich limit
#define DTC_P1519       0x5519  ///< BMW: Idle control valve stuck open
#define DTC_P1520       0x5520  ///< BMW: Idle control valve stuck closed
#define DTC_P1523       0x5523  ///< BMW: Intake manifold tuning valve stuck
#define DTC_P1550       0x5550  ///< BMW: Throttle actuator malfunction

// VW Gol AP 1.8 Specific
#define DTC_P1127       0x5127  ///< VW: Long term fuel trim mult. Bank 1 system too rich
#define DTC_P1128       0x5128  ///< VW: Long term fuel trim mult. Bank 1 system too lean
#define DTC_P1136       0x5136  ///< VW: Long term fuel trim add. Bank 1 system too rich
#define DTC_P1137       0x5137  ///< VW: Long term fuel trim add. Bank 1 system too lean
#define DTC_P1176       0x5176  ///< VW: O2 correction behind catalyst B1 limit attained
#define DTC_P1213       0x5213  ///< VW: Cylinder 1 fuel injector short to positive
#define DTC_P1214       0x5214  ///< VW: Cylinder 2 fuel injector short to positive
#define DTC_P1215       0x5215  ///< VW: Cylinder 3 fuel injector short to positive
#define DTC_P1216       0x5216  ///< VW: Cylinder 4 fuel injector short to positive
#define DTC_P1297       0x5297  ///< VW: Connection between turbo and throttle valve disrupted
#define DTC_P1545       0x5545  ///< VW: Throttle position control malfunction

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
void receiveBMWCanBus(void);  ///< Process BMW E46 PT-CAN RX messages

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
