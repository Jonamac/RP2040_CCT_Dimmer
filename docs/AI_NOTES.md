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
- **Do not add snap zones** that collapse ranges, pots are already calibrated

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
- Pot changes use Schmitt trigger hysteresis (±0.15 step dead-band per boundary) to prevent oscillation
- IIR filter α=0.10 (settled) / α=0.60 (moving, delta > 0.01) on raw ADC before step logic
- Buzzer clicks on each step change (duty and CCT), suppressed on first-call commit
- STANDBY fade uses `normalFadeActive` engine in `ledmix.cpp`
- STANDBY→NORMAL fade starts at `constrain(min_duty, 0, newB)` to avoid flash at step 0

## DUMB Mode

- Brightness: continuous, `min_duty + dutyNorm * (1 - min_duty)`, applied immediately
- CCT: continuous 2700K–6500K, applied immediately
- No gamma in DUMB (`applyLEDsImmediate` excludes `MODE_DUMB` from `useGamma`); dedicated adaptive IIR filter (α=0.10 settled / α=0.40 moving, threshold 0.005, applied to RAW ADC normalized values) + dead-band (0.005 brightness, 10K CCT)
- No buzzer clicks or beeps in DUMB (except `buzzerStartupBeep` at NORMAL boot, which ignores flags)
- DUMB display: pot-normalized percentage — `(rawB - min_duty) / (1 - min_duty) * 100`, floored at 0.01%. Maps min-pot → 0.01%, max-pot → 100%. `rawB = ledmix_getBrightness()` = `led_currentBrightness` = `newB` in DUMB mode.
- DUMB maximum: constrain `newB` to `[min_duty, 1.0]` — do NOT add snap zones (pots are calibrated)
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
1. `delay(100)` to let ADC input capacitance and voltage reference settle after power-on
2. Average 16 ADC samples for each pot
3. Compute `targetB = normalBrightnessSteps[idx]` and `startCCT`
4. `ledmix_set(0, startCCT)` + `applyLEDsImmediate(0, startCCT)` + `ledmix_initCurrent()`
5. `buzzerStartupBeep()` (ignores enable flags)
6. Start `normalFadeActive` from 0 → targetB
7. On fade complete: `systemInitialized = true`, `buzzer_click_enabled = true`, call `syncPotsAfterBoot()`

### DUMB boot
1. `delay(100)` to let ADC input capacitance and voltage reference settle after power-on
2. Average 16 ADC samples for each pot
3. Compute `endB` and `startCCT`
4. `ledmix_set(0, startCCT)` + `applyLEDsImmediate(0, startCCT)` + `ledmix_initCurrent()`
5. No buzzer
6. Start `dumbFadeActive` from 0 → endB
7. On fade complete: `systemInitialized = true`

## Pot Filter Architecture (pots.cpp)

- Shared IIR filter: `dutyFiltered`, `cctFiltered`, adaptive α (0.10 settled / 0.60 moving, threshold delta > 0.01) — used for NORMAL mode step logic
- DUMB-specific IIR: `dumbDutyFiltered`, `dumbCCTFiltered`, adaptive α (0.10 settled / 0.40 moving, threshold delta > 0.005) — applied to **raw (pre-shared-IIR) ADC normalized values**, NOT to shared filter output
- All four filters are **file-scope statics** in `pots.cpp` — NOT static locals inside `handlePots()`
- `rawDutyNorm` / `rawCCTNorm`: local variables saved immediately after `constrain` in `handlePots()`, before the shared IIR block overwrites `dutyNorm`/`cctNorm` — these are the DUMB IIR inputs
- `syncPotsAfterBoot(brightness, cct)`: called when NORMAL boot fade completes. Takes a fresh 8-sample ADC average, seeds `dutyFiltered`/`cctFiltered` from it, then **re-derives `prevDutyStep`/`prevCCTStep` from those same normalized values**. This invariant — filter and step indices derived from one ADC sample — ensures the first `handlePots()` call sees delta ≈ 0 → slow alpha → Schmitt trigger cannot fire immediately → no boot snap.
- `resetDumbFilter()`: resets `dumbDutyFiltered` and `dumbCCTFiltered` to -1 sentinel; called on NORMAL→DUMB switch
- `prevDutyStep`, `prevCCTStep` initialised to -1 (sentinel for first-call)

## Known Issues / History

- Boot CCT snap: persistent issue across multiple PRs. Root cause (final): even after seeding IIR filters from a fresh 8-sample ADC average, `prevCCTStep` was still derived from the boot-time `cct` parameter (a 16-sample average taken 1250ms earlier). Cold-start ADC drift meant these two disagreed → Schmitt trigger fired on first `handlePots()` frame → CCT snap. **Fixed:** (a) `delay(100)` added before boot ADC reads to let ADC settle; (b) `syncPotsAfterBoot()` now derives `prevDutyStep`/`prevCCTStep` from the same 8-sample ADC average that seeds the IIR filters — critical invariant maintained.
- DUMB gamma bug: `applyLEDsImmediate` was applying gamma to DUMB mode (`useGamma = !(FREQ || CAL)` did not exclude DUMB). At `min_duty=0.0453`, gamma gave `0.0453^2.2 = 0.00114 = 0.11%` display minimum instead of 0.01%, and max ≈ 52.5% instead of 100%. **Fixed:** `MODE_DUMB` is now excluded from `useGamma`; display formula changed to pot-normalized `(rawB - min_duty)/(1 - min_duty) * 100`.
- DUMB mode filter lag: old implementation was a cascade double-filter — shared adaptive IIR (α=0.10/0.60) followed by a second fixed-α IIR (α=0.05) on the already-filtered output. Combined lag was severe. **Fixed:** DUMB IIR now operates on raw (pre-shared-IIR) ADC normalized values, with adaptive α=0.40 (moving, delta > 0.005) / α=0.10 (settled). Feels analog-snappy when moving, suppresses noise when settled.
- DUMB idle jitter: with α=0.10 settled and RP2040 ADC noise of ~10–20 LSB, filtered output could still cross old dead-band thresholds. **Fixed:** widened to `DUMB_BRIGHTNESS_DB = 0.005` and `DUMB_CCT_DB = 10.0K`.
- DUMB min: pots are calibrated to reach 0 cleanly; snap zones should NOT be added.
- Step oscillation: old `|x - N| > 0.05` hysteresis caused oscillation at boundaries. Replaced with Schmitt trigger (±0.15 dead-band).

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
