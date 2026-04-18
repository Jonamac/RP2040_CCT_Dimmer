#ifndef BUZZER_CPP
#define BUZZER_CPP

#include "buzzer.h"

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

static void playTone(float freq, int ms) {
  if (freq <= 0 || ms <= 0) return;
  tone(BUZZER_PIN, (unsigned int)freq, ms);
}

void buzzerClick() {
  if (currentMode == MODE_DUMB) return;
  if (buzzer_click_enabled) {
    playTone(knob_click_freq, knob_click_ms);
  }
}

void buzzerLEDUpdateBeep() {
  if (currentMode == MODE_DUMB) return;
  if (buzzer_beep_enabled) {
    playTone(led_update_beep_freq, led_update_beep_ms);
  }
}

void buzzerModeChangeBeep() {
  if (currentMode == MODE_DUMB) return;
  if (buzzer_beep_enabled) {
    playTone(mode_change_beep_freq, mode_change_beep_ms);
  }
}

void buzzerStartupBeep() {
  // Startup beep ignores enable flags
  playTone(led_update_beep_freq, led_update_beep_ms);
}

// Dual-button hold to toggle buzzer modes //was changed to dispButton long press, dual-button hold needs a new function
void handleBuzzerToggle(unsigned long now, bool mainPressed, bool dispPressed) {
  if (currentMode == MODE_DUMB) return;
  
  bool both = mainPressed && dispPressed;

  if (both && !bothButtonsLast) {
    bothButtonsPressTime = now;
  }

  if (!both && bothButtonsLast) {
    unsigned long held = now - bothButtonsPressTime;
    if (held > 800) { // long-ish hold
      static int buzzerState = 0;
      buzzerState = (buzzerState + 1) % 3;

      if (buzzerState == 0) {
        buzzer_click_enabled = true;
        buzzer_beep_enabled  = true;
      } else if (buzzerState == 1) {
        buzzer_click_enabled = true;
        buzzer_beep_enabled  = false;
      } else {
        buzzer_click_enabled = false;
        buzzer_beep_enabled  = false;
      }
    }
  }

  bothButtonsLast = both;
}

#endif // BUZZER_CPP