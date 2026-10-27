#include "pclink.h"
#include "timekeeper.h"
#include "leds.h"
#include "pet.h"
#include "pet_art.h"
#include "media.h"
#include "night.h"
#include "display.h"
#include "config.h"

#ifdef HWTEST
static bool powerTest = false;
static uint32_t lastPowerLog = 0;
#endif

static char line[200];   // TRACK lines carry a title + artist
static uint32_t lastPcMs = 0;   // last message that came from the companion
static size_t len = 0;

static void run(char *cmd) {
  if (strncmp(cmd, "TRACK ", 6) == 0 || strcmp(cmd, "TRACK") == 0) { media::onTrack(cmd[5] ? cmd + 6 : cmd + 5); lastPcMs = millis(); }
  else if (strncmp(cmd, "STATE ", 6) == 0) { media::onState(cmd + 6); lastPcMs = millis(); }
  else if (strncmp(cmd, "VOL ", 4) == 0) { media::onVolume(cmd + 4); lastPcMs = millis(); }
  else if (strncmp(cmd, "HELLO", 5) == 0) { lastPcMs = millis(); Serial.printf("OK HELLO %s\n", FW_VERSION); }
  else if (strncmp(cmd, "TIME ", 5) == 0) {
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
  } else if (strncmp(cmd, "NIGHT", 5) == 0) {   // NIGHT ON / OFF / AUTO, or just NIGHT = show
    if (!strcmp(cmd, "NIGHT ON")) night::force(1);
    else if (!strcmp(cmd, "NIGHT OFF")) night::force(0);
    else if (!strcmp(cmd, "NIGHT AUTO")) night::force(-1);
    Serial.printf("OK NIGHT %s  backlight now %u (target %u)  strip target %u\n", night::active() ? "on" : "off",
                  display::backlight(), night::backlightTarget(), night::stripTarget());
  } else if (strcmp(cmd, "CRASH") == 0) {   // test the watchdog: hang the loop -> restart after WATCHDOG_S
    Serial.println("OK CRASH - hanging the loop now");
    Serial.flush();
    while (true) {}
  } else if (strcmp(cmd, "PANIC") == 0) {   // test a real crash
    Serial.println("OK PANIC");
    Serial.flush();
    abort();
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
    else if (strncmp(a, "STAGE ", 6) == 0) pet::setStage(atoi(a + 6));                    // PET STAGE <1-3>
    else if (strncmp(a, "SHINY ", 6) == 0) pet::setShiny(atoi(a + 6));                    // PET SHINY <0|1>
    else if (strncmp(a, "NOCARE ", 7) == 0) pet::setCareHoursAgo(strtoul(a + 7, nullptr, 10));   // PET NOCARE <hours>
    else if (strncmp(a, "SCALE ", 6) == 0) {                                                 // PET SCALE <20..45> = 2.0x..4.5x
      if (!petart::setScale(pet::stage(), atoi(a + 6))) { Serial.println("ERR PET SCALE: hatched + 20..45"); return; }
    }
    else if (strcmp(a, "RESET") == 0) pet::reset();
    else if (strcmp(a, "FEED") == 0) petart::reactTo(pet::feed(), petart::Reaction::Feed, millis());   // + reaction on Home
    else if (strcmp(a, "PLAY") == 0) petart::reactTo(pet::play(), petart::Reaction::Play, millis());
    else if (strncmp(a, "SET ", 4) == 0 && a[4] && a[5] == ' ') pet::setStat(a[4], atof(a + 6));   // PET SET f|u|e <0..100>
    else { Serial.println("ERR PET: SKIP <min> | XP <n> | HATCH | STAGE <1-3> | SHINY <0|1> | NOCARE <h> | SCALE <20-45> | RESET | FEED | PLAY | SET f|u|e <v>"); return; }
    pet::printStatus();
#endif
  } else if (strcmp(cmd, "PING") == 0) {
    Serial.printf("OK PONG %s\n", FW_VERSION);
  } else if (*cmd) {
    Serial.printf("ERR unknown '%s'\n", cmd);
  }
}

namespace pclink {
bool online() { return lastPcMs && millis() - lastPcMs < PC_ONLINE_MS; }

void update(uint32_t now) {
  static uint32_t lastLed = 0;   // green LED = PC companion connected (re-checked once a second)
  if (now - lastLed >= 1000) { lastLed = now; leds::setStatus(leds::Status::Green, online() ? leds::Mode::On : leds::Mode::Off); }
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
