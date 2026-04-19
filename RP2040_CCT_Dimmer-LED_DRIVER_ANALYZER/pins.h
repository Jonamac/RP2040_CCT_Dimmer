#pragma once
#include <Arduino.h>

// LED channels
#define WARM_PIN        0
#define COOL_PIN        1

// Inputs
#define MAIN_BUTTON_PIN 2
#define DISP_BUTTON_PIN 3
#define DUMB_SWITCH_PIN 6

// OLED I2C
#define OLED_SDA        4
#define OLED_SCL        5

// Pots
#define DUTY_POT_PIN    26  // ADC0
#define CCT_POT_PIN     27  // ADC1

// Buzzer
#define BUZZER_PIN      7

inline void initPins() {
  pinMode(WARM_PIN, OUTPUT);
  pinMode(COOL_PIN, OUTPUT);
  pinMode(MAIN_BUTTON_PIN, INPUT);
  pinMode(DISP_BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}