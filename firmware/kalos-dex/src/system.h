// Safety basics: watchdog (restarts the gadget if the loop hangs), reset reason, safe mode.
// Safe mode = no boot animation, strip + status LEDs off, "SAFE MODE" shown. It starts when
//   - the Back button is held while plugging in, or
//   - the last SAFE_AFTER_CRASHES starts all ended in a crash (watchdog, panic, brown-out) – no endless crash loop.
// Leave it by unplugging and plugging in again (without holding Back).
#pragma once
#include <Arduino.h>

namespace sys {
void begin();               // right after input::begin(): reads the reset reason, the crash count and the Back button
void startWatchdog();       // after the boot animation (that one takes ~4 s on purpose)
void update(uint32_t now);  // call every loop: feeds the watchdog, clears the crash count after a good minute
bool safeMode();
const char *safeReason();   // "Back held at start" / "3 crashes in a row" / ""
const char *resetReason();  // why the last start happened ("power on", "watchdog", ...)
bool lastWasCrash();
}  // namespace sys
