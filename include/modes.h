#ifndef MODES_H
#define MODES_H

#include <Arduino.h>

// =====================================================
//  MODE ENUMERATION
// =====================================================

enum Mode {
    MODE_NORMAL,        // 0
    MODE_DUMB,          // 1
    MODE_DEMO,          // 2
    MODE_FREQ,          // 3  (formerly MODE_OVERRIDE)
    MODE_CAL,           // 4  (formerly MODE_OVERRIDE_PLUS)
    MODE_STANDBY        // 5
};

// =====================================================
//  MODE SYSTEM API
// =====================================================

// Initialize mode system (called from setup)
void initModes();

// Called every loop to update mode transitions (mostly unused)
void updateModeState(unsigned long now);

// Called every loop to update mode behavior (DEMO fade, STANDBY fade, etc.)
void updateModeBehavior(unsigned long now);

// =====================================================
//  BUTTON HANDLERS
// =====================================================

// MAIN BUTTON — short press
void handleMainButtonRelease(unsigned long heldMs, unsigned long now);

// DUMB BUTTON — short press (standby toggle)
void handleDumbToggle(unsigned long now);

// DISPLAY BUTTON — short press
void handleDispButtonRelease(unsigned long heldMs);

// MAIN BUTTON — long press
void handleMainLongPress();

// MAIN BUTTON — short-long combo
void handleMainShortLongCombo();

// DISPLAY BUTTON — long press (buzzer mode cycle)
void handleDispLongPress();

// DISPLAY BUTTON — short-long combo (DEMO toggle)
void handleDispShortLongCombo();

#endif // MODES_H
