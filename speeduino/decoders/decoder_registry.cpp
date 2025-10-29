/**
 * @file decoder_registry.cpp
 * @brief Decoder registry implementation - table-driven dispatch
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * PRESERVES 100% LOGIC from decoders.cpp
 * Original: Giant switch-case or if-else chain
 * Modular: O(1) table lookup by decoder ID
 *
 * This registry contains all 28 supported decoders.
 * Each decoder is accessed via direct array index (decoder ID).
 */

#include "decoder_registry.h"
#include "decoder_declarations.h"
#include "../decoders.h" // For DECODER_* defines

// Anonymous namespace for private data
namespace {

/**
 * @brief Decoder registry table - direct indexing by decoder ID
 * @note Array is indexed by DECODER_* defines (0-28)
 * @note Initialized at compile-time (const data in flash)
 * @note Table size: 28 entries × ~32 bytes = ~896 bytes ROM
 */
static const DecoderInterface decoderRegistry[] = {
  // DECODER 0: Missing Tooth (36-1, 60-2, etc.) - Most common
  {
    .setup = &triggerSetup_missingTooth,
    .primaryISR = &triggerPri_missingTooth,
    .secondaryISR = &triggerSec_missingTooth,
    .thirdISR = &triggerThird_missingTooth,
    .getRPM = &getRPM_missingTooth,
    .getCrankAngle = &getCrankAngle_missingTooth,
    .name = "Missing Tooth",
    .decoderID = DECODER_MISSING_TOOTH
  },

  // DECODER 1: Basic Distributor
  {
    .setup = &triggerSetup_BasicDistributor,
    .primaryISR = &triggerPri_BasicDistributor,
    .secondaryISR = &triggerSec_BasicDistributor,
    .thirdISR = &triggerThird_BasicDistributor,
    .getRPM = &getRPM_BasicDistributor,
    .getCrankAngle = &getCrankAngle_BasicDistributor,
    .name = "Basic Distributor",
    .decoderID = DECODER_BASIC_DISTRIBUTOR
  },

  // DECODER 2: Dual Wheel
  {
    .setup = &triggerSetup_DualWheel,
    .primaryISR = &triggerPri_DualWheel,
    .secondaryISR = &triggerSec_DualWheel,
    .thirdISR = &triggerThird_DualWheel,
    .getRPM = &getRPM_DualWheel,
    .getCrankAngle = &getCrankAngle_DualWheel,
    .name = "Dual Wheel",
    .decoderID = DECODER_DUAL_WHEEL
  },

  // DECODER 3: GM 7X
  {
    .setup = &triggerSetup_GM7X,
    .primaryISR = &triggerPri_GM7X,
    .secondaryISR = &triggerSec_GM7X,
    .thirdISR = &triggerThird_GM7X,
    .getRPM = &getRPM_GM7X,
    .getCrankAngle = &getCrankAngle_GM7X,
    .name = "GM 7X",
    .decoderID = DECODER_GM7X
  },

  // DECODER 4: 4G63 (Mitsubishi)
  {
    .setup = &triggerSetup_4G63,
    .primaryISR = &triggerPri_4G63,
    .secondaryISR = &triggerSec_4G63,
    .thirdISR = &triggerThird_4G63,
    .getRPM = &getRPM_4G63,
    .getCrankAngle = &getCrankAngle_4G63,
    .name = "4G63 Mitsubishi",
    .decoderID = DECODER_4G63
  },

  // DECODER 5: GM 24X
  {
    .setup = &triggerSetup_24X,
    .primaryISR = &triggerPri_24X,
    .secondaryISR = &triggerSec_24X,
    .thirdISR = &triggerThird_24X,
    .getRPM = &getRPM_24X,
    .getCrankAngle = &getCrankAngle_24X,
    .name = "GM 24X",
    .decoderID = DECODER_24X
  },

  // DECODER 6: Jeep 2000
  {
    .setup = &triggerSetup_Jeep2000,
    .primaryISR = &triggerPri_Jeep2000,
    .secondaryISR = &triggerSec_Jeep2000,
    .thirdISR = &triggerThird_Jeep2000,
    .getRPM = &getRPM_Jeep2000,
    .getCrankAngle = &getCrankAngle_Jeep2000,
    .name = "Jeep 2000",
    .decoderID = DECODER_JEEP2000
  },

  // DECODER 7: Audi 135
  {
    .setup = &triggerSetup_Audi135,
    .primaryISR = &triggerPri_Audi135,
    .secondaryISR = &triggerSec_Audi135,
    .thirdISR = &triggerThird_Audi135,
    .getRPM = &getRPM_Audi135,
    .getCrankAngle = &getCrankAngle_Audi135,
    .name = "Audi 135",
    .decoderID = DECODER_AUDI135
  },

  // DECODER 8: Honda D17
  {
    .setup = &triggerSetup_HondaD17,
    .primaryISR = &triggerPri_HondaD17,
    .secondaryISR = &triggerSec_HondaD17,
    .thirdISR = &triggerThird_HondaD17,
    .getRPM = &getRPM_HondaD17,
    .getCrankAngle = &getCrankAngle_HondaD17,
    .name = "Honda D17",
    .decoderID = DECODER_HONDA_D17
  },

  // DECODER 9: Miata 99-05
  {
    .setup = &triggerSetup_Miata9905,
    .primaryISR = &triggerPri_Miata9905,
    .secondaryISR = &triggerSec_Miata9905,
    .thirdISR = &triggerThird_Miata9905,
    .getRPM = &getRPM_Miata9905,
    .getCrankAngle = &getCrankAngle_Miata9905,
    .name = "Miata 99-05",
    .decoderID = DECODER_MIATA_9905
  },

  // DECODER 10: Mazda AU
  {
    .setup = &triggerSetup_MazdaAU,
    .primaryISR = &triggerPri_MazdaAU,
    .secondaryISR = &triggerSec_MazdaAU,
    .thirdISR = &triggerThird_MazdaAU,
    .getRPM = &getRPM_MazdaAU,
    .getCrankAngle = &getCrankAngle_MazdaAU,
    .name = "Mazda AU",
    .decoderID = DECODER_MAZDA_AU
  },

  // DECODER 11: Non-360
  {
    .setup = &triggerSetup_non360,
    .primaryISR = &triggerPri_non360,
    .secondaryISR = &triggerSec_non360,
    .thirdISR = &triggerThird_non360,
    .getRPM = &getRPM_non360,
    .getCrankAngle = &getCrankAngle_non360,
    .name = "Non-360",
    .decoderID = DECODER_NON360
  },

  // DECODER 12: Nissan 360
  {
    .setup = &triggerSetup_Nissan360,
    .primaryISR = &triggerPri_Nissan360,
    .secondaryISR = &triggerSec_Nissan360,
    .thirdISR = &triggerThird_Nissan360,
    .getRPM = &getRPM_Nissan360,
    .getCrankAngle = &getCrankAngle_Nissan360,
    .name = "Nissan 360",
    .decoderID = DECODER_NISSAN_360
  },

  // DECODER 13: Subaru 6/7
  {
    .setup = &triggerSetup_Subaru67,
    .primaryISR = &triggerPri_Subaru67,
    .secondaryISR = &triggerSec_Subaru67,
    .thirdISR = &triggerThird_Subaru67,
    .getRPM = &getRPM_Subaru67,
    .getCrankAngle = &getCrankAngle_Subaru67,
    .name = "Subaru 6/7",
    .decoderID = DECODER_SUBARU_67
  },

  // DECODER 14: Daihatsu +1
  {
    .setup = &triggerSetup_Daihatsu,
    .primaryISR = &triggerPri_Daihatsu,
    .secondaryISR = &triggerSec_Daihatsu,
    .thirdISR = &triggerThird_Daihatsu,
    .getRPM = &getRPM_Daihatsu,
    .getCrankAngle = &getCrankAngle_Daihatsu,
    .name = "Daihatsu +1",
    .decoderID = DECODER_DAIHATSU_PLUS1
  },

  // DECODER 15: Harley Davidson
  {
    .setup = &triggerSetup_Harley,
    .primaryISR = &triggerPri_Harley,
    .secondaryISR = &triggerSec_Harley,
    .thirdISR = &triggerThird_Harley,
    .getRPM = &getRPM_Harley,
    .getCrankAngle = &getCrankAngle_Harley,
    .name = "Harley Davidson",
    .decoderID = DECODER_HARLEY
  },

  // DECODER 16: 36-2-2-2
  {
    .setup = &triggerSetup_ThirtySixMinus222,
    .primaryISR = &triggerPri_ThirtySixMinus222,
    .secondaryISR = &triggerSec_ThirtySixMinus222,
    .thirdISR = &triggerThird_ThirtySixMinus222,
    .getRPM = &getRPM_ThirtySixMinus222,
    .getCrankAngle = &getCrankAngle_ThirtySixMinus222,
    .name = "36-2-2-2",
    .decoderID = DECODER_36_2_2_2
  },

  // DECODER 17: 36-2-1
  {
    .setup = &triggerSetup_ThirtySixMinus21,
    .primaryISR = &triggerPri_ThirtySixMinus21,
    .secondaryISR = &triggerSec_ThirtySixMinus21,
    .thirdISR = &triggerThird_ThirtySixMinus21,
    .getRPM = &getRPM_ThirtySixMinus21,
    .getCrankAngle = &getCrankAngle_ThirtySixMinus21,
    .name = "36-2-1",
    .decoderID = DECODER_36_2_1
  },

  // DECODER 18: 420a (Chrysler)
  {
    .setup = &triggerSetup_420a,
    .primaryISR = &triggerPri_420a,
    .secondaryISR = &triggerSec_420a,
    .thirdISR = &triggerThird_420a,
    .getRPM = &getRPM_420a,
    .getCrankAngle = &getCrankAngle_420a,
    .name = "Chrysler 420a",
    .decoderID = DECODER_420A
  },

  // DECODER 19: Weber-Marelli
  {
    .setup = &triggerSetup_Weber,
    .primaryISR = &triggerPri_Weber,
    .secondaryISR = &triggerSec_Weber,
    .thirdISR = &triggerThird_Weber,
    .getRPM = &getRPM_Weber,
    .getCrankAngle = &getCrankAngle_Weber,
    .name = "Weber-Marelli",
    .decoderID = DECODER_WEBER
  },

  // DECODER 20: Ford ST170
  {
    .setup = &triggerSetup_FordST170,
    .primaryISR = &triggerPri_FordST170,
    .secondaryISR = &triggerSec_FordST170,
    .thirdISR = &triggerThird_FordST170,
    .getRPM = &getRPM_FordST170,
    .getCrankAngle = &getCrankAngle_FordST170,
    .name = "Ford ST170",
    .decoderID = DECODER_ST170
  },

  // DECODER 21: DRZ400 (Suzuki)
  {
    .setup = &triggerSetup_DRZ400,
    .primaryISR = &triggerPri_DRZ400,
    .secondaryISR = &triggerSec_DRZ400,
    .thirdISR = &triggerThird_DRZ400,
    .getRPM = &getRPM_DRZ400,
    .getCrankAngle = &getCrankAngle_DRZ400,
    .name = "Suzuki DRZ400",
    .decoderID = DECODER_DRZ400
  },

  // DECODER 22: NGC (Chrysler)
  {
    .setup = &triggerSetup_NGC,
    .primaryISR = &triggerPri_NGC,
    .secondaryISR = &triggerSec_NGC,
    .thirdISR = &triggerThird_NGC,
    .getRPM = &getRPM_NGC,
    .getCrankAngle = &getCrankAngle_NGC,
    .name = "Chrysler NGC",
    .decoderID = DECODER_NGC
  },

  // DECODER 23: Yamaha VMAX
  {
    .setup = &triggerSetup_Vmax,
    .primaryISR = &triggerPri_Vmax,
    .secondaryISR = &triggerSec_Vmax,
    .thirdISR = &triggerThird_Vmax,
    .getRPM = &getRPM_Vmax,
    .getCrankAngle = &getCrankAngle_Vmax,
    .name = "Yamaha VMAX",
    .decoderID = DECODER_VMAX
  },

  // DECODER 24: Renix (Jeep)
  {
    .setup = &triggerSetup_Renix,
    .primaryISR = &triggerPri_Renix,
    .secondaryISR = &triggerSec_Renix,
    .thirdISR = &triggerThird_Renix,
    .getRPM = &getRPM_Renix,
    .getCrankAngle = &getCrankAngle_Renix,
    .name = "Jeep Renix",
    .decoderID = DECODER_RENIX
  },

  // DECODER 25: Rover MEMS
  {
    .setup = &triggerSetup_RoverMEMS,
    .primaryISR = &triggerPri_RoverMEMS,
    .secondaryISR = &triggerSec_RoverMEMS,
    .thirdISR = &triggerThird_RoverMEMS,
    .getRPM = &getRPM_RoverMEMS,
    .getCrankAngle = &getCrankAngle_RoverMEMS,
    .name = "Rover MEMS",
    .decoderID = DECODER_ROVERMEMS
  },

  // DECODER 26: Suzuki K6A
  {
    .setup = &triggerSetup_SuzukiK6A,
    .primaryISR = &triggerPri_SuzukiK6A,
    .secondaryISR = &triggerSec_SuzukiK6A,
    .thirdISR = &triggerThird_SuzukiK6A,
    .getRPM = &getRPM_SuzukiK6A,
    .getCrankAngle = &getCrankAngle_SuzukiK6A,
    .name = "Suzuki K6A",
    .decoderID = DECODER_SUZUKI_K6A
  },

  // DECODER 27: Honda J32
  {
    .setup = &triggerSetup_HondaJ32,
    .primaryISR = &triggerPri_HondaJ32,
    .secondaryISR = &triggerSec_HondaJ32,
    .thirdISR = &triggerThird_HondaJ32,
    .getRPM = &getRPM_HondaJ32,
    .getCrankAngle = &getCrankAngle_HondaJ32,
    .name = "Honda J32",
    .decoderID = DECODER_HONDA_J32
  },

  // DECODER 28: Ford TFI
  {
    .setup = &triggerSetup_FordTFI,
    .primaryISR = &triggerPri_FordTFI,
    .secondaryISR = &triggerSec_FordTFI,
    .thirdISR = &triggerThird_FordTFI,
    .getRPM = &getRPM_FordTFI,
    .getCrankAngle = &getCrankAngle_FordTFI,
    .name = "Ford TFI",
    .decoderID = DECODER_FORD_TFI
  }
};

static const size_t decoderRegistrySize = sizeof(decoderRegistry) / sizeof(DecoderInterface);

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

const DecoderInterface* decoderRegistryGet(uint8_t decoderID)
{
  // Direct O(1) table lookup
  // Guard clause: Check bounds
  if (decoderID >= decoderRegistrySize) {
    return NULL;
  }

  return &decoderRegistry[decoderID];
}

const DecoderInterface* decoderRegistryGetDefault(void)
{
  // Default to Missing Tooth (most common decoder)
  return &decoderRegistry[DECODER_MISSING_TOOTH];
}

uint8_t decoderRegistryGetCount(void)
{
  return (uint8_t)decoderRegistrySize;
}

bool decoderRegistryIsValid(uint8_t decoderID)
{
  return (decoderID < decoderRegistrySize);
}
