#pragma once
#include <Arduino.h>
#include "state.h"
#include "pwm_control.h"
#include "buzzer.h"

void applyLEDsImmediate(float brightness, float cct);
void updateLEDLogic(unsigned long now);

float applyGamma(float v);
float brightnessTableLookup(float norm);