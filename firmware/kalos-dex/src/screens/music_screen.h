// Music = now playing from the PC: a spinning record (placeholder until album art), scrolling title, artist,
// progress, prev / play-pause / next, volume. Feed = previous, Play = next; the knob works on every screen
// (turn = volume, press = play/pause, double = mute – handled by the manager).
// RAM while shown: disc 124x124 8-bit 15 KB + title strip 232x26 12 KB + shared row strip 232x36 17 KB = 44 KB,
// freed on leave. Every part is drawn off-screen and pushed whole (no flicker), and only when its data changed.
#pragma once
#include "screen.h"

class MusicScreen : public Screen {
 public:
  const char *name() const override { return "Music"; }
  void enter() override;
  void update(uint32_t now) override;
  void leave() override;
  bool onEvent(const InputEvent &e) override;
  bool showsVolume() const override { return true; }
  void repaintTop() override { drawHeader(); }
 private:
  void drawDot();
  void drawAll();
  void drawHeader();
  void drawText();
  void drawTitle(uint32_t now);
  void drawProgress();
  void drawControls();
  void drawVolume();
  void drawDisc();
  uint32_t shownTrack = 0, shownState = 0, shownVol = 0, shownKeyAt = 0, lastDisc = 0, lastTitle = 0, titleStart = 0;
  int shownSecond = -1, titleW = 0;
  bool shownOnline = false, keyLit = false;
  float angle = 0;
};
