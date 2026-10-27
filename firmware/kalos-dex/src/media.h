// Media: what the PC companion says is playing (track, play state, position, volume) + the keys the gadget
// sends back. Protocol (one text line each, over USB) – PLAN §5:
//   PC -> gadget:  TRACK <title>\t<artist>   STATE playing|paused|stopped <pos s> <dur s>   VOL <0-100> <muted 0|1>
//   gadget -> PC:  KEY PLAYPAUSE | KEY NEXT | KEY PREV | KEY MUTE | KEY VOL <+n/-n knob clicks>
#pragma once
#include <Arduino.h>

namespace media {
enum class State : uint8_t { Stopped, Paused, Playing };

const char *title();          // "" = nothing playing
const char *artist();
State state();
uint32_t positionS();         // counts on by itself between PC updates while playing
uint32_t durationS();         // 0 = unknown (no progress bar)
int volume();                 // 0..100, -1 = not known yet
bool muted();
uint32_t trackVersion();      // goes up when the title / artist changes
uint32_t stateVersion();      // ... play state, length or a jump in position (not the normal 1 s tick)
uint32_t volumeVersion();     // ... volume or mute

void update(uint32_t now);    // call every loop: sends the bundled knob clicks

// keys (sent to the PC; volume + mute also change the local copy at once so the screen feels instant)
void playPause();
void next();
void prev();
void toggleMute();
void volumeClicks(int clicks);
const char *lastKey();        // for a short "sent" highlight on screen
uint32_t lastKeyAt();

// from pclink
void onTrack(char *args);
void onState(char *args);
void onVolume(char *args);
}  // namespace media
