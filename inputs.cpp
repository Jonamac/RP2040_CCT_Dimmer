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

// Forward declarations
void processButtons(unsigned long now);
void handleDumbSwitch(unsigned long now);

void readInputs(unsigned long now) {

    // DUMB switch must always be polled every loop
    handleDumbSwitch(now);

    // Pots must be sampled in both NORMAL and DUMB modes
    if (currentMode == MODE_NORMAL || currentMode == MODE_DUMB) {
        handlePots(now);
    }

    processButtons(now);
}