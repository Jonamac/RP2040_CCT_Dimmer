# RP2040 CCT Dimmer — State Machine Specification

This document defines the authoritative behavior of the firmware state machine.
All mode transitions, fade rules, and button semantics must follow this spec.

---

# 1. Modes Overview

The firmware has the following modes:

- **NORMAL**  
  Standard perceptual dimming + CCT mixing.  
  Uses brightness table + gamma 2.2.

- **DUMB**  
  Direct analog-style dimmer behavior.  
  No brightness table.  
  Gamma 2.2 applied.  
  Per‑channel min_duty rules apply.

- **STANDBY**  
  LEDs off (brightness = 0).  
  Entered from NORMAL or DUMB.  
  Exited back to the previous mode.

- **DEMO**  
  Automatic brightness/CCT cycling.  
  (Will be replaced later.)

- **FREQ (future)**  
  Strobe mode.

- **CAL (future)**  
  Diagnostics + calibration.

---

# 2. Mode Transition Rules

## 2.1 NORMAL → STANDBY
Triggered by MAIN short press.  
Fade down to 0 using NORMAL fade curve.

## 2.2 STANDBY → NORMAL
Triggered by MAIN short press.  
Fade up to brightness determined by pots.

## 2.3 DUMB → STANDBY
Triggered by MAIN short press.  
Fade down using DUMB fade curve.

## 2.4 STANDBY → DUMB
Triggered by MAIN short press **only if previousMode == MODE_DUMB**.  
Fade up using DUMB fade curve.

## 2.5 NORMAL ↔ DUMB (physical switch)
DUMB switch determines mode.  
Transitions must be debounced and ignored during fades.

---

# 3. Fade Engine Rules

## 3.1 NORMAL fade engine
- Fade down into STANDBY  
- Fade up out of STANDBY  
- Uses perceptual curve (gamma 2.2)  
- Uses brightness table for targetBrightness

## 3.2 DUMB fade engine
- Fade down into STANDBY  
- Fade up out of STANDBY  
- Uses linear brightness (gamma applied after)  
- Uses min_duty rules for per‑channel behavior

## 3.3 Boot fade
- Runs once at startup  
- LED engine and pot processing are frozen until complete  
- After fade: `currentBrightness = targetBrightness`

---

# 4. Brightness & CCT Rules

## 4.1 NORMAL brightness
- Knob → normalized 0–1  
- Mapped through brightness table  
- Gamma 2.2 applied  
- Step 0 = min_duty  
- Step 0 per‑channel behavior matches DUMB:
  - 2700K → warm = min_duty, cool = 0  
  - 6500K → cool = min_duty, warm = 0  
  - Mid CCT → both = min_duty

## 4.2 DUMB brightness
- Knob → normalized 0–1  
- Gamma 2.2 applied  
- min_duty enforced at bottom  
- Same per‑channel rules as above

## 4.3 CCT rules
- NORMAL: quantized to 100 K steps  
- DUMB: continuous  
- DUMB CCT changes apply immediately (no dependency on duty knob)

---

# 5. Display Rules

## 5.1 DUTY%
- Always shows `targetBrightness * 100`  
- Instant update (no fade)

## 5.2 mDUTY
Shown **only** when:

physicalDuty > 0
AND physicalDuty <= min_duty + epsilon

epsilon ≈ 0.0003

## 5.3 Mode label
- NORMAL, DUMB, STANDBY, DEMO, FREQ, CAL

---

# 6. Button Rules

## 6.1 MAIN button
- NORMAL: short → STANDBY  
- DUMB: short → toggle DUMB STANDBY  
- DEMO: short → STANDBY  
- Long press behavior reserved for future

## 6.2 DISP button
- NORMAL: short → toggle display  
- DUMB: short → toggle display  
- DEMO: long → exit demo

## 6.3 Dual-button
- Toggles buzzer enable/disable

---

# 7. DUMB Switch Rules

- Debounced (20–40 ms)  
- Startup lockout (300 ms)  
- Ignored during fades  
- Only triggers mode change on stable transitions

---

# 8. System Initialization

1. Boot fade runs  
2. LED engine frozen  
3. Pots frozen  
4. After fade:
   - `systemInitialized = true`
   - DUMB switch evaluated
   - Mode label drawn
