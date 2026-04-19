#include "pwm_control.h"
#include "pins.h"

static int g_pwmRange = 4095;

void initPWM() {
  g_pwmRange = 4095;
  analogWriteFreq(25000);
  analogWriteRange(g_pwmRange);
  analogWrite(WARM_PIN, 0);
  analogWrite(COOL_PIN, 0);
}

void setPwmRange(int range) {
  g_pwmRange = range;
  analogWriteRange(range);
}

void setWarmDuty(float duty) {
  if (duty < 0) duty = 0;
  if (duty > 1) duty = 1;
  int val = (int)(duty * (float)g_pwmRange + 0.5f);
  analogWrite(WARM_PIN, val);
}

void setCoolDuty(float duty) {
  if (duty < 0) duty = 0;
  if (duty > 1) duty = 1;
  int val = (int)(duty * (float)g_pwmRange + 0.5f);
  analogWrite(COOL_PIN, val);
}