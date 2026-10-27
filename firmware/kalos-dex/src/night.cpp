#include "night.h"
#include "config.h"
#include "storage.h"
#include "timekeeper.h"
#include "display.h"
#include "leds.h"

static constexpr uint8_t NIGHT_BACKLIGHT = BACKLIGHT_NORMAL * NIGHT_SCREEN_PCT / 100;   // 64
static constexpr uint8_t NIGHT_STRIP = 255 * NIGHT_LIGHTS_PCT / 100;                   // 25
static bool isNight = false;
static uint32_t lastCheck = 0, lastStep = 0;
static int stripShown = -1;
static int forced = -1;   // test: -1 = by the clock, 0 = day, 1 = night

namespace night {

bool active() { return isNight; }
uint8_t backlightTarget() { return isNight ? min(storage::backlightLevel(), NIGHT_BACKLIGHT) : storage::backlightLevel(); }
uint8_t stripTarget() {
  const uint8_t b = storage::settings().stripBrightness;
  return isNight ? min(b, NIGHT_STRIP) : b;
}

void update(uint32_t now) {
  if (now - lastCheck >= 1000) {   // is it night? (once a second)
    lastCheck = now;
    struct tm lt;
    bool n = false;
    if (!(storage::settings().flags & SET_NO_NIGHT_DIM) && timekeeper::localTime(lt)) {
      const int m = lt.tm_hour * 60 + lt.tm_min;
      n = NIGHT_FROM_MIN < NIGHT_TO_MIN ? (m >= NIGHT_FROM_MIN && m < NIGHT_TO_MIN) : (m >= NIGHT_FROM_MIN || m < NIGHT_TO_MIN);
    }
    if (forced >= 0) n = forced;
    if (n != isNight) {
      isNight = n;
      Serial.printf("night dimming: %s (screen -> %u, strip -> %u of 255)\n", n ? "ON" : "off", backlightTarget(), stripTarget());
    }
  }
  // strip: set when the target changes (test screens may set their own brightness meanwhile)
  const int st = stripTarget();
  if (st != stripShown) { stripShown = st; leds::setBrightness(st); }
  // screen: walk the backlight towards the target, one step at a time (a screen-change fade is 0, leave it)
  const uint8_t cur = display::backlight(), tgt = backlightTarget();
  if (cur && cur != tgt && now - lastStep >= NIGHT_RAMP_MS / BACKLIGHT_NORMAL) {
    lastStep = now;
    display::setBacklight(cur < tgt ? cur + 1 : cur - 1);
  }
}

void force(int mode) { forced = mode; lastCheck = 0; }
uint8_t backlightNow() { return display::backlight(); }

}  // namespace night
