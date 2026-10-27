// The shared look (UI polish, step 2026-10-27-08): the rounded smooth font (Nunito ExtraBold, tools/font_convert.py)
// and Pokémon-style HP bars. Works on the screen (tft) and on sprites. Not used by the clock face.
#pragma once
#include <TFT_eSPI.h>

namespace ui {
enum Font : uint8_t { SMALL, BODY, TITLE };   // 12 / 15 / 22 px, in place of the built-in fonts 1 / 2 / 4

// Text with the current datum, colours and padding of `g`. The font is loaded only for the call (~1 KB, freed
// again), so every other drawString keeps the built-in fonts. ALWAYS set a background colour first
// (setTextColor(fg, bg)): the edges are blended with it – the display can't be read back.
void text(TFT_eSPI &g, const char *s, int x, int y, Font f);
int width(TFT_eSPI &g, const char *s, Font f);
Font fit(TFT_eSPI &g, const char *s, int maxW, Font f);   // f, or the next smaller font when s is wider than maxW

constexpr uint16_t TRACK = 0x73D1;   // empty part of a bar: mid grey (TN panel: no near-white, step 07)
// Capsule bar: black 2 px rim, `track` behind, `fill` up to frac (0..1) with a lighter line on top.
void hpBar(TFT_eSPI &g, int x, int y, int w, int h, float frac, uint16_t fill, uint16_t track = TRACK);
}  // namespace ui
