// A screen = something that fills the display. The manager switches between them.
#pragma once
#include <Arduino.h>
#include "input.h"

class Screen {
 public:
  virtual ~Screen() {}
  virtual const char *name() const = 0;
  virtual void enter() = 0;               // draw everything (backlight is off while this runs)
  virtual void update(uint32_t now) {}    // called every loop: redraw only what changed
  virtual void leave() {}                 // free big buffers, switch off test lights, ...
  virtual bool onEvent(const InputEvent &e) { return false; }  // true = the screen used it
};

namespace screens {
void begin();
void next();          // Back button: next screen, one direction, looping
void home();          // hold the Back button
void update(uint32_t now);
void handle(const InputEvent &e);   // global buttons first, then the current screen
Screen *current();
}  // namespace screens
