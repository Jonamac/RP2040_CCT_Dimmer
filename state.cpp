#include "state.h"

Mode currentMode = MODE_NORMAL;
Mode previousMode = MODE_NORMAL;
bool systemInitialized = false;
bool demoJustResumed = false;

// min_duty: lowest stable linear PWM duty (pre-gamma)
// effective_off_threshold: threshold in gamma domain
// gamma_val: perceptual correction exponent
float min_duty = 0.0453f;              // per-channel minimum duty (linear)
float effective_off_threshold = 0.0011f; // perceptual cutoff (gamma domain)
float effective_off_threshold_linear = 0.0010f; // Linear cutoff for OVERRIDE modes

float gamma_val = 2.2f;

float freqStrobeHz = 10.0f;          // default
float freqDutyCycle = 0.5f;          // 50% on-time
unsigned long freqCycleStartTime = 0;
bool freqOnPhase = true;

float calPresets[5] = {
  2700.0f,
  3800.0f,
  4600.0f,
  5000.0f,
  6500.0f
};
int calPresetIndex = 0;

unsigned long soft_start_ms          = 1250;
unsigned long dumb_soft_start_ms     = 500;
unsigned long fade_time_ms           = 600;
unsigned long standby_fade_time_ms   = 1500;
unsigned long dumb_standby_fade_time_ms = 500;
unsigned long demo_mode_fade_ms      = 3000;
unsigned long demo_mode_hold_time_ms = 300;
unsigned long demo_mode_delay_ms     = 1750;
unsigned long display_timeout_ms     = 10000;
unsigned long led_update_delay_ms    = 300;

bool buzzer_click_enabled = false;
bool buzzer_beep_enabled  = false;

unsigned long buzzerQuietUntil = 0;

float led_update_beep_freq = 2637.02045530296f; // E7
int   led_update_beep_ms   = 40;
float mode_change_beep_freq = 1975.533205024496f; // B6
int   mode_change_beep_ms   = 40;
int   knob_click_freq       = 4000;
int   knob_click_ms         = 1;

float currentBrightness = 0.0f;
float currentCCT        = 4600.0f;

float targetBrightness  = 0.0f;
float targetCCT         = 4600.0f;

unsigned long lastInputChangeTime   = 0;
unsigned long lastLEDUpdateTime     = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastDisplayOnTime     = 0;

bool displayOn            = true;
bool demoIndicatorVisible = true;

bool standbyFromDumbActive      = false;
float standbyFromDumbStartB     = 0.0f;
unsigned long standbyFromDumbStartTime = 0;
bool dumbFadeActive = false;
bool dumbFadeDirection = false;
float dumbFadeStartB = 0.0f;
float dumbFadeEndB = 0.0f;
unsigned long dumbFadeStartTime = 0;
unsigned long dumbFadeDuration = 0;
float lastDutyNorm = 0.0f;
float lastCCTNorm = 0.0f;
float lastMappedCCT = 4600.0f;

unsigned long demoPhaseStartTime = 0;
int demoPhaseIndex               = 0;

bool mainButtonLast        = false;
unsigned long mainButtonPressTime = 0;
bool dispButtonLast        = false;
unsigned long dispButtonPressTime = 0;

bool bothButtonsLast       = false;
unsigned long bothButtonsPressTime = 0;

bool demoCCTPotLatched = false;