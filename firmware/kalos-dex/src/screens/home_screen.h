// Home = the pet: name + level, small clock, the pet (placeholder blob until there are sprites), what it
// just said, food / fun / energy / XP bars. Feed / Play buttons act on the pet.
#pragma once
#include "screen.h"
#include <TFT_eSPI.h>

class HomeScreen : public Screen {
 public:
  const char *name() const override { return "Home"; }
  void enter() override;
  void update(uint32_t now) override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
 private:
  void drawInfo();             // header, clock row, message, bars
  void drawPet(uint32_t now);  // the animated pet area (off-screen, then pushed)
  uint32_t shownVersion = 0, lastFrame = 0;
  int lastMinute = -1;
  bool msgShown = false;
};
