#include "calibration.h"

const int DUTY_MIN_RAW = 12;
const int DUTY_MAX_RAW = 4095;

const int CCT_MIN_RAW  = 12;
const int CCT_MAX_RAW  = 4095;

const int NORMAL_STEPS = 22;
const float normalBrightnessSteps[NORMAL_STEPS] = {
  0.00f, 0.025f, 0.05f,
  0.10f, 0.15f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f,
  0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f,
  0.85f, 0.90f, 0.95f, 1.00f
};