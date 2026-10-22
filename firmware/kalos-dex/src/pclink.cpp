#include "pclink.h"
#include "timekeeper.h"
#include "leds.h"
#include "pet.h"

#ifdef HWTEST
static bool powerTest = false;
static uint32_t lastPowerLog = 0;
#endif

static char line[96];
static size_t len = 0;

static void run(char *cmd) {
  if (strncmp(cmd, "TIME ", 5) == 0) {
    char *end;
    long long t = strtoll(cmd + 5, &end, 10);
    if (end == cmd + 5 || t < 1700000000LL || t > 4102444800LL) { Serial.println("ERR TIME bad value"); return; }
    long d = timekeeper::setFromPc((time_t)t);
    Serial.printf("OK TIME drift=%ld rtc=%s\n", d, timekeeper::hasLastDrift() ? "was-set" : "was-not-set");
#ifdef HWTEST
  } else if (strcmp(cmd, "POWERTEST ON") == 0) {   // worst case: strip full white (the limiter caps it)
    leds::setEffect(leds::Effect::Solid, 0xFFFFFF);
    leds::setBrightness(255);
    powerTest = true; lastPowerLog = 0;
    Serial.println("OK POWERTEST ON");
  } else if (strcmp(cmd, "POWERTEST OFF") == 0) {
    leds::setEffect(leds::Effect::Off);
    powerTest = false;
    Serial.println("OK POWERTEST OFF");
#endif
  } else if (strcmp(cmd, "PET") == 0) {
    pet::printStatus();
#ifdef HWTEST
  } else if (strncmp(cmd, "PET ", 4) == 0) {   // pet test commands
    const char *a = cmd + 4;
    if (strncmp(a, "SKIP ", 5) == 0) pet::skipMinutes(strtoul(a + 5, nullptr, 10));     // PET SKIP <minutes>
    else if (strncmp(a, "XP ", 3) == 0) pet::addXp(strtoul(a + 3, nullptr, 10));        // PET XP <n>
    else if (strcmp(a, "HATCH") == 0) pet::hatchNow();
    else if (strcmp(a, "RESET") == 0) pet::reset();
    else if (strcmp(a, "FEED") == 0) pet::feed();
    else if (strcmp(a, "PLAY") == 0) pet::play();
    else if (strncmp(a, "SET ", 4) == 0 && a[4] && a[5] == ' ') pet::setStat(a[4], atof(a + 6));   // PET SET f|u|e <0..100>
    else { Serial.println("ERR PET: SKIP <min> | XP <n> | HATCH | RESET | FEED | PLAY | SET f|u|e <v>"); return; }
    pet::printStatus();
#endif
  } else if (strcmp(cmd, "PING") == 0) {
    Serial.printf("OK PONG %s\n", FW_VERSION);
  } else if (*cmd) {
    Serial.printf("ERR unknown '%s'\n", cmd);
  }
}

namespace pclink {
void update(uint32_t now) {
#ifdef HWTEST
  if (powerTest && now - lastPowerLog >= 30000) {
    lastPowerLog = now;
    Serial.printf("power test: %lu s up, strip ~%d mA, whole gadget ~%d mA, free RAM %u KB\n", now / 1000,
                  leds::stripMilliamps(), leds::totalMilliamps(), ESP.getFreeHeap() / 1024);
  }
#endif
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { line[len] = 0; run(line); len = 0; continue; }
    if (len < sizeof line - 1) line[len++] = c;
    else len = 0;   // far too long: not for us, drop it
  }
}
}  // namespace pclink
