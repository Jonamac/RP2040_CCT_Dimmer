#pragma once
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

void initDisplay();
void drawAnalyzerUI(
    bool buttonMode,
    float B_linear,
    float B_gamma,
    float cctNorm,
    float stepDuty
);