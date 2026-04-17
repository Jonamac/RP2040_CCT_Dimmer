# AI Notes — RP2040 CCT Dimmer

Long-term memory file for AI assistants working on this codebase.
Last updated: 2026-04-17 (boot CCT snap definitive fix)

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

## Key Constants (state.cpp)

- `min_duty = 0.0453f` — lowest stable PWM duty per channel (linear, pre-gamma). Hardware-calibrated. Do NOT change.
- `effective_off_threshold = 0.0011f` — gamma-domain cutoff. LEDs output zero below this.
- `gamma_val = 2.2f`
- `soft_start_ms = 1250` — NORMAL boot fade duration
- `dumb_soft_start_ms = 500` — DUMB boot fade duration
- `standby_fade_time_ms = 1500` — NORMAL STANDBY fade duration
- `dumb_standby_fade_time_ms = 500` — DUMB STANDBY fade duration

## NORMAL Mode

- Brightness: stepped via `normalBrightnessSteps[]` table (`NORMAL_STEPS` entries, defined in `calibration.cpp`)
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
2. Average 32 ADC samples for each pot; Serial.print raw averages for diagnostic
3. Compute `targetB = normalBrightnessSteps[idx]` and `startCCT`
4. `ledmix_set(0, startCCT)` + `applyLEDsImmediate(0, startCCT)` + `ledmix_initCurrent()`
5. `buzzerStartupBeep()` (ignores enable flags)
6. Start `normalFadeActive` from 0 → targetB
7. On fade complete: `systemInitialized = true`, `buzzer_click_enabled = true`, call `syncPotsAfterBoot()`, call `ledmix_initCurrent()` to sync `led_currentCCT`/`led_currentBrightness` to the post-boot targets, then call `applyLEDsImmediate(led_currentBrightness, led_currentCCT)` to re-render LEDs at the corrected CCT in the same frame — eliminates the startup CCT snap completely

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

- Pre-startup LED flash: LEDs sometimes flash briefly at power-on before the boot fade. This is a hardware glitch — PWM hardware may briefly output a non-zero value during the time between power-on and `initPins()`/`initPWM()`. **Mitigated:** `setup()` now drives `WARM_PIN` and `COOL_PIN` LOW as the very first lines (before `Serial.begin`, `initPins`, `initPWM`) using `pinMode` + `digitalWrite`. This sets the GPIO output low before PWM hardware takes over, minimising the flash window.
- Boot CCT snap: persistent issue across multiple PRs. **Definitive root cause:** Cold-start ADC settling causes the boot-time ADC read (32-sample avg in `setup()`) to return ~89 counts lower than the warm-hardware read 1250ms later. This results in a 1-step CCT difference (e.g. 4500K boot vs 4600K true pot position). When `syncPotsAfterBoot()` previously seeded `cctFiltered` from the stale boot-time CCT parameter (e.g. step 18 → `cctFiltered=0.4737`), the IIR filter would then slowly converge toward the real pot position (cctNorm≈0.4937) over ~20 frames. When `cctStepFloat` crossed `prevCCTStep + 0.5 + 0.20 = 18.70`, the Schmitt trigger fired → visible CCT snap seconds after boot. **Fix:** `delay(500)` before boot ADC reads in `setup()` (both NORMAL and DUMB paths) lets the ADC settle so boot read matches the warm read. `syncPotsAfterBoot()` now takes a fresh 8-sample ADC read, seeds `dutyFiltered`/`cctFiltered` from intermediate `dutyNorm`/`cctNorm` variables, and derives `prevDutyStep`/`prevCCTStep` from those SAME variables — guaranteeing filter and step state are always consistent. `ledmix_initCurrent()` + `applyLEDsImmediate()` are called immediately after in `ledmix.cpp` to render the corrected CCT in the same frame.
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
