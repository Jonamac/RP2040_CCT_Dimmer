#ifndef CALIBRATION_CPP
#define CALIBRATION_CPP

#include "calibration.h"

const int DUTY_MIN_RAW = 15;
const int DUTY_MAX_RAW = 4095;

const int CCT_MIN_RAW  = 15;
const int CCT_MAX_RAW  = 4095;

const int NORMAL_STEPS = 22;

// Pre-gamma brightness table — values ARE the raw PWM duties sent to the hardware.
// applyLEDsImmediate() does NOT apply gamma for NORMAL mode; gamma is baked in here
// so equal knob steps produce perceptually uniform brightness steps.
//
// Formula: steps[i] = min_duty + pow(i / 21.0, 2.2) * (1.0 - min_duty)   for i = 0..21
//
// IMPORTANT: Step 0 MUST equal min_duty (0.000244f). It is NOT zero / off.
//            NORMAL mode has STANDBY for turning off; step 0 is the hardware minimum.
//            Pot all the way down = step 0 = min_duty = LEDs dimly on (mDUTY label).
//            Do NOT set step 0 to 0.0f — the NORMAL mixing logic relies on this.
//
// Step  0 = min_duty  — hardware minimum; OLED shows 0.01% + mDUTY label
// Step 11 = 0.240792  — perceptually 50% (12 o'clock); OLED shows 52%
// Step 21 = 1.000000  — full brightness; OLED shows 100%
//
// If min_duty is recalibrated, regenerate both tables with the new min_duty value.
const float normalBrightnessSteps[NORMAL_STEPS] = {
  0.000244f,  // step  0 — min_duty: hardware minimum (OLED: 0.01% + mDUTY)
  0.001478f,  // step  1
  0.005903f,  // step  2
  0.014037f,  // step  3
  0.026270f,  // step  4
  0.042714f,  // step  5
  0.064043f,  // step  6
  0.089508f,  // step  7
  0.119824f,  // step  8
  0.155355f,  // step  9
  0.195630f,  // step 10
  0.240792f,  // step 11 — 12 o'clock, perceptually 50%
  0.292033f,  // step 12
  0.348484f,  // step 13
  0.409976f,  // step 14
  0.477156f,  // step 15
  0.549889f,  // step 16
  0.628377f,  // step 17
  0.712295f,  // step 18
  0.802611f,  // step 19
  0.898127f,  // step 20
  1.000000f,  // step 21 — full brightness
};

// Cosmetic display percentages for NORMAL mode OLED — display-only, not PWM values.
// Appear as visually equal steps so the knob feels uniform.
// Step 0 = 0.01% (mDUTY); steps 1-21 = 1%, 5%, 10%, 15%...95%, 100%
const float normalDisplayPercent[NORMAL_STEPS] = {
   0.01f,  // step  0 — min_duty (mDUTY label shown alongside)
   1.00f,  // step  1
   5.00f,  // step  2
  10.00f,  // step  3
  15.00f,  // step  4
  20.00f,  // step  5
  25.00f,  // step  6
  30.00f,  // step  7
  35.00f,  // step  8
  40.00f,  // step  9
  45.00f,  // step 10
  50.00f,  // step 11 — 12 o'clock
  55.00f,  // step 12
  60.00f,  // step 13
  65.00f,  // step 14
  70.00f,  // step 15
  75.00f,  // step 16
  81.00f,  // step 17
  86.00f,  // step 18
  90.00f,  // step 19
  95.00f,  // step 20
 100.00f,  // step 21 — full brightness
};

#endif // CALIBRATION_CPP