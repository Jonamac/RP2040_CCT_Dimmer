#include "pwm_control.h"

float currentWarmDuty = 0.0f;
float currentCoolDuty = 0.0f;

// Using Arduino-Pico core helpers
// analogWriteFreq(pin, freq);
// analogWriteRange(range);

void initPWM() {
  analogWriteFreq(25000);     // 25kHz for all PWM pins
  analogWriteRange(4095);     // 12-bit resolution
  analogWrite(WARM_PIN, 0);
  analogWrite(COOL_PIN, 0);
}

void setWarmDuty(float duty) {
  if (duty < 0) duty = 0;
  if (duty > 1) duty = 1;

  currentWarmDuty = duty;   // <--- store it

  int val = (int)(duty * 4095.0f + 0.5f);
  analogWrite(WARM_PIN, val);
}

void setCoolDuty(float duty) {
  if (duty < 0) duty = 0;
  if (duty > 1) duty = 1;

  currentCoolDuty = duty;   // <--- store it

  int val = (int)(duty * 4095.0f + 0.5f);
  analogWrite(COOL_PIN, val);
}