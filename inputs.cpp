#include "inputs.h"

int lastCCTADC = 0;
int lastDutyADC = 0;

// Forward declarations from buttons.cpp and pots.cpp
void processButtons(unsigned long now);
void processPots(unsigned long now);
void handleDumbSwitch(unsigned long now);

void readInputs(unsigned long now) {
    handleDumbSwitch(now);   // <-- MUST run first
    processPots(now);        // pots depend on mode
    processButtons(now);     // buttons depend on mode
}
