#include "storage.h"
#include <Preferences.h>
#include <nvs.h>
#include <rom/crc.h>

// One stored copy of a record: header + the data + CRC over both.
struct Header { uint32_t magic; uint16_t version; uint16_t size; uint32_t seq; };
static constexpr uint32_t MAGIC = 0x4B414C53;      // "KALS"
static constexpr uint16_t SETTINGS_VERSION = 1;
static constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 3000;   // wait for the knob to stop turning

static Preferences prefs;
static Settings current;
static storage::Slot setSlot;
static bool dirty = false;
static uint32_t changedAt = 0, savedAt = 0;

namespace storage {

// Reads both slots of `key` and returns the newest valid one (false if none).
bool loadRecord(const char *key, uint16_t version, void *data, uint16_t size, Slot &io) {
  uint32_t &outSeq = io.seq; char &outSlot = io.slot;
  uint8_t buf[sizeof(Header) + 256 + 4];
  bool found = false;
  for (char s : {'A', 'B'}) {
    char k[12];
    snprintf(k, sizeof k, "%s%c", key, s);
    if (!prefs.isKey(k)) continue;   // slot never written yet (avoids a library error line in the log)
    size_t len = prefs.getBytesLength(k);
    if (len != sizeof(Header) + size + 4 || len > sizeof buf) continue;
    prefs.getBytes(k, buf, len);
    Header h; memcpy(&h, buf, sizeof h);
    uint32_t crcStored; memcpy(&crcStored, buf + sizeof h + size, 4);
    if (h.magic != MAGIC || h.version != version || h.size != size) continue;
    if (crc32_le(0, buf, sizeof h + size) != crcStored) { Serial.printf("storage: %s slot %c damaged, ignored\n", key, s); continue; }
    if (!found || h.seq > outSeq) {
      found = true; outSeq = h.seq; outSlot = s;
      memcpy(data, buf + sizeof h, size);
    }
  }
  return found;
}

// Writes into the slot NOT holding the newest copy.
bool saveRecord(const char *key, uint16_t version, const void *data, uint16_t size, Slot &io) {
  uint32_t &ioSeq = io.seq; char &ioSlot = io.slot;
  uint8_t buf[sizeof(Header) + 256 + 4];
  if (size > 256) return false;
  Header h{MAGIC, version, size, ioSeq + 1};
  memcpy(buf, &h, sizeof h);
  memcpy(buf + sizeof h, data, size);
  uint32_t crc = crc32_le(0, buf, sizeof h + size);
  memcpy(buf + sizeof h + size, &crc, 4);
  char s = (ioSlot == 'A') ? 'B' : 'A';
  char k[12];
  snprintf(k, sizeof k, "%s%c", key, s);
  if (prefs.putBytes(k, buf, sizeof h + size + 4) != sizeof h + size + 4) return false;
  ioSeq = h.seq; ioSlot = s;
  return true;
}

void begin() {
  prefs.begin("kalos", false, "kalos");   // our own 64 KB partition (partitions.csv)
  if (!loadRecord("set", SETTINGS_VERSION, &current, sizeof current, setSlot)) {
    Serial.println("storage: no saved settings yet, using defaults");
    current = Settings();
    setSlot = Slot();
  }
  current.bootCount++;
  saveNow();   // record this boot straight away
  Serial.printf("storage: boot #%lu, settings seq %lu in slot %c\n", (unsigned long)current.bootCount, (unsigned long)setSlot.seq, setSlot.slot);
}

void update(uint32_t now) {
  if (dirty && now - changedAt > SETTINGS_SAVE_DELAY_MS) saveNow();
}

Settings &settings() { return current; }
void settingsChanged() { dirty = true; changedAt = millis(); }

void saveNow() {
  if (saveRecord("set", SETTINGS_VERSION, &current, sizeof current, setSlot)) {
    dirty = false; savedAt = millis();
    Serial.printf("storage: settings saved (seq %lu, slot %c)\n", (unsigned long)setSlot.seq, setSlot.slot);
  } else {
    Serial.println("storage: SAVE FAILED");
  }
}

void resetSettings() {
  uint32_t boots = current.bootCount;
  current = Settings();
  current.bootCount = boots;
  saveNow();
}

uint32_t settingsSeq() { return setSlot.seq; }
char settingsSlot() { return setSlot.slot; }
uint32_t lastSaveMs() { return savedAt; }
void nvsUsage(size_t &used, size_t &total) {
  nvs_stats_t st;
  if (nvs_get_stats("kalos", &st) == ESP_OK) { used = st.used_entries; total = st.total_entries; }
  else { used = total = 0; }
}

}  // namespace storage
