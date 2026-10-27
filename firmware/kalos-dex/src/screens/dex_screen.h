// Dex entry = the pet's Pokédex page, two pages (Feed = previous, Play = next):
//   ENTRY     picture, No. + name, category, type badges, height / weight, the Pokédex entry text
//   TOGETHER  level, XP, next evolution, time together, met / hatched dates, age, fed, played, nights, shiny, mood
// All species data comes from pet_line.h, the picture from pet_art – swap those for a real line, not this file.
// RAM while shown: picture 112x112 16-bit 25 KB + row strip 232x20 9 KB = 34 KB, freed on leave.
#pragma once
#include "screen.h"

class DexScreen : public Screen {
 public:
  const char *name() const override { return "Dex"; }
  void enter() override;
  void update(uint32_t now) override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
  void repaintTop() override { drawHeader(); }
 private:
  void drawHeader();
  void drawPage();
  void drawEntry();
  void drawTogether();
  int page = 0;
  uint32_t shownVersion = 0;
  int shownMinute = -1;
};
