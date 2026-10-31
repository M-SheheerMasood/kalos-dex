// Saving to flash (NVS) that survives a power cut at any moment:
// every record is kept in TWO slots (A/B) with a sequence number and a CRC32. A save always
// overwrites the OLDER slot, so if power dies mid-write the newer good copy is still there.
#pragma once
#include <Arduino.h>
#include "config.h"

struct Settings {
  uint8_t stripBrightness = STRIP_BRIGHTNESS;
  uint8_t backlight = BACKLIGHT_NORMAL;
  uint8_t hatchMin = 0;                           // egg hatch time in minutes, 0 = PET_HATCH_MIN_DEFAULT (was the unused watchFace)
  uint8_t flags = 0;                              // SET_* bits below (was "reserved": same layout, old saves load)
  uint32_t visibleMask = STRIP_VISIBLE_DEFAULT;   // one bit per strip LED: 1 = visible (LED 1 = bit 0)
  uint32_t bootCount = 0;
};

constexpr uint8_t SET_24H = 1;            // clock shows 24 h instead of 12 h
constexpr uint8_t SET_NO_NIGHT_DIM = 4;   // night dimming switched OFF (so it is on by default)
constexpr uint8_t SET_KNOB_FLIP = 2;      // knob the other way round (volume + Settings values; from KNOB_VOL_DIR)
constexpr uint8_t SET_REMEMBER_PAGE = 8;  // screens open where they were left (off = at their first page / row)

namespace storage {
// any record (settings, pet, ...): two copies + CRC, see above. size <= 256 bytes.
struct Slot { uint32_t seq = 0; char slot = '-'; };   // where the newest copy of a record is
bool loadRecord(const char *key, uint16_t version, void *data, uint16_t size, Slot &io);
bool saveRecord(const char *key, uint16_t version, const void *data, uint16_t size, Slot &io);

void begin();                       // loads everything (defaults if nothing valid is stored)
void update(uint32_t now);          // saves changed records when it's a good moment
Settings &settings();
void settingsChanged();             // call after changing settings(); saved a few seconds later
void saveNow();                     // force (e.g. before a reset)
void resetSettings();
// test / info
uint32_t settingsSeq();
char settingsSlot();                // 'A', 'B' or '-' (nothing stored yet)
uint32_t lastSaveMs();
void nvsUsage(size_t &used, size_t &total);

// settings, ready to use
inline bool clock24h() { return settings().flags & SET_24H; }
inline bool rememberPage() { return settings().flags & SET_REMEMBER_PAGE; }
inline int knobVolDir() { return settings().flags & SET_KNOB_FLIP ? -KNOB_VOL_DIR : KNOB_VOL_DIR; }
inline uint32_t hatchSeconds() { return (settings().hatchMin ? settings().hatchMin : PET_HATCH_MIN_DEFAULT) * 60UL; }
inline uint8_t backlightLevel() { return constrain(settings().backlight, BACKLIGHT_MIN, BACKLIGHT_NORMAL); }
}  // namespace storage
