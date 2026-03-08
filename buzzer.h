#pragma once
#include <Arduino.h>
#include "pins.h"
#include "state.h"

void initBuzzer();
void buzzerClick();
void buzzerLEDUpdateBeep();
void buzzerModeChangeBeep();
void buzzerStartupBeep();
void handleBuzzerToggle(unsigned long now, bool mainPressed, bool dispPressed);