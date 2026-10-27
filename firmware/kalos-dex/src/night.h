// Night dimming: between NIGHT_FROM_MIN and NIGHT_TO_MIN (local time, only when the time is known) the screen
// and the strip are capped at NIGHT_SCREEN_PCT / NIGHT_LIGHTS_PCT – never brighter than the user's own setting.
// Changes ramp smoothly in update(). Settings row "Night dim" switches it on / off (on by default).
#pragma once
#include <Arduino.h>

namespace night {
void update(uint32_t now);   // call every loop
bool active();               // dimming right now
uint8_t backlightTarget();   // what the screen should be at right now (user setting or the night cap)
uint8_t stripTarget();
void force(int mode);        // test (hwtest NIGHT ON / OFF / AUTO): 1 = night, 0 = day, -1 = by the clock
}  // namespace night
