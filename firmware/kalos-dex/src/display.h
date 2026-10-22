// Display layer: TFT set-up, backlight PWM, fades, colours shared by every screen.
#pragma once
#include <TFT_eSPI.h>
#include "config.h"

extern TFT_eSPI tft;

namespace display {
void begin();
void setBacklight(uint8_t level);           // 0..255, instant
uint8_t backlight();
void fadeBacklight(uint8_t to, uint16_t ms); // smooth change, blocks for `ms`
}  // namespace display

// Kalos Dex colours (RGB565)
namespace col {
constexpr uint16_t RED = 0xD8A4;        // cap red
constexpr uint16_t RED_DARK = 0x9000;
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t SILVER = 0xBDF7;
constexpr uint16_t SCREEN_BG = 0xB7DF;  // pale screen blue (like the reference picture)
constexpr uint16_t SCREEN_DARK = 0x2A4F;
constexpr uint16_t TEXT = 0x18E3;
}  // namespace col
