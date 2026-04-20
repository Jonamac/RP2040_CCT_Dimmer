# AI Notes — RP2040 CCT Dimmer

Long-term memory file for AI assistants working on this codebase.
Last updated: 2026-04-17

---

## Hardware

- MCU: RP2040 (Raspberry Pi Pico-compatible)
- LED channels: Warm white + Cool white, controlled via PWM
- Pots: Two 12-bit ADC pots — DUTY (brightness) and CCT (colour temperature)
- Buzzer: Piezo, controlled via `tone()`
- Display: SSD1306 128×32 OLED (I2C)
- DUMB switch: Physical toggle (INPUT_PULLDOWN), switches between NORMAL and DUMB operating modes

## Calibration (calibration.h / calibration.cpp)

- `DUTY_MIN_RAW = 15`, `DUTY_MAX_RAW = 4095` — pots reach ADC endpoints cleanly
- `CCT_MIN_RAW  = 15`, `CCT_MAX_RAW  = 4095`
- **Note:** physical pots may not mechanically reach ADC=4095 at the clockwise stop. This can cause the measured center to fall below the electrical center (e.g., CCT pot centre reads ~4400K instead of 4600K). Endpoint snap zones in DUMB mode compensate for the duty pot extremes; CAL mode is the proper fix for CCT calibration offset.

## min_duty Calibration Procedure

`min_duty` MUST be set by hardware measurement using the LED Driver Analyzer sketch (`RP2040_CCT_Dimmer-LED_DRIVER_ANALYZER/`). Do NOT guess or use a theoretical value.

**Procedure:**
1. Open `RP2040_CCT_Dimmer-LED_DRIVER_ANALYZER/RP2040_CCT_Dimmer-LED_DRIVER_ANALYZER.ino` in Arduino IDE. All supporting `.cpp`/`.h` files are in the same folder and compile automatically. Do NOT use PlatformIO for this sketch.
2. Flash to device
3. Ensure DUMB switch is OFF (linear mode — no gamma applied)
4. Both channels start. Warm (W) is selected by default; both-button short press toggles channel
5. Step DOWN from a high value using DOWN button (DISP) until the LED just extinguishes or flickers
6. Step UP one count at a time (UP = MAIN button) until the LED is stable
7. Record the `RAW` count shown on the OLED
8. `min_duty = RAW / 4095.0f`
9. Update `float min_duty` in `src/state.cpp`

**OLED display layout:** `RAW:<count>  D:<float>` / `<duty%>  <ns>ns` / `RNG:<range>  LIN/GAM  W/C  POT/BTN`

**Button map in analyzer:** MAIN = step up (+1/+10/+50 counts on tap/long/very-long), DISP = step down. Both buttons together: release <2s = cycle PWM range forward; release ≥2s = cycle backward. Keep PWM range at 4095 for calibration.

**Channel selection:** CCT pot (right knob) selects channel live — left half = Warm (W), right half = Cool (C). No button combo needed.

**Pot vs Button mode:** Display shows POT (duty pot is live) or BTN (duty pot frozen). First tap of either button permanently freezes the duty pot for that session; only reboot restores pot control. Use the pot for coarse positioning, then tap a button to lock it in and fine-tune.

**Pot control:** Duty pot (left knob) acts as coarse control in POT mode. Moving the pot by >8 ADC counts overrides stepDuty immediately. Sketch seeds stepDuty from the pot at boot.

**Known pitfall:** Buttons are TTP223 active-high touch sensors; sketch uses INPUT mode (no internal pull needed). INPUT_PULLUP holds the line permanently HIGH, freezing the loop — do not use INPUT_PULLUP.

**Note:** At 25 kHz with range 4095, each RAW count = 40000ns/4095 ≈ 9.77 ns of on-time.



- `min_duty = 0.000244f` — lowest stable PWM duty per channel (linear, pre-gamma). Hardware-calibrated on 2026-04-19. RAW:1 at RNG:4095, 25kHz. Scope-measured ~38ns actual pulse (hardware clock quantization rounds up from theoretical 9.77ns). Both channels identical. Do NOT change without re-running the LED Driver Analyzer procedure.
- `effective_off_threshold = 0.0011f` — **RETIRED**. Was a gamma-domain software guard; now obsolete since `min_duty` is hardware-calibrated. Sub-min_duty values quantize to RAW:0 via `setWarmDuty/setCoolDuty` automatically. Variable kept in state.cpp/state.h but not used by `applyLEDsImmediate`.
- `gamma_val = 2.2f`
- `soft_start_ms = 1250` — NORMAL boot fade duration
- `dumb_soft_start_ms = 500` — DUMB boot fade duration
- `standby_fade_time_ms = 1500` — NORMAL STANDBY fade duration
- `dumb_standby_fade_time_ms = 500` — DUMB STANDBY fade duration

## NORMAL Mode

- **PERMANENT RULE — NORMAL Step 0 = min_duty, NOT 0.0f.** STANDBY handles off. Step 0 is the hardware minimum (LEDs dimly on, mDUTY label). Do NOT set step 0 to 0.0f — this has been confirmed multiple times.
- Brightness: stepped via `normalBrightnessSteps[]` table (`NORMAL_STEPS` = 22 entries, defined in `calibration.cpp`)
- Table is pre-gamma raw PWM duties. Formula: `steps[i] = min_duty + pow(i/21.0, 2.2) * (1 - min_duty)`. `applyLEDsImmediate()` does NOT apply gamma for NORMAL.
- Step 0 = `min_duty` (0.000244f): pot fully down → hardware minimum, OLED `0.01%` + `mDUTY` label
- Step 11 ≈ 0.2408: perceptually 50% (12 o'clock); OLED shows `52%` (cosmetic)
- Step 21 = 1.0: full brightness; OLED shows `100%`
- OLED uses `normalDisplayPercent[]` cosmetic table: step 0 = 0.01%, steps 1–21 = 5%,10%,14%,19%,24%,29%,33%,38%,43%,48%,52%,57%,62%,67%,71%,76%,81%,86%,90%,95%,100% (indexed by `pots_getNormalDutyStep()`)
- NORMAL mixing floor: when `B_linear ≤ min_duty` both active channels scale 0→min_duty (mirrors DUMB fade-region). Prevents step 0 from being silenced by the old `reserved = 2*min_duty` logic.
- CCT: 39 steps, 2700K–6500K, 100K per step
- Pot changes use Schmitt trigger hysteresis (±0.20 step dead-band per boundary) to prevent oscillation
- IIR filter α=0.10 (settled) / α=0.60 (moving, delta > 0.025) on raw ADC before step logic
- Buzzer clicks on each step change (duty and CCT), suppressed on first-call commit
- STANDBY fade uses `normalFadeActive` engine in `ledmix.cpp`
- STANDBY→NORMAL fade starts at `constrain(min_duty, 0, newB)` to avoid flash at step 0

## DUMB Mode

- Brightness: continuous, `min_duty + dutyNorm * (1 - min_duty)`, applied immediately
- CCT: continuous 2700K–6500K, applied immediately
- Gamma IS applied in DUMB (`applyLEDsImmediate` includes `MODE_DUMB` in `useGamma`); dedicated adaptive IIR filter (α=0.05 settled / α=0.40 moving, threshold 0.005, applied to RAW ADC normalized values) + dead-band (0.005 brightness, 10K CCT)
- No buzzer clicks or beeps in DUMB (except `buzzerStartupBeep` at NORMAL boot, which ignores flags)
- DUMB display: pot-normalized duty — `(rawB - min_duty) / (1 - min_duty) * 100`, floored at 0.01%. Shows 0.01% at pot-minimum, 100% at pot-maximum. `rawB = ledmix_getBrightness()` equals `min_duty` exactly when `dumbDutyFiltered` snaps to 0.
- mDUTY indicator in DUMB: shown when `ledmix_getBrightness()` is within ±0.001 of `min_duty` (i.e., pot is at minimum). In NORMAL/DEMO/etc: shown when `currentWarmDuty == min_duty || currentCoolDuty == min_duty`.
- DUMB CCT display: 10K resolution (rounded to nearest 10K) for continuous movement feel; NORMAL mode uses 100K resolution
- DUMB duty endpoint snap zones: `dumbDutyFiltered < 0.015` → clamp to 0; `dumbDutyFiltered > 0.985` → clamp to 1. Ensures pots that don't reach full ADC range can still hit 0% and 100%. (was 0.03/0.97 — too wide at low end, reduced fine control)
- DUMB CCT endpoint snap zones: `dumbCCTFiltered < 0.01` → clamp to 0.0 (2700K); `dumbCCTFiltered > 0.99` → clamp to 1.0 (6500K). Without these, `dumbCCTFiltered` approaches 0/1 asymptotically and `mix` in `applyLEDsImmediate` never equals exactly 0.0f or 1.0f. The `mix==0.0f` (warm=min_duty, cool=off) and `mix==1.0f` (cool=min_duty, warm=off) branches are ONLY reachable with these snap zones.
- DUMB CCT center snap zone: if computed CCT is within ±75K of 4600K, snap to 4600K exactly. Makes neutral/center position easy to find and hold.
- DUMB maximum: constrain `newB` to `[min_duty, 1.0]`
- DUMB STANDBY uses `dumbFadeActive` engine

## Mode Transitions

- NORMAL→STANDBY: `normalFadeActive` fade down, buzzer mode-change beep
- STANDBY→NORMAL: `normalFadeActive` fade up, buzzer mode-change beep
- DUMB→STANDBY: `dumbFadeActive` fade down, NO buzzer
- STANDBY→DUMB: `dumbFadeActive` fade up, NO buzzer
- DUMB→NORMAL (switch flip): 8-sample ADC average → NORMAL step; `syncPotsAfterBoot()` immediately; fade up via `normalFadeActive` (fade_time_ms=600ms); re-enable `buzzer_beep_enabled` and `buzzer_click_enabled`
- NORMAL→DUMB (switch flip): cancel `normalFadeActive`; `resetDumbFilter()`; 8-sample ADC average; immediate apply; pot state takes over instantly
- DUMB switch during STANDBY: update `previousMode` to reflect switch position; exit goes to correct mode

## Boot Sequence

### NORMAL boot
1. `delay(500)` to let ADC input capacitance and voltage reference settle after power-on
2. Read DUTY ADC (32 samples) — brightness step doesn't require ADC settling
3. Compute `targetB = normalBrightnessSteps[idx]`
4. `ledmix_set(0, 4600.0f)` + `applyLEDsImmediate(0, 4600.0f)` + `ledmix_initCurrent()` — LEDs off, placeholder CCT (no visual effect at brightness 0)
5. `buzzerStartupBeep()` (blocking, ~200–400ms) — this free settling time lets the RP2040 ADC input capacitor and voltage reference reach operating temperature
6. Read CCT ADC (32 samples) **post-beep** — now settled; produces the same value that `syncPotsAfterBoot()` will read at fade-end
7. Compute `startCCT`; `ledmix_set(0, startCCT)` + `ledmix_initCurrent()` — update with settled CCT
8. Start `normalFadeActive` from 0 → targetB
9. On fade complete: `systemInitialized = true`, `buzzer_click_enabled = true`, call `syncPotsAfterBoot()`, call `ledmix_initCurrent()` to sync `led_currentCCT`/`led_currentBrightness` to the post-boot targets, then call `applyLEDsImmediate(led_currentBrightness, led_currentCCT)` to re-render LEDs at the corrected CCT in the same frame

### DUMB boot
1. `delay(500)` to let ADC input capacitance and voltage reference settle after power-on
2. Average 32 ADC samples for each pot; Serial.print raw averages for diagnostic
3. Compute `endB` and `startCCT`
4. `ledmix_set(0, startCCT)` + `applyLEDsImmediate(0, startCCT)` + `ledmix_initCurrent()`
5. No buzzer
6. Start `dumbFadeActive` from 0 → endB
7. On fade complete: `systemInitialized = true`

## Pot Filter Architecture (pots.cpp)

- Shared IIR filter: `dutyFiltered`, `cctFiltered`, adaptive α (0.10 settled / 0.60 moving, threshold delta > 0.025) — used for NORMAL mode step logic
- DUMB-specific IIR: `dumbDutyFiltered`, `dumbCCTFiltered`, adaptive α (**0.05 settled** / 0.40 moving, threshold delta > 0.005) — applied to **raw (pre-shared-IIR) ADC normalized values**, NOT to shared filter output
- All four filters are **file-scope statics** in `pots.cpp` — NOT static locals inside `handlePots()`
- `rawDutyNorm` / `rawCCTNorm`: local variables saved immediately after `constrain` in `handlePots()`, before the shared IIR block overwrites `dutyNorm`/`cctNorm` — these are the DUMB IIR inputs
- `syncPotsAfterBoot(brightness, cct)`: called when NORMAL boot fade completes. Takes a fresh 8-sample ADC average, computes `dutyNorm`/`cctNorm`, seeds `dutyFiltered`/`cctFiltered` from those values, then **re-derives `prevDutyStep`/`prevCCTStep` from those same `dutyNorm`/`cctNorm` variables**. This invariant — filter and step indices derived from one ADC sample via the same intermediate normalized values — ensures the first `handlePots()` call sees delta ≈ 0 → slow alpha → Schmitt trigger cannot fire immediately → no boot snap. Then calls `ledmix_set(currentBrightness, currentCCT)` to update ledmix targets before returning.
- `resetDumbFilter()`: resets `dumbDutyFiltered` and `dumbCCTFiltered` to -1 sentinel; called on NORMAL→DUMB switch
- `prevDutyStep`, `prevCCTStep` initialised to -1 (sentinel for first-call)

## Known Issues / History

- DUMB mode gamma crushes min_duty: In DUMB mode, the generic gamma scale `B_perceptual / B_linear` was applied to channel duties even in the `B_linear <= min_duty` branch. At pot-minimum (B_linear = min_duty = 0.0453), scale = pow(0.0453, 2.2)/0.0453 ≈ 0.025, so physical duty = 0.0453 × 0.025 ≈ 0.00114 — far below the hardware minimum stable duty. This caused: (a) lights brighter/more unstable than expected at minimum (0.00114% duty is below stable threshold), (b) standby fade from min_duty imperceptible (almost nothing to fade from 0.00114 to 0), (c) boot fade appeared to jump on because the sub-min_duty linear region produced near-zero physical output throughout. **Fixed:** DUMB section in `applyLEDsImmediate` now computes physical channel duty directly and returns early (bypassing the generic gamma scale). For B_linear ≤ min_duty (fade region only): `duty = min_duty × (B_linear / min_duty)` — linear from 0 to min_duty, giving a visible smooth fade. For B_linear > min_duty: `norm = (B_linear - min_duty) / (1 - min_duty)`, `B_phys = min_duty + pow(norm, gamma_val) × (1 - min_duty)` — gamma applied to the normalised range above min_duty. Result: pot-minimum → physical duty = min_duty (hardware minimum, stable & dim), pot-maximum → 1.0. Standby fade from min_duty now produces a visible linear ramp from 0.0453 → 0.



- Pre-startup LED flash: LEDs sometimes flash briefly at power-on before the boot fade. This is a hardware glitch — PWM hardware may briefly output a non-zero value during the time between power-on and `initPins()`/`initPWM()`. **Mitigated:** `setup()` now drives `WARM_PIN` and `COOL_PIN` LOW as the very first lines (before `Serial.begin`, `initPins`, `initPWM`) using `pinMode` + `digitalWrite`. This sets the GPIO output low before PWM hardware takes over, minimising the flash window.
- Boot CCT snap: persistent issue across multiple PRs. **Definitive root cause:** Cold-start ADC input capacitor charging causes the CCT ADC read taken immediately after power-on to underread by ~89 counts (~1 step, e.g. 4500K instead of 4600K). **Fix:** DUTY ADC is read first (doesn't need settling), then `buzzerStartupBeep()` fires (blocking ~200–400ms — free settling time), then CCT ADC is read post-beep. This settled post-beep read matches the value `syncPotsAfterBoot()` takes at fade-end, so `startCCT` equals the true pot position → no step mismatch → no Schmitt trigger snap. In addition, `syncPotsAfterBoot()` seeds `cctFiltered`/`prevCCTStep` from a fresh 8-sample ADC read (not from the boot CCT parameter), and `ledmix_initCurrent()` + `applyLEDsImmediate()` are called immediately after in `ledmix.cpp` to render at the corrected CCT in the same frame. **If boot CCT snap persists after this fix**, the root cause is a hardware ADC transient at power-on that software cannot fully compensate; this is cosmetic only (CCT shifts by 100K at end of startup fade if pot is between steps) and can be accepted until CAL mode is implemented.
- Persistent 200K CCT offset at boot (CCT pot at center shows 4400K instead of 4600K): suspected hardware calibration issue.
  - If the pot's physical max stop does not reach ADC=4095 (e.g. max ≈ 3660 counts), then the electrical centre falls at ~1830 counts → normalizes to ~0.447 → 4400K.
  - Boot delay increased to 500ms and sample count increased to 32 to rule out ADC settling.
  - Serial debug prints raw ADC values at boot (both paths in `RP2040_CCT_Dimmer.ino`) and in `syncPotsAfterBoot()` so the actual hardware reading can be measured.
  - Proper fix deferred to CAL mode (will allow measuring `CCT_MAX_RAW` in-situ).
- DUMB gamma bug: `applyLEDsImmediate` was applying gamma to DUMB mode (`useGamma = !(FREQ || CAL)` did not exclude DUMB). At `min_duty=0.0453`, gamma gave `0.0453^2.2 = 0.00114 = 0.11%` display minimum instead of 0.01%, and max ≈ 52.5% instead of 100%. **Fixed:** `MODE_DUMB` is no longer excluded from `useGamma` — gamma is correctly restored in DUMB. Display formula changed to pot-normalized `(rawB - min_duty)/(1 - min_duty) * 100` (0.01%–100%) so at pot-minimum `rawB=min_duty` → 0.01%, at pot-maximum → 100%. The `effective_off_threshold` guard already excludes DUMB mode so LEDs won't be cut off at minimum.
- DUMB mode filter lag: old implementation was a cascade double-filter — shared adaptive IIR (α=0.10/0.60) followed by a second fixed-α IIR (α=0.05) on the already-filtered output. Combined lag was severe. **Fixed:** DUMB IIR now operates on raw (pre-shared-IIR) ADC normalized values, with adaptive α=0.40 (moving, delta > 0.005) / α=0.10 (settled). Feels analog-snappy when moving, suppresses noise when settled.
- DUMB idle jitter: with α=0.10 settled and RP2040 ADC noise of ~10–20 LSB, filtered output could still cross old dead-band thresholds. **Fixed:** widened to `DUMB_BRIGHTNESS_DB = 0.005` and `DUMB_CCT_DB = 10.0K`.
- DUMB min: pots are calibrated to reach 0 cleanly; snap zones should NOT be added.
- Step oscillation: old `|x - N| > 0.05` hysteresis caused oscillation at boundaries. Replaced with Schmitt trigger (±0.20 dead-band). Also increased shared IIR moving threshold from 0.01 to 0.025 (≈102 ADC counts, well above RP2040 ADC noise floor of 30–50 LSB) to prevent fast-alpha from being triggered by ADC noise in NORMAL mode idle.

## Files

- `RP2040_CCT_Dimmer.ino` — setup + loop
- `state.cpp` / `state.h` — all global state and constants
- `pots.cpp` / `pots.h` — pot ADC, IIR filter, step logic, DUMB continuous logic
- `ledmix.cpp` / `ledmix.h` — LED engine: gamma, mixing, fade engines, immediate apply
- `modes.cpp` / `modes.h` — mode state machine, button handlers, DUMB switch handler
- `buzzer.cpp` / `buzzer.h` — buzzer tones and guards
- `displayui.cpp` / `display_ui.h` — OLED display logic
- `calibration.cpp` / `calibration.h` — ADC calibration, brightness table
- `brightness_table.h` / `brightness_table.cpp` — perceptual brightness lookup
- `pins.h` — pin assignments
- `pwm_control.h` / `.cpp` — PWM output
- `inputs.h` / `.cpp` — button reading
- `timing.h` / `.cpp` — timing init
- `freq_mode.h` / `.cpp` — FREQ strobe mode
- `docs/AI_NOTES.md` — this file

## PR Plan (Original + Status)

### Master Bug Register (original)

| # | Severity | File(s) | Issue |
|---|---|---|---|
| 1 | 🔴 Critical | inputs.cpp | handleDumbSwitch() never called from readInputs() |
| 2 | 🔴 Critical | inputs.cpp | handlePots() never called in DUMB mode |
| 3 | 🔴 Critical | RP2040_CCT_Dimmer.ino | DUMB boot path never set currentMode = MODE_DUMB |
| 4 | 🔴 Critical | RP2040_CCT_Dimmer.ino + ledmix.cpp | DUMB fade engine never ran |
| 5 | 🔴 Critical | displayui.cpp | DUTY_PHYSICAL_MIN/MAX undefined — compile error |
| 6 | 🟠 High | modes.cpp | STANDBY→NORMAL snapped instead of fading |
| 7 | 🟠 High | modes.cpp | handleDumbSwitch() had no debounce/startup lockout/fade guard |
| 8 | 🟠 High | inputs.cpp + pots.cpp | DUMB CCT never applied immediately |
| 9 | 🟡 Medium | buzzer.cpp | buzzerModeChangeBeep() silenced in DUMB — DUMB→STANDBY beep broken |
| 10 | 🟡 Medium | buzzer.cpp | buzzerQuietUntil never checked |
| 11 | 🟡 Medium | calibration.cpp | normalBrightnessSteps[0] = 0.00, should be min_duty |
| 12 | 🟡 Medium | displayui.cpp | DUTY% computed from physical PWM — should be targetBrightness * 100 |
| 13 | 🟡 Medium | displayui.cpp | mDUTY uses exact equality — must use <= min_duty + epsilon |
| 14 | 🟢 Low | modes.cpp | handleDumbToggle() doesn't exist — DUMB button logic not isolated |
| 15 | 🟢 Low | modes.cpp | Dead code: second MODE_NORMAL branch in handleMainLongPress() |
| 16 | 🟢 Low | modes.cpp | Dead code: lines 344–348 in handleDispButtonRelease() |
| 17 | 🟢 Low | inputs.cpp | processPots() forward-declared but never defined or called |

### PR Status

| PR | Title | Status | Notes |
|---|---|---|---|
| PR 1 | readInputs() Foundation + DUMB Pots/Switch Wiring (bugs 1, 2, 8) | ✅ Done | Merged |
| PR 2 | DUMB Switch Debounce + Startup Lockout (bug 7) | ✅ Done | Merged |
| PR 3 | Boot Fix + Loop Guard + NORMAL Fade Engine (bugs 3, 4, 6) | ✅ Done | Merged + many hotfixes |
| Hotfixes | Boot CCT snap, DUMB filter lag, display bugs, transitions | ✅ Done | PRs #4–#12 merged |
| Boot CCT snap | CCT snaps 100K at end of startup fade | ⚠️ Parked | Persists after 8+ fix attempts; cosmetic; revisit in CAL mode era |
| PR 4 | handleDumbToggle() Extraction (bug 14) | ✅ Done | |
| PR 5 | Display Fixes: DUTY%, mDUTY (bugs 12, 13) | 📋 Planned | Some fixes already applied via hotfixes; verify/complete |
| PR 6 | Brightness Table Rebuild (bug 11) | ✅ Done | Step 0 = min_duty (PERMANENT — never 0.0f). Pre-gamma `steps[i]=min_duty+pow(i/21.0,2.2)*(1-min_duty)`. Cosmetic `normalDisplayPercent[]`. NORMAL mixing floor: B≤min_duty → both channels scale 0→min_duty (mirrors DUMB fade-region). DUMB→NORMAL fade startB fixed to `currentWarmDuty+currentCoolDuty` (physical output, not pre-gamma). Buzzer flags cleared in STANDBY dumb-switch path; DISP toggle silent from DUMB STANDBY. |
| PR 7 | Buzzer Fixes + Code Cleanup (bugs 9, 10, 15, 16, 17) | 📋 Planned | |

### PR 4 Detail — handleDumbToggle() Extraction
- Extract DUMB-specific standby toggle block from `handleMainButtonRelease()` into `handleDumbToggle(unsigned long now)`
- Declare in `modes.h`
- Call from DUMB short-press path instead of routing through handleMainButtonRelease

### PR 5 Detail — Display Fixes
- DUTY%: `ledmix_getBrightness() * 100.0f` (= targetBrightness). Add `ledmix_getTargetBrightness()` getter if needed.
- mDUTY: `(currentWarmDuty > 0 && currentWarmDuty <= min_duty + 0.0003f) || (currentCoolDuty > 0 && currentCoolDuty <= min_duty + 0.0003f)`
- Note: Many display fixes already landed in hotfixes (DUMB DUTY pot-normalized, CCT 10K resolution, mode label position, STANDBY badge position, mDUTY position). Verify these are present before adding.

### PR 6 Detail — Brightness Table Rebuild
- Rebuild `normalBrightnessSteps[22]` as piecewise linear:
  - Step 0 = min_duty (0.0453) — minimum stable PWM (not 0.00)
  - Steps 0→11: linear from min_duty → 0.50
  - Steps 11→21: linear from 0.50 → 1.0
  - Step 11 = 0.50 exactly (knob at 12 o'clock → DUTY = 50.00%)
  - Perceptual uniformity from applyGamma() in ledmix.cpp

### PR 7 Detail — Buzzer Fixes + Cleanup
- Remove `if (currentMode == MODE_DUMB) return` from `buzzerModeChangeBeep()` only (keep in buzzerClick())
- Add `buzzerQuietUntil` check to `buzzerModeChangeBeep()` and `buzzerLEDUpdateBeep()`
- Remove dead second MODE_NORMAL branch in `handleMainLongPress()`
- Remove unreachable block in `handleDispButtonRelease()`
- Remove `processPots()` forward declaration from `inputs.cpp`

### Known Issues (Parked)
- **Boot CCT snap**: CCT snaps 100K at end of startup fade in NORMAL mode. Cosmetic (1 step, 100K). Root cause: RP2040 ADC cold-start reading is consistently 1 step lower than warm reading; by fade-end the pot reads correctly and triggers a Schmitt step. Multiple fix attempts failed. Revisit when CAL mode is implemented (CAL mode will let user set true ADC endpoints and may eliminate the mismatch).
- **Cool white channel PWM ~5ns shorter than warm white**: PCB trace length asymmetry between the two GPIO pins. Cannot be fixed in software at this precision.
