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

class ClockTestScreen : public Screen {
 public:
  const char *name() const override { return "Clock test"; }
  void enter() override;
  void update(uint32_t now) override;
 private:
  uint32_t lastDraw = 0;
  time_t lastShown = -1;
};
