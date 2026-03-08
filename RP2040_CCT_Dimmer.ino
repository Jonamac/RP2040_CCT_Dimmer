#include <Arduino.h>
#include "pins.h"
#include "state.h"
#include "pwm_control.h"
#include "buzzer.h"
#include "inputs.h"
#include "ledmix.h"
#include "display_ui.h"
#include "modes.h"
#include "timing.h"
#include "freq_mode.h"

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  initPins();
  initPWM();
  // Force PWM slices to attach before any mode logic runs
  analogWrite(WARM_PIN, 0);
  analogWrite(COOL_PIN, 0);
  initBuzzer();
  initDisplay();
  initTiming();
  initModes();

  pinMode(DUMB_SWITCH_PIN, INPUT_PULLDOWN);

  unsigned long now = millis();

  // ------------------------------------------------------
  //  CHECK DUMB MODE *BEFORE* ANYTHING ELSE
  // ------------------------------------------------------
  bool dumbAtBoot = digitalRead(DUMB_SWITCH_PIN);

  if (dumbAtBoot) {
    buzzer_click_enabled = false;
    buzzer_beep_enabled  = false;

    readInputs(now);

    // Compute pot brightness once
    float startB = 0.0f;
    float endB   = targetBrightness;

    // Start fade UP
    dumbFadeActive     = true;
    dumbFadeDirection  = true;
    dumbFadeStartB     = startB;
    dumbFadeEndB       = endB;
    dumbFadeStartTime  = now;
    dumbFadeDuration   = dumb_soft_start_ms;

    currentBrightness = startB;
    currentCCT        = targetCCT;
    applyLEDsImmediate(currentBrightness, currentCCT);

    systemInitialized = true;
    return;
  }


  // ------------------------------------------------------
  //  NORMAL BOOT (soft start)
  // ------------------------------------------------------
  buzzer_click_enabled = false;
  buzzer_beep_enabled  = true;

  readInputs(now);

  currentBrightness = 0.0f;
  currentCCT        = targetCCT;
  applyLEDsImmediate(0.0f, currentCCT);

  const int steps = 100;
  for (int i = 0; i <= steps; i++) {
      if (i == 0) {
          buzzerStartupBeep();
      }
      float b = targetBrightness * ((float)i / steps);
      applyLEDsImmediate(b, currentCCT);
      delay(soft_start_ms / steps);
  }

  currentBrightness = targetBrightness;
  currentCCT        = targetCCT;

  buzzer_click_enabled = true;
  systemInitialized = true;
}

void loop() {
  unsigned long now = millis();

  readInputs(now);          // pots + buttons + buzzer toggles
  updateModeState(now);     // state machine transitions
  updateModeBehavior(now);  // per-mode behavior (demo, standby, etc.)
  if (currentMode != MODE_DUMB) {
    updateLEDLogic(now);      // gamma, min duty, fades, LED update delay
  }
  updateDisplayLogic(now);  // text, bars, flashing, timeout
}