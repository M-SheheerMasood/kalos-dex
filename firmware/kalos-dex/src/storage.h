// Saving to flash (NVS) that survives a power cut at any moment:
// every record is kept in TWO slots (A/B) with a sequence number and a CRC32. A save always
// overwrites the OLDER slot, so if power dies mid-write the newer good copy is still there.
#pragma once
#include <Arduino.h>
#include "config.h"

struct Settings {
  uint8_t stripBrightness = STRIP_BRIGHTNESS;
  uint8_t backlight = BACKLIGHT_NORMAL;
  uint8_t watchFace = 0;
  uint8_t reserved = 0;
  uint32_t visibleMask = STRIP_VISIBLE_DEFAULT;   // one bit per strip LED: 1 = visible (LED 1 = bit 0)
  uint32_t bootCount = 0;
};

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
}  // namespace storage
