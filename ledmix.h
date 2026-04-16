#pragma once
#include <Arduino.h>
#include "state.h"
#include "pwm_control.h"
#include "buzzer.h"

// Set the LED engine's target brightness + CCT
void ledmix_set(float brightness, float cct);

// Syncs led_current* to led_target* — call once after setup ledmix_set
void ledmix_initCurrent();

// Get the LED engine's current output state
float ledmix_getBrightness();
float ledmix_getCCT();

// Immediate LED application (gamma + mixing + PWM)
void applyLEDsImmediate(float brightness, float cct);

// LED engine update (DUMB fade, FREQ strobe)
void updateLEDLogic(unsigned long now);

// Gamma + brightness table
float applyGamma(float v);
float brightnessTableLookup(float norm);

float ledmix_getWarmDuty();
float ledmix_getCoolDuty();