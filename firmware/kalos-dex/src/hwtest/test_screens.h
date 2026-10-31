// Hardware-test screens (hwtest build only): edges, input monitor, lights + strip calibration, info, clock test.
#pragma once
#include "../screens/screen.h"
#include <time.h>

class EdgeScreen : public Screen {
 public:
  const char *name() const override { return "Edges"; }
  void enter() override;
};

class InputScreen : public Screen {
 public:
  const char *name() const override { return "Input"; }
  void enter() override;
  void update(uint32_t now) override;
  bool onEvent(const InputEvent &e) override;
 private:
  char log[6][28] = {};
  bool dirty = true;
  bool lastDown[5] = {};
  int32_t lastClicks = INT32_MIN;
};

class LightsScreen : public Screen {
 public:
  const char *name() const override { return "Lights"; }
  void enter() override;
  void update(uint32_t now) override;
  bool onEvent(const InputEvent &e) override;
  void leave() override;
 private:
  int effect = 4, colourIdx = 0, calIdx = -1;
  bool dirty = true;
  uint32_t lastDraw = 0;
};

class InfoScreen : public Screen {
 public:
  const char *name() const override { return "Info"; }
  void enter() override;
  void update(uint32_t now) override;
 private:
  uint32_t lastDraw = 0;
};

// Display: try gamma presets + VCOM on this TN panel (colours shift with the viewing angle, PLAN §3d).
// Feed / Play = preset, knob = VCOM; the pattern shows what drifts (grey + RGB ramps, the Home bars, pet + shadow).
class DisplayScreen : public Screen {
 public:
  const char *name() const override { return "Display"; }
  void enter() override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
 private:
  void apply();
  void draw();
  int preset = 0, vcom = -1;   // vcom -1 = the preset's own value
};

// Glow: step through every mood's side glow on the real strip (Feed / Play), without touching the pet.
class GlowScreen : public Screen {
 public:
  const char *name() const override { return "Glow"; }
  void enter() override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
 private:
  void draw();
  int idx = 0;
};

class ClockTestScreen : public Screen {
 public:
  const char *name() const override { return "Clock test"; }
  void enter() override;
  void update(uint32_t now) override;
 private:
  uint32_t lastDraw = 0;
  time_t lastShown = -1;
};
