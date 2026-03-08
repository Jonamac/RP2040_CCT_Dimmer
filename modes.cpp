#include "modes.h"
#include "state.h"
#include "pots.h"
#include "pots_state.h"
#include "calibration.h"
#include "inputs.h"
#include "display_ui.h"
#include "ledmix.h"
#include "pins.h"

// =====================================================
//  INITIALIZATION
// =====================================================
void initModes() {
    currentMode      = MODE_NORMAL;
    previousMode     = MODE_NORMAL;
    targetBrightness = 0.0f;
    targetCCT        = 4600.0f;
}

// -----------------------------
//  DUMB MODE SWITCH HANDLING
// -----------------------------
static bool lastDumbSwitch = false;

void handleDumbSwitch(unsigned long now) {
    bool dumbSwitch = digitalRead(DUMB_SWITCH_PIN);

    // --- SWITCH TURNED ON (entering DUMB MODE) ---
    if (dumbSwitch && !lastDumbSwitch) {

        // Force immediate pot resync
        lastDutyADC       = -1;
        prevDutyStepIndex = -1;
        prevCCTStepIndex  = -1;

        // Do NOT touch currentBrightness/targetBrightness/lastLEDUpdateTime here

        if (currentMode == MODE_STANDBY) {
            // We’re in STANDBY but the user just enabled DUMB:
            // remember that we *want* DUMB when we leave STANDBY
            previousMode = MODE_DUMB;
            // stay in STANDBY for now; fade engine will handle LEDs
        } else {
            currentMode = MODE_DUMB;
        }
    }

    // --- SWITCH TURNED OFF (leaving DUMB MODE) ---
    else if (!dumbSwitch && lastDumbSwitch) {

        if (currentMode == MODE_STANDBY) {
            previousMode = MODE_NORMAL;
            currentMode  = MODE_STANDBY;
        }
        else if (currentMode == MODE_DUMB) {
            currentMode = MODE_NORMAL;

            // Sample pots for NORMAL mode brightness
            int rawDutyADC = readADC(DUTY_POT_PIN);
            float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                             float(DUTY_MAX_RAW - DUTY_MIN_RAW);
            dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

            int idx = (int)round(dutyNorm * (NORMAL_STEPS - 1));
            idx = constrain(idx, 0, NORMAL_STEPS - 1);
            targetBrightness = normalBrightnessSteps[idx];

            // Sample pots for NORMAL mode CCT
            int rawCCTADC = readADC(CCT_POT_PIN);
            float norm = (rawCCTADC - CCT_MIN_RAW) /
                         float(CCT_MAX_RAW - CCT_MIN_RAW);
            norm = constrain(norm, 0.0f, 1.0f);

            float rawCCT = 2700.0f + norm * (6500.0f - 2700.0f);
            int stepIndex = (int)round((rawCCT - 2700.0f) / 100.0f);
            stepIndex = constrain(stepIndex, 0, 38);
            targetCCT = 2700.0f + stepIndex * 100.0f;

            currentBrightness = targetBrightness;
            currentCCT        = targetCCT;
            applyLEDsImmediate(currentBrightness, currentCCT);
        }
    }

    lastDumbSwitch = dumbSwitch;
}

// =====================================================
//  MAIN BUTTON — SHORT PRESS
// =====================================================
void handleMainButtonRelease(unsigned long heldMs, unsigned long now) {

    // DUMB MODE: only allow STANDBY toggle
    if (currentMode == MODE_DUMB) {
        if (heldMs < 1500) {
            previousMode = currentMode;
            currentMode  = MODE_STANDBY;

            // Start fade DOWN using the SAME 'now' as the loop
            dumbFadeActive     = true;
            dumbFadeDirection  = false; // down
            dumbFadeStartB     = currentBrightness;
            dumbFadeEndB       = 0.0f;
            dumbFadeStartTime  = now;                 // <-- CHANGED
            dumbFadeDuration   = dumb_standby_fade_time_ms; // or standby_fade_time_ms

            targetBrightness = 0.0f;

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
        }
        return;
    }

    // Ignore long presses here
    if (heldMs > 1500) return;

    // NORMAL → STANDBY
    if (currentMode == MODE_NORMAL) {
        previousMode     = currentMode;
        currentMode      = MODE_STANDBY;
        targetBrightness = 0.0f;

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // =====================================================
    //  STANDBY EXIT LOGIC
    // =====================================================
    if (currentMode == MODE_STANDBY) {
        // Leaving STANDBY: stop any DUMB→STANDBY fade
        // Return to DUMB MODE
        if (previousMode == MODE_DUMB) {
            previousMode = currentMode;
            currentMode  = MODE_DUMB;

            int rawDutyADC = readADC(DUTY_POT_PIN);
            float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                             float(DUTY_MAX_RAW - DUTY_MIN_RAW);
            dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);
            float linearBrightness = min_duty + dutyNorm * (1.0f - min_duty);

            dumbFadeActive     = true;
            dumbFadeDirection  = true; // up
            dumbFadeStartB     = 0.0f;
            dumbFadeEndB       = linearBrightness;
            dumbFadeStartTime  = now;                 // <-- CHANGED
            dumbFadeDuration   = dumb_soft_start_ms;

            targetBrightness = linearBrightness;

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
            return;
        }


        // Return to NORMAL MODE
    if (previousMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_NORMAL;

        // Sample pots for NORMAL mode
        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                        float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idx = (int)round(dutyNorm * (NORMAL_STEPS - 1));
        idx = constrain(idx, 0, NORMAL_STEPS - 1);

        targetBrightness  = normalBrightnessSteps[idx];
        currentBrightness = targetBrightness;   // <-- REQUIRED

        int rawCCTADC = analogRead(CCT_POT_PIN);
        float norm = (rawCCTADC - CCT_MIN_RAW) /
                    float(CCT_MAX_RAW - CCT_MIN_RAW);
        norm = constrain(norm, 0.0f, 1.0f);

        float rawCCT = 2700.0f + norm * (6500.0f - 2700.0f);
        int stepIndex = (int)round((rawCCT - 2700.0f) / 100.0f);
        stepIndex = constrain(stepIndex, 0, 38);

        targetCCT  = 2700.0f + stepIndex * 100.0f;
        currentCCT = targetCCT;                 // <-- REQUIRED

        applyLEDsImmediate(currentBrightness, currentCCT);

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

        // Return to OVERRIDE+
        if (previousMode == MODE_OVERRIDE_PLUS) {
            currentMode = MODE_OVERRIDE_PLUS;

            int rawDutyADC = analogRead(DUTY_POT_PIN);
            float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                             float(DUTY_MAX_RAW - DUTY_MIN_RAW);
            dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

            int idx = (int)round(dutyNorm * 19.0f);
            idx = constrain(idx, 0, 19);

            const float overrideBrightnessSteps[20] = {
                0.002f, 0.003f, 0.004f, 0.005f, 0.006f,
                0.007f, 0.008f, 0.009f, 0.010f, 0.011f,
                0.012f, 0.013f, 0.014f, 0.015f, 0.016f,
                0.017f, 0.018f, 0.019f, 0.020f, 0.020f
            };
            targetBrightness = overrideBrightnessSteps[idx];

            int rawCCTADC = analogRead(CCT_POT_PIN);
            int cidx = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
            cidx = constrain(cidx, 0, 4);
            overridePresetIndex = cidx;
            targetCCT           = overridePresets[cidx];

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
            return;
        }

        // Return to DEMO
        if (previousMode == MODE_DEMO) {
            currentMode = MODE_DEMO;
            demoJustResumed = true;
            resetDemoBrightnessTracking();
            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
            return;
        }

        // Default: STANDBY → NORMAL
        currentMode = MODE_NORMAL;

        // Do not reset pot tracking — let fade engine handle the transition

        // Sample pots for NORMAL mode
        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idx = (int)round(dutyNorm * (NORMAL_STEPS - 1));
        idx = constrain(idx, 0, NORMAL_STEPS - 1);
        targetBrightness = normalBrightnessSteps[idx];

        int rawCCTADC = analogRead(CCT_POT_PIN);
        float norm = (rawCCTADC - CCT_MIN_RAW) /
                     float(CCT_MAX_RAW - CCT_MIN_RAW);
        norm = constrain(norm, 0.0f, 1.0f);

        float rawCCT = 2700.0f + norm * (6500.0f - 2700.0f);
        int stepIndex = (int)round((rawCCT - 2700.0f) / 100.0f);
        stepIndex = constrain(stepIndex, 0, 38);
        targetCCT = 2700.0f + stepIndex * 100.0f;

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // OVERRIDE → cycle CCT presets
    if (currentMode == MODE_OVERRIDE) {
        float c = targetCCT;

        if (c >= 4550.0f && c <= 4650.0f) {
            targetCCT = 2700.0f;
        } else if (c >= 2650.0f && c <= 2750.0f) {
            targetCCT = 3800.0f;
        } else if (c >= 3750.0f && c <= 3850.0f) {
            targetCCT = 5000.0f;
        } else if (c >= 4950.0f && c <= 5050.0f) {
            targetCCT = 6500.0f;
        } else {
            targetCCT = 4600.0f;
        }

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // OVERRIDE+ → STANDBY
    if (currentMode == MODE_OVERRIDE_PLUS) {
        previousMode     = currentMode;
        currentMode      = MODE_STANDBY;
        targetBrightness = 0.0f;

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // DEMO → STANDBY
    if (currentMode == MODE_DEMO) {
        previousMode     = currentMode;
        currentMode      = MODE_STANDBY;
        targetBrightness = 0.0f;

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }
}

// =====================================================
//  DISPLAY BUTTON — SHORT PRESS / LONG PRESS
// =====================================================
void handleDispButtonRelease(unsigned long heldMs) {

    // DUMB MODE: ignore display button except toggle display
    if (currentMode == MODE_DUMB) {
        displayOn = !displayOn;
        if (displayOn) lastDisplayOnTime = millis();
        return;
    }

    if (currentMode == MODE_DUMB || previousMode == MODE_DUMB) {
        toggleDisplay();
        return;
    }

    // Long press → DEMO toggle
    if (heldMs >= demo_mode_delay_ms) {
        if (currentMode == MODE_DEMO) {
            currentMode = MODE_NORMAL;
        } else {
            previousMode        = currentMode;
            currentMode         = MODE_DEMO;
            demoPhaseIndex      = 0;
            demoPhaseStartTime  = millis();
            demoJustResumed     = true;
            resetDemoBrightnessTracking();
        }
        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // Short press → toggle display
    if (heldMs < demo_mode_delay_ms) {

        if (currentMode == MODE_OVERRIDE ||
            currentMode == MODE_OVERRIDE_PLUS) {

            if (!displayOn) {
                displayOn         = true;
                lastDisplayOnTime = millis();
                if (systemInitialized) {
                    buzzerModeChangeBeep();
                    buzzerQuietUntil = millis() + 200;
                }
            }

        } else {
            displayOn = !displayOn;
            if (displayOn) lastDisplayOnTime = millis();
            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
        }

        return;
    }
}

// =====================================================
//  MAIN BUTTON — LONG PRESS
// =====================================================
void handleMainLongPress() {

    // DUMB MODE: ignore long press
    if (currentMode == MODE_DUMB) return;

    if (currentMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_OVERRIDE;

        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idx = (int)round(dutyNorm * 19.0f);
        idx = constrain(idx, 0, 19);

        const float overrideBrightnessSteps[20] = {
            0.002f, 0.003f, 0.004f, 0.005f, 0.006f,
            0.007f, 0.008f, 0.009f, 0.010f, 0.011f,
            0.012f, 0.013f, 0.014f, 0.015f, 0.016f,
            0.017f, 0.018f, 0.019f, 0.020f, 0.020f
        };
        targetBrightness = overrideBrightnessSteps[idx];

        overridePresetIndex = 2;
        targetCCT           = overridePresets[overridePresetIndex];

    } else if (currentMode == MODE_OVERRIDE ||
               currentMode == MODE_OVERRIDE_PLUS ||
               currentMode == MODE_DEMO ||
               currentMode == MODE_STANDBY) {
        previousMode = currentMode;
        currentMode  = MODE_NORMAL;
    }

    if (systemInitialized) {
        buzzerModeChangeBeep();
        buzzerQuietUntil = millis() + 200;
    }
}

// =====================================================
//  MAIN BUTTON — SHORT-LONG COMBO
// =====================================================
void handleMainShortLongCombo() {

    // DUMB MODE: ignore
    if (currentMode == MODE_DUMB) return;

    // STANDBY: combo toggles between NORMAL and OVERRIDE+
    if (currentMode == MODE_STANDBY) {

        if (previousMode == MODE_OVERRIDE_PLUS) {
            // Came from OVERRIDE+ → go to NORMAL
            currentMode = MODE_NORMAL;
        } else {
            // Default: go to OVERRIDE+
            previousMode = MODE_NORMAL;
            currentMode  = MODE_OVERRIDE_PLUS;
        }

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    if (currentMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_OVERRIDE_PLUS;

        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int bIdx = (int)round(dutyNorm * 19.0f);
        bIdx = constrain(bIdx, 0, 19);

        const float overrideBrightnessSteps[20] = {
            0.002f, 0.003f, 0.004f, 0.005f, 0.006f,
            0.007f, 0.008f, 0.009f, 0.010f, 0.011f,
            0.012f, 0.013f, 0.014f, 0.015f, 0.016f,
            0.017f, 0.018f, 0.019f, 0.020f, 0.020f
        };
        targetBrightness = overrideBrightnessSteps[bIdx];

        int rawCCTADC = analogRead(CCT_POT_PIN);
        int cIdx = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
        cIdx = constrain(cIdx, 0, 4);
        overridePresetIndex = cIdx;
        targetCCT           = overridePresets[cIdx];

    } else if (currentMode == MODE_OVERRIDE_PLUS) {
        previousMode = currentMode;
        currentMode  = MODE_NORMAL;
    }

    if (systemInitialized) {
        buzzerModeChangeBeep();
        buzzerQuietUntil = millis() + 200;
    }
}

// =====================================================
//  DISPLAY BUTTON — LONG PRESS (BUZZER MODE CYCLE)
// =====================================================
void handleDispLongPress() {

    // DUMB MODE: ignore
    if (currentMode == MODE_DUMB) return;

    static int buzzerState = 0;

    buzzerState = (buzzerState + 1) % 3;

    if (buzzerState == 0) {
        buzzer_beep_enabled  = true;
        buzzer_click_enabled = true;
        showStatusFlash("BZR ON", 3, 100);
    } else if (buzzerState == 1) {
        buzzer_beep_enabled  = false;
        buzzer_click_enabled = true;
        showStatusFlash("BEEP OFF", 3, 100);
    } else {
        buzzer_beep_enabled  = false;
        buzzer_click_enabled = false;
        showStatusFlash("BZR OFF", 3, 100);
    }
}

// =====================================================
//  DISPLAY BUTTON — SHORT-LONG COMBO (DEMO TOGGLE)
// =====================================================
void handleDispShortLongCombo() {

    // DUMB MODE: ignore
    if (currentMode == MODE_DUMB) return;

    if (currentMode == MODE_DEMO) {
        currentMode = previousMode;
    } else {
        previousMode        = currentMode;
        currentMode         = MODE_DEMO;
        demoPhaseIndex      = 0;
        demoPhaseStartTime  = millis();
        demoJustResumed     = true;
        resetDemoBrightnessTracking();

        int rawDutyADC = analogRead(DUTY_POT_PIN);
        int idx = map(rawDutyADC, DUTY_MIN_RAW, DUTY_MAX_RAW, 0, 6);
        idx = constrain(idx, 0, 6);

        const float demoSteps[7] = {
            min_duty,
            0.025f,
            0.05f,
            0.10f,
            0.15f,
            0.20f,
            0.25f
        };
        targetBrightness = demoSteps[idx];
    }

    if (systemInitialized) {
        buzzerModeChangeBeep();
        buzzerQuietUntil = millis() + 200;
    }
}

// =====================================================
//  MODE STATE UPDATE (NO-OP)
// =====================================================
void updateModeState(unsigned long now) {
    // Do NOT block state transitions for DUMB mode
    // This was preventing previousMode from updating correctly
}

// =====================================================
//  MODE BEHAVIOR UPDATE
// =====================================================
static float demoTargets[4] = {2700.0f, 4600.0f, 6500.0f, 4600.0f};

void updateModeBehavior(unsigned long now) {

    if (currentMode == MODE_DUMB) return;

    if (currentMode == MODE_DEMO) {
        unsigned long phaseElapsed = now - demoPhaseStartTime;
        if (phaseElapsed > demo_mode_fade_ms + demo_mode_hold_time_ms) {
            demoPhaseIndex     = (demoPhaseIndex + 1) % 4;
            demoPhaseStartTime = now;
        }
        targetCCT = demoTargets[demoPhaseIndex];

        if (targetBrightness > 0.25f) targetBrightness = 0.25f;
        if (targetBrightness < min_duty) targetBrightness = min_duty;
    }

    if (currentMode == MODE_STANDBY) {
        targetBrightness = 0.0f;
    }
}