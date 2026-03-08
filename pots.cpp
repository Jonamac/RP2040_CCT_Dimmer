#include "pots.h"
#include "pins.h"
#include "modes.h"
#include "state.h"
#include "inputs.h"        // <-- must come before using lastCCTADC
#include "ledmix.h"        // <-- must come before brightnessTableLookup
#include "freq_mode.h"     // <-- for freqTable and FREQ_STEPS
#include "calibration.h"
#include "display_ui.h"
#include "pots_state.h"

// Smooth ADC by averaging 12 samples
int readADC(int pin) {
    long sum = 0;
    for (int i = 0; i < 12; i++) {
        sum += analogRead(pin);
    }
    return sum / 12;
}

// Normal mode brightness steps (declared in calibration.h)
extern const float normalBrightnessSteps[];
extern const int NORMAL_STEPS;

// DEMO brightness tracking
static int prevDemoIdx  = -1;

// Shared pot state (global)
int prevDutyStepIndex = -1;
int prevCCTStepIndex  = -1;

// DEMO speed tracking
static int lastDemoSpeedADC = -1;

// NORMAL/OVERRIDE brightness tracking
static int dutyStepIndex = -1;

// Reset DEMO brightness tracking
void resetDemoBrightnessTracking() {
    prevDemoIdx       = -1;
    lastDutyADC       = -1;
    lastDemoSpeedADC  = -1;
}

// -----------------------------
//  DUMB MODE SWITCH HANDLING MOVED TO MODES.CPP
// -----------------------------

// -----------------------------
//  POT LOGIC
// -----------------------------
void processPots(unsigned long now) {

    //debug
        if (currentMode == MODE_DUMB) {
        Serial.println("DUMB block running");
    }

    // First handle DUMB switch transitions
    // handleDumbSwitch(now); //handled in readInputs()

    // ===== DUTY POT =====
    int rawDutyADC = readADC(DUTY_POT_PIN);

    if (lastDutyADC < 0) lastDutyADC = rawDutyADC;
    if (abs(rawDutyADC - lastDutyADC) >= 20)
        lastDutyADC = rawDutyADC;

    // ===============================
    //  DUMB MODE — RAW ANALOG CONTROL
    // ===============================
    if (currentMode == MODE_DUMB && !dumbFadeActive) {

        // ----- BRIGHTNESS -----
        float dutyNorm = (lastDutyADC - DUTY_MIN_RAW) /
                        float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        float linearBrightness = min_duty + dutyNorm * (1.0f - min_duty);

        currentBrightness = linearBrightness;
        targetBrightness  = linearBrightness;

        Serial.print("DUMB POT: rawDutyADC=");
        Serial.print(lastDutyADC);
        Serial.print(" dutyNorm=");
        Serial.print(dutyNorm, 4);
        Serial.print(" linB=");
        Serial.println(linearBrightness, 6);

        // ----- CCT -----
        int rawCCTADC = readADC(CCT_POT_PIN);

        if (lastCCTADC < 0) lastCCTADC = rawCCTADC;
        if (abs(rawCCTADC - lastCCTADC) >= 20)
            lastCCTADC = rawCCTADC;

        float cctNorm = (lastCCTADC - CCT_MIN_RAW) /
                        float(CCT_MAX_RAW - CCT_MIN_RAW);
        cctNorm = constrain(cctNorm, 0.0f, 1.0f);

        if (cctNorm < 0.02f) cctNorm = 0.0f;
        if (cctNorm > 0.98f) cctNorm = 1.0f;

        float cct = 2700.0f + cctNorm * (6500.0f - 2700.0f);

        if (cct > 4551.0f && cct < 4649.0f)
            cct = 4600.0f;

        currentCCT = cct;
        targetCCT  = cct;

        applyLEDsImmediate(currentBrightness, currentCCT);
        return;
    }

    // ===============================
    //  STANDBY POT ROUTING
    // ===============================
    if (currentMode == MODE_STANDBY) {

        if (previousMode == MODE_DEMO) {

            int rawCCTADC = readADC(CCT_POT_PIN);

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

            return;
        }

        return;
    }

    // ===============================
    //  DEMO MODE — BRIGHTNESS
    // ===============================
    if (currentMode == MODE_DEMO) {

        const float demoSteps[7] = {
            min_duty,
            0.025f,
            0.05f,
            0.10f,
            0.15f,
            0.20f,
            0.25f
        };

        int idx = 0;

        if (rawDutyADC <= DUTY_MIN_RAW + 40) {
            idx = 0;
        }
        else if (rawDutyADC >= DUTY_MAX_RAW - 80) {
            idx = 6;
        }
        else {
            idx = map(rawDutyADC, DUTY_MIN_RAW, DUTY_MAX_RAW, 0, 6);
        }

        idx = constrain(idx, 0, 6);

        if (idx != prevDemoIdx) {
            prevDemoIdx      = idx;
            targetBrightness = demoSteps[idx];

            currentBrightness = targetBrightness;
            applyLEDsImmediate(currentBrightness, currentCCT);

            lastInputChangeTime = now;

            if (!demoJustResumed &&
                systemInitialized &&
                buzzer_click_enabled &&
                millis() > buzzerQuietUntil)
            {
                buzzerClick();
            }

            demoJustResumed = false;
        }
    }

    // ===============================
    //  DEMO MODE — SPEED (CCT knob)
    // ===============================
    if (currentMode == MODE_DEMO) {

        int rawCCTADC = readADC(CCT_POT_PIN);

        if (lastDemoSpeedADC < 0) lastDemoSpeedADC = rawCCTADC;
        if (abs(rawCCTADC - lastDemoSpeedADC) < 40)
            return;

        lastDemoSpeedADC = rawCCTADC;

        static int lastSpeedIndex = -1;

        int speedIndex = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
        speedIndex = constrain(speedIndex, 0, 4);

        if (speedIndex != lastSpeedIndex) {
            lastSpeedIndex = speedIndex;

            const int demoSpeeds[5] = {500, 1000, 2500, 3500, 5000};
            const int speedPct[5]   = {100, 75, 50, 25, 1};

            demo_mode_fade_ms   = demoSpeeds[speedIndex];
            demoSpeedPercent    = speedPct[speedIndex];
            demoSpeedFlashUntil = now + 600;

            if (systemInitialized &&
                buzzer_click_enabled &&
                millis() > buzzerQuietUntil)
            {
                buzzerClick();
            }
        }

        return;
    }

    // =====================
    // FREQ MODE POT ROUTING
    // =====================
    if (currentMode == MODE_FREQ) {

        // --- Brightness (DUTY pot) ---
        float dutyNorm = (lastDutyADC - DUTY_MIN_RAW) /
                        float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        // Use NORMAL brightness mapping
        targetBrightness = brightnessTableLookup(dutyNorm);

        // --- Frequency (CCT pot) ---
        float cctNorm = (lastCCTADC - CCT_MIN_RAW) /
                        float(CCT_MAX_RAW - CCT_MIN_RAW);
        cctNorm = constrain(cctNorm, 0.0f, 1.0f);

        int idx = round(cctNorm * (FREQ_STEPS - 1));
        idx = constrain(idx, 0, FREQ_STEPS - 1);

        freqStrobeHz = freqTable[idx];

        return; // done for FREQ mode
    }

    // ===============================
    //  OVERRIDE / OVERRIDE+ BRIGHTNESS
    // ===============================
    if (currentMode == MODE_FREQ || currentMode == MODE_CAL) {

        float dutyNorm = (lastDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idealIdx = (int)round(dutyNorm * 19.0f);
        idealIdx = constrain(idealIdx, 0, 19);

        dutyStepIndex = idealIdx;

        if (dutyStepIndex != prevDutyStepIndex) {
            prevDutyStepIndex = dutyStepIndex;

            const float overrideBrightnessSteps[20] = {
                0.0012f, 0.0014f, 0.0016f, 0.0018f, 0.0020f,
                0.0022f, 0.0024f, 0.0026f, 0.0028f, 0.0030f,
                0.0032f, 0.0034f, 0.0036f, 0.0038f, 0.0040f,
                0.0045f, 0.0050f, 0.0060f, 0.0080f, 0.0200f
            };
            targetBrightness = overrideBrightnessSteps[dutyStepIndex];

            lastInputChangeTime = now;
            if (systemInitialized &&
                buzzer_click_enabled &&
                millis() > buzzerQuietUntil)
            {
                buzzerClick();
            }
        }
    }

    // ===============================
    //  NORMAL MODE — BRIGHTNESS
    // ===============================
    if (currentMode == MODE_NORMAL) {

        float dutyNorm = (lastDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idealIdx = (int)round(dutyNorm * (NORMAL_STEPS - 1));
        idealIdx = constrain(idealIdx, 0, NORMAL_STEPS - 1);

        dutyStepIndex = idealIdx;

        if (dutyStepIndex != prevDutyStepIndex) {
            prevDutyStepIndex = dutyStepIndex;

            targetBrightness = normalBrightnessSteps[dutyStepIndex];

            lastInputChangeTime = now;
            if (systemInitialized &&
                buzzer_click_enabled &&
                millis() > buzzerQuietUntil)
            {
                buzzerClick();
            }
        }
    }

    // ===============================
    //  CCT POT — NORMAL MODE ONLY
    // ===============================
    if (currentMode == MODE_NORMAL) {

        static int lastCCTADC = -1;

        int rawCCTADC = readADC(CCT_POT_PIN);

        if (lastCCTADC < 0) lastCCTADC = rawCCTADC;
        if (abs(rawCCTADC - lastCCTADC) < 20) {
            return;
        }
        lastCCTADC = rawCCTADC;

        float norm = (rawCCTADC - CCT_MIN_RAW) /
                     float(CCT_MAX_RAW - CCT_MIN_RAW);
        norm = constrain(norm, 0.0f, 1.0f);

        float rawCCT = 2700.0f + norm * (6500.0f - 2700.0f);
        int stepIndex = (int)round((rawCCT - 2700.0f) / 100.0f);
        stepIndex = constrain(stepIndex, 0, 38);

        if (stepIndex != prevCCTStepIndex) {
            prevCCTStepIndex    = stepIndex;
            targetCCT           = 2700.0f + stepIndex * 100.0f;
            lastInputChangeTime = now;

            if (systemInitialized &&
                buzzer_click_enabled &&
                millis() > buzzerQuietUntil)
            {
                buzzerClick();
            }
        }
    }

    // ===============================
    //  OVERRIDE+ MODE — CCT PRESETS
    // ===============================
    if (currentMode == MODE_CAL) {

        int rawCCTADC = readADC(CCT_POT_PIN);

        static int lastOverrideADC = -1;
        if (lastOverrideADC < 0) lastOverrideADC = rawCCTADC;
        if (abs(rawCCTADC - lastOverrideADC) < 20) {
            return;
        }
        lastOverrideADC = rawCCTADC;

        int idx = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
        idx = constrain(idx, 0, 4);

        if (rawCCTADC > CCT_MAX_RAW - 30) {
            idx = 4;
        }

        if (idx != calPresetIndex) {
            calPresetIndex = idx;
            targetCCT           = calPresets[idx];
            lastInputChangeTime = now;

            if (systemInitialized &&
                buzzer_click_enabled &&
                millis() > buzzerQuietUntil)
            {
                buzzerClick();
            }
        }
    }
}