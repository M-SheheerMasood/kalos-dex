#include "timekeeper.h"
#include <Wire.h>
#include <sys/time.h>
#include "config.h"

static constexpr uint8_t RTC_ADDR = 0x68;
static constexpr time_t EARLIEST = 1760000000;   // 2025-10-09: anything earlier = not set

static timekeeper::Source src = timekeeper::Source::None;
static timekeeper::RtcState rtc = timekeeper::RtcState::Missing;
static bool driftKnown = false;
static long drift = 0;
static uint32_t pcSyncMs = 0, lastRtcCheck = 0;

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// days since 1970-01-01 for a UTC date (no time-zone business, unlike mktime)
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

static void setSystem(time_t utc) {
  timeval tv{utc, 0};
  settimeofday(&tv, nullptr);
}

namespace timekeeper {

bool rtcRead(time_t &utc) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  if (Wire.endTransmission() != 0) { rtc = RtcState::Missing; return false; }
  if (Wire.requestFrom(RTC_ADDR, (uint8_t)7) != 7) { rtc = RtcState::Missing; return false; }
  uint8_t r[7];
  for (auto &b : r) b = Wire.read();
  if (r[0] & 0x80) { rtc = RtcState::Stopped; return false; }   // CH bit: oscillator halted
  int hour;
  if (r[2] & 0x40) {   // 12 h mode (we always write 24 h, but who knows what was there)
    hour = bcd2bin(r[2] & 0x1F) % 12 + ((r[2] & 0x20) ? 12 : 0);
  } else {
    hour = bcd2bin(r[2] & 0x3F);
  }
  const int year = 2000 + bcd2bin(r[6]), mon = bcd2bin(r[5] & 0x1F), day = bcd2bin(r[4] & 0x3F);
  const int min = bcd2bin(r[1] & 0x7F), sec = bcd2bin(r[0] & 0x7F);
  if (mon < 1 || mon > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59) { rtc = RtcState::Stopped; return false; }
  utc = (time_t)daysFromCivil(year, mon, day) * 86400 + hour * 3600 + min * 60 + sec;
  if (utc < EARLIEST) { rtc = RtcState::Stopped; return false; }   // running, but never set properly
  rtc = RtcState::Ok;
  return true;
}

static bool rtcWrite(time_t utc) {
  struct tm t;
  gmtime_r(&utc, &t);
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  Wire.write(bin2bcd(t.tm_sec) & 0x7F);    // CH = 0: oscillator runs
  Wire.write(bin2bcd(t.tm_min));
  Wire.write(bin2bcd(t.tm_hour));          // bit 6 = 0: 24 h mode
  Wire.write(bin2bcd(t.tm_wday + 1));
  Wire.write(bin2bcd(t.tm_mday));
  Wire.write(bin2bcd(t.tm_mon + 1));
  Wire.write(bin2bcd(t.tm_year % 100));
  if (Wire.endTransmission() != 0) { rtc = RtcState::Missing; return false; }
  rtc = RtcState::Ok;
  return true;
}

void begin() {
  setenv("TZ", TZ_POSIX, 1);
  tzset();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);   // DS1307: 100 kHz max
  Wire.setTimeOut(20);
  // I2C scan in the log: expect 0x50 (EEPROM on the RTC board) and 0x68 (DS1307)
  Serial.print("I2C devices:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); n++; }
  }
  Serial.println(n ? "" : " none (check SDA/SCL/pull-ups/power)");
  time_t t;
  if (rtcRead(t)) {
    setSystem(t);
    src = Source::Rtc;
    struct tm lt; localtime_r(&t, &lt);
    Serial.printf("RTC: ok, %04d-%02d-%02d %02d:%02d:%02d (Pakistan time)\n", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec);
  } else {
    Serial.printf("RTC: %s - waiting for the PC to send the time\n", rtcStateName());
  }
  lastRtcCheck = millis();
}

void update(uint32_t now) {
  // RTC unplugged/replugged or set later: look again every 10 s while we have no time
  if (src != Source::None || now - lastRtcCheck < 10000) return;
  lastRtcCheck = now;
  time_t t;
  if (rtcRead(t)) { setSystem(t); src = Source::Rtc; Serial.println("RTC: found, time taken from it"); }
}

long setFromPc(time_t utc) {
  time_t r;
  driftKnown = rtcRead(r);
  drift = driftKnown ? (long)(r - utc) : 0;
  setSystem(utc);
  const bool ok = rtcWrite(utc);
  src = Source::Pc;
  pcSyncMs = millis();
  Serial.printf("time from PC; RTC %s%s\n", ok ? "set" : "NOT set (missing?)",
                driftKnown ? (String(", was off by ") + drift + " s").c_str() : "");
  return drift;
}

bool valid() { return src != Source::None; }
time_t nowUtc() { return time(nullptr); }
bool localTime(struct tm &out) {
  if (!valid()) return false;
  time_t t = time(nullptr);
  localtime_r(&t, &out);
  return true;
}
Source source() { return src; }
const char *sourceName() { return src == Source::Pc ? "PC" : src == Source::Rtc ? "RTC" : "none"; }
RtcState rtcState() { return rtc; }
const char *rtcStateName() { return rtc == RtcState::Ok ? "ok" : rtc == RtcState::Stopped ? "not set / lost power" : "not found"; }
bool hasLastDrift() { return driftKnown; }
long lastDrift() { return drift; }
uint32_t lastPcSyncMs() { return pcSyncMs; }

}  // namespace timekeeper
