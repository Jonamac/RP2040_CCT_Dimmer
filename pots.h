#pragma once

int readADC(int pin);
void handlePots(unsigned long now);

// Called from ledmix.cpp when boot fade completes.
// Seeds IIR filters and step state to match the fade target,
// preventing a snap on the first handlePots() call after boot.
void syncPotsAfterBoot(float brightness, float cct);

// Resets the DUMB-specific IIR filter sentinels.
// Call when entering DUMB mode to force a fresh filter seed on next handlePots().
void resetDumbFilter();