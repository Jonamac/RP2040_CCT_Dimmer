#pragma once
#include <Arduino.h>

int readADC(int pin);

void readInputs(unsigned long now);
void processButtons(unsigned long now);
// Reset DEMO brightness tracking so it re-evaluates the pot
void resetDemoBrightnessTracking();