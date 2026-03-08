#include "pins.h"
#include "modes.h"
#include "state.h"
#include "pots_state.h"
#include "inputs.h"
#include "display_ui.h"
#include "buzzer.h"

// ===============================
//  BUTTON ENGINE (DJI-STYLE)
// ===============================

// Timing constants
static const unsigned long SHORT_DECISION_DELAY_MS = 250;  // delay before committing short
static const unsigned long COMBO_WINDOW_MS         = 300;  // window for short→long combo
static const unsigned long LONG_PRESS_MS           = 800;  // long press threshold

// MAIN button state
static bool          mainLast            = false;
static unsigned long mainPressTime       = 0;
static bool          mainLongFired       = false;

// Pending short for MAIN (for DJI-style + combo)
static bool          mainShortPending    = false;
static unsigned long mainShortTime       = 0;
static unsigned long mainShortHeldMs     = 0;
static bool          mainComboActive     = false;

// DISP button state
static bool          dispLast            = false;
static unsigned long dispPressTime       = 0;
static bool          dispLongFired       = false;

// Pending short for DISP
static bool          dispShortPending    = false;
static unsigned long dispShortTime       = 0;
static unsigned long dispShortHeldMs     = 0;
static bool          dispComboActive     = false;

// Forward declarations (local to this file)
static void handleMainButtonLogic(unsigned long now);
static void handleDispButtonLogic(unsigned long now);

// -----------------------------
// MAIN BUTTON LOGIC
// -----------------------------
static void handleMainButtonLogic(unsigned long now) {
    bool mainPressed = digitalRead(MAIN_BUTTON_PIN);

    // Press edge
    if (mainPressed && !mainLast) {
        mainPressTime   = now;
        mainLongFired   = false;
        mainComboActive = false;

        // Check if this press is a candidate for combo (second press)
        if (mainShortPending &&
            (now - mainShortTime) <= COMBO_WINDOW_MS &&
            currentMode != MODE_DUMB) {
            // Start combo on this second press
            mainComboActive = true;
        }
    }

    // Held
    if (mainPressed && mainLast) {
        unsigned long held = now - mainPressTime;

        // Long press
        if (!mainLongFired && held >= LONG_PRESS_MS) {

            if (mainComboActive) {
                // Short-long combo (NORMAL + STANDBY allowed, DUMB blocked above)
                handleMainShortLongCombo();
                mainShortPending = false;  // consume pending short
            } else {
                // Pure long press
                // DUMB MODE: long press ignored by spec
                if (currentMode != MODE_DUMB) {
                    handleMainLongPress();
                }
            }

            mainLongFired = true;
        }
    }

    // Release edge
    if (!mainPressed && mainLast) {
        unsigned long held = now - mainPressTime;

        if (!mainLongFired) {
            // No long fired → candidate short
            // DUMB MODE: short always goes to STANDBY immediately (no DJI delay)
            if (currentMode == MODE_DUMB) {
                handleMainButtonRelease(held, now);
            } else {
                // NORMAL / STANDBY / OVERRIDE / DEMO:
                // DJI-style: short is pending, not immediate
                mainShortPending = true;
                mainShortTime    = now;
                mainShortHeldMs  = held;
            }
        }
    }

    // Commit pending short if:
    //  - enough time has passed
    //  - no combo started on second press
    //  - no long consumed it
    if (mainShortPending &&
        (now - mainShortTime) > SHORT_DECISION_DELAY_MS &&
        !mainComboActive) {

        handleMainButtonRelease(mainShortHeldMs, now);
        mainShortPending = false;
    }

    mainLast = mainPressed;
}

// -----------------------------
// DISPLAY BUTTON LOGIC
// -----------------------------
static void handleDispButtonLogic(unsigned long now) {
    bool dispPressed = digitalRead(DISP_BUTTON_PIN);

    // In DUMB MODE: display button only toggles display (short), no long, no combo
    if (currentMode == MODE_DUMB) {

        if (dispPressed && !dispLast) {
            dispPressTime  = now;
            dispLongFired  = false;
        }

        if (!dispPressed && dispLast) {
            unsigned long held = now - dispPressTime;
            // Always treat as short in DUMB MODE
            handleDispButtonRelease(held);
        }

        dispLast = dispPressed;
        return;
    }

    // Normal / Standby / Override / Demo behavior with DJI-style + combo

    // Press edge
    if (dispPressed && !dispLast) {
        dispPressTime   = now;
        dispLongFired   = false;
        dispComboActive = false;

        // Check if this press is a candidate for combo (second press)
        if (dispShortPending &&
            (now - dispShortTime) <= COMBO_WINDOW_MS) {
            dispComboActive = true;
        }
    }

    // Held
    if (dispPressed && dispLast) {
        unsigned long held = now - dispPressTime;

        if (!dispLongFired && held >= LONG_PRESS_MS) {

            if (dispComboActive) {
                // Short-long combo → DEMO toggle
                handleDispShortLongCombo();
                dispShortPending = false;
            } else {
                // Pure long press → DEMO long behavior
                handleDispLongPress();
            }

            dispLongFired = true;
        }
    }

    // Release edge
    if (!dispPressed && dispLast) {
        unsigned long held = now - dispPressTime;

        if (!dispLongFired) {
            // Candidate short, pending DJI-style
            dispShortPending = true;
            dispShortTime    = now;
            dispShortHeldMs  = held;
        }
    }

    // Commit pending short if:
    //  - enough time has passed
    //  - no combo started on second press
    //  - no long consumed it
    if (dispShortPending &&
        (now - dispShortTime) > SHORT_DECISION_DELAY_MS &&
        !dispComboActive) {

        handleDispButtonRelease(dispShortHeldMs);
        dispShortPending = false;
    }

    dispLast = dispPressed;
}

// -----------------------------
// PUBLIC ENTRY POINT
// -----------------------------
void processButtons(unsigned long now) {
    handleMainButtonLogic(now);
    handleDispButtonLogic(now);

    // Dual-button buzzer toggle (unchanged)
    bool mainPressed = digitalRead(MAIN_BUTTON_PIN);
    bool dispPressed = digitalRead(DISP_BUTTON_PIN);
    if (systemInitialized) {
        handleBuzzerToggle(now, mainPressed, dispPressed);
    }
}