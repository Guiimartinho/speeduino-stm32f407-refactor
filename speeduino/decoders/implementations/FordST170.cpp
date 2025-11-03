#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

namespace {

/// FASE T: Ford ST170 VVT recording extraction
/// @brief Separates VVT angle recording concern from missing tooth sync logic
/// Reduces nesting from 4 levels to 2 (guard clauses)
/// @param revOne Revolution tracker
/// @param secToothCount Secondary tooth count
static inline void recordVVTAngle(uint8_t revOne, uint8_t secToothCount) {
    if (configPage6.vvtEnabled == 0) { return; }
    if (revOne != 1) { return; }
    if (secToothCount != 1) { return; }

    int16_t curAngle = getCrankAngle();
    while (curAngle > 360) { curAngle -= 360; }

    if (configPage6.vvtMode == VVT_MODE_CLOSED_LOOP) {
        curAngle = LOW_PASS_FILTER((curAngle << 1), configPage4.ANGLEFILTER_VVT, curAngle);
        currentStatus.vvt1Angle = 360 - curAngle - configPage10.vvtCL0DutyAng;
    }
}

/// @brief Process secondary trigger gap detection
/// @return true if gap detected
static inline bool detectSecondaryGap() {
    targetGap2 = (3 * (toothLastSecToothTime - toothLastMinusOneSecToothTime)) >> 1;
    toothLastMinusOneSecToothTime = toothLastSecToothTime;

    if ((curGap2 >= targetGap2) || (secondaryToothCount == 5)) {
        secondaryToothCount = 1;
        revolutionOne = 1;
        triggerSecFilterTime = 0;
        return true;
    }

    triggerSecFilterTime = curGap2 >> 2;
    secondaryToothCount++;
    return false;
}

/// @brief Calculate end tooth for ST170
/// @param ignitionAngle Ignition angle
/// @param toothAdder Tooth adder for sequential
/// @return End tooth number
static inline uint16_t calcSetEndTeeth(int ignitionAngle, uint8_t toothAdder) {
    int16_t tempEndTooth = ignitionAngle - configPage4.triggerAngle;
#ifdef USE_LIBDIVIDE
    tempEndTooth = libdivide::libdivide_s16_do(tempEndTooth, &divTriggerToothAngle);
#else
    tempEndTooth = tempEndTooth / (int16_t)triggerToothAngle;
#endif
    tempEndTooth = nudge(1, 36U + toothAdder, tempEndTooth - 1, 36U + toothAdder);
    return clampToActualTeeth((uint16_t)tempEndTooth, toothAdder);
}

} // anonymous namespace

void triggerSetup_FordST170(void) {
    configPage4.triggerTeeth = 36;
    configPage4.triggerMissingTeeth = 1;
    configPage4.TrigSpeed = CRANK_SPEED;

    triggerToothAngle = 360 / configPage4.triggerTeeth;
    triggerActualTeeth = configPage4.triggerTeeth - configPage4.triggerMissingTeeth;
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth));

    triggerSecFilterTime = MICROS_PER_MIN / MAX_RPM / 8U / 2U;

    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    checkSyncToothCount = (36) >> 1;
    toothLastMinusOneToothTime = 0;
    toothCurrentCount = 0;
    secondaryToothCount = 0;
    toothOneTime = 0;
    toothOneMinusOneTime = 0;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * (1U + 1U));
#ifdef USE_LIBDIVIDE
    divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

void triggerPri_FordST170(void) {
    triggerPri_missingTooth();
}

void triggerSec_FordST170(void) {
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;

    if ((toothLastSecToothTime == 0)) {
        curGap2 = 0;
        toothLastSecToothTime = curTime2;
    }

    if (curGap2 < triggerSecFilterTime) { return; }

    detectSecondaryGap();

    toothLastSecToothTime = curTime2;

    recordVVTAngle(revolutionOne, secondaryToothCount);
}

uint16_t getRPM_FordST170(void) {
    uint16_t tempRPM = 0;
    if (currentStatus.RPM < currentStatus.crankRPM) {
        if (toothCurrentCount != 1) {
            tempRPM = crankingGetRPM(36, CRANK_SPEED);
        } else {
            tempRPM = currentStatus.RPM;
        }
    } else {
        tempRPM = stdGetRPM(CRANK_SPEED);
    }
    return tempRPM;
}

int getCrankAngle_FordST170(void) {
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempRevolutionOne = revolutionOne;
    tempToothLastToothTime = toothLastToothTime;
    interrupts();

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;

    if ((tempRevolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED)) {
        crankAngle += 360;
    }

    lastCrankAngleCalc = micros();
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

void triggerSetEndTeeth_FordST170(void) {
    byte toothAdder = 0;
    if ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && (configPage4.TrigSpeed == CRANK_SPEED)) {
        toothAdder = 36;
    }

    ignition1EndTooth = calcSetEndTeeth(ignition1EndAngle, toothAdder);
    ignition2EndTooth = calcSetEndTeeth(ignition2EndAngle, toothAdder);
    ignition3EndTooth = calcSetEndTeeth(ignition3EndAngle, toothAdder);
    ignition4EndTooth = calcSetEndTeeth(ignition4EndAngle, toothAdder);
}
