#pragma once
#include <Arduino.h>
#include "pins.h"

extern float currentWarmDuty;
extern float currentCoolDuty;

void initPWM();
void setWarmDuty(float duty); // 0.0–1.0
void setCoolDuty(float duty); // 0.0–1.0