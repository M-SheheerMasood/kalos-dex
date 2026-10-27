// Kalos Dex – pins and tunables (docs/pinout.md). Change things here, not in the code.
#pragma once
#include <Arduino.h>

// ---- pins ----
constexpr int PIN_BACKLIGHT = 5;     // display LED pin, PWM: higher = brighter
constexpr int PIN_KNOB_CLK = 32;
constexpr int PIN_KNOB_DT = 33;
constexpr int PIN_KNOB_SW = 25;      // knob press (pressed = LOW)
constexpr int PIN_BTN_SCREEN = 26;   // Back button (next screen / hold = Home), pressed = LOW
constexpr int PIN_BTN_FEED = 27;
constexpr int PIN_BTN_PLAY = 14;
// (G35 Play/Pause button dropped 2026-10-19 – play/pause moves to the knob. G15 and G35 are free.)
constexpr int PIN_STRIP = 13;
constexpr int PIN_LED_GREEN = 4;
constexpr int PIN_LED_YELLOW = 19;
constexpr int PIN_LED_RED = 2;
constexpr int PIN_I2C_SDA = 21;
constexpr int PIN_I2C_SCL = 22;

// ---- time ----
#define TZ_POSIX "PKT-5"                  // Pakistan, UTC+5, no daylight saving
// 12 h / 24 h is a setting now (Settings screen, storage::clock24h())

// ---- display ----
constexpr uint8_t DISPLAY_ROTATION = 2;   // portrait, header pins at the top (as in the case)
constexpr uint8_t DISPLAY_VCOM = 0x98;    // ILI9341 VCOM offset (0xC7), picked by the user at the desk angle 2026-10-27:
                                          // the TN panel's pet shadow + light greys show, only the 2 darkest greys merge
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr int SAFE_MARGIN = 4;            // the panel border hides ~2 px; keep content >= 4 px in
constexpr int TOP_INSET = 12;             // on the desk the case's top lip hides ~9 rows (user, Edges ruler 2026-10-23): keep text/icons below this
constexpr uint8_t BACKLIGHT_NORMAL = 160; // 0..255 (~63 %): full brightness heats the display board = the MAX in Settings
constexpr uint8_t BACKLIGHT_MIN = 16;     // Settings can dim down to 10 % of that
constexpr int BACKLIGHT_PWM_CHANNEL = 0;
constexpr uint16_t SCREEN_FADE_MS = 90;   // backlight dip used to hide screen changes
constexpr uint16_t BOOT_CLOSED_MS = 600;  // boot: closed Pokédex shown before it opens
constexpr uint16_t BOOT_OPEN_MS = 2200;   // boot: how long the halves take to slide apart (user: slower, twice)

// ---- buttons ----
constexpr uint16_t DEBOUNCE_MS = 30;
constexpr uint16_t LONG_PRESS_MS = 700;
constexpr uint16_t DOUBLE_MS = 300;       // only for buttons with double-press switched on

// ---- lights ----
constexpr int STRIP_LEN = 16;             // as built 2026-10-22: LEDs 1-6 left channel, 7-10 hidden top run, 11-16 right channel
constexpr uint32_t STRIP_VISIBLE_DEFAULT = 0xFC3F;   // bits 0-5 + 10-15 = LEDs 1-6 and 11-16 visible, 7-10 always off
constexpr uint8_t STRIP_BRIGHTNESS = 60;  // default 0..255, before the limiter
// power budget (USB 2 port = 500 mA), estimates on the safe side (measured on the breadboard, re-checked closed: ~449 mA max)
constexpr int TOTAL_BUDGET_MA = 450;
constexpr int ESP32_MA = 120;             // ESP32 running (no Wi-Fi: the PC link is over USB)
constexpr int DISPLAY_LOGIC_MA = 15;
constexpr int BACKLIGHT_FULL_MA = 100;
constexpr float IDLE_MA_PER_LED = 1.0f;
constexpr float MA_PER_CHANNEL = 20.0f;   // one colour channel of one LED at full

// ---- pet (F6) – stats go 0..100; "hours" = how long a full stat takes to run out
constexpr float PET_HUNGER_HOURS = 14;       // food 100 -> 0 while awake (half as fast asleep)
constexpr float PET_SAD_HOURS = 20;          // fun 100 -> 0 while awake (twice as fast when hungry, paused asleep)
constexpr float PET_TIRED_HOURS = 18;        // energy 100 -> 0 while awake
constexpr float PET_REST_HOURS = 8;          // energy 0 -> 100 while asleep
constexpr int PET_SLEEP_FROM = 23;           // local hour it falls asleep ...
constexpr int PET_SLEEP_TO = 7;              // ... and wakes up (only when the time is known)
constexpr uint32_t PET_WAKE_MS = 10UL * 60 * 1000;   // a button press at night wakes it for 10 min
constexpr uint32_t PET_CATCHUP_MAX_S = 48UL * 3600;  // time switched off counts at most 48 h
constexpr float PET_CATCHUP_FLOOR = 10;      // time off never pushes a stat below this (it never dies)
constexpr uint32_t PET_EGG_SECONDS = 3600;   // egg hatches after 1 h switched on (music hatching comes with F7)
constexpr uint8_t PET_SHINY_ONE_IN = 20;
constexpr uint32_t PET_ANGRY_AFTER_S = 24UL * 3600;   // no accepted feed / play this long (real time) -> angry
constexpr uint8_t PET_CALM_TRIES = 3;                 // angry: the 3rd counted try is accepted ...
constexpr uint32_t PET_CALM_TRY_MS = 60UL * 1000;     // ... and a try counts only this long after the last one
constexpr uint32_t PET_SAVE_EVERY_MS = 3UL * 60 * 1000;   // + 3 s after every feed / play
// actions
constexpr float PET_FEED_FOOD = 30, PET_FEED_FUN = 3, PET_FULL_AT = 90;   // refuses food at >= 90
constexpr float PET_PLAY_FUN = 15, PET_PLAY_ENERGY = 8, PET_PLAY_FOOD = 3, PET_TOO_TIRED = 15;
// XP: level n -> n+1 needs PET_XP_BASE + PET_XP_STEP * n
constexpr uint32_t PET_XP_BASE = 10, PET_XP_STEP = 5;
constexpr uint32_t PET_XP_FEED = 5;          // only when food was below 70 (no XP farming)
constexpr uint32_t PET_XP_PLAY = 8;          // only when fun was below 90
constexpr uint32_t PET_XP_CONTENT_S = 600;   // +1 XP per 10 min awake with every stat >= 50
constexpr uint8_t PET_MAX_LEVEL = 100;

// ---- PC link + media (F7)
constexpr uint32_t PC_ONLINE_MS = 4000;      // no message from the companion this long = "PC offline"
constexpr int KNOB_VOL_DIR = -1;            // flip to 1 if turning the knob changes the volume the wrong way
constexpr int MEDIA_VOL_PER_CLICK = 2;       // shown at once; the companion uses the same step
constexpr uint32_t MEDIA_VOL_SEND_MS = 80;   // knob clicks are bundled: at most one KEY VOL line per 80 ms
constexpr uint32_t MEDIA_LOCAL_VOL_MS = 800;
constexpr uint32_t VOL_POPUP_MS = 1500;
constexpr uint32_t MUTE_PULSE_MS = 2400;     // mute badge: one slow fade in + out
constexpr uint8_t MUTE_ALPHA_MAX = 190;      // never fully solid (255): the screen behind stays visible      // volume pop-up (top band, every screen but Music) stays this long // after a knob turn, ignore the PC's (older) volume this long

// ---- safety
constexpr uint32_t WATCHDOG_S = 8;             // loop stuck this long -> restart (longest normal block: ~0.1 s)
constexpr uint32_t SAFE_AFTER_CRASHES = 3;     // this many crash restarts in a row -> safe mode
constexpr uint32_t CRASH_CLEAR_MS = 60000;     // running this long counts as "fine": crash count back to 0

// ---- night dimming (Settings: on/off) – the user picked these levels with the Settings screen
constexpr int NIGHT_FROM_MIN = 0 * 60 + 15;  // 00:15 local ...
constexpr int NIGHT_TO_MIN = 7 * 60;         // ... until 07:00
constexpr int NIGHT_SCREEN_PCT = 40;         // screen at most 40 % (same scale as Settings)
constexpr int NIGHT_LIGHTS_PCT = 10;         // strip at most 10 %
constexpr uint16_t NIGHT_RAMP_MS = 2500;     // how long the change takes (smooth, never blocks)
