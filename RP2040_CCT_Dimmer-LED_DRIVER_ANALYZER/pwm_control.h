#pragma once
#include <Arduino.h>

void initPWM();
void setPwmRange(int range);  // keep in sync with analogWriteRange()
void setWarmDuty(float duty); // 0.0–1.0
void setCoolDuty(float duty); // 0.0–1.0