// ===============================
// pots.cpp — Revision 3
// Deterministic, mode-exclusive pot engine
// DUMB: continuous, immediate
// NORMAL: stepped, via ledmix_set
// ===============================

#include "pins.h"
#include "modes.h"
#include "pots.h"
#include "pots_state.h"
#include "state.h"
#include "ledmix.h"
#include "inputs.h"
#include "freq_mode.h"
#include "display_ui.h"
#include "calibration.h"
#include <Arduino.h>

float lastDutyNorm = 0.0f;
float lastCCTNorm  = 0.0f;

static const float DUTY_STEP_HYST      = 0.05f;   // 5% of a step
static const float CCT_STEP_HYST       = 0.05f;
static const float DUMB_BRIGHTNESS_DB  = 0.002f;  // 0.2% brightness dead-band for ADC noise
static const float DUMB_CCT_DB         = 5.0f;    // 5 K CCT dead-band for ADC noise

// Local state
static int prevDutyStep = -1;
static int prevCCTStep  = -1;

static float currentBrightness = 0.0f;
static float currentCCT = 4600.0f;

// Main pot handler
void handlePots(unsigned long now)
{
    // Freeze pot processing until boot fade completes
    if (!systemInitialized) return;

    // --- Shared: ADC read + normalize + IIR filter ---
    int rawDutyADC = analogRead(DUTY_POT_PIN);
    int rawCCTADC  = analogRead(CCT_POT_PIN);

    float dutyNorm = (rawDutyADC - DUTY_MIN_RAW) / float(DUTY_MAX_RAW - DUTY_MIN_RAW);
    float cctNorm  = (rawCCTADC  - CCT_MIN_RAW)  / float(CCT_MAX_RAW  - CCT_MIN_RAW);
    dutyNorm = constrain(dutyNorm, 0.0f, 1.0f);
    cctNorm  = constrain(cctNorm,  0.0f, 1.0f);

    static float dutyFiltered = -1.0f;  // -1 = uninitialized sentinel
    static float cctFiltered  = -1.0f;

    if (dutyFiltered < 0.0f) {
        // First real call after boot — seed filter to actual value
        dutyFiltered = dutyNorm;
        cctFiltered  = cctNorm;
    } else {
        dutyFiltered = dutyFiltered * 0.85f + dutyNorm * 0.15f;
        cctFiltered  = cctFiltered  * 0.85f + cctNorm  * 0.15f;
    }
    dutyNorm = dutyFiltered;
    cctNorm  = cctFiltered;

    // -------------------------
    // DUMB MODE — continuous, immediate
    // -------------------------
    if (currentMode == MODE_DUMB)
    {
        // Do not fight an active DUMB fade
        if (dumbFadeActive) return;

        // Continuous brightness: bottom of range = min_duty
        float newB = min_duty + dutyNorm * (1.0f - min_duty);
        float newC = 2700.0f + cctNorm * (6500.0f - 2700.0f);

        static float prevDumbB = -1.0f;
        static float prevDumbC = -1.0f;
        bool isFirstCall = (prevDumbB < 0.0f);

        // Only update if change exceeds dead-band (prevents ADC jitter from
        // causing constant LED flicker and display digit bounce)
        bool brightnessChanged = fabsf(newB - prevDumbB) > DUMB_BRIGHTNESS_DB;
        bool cctChanged        = fabsf(newC - prevDumbC) > DUMB_CCT_DB;

        if (isFirstCall || brightnessChanged || cctChanged) {
            currentBrightness = newB;
            currentCCT        = newC;
            prevDumbB         = newB;
            prevDumbC         = newC;

            ledmix_set(currentBrightness, currentCCT);
            applyLEDsImmediate(currentBrightness, currentCCT);
        }
        return;
    }

    // -------------------------
    // NORMAL MODE — stepped
    // -------------------------
    float mappedCCT = 2700.0f + cctNorm * (6500.0f - 2700.0f);

    // Duty: 22 steps (0..NORMAL_STEPS-1) with hysteresis
    // Use normalBrightnessSteps for consistency with STANDBY exit calculation
    float dutyStepFloat     = dutyNorm * (NORMAL_STEPS - 1);
    int   dutyStepCandidate = roundf(dutyStepFloat);
    dutyStepCandidate = constrain(dutyStepCandidate, 0, NORMAL_STEPS - 1);

    if (fabsf(dutyStepFloat - (float)prevDutyStep) > DUTY_STEP_HYST) {
        if (dutyStepCandidate != prevDutyStep) {
            prevDutyStep      = dutyStepCandidate;
            currentBrightness = normalBrightnessSteps[prevDutyStep];
        }
    }

    // CCT: 39 steps (2700..6500 K, 100 K each) with hysteresis
    float cctStepFloat     = (mappedCCT - 2700.0f) / 100.0f;
    int   cctStepCandidate = roundf(cctStepFloat);

    if (fabsf(cctStepFloat - (float)prevCCTStep) > CCT_STEP_HYST) {
        if (cctStepCandidate != prevCCTStep) {
            prevCCTStep = cctStepCandidate;
            currentCCT  = 2700.0f + prevCCTStep * 100.0f;
        }
    }

    // NORMAL uses the fade/LED engine — do not call applyLEDsImmediate here
    ledmix_set(currentBrightness, currentCCT);
}