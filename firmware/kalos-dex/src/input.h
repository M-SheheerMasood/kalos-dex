// Input layer: knob (polled every 0.5 ms – interrupts lost clicks with this KY-040) + buttons -> a small queue of events.
#pragma once
#include <Arduino.h>

enum class Btn : uint8_t { Knob, Screen, Feed, Play, COUNT };   // Screen = the Back button
enum class Ev : uint8_t { None, Turn, Press, Hold, Double };

struct InputEvent {
  Ev type = Ev::None;
  Btn button = Btn::Knob;  // for Press / Hold / Double
  int8_t steps = 0;        // for Turn: + clockwise, - anticlockwise (clicks since the last event)
};

namespace input {
void begin();
void update(uint32_t now);            // call every loop
bool next(InputEvent &e);             // pops the oldest event, false if none
void enableDouble(Btn b, bool on);    // double-press detection (delays single presses by DOUBLE_MS)
bool isDown(Btn b);                   // raw state (e.g. "hold Feed + Play together")
const char *name(Btn b);
int32_t totalClicks();                // knob clicks since boot (for tests / egg ideas)
}  // namespace input
