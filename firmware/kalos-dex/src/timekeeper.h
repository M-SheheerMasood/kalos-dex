// Time: DS1307 RTC (keeps time while unplugged, coin cell) + time from the PC over USB (pclink).
// The RTC stores UTC; the ESP32's own clock runs in UTC and shows Pakistan time (TZ_POSIX in config.h).
#pragma once
#include <Arduino.h>
#include <time.h>

namespace timekeeper {
enum class Source : uint8_t { None, Rtc, Pc };   // where the current time came from

void begin();                        // I2C, read the RTC, start the ESP32 clock from it
void update(uint32_t now);           // call every loop (re-checks the RTC now and then)
bool valid();                        // false = no idea what time it is (show "--:--")
time_t nowUtc();
bool localTime(struct tm &out);      // Pakistan time; false if not valid
Source source();
const char *sourceName();
// the PC sent the exact time: set the ESP32 clock + the RTC; returns how far off the RTC was (s)
long setFromPc(time_t utc);

// info / test screen
enum class RtcState : uint8_t { Missing, Stopped, Ok };   // Stopped = lost power / never set
RtcState rtcState();
const char *rtcStateName();
bool rtcRead(time_t &utc);           // raw RTC read (UTC)
bool hasLastDrift();
long lastDrift();                    // RTC minus PC time at the last sync, seconds
uint32_t lastPcSyncMs();             // millis() of the last PC sync (0 = never)
}  // namespace timekeeper
