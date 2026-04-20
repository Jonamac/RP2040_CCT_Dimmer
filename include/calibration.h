#ifndef CALIBRATION_H
#define CALIBRATION_H

// Duty pot calibration
extern const int DUTY_MIN_RAW;
extern const int DUTY_MAX_RAW;

// CCT pot calibration
extern const int CCT_MIN_RAW;
extern const int CCT_MAX_RAW;

// Normal mode brightness table
extern const int NORMAL_STEPS;
extern const float normalBrightnessSteps[];

// Cosmetic display percentages for NORMAL mode OLED — display-only, not PWM values.
extern const float normalDisplayPercent[];

#endif // CALIBRATION_H