#include "clock_screen.h"
#include <math.h>
#include "display.h"
#include "timekeeper.h"

// ---- the scene is W x H "Minecraft pixels", each TEX x TEX screen pixels
static constexpr int TEX = 4;
static constexpr int W = SCREEN_W / TEX, H = SCREEN_H / TEX;   // 60 x 80
static constexpr float CX = W / 2.0f, CY = H / 2.0f;            // clock centre = screen centre
static constexpr int HORIZON = H / 2;                            // horizon at the centre: sun rises at 6, sets at 6
static constexpr float NUM_R = 21.5f;                           // numbers sit on this circle (even spacing)
static constexpr float ORBIT = 31;                              // sun / moon path: outside the numbers
static constexpr float BELOW = 0.33f;                           // sky below the horizon: this bright
static constexpr int BORDER = 4;                                 // gold frame thickness (Minecraft pixels)
static constexpr int STRIP_ROWS = 10;                            // pushed to the screen in strips of 10 rows (40 px)

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
struct RGB { float r, g, b; };
static RGB mix(RGB a, RGB b, float t) { t = constrain(t, 0.f, 1.f); return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static uint16_t to565(RGB c, float k = 1) { return rgb(c.r * k, c.g * k, c.b * k); }
static uint32_t hash(uint32_t x, uint32_t y) { uint32_t h = x * 374761393u + y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }

static const RGB DAY{120, 167, 255}, SUNSET{255, 132, 60}, NIGHT{12, 14, 44};
static uint16_t tex[W * H];

// clouds on the sky disc: angle from the sun (degrees, + = clockwise), distance, size (Minecraft pixels)
struct Cloud { int16_t ang; uint8_t r, w, h; };
static const Cloud CLOUDS[] = {{-50, 34, 8, 2}, {-25, 18, 6, 2}, {5, 40, 9, 3}, {20, 27, 5, 2}, {48, 36, 7, 2}, {-8, 46, 6, 2}, {35, 12, 4, 1}};
static constexpr int STAR_COUNT = 110;   // stars: made from a hash, all on the night half (100°..260° from the sun)

// 3x5 blocky digits (Minecraft-ish), one row per byte, bit 2 = left column
static const uint8_t GLYPH[10][5] = {{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
                                     {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};

static void put(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) tex[y * W + x] = c; }
static void putSky(int x, int y, RGB c) { put(x, y, to565(c)); }
static uint16_t dim565(uint16_t c, float k) {   // darken an already packed colour
  return rgb(((c >> 11) & 31) * 8.22f * k, ((c >> 5) & 63) * 4.05f * k, (c & 31) * 8.22f * k);
}
static void block(float cx, float cy, int w, int h, RGB c) {
  const int x0 = lroundf(cx - w / 2.f), y0 = lroundf(cy - h / 2.f);
  for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) putSky(x, y, c);
}

// the whole screen is the turning sky disc; below the horizon it is the same sky, very dim
static void drawScene(float rot) {
  const float sunUp = cosf(rot * DEG_TO_RAD);                               // 1 at noon, -1 at midnight
  const float dusk = max(0.f, 1 - fabsf(sunUp) / 0.4f);                     // sun near the horizon
  const float sunSide = sinf(rot * DEG_TO_RAD);                             // < 0: morning, sun on the left
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const float dx = x + 0.5f - CX, dy = y + 0.5f - CY;
      float phi = atan2f(dx, -dy) * RAD_TO_DEG - rot;
      while (phi > 180) phi -= 360;
      while (phi < -180) phi += 360;
      const float a = fabsf(phi);
      RGB c = a < 87 ? DAY : a > 93 ? NIGHT : (((x + y) & 1) ? DAY : NIGHT);   // day half / night half, dithered edge
      if (y < HORIZON) {                                                     // sunrise / sunset glow near the horizon
        const float vf = constrain(1 - (HORIZON - y - 1) / 12.f, 0.f, 1.f);
        const float hf = constrain(0.5f + 0.5f * (dx / CX) * (sunSide >= 0 ? 1 : -1), 0.f, 1.f);
        c = mix(c, SUNSET, floorf(dusk * vf * hf * 4) / 4 * 0.9f);
      }
      tex[y * W + x] = to565(c);
    }
  auto at = [&](float ang, float r, float &px, float &py) {
    const float t = (ang + rot) * DEG_TO_RAD;
    px = CX + r * sinf(t); py = CY - r * cosf(t);
  };
  float px, py;
  for (int i = 0; i < STAR_COUNT; i++) {
    const uint32_t h = hash(i, 77);
    at(100 + h % 160, 3 + (h >> 8) % 48, px, py);
    putSky((int)px, (int)py, i % 3 ? RGB{255, 255, 255} : RGB{190, 200, 255});
  }
  for (const Cloud &k : CLOUDS) {
    at(k.ang, k.r, px, py);
    block(px, py, k.w, k.h, {245, 245, 245});
    block(px + 1, py + 1, k.w - 2, 1, {215, 220, 235});
  }
  at(0, ORBIT, px, py);                                                       // sun
  block(px, py, 8, 8, {255, 214, 64});
  block(px, py, 6, 6, {255, 238, 120});
  block(px, py, 2, 2, {255, 252, 215});
  at(180, ORBIT, px, py);                                                     // moon with craters
  block(px, py, 7, 7, {222, 224, 205});
  putSky(lroundf(px - 2), lroundf(py - 1), {165, 168, 160});
  putSky(lroundf(px + 1), lroundf(py + 1), {165, 168, 160});
  putSky(lroundf(px + 2), lroundf(py - 2), {185, 187, 178});
  for (int y = HORIZON - 1; y < H; y++)
    for (int x = 0; x < W; x++) {
      const float k = y >= HORIZON ? BELOW : (y == HORIZON - 1 && (x & 1)) ? 0.66f : 1.f;   // dithered horizon row
      if (k < 1) tex[y * W + x] = dim565(tex[y * W + x], k);
    }
}

static void drawBorder() {   // thick gold frame: light on top/left, dark on bottom/right (bevel)
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const int d = min(min(x, y), min(W - 1 - x, H - 1 - y));
      if (d >= BORDER) continue;
      if ((x == 0 || x == W - 1) && (y == 0 || y == H - 1)) { tex[y * W + x] = rgb(16, 22, 40); continue; }  // blocky corner
      const bool topLeft = (x <= y && x <= W - 1 - x && x < H - 1 - y) || (y < x && y < W - 1 - x && y < H - 1 - y);
      uint16_t c;
      if (d == 0) c = rgb(120, 80, 10);
      else if (d == BORDER - 1) c = rgb(95, 62, 8);
      else if (d == 1) c = topLeft ? rgb(255, 236, 140) : rgb(190, 135, 25);
      else c = topLeft ? rgb(240, 200, 70) : rgb(215, 160, 35);
      if (d == 2 && hash(x, y) % 9 == 0) c = topLeft ? rgb(255, 250, 200) : rgb(235, 190, 60);   // glints
      tex[y * W + x] = c;
    }
}

static void drawNumber(int n, float cx, float cy) {   // gold digits with a full dark outline (stand out from stars)
  char s[3]; snprintf(s, sizeof s, "%d", n);
  const int len = strlen(s), w = len * 4 - 1, x0 = lroundf(cx - w / 2.f), y0 = lroundf(cy - 2.5f);
  for (int pass = 0; pass < 2; pass++)
    for (int k = 0; k < len; k++)
      for (int gy = 0; gy < 5; gy++)
        for (int gx = 0; gx < 3; gx++) {
          if (!(GLYPH[s[k] - '0'][gy] & (4 >> gx))) continue;
          const int X = x0 + k * 4 + gx, Y = y0 + gy;
          if (pass == 0) { for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) put(X + ox, Y + oy, rgb(25, 18, 8)); }
          else put(X, Y, gy < 2 ? rgb(255, 215, 70) : rgb(230, 170, 40));
        }
}

// sword hands: a small pixel-art sprite (rows from the pommel to the tip, 5 columns), turned by sampling
// it for every Minecraft pixel, so it stays blocky at any angle
struct SwordColours { uint16_t light, mid, shade; };
static void sword(float angDeg, int rows, SwordColours sc) {
  const float a = angDeg * DEG_TO_RAD, ux = sinf(a), uy = -cosf(a);
  const uint16_t OUT = rgb(20, 20, 25), HANDLE = rgb(120, 85, 45), POMMEL = rgb(85, 62, 35), GUARD = rgb(70, 52, 32);
  const int r = rows + 2;
  for (int y = max(0, (int)(CY - r)); y < min(H, (int)(CY + r)); y++)
    for (int x = max(0, (int)(CX - r)); x < min(W, (int)(CX + r)); x++) {
      const float px = x + 0.5f - CX, py = y + 0.5f - CY;
      const int row = (int)floorf(px * ux + py * uy + 2), col = lroundf(-px * uy + py * ux) + 2;
      if (row < 0 || row >= rows || col < 0 || col > 4) continue;
      const bool edge = (col == 0 || col == 4);
      uint16_t c;
      if (row == 0) { if (edge) continue; c = col == 2 ? POMMEL : OUT; }                       // pommel
      else if (row < 5) { if (edge) continue; c = col == 2 ? HANDLE : OUT; }                  // handle
      else if (row == 5) c = edge ? OUT : GUARD;                                                // cross-guard
      else if (row == rows - 2) { if (edge) continue; c = col == 2 ? sc.mid : OUT; }          // tip
      else if (row == rows - 1) { if (col != 2) continue; c = OUT; }
      else c = edge ? OUT : col == 1 ? sc.light : col == 2 ? sc.mid : sc.shade;               // blade
      tex[y * W + x] = c;
    }
}

// thin blocky hand (seconds): every Minecraft pixel within `radius` of the centre line takes the colour
static void hand(float angDeg, float length, float tail, float radius, uint16_t c) {
  const float a = angDeg * DEG_TO_RAD, ux = sinf(a), uy = -cosf(a);
  const int r = (int)ceilf(length + radius + 1);
  for (int y = max(0, (int)(CY - r)); y < min(H, (int)(CY + r)); y++)
    for (int x = max(0, (int)(CX - r)); x < min(W, (int)(CX + r)); x++) {
      const float px = x + 0.5f - CX, py = y + 0.5f - CY;
      const float s = constrain(px * ux + py * uy, -tail, length);
      const float ex = px - s * ux, ey = py - s * uy;
      if (ex * ex + ey * ey <= radius * radius) tex[y * W + x] = c;
    }
}

void ClockScreen::render(const struct tm &lt, bool ok) {
  const float hours = ok ? lt.tm_hour + lt.tm_min / 60.f + lt.tm_sec / 3600.f : 12;
  drawScene((hours - 12) * 15);                    // 360° per 24 h: sun on top at noon
  drawBorder();
  for (int i = 1; i <= 12; i++) drawNumber(i, CX + sinf(i * 30 * DEG_TO_RAD) * NUM_R, CY - cosf(i * 30 * DEG_TO_RAD) * NUM_R);
  if (ok) {
    const uint16_t red = rgb(230, 30, 30);
    sword((lt.tm_hour % 12 + lt.tm_min / 60.f + lt.tm_sec / 3600.f) * 30, 17, {rgb(255, 255, 255), rgb(205, 205, 210), rgb(130, 130, 140)});  // iron = hours
    sword((lt.tm_min + lt.tm_sec / 60.f) * 6, 25, {rgb(200, 255, 250), rgb(80, 215, 205), rgb(30, 140, 130)});                             // diamond = minutes
    hand(lt.tm_sec * 6, 24, 5, 0.5f, red);
    for (int y = 39; y <= 40; y++) for (int x = 29; x <= 30; x++) tex[y * W + x] = red;
  }
  // blow the Minecraft pixels up 4x and push in strips (small buffer, whole screen in ~60 ms)
  TFT_eSprite s(&tft);
  s.setColorDepth(16);
  if (!s.createSprite(SCREEN_W, STRIP_ROWS * TEX)) return;
  uint16_t *buf = (uint16_t *)s.getPointer();
  for (int sy = 0; sy < H; sy += STRIP_ROWS) {
    for (int ty = 0; ty < STRIP_ROWS; ty++) {
      uint16_t *row = buf + ty * TEX * SCREEN_W;
      for (int tx = 0; tx < W; tx++) {
        const uint16_t c = tex[(sy + ty) * W + tx], sw = (c >> 8) | (c << 8);   // sprite memory is byte-swapped
        for (int k = 0; k < TEX; k++) row[tx * TEX + k] = sw;
      }
      for (int k = 1; k < TEX; k++) memcpy(row + k * SCREEN_W, row, SCREEN_W * 2);
    }
    screens::blendCorner(s, 0, sy * TEX);   // mute badge fades over the top-right corner
    s.pushSprite(0, sy * TEX);
  }
  s.deleteSprite();
}

void ClockScreen::enter() {
  lastSec = -1;
  update(millis());
}

bool ClockScreen::onEvent(const InputEvent &e) {
  if (e.type == Ev::Hold && e.button == Btn::Play) {   // fast-day demo on/off
    demo = !demo;
    demoStart = millis(); demoBase = timekeeper::nowUtc(); lastDemoFrame = 0;
    Serial.printf("clock: fast-day demo %s\n", demo ? "ON" : "off");
    lastSec = -1;
    return true;
  }
  return false;
}

void ClockScreen::update(uint32_t now) {
  struct tm lt;
  if (demo) {                                          // 1 real second = 24 clock minutes, ~12 frames a second
    if (now - lastDemoFrame < 80) return;
    lastDemoFrame = now;
    const time_t t = demoBase + (time_t)((now - demoStart) * 1.44f);
    localtime_r(&t, &lt);
    render(lt, true);
    return;
  }
  const bool ok = timekeeper::localTime(lt);
  const int sec = ok ? lt.tm_sec : -2;
  if (sec == lastSec) return;
  lastSec = sec;
  const uint32_t t = millis();
  render(lt, ok);
  static bool logged = false;
  if (!logged) { logged = true; Serial.printf("clock: frame drawn in %lu ms\n", millis() - t); }
}
