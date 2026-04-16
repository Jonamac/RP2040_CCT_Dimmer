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

static const float DUTY_STEP_HYST = 0.05f;   // 5% of a step
static const float CCT_STEP_HYST  = 0.05f;

// Local state
static int prevDutyStep = -1;
static int prevCCTStep  = -1;

static float currentBrightness = 0.0f;
static float targetBrightness  = 0.0f;
static float startBrightness   = 0.0f;

static float currentCCT = 4600.0f;
static float targetCCT  = 4600.0f;
static float startCCT   = 4600.0f;

static unsigned long fadeStartMs    = 0;
static unsigned long fadeDurationMs = 0;

// DEMO brightness steps
static const float demoBrightnessSteps[7] = {
    0.05f, 0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f
};

// Hybrid fade curve
static float hybridFade(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.8f) return t;
    float tail = (t - 0.8f) / 0.2f;
    return 0.8f + 0.2f * powf(tail, 1.8f);
}

// Mid-CCT brightness boost (NORMAL mode only)
static float applyCCTBoost(float linearBrightness, float cct)
{
    float cctNorm = (cct - 2700.0f) / (6500.0f - 2700.0f);
    if (cctNorm < 0.0f) cctNorm = 0.0f;
    if (cctNorm > 1.0f) cctNorm = 1.0f;

    float boost   = 1.0f + 0.5f * sinf(PI * cctNorm);
    float boosted = linearBrightness * boost;
    if (boosted > 1.0f) boosted = 1.0f;
    return boosted;
}

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

    static float dutyFiltered = 0.0f;
    static float cctFiltered  = 0.0f;
    dutyFiltered = dutyFiltered * 0.85f + dutyNorm * 0.15f;
    cctFiltered  = cctFiltered  * 0.85f + cctNorm  * 0.15f;
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
        currentBrightness = newB;

        // Continuous CCT (no quantization in DUMB)
        float newC = 2700.0f + cctNorm * (6500.0f - 2700.0f);
        currentCCT = newC;

        // Set target then apply immediately so CCT changes render
        // without needing to move the duty knob first
        ledmix_set(currentBrightness, currentCCT);
        applyLEDsImmediate(currentBrightness, currentCCT);
        return;
    }

    // -------------------------
    // NORMAL MODE — stepped
    // -------------------------
    float mappedCCT = 2700.0f + cctNorm * (6500.0f - 2700.0f);

    // Duty: 21 steps (0..20) with hysteresis
    float dutyStepFloat     = dutyNorm * 20.0f;
    int   dutyStepCandidate = roundf(dutyStepFloat);

    if (fabsf(dutyStepFloat - (float)prevDutyStep) > DUTY_STEP_HYST) {
        if (dutyStepCandidate != prevDutyStep) {
            prevDutyStep = dutyStepCandidate;
            float linear = prevDutyStep / 20.0f;
            if (linear < min_duty) linear = min_duty;

            float estCCT      = 2700.0f + cctNorm * (6500.0f - 2700.0f);
            currentBrightness = applyCCTBoost(linear, estCCT);
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