#include "screen.h"
#include "display.h"
#include "home_screen.h"
#include "clock_screen.h"

static HomeScreen homeScreen;
static ClockScreen clockScreen;
#ifdef HWTEST   // hardware-test build: the test screens come after the real ones
#include "../hwtest/test_screens.h"
static ClockTestScreen clockTestScreen;
static InputScreen inputScreen;
static LightsScreen lightsScreen;
static EdgeScreen edgeScreen;
static InfoScreen infoScreen;
static Screen *order[] = {&homeScreen, &clockScreen, &clockTestScreen, &inputScreen, &lightsScreen, &edgeScreen, &infoScreen};
#else
static Screen *order[] = {&homeScreen, &clockScreen};
#endif
static constexpr int COUNT = sizeof(order) / sizeof(order[0]);
static int currentIdx = 0;

// Hide the redraw behind a quick backlight dip, so a screen never appears half-drawn.
static void show(int i) {
  const uint8_t level = display::backlight() ? display::backlight() : BACKLIGHT_NORMAL;
  display::fadeBacklight(0, SCREEN_FADE_MS);
  order[currentIdx]->leave();
  currentIdx = i;
  const uint32_t t = millis();
  order[currentIdx]->enter();
  Serial.printf("screen -> %s (drawn in %lu ms)  RAM free %u KB, biggest block %u KB\n", order[currentIdx]->name(),
                millis() - t, ESP.getFreeHeap() / 1024, heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
  display::fadeBacklight(level, SCREEN_FADE_MS);
}

namespace screens {
void begin() {
  currentIdx = 0;
  order[0]->enter();
  Serial.printf("screen -> %s  RAM free %u KB, biggest block %u KB\n", order[0]->name(), ESP.getFreeHeap() / 1024,
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
}
void next() { show((currentIdx + 1) % COUNT); }
void home() { if (currentIdx != 0) show(0); }
void update(uint32_t now) { order[currentIdx]->update(now); }
void handle(const InputEvent &e) {
  // the screen gets the first look (the input monitor wants to show everything)
  order[currentIdx]->onEvent(e);
  if (e.type == Ev::Press && e.button == Btn::Screen) next();
  else if (e.type == Ev::Hold && e.button == Btn::Screen) home();
}
Screen *current() { return order[currentIdx]; }
}  // namespace screens
