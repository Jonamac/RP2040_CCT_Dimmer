#include "state.h"
#include "modes.h"
#include "buzzer.h"
#include "inputs.h"
#include "pots.h"

int lastCCTADC = 0;
int lastDutyADC = 0;
int readADC(int pin) {
    return analogRead(pin);
}

// Forward declarations from buttons.cpp and pots.cpp
void processButtons(unsigned long now);
void processPots(unsigned long now);
void handleDumbSwitch(unsigned long now);

void readInputs(unsigned long now) {

    if (currentMode == MODE_NORMAL) {
        handlePots(now);
    }

    processButtons(now);
}