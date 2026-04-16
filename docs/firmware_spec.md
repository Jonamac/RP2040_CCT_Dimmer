# RP2040 CCT Dimmer — Firmware Architecture Specification

This document describes the architecture, responsibilities, and data flow of the
firmware. GitHub Copilot should use this as the authoritative reference.

---

# 1. File Responsibilities

## 1.1 RP2040_CCT_Dimmer.ino
- setup()  
- loop()  
- boot fade  
- calls readInputs(), updateLEDLogic(), updateDisplay()

## 1.2 modes.cpp/.h
- Mode state machine  
- Mode transitions  
- DUMB toggle logic  
- NORMAL standby logic  
- Fade engine state variables  
- Boot fade freeze logic  
- DUMB switch handling

## 1.3 pots.cpp/.h
- ADC sampling  
- Pot normalization  
- Step quantization (NORMAL)  
- Continuous mapping (DUMB)  
- CCT mapping  
- Pot change detection

## 1.4 ledmix.cpp/.h
- PWM mixing  
- Gamma correction  
- min_duty enforcement  
- Per‑channel CCT mixing  
- physicalDuty calculation

## 1.5 display_ui.cpp/.h
- OLED rendering  
- DUTY% display  
- mDUTY display  
- Mode label  
- CCT line  
- Layout logic

## 1.6 state.cpp/.h
- Global variables  
- Configuration constants  
- System flags  
- Buzzer state

## 1.7 freq_mode.cpp/.h (future)
- Frequency table  
- Strobe envelope  
- FREQ mode logic

---

# 2. Data Flow

loop()
├─ readInputs()
│   ├─ handleDumbSwitch()
│   ├─ processPots()
│   └─ processButtons()
├─ updateLEDLogic()
│   ├─ NORMAL fade engine
│   ├─ DUMB fade engine
│   └─ applyLEDsImmediate()
└─ updateDisplay()

---

# 3. Fade Engine Architecture

## 3.1 NORMAL fade engine
- Lives inside updateLEDLogic()  
- Interpolates currentBrightness → targetBrightness  
- Uses perceptual curve  
- Used for:
  - NORMAL → STANDBY  
  - STANDBY → NORMAL

## 3.2 DUMB fade engine
- Lives inside updateLEDLogic()  
- Interpolates linearly  
- Used for:
  - DUMB → STANDBY  
  - STANDBY → DUMB

## 3.3 Boot fade
- Runs in setup()  
- LED engine frozen until complete

---

# 4. Brightness Table

`normalBrightnessSteps[]` is defined in `brightness_table.h`.

Rules:
- Step 0 = min_duty  
- Step N = 1.0  
- Gamma 2.2 applied  
- Midpoint corresponds to knob midpoint  
- Perceptual spacing

---

# 5. DUMB Mode Architecture

- No brightness table  
- Gamma 2.2 applied  
- Per‑channel min_duty rules  
- CCT continuous  
- CCT changes apply immediately  
- DUMB fade engine handles standby transitions  
- DUMB mode never calls handleMainButtonRelease()

---

# 6. Startup Behavior

- Boot fade runs  
- LED engine frozen  
- Pots frozen  
- DUMB switch ignored for 300 ms  
- After fade:
  - Evaluate DUMB switch  
  - Set mode  
  - Draw display  
  - Enable LED engine

---

# 7. Buzzer Behavior

- Click/beep on mode changes  
- Quiet window after beep  
- Dual-button toggle  
- DUMB mode must not disable buzzer  
