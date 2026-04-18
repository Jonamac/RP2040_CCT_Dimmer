#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "state.h"

void initDisplay();
void toggleDisplay();
void updateDisplayLogic(unsigned long now);
void showStatusFlash(const char* text, int times, int ms);
void demoModeSetSpeedIndex(int idx);

extern unsigned long demoSpeedFlashUntil;
extern int demoSpeedPercent;

#endif // DISPLAY_UI_H