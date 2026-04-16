#include "modes.h"
#include "state.h"
#include "pots.h"
#include "pots_state.h"
#include "calibration.h"
#include "inputs.h"
#include "display_ui.h"
#include "ledmix.h"
#include "pins.h"
#include "freq_mode.h"

// =====================================================
//  INITIALIZATION
// =====================================================
void initModes() {
    currentMode  = MODE_NORMAL;
    previousMode = MODE_NORMAL;

    // Initialize LED engine to neutral state
    ledmix_set(0.0f, 4600.0f);
}

// -----------------------------
//  DUMB MODE SWITCH HANDLING
// -----------------------------

static bool          lastDumbSwitch        = false;
static bool          dumbSwitchRaw         = false;
static unsigned long dumbDebounceStartTime = 0;
static bool          dumbDebouncing        = false;

static const unsigned long DUMB_DEBOUNCE_MS        = 25;
static const unsigned long DUMB_STARTUP_LOCKOUT_MS = 300;

void handleDumbSwitch(unsigned long now) {

    // --- Startup lockout: ignore all switch activity for 300 ms after boot ---
    if (now < DUMB_STARTUP_LOCKOUT_MS) return;

    bool raw = digitalRead(DUMB_SWITCH_PIN);

    // --- Debounce: only act after the reading has been stable for DUMB_DEBOUNCE_MS ---
    if (raw != dumbSwitchRaw) {
        // Reading changed — start or restart the debounce timer
        dumbSwitchRaw         = raw;
        dumbDebounceStartTime = now;
        dumbDebouncing        = true;
        return;
    }

    if (dumbDebouncing) {
        if ((now - dumbDebounceStartTime) < DUMB_DEBOUNCE_MS) {
            return; // still within debounce window
        }
        // Debounce window elapsed — reading is stable; commit it
        dumbDebouncing = false;
    }

    // At this point, `dumbSwitchRaw` is the stable, debounced value.
    bool dumbSwitch = dumbSwitchRaw;

    // --- Ignore switch changes during active fades ---
    if (dumbFadeActive)   return;
    if (normalFadeActive) return;  // declared in state.h; fade engine wired up in PR 3

    // --- Ignore switch changes while in STANDBY (handled by previousMode on exit) ---
    if (currentMode == MODE_STANDBY) {
        lastDumbSwitch = dumbSwitch;
        return;
    }

    // --- SWITCH TURNED ON (entering DUMB MODE) ---
    if (dumbSwitch && !lastDumbSwitch) {
        lastDutyNorm = -1.0f; // force pot resync

        currentMode = MODE_DUMB;
    }

    // --- SWITCH TURNED OFF (leaving DUMB MODE) ---
    else if (!dumbSwitch && lastDumbSwitch) {
        if (currentMode == MODE_DUMB) {
            currentMode = MODE_NORMAL;

            // Sample pots and snap to NORMAL state
            int rawDutyADC = readADC(DUTY_POT_PIN);
            float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                             float(DUTY_MAX_RAW - DUTY_MIN_RAW);
            dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

            int idx = round(dutyNorm * (NORMAL_STEPS - 1));
            idx = constrain(idx, 0, NORMAL_STEPS - 1);
            float newB = normalBrightnessSteps[idx];

            int rawCCTADC = readADC(CCT_POT_PIN);
            float norm = (rawCCTADC - CCT_MIN_RAW) /
                         float(CCT_MAX_RAW - CCT_MIN_RAW);
            norm = constrain(norm, 0.0f, 1.0f);

            float rawCCT   = 2700.0f + norm * (6500.0f - 2700.0f);
            int stepIndex  = round((rawCCT - 2700.0f) / 100.0f);
            stepIndex      = constrain(stepIndex, 0, 38);
            float newC     = 2700.0f + stepIndex * 100.0f;

            ledmix_set(newB, newC);
            applyLEDsImmediate(newB, newC);
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

            // Start fade DOWN
            dumbFadeActive     = true;
            dumbFadeDirection  = false;
            dumbFadeStartB     = ledmix_getBrightness();
            dumbFadeEndB       = 0.0f;
            dumbFadeStartTime  = now;
            dumbFadeDuration   = dumb_standby_fade_time_ms;

            ledmix_set(0.0f, ledmix_getCCT());

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
        }
        return;
    }

    // Ignore long presses
    if (heldMs > 1500) return;

    // NORMAL → STANDBY
    if (currentMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_STANDBY;

        // Start NORMAL fade DOWN to 0
        normalFadeActive    = true;
        normalFadeStartB    = ledmix_getBrightness();
        normalFadeEndB      = 0.0f;
        normalFadeStartTime = now;
        normalFadeDuration  = standby_fade_time_ms;

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
            dumbFadeDirection  = true;
            dumbFadeStartB     = 0.0f;
            dumbFadeEndB       = linearBrightness;
            dumbFadeStartTime  = now;
            dumbFadeDuration   = dumb_soft_start_ms;

            ledmix_set(linearBrightness, ledmix_getCCT());

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
            return;
        }

        // Return to FREQ
        if (previousMode == MODE_FREQ) {
            currentMode = MODE_FREQ;
            freqCycleStartTime = 0;
            return;
        }

        // Return to NORMAL
        if (previousMode == MODE_NORMAL) {
            previousMode = currentMode;
            currentMode  = MODE_NORMAL;

            // Sample pots for NORMAL mode
            int rawDutyADC = analogRead(DUTY_POT_PIN);
            float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                             float(DUTY_MAX_RAW - DUTY_MIN_RAW);
            dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

            int idx = round(dutyNorm * (NORMAL_STEPS - 1));
            idx = constrain(idx, 0, NORMAL_STEPS - 1);
            float newB = normalBrightnessSteps[idx];

            int rawCCTADC = analogRead(CCT_POT_PIN);
            float norm = (rawCCTADC - CCT_MIN_RAW) /
                         float(CCT_MAX_RAW - CCT_MIN_RAW);
            norm = constrain(norm, 0.0f, 1.0f);

            float rawCCT = 2700.0f + norm * (6500.0f - 2700.0f);
            int stepIndex = round((rawCCT - 2700.0f) / 100.0f);
            stepIndex = constrain(stepIndex, 0, 38);
            float newC = 2700.0f + stepIndex * 100.0f;

            ledmix_set(newB, newC);

            // Start NORMAL fade UP from min_duty
            normalFadeActive    = true;
            normalFadeStartB    = min_duty;
            normalFadeEndB      = newB;
            normalFadeStartTime = now;
            normalFadeDuration  = standby_fade_time_ms;

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
            return;
        }

        // Return to OVERRIDE+
        if (previousMode == MODE_CAL) {
            currentMode = MODE_CAL;

            int rawDutyADC = analogRead(DUTY_POT_PIN);
            float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                             float(DUTY_MAX_RAW - DUTY_MIN_RAW);
            dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

            int idx = round(dutyNorm * 19.0f);
            idx = constrain(idx, 0, 19);

            const float overrideBrightnessSteps[20] = {
                0.002f, 0.003f, 0.004f, 0.005f, 0.006f,
                0.007f, 0.008f, 0.009f, 0.010f, 0.011f,
                0.012f, 0.013f, 0.014f, 0.015f, 0.016f,
                0.017f, 0.018f, 0.019f, 0.020f, 0.020f
            };
            float newB = overrideBrightnessSteps[idx];

            int rawCCTADC = analogRead(CCT_POT_PIN);
            int cidx = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
            cidx = constrain(cidx, 0, 4);
            calPresetIndex = cidx;
            float newC = calPresets[cidx];

            ledmix_set(newB, newC);

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

            if (systemInitialized) {
                buzzerModeChangeBeep();
                buzzerQuietUntil = millis() + 200;
            }
            return;
        }

        // Default: STANDBY → NORMAL
        currentMode = MODE_NORMAL;

        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idx = round(dutyNorm * (NORMAL_STEPS - 1));
        idx = constrain(idx, 0, NORMAL_STEPS - 1);
        float newB = normalBrightnessSteps[idx];

        int rawCCTADC = analogRead(CCT_POT_PIN);
        float norm = (rawCCTADC - CCT_MIN_RAW) /
                     float(CCT_MAX_RAW - CCT_MIN_RAW);
        norm = constrain(norm, 0.0f, 1.0f);

        float rawCCT = 2700.0f + norm * (6500.0f - 2700.0f);
        int stepIndex = round((rawCCT - 2700.0f) / 100.0f);
        stepIndex = constrain(stepIndex, 0, 38);
        float newC = 2700.0f + stepIndex * 100.0f;

        ledmix_set(newB, newC);

        normalFadeActive    = true;
        normalFadeStartB    = min_duty;
        normalFadeEndB      = newB;
        normalFadeStartTime = now;
        normalFadeDuration  = standby_fade_time_ms;

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // OVERRIDE → cycle CCT presets
    if (currentMode == MODE_FREQ) {
        float c = ledmix_getCCT();

        if (c >= 4550.0f && c <= 4650.0f) {
            ledmix_set(ledmix_getBrightness(), 2700.0f);
        } else if (c >= 2650.0f && c <= 2750.0f) {
            ledmix_set(ledmix_getBrightness(), 3800.0f);
        } else if (c >= 3750.0f && c <= 3850.0f) {
            ledmix_set(ledmix_getBrightness(), 5000.0f);
        } else if (c >= 4950.0f && c <= 5050.0f) {
            ledmix_set(ledmix_getBrightness(), 6500.0f);
        } else {
            ledmix_set(ledmix_getBrightness(), 4600.0f);
        }

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // OVERRIDE+ → STANDBY
    if (currentMode == MODE_CAL) {
        previousMode = currentMode;
        currentMode  = MODE_STANDBY;

        ledmix_set(0.0f, ledmix_getCCT());

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // DEMO → STANDBY
    if (currentMode == MODE_DEMO) {
        previousMode = currentMode;
        currentMode  = MODE_STANDBY;

        ledmix_set(0.0f, ledmix_getCCT());

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
        }
        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // Short press → toggle display
    if (heldMs < demo_mode_delay_ms) {

        if (currentMode == MODE_FREQ ||
            currentMode == MODE_CAL) {

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

    // NORMAL → FREQ
    if (currentMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_FREQ;
        freqCycleStartTime = 0;
        return;
    }

    // FREQ → NORMAL
    if (currentMode == MODE_FREQ) {
        previousMode = currentMode;
        currentMode  = MODE_NORMAL;
        return;
    }

    // NORMAL → OVERRIDE+
    if (currentMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_CAL;

        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int idx = round(dutyNorm * 19.0f);
        idx = constrain(idx, 0, 19);

        const float overrideBrightnessSteps[20] = {
            0.002f, 0.003f, 0.004f, 0.005f, 0.006f,
            0.007f, 0.008f, 0.009f, 0.010f, 0.011f,
            0.012f, 0.013f, 0.014f, 0.015f, 0.016f,
            0.017f, 0.018f, 0.019f, 0.020f, 0.020f
        };
        float newB = overrideBrightnessSteps[idx];

        calPresetIndex = 2;
        float newC = calPresets[calPresetIndex];

        ledmix_set(newB, newC);
    }

    // OVERRIDE+ → NORMAL
    else if (currentMode == MODE_FREQ ||
             currentMode == MODE_CAL ||
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

        if (previousMode == MODE_CAL) {
            // Came from OVERRIDE+ → go to NORMAL
            currentMode = MODE_NORMAL;
        } else {
            // Default: go to OVERRIDE+
            previousMode = MODE_NORMAL;
            currentMode  = MODE_CAL;
        }

        if (systemInitialized) {
            buzzerModeChangeBeep();
            buzzerQuietUntil = millis() + 200;
        }
        return;
    }

    // NORMAL → OVERRIDE+
    if (currentMode == MODE_NORMAL) {
        previousMode = currentMode;
        currentMode  = MODE_CAL;

        int rawDutyADC = analogRead(DUTY_POT_PIN);
        float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) /
                         float(DUTY_MAX_RAW - DUTY_MIN_RAW);
        dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);

        int bIdx = round(dutyNorm * 19.0f);
        bIdx = constrain(bIdx, 0, 19);

        const float overrideBrightnessSteps[20] = {
            0.002f, 0.003f, 0.004f, 0.005f, 0.006f,
            0.007f, 0.008f, 0.009f, 0.010f, 0.011f,
            0.012f, 0.013f, 0.014f, 0.015f, 0.016f,
            0.017f, 0.018f, 0.019f, 0.020f, 0.020f
        };
        float newB = overrideBrightnessSteps[bIdx];

        int rawCCTADC = analogRead(CCT_POT_PIN);
        int cIdx = map(rawCCTADC, CCT_MIN_RAW, CCT_MAX_RAW, 0, 4);
        cIdx = constrain(cIdx, 0, 4);
        calPresetIndex = cIdx;
        float newC = calPresets[cIdx];

        ledmix_set(newB, newC);
    }

    // OVERRIDE+ → NORMAL
    else if (currentMode == MODE_CAL) {
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
        float newB = demoSteps[idx];

        ledmix_set(newB, ledmix_getCCT());
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
    // No blocking logic here
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

        float newC = demoTargets[demoPhaseIndex];
        float newB = ledmix_getBrightness();

        if (newB > 0.25f) newB = 0.25f;
        if (newB < min_duty) newB = min_duty;

        ledmix_set(newB, newC);
    }

    if (currentMode == MODE_STANDBY && !normalFadeActive) {
        ledmix_set(0.0f, ledmix_getCCT());
    }
}