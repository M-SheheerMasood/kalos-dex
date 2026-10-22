// The virtual pet (F6): food / fun / energy (0..100), XP + levels, evolves, sleeps at night, keeps living
// while switched off (capped catch-up from the RTC), never dies. Owns the mood glow + the red status LED.
// Saved with storage's A/B + CRC records: every 3 min and 3 s after each feed / play.
#pragma once
#include <Arduino.h>

namespace pet {
enum class Mood : uint8_t { Egg, Sleeping, Hungry, Tired, Sad, Happy, Okay };
enum class Result : uint8_t { Ok, Full, TooTired, Egg };

void begin();                  // after storage + timekeeper: load, catch up on the time switched off
void update(uint32_t now);     // call every loop

Result feed();
Result play();
void wake();                   // any button at night: awake for PET_WAKE_MS

Mood mood();
const char *moodName(Mood m);
bool hatched();
uint8_t stage();               // 0 = egg, 1..3
const char *name();            // species name of the current stage ("Egg" before hatching)
bool shiny();
bool asleep();
float food(), fun(), energy();
uint8_t level();
uint32_t xp(), xpNeeded();     // XP inside the current level / needed for the next
uint32_t eggSecondsLeft();
const char *message();         // last thing that happened ("Yum!", "I'm full!", ...) – "" when none
uint32_t messageAt();          // millis() of that message
uint32_t version();            // goes up whenever anything shown on screen changes

void holdGlow(bool hold);      // test screens borrow the strip + status LEDs; false = give them back
void reset();                  // new egg (Settings later: hold Feed + Play 3 s)
void printStatus();            // serial "PET"

// testing (serial commands in the hwtest build)
void skipMinutes(uint32_t minutes);   // pretend it was switched off this long (same catch-up as at boot)
void addXp(uint32_t n);
void hatchNow();
void setStat(char which, float v);    // 'f' food, 'u' fun, 'e' energy
}  // namespace pet
