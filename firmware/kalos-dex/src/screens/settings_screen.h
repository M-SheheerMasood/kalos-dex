// Settings: screen brightness, light-strip brightness, night dimming, 12 / 24 h, knob direction, reset pet.
// The knob turns the same way round here as for the volume ("Knob direction" flips both).
// Feed / Play = previous / next row, knob turn (or press) = change it. Here the knob is NOT the volume knob.
// Reset pet = hold Feed + Play together for 3 s (anywhere on this screen). Saved 3 s after the last change.
// RAM while shown: one row strip 232 x 32 x 2 = 15 KB, freed on leave.
#pragma once
#include "screen.h"

class SettingsScreen : public Screen {
 public:
  const char *name() const override { return "Settings"; }
  void enter() override;
  void update(uint32_t now) override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
  void repaintTop() override { drawHeader(); }
 private:
  void drawHeader();
  void drawRow(int i);
  void change(int steps);
  int sel = 0;
  uint32_t holdStart = 0, doneAt = 0;
  int shownHold = -1;
};
