#include "ledmix.h"

float applyGamma(float v) {
  if (v <= 0) return 0;
  if (v >= 1) return 1;
  return powf(v, gamma_val);
}

void applyLEDsImmediate(float brightness, float cct) {
    //DEBUG, for finding the values stored in calibration.cpp (DUTY_MIN_RAW, DUTY_MAX_RAW, CCT_MIN_RAW, CCT_MAX_RAW)
    // Serial.print("MODE=");
    // Serial.print(currentMode);
    // Serial.print("  B_in=");
    // Serial.print(brightness, 6);
    // Serial.print("  CCT=");
    // Serial.println(cct);
    // 1. Clamp input brightness to [0,1]


    // 1. Clamp brightness
    if (brightness <= 0.0f) {
        setWarmDuty(0.0f);
        setCoolDuty(0.0f);
        return;
    }
    if (brightness > 1.0f) brightness = 1.0f;

    // 2. Gamma usage
    bool useGamma =
        !(currentMode == MODE_FREQ || currentMode == MODE_CAL);

    float B_linear      = brightness;
    float B_perceptual  = useGamma ? applyGamma(B_linear) : B_linear;

    // 3. Effective-off threshold (NOT used in DUMB mode)
    // DUMB MODE must NEVER use effective-off threshold
    if (currentMode == MODE_DUMB) {
        // skip cutoff logic entirely
    } else {
        if (B_perceptual < effective_off_threshold) {
            setWarmDuty(0.0f);
            setCoolDuty(0.0f);
            return;
        }
    }

    // 4. Compute CCT mix
    float t = (cct - 2700.0f) / (6500.0f - 2700.0f);
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    float wCool = t;
    float wWarm = 1.0f - t;

    float dutyWarm_lin = 0.0f;
    float dutyCool_lin = 0.0f;

    // ============================================================
    //  DUMB MODE — CORRECT, FINAL, SINGLE IMPLEMENTATION
    // ============================================================
    if (currentMode == MODE_DUMB) {

        float mix = t;

        if (B_linear <= min_duty) {
            // Minimum brightness behavior
            if (mix == 0.0f) {
                dutyWarm_lin = min_duty;
                dutyCool_lin = 0.0f;
            } else if (mix == 1.0f) {
                dutyWarm_lin = 0.0f;
                dutyCool_lin = min_duty;
            } else {
                dutyWarm_lin = min_duty;
                dutyCool_lin = min_duty;
            }
        } else {
            // Above min_duty
            if (mix == 0.0f) {
                dutyWarm_lin = B_linear;
                dutyCool_lin = 0.0f;
            } else if (mix == 1.0f) {
                dutyWarm_lin = 0.0f;
                dutyCool_lin = B_linear;
            } else {
                float extra = B_linear - min_duty;
                dutyWarm_lin = min_duty + extra * (1.0f - mix);
                dutyCool_lin = min_duty + extra * mix;
            }
        }

        // DEBUG
        Serial.print("APPLY[DUMB]: B_in=");
        Serial.print(brightness, 6);
        Serial.print(" CCT=");
        Serial.print(cct);
        Serial.print(" W_lin=");
        Serial.print(dutyWarm_lin, 6);
        Serial.print(" C_lin=");
        Serial.println(dutyCool_lin, 6);

    }
    // ============================================================
    //  NORMAL / DEMO / STANDBY / OVERRIDE
    // ============================================================
    else {

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
            float scale = (reserved > 0) ? B_linear / reserved : 0;
            dutyWarm_lin = (wWarm > 0) ? min_duty * scale : 0;
            dutyCool_lin = (wCool > 0) ? min_duty * scale : 0;
        }

        // DEBUG
        Serial.print("APPLY[NORM]: B_in=");
        Serial.print(brightness, 6);
        Serial.print(" CCT=");
        Serial.print(cct);
        Serial.print(" W_lin=");
        Serial.print(dutyWarm_lin, 6);
        Serial.print(" C_lin=");
        Serial.println(dutyCool_lin, 6);
    }

    // ============================================================
    //  6. Apply perceptual scaling (gamma)
    // ============================================================
    float scale = useGamma ? (B_perceptual / B_linear) : 1.0f;

    float dutyWarm = dutyWarm_lin * scale;
    float dutyCool = dutyCool_lin * scale;

    setWarmDuty(dutyWarm);
    setCoolDuty(dutyCool);
}

void updateLEDLogic(unsigned long now) {
    //debug
    Serial.print("DUMB FADE: mode=");
    Serial.print((int)currentMode);
    Serial.print(" t=");
    Serial.print((float)(now - dumbFadeStartTime) / (float)dumbFadeDuration, 3);
    Serial.print(" B=");
    Serial.println(currentBrightness, 4);

    unsigned long elapsed = now - dumbFadeStartTime;
    float t = (float)elapsed / (float)dumbFadeDuration;
    // 1) Dedicated DUMB fade handler (runs regardless of mode)
    if (dumbFadeActive) {
        unsigned long elapsed;
        if (now >= dumbFadeStartTime) {
            elapsed = now - dumbFadeStartTime;
        } else {
            elapsed = 0;  // guard against underflow
        }

        float t = (float)elapsed / (float)dumbFadeDuration;
        if (t > 1.0f) t = 1.0f;

        float newB = dumbFadeStartB + (dumbFadeEndB - dumbFadeStartB) * t;

        currentBrightness = newB;
        applyLEDsImmediate(currentBrightness, currentCCT);

        if (t >= 1.0f) {
            dumbFadeActive = false;
        }
        return;
    }

    // 2) DUMB mode (no generic fade when not in a DUMB-specific fade)
    if (currentMode == MODE_DUMB) {
        return;
    }

    // 3) Generic fade engine for all non-DUMB modes
    if (lastLEDUpdateTime == 0) {
        lastLEDUpdateTime = now;
        return;
    }

    unsigned long dt = now - lastLEDUpdateTime;
    if (dt == 0) return;
    lastLEDUpdateTime = now;

    float fadeMs;
    if (currentMode == MODE_STANDBY) {
        fadeMs = standby_fade_time_ms;
    } else if (currentMode == MODE_FREQ || currentMode == MODE_CAL) {
        fadeMs = fade_time_ms / 2.0f;
    } else {
        fadeMs = fade_time_ms;
    }

    float step = (float)dt / (float)fadeMs;
    if (step > 1.0f) step = 1.0f;

    float newB = currentBrightness + (targetBrightness - currentBrightness) * step;
    float newC = currentCCT + (targetCCT - currentCCT) * step;

    if (fabs(newB - targetBrightness) < 0.0005f) newB = targetBrightness;
    if (fabs(newC - targetCCT)      < 0.1f)     newC = targetCCT;

    if (fabs(newB - currentBrightness) < 0.0001f &&
        fabs(newC - currentCCT)        < 0.1f) {
        return;
    }

    currentBrightness = newB;
    currentCCT        = newC;

    applyLEDsImmediate(currentBrightness, currentCCT);
}
// Temporarily disable LED update beeps to avoid long/late buzzing
// We'll reintroduce a single "done" chirp later if desired.
// buzzerLEDUpdateBeep();
