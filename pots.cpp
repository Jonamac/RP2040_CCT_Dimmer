#include "pots.h"
#include "pins.h"
#include "modes.h"
#include "state.h"
#include "inputs.h"
#include "ledmix.h"
#include "freq_mode.h"
#include "calibration.h"
#include "display_ui.h"
#include "pots_state.h"

// ============================================================
//  ADC SMOOTHING
// ============================================================
int readADC(int pin) {
    long sum = 0;
    for (int i = 0; i < 12; i++) {
        sum += analogRead(pin);
    }
    return sum / 12;
}

// DEMO brightness tracking
static int prevDemoIdx = -1;

// NORMAL mode step tracking
static int prevDutyStepIndex = -1;
static int prevCCTStepIndex  = -1;

// DEMO speed tracking
static int lastDemoSpeedADC = -1;

// ============================================================
//  MAIN POT PROCESSOR
// ============================================================
void processPots(unsigned long now) {

    // ============================================================
    //  UNIVERSAL POT NORMALIZATION (runs once per frame)
    // ============================================================
    int rawDutyADC = readADC(DUTY_POT_PIN);
    int rawCCTADC  = readADC(CCT_POT_PIN);

    if (lastDutyADC < 0) lastDutyADC = rawDutyADC;
    if (abs(rawDutyADC - lastDutyADC) >= 20)
        lastDutyADC = rawDutyADC;

    if (lastCCTADC < 0) lastCCTADC = rawCCTADC;
    if (abs(rawCCTADC - lastCCTADC) >= 20)
        lastCCTADC = rawCCTADC;

    float dutyNorm = (lastDutyADC - DUTY_MIN_RAW) /
                     float(DUTY_MAX_RAW - DUTY_MIN_RAW);
    dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);
    lastDutyNorm = dutyNorm;

    float cctNorm = (lastCCTADC - CCT_MIN_RAW) /
                    float(CCT_MAX_RAW - CCT_MIN_RAW);
    cctNorm = constrain(cctNorm, 0.0f, 1.0f);
    lastCCTNorm = cctNorm;

    float mappedCCT = 2700.0f + cctNorm * (6500.0f - 2700.0f);
    lastMappedCCT = mappedCCT;

    // ============================================================
    //  DUMB MODE — RAW ANALOG CONTROL
    // ============================================================
    if (currentMode == MODE_DUMB && !dumbFadeActive) {

        Serial.println("[DUMB] Processing pots");

        // Brightness (linear, min_duty aware)
        float linB = min_duty + dutyNorm * (1.0f - min_duty);
        currentBrightness = linB;
        targetBrightness  = linB;

        // CCT direct mapping
        float cct = mappedCCT;
        currentCCT = cct;
        targetCCT  = cct;

        Serial.print("[DUMB] linB=");
        Serial.print(linB, 6);
        Serial.print(" CCT=");
        Serial.println(cct);

        applyLEDsImmediate(currentBrightness, currentCCT);
        return;
    }

    // ============================================================
    //  STANDBY MODE — ONLY DEMO SPEED ADJUSTMENT
    // ============================================================
    if (currentMode == MODE_STANDBY) {

        if (previousMode == MODE_DEMO) {

            Serial.println("[STANDBY] Adjusting DEMO speed");

            if (lastDemoSpeedADC < 0) lastDemoSpeedADC = rawCCTADC;
            if (abs(rawCCTADC - lastDemoSpeedADC) < 40)
                return;

            lastDemoSpeedADC = rawCCTADC;

            int speedIndex = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
            speedIndex = constrain(speedIndex, 0, 4);

            const int demoSpeeds[5] = {500, 1000, 2500, 3500, 5000};
            const int speedPct[5]   = {100, 75, 50, 25, 1};

            demo_mode_fade_ms   = demoSpeeds[speedIndex];
            demoSpeedPercent    = speedPct[speedIndex];
            demoSpeedFlashUntil = now + 600;

            Serial.print("[STANDBY] DEMO speed=");
            Serial.println(demoSpeedPercent);
        }

        return;
    }

    // ============================================================
    //  DEMO MODE — BRIGHTNESS STEPS
    // ============================================================
    if (currentMode == MODE_DEMO) {

        Serial.println("[DEMO] Processing brightness");

        const float demoSteps[7] = {
            min_duty,
            0.025f,
            0.05f,
            0.10f,
            0.15f,
            0.20f,
            0.25f
        };

        int idx = map(lastDutyADC, DUTY_MIN_RAW, DUTY_MAX_RAW, 0, 6);
        idx = constrain(idx, 0, 6);

        if (idx != prevDemoIdx) {
            prevDemoIdx = idx;

            targetBrightness = demoSteps[idx];
            currentBrightness = targetBrightness;

            Serial.print("[DEMO] Brightness step=");
            Serial.print(idx);
            Serial.print(" B=");
            Serial.println(targetBrightness, 4);

            applyLEDsImmediate(currentBrightness, currentCCT);
            lastInputChangeTime = now;
        }

        // DEMO SPEED (CCT pot)
        if (lastDemoSpeedADC < 0) lastDemoSpeedADC = rawCCTADC;
        if (abs(rawCCTADC - lastDemoSpeedADC) >= 40) {

            lastDemoSpeedADC = rawCCTADC;

            int speedIndex = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
            speedIndex = constrain(speedIndex, 0, 4);

            const int demoSpeeds[5] = {500, 1000, 2500, 3500, 5000};
            const int speedPct[5]   = {100, 75, 50, 25, 1};

            demo_mode_fade_ms   = demoSpeeds[speedIndex];
            demoSpeedPercent    = speedPct[speedIndex];
            demoSpeedFlashUntil = now + 600;

            Serial.print("[DEMO] Speed=");
            Serial.println(demoSpeedPercent);
        }

        return;
    }

    // ============================================================
    //  FREQ MODE — BRIGHTNESS + FREQUENCY
    // ============================================================
    if (currentMode == MODE_FREQ) {

        Serial.println("[FREQ] Processing pots");

        // Brightness
        targetBrightness = brightnessTableLookup(dutyNorm);
        currentBrightness = targetBrightness; // bypass fade

        // Freeze CCT
        targetCCT = currentCCT;

        // Frequency
        int idx = round(cctNorm * (FREQ_STEPS - 1));
        idx = constrain(idx, 0, FREQ_STEPS - 1);
        freqStrobeHz = freqTable[idx];

        Serial.print("[FREQ] B=");
        Serial.print(targetBrightness, 4);
        Serial.print(" Hz=");
        Serial.println(freqStrobeHz);

        return;
    }

    // ============================================================
    //  CAL MODE — CCT PRESETS
    // ============================================================
    if (currentMode == MODE_CAL) {

        Serial.println("[CAL] Processing pots");

        int idx = map(lastCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
        idx = constrain(idx, 0, 4);

        if (idx != calPresetIndex) {
            calPresetIndex = idx;
            targetCCT = calPresets[idx];

            Serial.print("[CAL] CCT preset=");
            Serial.println(targetCCT);

            lastInputChangeTime = now;
        }

        return;
    }

    // ============================================================
    //  NORMAL MODE — BRIGHTNESS + CCT
    // ============================================================
    if (currentMode == MODE_NORMAL) {

        Serial.println("[NORMAL] Processing pots");

        // ----- BRIGHTNESS -----
        int idealIdx = (int)round(dutyNorm * (NORMAL_STEPS - 1));
        idealIdx = constrain(idealIdx, 0, NORMAL_STEPS - 1);

        if (idealIdx != prevDutyStepIndex) {
            prevDutyStepIndex = idealIdx;

            targetBrightness = normalBrightnessSteps[idealIdx];

            Serial.print("[NORMAL] Brightness step=");
            Serial.print(idealIdx);
            Serial.print(" B=");
            Serial.println(targetBrightness, 4);

            lastInputChangeTime = now;
        }

        // ----- CCT -----
        float rawCCT = mappedCCT;
        int stepIndex = (int)round((rawCCT - 2700.0f) / 100.0f);
        stepIndex = constrain(stepIndex, 0, 38);

        if (stepIndex != prevCCTStepIndex) {
            prevCCTStepIndex = stepIndex;

            targetCCT = 2700.0f + stepIndex * 100.0f;

            Serial.print("[NORMAL] CCT step=");
            Serial.print(stepIndex);
            Serial.print(" CCT=");
            Serial.println(targetCCT);

            lastInputChangeTime = now;
        }

        return;
    }
}