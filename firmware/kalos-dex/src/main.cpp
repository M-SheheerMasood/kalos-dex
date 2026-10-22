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
#include "screens/screen.h"

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
  display::begin();
  leds::begin();   // after the display: re-claims GPIO 19 from the SPI driver (yellow LED)
  storage::begin();
  timekeeper::begin();
  leds::setBrightness(storage::settings().stripBrightness);
  leds::setVisibleMask(storage::settings().visibleMask);
  Serial.print("strip: visible map ");
  for (int i = 0; i < STRIP_LEN; i++) Serial.print(leds::isVisible(i) ? '1' : '0');
  Serial.println("  (LED 1 first)");
  pet::begin();   // after storage + time: loads the pet and catches up on the time it was switched off
  uint32_t t = millis();
  bootAnimation();
  Serial.printf("boot animation: %lu ms, free RAM after: %u KB\n", millis() - t, ESP.getFreeHeap() / 1024);
  display::setBacklight(0);
  screens::begin();
  display::fadeBacklight(BACKLIGHT_NORMAL, SCREEN_FADE_MS);
}

void loop() {
  uint32_t now = millis();
  input::update(now);
  leds::update(now);
  storage::update(now);
  timekeeper::update(now);
  pclink::update(now);
  pet::update(now);
  InputEvent e;
  while (input::next(e)) screens::handle(e);
  screens::update(now);
  delay(5);
}
