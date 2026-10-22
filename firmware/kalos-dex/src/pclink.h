// Link to the PC over the USB cable (the same serial port as the log).
// The PC sends one command per line; the gadget answers with a line starting "OK " or "ERR ".
// Everything else on the port is just the log (the PC companion ignores it).
//   TIME <unix seconds UTC>   -> sets the clock + RTC, answers "OK TIME drift=<s>"
//   PING                      -> "OK PONG <firmware version>"
//   POWERTEST ON / OFF        -> strip full white (capped by the limiter) + a power line in the log every 30 s
#pragma once
#include <Arduino.h>

namespace pclink {
void update(uint32_t now);   // call every loop: reads whatever arrived, runs complete lines
}
