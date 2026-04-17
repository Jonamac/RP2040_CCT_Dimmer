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
#include "buzzer.h"
#include <Arduino.h>

float lastDutyNorm = 0.0f;
float lastCCTNorm  = 0.0f;

static const float DUTY_STEP_HYST      = 0.05f;   // 5% of a step
static const float CCT_STEP_HYST       = 0.05f;
static const float DUMB_BRIGHTNESS_DB  = 0.003f;  // 0.3% brightness dead-band for ADC noise
static const float DUMB_CCT_DB         = 5.0f;    // 5 K CCT dead-band for ADC noise
// Snap multiplier for DUMB minimum: newB < min_duty * this → snap to min_duty.
// 3.0× covers ADC floors up to ~dutyNorm 0.095 (ADC ~400 counts above DUTY_MIN_RAW).
static const float DUMB_MIN_SNAP_MULT  = 3.0f;

// IIR filter state — file-scope so syncPotsAfterBoot() can seed them
static float dutyFiltered = -1.0f;
static float cctFiltered  = -1.0f;

// DUMB-specific IIR filter — stronger smoothing for continuous (non-stepped) output
static float dumbDutyFiltered = -1.0f;
static float dumbCCTFiltered  = -1.0f;

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

    if (dutyFiltered < 0.0f) {
        // No seed — first real call, seed filter from raw ADC (no averaging artifact)
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

        // Apply a second, stronger IIR pass for DUMB mode to suppress ADC noise.
        // α = 0.05 means 5% new data per frame → much smoother than shared filter.
        if (dumbDutyFiltered < 0.0f) {
            dumbDutyFiltered = dutyNorm;
            dumbCCTFiltered  = cctNorm;
        } else {
            dumbDutyFiltered = dumbDutyFiltered * 0.95f + dutyNorm * 0.05f;
            dumbCCTFiltered  = dumbCCTFiltered  * 0.95f + cctNorm  * 0.05f;
        }

        float newB = min_duty + dumbDutyFiltered * (1.0f - min_duty);
        float newC = 2700.0f + dumbCCTFiltered  * (6500.0f - 2700.0f);

        // Snap to exact endpoints to compensate for ADC mechanical limits.
        // The pot floor may not reach DUTY_MIN_RAW, giving newB slightly above
        // min_duty. Snap any value within 3.0x min_duty to exactly min_duty.
        // Snap anything above 0.99 to exactly 1.0.
        if (newB < min_duty * DUMB_MIN_SNAP_MULT) newB = min_duty;
        if (newB > 0.99f)           newB = 1.0f;

        static float prevDumbB = -1.0f;
        static float prevDumbC = -1.0f;

        // First call (sentinel < 0) or change exceeds dead-band
        bool brightnessChanged = (prevDumbB < 0.0f) || (fabsf(newB - prevDumbB) > DUMB_BRIGHTNESS_DB);
        bool cctChanged        = (prevDumbC < 0.0f) || (fabsf(newC - prevDumbC) > DUMB_CCT_DB);

        if (brightnessChanged || cctChanged) {
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
            buzzerClick();
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

void syncPotsAfterBoot(float brightness, float cct)
{
    // Find nearest brightness step to fade target
    int bestBStep = 0;
    float bestBDiff = fabsf(normalBrightnessSteps[0] - brightness);
    for (int i = 1; i < NORMAL_STEPS; i++) {
        float diff = fabsf(normalBrightnessSteps[i] - brightness);
        if (diff < bestBDiff) {
            bestBDiff = diff;
            bestBStep = i;
        }
    }
    prevDutyStep      = bestBStep;
    currentBrightness = normalBrightnessSteps[bestBStep];

    // Find nearest CCT step to fade target CCT
    int cctStep = constrain((int)round((cct - 2700.0f) / 100.0f), 0, 38);
    prevCCTStep = cctStep;
    currentCCT  = 2700.0f + cctStep * 100.0f;

    // Seed IIR filters from committed step values — NOT from ADC.
    // Seeding from ADC can land on a different step than setup() computed,
    // causing the very first handlePots() call to snap CCT/brightness.
    dutyFiltered = (float)prevDutyStep / (float)(NORMAL_STEPS - 1);
    cctFiltered  = (currentCCT - 2700.0f) / (6500.0f - 2700.0f);

    // Also update ledmix targets to match, preventing fallthrough snap
    ledmix_set(currentBrightness, currentCCT);
}