// Clock face 1: "square Minecraft clock" – the whole screen is the scene, drawn in chunky 4x4 "Minecraft
// pixels": a day/night sky disc (sun, moon, stars, clouds) turning once a day around the centre, very dim
// below the horizon; thick bevelled gold border; gold outlined numbers on a circle; sword hands (iron = hours,
// diamond = minutes, move every second) and a ticking red second hand.
// (The earlier round "sky dial" is kept in archive/clock_sky_dial_v1/.)
#pragma once
#include "screen.h"

class ClockScreen : public Screen {
 public:
  const char *name() const override { return "Clock"; }
  void enter() override;
  void update(uint32_t now) override;
  bool onEvent(const InputEvent &e) override;
 private:
  void render(const struct tm &lt, bool ok);
  int lastSec = -1;
  bool demo = false;          // "fast day": one whole day in 60 s (hold Play)
  uint32_t demoStart = 0, lastDemoFrame = 0;
  time_t demoBase = 0;
};
