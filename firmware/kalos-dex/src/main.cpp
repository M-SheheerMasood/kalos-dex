// Kalos Dex – firmware (PLAN.md). Done: F0 skeleton, F1 display + boot + screens, F2 input, F3 lights,
// F4 storage, F5 time (RTC + time from the PC over USB) + clock face, F6 pet engine (placeholder blob).
// Next: F6 sprites (needs the Pokémon line), F7 PC companion.
// Two builds from this one code base:  pio run -e esp32dev (the gadget)  /  pio run -e hwtest (+ test screens).
#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "boot.h"
#include "input.h"
#include "leds.h"
#include "storage.h"
#include "timekeeper.h"
#include "pclink.h"
#include "pet.h"
#include "media.h"
#include "system.h"
#include "night.h"
#include "screens/screen.h"

// safe mode instead of the boot animation: plain, small, nothing that could have caused the crash
static void safeModeScreen() {
  tft.fillScreen(col::SCREEN_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(col::RED, col::SCREEN_BG);
  tft.drawString("SAFE MODE", SCREEN_W / 2, SCREEN_H / 2 - 30, 4);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  tft.drawString(sys::safeReason(), SCREEN_W / 2, SCREEN_H / 2 + 4, 2);
  tft.drawString("lights off", SCREEN_W / 2, SCREEN_H / 2 + 24, 2);
  tft.drawString("unplug + replug to leave", SCREEN_W / 2, SCREEN_H / 2 + 44, 2);
  display::fadeBacklight(storage::backlightLevel(), 300);
  delay(2500);
}

void setup() {
  Serial.begin(115200);
  delay(200);
#ifdef HWTEST
  Serial.printf("\n=== Kalos Dex %s  [HARDWARE TEST BUILD] ===\n", FW_VERSION);
#else
  Serial.printf("\n=== Kalos Dex %s ===\n", FW_VERSION);
#endif
  Serial.printf("free RAM at start: %u KB, biggest block %u KB\n", ESP.getFreeHeap() / 1024,
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
  input::begin();
  sys::begin();    // reset reason, crash count, Back held = safe mode
  display::begin();
  leds::begin();   // after the display: re-claims GPIO 19 from the SPI driver (yellow LED)
  leds::setSafe(sys::safeMode());
  storage::begin();
  timekeeper::begin();
  leds::setBrightness(storage::settings().stripBrightness);   // night::update() caps it at night
  leds::setVisibleMask(storage::settings().visibleMask);
  Serial.print("strip: visible map ");
  for (int i = 0; i < STRIP_LEN; i++) Serial.print(leds::isVisible(i) ? '1' : '0');
  Serial.println("  (LED 1 first)");
  pet::begin();   // after storage + time: loads the pet and catches up on the time it was switched off
  uint32_t t = millis();
  if (sys::safeMode()) safeModeScreen();
  else bootAnimation();
  Serial.printf("%s: %lu ms, free RAM after: %u KB\n", sys::safeMode() ? "safe-mode screen" : "boot animation", millis() - t,
                ESP.getFreeHeap() / 1024);
  display::setBacklight(0);
  screens::begin();
  display::fadeBacklight(storage::backlightLevel(), SCREEN_FADE_MS);
  sys::startWatchdog();   // from here on the loop must never stop for WATCHDOG_S
}

void loop() {
  uint32_t now = millis();
  sys::update(now);
  input::update(now);
  leds::update(now);
  storage::update(now);
  timekeeper::update(now);
  pclink::update(now);
  pet::update(now);
  media::update(now);
  night::update(now);
  InputEvent e;
  while (input::next(e)) screens::handle(e);
  screens::update(now);
  delay(5);
}
