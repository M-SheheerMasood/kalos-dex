// A screen = something that fills the display. The manager switches between them.
#pragma once
#include <Arduino.h>
#include "config.h"
#include "input.h"
#include <TFT_eSPI.h>

class Screen {
 public:
  virtual ~Screen() {}
  virtual const char *name() const = 0;
  virtual void enter() = 0;               // draw everything (backlight is off while this runs)
  virtual void update(uint32_t now) {}    // called every loop: redraw only what changed
  virtual void leave() {}                 // free big buffers, switch off test lights, ...
  virtual bool onEvent(const InputEvent &e) { return false; }  // true = the screen used it
  // The top band (TOP_BAND_H) was covered by the volume pop-up / mute badge: draw it again.
  // Default = draw the whole screen again (fine for test screens).
  virtual void repaintTop() { leave(); enter(); }
  virtual bool showsVolume() const { return false; }   // true = no pop-up / badge here (Music has its own)
};

constexpr int TOP_BAND_H = 44;   // the red title bar every screen has at the top (rows 0..TOP_INSET hide under the case lip)
constexpr int TITLE_Y = 25;      // centre line of the title text: ~5 red rows under the "g" (user picked on the gadget, step 09)
constexpr int TOP_MID_Y = (TOP_INSET + TOP_BAND_H) / 2;   // middle of the VISIBLE part of the bar: text + icons go here

namespace screens {
void begin();
void next();          // Back button: next screen, one direction, looping
void home();          // hold the Back button
void update(uint32_t now);
void handle(const InputEvent &e);   // global buttons first, then the current screen
Screen *current();
void repaintTop();   // the current screen redraws its top band (after an overlay)
// Call on any sprite just before pushing it to the screen at (x, y): if it covers the mute badge's corner,
// the manager keeps those pixels (the badge fades over them) and blends the badge in.
void blendCorner(TFT_eSprite &spr, int x, int y);
}  // namespace screens
