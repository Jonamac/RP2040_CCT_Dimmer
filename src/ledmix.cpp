#ifndef LEDMIX_CPP
#define LEDMIX_CPP

#include "ledmix.h"
#include "brightness_table.h"
#include "pots.h"

// ============================================================
// Internal LED engine state
// ============================================================
static float led_currentBrightness = 0.0f;
static float led_currentCCT        = 4600.0f;

static float led_targetBrightness  = 0.0f;
static float led_targetCCT         = 4600.0f;

static float lastWarmDuty = 0.0f;
static float lastCoolDuty = 0.0f;

// ============================================================
// Public getters
// ============================================================
float ledmix_getBrightness() { return led_currentBrightness; }
float ledmix_getCCT()        { return led_currentCCT; }

// ============================================================
// Public setter — called by pots.cpp
// ============================================================
void ledmix_set(float brightness, float cct)
{
    led_targetBrightness = brightness;
    led_targetCCT        = cct;
}

// ============================================================
// Sync current to target — call once in setup after ledmix_set
// ============================================================
void ledmix_initCurrent()
{
    led_currentBrightness = led_targetBrightness;
    led_currentCCT        = led_targetCCT;
}

// ============================================================
// Gamma correction
// ============================================================
float applyGamma(float v)
{
    if (v <= 0) return 0;
    if (v >= 1) return 1;
    return powf(v, gamma_val);
}

// ============================================================
// Immediate LED application (gamma + mixing + PWM)
// ============================================================
void applyLEDsImmediate(float brightness, float cct)
{
    // Clamp brightness
    if (brightness <= 0.0f) {
        setWarmDuty(0.0f);
        setCoolDuty(0.0f);
        return;
    }
    if (brightness > 1.0f) brightness = 1.0f;

    // DUMB mode applies its own internal gamma (norm_g path) and returns early.
    // All other modes (NORMAL, STANDBY, DEMO, FREQ, CAL) use pre-gamma table values
    // or raw duty cycles — no additional gamma correction applied.
    // effective_off_threshold is retired: min_duty is the hardware-calibrated floor;
    // sub-min_duty values during fades quantize to RAW:0 via setWarmDuty/setCoolDuty.
    bool useGamma = (currentMode == MODE_DUMB);

    float B_linear     = brightness;
    float B_perceptual = useGamma ? applyGamma(B_linear) : B_linear;

    // CCT mix
    float t = (cct - 2700.0f) / (6500.0f - 2700.0f);
    t = constrain(t, 0.0f, 1.0f);

    float wCool = t;
    float wWarm = 1.0f - t;

    float dutyWarm_lin = 0.0f;
    float dutyCool_lin = 0.0f;

    // ============================================================
    // DUMB MODE
    // Gamma is applied only to the normalised range *above* min_duty so that:
    //   pot-minimum  → physical channel duty = min_duty  (hardware minimum, dim but stable)
    //   pot-maximum  → physical channel duty = 1.0
    // This prevents the generic gamma scale below from crushing the output to
    // pow(min_duty, 2.2) ≈ 0.00114 at pot-minimum, which is below the stable
    // operating threshold and makes the standby fade imperceptible.
    // ============================================================
    if (currentMode == MODE_DUMB)
    {
        float mix = t;
        float dutyWarm_out, dutyCool_out;

        if (B_linear <= min_duty) {
            // Linear fade region — only traversed during fades (boot, standby).
            // Scale channel duties proportionally so the fade from 0 → min_duty
            // produces a visible, smooth ramp from off to the hardware minimum.
            float s = (min_duty > 0.0f) ? (B_linear / min_duty) : 0.0f;
            if (mix == 0.0f) {
                dutyWarm_out = min_duty * s;
                dutyCool_out = 0.0f;
            } else if (mix == 1.0f) {
                dutyWarm_out = 0.0f;
                dutyCool_out = min_duty * s;
            } else {
                dutyWarm_out = min_duty * s;
                dutyCool_out = min_duty * s;
            }
        } else {
            // Normal operating range: apply gamma to the normalised portion
            // above min_duty. B_phys maps [min_duty, 1.0] → [min_duty, 1.0]
            // with a gamma curve in between.
            float norm   = (B_linear - min_duty) / (1.0f - min_duty); // 0→1
            float norm_g = useGamma ? powf(norm, gamma_val) : norm;
            float B_phys = min_duty + norm_g * (1.0f - min_duty);     // min_duty→1.0

            if (mix == 0.0f) {
                dutyWarm_out = B_phys;
                dutyCool_out = 0.0f;
            } else if (mix == 1.0f) {
                dutyWarm_out = 0.0f;
                dutyCool_out = B_phys;
            } else {
                float extra  = B_phys - min_duty;
                dutyWarm_out = min_duty + extra * (1.0f - mix);
                dutyCool_out = min_duty + extra * mix;
            }
        }

        lastWarmDuty = dutyWarm_out;
        lastCoolDuty = dutyCool_out;
        setWarmDuty(dutyWarm_out);
        setCoolDuty(dutyCool_out);
        return;
    }

    // ============================================================
    // NORMAL / DEMO / STANDBY / OVERRIDE
    // ============================================================
    else
    {
        int active = (wWarm > 0 ? 1 : 0) + (wCool > 0 ? 1 : 0);
        float reserved = active * min_duty;

        if (B_linear >= reserved) {
            float Brem = B_linear - reserved;
            float sumW = wWarm + wCool;
            float wWarmN = (sumW > 0) ? wWarm / sumW : 0;
            float wCoolN = (sumW > 0) ? wCool / sumW : 0;

            dutyWarm_lin = (wWarm > 0) ? (min_duty + Brem * wWarmN) : 0;
            dutyCool_lin = (wCool > 0) ? (min_duty + Brem * wCoolN) : 0;
        } else {
            // B_linear < combined channel floor (traversed during fades only, and at step 0).
            // Mirror DUMB fade-region behavior: scale both active channels from 0 → min_duty.
            // At step 0 (B_linear = min_duty, both active), s = 1.0 → each channel = min_duty.
            // During standby fade-to-zero, s ramps from 1.0 → 0 smoothly.
            float s = (min_duty > 0.0f) ? (B_linear / min_duty) : 0.0f;
            dutyWarm_lin = (wWarm > 0) ? min_duty * s : 0.0f;
            dutyCool_lin = (wCool > 0) ? min_duty * s : 0.0f;
        }
    }

    // Gamma scaling
    float scale = useGamma ? (B_perceptual / B_linear) : 1.0f;

    float dutyWarm = dutyWarm_lin * scale;
    float dutyCool = dutyCool_lin * scale;

    lastWarmDuty = dutyWarm;
    lastCoolDuty = dutyCool;

    setWarmDuty(dutyWarm);
    setCoolDuty(dutyCool);
}

// ============================================================
// Brightness table lookup
// ============================================================
float brightnessTableLookup(float norm)
{
    int idx = round(norm * (BRIGHTNESS_STEPS - 1));
    idx = constrain(idx, 0, BRIGHTNESS_STEPS - 1);
    return brightnessTable[idx];
}

// ============================================================
// LED engine update — NORMAL fade + DUMB fade + FREQ strobe
// ============================================================
void updateLEDLogic(unsigned long now)
{
    // ---------------------------
    // NORMAL fade engine
    // (STANDBY→NORMAL, NORMAL→STANDBY, boot)
    // ---------------------------
    if (normalFadeActive)
    {
        unsigned long elapsed =
            (now >= normalFadeStartTime) ? (now - normalFadeStartTime) : 0;

        float t = (float)elapsed / (float)normalFadeDuration;
        if (t > 1.0f) t = 1.0f;

        float newB = normalFadeStartB + (normalFadeEndB - normalFadeStartB) * t;

        led_currentBrightness = newB;
        led_currentCCT        = led_targetCCT;  // CCT snaps, not faded

        applyLEDsImmediate(led_currentBrightness, led_currentCCT);

        if (t >= 1.0f)
        {
            normalFadeActive = false;

            // Sync target so fallthrough doesn't snap to stale 0
            led_targetBrightness = normalFadeEndB;

            if (bootFadeActive)
            {
                bootFadeActive       = false;
                systemInitialized    = true;
                buzzer_click_enabled = true;  // enable clicks after NORMAL boot
                syncPotsAfterBoot(normalFadeEndB, led_currentCCT);
                ledmix_initCurrent();  // sync current to updated target; prevents 1-frame CCT snap
                applyLEDsImmediate(led_currentBrightness, led_currentCCT);  // re-render at corrected CCT same frame — no snap
            }
        }

        return;
    }

    // ---------------------------
    // DUMB fade engine
    // ---------------------------
    if (dumbFadeActive)
    {
        unsigned long elapsed =
            (now >= dumbFadeStartTime) ? (now - dumbFadeStartTime) : 0;

        float t = (float)elapsed / (float)dumbFadeDuration;
        if (t > 1.0f) t = 1.0f;

        float newB = dumbFadeStartB + (dumbFadeEndB - dumbFadeStartB) * t;

        led_currentBrightness = newB;
        applyLEDsImmediate(led_currentBrightness, led_currentCCT);

        if (t >= 1.0f)
        {
            dumbFadeActive = false;

            // Sync target so fallthrough doesn't snap to stale 0
            led_targetBrightness = dumbFadeEndB;

            if (bootFadeActive)
            {
                bootFadeActive    = false;
                systemInitialized = true;
                // buzzer_click_enabled stays false in DUMB — buzzer PR handles this
            }
        }

        return;
    }

    // ---------------------------
    // FREQ strobe engine
    // ---------------------------
    if (currentMode == MODE_FREQ)
    {
        unsigned long nowMs = millis();

        if (freqCycleStartTime == 0) {
            freqCycleStartTime = nowMs;
            freqOnPhase = true;
        }

        float periodMs = 1000.0f / freqStrobeHz;
        float onMs     = periodMs * freqDutyCycle;

        unsigned long elapsed = nowMs - freqCycleStartTime;

        if (elapsed >= periodMs) {
            freqCycleStartTime = nowMs;
            elapsed = 0;
        }

        bool onNow = (elapsed < onMs);

        float effectiveB = onNow ? led_targetBrightness : 0.0f;

        led_currentBrightness = effectiveB;
        applyLEDsImmediate(led_currentBrightness, led_currentCCT);
        return;
    }

    // ---------------------------
    // NORMAL / DEMO / STANDBY
    // ---------------------------
    led_currentBrightness = led_targetBrightness;
    led_currentCCT        = led_targetCCT;

    applyLEDsImmediate(led_currentBrightness, led_currentCCT);
}

float ledmix_getWarmDuty() { return lastWarmDuty; }
float ledmix_getCoolDuty() { return lastCoolDuty; }

#endif // LEDMIX_CPP