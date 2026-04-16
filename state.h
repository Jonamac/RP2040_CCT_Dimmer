#pragma once
#include <Arduino.h>
#include "modes.h"

// ===============================
//  MODE STATE
// ===============================

// These are defined in state.cpp
extern Mode currentMode;
extern Mode previousMode;

// ===============================
//  DEMO MODE STATE
// ===============================

// Index of current DEMO phase (CCT cycling)
extern int demoPhaseIndex;

// Timestamp when current DEMO phase started
extern unsigned long demoPhaseStartTime;

// DEMO speed percentage (100, 75, 50, 25, 1)
extern int demoSpeedPercent;

// Timestamp until which DEMO speed flash is shown
extern unsigned long demoSpeedFlashUntil;

// True only for the FIRST frame after returning to DEMO from STANDBY
extern bool demoJustResumed;

// ===============================
//  STANDBY / DISPLAY STATE
// ===============================

extern bool displayOn;
extern unsigned long lastDisplayOnTime;
extern bool standbyFromDumbActive;
extern float standbyFromDumbStartB;
extern unsigned long standbyFromDumbStartTime;
extern bool dumbFadeActive;

extern bool normalFadeActive;
extern bool dumbFadeDirection; // true = up, false = down
extern float dumbFadeStartB;
extern float dumbFadeEndB;
extern unsigned long dumbFadeStartTime;
extern unsigned long dumbFadeDuration;
extern float lastDutyNorm;
extern float lastCCTNorm;
extern float lastMappedCCT;

// NORMAL fade engine (STANDBY ↔ NORMAL transitions + boot)
extern float         normalFadeStartB;
extern float         normalFadeEndB;
extern unsigned long normalFadeStartTime;
extern unsigned long normalFadeDuration;

// Boot fade in progress — freezes pots and defers systemInitialized
extern bool          bootFadeActive;

// ===============================
//  BUZZER STATE
// ===============================

extern bool buzzer_beep_enabled;
extern bool buzzer_click_enabled;

extern unsigned long buzzerQuietUntil;

// ===============================
//  SYSTEM INITIALIZATION
// ===============================

// False until the first LED frame is applied.
// Prevents the startup beep from firing too early.
extern bool systemInitialized;

// ===============================
//  TIMING CONSTANTS
// ===============================

extern unsigned long soft_start_ms;
extern unsigned long dumb_soft_start_ms;
extern unsigned long fade_time_ms;
extern unsigned long standby_fade_time_ms;
extern unsigned long dumb_standby_fade_time_ms;
extern unsigned long demo_mode_fade_ms;
extern unsigned long demo_mode_hold_time_ms;
extern unsigned long demo_mode_delay_ms;
extern unsigned long display_timeout_ms;
extern unsigned long led_update_delay_ms;

// ===============================
//  BUZZER CONSTANTS
// ===============================

extern float led_update_beep_freq;
extern int   led_update_beep_ms;
extern float mode_change_beep_freq;
extern int   mode_change_beep_ms;
extern int   knob_click_freq;
extern int   knob_click_ms;

// ===============================
// FREQ MODE
// ===============================

extern float freqStrobeHz;
extern float freqDutyCycle;
extern unsigned long freqCycleStartTime;
extern bool freqOnPhase;

// ===============================
//  CAL / PRESET STATE
// ===============================

extern float calPresets[5];
extern int   calPresetIndex;

// ===============================
//  LED ENGINE CONSTANTS
// ===============================

extern float min_duty;
extern float effective_off_threshold;
extern float gamma_val;

// ===============================
//  INTERNAL TIMESTAMPS
// ===============================

extern unsigned long lastInputChangeTime;
extern unsigned long lastLEDUpdateTime;
extern unsigned long lastDisplayUpdateTime;

// ===============================
//  BUTTON STATE
// ===============================

extern bool mainButtonLast;
extern unsigned long mainButtonPressTime;

extern bool dispButtonLast;
extern unsigned long dispButtonPressTime;

extern bool bothButtonsLast;
extern unsigned long bothButtonsPressTime;

// ===============================
//  DEMO INTERNAL FLAGS
// ===============================

extern bool demoIndicatorVisible;
extern bool demoCCTPotLatched;