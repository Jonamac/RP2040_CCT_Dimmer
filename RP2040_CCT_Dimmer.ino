#include <Arduino.h>
#include "pins.h"
#include "state.h"
#include "pwm_control.h"
#include "buzzer.h"
#include "inputs.h"
#include "pots.h"
#include "ledmix.h"
#include "display_ui.h"
#include "modes.h"
#include "timing.h"
#include "freq_mode.h"
#include "calibration.h"

void setup() {
    Serial.begin(115200);
    analogReadResolution(12);

    initPins();
    initPWM();
    analogWrite(WARM_PIN, 0);
    analogWrite(COOL_PIN, 0);
    initBuzzer();
    initDisplay();
    initTiming();
    initModes();

    pinMode(DUMB_SWITCH_PIN, INPUT_PULLDOWN);

    unsigned long now = millis();

    // -------------------------------------------------------
    //  CHECK DUMB MODE *BEFORE* ANYTHING ELSE
    // -------------------------------------------------------
    bool dumbAtBoot = digitalRead(DUMB_SWITCH_PIN);

    if (dumbAtBoot) {
        // Set mode immediately so all subsequent logic is correct
        currentMode = MODE_DUMB;

        buzzer_click_enabled = false;
        buzzer_beep_enabled  = false;

        // Allow ADC input capacitance and voltage reference to settle after power-on.
        delay(500);

        // Average 32 readings for stable boot-time pot values
        // (single samples are too noisy at 12-bit resolution)
        int dutySum = 0, cctSum = 0;
        for (int i = 0; i < 32; i++) {
            dutySum += analogRead(DUTY_POT_PIN);
            cctSum  += analogRead(CCT_POT_PIN);
        }
        int rawDutyADC = dutySum / 32;
        int rawCCTADC  = cctSum  / 32;
        Serial.print("Boot ADC — DUTY raw: "); Serial.print(rawDutyADC);
        Serial.print(" | CCT raw: "); Serial.println(rawCCTADC);

        float dutyNorm = constrain(
            (rawDutyADC - DUTY_MIN_RAW) / float(DUTY_MAX_RAW - DUTY_MIN_RAW),
            0.0f, 1.0f);
        float endB = min_duty + dutyNorm * (1.0f - min_duty);

        float cctNorm = constrain(
            (rawCCTADC - CCT_MIN_RAW) / float(CCT_MAX_RAW - CCT_MIN_RAW),
            0.0f, 1.0f);
        float startCCT = 2700.0f + cctNorm * (6500.0f - 2700.0f);

        // Apply initial state (LEDs off, CCT set)
        ledmix_set(0.0f, startCCT);
        applyLEDsImmediate(0.0f, startCCT);
        ledmix_initCurrent();

        // Start async DUMB boot fade
        dumbFadeActive    = true;
        dumbFadeDirection = true;
        dumbFadeStartB    = 0.0f;
        dumbFadeEndB      = endB;
        dumbFadeStartTime = now;
        dumbFadeDuration  = dumb_soft_start_ms;
        bootFadeActive    = true;

        // systemInitialized set by updateLEDLogic when dumbFade completes
        return;
    }

    // -------------------------------------------------------
    //  NORMAL BOOT — async soft start
    // -------------------------------------------------------
    buzzer_click_enabled = false;
    buzzer_beep_enabled  = true;

    // Allow ADC input capacitance and voltage reference to settle after power-on.
    delay(500);

    // Average 32 readings for stable boot-time pot values
    // (single samples are too noisy at 12-bit resolution)
    int dutySum = 0, cctSum = 0;
    for (int i = 0; i < 32; i++) {
        dutySum += analogRead(DUTY_POT_PIN);
        cctSum  += analogRead(CCT_POT_PIN);
    }
    int rawDutyADC = dutySum / 32;
    int rawCCTADC  = cctSum  / 32;
    Serial.print("Boot ADC — DUTY raw: "); Serial.print(rawDutyADC);
    Serial.print(" | CCT raw: "); Serial.println(rawCCTADC);

    float dutyNorm = constrain(
        (rawDutyADC - DUTY_MIN_RAW) / float(DUTY_MAX_RAW - DUTY_MIN_RAW),
        0.0f, 1.0f);
    int idx = round(dutyNorm * (NORMAL_STEPS - 1));
    idx = constrain(idx, 0, NORMAL_STEPS - 1);
    float targetB = normalBrightnessSteps[idx];

    float cctNorm = constrain(
        (rawCCTADC - CCT_MIN_RAW) / float(CCT_MAX_RAW - CCT_MIN_RAW),
        0.0f, 1.0f);
    int stepIdx = constrain((int)round(cctNorm * 38.0f), 0, 38);
    float startCCT = 2700.0f + stepIdx * 100.0f;

    // Apply initial state (LEDs off, CCT set)
    ledmix_set(0.0f, startCCT);
    applyLEDsImmediate(0.0f, startCCT);
    ledmix_initCurrent();

    // Fire startup beep immediately (buzzerStartupBeep ignores enable flags)
    buzzerStartupBeep();

    // Start async NORMAL boot fade
    normalFadeActive    = true;
    normalFadeStartB    = 0.0f;
    normalFadeEndB      = targetB;
    normalFadeStartTime = millis();
    normalFadeDuration  = soft_start_ms;
    bootFadeActive      = true;

    // systemInitialized and buzzer_click_enabled set by updateLEDLogic when fade completes
}

void loop() {
  unsigned long now = millis();

  readInputs(now);          // pots + buttons + buzzer toggles
  updateModeState(now);     // state machine transitions
  updateModeBehavior(now);  // per-mode behavior (demo, standby, etc.)
  updateLEDLogic(now);      // gamma, min duty, fades, LED update delay
  updateDisplayLogic(now);  // text, bars, flashing, timeout
}