#ifndef POTS_H
#define POTS_H

int readADC(int pin);
void handlePots(unsigned long now);

// Called from ledmix.cpp when boot fade completes.
// Seeds IIR filters and step state to match the fade target,
// preventing a snap on the first handlePots() call after boot.
void syncPotsAfterBoot(float brightness, float cct);

// Resets the DUMB-specific IIR filter sentinels.
// Call when entering DUMB mode to force a fresh filter seed on next handlePots().
void resetDumbFilter();

// Returns the current NORMAL mode duty step index (0–NORMAL_STEPS-1).
// Used by displayui.cpp to look up the cosmetic display percentage.
int pots_getNormalDutyStep();

#endif // POTS_H