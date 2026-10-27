// Lights: 3 status LEDs + the glow strip (effects, brightness, power limiter, visible-LED map).
#pragma once
#include <Arduino.h>

namespace leds {

// ---- status LEDs (meanings set by the features: green = PC connected (F7), red = pet needs you (F6))
enum class Status : uint8_t { Green, Yellow, Red };
enum class Mode : uint8_t { Off, On, BlinkSlow, BlinkFast };
void setStatus(Status which, Mode mode);

// ---- glow strip
enum class Effect : uint8_t { Off, Solid, Breathe, Pulse, Rainbow, COUNT };
const char *effectName(Effect e);
void setEffect(Effect e, uint32_t colour = 0xFFB040, uint16_t periodMs = 3000);  // colour = 0xRRGGBB
void flash(uint32_t colour, uint16_t ms);  // short overlay (level-up, feed, ...) on top of the effect
void flashRainbow(uint16_t ms);            // same, a fast rainbow ripple (play, level-up)
void setBrightness(uint8_t b);             // 0..255, before the power limiter
uint8_t brightness();
int stripMilliamps();                      // estimated current of the lit LEDs this frame
int totalMilliamps();                      // estimate for the whole gadget

// ---- visible-LED map (LEDs hidden inside the case stay off)
bool isVisible(int i);
void setVisible(int i, bool v);
uint32_t visibleMask();
void setVisibleMask(uint32_t m);
void calibrate(int i);                     // >= 0: light ONLY LED i (white); -1: back to normal

void begin();
void setSafe(bool on);                     // safe mode: strip + status LEDs stay off
void update(uint32_t now);                 // call every loop

}  // namespace leds
