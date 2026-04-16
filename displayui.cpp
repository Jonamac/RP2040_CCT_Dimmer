#include "display_ui.h"
#include "state.h"
#include "pwm_control.h"
#include "ledmix.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>

extern float min_duty;
int demoSpeedIndex = 0;

void demoModeSetSpeedIndex(int idx) {
    if (idx < 0) idx = 0;
    if (idx > 6) idx = 6;
    demoSpeedIndex = idx;
}

void toggleDisplay() {
    displayOn = !displayOn;
    if (displayOn) {
        lastDisplayOnTime = millis();
    }
}

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Demo speed flash state
unsigned long demoSpeedFlashUntil = 0;
int demoSpeedPercent = -1;

void initDisplay() {
    Wire.setSDA(OLED_SDA);
    Wire.setSCL(OLED_SCL);
    Wire.begin();
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();
    displayOn = true;
    lastDisplayOnTime = millis();
}

// ------------------------------------------------------------
// Draw STANDBY badge in bottom-right corner
// ------------------------------------------------------------
static void drawStandbyBadge() {
    display.setTextSize(1);
    display.setCursor(86, 23); 
    display.print("STANDBY");
}

// ------------------------------------------------------------
// Draw main UI with mode label + bars + optional status text
// ------------------------------------------------------------
static void drawMainUI(const char* modeLabel, bool showStandby) {
    display.clearDisplay();
    display.setRotation(0); // upside-down
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Choose what to display
    float dispB;
    float dispC;

    if (currentMode == MODE_DEMO) {
        dispB = ledmix_getBrightness();
        dispC = ledmix_getCCT();

        // Round CCT to nearest 50K for readability
        dispC = round(dispC / 50.0f) * 50.0f;
    } else {
        dispB = ledmix_getBrightness();
        dispC = ledmix_getCCT();
    }

    // DUTY line
    display.setCursor(0, 2);
    display.print("DUTY ");
    float displayDutyPercent;
    if (currentMode == MODE_DUMB || (currentMode == MODE_STANDBY && previousMode == MODE_DUMB)) {
        // Normalize DUMB brightness to 0–100% relative to the usable range:
        // 0% = pot fully down (LEDs at min_duty, just barely on)
        // 100% = pot fully up
        float b = ledmix_getBrightness();
        float range = 1.0f - min_duty;
        displayDutyPercent = (range > 0.0f) ? ((b - min_duty) / range * 100.0f) : 0.0f;
        displayDutyPercent = constrain(displayDutyPercent, 0.01f, 100.0f);
    } else {
        // NORMAL / STANDBY(from NORMAL) / DEMO / FREQ / etc: raw brightness * 100
        displayDutyPercent = ledmix_getBrightness() * 100.0f;
    }
    display.print(displayDutyPercent, 2);
    display.print("%");

    // =====================
    // FREQ MODE DISPLAY
    // =====================
    if (currentMode == MODE_FREQ) {
        display.setCursor(0, 12);
        display.print("FREQ ");
        display.print(freqStrobeHz, 3);
        display.print("Hz");
    }
    else {
        // Normal CCT line
        display.setCursor(0, 12);
        display.print("CCT ");
        int cctRounded = (int)(round(dispC / 100.0f) * 100.0f);
        display.print(cctRounded);
        display.print("K");
    }

    if (currentWarmDuty == min_duty || currentCoolDuty == min_duty) {
        display.setCursor(100, 2);  //Match DUTY line heright
        display.print("mDUTY");
    }

    // Main mode label (NORMAL, DUMB, DEMO, FREQ, CAL)
    if (modeLabel && strlen(modeLabel) > 0) {
        display.setCursor(90, 12);
        display.print(modeLabel);
    }

    // STANDBY badge if needed
    if (showStandby) {
        drawStandbyBadge();
    }

    // Progress bars (use dispB/dispC so they match the text)
    int dutyBar = max(1, (int)(dispB * SCREEN_WIDTH));

    float t = (dispC - 2700.0f) / (6500.0f - 2700.0f);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int cctBar = max(1, (int)(t * SCREEN_WIDTH));

    display.fillRect(0, 0, dutyBar, 1, SSD1306_WHITE);
    display.fillRect(0, SCREEN_HEIGHT - 1, cctBar, 1, SSD1306_WHITE);

    display.display();
}

// ------------------------------------------------------------
// Update display based on current mode + standby state
// ------------------------------------------------------------
void updateDisplayLogic(unsigned long now) {
    if (!displayOn) {
        display.clearDisplay();
        display.display();
        return;
    }

    const char* modeLabel = "";
    bool showStandby = false;

    // Determine if we are in STANDBY
    if (currentMode == MODE_STANDBY) {
        showStandby = true;

        // The mode label should reflect the *previous* mode
        switch (previousMode) {
            case MODE_NORMAL:        modeLabel = "NORMAL"; break;
            case MODE_DUMB:          modeLabel = "DUMB"; break;
            case MODE_DEMO:          modeLabel = "DEMO"; break;
            case MODE_FREQ: modeLabel = "FREQ"; break;
            case MODE_CAL:  modeLabel = "CAL";  break;
            default:                 modeLabel = ""; break;
        }
    }
    else {
        // Active modes
        switch (currentMode) {
            case MODE_NORMAL:        modeLabel = "NORMAL"; break;
            case MODE_DUMB:          modeLabel = "DUMB"; break;
            case MODE_FREQ: modeLabel = "FREQ"; break;
            case MODE_CAL:  modeLabel = "CAL";  break;

            case MODE_DEMO:
                // If a speed flash is active, show SPEED instead of DEMO
                if (now < demoSpeedFlashUntil && demoSpeedPercent >= 0) {
                    static char buf[16];
                    sprintf(buf, "SPEED %d%%", demoSpeedPercent);
                    modeLabel = buf;
                } else {
                    modeLabel = "DEMO";
                }
                break;

            default:
                modeLabel = "";
                break;
        }
    }

    drawMainUI(modeLabel, showStandby);
    lastDisplayUpdateTime = now;
}

// ------------------------------------------------------------
// Flash a temporary status message
// ------------------------------------------------------------
void showStatusFlash(const char* text, int times, int ms) {
    for (int i = 0; i < times; i++) {
        display.clearDisplay();
        display.setRotation(0);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(20, 10);
        display.print(text);
        display.display();
        delay(ms);
        display.clearDisplay();
        display.display();
        delay(ms);
    }
    lastDisplayOnTime = millis();
    displayOn = true;
}