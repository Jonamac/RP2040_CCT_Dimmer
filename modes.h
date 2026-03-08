#pragma once
#include <Arduino.h>

// =====================================================
//  MODE ENUMERATION
// =====================================================

enum Mode {
    MODE_NORMAL,
    MODE_STANDBY,
    MODE_OVERRIDE,
    MODE_OVERRIDE_PLUS,
    MODE_DEMO,
    MODE_DUMB
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