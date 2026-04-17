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

static const float DUTY_STEP_HYST_ST    = 0.15f;  // Schmitt dead-band per side (duty)
static const float CCT_STEP_HYST_ST     = 0.15f;  // Schmitt dead-band per side (CCT)
static const float DUMB_BRIGHTNESS_DB  = 0.005f;  // wider dead-band to suppress ADC noise
static const float DUMB_CCT_DB         = 10.0f;   // 10 K CCT dead-band to suppress ADC noise
static const float DUMB_DUTY_SNAP_LO   = 0.03f;   // endpoint snap: below this → clamp to 0
static const float DUMB_DUTY_SNAP_HI   = 0.97f;   // endpoint snap: above this → clamp to 1
static const float DUMB_CCT_CENTER_K   = 4600.0f; // center snap target (neutral CCT)
static const float DUMB_CCT_CENTER_TOL = 75.0f;   // ±K around center that snaps to center

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

    // Save raw (pre-IIR) normalized values for DUMB mode — prevents cascade double-filter
    float rawDutyNorm = dutyNorm;
    float rawCCTNorm  = cctNorm;

    if (dutyFiltered < 0.0f) {
        // No seed — first real call, seed filter from raw ADC (no averaging artifact)
        dutyFiltered = dutyNorm;
        cctFiltered  = cctNorm;
    } else {
        // Adaptive IIR filter:
        // - Fast response when pot is moving (large frame-to-frame delta)
        // - Stable filtering when pot is settled (small delta)
        // α ramps from 0.10 (settled) to 0.60 (moving fast)
        // Threshold for "moving": > 0.01 normalized (~41 ADC counts)
        float dutyDelta = fabsf(dutyNorm - dutyFiltered);
        float cctDelta  = fabsf(cctNorm  - cctFiltered);
        float dutyAlpha = (dutyDelta > 0.01f) ? 0.60f : 0.10f;
        float cctAlpha  = (cctDelta  > 0.01f) ? 0.60f : 0.10f;
        dutyFiltered = dutyFiltered * (1.0f - dutyAlpha) + dutyNorm * dutyAlpha;
        cctFiltered  = cctFiltered  * (1.0f - cctAlpha)  + cctNorm  * cctAlpha;
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

        // Single adaptive IIR for DUMB mode, applied to raw (pre-shared-IIR) ADC values.
        // α=0.40 when moving (delta > 0.005, ~20 ADC counts) → ~2.5-frame lag, analog-feel.
        // α=0.05 when settled → ~20-frame smoothing, suppresses ADC noise.
        if (dumbDutyFiltered < 0.0f) {
            dumbDutyFiltered = rawDutyNorm;
            dumbCCTFiltered  = rawCCTNorm;
        } else {
            float dDutyDelta = fabsf(rawDutyNorm - dumbDutyFiltered);
            float dCCTDelta  = fabsf(rawCCTNorm  - dumbCCTFiltered);
            float dDutyAlpha = (dDutyDelta > 0.005f) ? 0.40f : 0.05f;   // was 0.10 settled
            float dCCTAlpha  = (dCCTDelta  > 0.005f) ? 0.40f : 0.05f;   // was 0.10 settled
            dumbDutyFiltered = dumbDutyFiltered * (1.0f - dDutyAlpha) + rawDutyNorm * dDutyAlpha;
            dumbCCTFiltered  = dumbCCTFiltered  * (1.0f - dCCTAlpha)  + rawCCTNorm  * dCCTAlpha;
        }

        // Endpoint snap zones — ensure full range is reachable
        if (dumbDutyFiltered < DUMB_DUTY_SNAP_LO) dumbDutyFiltered = 0.0f;
        if (dumbDutyFiltered > DUMB_DUTY_SNAP_HI) dumbDutyFiltered = 1.0f;

        float newB = min_duty + dumbDutyFiltered * (1.0f - min_duty);
        float newC = 2700.0f + dumbCCTFiltered  * (6500.0f - 2700.0f);

        // Soft center snap: within ±DUMB_CCT_CENTER_TOL of neutral CCT snaps to center exactly.
        // Makes it easy to find and hold neutral CCT without being too restrictive.
        if (fabsf(newC - DUMB_CCT_CENTER_K) < DUMB_CCT_CENTER_TOL) newC = DUMB_CCT_CENTER_K;

        // Clamp to valid range
        newB = constrain(newB, min_duty, 1.0f);
        newC = constrain(newC, 2700.0f, 6500.0f);

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

    // Duty: NORMAL_STEPS steps with Schmitt-trigger hysteresis.
    // Up-step:   must cross N + 0.5 + DUTY_STEP_HYST_ST
    // Down-step: must cross N - 0.5 - DUTY_STEP_HYST_ST
    // This prevents oscillation at step boundaries.
    bool dutyFirstCall = (prevDutyStep < 0);

    float dutyStepFloat     = dutyNorm * (NORMAL_STEPS - 1);
    int   dutyStepCandidate = constrain((int)roundf(dutyStepFloat), 0, NORMAL_STEPS - 1);

    bool dutyStepChanged = false;
    if (prevDutyStep < 0) {
        // First call — commit immediately
        prevDutyStep      = dutyStepCandidate;
        currentBrightness = normalBrightnessSteps[prevDutyStep];
        dutyStepChanged   = true;
    } else if (dutyStepCandidate > prevDutyStep &&
               dutyStepFloat > (float)prevDutyStep + 0.5f + DUTY_STEP_HYST_ST) {
        prevDutyStep      = dutyStepCandidate;
        currentBrightness = normalBrightnessSteps[prevDutyStep];
        dutyStepChanged   = true;
    } else if (dutyStepCandidate < prevDutyStep &&
               dutyStepFloat < (float)prevDutyStep - 0.5f - DUTY_STEP_HYST_ST) {
        prevDutyStep      = dutyStepCandidate;
        currentBrightness = normalBrightnessSteps[prevDutyStep];
        dutyStepChanged   = true;
    }

    if (dutyStepChanged && !dutyFirstCall && systemInitialized) {
        buzzerClick();
    }

    // CCT: 39 steps (2700..6500 K, 100 K each) with Schmitt-trigger hysteresis.
    // Up-step:   must cross N + 0.5 + CCT_STEP_HYST_ST
    // Down-step: must cross N - 0.5 - CCT_STEP_HYST_ST
    bool cctFirstCall = (prevCCTStep < 0);

    float cctStepFloat     = (mappedCCT - 2700.0f) / 100.0f;
    int   cctStepCandidate = constrain((int)roundf(cctStepFloat), 0, 38);

    bool cctStepChanged = false;
    if (prevCCTStep < 0) {
        // First call — commit immediately
        prevCCTStep    = cctStepCandidate;
        currentCCT     = 2700.0f + prevCCTStep * 100.0f;
        cctStepChanged = true;
    } else if (cctStepCandidate > prevCCTStep &&
               cctStepFloat > (float)prevCCTStep + 0.5f + CCT_STEP_HYST_ST) {
        prevCCTStep    = cctStepCandidate;
        currentCCT     = 2700.0f + prevCCTStep * 100.0f;
        cctStepChanged = true;
    } else if (cctStepCandidate < prevCCTStep &&
               cctStepFloat < (float)prevCCTStep - 0.5f - CCT_STEP_HYST_ST) {
        prevCCTStep    = cctStepCandidate;
        currentCCT     = 2700.0f + prevCCTStep * 100.0f;
        cctStepChanged = true;
    }

    if (cctStepChanged && !cctFirstCall && systemInitialized) {
        buzzerClick();
    }

    // NORMAL uses the fade/LED engine — do not call applyLEDsImmediate here
    ledmix_set(currentBrightness, currentCCT);
}

void syncPotsAfterBoot(float brightness, float cct)
{
    // Take fresh 8-sample ADC to get accurate post-fade pot positions.
    // Boot-time ADC (16 samples, taken over the soft-start fade duration) can differ from
    // post-fade ADC due to ADC settling at cold start. All internal state must derive from
    // ONE reading to guarantee filter and step indices are consistent — preventing Schmitt snap.
    int dutySum = 0, cctSum = 0;
    for (int i = 0; i < 8; i++) {
        dutySum += analogRead(DUTY_POT_PIN);
        cctSum  += analogRead(CCT_POT_PIN);
    }
    dutyFiltered = constrain((dutySum / 8.0f - DUTY_MIN_RAW) / float(DUTY_MAX_RAW - DUTY_MIN_RAW), 0.0f, 1.0f);
    cctFiltered  = constrain((cctSum  / 8.0f - CCT_MIN_RAW)  / float(CCT_MAX_RAW  - CCT_MIN_RAW),  0.0f, 1.0f);

    // Derive step indices from the SAME normalized values that seed the filter.
    // This is the critical invariant: filter and step indices must always agree,
    // or the very first handlePots() call will trigger a Schmitt transition.
    prevDutyStep      = constrain((int)roundf(dutyFiltered * (NORMAL_STEPS - 1)), 0, NORMAL_STEPS - 1);
    currentBrightness = normalBrightnessSteps[prevDutyStep];
    prevCCTStep       = constrain((int)roundf(cctFiltered * 38.0f), 0, 38);
    currentCCT        = 2700.0f + prevCCTStep * 100.0f;

    Serial.print("syncPotsAfterBoot ADC — DUTY raw: "); Serial.print(dutySum / 8);
    Serial.print(" | CCT raw: "); Serial.println(cctSum / 8);
    Serial.print("  → dutyFiltered: "); Serial.print(dutyFiltered, 4);
    Serial.print(" | cctFiltered: "); Serial.println(cctFiltered, 4);
    Serial.print("  → prevDutyStep: "); Serial.print(prevDutyStep);
    Serial.print(" | prevCCTStep: "); Serial.println(prevCCTStep);

    // Update ledmix to the derived values.
    // If post-boot ADC differs from boot-time ADC, this may shift CCT/brightness
    // at the moment the fade ends — but this is unavoidable and far less visible
    // than a snap one frame later in handlePots().
    ledmix_set(currentBrightness, currentCCT);
}

void resetDumbFilter()
{
    dumbDutyFiltered = -1.0f;
    dumbCCTFiltered  = -1.0f;
}