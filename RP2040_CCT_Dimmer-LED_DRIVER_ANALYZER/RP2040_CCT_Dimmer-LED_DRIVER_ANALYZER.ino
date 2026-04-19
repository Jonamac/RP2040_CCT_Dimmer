#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "pins.h"
#include "calibration.h"
#include "pwm_control.h"
#include "display_stub.h"

// -------------------------
// Globals
// -------------------------
bool displayOn = true;
bool buttonsHaveSettled = false;
bool lastUp = false;
bool lastDown = false;
float stepDuty = 0.0f;
int pwmRanges[] = {4095, 2047, 1023, 511, 255};
int pwmRangeIndex = 0;

bool useWarm = true;  // warm channel active, cool off

// -------------------------
// ADC smoothing
// -------------------------
int readADC(int pin) {
    long sum = 0;
    for (int i = 0; i < 12; i++) {
        sum += analogRead(pin);
    }
    return sum / 12;
}

// -------------------------
// Gamma
// -------------------------
float gamma_val = 2.2f;
float applyGamma(float v) {
    if (v <= 0) return 0;
    if (v >= 1) return 1;
    return powf(v, gamma_val);
}

// -------------------------
// CCT preset snapping
// -------------------------
float snapCCT(float cctNorm) {
    // Convert normalized 0–1 to Kelvin
    float cct = 2700.0f + cctNorm * (6500.0f - 2700.0f);

    // Snap to nearest of the three presets
    float d2700 = fabs(cct - 2700.0f);
    float d4600 = fabs(cct - 4600.0f);
    float d6500 = fabs(cct - 6500.0f);

    if (d2700 <= d4600 && d2700 <= d6500) return 0.0f;   // normalized 2700K
    if (d4600 <= d2700 && d4600 <= d6500) return 0.5f;   // normalized 4600K
    return 1.0f;                                         // normalized 6500K
}

// -------------------------
// Step mode globals
// -------------------------
unsigned long upPressStart = 0;
unsigned long downPressStart = 0;
bool upWasHeld = false;
bool downWasHeld = false;

bool buttonMode = false;
float frozenCCT = 0.0f;

float stepSize = 1.0f / 4095.0f;

float min_duty = 0.002f;
float effective_off_threshold = 0.00005f;

// -------------------------
// Setup
// -------------------------
void setup() {
    analogReadResolution(12);

    initDisplay();   // object model, safe to call here
    initPWM();

    pinMode(DUMB_SWITCH_PIN, INPUT_PULLDOWN);
    pinMode(MAIN_BUTTON_PIN, INPUT);   // active-HIGH, external pull-down on PCB
    pinMode(DISP_BUTTON_PIN, INPUT);   // active-HIGH, external pull-down on PCB

    // Seed stepDuty from current pot position so LED starts at pot level
    {
        long sum = 0;
        for (int i = 0; i < 32; i++) sum += analogRead(DUTY_POT_PIN);
        stepDuty = (float)(sum / 32) / 4095.0f;
    }
}

// -------------------------
// Main Loop
// -------------------------
void loop() {

    bool upPressed   = (digitalRead(MAIN_BUTTON_PIN) == HIGH);
    bool downPressed = (digitalRead(DISP_BUTTON_PIN) == HIGH);

    static bool lastUp   = false;
    static bool lastDown = false;

    // -------------------------
    // Channel selection via CCT pot (always live: left=Warm, right=Cool)
    // -------------------------
    {
        int rawCCT = readADC(CCT_POT_PIN);
        useWarm = (rawCCT < 2048);
    }

    // -------------------------
    // Combo detection (both buttons: release <2s = next PWM range, >=2s = prev)
    // -------------------------
    static unsigned long comboStart = 0;
    bool both = upPressed && downPressed;

    if (both && !(lastUp && lastDown)) {
        comboStart = millis();
    }

    if (!both && (lastUp && lastDown)) {
        unsigned long held = millis() - comboStart;
        if (held < 2000) {
            pwmRangeIndex = (pwmRangeIndex + 1) % 5;
        } else {
            pwmRangeIndex = (pwmRangeIndex + 4) % 5;
        }
        setPwmRange(pwmRanges[pwmRangeIndex]);
    }

    if (both) {
        lastUp   = upPressed;
        lastDown = downPressed;
        return; // ignore stepping while combo held
    }

    // -------------------------
    // Rising edges (computed before pot/step sections)
    // -------------------------
    bool upEdge   = (upPressed   && !lastUp);
    bool downEdge = (downPressed && !lastDown);

    // -------------------------
    // Pot coarse control — locked out permanently after first button press
    // -------------------------
    if (upEdge || downEdge) buttonMode = true;

    if (!buttonMode) {
        int rawPot = readADC(DUTY_POT_PIN);
        static int lastRawPot = -1;
        if (lastRawPot < 0 || abs(rawPot - lastRawPot) > 8) {
            stepDuty = (float)rawPot / 4095.0f;
        }
        lastRawPot = rawPot;
    }

    // -------------------------
    // Button stepping (fine control)
    // -------------------------
    const float step1  = 1.0f / pwmRanges[pwmRangeIndex];
    const float step10 = step1 * 10.0f;
    const float step50 = step1 * 50.0f;

    static unsigned long upStart   = 0;
    static unsigned long downStart = 0;

    if (upEdge)   upStart   = millis();
    if (downEdge) downStart = millis();

    bool upLong      = upPressed   && (millis() - upStart   >= 600);
    bool upVeryLong  = upPressed   && (millis() - upStart   >= 1500);
    bool downLong    = downPressed && (millis() - downStart >= 600);
    bool downVeryLong= downPressed && (millis() - downStart >= 1500);

    if (upEdge)                    stepDuty += step1;
    if (downEdge)                  stepDuty -= step1;
    if (upLong    && !upVeryLong)  stepDuty += step10;
    if (downLong  && !downVeryLong)stepDuty -= step10;
    if (upVeryLong)                stepDuty += step50;
    if (downVeryLong)              stepDuty -= step50;

    if (stepDuty < 0.0f) stepDuty = 0.0f;
    if (stepDuty > 1.0f) stepDuty = 1.0f;

    lastUp   = upPressed;
    lastDown = downPressed;

    // -------------------------
    // Apply output
    // -------------------------
    bool gammaOn = (digitalRead(DUMB_SWITCH_PIN) == HIGH);
    float B_linear = stepDuty;
    float B_gamma  = gammaOn ? applyGamma(B_linear) : B_linear;

    if (useWarm) {
        setWarmDuty(B_gamma);
        setCoolDuty(0.0f);
    } else {
        setWarmDuty(0.0f);
        setCoolDuty(B_gamma);
    }

    // -------------------------
    // OLED diagnostics
    // -------------------------
    float dutyPercent = B_gamma * 100.0f;
    int   rawValue    = (int)(B_gamma * pwmRanges[pwmRangeIndex] + 0.5f);
    float pw_ns = (pwmRanges[pwmRangeIndex] > 0)
        ? ((float)rawValue / (float)pwmRanges[pwmRangeIndex]) * 40000.0f
        : 0.0f;

    display.clearDisplay();
    display.setRotation(0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Line 0: RAW count + duty float
    display.setCursor(0, 0);
    display.print("RAW:");
    display.print(rawValue);
    display.setCursor(54, 0);
    display.print("D:");
    display.print(B_gamma, 4);

    // Line 1: duty % + pulse width
    display.setCursor(0, 10);
    display.print(dutyPercent, 3);
    display.print("% ");
    display.print((int)pw_ns);
    display.print("ns");

    // Line 2: range, LIN/GAM, W/C, POT/BTN
    display.setCursor(0, 20);
    display.print("RNG:");
    display.print(pwmRanges[pwmRangeIndex]);
    display.setCursor(54, 20);
    display.print(gammaOn ? "GAM" : "LIN");
    display.setCursor(84, 20);
    display.print(useWarm ? "W" : "C");
    display.setCursor(102, 20);
    display.print(buttonMode ? "BTN" : "POT");

    display.display();

    delay(15);
}