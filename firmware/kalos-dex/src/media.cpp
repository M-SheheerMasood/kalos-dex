#include "media.h"
#include "config.h"

static char sTitle[96] = "", sArtist[64] = "";
static media::State sState = media::State::Stopped;
static uint32_t pos = 0, posAt = 0, dur = 0, trackVer = 1, stateVer = 1, volVer = 1;
static int vol = -1;
static bool isMuted = false;
static uint32_t localVolAt = 0;           // the knob changed the volume: ignore the PC's old value for a moment
static int pendingClicks = 0;             // knob clicks not sent yet (bundled, see update())
static uint32_t lastVolSent = 0;
static const char *key = "";
static uint32_t keyAt = 0;

static void send(const char *k, const char *line) { Serial.println(line); key = k; keyAt = millis(); }

// copy, keeping only printable ASCII (the fonts have nothing else; the companion transliterates first)
static void copyText(char *dst, size_t n, const char *src) {
  size_t i = 0;
  for (; *src && i < n - 1; src++) dst[i++] = (*src >= 32 && *src < 127) ? *src : '?';
  dst[i] = 0;
}

namespace media {

const char *title() { return sTitle; }
const char *artist() { return sArtist; }
State state() { return sState; }
uint32_t positionS() {
  const uint32_t p = sState == State::Playing ? pos + (millis() - posAt) / 1000 : pos;
  return dur ? min(p, dur) : p;
}
uint32_t durationS() { return dur; }
int volume() { return vol; }
bool muted() { return isMuted; }
uint32_t trackVersion() { return trackVer; }
uint32_t stateVersion() { return stateVer; }
uint32_t volumeVersion() { return volVer; }

void playPause() {
  send("playpause", "KEY PLAYPAUSE");
  if (sState == State::Playing) { pos = positionS(); sState = State::Paused; stateVer++; }   // show it at once,
  else if (*sTitle) { posAt = millis(); sState = State::Playing; stateVer++; }               // the PC confirms
}
void next() { send("next", "KEY NEXT"); }
void prev() { send("prev", "KEY PREV"); }
void toggleMute() { send("mute", "KEY MUTE"); isMuted = !isMuted; localVolAt = millis(); volVer++; }
void volumeClicks(int clicks) {
  pendingClicks += clicks;   // sent by update(), at most every MEDIA_VOL_SEND_MS
  key = "vol"; keyAt = millis();
  if (vol >= 0) vol = constrain(vol + clicks * MEDIA_VOL_PER_CLICK, 0, 100);
  if (isMuted && clicks > 0) isMuted = false;   // Windows un-mutes when the volume goes up
  localVolAt = millis();
  volVer++;
}
void update(uint32_t now) {
  if (pendingClicks && now - lastVolSent >= MEDIA_VOL_SEND_MS) {
    Serial.printf("KEY VOL %+d\n", pendingClicks);
    pendingClicks = 0;
    lastVolSent = now;
    localVolAt = now;
  }
}
const char *lastKey() { return key; }
uint32_t lastKeyAt() { return keyAt; }

void onTrack(char *args) {
  char *tab = strchr(args, '\t');
  if (tab) *tab = 0;
  char t[sizeof sTitle], a[sizeof sArtist];
  copyText(t, sizeof t, args);
  copyText(a, sizeof a, tab ? tab + 1 : "");
  if (!strcmp(t, sTitle) && !strcmp(a, sArtist)) return;   // same song again: nothing to redraw
  strcpy(sTitle, t); strcpy(sArtist, a);
  trackVer++;
}

void onState(char *args) {
  char word[10] = "";
  unsigned long p = 0, d = 0;
  if (sscanf(args, "%9s %lu %lu", word, &p, &d) < 1) return;
  const State s = !strcmp(word, "playing") ? State::Playing : !strcmp(word, "paused") ? State::Paused : State::Stopped;
  // a position jump (seek / new song) or a state change is news; the normal 1 s tick is not
  const bool news = s != sState || d != dur || labs((long)p - (long)positionS()) > 2;
  sState = s; pos = p; posAt = millis(); dur = d;
  if (news) stateVer++;
}

void onVolume(char *args) {
  int v = -1, m = 0;
  if (sscanf(args, "%d %d", &v, &m) < 1 || v < 0 || v > 100) return;
  if (millis() - localVolAt < MEDIA_LOCAL_VOL_MS) return;   // our own change is still on its way
  if (v != vol || (m != 0) != isMuted) { vol = v; isMuted = m != 0; volVer++; }
}

}  // namespace media
