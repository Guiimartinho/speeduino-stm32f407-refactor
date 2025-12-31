/*
All functions in the gamma file return

*/
#ifndef CORRECTIONS_H
#define CORRECTIONS_H

#include "table2d.h"
#include "table3d.h"

#define IGN_IDLE_THRESHOLD 200 //RPM threshold (below CL idle target) for when ign based idle control will engage

void initialiseCorrections(void);
uint16_t correctionsFuel(void);
uint8_t calculateAfrTarget(table3d16RpmLoad &afrLookUpTable, const statuses &current, const config2 &page2, const config6 &page6);
byte correctionWUE(void); //Warmup enrichment
uint16_t correctionCranking(void); //Cranking enrichment
byte correctionASE(void); //After Start Enrichment
uint16_t correctionAccel(void); //Acceleration Enrichment
byte correctionFloodClear(void); //Check for flood clear on cranking
byte correctionAFRClosedLoop(void); //Closed loop AFR adjustment
byte correctionFlex(void); //Flex fuel adjustment
byte correctionFuelTemp(void); //Fuel temp correction
byte correctionBatVoltage(void); //Battery voltage correction
byte correctionIATDensity(void); //Inlet temp density correction
byte correctionBaro(void); //Barometric pressure correction
byte correctionLaunch(void); //Launch control correction
byte correctionDFCOfuel(void); //DFCO taper correction
bool correctionDFCO(void); //Decelleration fuel cutoff

int8_t correctionsIgn(int8_t advance);
int8_t correctionFixedTiming(int8_t advance);
int8_t correctionCrankingFixedTiming(int8_t advance);
int8_t correctionFlexTiming(int8_t advance);
int8_t correctionWMITiming(int8_t advance);
int8_t correctionKnockTiming(int8_t advance);
int8_t correctionIATretard(int8_t advance);
int8_t correctionCLTadvance(int8_t advance);
int8_t correctionIdleAdvance(int8_t advance);
int8_t correctionSoftRevLimit(int8_t advance);
int8_t correctionNitrous(int8_t advance);
int8_t correctionSoftLaunch(int8_t advance);
int8_t correctionSoftFlatShift(int8_t advance);
int8_t correctionKnock(int8_t advance);
int8_t correctionDFCOignition(int8_t advance);

uint16_t correctionsDwell(uint16_t dwell);

extern byte activateMAPDOT; //The mapDOT value seen when the MAE was activated.
extern byte activateTPSDOT; //The tpsDOT value seen when the MAE was activated.

extern uint16_t AFRnextCycle;
extern uint8_t aseTaper;
extern uint8_t dfcoDelay;
extern uint8_t idleAdvTaper;
extern uint8_t crankingEnrichTaper;
extern uint8_t dfcoTaper;

// Correction tables - made global for modular access (defined in corrections.cpp)
extern table2D_u8_u8_4 taeTable;
extern table2D_u8_u8_4 maeTable;
extern table2D_u8_u8_10 WUETable;
extern table2D_u8_u8_4 ASETable;
extern table2D_u8_u8_4 ASECountTable;
extern table2D_u8_u8_4 crankingEnrichTable;
extern table2D_u8_u8_6 dwellVCorrectionTable;
extern table2D_u8_u8_6 injectorVCorrectionTable;
extern table2D_u8_u8_9 IATDensityCorrectionTable;
extern table2D_u8_u8_8 baroFuelTable;
extern table2D_u8_u8_6 IATRetardTable;
extern table2D_u8_u8_6 idleAdvanceTable;
extern table2D_u8_u8_6 CLTAdvanceTable;
extern table2D_u8_u8_6 flexFuelTable;
extern table2D_u8_u8_6 flexAdvTable;
extern table2D_u8_u8_6 fuelTempTable;
extern table2D_u8_u8_6 wmiAdvTable;

// ============================================================================
// MODULARIZED CORRECTIONS SYSTEM
// ============================================================================
// New modular architecture with interface-based corrections management
// All correction implementations preserved 100% in corrections.cpp
// Coordinator provides unified API with modular subsystems:
// - Fuel corrections (WUE, ASE, cranking, accel, environmental, DFCO)
// - Ignition corrections (environmental, timing, protection, special)
// - Dwell corrections (battery voltage compensation)
// - AFR corrections (target calculation, closed loop)
// ============================================================================
//
// Include modular corrections coordinator (optional - for future migration)
// For now, all correction code remains in corrections.cpp
// Modular headers available in corrections/ directory for future extraction
//
// #include "corrections/corrections_coordinator.h"
//
// When ready to use modular system, uncomment above and replace direct
// correction function calls with correctionsCoordinator* functions

#endif // CORRECTIONS_H
