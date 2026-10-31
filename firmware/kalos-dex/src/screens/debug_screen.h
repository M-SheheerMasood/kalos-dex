// Debug = cheats on the gadget (step 2026-10-29-06, user's design): shown after Settings only while "Debug" is on
// (Settings page 2). Works on the TEST pet (pet::setTestMode): nothing is saved, Debug off = the real pet is back.
// Feed / Play = previous / next row (like Settings), knob left / right = the row's two actions (shown at the bottom).
// RAM while shown: one row strip 232 x 26 x 2 = 12 KB, freed on leave.
#pragma once
#include "screen.h"

class DebugScreen : public Screen {
 public:
  const char *name() const override { return "Debug"; }
  void enter() override;
  void update(uint32_t now) override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
  void repaintTop() override { drawHeader(); }
 private:
  void drawHeader();
  void drawRow(int i);
  void drawFooter();
  void act(bool right);   // the chosen row's left / right action
  int sel = 0;
  uint32_t shownVersion = 0;
  char msg[40] = "";
};
