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
  void repaintTop() override { drawHeader(); }
 private:
  void drawHeader();
  void drawInfo();             // header, clock row, message, bars
  void drawPet(uint32_t now, bool push = true);  // the animated pet area (off-screen, then pushed)
  uint32_t shownVersion = 0, shownFrame = 0, shownHead = UINT32_MAX;
  int lastMinute = -1;
  bool msgShown = false;
  int8_t shownLayout = -1;   // 0 = egg rows, 1 = pet rows, -1 = not drawn yet
  // the big moments: hold Feed + Play, then a show -> white -> the new pet. Hatching (S4): cracks first; evolving (S5):
  // the screen goes dark, the pet turns white and swaps with its next form
  enum class Hatch : uint8_t { None, Cracks, Evolving, WhiteIn, White, Dim, Bright };
  bool evolving = false;       // which show: false = hatch, true = evolution
  void fillShow(uint32_t now); // the pet area as the show has it now (not pushed)
  int showY() const;
  void updateHatch(uint32_t now);
  void drawAll();              // the whole screen (enter, and after the hatch)
  void drawEggRows();          // the egg's text line + bar (also the "Hold" bar while held)
  Hatch hatch = Hatch::None;
  uint32_t holdStart = 0, phaseAt = 0, lastShowDraw = 0;
  int shownHold = -1;
  uint8_t backlightTo = 0;
};
