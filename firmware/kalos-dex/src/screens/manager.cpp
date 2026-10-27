#include "screen.h"
#include "display.h"
#include "ui.h"
#include "storage.h"
#include "home_screen.h"
#include "clock_screen.h"
#include "music_screen.h"
#include "settings_screen.h"
#include "dex_screen.h"
#include "media.h"
#include "night.h"

static HomeScreen homeScreen;
static ClockScreen clockScreen;
static MusicScreen musicScreen;
static SettingsScreen settingsScreen;
static DexScreen dexScreen;
#ifdef HWTEST   // hardware-test build: the test screens come after the real ones
#include "../hwtest/test_screens.h"
static ClockTestScreen clockTestScreen;
static InputScreen inputScreen;
static LightsScreen lightsScreen;
static EdgeScreen edgeScreen;
static InfoScreen infoScreen;
static DisplayScreen displayScreen;
static Screen *order[] = {&homeScreen, &dexScreen, &clockScreen, &musicScreen, &settingsScreen, &displayScreen, &clockTestScreen, &inputScreen, &lightsScreen, &edgeScreen, &infoScreen};
#else
static Screen *order[] = {&homeScreen, &dexScreen, &clockScreen, &musicScreen, &settingsScreen};
#endif
static constexpr int COUNT = sizeof(order) / sizeof(order[0]);
static int currentIdx = 0;

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
static bool behindInit = [] { for (uint16_t &p : behind) p = col::RED; return true; }();

static bool makeBadge() {
  if (badge.created()) return true;
  badge.setColorDepth(16);
  if (!badge.createSprite(BADGE, BADGE)) return false;
  badge.fillSprite(col::SCREEN_DARK);
  badge.drawRect(0, 0, BADGE, BADGE, col::WHITE);
  speaker(badge, 3, 5, col::WHITE);
  badge.drawLine(15, 8, 21, 16, col::RED); badge.drawLine(16, 8, 22, 16, col::RED);
  badge.drawLine(21, 8, 15, 16, col::RED); badge.drawLine(22, 8, 16, 16, col::RED);
  for (int y = 0; y < BADGE; y++) for (int x = 0; x < BADGE; x++) badgeImg[y * BADGE + x] = badge.readPixel(x, y);
  return true;
}

static void pushBadge() {   // behind + badge at badgeAlpha
  if (!makeBadge()) return;
  for (int y = 0; y < BADGE; y++)
    for (int x = 0; x < BADGE; x++) {
      const int i = y * BADGE + x;
      badge.drawPixel(x, y, badgeAlpha ? tft.alphaBlend(badgeAlpha, badgeImg[i], behind[i]) : behind[i]);
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
  // mute badge: slow fade in / out (never fully solid, so what's behind shows through)
  const bool wantBadge = here && media::muted() && !popupUp;
  if (wantBadge && now - lastBadge >= BADGE_FRAME_MS) {
    lastBadge = now;
    const float ph = (now % MUTE_PULSE_MS) / float(MUTE_PULSE_MS);
    badgeAlpha = MUTE_ALPHA_MAX * (0.5f - 0.5f * cosf(ph * TWO_PI));
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
}

namespace screens {
void begin() {
  input::enableDouble(Btn::Knob, true);   // knob: press = play/pause, double = mute (everywhere)
  currentIdx = 0;
  order[0]->enter();
  Serial.printf("screen -> %s  RAM free %u KB, biggest block %u KB\n", order[0]->name(), ESP.getFreeHeap() / 1024,
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
}
void next() { show((currentIdx + 1) % COUNT); }
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
  if (e.type == Ev::Press && e.button == Btn::Screen) next();
  else if (e.type == Ev::Hold && e.button == Btn::Screen) home();
  else if (used) return;
  // the knob is the PC's media knob on every screen (unless a test screen uses it)
  else if (e.type == Ev::Turn) media::volumeClicks(storage::knobVolDir() * e.steps);
  else if (e.type == Ev::Press && e.button == Btn::Knob) media::playPause();
  else if (e.type == Ev::Double && e.button == Btn::Knob) media::toggleMute();
}
Screen *current() { return order[currentIdx]; }
}  // namespace screens
