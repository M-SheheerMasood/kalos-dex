#include "screen.h"
#include "display.h"
#include "ui.h"
#include "storage.h"
#include "home_screen.h"
#include "clock_screen.h"
#include "music_screen.h"
#include "settings_screen.h"
#include "dex_screen.h"
#include "debug_screen.h"
#include "pet.h"
#include "media.h"
#include "night.h"

static HomeScreen homeScreen;
static ClockScreen clockScreen;
static MusicScreen musicScreen;
static SettingsScreen settingsScreen;
static DexScreen dexScreen;
static DebugScreen debugScreen;   // only while Debug is on (Settings page 2)
#ifdef HWTEST   // hardware-test build: the test screens come after the real ones
#include "../hwtest/test_screens.h"
static ClockTestScreen clockTestScreen;
static InputScreen inputScreen;
static LightsScreen lightsScreen;
static EdgeScreen edgeScreen;
static InfoScreen infoScreen;
static DisplayScreen displayScreen;
static GlowScreen glowScreen;
static Screen *order[] = {&homeScreen, &dexScreen, &clockScreen, &musicScreen, &settingsScreen, &debugScreen, &glowScreen, &displayScreen, &clockTestScreen, &inputScreen, &lightsScreen, &edgeScreen, &infoScreen};
#else
static Screen *order[] = {&homeScreen, &dexScreen, &clockScreen, &musicScreen, &settingsScreen, &debugScreen};
#endif
static constexpr int COUNT = sizeof(order) / sizeof(order[0]);
static int currentIdx = 0;
static uint32_t shownAt = 0;   // when the current screen finished appearing (Back is ignored until + SCREEN_LOCK_MS)

// ---- overlays on every screen (not Music, it shows the volume itself), in the top band:
// volume pop-up for VOL_POPUP_MS after the volume changes; a blinking mute badge in the top-right corner.
// While the pop-up is up, the screen below draws through a clip window that leaves the band alone.
static constexpr int BADGE = 24, BADGE_X = SCREEN_W - SAFE_MARGIN - BADGE - 2, BADGE_Y = TOP_MID_Y - BADGE / 2 - 6;   // 6 rows above centre: user picked by eye (step 09), top row 10 still shows under the lip
static uint32_t popupUntil = 0, shownVolVer = 0, lastBadge = 0;
static constexpr uint32_t BADGE_FRAME_MS = 40;
static bool popupUp = false, badgeUp = false;

static void speaker(TFT_eSPI &g, int x, int y, uint16_t c) {   // little speaker, 12 x 14, top-left at x, y
  g.fillRect(x, y + 4, 4, 6, c);
  g.fillTriangle(x + 3, y + 7, x + 10, y, x + 10, y + 14, c);
}

static void drawPopup() {
  tft.resetViewport();
  const int v = media::volume();
  const bool m = media::muted();
  tft.fillRect(0, 0, SCREEN_W, TOP_BAND_H, col::SCREEN_DARK);
  speaker(tft, SAFE_MARGIN + 8, TOP_MID_Y - 7, col::WHITE);
  if (m) { tft.drawLine(SAFE_MARGIN + 22, TOP_MID_Y - 6, SAFE_MARGIN + 30, TOP_MID_Y + 6, col::RED); tft.drawLine(SAFE_MARGIN + 30, TOP_MID_Y - 6, SAFE_MARGIN + 22, TOP_MID_Y + 6, col::RED); }
  const int bx = 48, bw = SCREEN_W - bx - 54;
  ui::hpBar(tft, bx, TOP_MID_Y - 6, bw, 12, v < 0 ? 0 : v / 100.0f, m ? col::SILVER : 0x3C7F);
  char buf[8];
  snprintf(buf, sizeof buf, m ? "MUTE" : v < 0 ? "--" : "%d", v);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(m ? col::RED : col::WHITE, col::SCREEN_DARK);
  ui::text(tft, buf, SCREEN_W - SAFE_MARGIN - 4, TOP_MID_Y + 1, ui::BODY);
  tft.setViewport(0, TOP_BAND_H, SCREEN_W, SCREEN_H - TOP_BAND_H, false);   // screen below can't touch the band
}

// Mute badge, top-right corner, on top of whatever is there (nothing moves for it). It fades in and out slowly
// over the pixels behind it. The display can't be read back (no MISO), so screens hand their corner pixels over
// while they draw: screens::blendCorner() keeps a copy of them ("behind") and blends the badge in. Screens
// that never call it have a plain red title bar there, so "behind" starts as red.
static TFT_eSprite badge(&tft);              // 24 x 24, 1.2 KB, made once: the frame that gets pushed
static uint16_t behind[BADGE * BADGE];       // what the screen has under the badge (1.2 KB)
static uint16_t badgeImg[BADGE * BADGE];     // the badge itself (1.2 KB)
static uint8_t badgeAlpha = 0;               // 0 = invisible ... 255 = solid
static constexpr uint16_t BADGE_CLEAR = 0xF81F;   // magenta: "not part of the badge" (never used in it)
static bool behindInit = [] { for (uint16_t &p : behind) p = col::RED; return true; }();

static bool makeBadge() {
  if (badge.created()) return true;
  badge.setColorDepth(16);
  if (!badge.createSprite(BADGE, BADGE)) return false;
  // style A (user's pick 2026-10-28): dark rounded card, 2 px black rim, white speaker, red cross; outside the
  // rounded corners = BADGE_CLEAR (shows what's behind)
  badge.fillSprite(BADGE_CLEAR);
  badge.fillRoundRect(0, 0, BADGE, BADGE, 6, col::BLACK);
  badge.fillRoundRect(2, 2, BADGE - 4, BADGE - 4, 5, col::SCREEN_DARK);
  badge.fillRect(4, 10, 4, 4, col::WHITE);                                   // speaker: box + cone
  badge.fillTriangle(7, 10, 12, 6, 12, 17, col::WHITE);
  badge.fillTriangle(7, 10, 7, 13, 12, 17, col::WHITE);
  for (int o = 0; o < 2; o++) {                                              // red cross, 2 px lines
    badge.drawLine(14 + o, 9, 20 + o, 15, 0xE945);
    badge.drawLine(14 + o, 15, 20 + o, 9, 0xE945);
  }
  for (int y = 0; y < BADGE; y++) for (int x = 0; x < BADGE; x++) badgeImg[y * BADGE + x] = badge.readPixel(x, y);
  return true;
}

static void pushBadge() {   // behind + badge at badgeAlpha
  if (!makeBadge()) return;
  for (int y = 0; y < BADGE; y++)
    for (int x = 0; x < BADGE; x++) {
      const int i = y * BADGE + x;
      const bool clear = badgeImg[i] == BADGE_CLEAR || !badgeAlpha;
      badge.drawPixel(x, y, clear ? behind[i] : tft.alphaBlend(badgeAlpha, badgeImg[i], behind[i]));
    }
  badge.pushSprite(BADGE_X, BADGE_Y);
}

static void overlays(Screen *scr, uint32_t now) {
  const bool here = !scr->showsVolume();
  if (media::volumeVersion() != shownVolVer) {
    const bool first = shownVolVer == 0 || media::volume() < 0;   // first value from the PC: no pop-up
    shownVolVer = media::volumeVersion();
    if (here && !first) { popupUntil = now + VOL_POPUP_MS; popupUp = true; drawPopup(); }
  }
  if (popupUp && (!here || (int32_t)(now - popupUntil) >= 0)) {
    popupUp = false;
    tft.resetViewport();
    if (here) scr->repaintTop();
  }
  // mute badge: fade up, hold at the peak, fade down, hold at 0 (user, 2026-10-28); never fully solid
  const bool wantBadge = here && media::muted() && !popupUp;
  if (wantBadge && now - lastBadge >= BADGE_FRAME_MS) {
    lastBadge = now;
    const uint32_t t = now % (2 * (MUTE_FADE_MS + MUTE_HOLD_MS));
    float a = 0;
    if (t < MUTE_FADE_MS) a = t / float(MUTE_FADE_MS);
    else if (t < MUTE_FADE_MS + MUTE_HOLD_MS) a = 1;
    else if (t < 2 * MUTE_FADE_MS + MUTE_HOLD_MS) a = 1 - (t - MUTE_FADE_MS - MUTE_HOLD_MS) / float(MUTE_FADE_MS);
    badgeAlpha = MUTE_ALPHA_MAX * (0.5f - 0.5f * cosf(a * PI));   // eased in and out
    pushBadge();
    badgeUp = true;
  } else if (!wantBadge && badgeUp) {
    badgeUp = false;
    badgeAlpha = 0;
    if (!popupUp && here) pushBadge();   // put the saved pixels back
  }
}

// Hide the redraw behind a quick backlight dip, so a screen never appears half-drawn.
static void show(int i) {
  const uint8_t level = display::backlight() ? display::backlight() : night::backlightTarget();
  display::fadeBacklight(0, SCREEN_FADE_MS);
  order[currentIdx]->leave();
  if (popupUp) { popupUp = false; tft.resetViewport(); }   // a new screen draws everything
  badgeUp = false; badgeAlpha = 0;
  for (uint16_t &p : behind) p = col::RED;   // until the new screen hands its corner over
  currentIdx = i;
  const uint32_t t = millis();
  order[currentIdx]->enter();
  Serial.printf("screen -> %s (drawn in %lu ms)  RAM free %u KB, biggest block %u KB\n", order[currentIdx]->name(),
                millis() - t, ESP.getFreeHeap() / 1024, heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
  display::fadeBacklight(level, SCREEN_FADE_MS);
  shownAt = millis();
}

namespace screens {
void begin() {
  input::enableDouble(Btn::Knob, true);   // knob: press = play/pause, double = mute (everywhere)
  currentIdx = 0;
  order[0]->enter();
  Serial.printf("screen -> %s  RAM free %u KB, biggest block %u KB\n", order[0]->name(), ESP.getFreeHeap() / 1024,
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
}
void next() {
  int i = (currentIdx + 1) % COUNT;
  if (order[i] == &debugScreen && !pet::testMode()) i = (i + 1) % COUNT;   // Debug off: skipped
  show(i);
}
void home() { if (currentIdx != 0) show(0); }
void update(uint32_t now) {
  order[currentIdx]->update(now);
  overlays(order[currentIdx], now);
}
void repaintTop() { order[currentIdx]->repaintTop(); }

void blendCorner(TFT_eSprite &spr, int sx, int sy) {
  const int x0 = max(BADGE_X, sx), x1 = min(BADGE_X + BADGE, sx + (int)spr.width());
  const int y0 = max(BADGE_Y, sy), y1 = min(BADGE_Y + BADGE, sy + (int)spr.height());
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      const int i = (y - BADGE_Y) * BADGE + (x - BADGE_X);
      behind[i] = spr.readPixel(x - sx, y - sy);
      if (badgeAlpha && !popupUp) spr.drawPixel(x - sx, y - sy, tft.alphaBlend(badgeAlpha, badgeImg[i], behind[i]));
    }
}
void handle(const InputEvent &e) {
  // the screen gets the first look (the input monitor wants to show everything)
  const bool used = order[currentIdx]->onEvent(e);
  // Back pressed while a screen was switching (queued) or right after: ignored, no accidental double skip (user,
  // 2026-10-31). Holding Back for Home is a separate event and always works.
  if (e.type == Ev::Press && e.button == Btn::Screen) { if (millis() - shownAt >= SCREEN_LOCK_MS) next(); }
  else if (e.type == Ev::Hold && e.button == Btn::Screen) home();
  else if (used) return;
  // the knob is the PC's media knob on every screen (unless a test screen uses it)
  else if (e.type == Ev::Turn) media::volumeClicks(storage::knobVolDir() * e.steps);
  else if (e.type == Ev::Press && e.button == Btn::Knob) media::playPause();
  else if (e.type == Ev::Double && e.button == Btn::Knob) media::toggleMute();
}
Screen *current() { return order[currentIdx]; }
}  // namespace screens
