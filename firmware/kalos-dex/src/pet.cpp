#include "pet.h"
#include "config.h"
#include "pet_line.h"
#include "storage.h"
#include "timekeeper.h"
#include "leds.h"

// What goes to flash. Stats are stored x100 so slow decay isn't lost to rounding. Never reorder:
// add fields only into `spare` (and keep the size), or bump PET_VERSION (old saves would be dropped).
struct PetSave {
  uint8_t line, stage, level, flags;           // flags bit 0 = shiny
  uint16_t food, fun, energy, reserved;
  uint32_t xp, eggSec, contentSec;
  uint32_t savedUtc;                           // when this copy was written (0 = time unknown)
  uint32_t hatchedUtc, feeds, plays;
  uint8_t spare[8];
};
static_assert(sizeof(PetSave) == 48, "PetSave layout changed");
static constexpr uint16_t PET_VERSION = 1;

static PetSave p;                       // integer fields live here; the stats as floats below
static float sFood, sFun, sEnergy, eggSec, contentSec;
static storage::Slot slot;
static bool dirty = false, glowHeld = false, isAsleep = false;
static uint32_t changedAt = 0, savedAt = 0, lastTick = 0, wokeAt = 0, ver = 1;
static pet::Mood curMood = pet::Mood::Egg;
static const char *msg = "";
static uint32_t msgAt = 0;
static int shown[4] = {-1, -1, -1, -1}; // whole-number stats + egg minutes last shown (redraw when they change)

static uint32_t need(uint8_t lvl) { return PET_XP_BASE + PET_XP_STEP * lvl; }
static void say(const char *m) { msg = m; msgAt = millis(); ver++; Serial.printf("pet: %s\n", m); }
static void changed() { dirty = true; changedAt = millis(); ver++; }

static bool nightAt(time_t utc) {
  struct tm lt;
  localtime_r(&utc, &lt);
  return PET_SLEEP_FROM > PET_SLEEP_TO ? (lt.tm_hour >= PET_SLEEP_FROM || lt.tm_hour < PET_SLEEP_TO)
                                       : (lt.tm_hour >= PET_SLEEP_FROM && lt.tm_hour < PET_SLEEP_TO);
}

// lower a stat, but never below `floor` (unless it already was)
static void drop(float &v, float by, float floor) { v = max(v - by, min(v, floor)); }

// Let `sec` seconds pass. timeKnown = false: no day/night, so energy is left alone.
static void simulate(float sec, bool asleep, bool timeKnown, float floor) {
  if (!p.stage) return;   // eggs don't get hungry
  const float h = sec / 3600;
  if (asleep) {
    drop(sFood, 100 / PET_HUNGER_HOURS * h * 0.5f, floor);
    sEnergy = min(100.f, sEnergy + 100 / PET_REST_HOURS * h);
  } else {
    drop(sFood, 100 / PET_HUNGER_HOURS * h, floor);
    drop(sFun, 100 / PET_SAD_HOURS * h * (sFood < 25 ? 2 : 1), floor);
    if (timeKnown) drop(sEnergy, 100 / PET_TIRED_HOURS * h, floor);
  }
}

static pet::Mood calcMood() {
  using M = pet::Mood;
  if (!p.stage) return M::Egg;
  if (isAsleep) return M::Sleeping;
  if (sFood < 25) return M::Hungry;
  if (sEnergy < PET_TOO_TIRED) return M::Tired;
  if (sFun < 30) return M::Sad;
  if (sFood >= 60 && sFun >= 60 && sEnergy >= 60) return M::Happy;
  return M::Okay;
}

// mood -> glow (PLAN §3b) + red LED = "needs you"
static void applyGlow() {
  if (glowHeld) return;
  using E = leds::Effect;
  switch (curMood) {
    case pet::Mood::Egg:      leds::setEffect(E::Breathe, 0xFFE8C0, 5000); break;   // warm white
    case pet::Mood::Sleeping: leds::setEffect(E::Solid, 0x000C40); break;          // dim night blue
    case pet::Mood::Hungry:   leds::setEffect(E::Pulse, 0xFF7000, 1500); break;    // yellow-orange pulse
    case pet::Mood::Tired:    leds::setEffect(E::Breathe, 0x8040A0, 6000); break;  // soft purple
    case pet::Mood::Sad:      leds::setEffect(E::Breathe, 0x2050FF, 6000); break;  // soft blue
    default:                  leds::setEffect(E::Breathe, 0xFFB040, 4000); break;  // day: warm yellow
  }
  const bool needsYou = curMood == pet::Mood::Hungry || curMood == pet::Mood::Sad;
  leds::setStatus(leds::Status::Red, needsYou ? leds::Mode::On : leds::Mode::Off);
}

static void refreshMood() {
  const pet::Mood m = calcMood();
  if (m == curMood) return;
  Serial.printf("pet: mood %s -> %s\n", pet::moodName(curMood), pet::moodName(m));
  curMood = m;
  ver++;
  applyGlow();
}

static void save() {
  p.food = lroundf(sFood * 100); p.fun = lroundf(sFun * 100); p.energy = lroundf(sEnergy * 100);
  p.eggSec = eggSec; p.contentSec = contentSec;
  p.savedUtc = timekeeper::valid() ? timekeeper::nowUtc() : 0;
  if (storage::saveRecord("pet", PET_VERSION, &p, sizeof p, slot)) { dirty = false; savedAt = millis(); }
  else Serial.println("pet: SAVE FAILED");
}

static void newEgg() {
  memset(&p, 0, sizeof p);
  p.line = PET_LINE.id;
  sFood = sFun = sEnergy = 100;
  eggSec = contentSec = 0;
}

static void hatch() {
  p.stage = 1; p.level = 1; p.xp = 0;
  p.flags = (esp_random() % PET_SHINY_ONE_IN == 0) ? 1 : 0;
  p.hatchedUtc = timekeeper::valid() ? timekeeper::nowUtc() : 0;
  sFood = 80; sFun = 80; sEnergy = 100;
  leds::flash(0xFFFFFF, 1500);
  say(p.flags & 1 ? "It hatched... a SHINY!" : "It hatched!");
  changed();
}

static void gainXp(uint32_t n) {
  if (!p.stage || p.level >= PET_MAX_LEVEL) return;
  p.xp += n;
  while (p.level < PET_MAX_LEVEL && p.xp >= need(p.level)) {
    p.xp -= need(p.level);
    p.level++;
    leds::flashRainbow(1500);
    say("Level up!");
    if (p.stage < 3 && p.level >= PET_LINE.evolveAt[p.stage - 1]) {
      p.stage++;
      leds::flash(0xFFFFFF, 2000);
      say("What? It's evolving!");
    }
  }
  if (p.level >= PET_MAX_LEVEL) p.xp = 0;
  changed();
}

// time switched off (or "skipped" in a test): minute by minute so sleep hours count right
static void catchUp(time_t from, uint32_t seconds) {
  const bool known = from != 0;
  seconds = min(seconds, PET_CATCHUP_MAX_S);
  for (uint32_t done = 0; done < seconds; done += 60) {
    const uint32_t step = min<uint32_t>(60, seconds - done);
    simulate(step, known && nightAt(from + done), known, PET_CATCHUP_FLOOR);
  }
}

namespace pet {

void begin() {
  if (storage::loadRecord("pet", PET_VERSION, &p, sizeof p, slot)) {
    sFood = p.food / 100.f; sFun = p.fun / 100.f; sEnergy = p.energy / 100.f;
    eggSec = p.eggSec; contentSec = p.contentSec;
    Serial.printf("pet: loaded (seq %lu, slot %c)\n", (unsigned long)slot.seq, slot.slot);
    if (p.savedUtc && timekeeper::valid() && timekeeper::nowUtc() > (time_t)p.savedUtc) {
      const uint32_t off = timekeeper::nowUtc() - p.savedUtc;
      catchUp(p.savedUtc, off);
      Serial.printf("pet: was off for %lu h %lu min (counted up to %lu h)\n", (unsigned long)(off / 3600),
                    (unsigned long)(off / 60 % 60), (unsigned long)(PET_CATCHUP_MAX_S / 3600));
    }
  } else {
    Serial.println("pet: nothing saved yet - a new egg");
    newEgg();
    save();
  }
  isAsleep = false;
  curMood = calcMood();
  applyGlow();
  lastTick = millis();
  printStatus();
}

void update(uint32_t now) {
  if (now - lastTick < 1000) return;
  const float sec = (now - lastTick) / 1000.f;
  lastTick = now;

  const bool known = timekeeper::valid();
  const bool woken = wokeAt && now - wokeAt < PET_WAKE_MS;
  const bool sleepNow = p.stage && known && nightAt(timekeeper::nowUtc()) && !woken;
  if (sleepNow != isAsleep) { isAsleep = sleepNow; say(sleepNow ? "Zzz..." : "Good morning!"); }

  if (!p.stage) {
    eggSec += sec;
    if (eggSec >= PET_EGG_SECONDS) hatch();
  } else {
    simulate(sec, isAsleep, known, 0);
    if (!isAsleep && sFood >= 50 && sFun >= 50 && sEnergy >= 50) {   // content: a little XP over time
      contentSec += sec;
      if (contentSec >= PET_XP_CONTENT_S) { contentSec -= PET_XP_CONTENT_S; gainXp(1); }
    }
  }
  refreshMood();

  const int nowShown[4] = {(int)sFood, (int)sFun, (int)sEnergy, (int)(eggSecondsLeft() / 60)};
  if (memcmp(nowShown, shown, sizeof shown)) { memcpy(shown, nowShown, sizeof shown); ver++; }

  if ((dirty && now - changedAt > 3000) || now - savedAt > PET_SAVE_EVERY_MS) save();
}

void wake() { if (isAsleep || nightAt(timekeeper::nowUtc())) wokeAt = millis(); }

Result feed() {
  if (!p.stage) { say("It's still an egg."); return Result::Egg; }
  wake();
  if (sFood >= PET_FULL_AT) { say("I'm full!"); return Result::Full; }
  if (sFood < 70) gainXp(PET_XP_FEED);
  sFood = min(100.f, sFood + PET_FEED_FOOD);
  sFun = min(100.f, sFun + PET_FEED_FUN);
  p.feeds++;
  leds::flash(0x30FF60, 600);
  say("Yum!");
  changed();
  isAsleep = false; refreshMood();
  return Result::Ok;
}

Result play() {
  if (!p.stage) { say("It's still an egg."); return Result::Egg; }
  wake();
  if (sEnergy < PET_TOO_TIRED) { say("Too tired..."); return Result::TooTired; }
  if (sFun < 90) gainXp(PET_XP_PLAY);
  sFun = min(100.f, sFun + PET_PLAY_FUN);
  sEnergy = max(0.f, sEnergy - PET_PLAY_ENERGY);
  sFood = max(0.f, sFood - PET_PLAY_FOOD);
  p.plays++;
  leds::flashRainbow(800);
  say("Yay!");
  changed();
  isAsleep = false; refreshMood();
  return Result::Ok;
}

Mood mood() { return curMood; }
const char *moodName(Mood m) {
  static const char *N[] = {"Egg", "Sleeping", "Hungry", "Tired", "Sad", "Happy", "Okay"};
  return N[(int)m];
}
bool hatched() { return p.stage > 0; }
uint8_t stage() { return p.stage; }
const char *name() { return p.stage ? PET_LINE.names[p.stage - 1] : "Egg"; }
bool shiny() { return p.flags & 1; }
bool asleep() { return isAsleep; }
float food() { return sFood; }
float fun() { return sFun; }
float energy() { return sEnergy; }
uint8_t level() { return p.level; }
uint32_t xp() { return p.xp; }
uint32_t xpNeeded() { return need(p.level); }
uint32_t eggSecondsLeft() { return p.stage ? 0 : PET_EGG_SECONDS - min<uint32_t>(eggSec, PET_EGG_SECONDS); }
const char *message() { return msg; }
uint32_t messageAt() { return msgAt; }
uint32_t version() { return ver; }

void holdGlow(bool hold) {
  glowHeld = hold;
  if (!hold) applyGlow();
}

void reset() {
  newEgg();
  isAsleep = false; wokeAt = 0;
  curMood = calcMood();
  applyGlow();
  say("A new egg!");
  save();
}

void printStatus() {
  Serial.printf("PET %s%s stage %u  Lv %u (xp %lu/%lu)  food %.1f fun %.1f energy %.1f  mood %s  feeds %lu plays %lu",
                name(), shiny() ? " (shiny)" : "", p.stage, p.level, (unsigned long)p.xp, (unsigned long)need(p.level),
                sFood, sFun, sEnergy, moodName(curMood), (unsigned long)p.feeds, (unsigned long)p.plays);
  if (!p.stage) Serial.printf("  egg hatches in %lu min", eggSecondsLeft() / 60);
  Serial.printf("  saved seq %lu slot %c\n", (unsigned long)slot.seq, slot.slot);
}

void skipMinutes(uint32_t minutes) {
  catchUp(timekeeper::valid() ? timekeeper::nowUtc() : 0, minutes * 60);
  refreshMood();
  changed();
}
void addXp(uint32_t n) { gainXp(n); }
void hatchNow() { if (!p.stage) hatch(); refreshMood(); }
void setStat(char which, float v) {
  v = constrain(v, 0.f, 100.f);
  if (which == 'f') sFood = v; else if (which == 'u') sFun = v; else if (which == 'e') sEnergy = v;
  refreshMood();
  changed();
}

}  // namespace pet
