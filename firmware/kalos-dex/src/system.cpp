#include "system.h"
#include <esp_task_wdt.h>
#include "config.h"

// Survives a crash / watchdog restart (not a power cut): counts crashes in a row. Magic = "these values are real".
RTC_NOINIT_ATTR static uint32_t crashMagic, crashCount;
static constexpr uint32_t CRASH_MAGIC = 0x4B445843;   // "KDXC"

static bool safe = false, crashed = false, cleared = false, wdtOn = false;
static const char *why = "", *reasonText = "?";

namespace sys {

void begin() {
  const esp_reset_reason_t r = esp_reset_reason();
  switch (r) {
    case ESP_RST_POWERON: reasonText = "power on"; break;
    case ESP_RST_EXT: reasonText = "reset pin"; break;
    case ESP_RST_SW: reasonText = "software restart"; break;
    case ESP_RST_PANIC: reasonText = "CRASH (panic)"; break;
    case ESP_RST_INT_WDT: reasonText = "CRASH (interrupt watchdog)"; break;
    case ESP_RST_TASK_WDT: reasonText = "CRASH (loop watchdog)"; break;
    case ESP_RST_WDT: reasonText = "CRASH (watchdog)"; break;
    case ESP_RST_BROWNOUT: reasonText = "CRASH (power dip / brown-out)"; break;
    default: reasonText = "other"; break;
  }
  crashed = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
  if (crashMagic != CRASH_MAGIC || r == ESP_RST_POWERON) { crashMagic = CRASH_MAGIC; crashCount = 0; }
  if (crashed) crashCount++;
  else crashCount = 0;

  // Back held right now (input::begin() set the pull-up; give it a moment to settle)
  delay(20);
  const bool backHeld = digitalRead(PIN_BTN_SCREEN) == LOW;
  if (backHeld) { safe = true; why = "Back held at start"; }
  else if (crashCount >= SAFE_AFTER_CRASHES) { safe = true; why = "crashes in a row"; }

  Serial.printf("start reason: %s (crashes in a row: %lu)\n", reasonText, (unsigned long)crashCount);
  if (safe) Serial.printf("*** SAFE MODE: %s - no boot animation, lights off. Unplug + replug to leave. ***\n", why);
}

void startWatchdog() {
  esp_task_wdt_init(WATCHDOG_S, true);   // true = restart when it bites
  esp_task_wdt_add(nullptr);             // watch the loop task
  wdtOn = true;
  Serial.printf("watchdog: on (%u s)\n", WATCHDOG_S);
}

void update(uint32_t now) {
  if (wdtOn) esp_task_wdt_reset();
  if (!cleared && now > CRASH_CLEAR_MS) { cleared = true; crashCount = 0; }   // ran fine for a while
}

bool safeMode() { return safe; }
const char *safeReason() { return why; }
const char *resetReason() { return reasonText; }
bool lastWasCrash() { return crashed; }

}  // namespace sys
