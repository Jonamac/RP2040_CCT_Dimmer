#pragma once

int readADC(int pin);
void handlePots(unsigned long now);

// Called from setup() to seed the IIR filter and step state from boot-time pot readings.
// Prevents the first handlePots() call from snapping to a different step than what
// setup() computed for the boot fade target.
void initPotState(int dutyStep, int cctStep, float dutyNorm, float cctNorm);