#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

namespace {

struct Subaru67SyncConfig {
    uint8_t secondaryCount;
    uint8_t expectedTooth1;
    uint8_t expectedTooth2;
    uint8_t defaultToothCount;
};

static const Subaru67SyncConfig subaru67SyncConfigs[3] = {
    {1, 5, 11, 5},
    {2, 8,  0, 8},
    {3, 2,  0, 2}
};

static inline bool validateSubaru67Sync(uint8_t secondaryCount, uint8_t toothCount, uint8_t* outToothCount)
{
    for (uint8_t i = 0; i < 3; i++) {
        const Subaru67SyncConfig* config = &subaru67SyncConfigs[i];
        if (config->secondaryCount == secondaryCount) {
            bool isValid = (toothCount == config->expectedTooth1);
            if (config->expectedTooth2 != 0) {
                isValid = isValid || (toothCount == config->expectedTooth2);
            }
            if (!isValid) {
                *outToothCount = config->defaultToothCount;
            }
            return isValid;
        }
    }
    return false;
}

} // namespace

void triggerSetup_Subaru67(void)
{
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL));
    triggerSecFilterTime = 0;
    secondaryToothCount = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 1;
    triggerToothAngle = 2;
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    toothSystemCount = 0;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 93U);

    toothAngles[0] = 710;
    toothAngles[1] = 83;
    toothAngles[2] = 115;
    toothAngles[3] = 170;
    toothAngles[4] = toothAngles[1] + 180;
    toothAngles[5] = toothAngles[2] + 180;
    toothAngles[6] = toothAngles[3] + 180;
    toothAngles[7] = toothAngles[1] + 360;
    toothAngles[8] = toothAngles[2] + 360;
    toothAngles[9] = toothAngles[3] + 360;
    toothAngles[10] = toothAngles[1] + 540;
    toothAngles[11] = toothAngles[2] + 540;
}

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

    if (toothCurrentCount > 13) {
        toothCurrentCount = 0;
        currentStatus.hasSync = false;
        currentStatus.syncLossCounter++;
    }

    if (secondaryToothCount == 0) {
        // No action
    } else if (secondaryToothCount >= 1 && secondaryToothCount <= 3) {
        uint8_t newToothCount = toothCurrentCount;
        bool isValid = validateSubaru67Sync(secondaryToothCount, toothCurrentCount, &newToothCount);

        if (isValid) {
            currentStatus.hasSync = true;
        } else {
            currentStatus.hasSync = false;
            currentStatus.syncLossCounter++;
            toothCurrentCount = newToothCount;
        }
        secondaryToothCount = 0;
    } else {
        currentStatus.hasSync = false;
        BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
        currentStatus.syncLossCounter++;
        secondaryToothCount = 0;
    }

    if (currentStatus.hasSync == true) {
        if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && configPage4.ignCranklock) {
            if ((toothCurrentCount == 1) || (toothCurrentCount == 7)) {
                endCoil1Charge();
                endCoil3Charge();
            } else if ((toothCurrentCount == 4) || (toothCurrentCount == 10)) {
                endCoil2Charge();
                endCoil4Charge();
            }
        }

        if (toothCurrentCount > 12) {
            toothCurrentCount = 1;
            toothOneMinusOneTime = toothOneTime;
            toothOneTime = curTime;
            currentStatus.startRevolutions++;
        }

        if (toothCurrentCount == 1) {
            triggerToothAngle = 55;
        } else if (toothCurrentCount == 2) {
            triggerToothAngle = 93;
        } else {
            triggerToothAngle = toothAngles[(toothCurrentCount-1)] - toothAngles[(toothCurrentCount-2)];
        }
        BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);

        if ((configPage2.perToothIgn == true) && (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK))) {
            int16_t crankAngle = toothAngles[(toothCurrentCount - 1)] + configPage4.triggerAngle;
            if (configPage4.sparkMode != IGN_MODE_SEQUENTIAL) {
                crankAngle = ignitionLimits(toothAngles[(toothCurrentCount-1)]);
                if ((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (toothCurrentCount > 6)) {
                    checkPerToothTiming(crankAngle, (toothCurrentCount-6));
                } else {
                    checkPerToothTiming(crankAngle, toothCurrentCount);
                }
            } else {
                checkPerToothTiming(crankAngle, toothCurrentCount);
            }
        }
    }
}

void triggerSec_Subaru67(void)
{
    if ((toothSystemCount == 0) || (toothSystemCount == 3)) {
        curTime2 = micros();
        curGap2 = curTime2 - toothLastSecToothTime;

        if (curGap2 > triggerSecFilterTime) {
            toothLastSecToothTime = curTime2;
            secondaryToothCount++;
            toothSystemCount = 0;

            if (secondaryToothCount > 1) {
                triggerSecFilterTime = curGap2 >> 2;
            } else {
                triggerSecFilterTime = 0;
            }
        }
    } else {
        if (toothSystemCount > 3) {
            toothSystemCount = 0;
            secondaryToothCount = 1;
            currentStatus.hasSync = false;
            currentStatus.syncLossCounter++;
        }
        secondaryToothCount = 0;
    }
}

uint16_t getRPM_Subaru67(void)
{
    uint16_t tempRPM = 0;
    if (currentStatus.startRevolutions > 0) {
        tempRPM = stdGetRPM(CAM_SPEED);
    }
    return tempRPM;
}

int getCrankAngle_Subaru67(void)
{
    int crankAngle = 0;
    if (currentStatus.hasSync == true) {
        unsigned long tempToothLastToothTime;
        int tempToothCurrentCount;

        noInterrupts();
        tempToothCurrentCount = toothCurrentCount;
        tempToothLastToothTime = toothLastToothTime;
        lastCrankAngleCalc = micros();
        interrupts();

        crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;

        elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
        crankAngle += timeToAngleIntervalTooth(elapsedTime);

        if (crankAngle >= 720) { crankAngle -= 720; }
        if (crankAngle < 0) { crankAngle += 360; }
    }

    return crankAngle;
}

void triggerSetEndTeeth_Subaru67(void)
{
    if (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) {
        if (currentStatus.advance >= 10) {
            ignition1EndTooth = 12;
            ignition2EndTooth = 3;
            ignition3EndTooth = 6;
            ignition4EndTooth = 9;
        } else {
            ignition1EndTooth = 1;
            ignition2EndTooth = 4;
            ignition3EndTooth = 7;
            ignition4EndTooth = 10;
        }
    } else {
        if (currentStatus.advance >= 10) {
            ignition1EndTooth = 6;
            ignition2EndTooth = 3;
        } else {
            ignition1EndTooth = 1;
            ignition2EndTooth = 4;
        }
    }
}
