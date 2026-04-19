#include "display_stub.h"
#include "pins.h"
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// Global display object (object model, same as main project)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void initDisplay() {
    Wire.setSDA(OLED_SDA);
    Wire.setSCL(OLED_SCL);
    Wire.begin();

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();
}

// Draw the analyzer UI (rotation matches main project)
void drawAnalyzerUI(
    bool buttonMode,
    float B_linear,
    float B_gamma,
    float cctNorm,
    float stepDuty
) {
    display.clearDisplay();
    display.setRotation(0);   // match main project
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.print("BRI: ");
    display.println(B_linear, 6);

    display.print("GAM: ");
    display.println(B_gamma, 6);

    display.print("CCT: ");
    display.println(cctNorm, 6);

    display.print("STEP: ");
    display.println(stepDuty, 6);

    display.display();

}