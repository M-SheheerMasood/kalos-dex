#include "leds.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(STRIP_LEN, PIN_STRIP, NEO_GRB + NEO_KHZ800);

// ---------------- status LEDs
static const int STATUS_PIN[3] = {PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_RED};
static leds::Mode statusMode[3] = {};

// ---------------- strip state
static leds::Effect effect = leds::Effect::Off;
static uint32_t effColour = 0xFFB040;
static uint16_t effPeriod = 3000;
static uint8_t userBrightness = STRIP_BRIGHTNESS;
static uint32_t flashColour = 0, flashUntil = 0;
static bool flashRb = false;
static bool visible[STRIP_LEN];
static int calibrating = -1;
static int lastMa = 0;
static bool safe = false;

static const char *EFFECT_NAMES[] = {"Off", "Solid", "Breathe", "Pulse", "Rainbow"};

static uint32_t scale(uint32_t c, uint8_t k) {  // colour * k/255
  uint8_t r = ((c >> 16) & 255) * k / 255, g = ((c >> 8) & 255) * k / 255, b = (c & 255) * k / 255;
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

namespace leds {

void begin() {
  // NOTE: must run AFTER display::begin(). The ESP32 SPI driver attaches MISO to GPIO 19 even when
  // we pass "no MISO" (-1 -> default pin 19) and makes it an INPUT, which killed the yellow LED.
  // pinMode(OUTPUT) takes the pin back; the display never reads, so it loses nothing.
  for (int p : STATUS_PIN) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  for (bool &v : visible) v = true;   // main.cpp loads the saved visible-LED map right after
  strip.begin();
  strip.clear();
  strip.show();
}

void setSafe(bool on) { safe = on; }
void setStatus(Status which, Mode mode) { statusMode[(int)which] = mode; }
const char *effectName(Effect e) { return EFFECT_NAMES[(int)e]; }
void setEffect(Effect e, uint32_t colour, uint16_t periodMs) { effect = e; effColour = colour; effPeriod = max<uint16_t>(periodMs, 200); }
void flash(uint32_t colour, uint16_t ms) { flashColour = colour; flashRb = false; flashUntil = millis() + ms; }
void flashRainbow(uint16_t ms) { flashRb = true; flashUntil = millis() + ms; }
void setBrightness(uint8_t b) { userBrightness = b; }
uint8_t brightness() { return userBrightness; }
int stripMilliamps() { return lastMa; }
int totalMilliamps() {
  return lastMa + ESP32_MA + DISPLAY_LOGIC_MA + BACKLIGHT_FULL_MA * BACKLIGHT_NORMAL / 255 + STRIP_LEN * IDLE_MA_PER_LED;
}
bool isVisible(int i) { return i >= 0 && i < STRIP_LEN && visible[i]; }
void setVisible(int i, bool v) { if (i >= 0 && i < STRIP_LEN) visible[i] = v; }
void calibrate(int i) { calibrating = i; }
uint32_t visibleMask() { uint32_t m = 0; for (int i = 0; i < STRIP_LEN; i++) if (visible[i]) m |= 1UL << i; return m; }
void setVisibleMask(uint32_t m) { for (int i = 0; i < STRIP_LEN; i++) visible[i] = (m >> i) & 1; }

void update(uint32_t now) {
  // status LEDs
  for (int i = 0; i < 3; i++) {
    bool on = !safe && (statusMode[i] == Mode::On ||
              (statusMode[i] == Mode::BlinkSlow && (now / 500) % 2) ||
              (statusMode[i] == Mode::BlinkFast && (now / 120) % 2));
    digitalWrite(STATUS_PIN[i], on);
  }

  // strip at ~50 fps
  static uint32_t lastFrame = 0;
  if (now - lastFrame < 20) return;
  lastFrame = now;

  uint32_t frame[STRIP_LEN];
  float phase = (now % effPeriod) / float(effPeriod);   // 0..1 through one cycle
  for (int i = 0; i < STRIP_LEN; i++) {
    uint32_t c = 0;
    switch (effect) {
      case Effect::Off: c = 0; break;
      case Effect::Solid: c = effColour; break;
      case Effect::Breathe: c = scale(effColour, 40 + 215 * (0.5f - 0.5f * cosf(phase * TWO_PI))); break;
      case Effect::Pulse: c = scale(effColour, phase < 0.15f ? 255 : 60); break;  // short beat, then dim
      case Effect::Rainbow: c = Adafruit_NeoPixel::ColorHSV(uint16_t(phase * 65535) + i * 65536L / STRIP_LEN); break;
      default: break;
    }
    if (now < flashUntil) c = flashRb ? Adafruit_NeoPixel::ColorHSV(uint16_t(now * 120) + i * 65536L / STRIP_LEN) : flashColour;
    if (calibrating >= 0) c = (i == calibrating) ? 0xFFFFFF : 0;
    else if (!visible[i] || safe) c = 0;
    frame[i] = c;
  }

  // power limiter: estimate the current and dim the whole frame if it would exceed the budget
  uint32_t channelSum = 0;
  for (uint32_t c : frame) channelSum += ((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255);
  float fullMa = channelSum / 255.0f * MA_PER_CHANNEL;
  uint8_t b = (calibrating >= 0) ? 30 : userBrightness;
  const int stripBudget = TOTAL_BUDGET_MA - (totalMilliamps() - lastMa);
  if (fullMa * b / 255.0f > stripBudget) b = stripBudget * 255.0f / fullMa;
  lastMa = fullMa * b / 255.0f;

  strip.setBrightness(b);  // set before the colours: NeoPixel scales them when they're stored
  for (int i = 0; i < STRIP_LEN; i++) strip.setPixelColor(i, frame[i]);
  strip.show();
}

}  // namespace leds
