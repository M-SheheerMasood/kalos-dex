#include "boot.h"
#include "display.h"
#include "storage.h"

// The two red halves are drawn once into off-screen sprites, then slid apart frame by frame.
// The screen behind them (background + logo) is one more sprite. 8-bit colour keeps the three
// sprites at ~150 KB, which is fine at boot (nothing else is running yet); all are freed at the end.
static constexpr int HALF_H = SCREEN_H / 2;
static constexpr int RING_R = 64;      // white rim outer radius
static constexpr int BAND_R = 60;      // black band outer radius
static constexpr int INNER_R = 44;     // screen visible inside the band
static constexpr int OPEN_PX = 98;     // how far each half slides
static constexpr float GROOVE_R1 = 71, GROOVE_R2 = 97, GROOVE_HALF_W = 1.5f;   // corner rings (3 px wide)

// One half of the closed dex. `top` = true: the arch (circle centre) is on its bottom edge.
static void drawHalf(TFT_eSprite &s, bool top) {
  const int cy = top ? HALF_H : 0;      // circle centre on the edge where the halves meet
  s.fillSprite(col::RED);
  // rounded outer corners
  const int r = 18;
  s.fillRect(0, top ? 0 : HALF_H - r, r, r, col::BLACK);
  s.fillRect(SCREEN_W - r, top ? 0 : HALF_H - r, r, r, col::BLACK);
  s.fillCircle(r, top ? r : HALF_H - r - 1, r, col::RED);
  s.fillCircle(SCREEN_W - r - 1, top ? r : HALF_H - r - 1, r, col::RED);
  // two curved decoration grooves near the outer corners, like the prop: 3 px black rings (a 1 px dark-red
  // line nearly vanished in 8-bit colour). Per pixel, so the ring has no gaps.
  const int cyCorner = top ? 0 : HALF_H;
  for (int y = 0; y < HALF_H; y++)
    for (int x = 0; x < SCREEN_W; x++)
      for (int cx : {0, SCREEN_W - 1}) {
        const float d = sqrtf(float((x - cx) * (x - cx) + (y - cyCorner) * (y - cyCorner)));
        if (fabsf(d - GROOVE_R1) <= GROOVE_HALF_W || fabsf(d - GROOVE_R2) <= GROOVE_HALF_W) s.drawPixel(x, y, col::BLACK);
      }
  // Pokéball half: white rim, black band, screen colour inside
  s.fillCircle(SCREEN_W / 2, cy, RING_R, col::SILVER);
  s.fillCircle(SCREEN_W / 2, cy, BAND_R, col::BLACK);
  s.fillCircle(SCREEN_W / 2, cy, INNER_R, col::SCREEN_BG);
  // silver edge line where the halves meet
  s.fillRect(0, top ? HALF_H - 3 : 0, SCREEN_W, 3, col::SILVER);
}

static void drawScreenBehind(TFT_eSprite &s) {
  s.fillSprite(col::SCREEN_BG);
  s.setTextDatum(MC_DATUM);
  s.setTextColor(col::RED_DARK);
  s.drawString("KALOS", SCREEN_W / 2, SCREEN_H / 2 - 22, 4);
  s.setTextColor(col::TEXT);
  s.drawString("DEX", SCREEN_W / 2, SCREEN_H / 2 + 8, 4);
  s.setTextColor(col::SCREEN_DARK);
  s.drawString(FW_VERSION, SCREEN_W / 2, SCREEN_H / 2 + 40, 2);
}

static float easeOut(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }

void bootAnimation() {
  TFT_eSprite top(&tft), bottom(&tft), behind(&tft);
  for (TFT_eSprite *s : {&top, &bottom, &behind}) s->setColorDepth(8);
  bool ok = top.createSprite(SCREEN_W, HALF_H) && bottom.createSprite(SCREEN_W, HALF_H) &&
            behind.createSprite(SCREEN_W, SCREEN_H);
  if (!ok) {  // not enough memory: show a plain boot screen instead of crashing
    Serial.println("boot: not enough RAM for the animation, plain screen instead");
    tft.fillScreen(col::SCREEN_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(col::TEXT, col::SCREEN_BG);
    tft.drawString("KALOS DEX", SCREEN_W / 2, SCREEN_H / 2, 4);
    display::fadeBacklight(storage::backlightLevel(), 300);
    delay(600);
  } else {
    drawHalf(top, true);
    drawHalf(bottom, false);
    drawScreenBehind(behind);

    // closed dex, fade the light up
    top.pushSprite(0, 0);
    bottom.pushSprite(0, HALF_H);
    display::fadeBacklight(storage::backlightLevel(), 350);
    delay(BOOT_CLOSED_MS);

    // slide the halves apart
    const uint32_t ms = BOOT_OPEN_MS, start = millis();
    while (true) {
      float t = min(1.0f, (millis() - start) / float(ms));
      int open = int(easeOut(t) * OPEN_PX);
      // screen strip that is visible between the halves
      int y0 = HALF_H - open, h = 2 * open;
      if (h > 0) behind.pushSprite(0, y0, 0, y0, SCREEN_W, h);
      top.pushSprite(0, -open);
      bottom.pushSprite(0, HALF_H + open);
      if (t >= 1) break;
    }

    // loading bar
    const int bx = 40, by = SCREEN_H / 2 + 62, bw = SCREEN_W - 80, bh = 8;
    tft.drawRoundRect(bx - 2, by - 2, bw + 4, bh + 4, 4, col::SCREEN_DARK);
    for (int x = 0; x <= bw; x += 4) {
      tft.fillRect(bx, by, x, bh, col::RED);
      delay(8);
    }
    delay(250);
  }
  top.deleteSprite();
  bottom.deleteSprite();
  behind.deleteSprite();
}
