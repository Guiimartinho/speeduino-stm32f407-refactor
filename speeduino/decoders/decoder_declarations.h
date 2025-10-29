/**
 * @file decoder_declarations.h
 * @brief Forward declarations for all 28 decoder implementations
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This file contains forward declarations for all decoder functions.
 * Extracted from decoders.h (original lines 87-352)
 *
 * @note All decoders follow same 6-function pattern:
 *       - triggerSetup_<name>()
 *       - triggerPri_<name>()
 *       - triggerSec_<name>()
 *       - triggerThird_<name>()
 *       - getRPM_<name>()
 *       - getCrankAngle_<name>()
 */

#ifndef DECODER_DECLARATIONS_H
#define DECODER_DECLARATIONS_H

#include <stdint.h>

// ============================================================================
// DECODER 0: Missing Tooth (most common - 36-1, 60-2, etc.)
// ============================================================================
void triggerSetup_missingTooth(void);
void triggerPri_missingTooth(void);
void triggerSec_missingTooth(void);
void triggerThird_missingTooth(void);
uint16_t getRPM_missingTooth(void);
int getCrankAngle_missingTooth(void);
void triggerSetEndTeeth_missingTooth(void);

// ============================================================================
// DECODER 1: Basic Distributor
// ============================================================================
void triggerSetup_BasicDistributor(void);
void triggerPri_BasicDistributor(void);
void triggerSec_BasicDistributor(void);
void triggerThird_BasicDistributor(void);
uint16_t getRPM_BasicDistributor(void);
int getCrankAngle_BasicDistributor(void);
void triggerSetEndTeeth_BasicDistributor(void);

// ============================================================================
// DECODER 2: Dual Wheel
// ============================================================================
void triggerSetup_DualWheel(void);
void triggerPri_DualWheel(void);
void triggerSec_DualWheel(void);
void triggerThird_DualWheel(void);
uint16_t getRPM_DualWheel(void);
int getCrankAngle_DualWheel(void);
void triggerSetEndTeeth_DualWheel(void);

// ============================================================================
// DECODER 3: GM 7X
// ============================================================================
void triggerSetup_GM7X(void);
void triggerPri_GM7X(void);
void triggerSec_GM7X(void);
void triggerThird_GM7X(void);
uint16_t getRPM_GM7X(void);
int getCrankAngle_GM7X(void);
void triggerSetEndTeeth_GM7X(void);

// ============================================================================
// DECODER 4: 4G63 (Mitsubishi)
// ============================================================================
void triggerSetup_4G63(void);
void triggerPri_4G63(void);
void triggerSec_4G63(void);
void triggerThird_4G63(void);
uint16_t getRPM_4G63(void);
int getCrankAngle_4G63(void);
void triggerSetEndTeeth_4G63(void);

// ============================================================================
// DECODER 5: GM 24X
// ============================================================================
void triggerSetup_24X(void);
void triggerPri_24X(void);
void triggerSec_24X(void);
void triggerThird_24X(void);
uint16_t getRPM_24X(void);
int getCrankAngle_24X(void);
void triggerSetEndTeeth_24X(void);

// ============================================================================
// DECODER 6: Jeep 2000
// ============================================================================
void triggerSetup_Jeep2000(void);
void triggerPri_Jeep2000(void);
void triggerSec_Jeep2000(void);
void triggerThird_Jeep2000(void);
uint16_t getRPM_Jeep2000(void);
int getCrankAngle_Jeep2000(void);
void triggerSetEndTeeth_Jeep2000(void);

// ============================================================================
// DECODER 7: Audi 135
// ============================================================================
void triggerSetup_Audi135(void);
void triggerPri_Audi135(void);
void triggerSec_Audi135(void);
void triggerThird_Audi135(void);
uint16_t getRPM_Audi135(void);
int getCrankAngle_Audi135(void);
void triggerSetEndTeeth_Audi135(void);

// ============================================================================
// DECODER 8: Honda D17
// ============================================================================
void triggerSetup_HondaD17(void);
void triggerPri_HondaD17(void);
void triggerSec_HondaD17(void);
void triggerThird_HondaD17(void);
uint16_t getRPM_HondaD17(void);
int getCrankAngle_HondaD17(void);
void triggerSetEndTeeth_HondaD17(void);

// ============================================================================
// DECODER 9: Miata 99-05
// ============================================================================
void triggerSetup_Miata9905(void);
void triggerPri_Miata9905(void);
void triggerSec_Miata9905(void);
void triggerThird_Miata9905(void);
uint16_t getRPM_Miata9905(void);
int getCrankAngle_Miata9905(void);
void triggerSetEndTeeth_Miata9905(void);

// ============================================================================
// DECODER 10: Mazda AU
// ============================================================================
void triggerSetup_MazdaAU(void);
void triggerPri_MazdaAU(void);
void triggerSec_MazdaAU(void);
void triggerThird_MazdaAU(void);
uint16_t getRPM_MazdaAU(void);
int getCrankAngle_MazdaAU(void);
void triggerSetEndTeeth_MazdaAU(void);

// ============================================================================
// DECODER 11: Non-360
// ============================================================================
void triggerSetup_non360(void);
void triggerPri_non360(void);
void triggerSec_non360(void);
void triggerThird_non360(void);
uint16_t getRPM_non360(void);
int getCrankAngle_non360(void);
void triggerSetEndTeeth_non360(void);

// ============================================================================
// DECODER 12: Nissan 360
// ============================================================================
void triggerSetup_Nissan360(void);
void triggerPri_Nissan360(void);
void triggerSec_Nissan360(void);
void triggerThird_Nissan360(void);
uint16_t getRPM_Nissan360(void);
int getCrankAngle_Nissan360(void);
void triggerSetEndTeeth_Nissan360(void);

// ============================================================================
// DECODER 13: Subaru 6/7
// ============================================================================
void triggerSetup_Subaru67(void);
void triggerPri_Subaru67(void);
void triggerSec_Subaru67(void);
void triggerThird_Subaru67(void);
uint16_t getRPM_Subaru67(void);
int getCrankAngle_Subaru67(void);
void triggerSetEndTeeth_Subaru67(void);

// ============================================================================
// DECODER 14: Daihatsu +1
// ============================================================================
void triggerSetup_Daihatsu(void);
void triggerPri_Daihatsu(void);
void triggerSec_Daihatsu(void);
void triggerThird_Daihatsu(void);
uint16_t getRPM_Daihatsu(void);
int getCrankAngle_Daihatsu(void);
void triggerSetEndTeeth_Daihatsu(void);

// ============================================================================
// DECODER 15: Harley Davidson
// ============================================================================
void triggerSetup_Harley(void);
void triggerPri_Harley(void);
void triggerSec_Harley(void);
void triggerThird_Harley(void);
uint16_t getRPM_Harley(void);
int getCrankAngle_Harley(void);
void triggerSetEndTeeth_Harley(void);

// ============================================================================
// DECODER 16: 36-2-2-2
// ============================================================================
void triggerSetup_ThirtySixMinus222(void);
void triggerPri_ThirtySixMinus222(void);
void triggerSec_ThirtySixMinus222(void);
void triggerThird_ThirtySixMinus222(void);
uint16_t getRPM_ThirtySixMinus222(void);
int getCrankAngle_ThirtySixMinus222(void);
void triggerSetEndTeeth_ThirtySixMinus222(void);

// ============================================================================
// DECODER 17: 36-2-1
// ============================================================================
void triggerSetup_ThirtySixMinus21(void);
void triggerPri_ThirtySixMinus21(void);
void triggerSec_ThirtySixMinus21(void);
void triggerThird_ThirtySixMinus21(void);
uint16_t getRPM_ThirtySixMinus21(void);
int getCrankAngle_ThirtySixMinus21(void);
void triggerSetEndTeeth_ThirtySixMinus21(void);

// ============================================================================
// DECODER 18: 420a (Chrysler)
// ============================================================================
void triggerSetup_420a(void);
void triggerPri_420a(void);
void triggerSec_420a(void);
void triggerThird_420a(void);
uint16_t getRPM_420a(void);
int getCrankAngle_420a(void);
void triggerSetEndTeeth_420a(void);

// ============================================================================
// DECODER 19: Weber-Marelli
// ============================================================================
void triggerSetup_Weber(void);
void triggerPri_Weber(void);
void triggerSec_Weber(void);
void triggerThird_Weber(void);
uint16_t getRPM_Weber(void);
int getCrankAngle_Weber(void);
void triggerSetEndTeeth_Weber(void);

// ============================================================================
// DECODER 20: Ford ST170
// ============================================================================
void triggerSetup_FordST170(void);
void triggerPri_FordST170(void);
void triggerSec_FordST170(void);
void triggerThird_FordST170(void);
uint16_t getRPM_FordST170(void);
int getCrankAngle_FordST170(void);
void triggerSetEndTeeth_FordST170(void);

// ============================================================================
// DECODER 21: DRZ400 (Suzuki)
// ============================================================================
void triggerSetup_DRZ400(void);
void triggerPri_DRZ400(void);
void triggerSec_DRZ400(void);
void triggerThird_DRZ400(void);
uint16_t getRPM_DRZ400(void);
int getCrankAngle_DRZ400(void);
void triggerSetEndTeeth_DRZ400(void);

// ============================================================================
// DECODER 22: NGC (Chrysler)
// ============================================================================
void triggerSetup_NGC(void);
void triggerPri_NGC(void);
void triggerSec_NGC(void);
void triggerThird_NGC(void);
uint16_t getRPM_NGC(void);
int getCrankAngle_NGC(void);
void triggerSetEndTeeth_NGC(void);

// ============================================================================
// DECODER 23: Yamaha VMAX
// ============================================================================
void triggerSetup_Vmax(void);
void triggerPri_Vmax(void);
void triggerSec_Vmax(void);
void triggerThird_Vmax(void);
uint16_t getRPM_Vmax(void);
int getCrankAngle_Vmax(void);
void triggerSetEndTeeth_Vmax(void);

// ============================================================================
// DECODER 24: Renix (Jeep)
// ============================================================================
void triggerSetup_Renix(void);
void triggerPri_Renix(void);
void triggerSec_Renix(void);
void triggerThird_Renix(void);
uint16_t getRPM_Renix(void);
int getCrankAngle_Renix(void);
void triggerSetEndTeeth_Renix(void);

// ============================================================================
// DECODER 25: Rover MEMS
// ============================================================================
void triggerSetup_RoverMEMS(void);
void triggerPri_RoverMEMS(void);
void triggerSec_RoverMEMS(void);
void triggerThird_RoverMEMS(void);
uint16_t getRPM_RoverMEMS(void);
int getCrankAngle_RoverMEMS(void);
void triggerSetEndTeeth_RoverMEMS(void);

// ============================================================================
// DECODER 26: Suzuki K6A
// ============================================================================
void triggerSetup_SuzukiK6A(void);
void triggerPri_SuzukiK6A(void);
void triggerSec_SuzukiK6A(void);
void triggerThird_SuzukiK6A(void);
uint16_t getRPM_SuzukiK6A(void);
int getCrankAngle_SuzukiK6A(void);
void triggerSetEndTeeth_SuzukiK6A(void);

// ============================================================================
// DECODER 27: Honda J32
// ============================================================================
void triggerSetup_HondaJ32(void);
void triggerPri_HondaJ32(void);
void triggerSec_HondaJ32(void);
void triggerThird_HondaJ32(void);
uint16_t getRPM_HondaJ32(void);
int getCrankAngle_HondaJ32(void);
void triggerSetEndTeeth_HondaJ32(void);

// ============================================================================
// DECODER 28: Ford TFI
// ============================================================================
void triggerSetup_FordTFI(void);
void triggerPri_FordTFI(void);
void triggerSec_FordTFI(void);
void triggerThird_FordTFI(void);
uint16_t getRPM_FordTFI(void);
int getCrankAngle_FordTFI(void);
void triggerSetEndTeeth_FordTFI(void);

#endif // DECODER_DECLARATIONS_H
