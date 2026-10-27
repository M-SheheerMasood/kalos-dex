#include "display.h"

TFT_eSPI tft;
static uint8_t current = 0;

namespace display {

void begin() {
  ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, BACKLIGHT_PWM_CHANNEL);
  setBacklight(0);  // stay dark until something is drawn
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  tft.writecommand(0xC7);   // VCOM offset: the library's gamma, the user's contrast (hwtest Display screen)
  tft.writedata(DISPLAY_VCOM);
  tft.fillScreen(TFT_BLACK);
}

void setBacklight(uint8_t level) {
  current = level;
  ledcWrite(BACKLIGHT_PWM_CHANNEL, level);
}

uint8_t backlight() { return current; }

void fadeBacklight(uint8_t to, uint16_t ms) {
  const uint8_t from = current;
  const uint32_t start = millis();
  for (uint32_t t = 0; t < ms; t = millis() - start) {
    setBacklight(from + (int(to) - int(from)) * int(t) / int(ms));
    delay(4);
  }
  setBacklight(to);
}

}  // namespace display
