#include "music_screen.h"
#include "display.h"
#include "ui.h"
#include "media.h"
#include "pclink.h"

// layout (portrait 240 x 320)
static constexpr int HEAD_H = TOP_BAND_H;
static constexpr int DISC = 124, DISC_X = (SCREEN_W - DISC) / 2, DISC_Y = 50, DISC_R = 58, LABEL_R = 24;   // centre stays at y 112
static constexpr int TRACK_Y = 184, TITLE_W = SCREEN_W - 2 * SAFE_MARGIN, TITLE_H = 26, TITLE_GAP = 40;
static constexpr int ARTIST_Y = 220, PROG_Y = 240, CTRL_Y = 266, VOL_Y = 302;
static constexpr uint32_t DISC_MS = 100, TITLE_MS = 40, TITLE_HOLD_MS = 1500, KEY_LIT_MS = 300;
static constexpr float RPM = 15;   // slower than a real record (user: 33 rpm looked too fast)

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static const uint16_t C_VINYL = rgb(30, 30, 34), C_GROOVE = rgb(60, 60, 66), C_SHEEN = rgb(110, 110, 120),
                      C_BAR = rgb(60, 140, 255), C_OFF = rgb(150, 150, 150);

static TFT_eSprite disc(&tft), titleSpr(&tft), row(&tft);   // row: one shared strip for the lower rows
static constexpr int ROW_W = SCREEN_W - 2 * SAFE_MARGIN, ROW_H = 36;

// clear the shared strip, let `draw` paint it (y = 0 is the row's top), push the top `h` lines at screen y
template <typename F> static void drawRow(int y, int h, F draw) {
  if (!row.created()) return;
  row.fillSprite(col::SCREEN_BG);
  draw(row);
  row.pushSprite(SAFE_MARGIN, y, 0, 0, ROW_W, h);
}

static void fmtTime(char *buf, size_t n, uint32_t s) {
  if (s >= 3600) snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
  else snprintf(buf, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

static const char *shownTitle() {
  if (!pclink::online()) return "PC offline";
  return *media::title() ? media::title() : "Nothing playing";
}

void MusicScreen::drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEAD_H, col::RED);
  tft.setTextColor(col::WHITE, col::RED);
  tft.setTextDatum(ML_DATUM);
  ui::text(tft, "MUSIC", SAFE_MARGIN + 6, TITLE_Y, ui::TITLE);
  drawDot();
}

void MusicScreen::drawDot() {   // PC dot: filled green = connected
  const int dx = SCREEN_W - SAFE_MARGIN - 14, dy = TOP_MID_Y;
  tft.fillCircle(dx, dy, 6, pclink::online() ? rgb(80, 230, 110) : col::RED);
  tft.drawCircle(dx, dy, 6, col::WHITE);
}

// the record: grooves + a turning sheen + a Pokéball label (placeholder until album art)
void MusicScreen::drawDisc() {
  if (!disc.created()) return;
  const bool on = pclink::online() && *media::title();
  const int c = DISC / 2;
  disc.fillSprite(col::SCREEN_BG);
  disc.fillCircle(c, c, DISC_R, C_VINYL);
  for (int r = DISC_R - 4; r > LABEL_R + 4; r -= 6) disc.drawCircle(c, c, r, C_GROOVE);
  const float a = angle * DEG_TO_RAD, ca = cosf(a), sa = sinf(a);
  for (int k : {-1, 1}) {   // two sheen lines across the grooves turn with the record
    disc.drawLine(c + k * ca * (LABEL_R + 6), c + k * sa * (LABEL_R + 6), c + k * ca * (DISC_R - 4), c + k * sa * (DISC_R - 4), C_SHEEN);
  }
  // label: top half red, bottom half white, black band + button – turned by `angle`
  const uint16_t top = on ? col::RED : C_OFF, bottom = col::WHITE;
  for (int y = -LABEL_R; y <= LABEL_R; y++)
    for (int x = -LABEL_R; x <= LABEL_R; x++) {
      if (x * x + y * y > LABEL_R * LABEL_R) continue;
      const float d = x * sa - y * ca;   // distance from the band through the centre
      disc.drawPixel(c + x, c + y, fabsf(d) < 2.5f ? col::BLACK : d < 0 ? top : bottom);
    }
  disc.fillCircle(c, c, 7, col::BLACK);
  disc.fillCircle(c, c, 4, col::WHITE);
  disc.pushSprite(DISC_X, DISC_Y);
}

// title: centred if it fits, otherwise it scrolls (waits a moment at the start of each loop)
void MusicScreen::drawTitle(uint32_t now) {
  if (!titleSpr.created()) return;
  const char *t = shownTitle();
  titleSpr.fillSprite(col::SCREEN_BG);
  titleSpr.setTextColor(pclink::online() ? col::TEXT : col::SCREEN_DARK, col::SCREEN_BG);
  if (titleW <= TITLE_W) {
    titleSpr.setTextDatum(TC_DATUM);
    ui::text(titleSpr, t, TITLE_W / 2, 0, ui::TITLE);
  } else {
    const uint32_t loop = (titleW + TITLE_GAP) / 2 * TITLE_MS + TITLE_HOLD_MS, tt = (now - titleStart) % loop;
    const int off = tt < TITLE_HOLD_MS ? 0 : (tt - TITLE_HOLD_MS) / TITLE_MS * 2;
    titleSpr.setTextDatum(TL_DATUM);
    ui::text(titleSpr, t, -off, 0, ui::TITLE);
    ui::text(titleSpr, t, -off + titleW + TITLE_GAP, 0, ui::TITLE);
  }
  titleSpr.pushSprite(SAFE_MARGIN, TRACK_Y);
}

void MusicScreen::drawText() {
  titleW = ui::width(tft, shownTitle(), ui::TITLE);
  titleStart = millis();
  drawTitle(titleStart);
  const char *a = !pclink::online() ? "start the companion on the PC" : media::artist();
  char buf[64];
  snprintf(buf, sizeof buf, "%s", a);
  for (int n = strlen(buf); n > 3 && ui::width(tft, buf, ui::BODY) > TITLE_W; n--) strcpy(buf + n - 4, "...");   // shorten to fit
  drawRow(ARTIST_Y - 8, 17, [&](TFT_eSprite &g) {
    g.setTextDatum(MC_DATUM);
    g.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
    ui::text(g, buf, ROW_W / 2, 8, ui::BODY);
  });
}

void MusicScreen::drawProgress() {
  const uint32_t dur = media::durationS(), p = media::positionS();
  shownSecond = p;
  const bool show = pclink::online() && *media::title();
  drawRow(PROG_Y - 8, 17, [&](TFT_eSprite &g) {
    if (!show) return;
    char buf[12];
    const int y = 8;
    g.setTextColor(col::TEXT, col::SCREEN_BG);
    fmtTime(buf, sizeof buf, p);
    g.setTextDatum(ML_DATUM);
    ui::text(g, buf, 4, y, ui::BODY);
    if (!dur) return;
    fmtTime(buf, sizeof buf, dur);
    g.setTextDatum(MR_DATUM);
    ui::text(g, buf, ROW_W - 4, y, ui::BODY);
    const int bx = 52, bw = ROW_W - 2 * bx;
    ui::hpBar(g, bx, y - 5, bw, 10, (float)p / dur, col::RED);
  });
}

// |<<   > or ||   >>|  with the button that does it underneath; lights up red for a moment when used
void MusicScreen::drawControls() {
  const char *k = media::lastKey();
  keyLit = millis() - media::lastKeyAt() < KEY_LIT_MS;
  const bool playing = media::state() == media::State::Playing;
  drawRow(CTRL_Y - 13, ROW_H, [&](TFT_eSprite &g) {
    auto c = [&](const char *which) { return keyLit && !strcmp(k, which) ? col::RED : col::SCREEN_DARK; };
    const int y = 12, o = -SAFE_MARGIN;   // o: screen x -> strip x
    uint16_t p = c("prev");    // previous: bar + two triangles pointing left
    g.fillRect(48 + o, y - 9, 3, 18, p);
    g.fillTriangle(62 + o, y - 9, 62 + o, y + 9, 52 + o, y, p); g.fillTriangle(73 + o, y - 9, 73 + o, y + 9, 63 + o, y, p);
    p = c("playpause");
    if (playing) { g.fillRect(111 + o, y - 10, 6, 20, p); g.fillRect(123 + o, y - 10, 6, 20, p); }
    else g.fillTriangle(112 + o, y - 11, 112 + o, y + 11, 131 + o, y, p);
    p = c("next");             // next: two triangles pointing right + bar
    g.fillTriangle(167 + o, y - 9, 167 + o, y + 9, 177 + o, y, p); g.fillTriangle(178 + o, y - 9, 178 + o, y + 9, 188 + o, y, p);
    g.fillRect(189 + o, y - 9, 3, 18, p);
    g.setTextDatum(TC_DATUM);
    g.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
    ui::text(g, "Feed", 61 + o, y + 11, ui::SMALL);
    ui::text(g, "knob", 120 + o, y + 11, ui::SMALL);
    ui::text(g, "Play", 179 + o, y + 11, ui::SMALL);
  });
}

void MusicScreen::drawVolume() {
  const int v = media::volume();
  const bool m = media::muted();
  drawRow(VOL_Y - 9, 19, [&](TFT_eSprite &g) {
    const int y = 9;
    g.setTextColor(col::TEXT, col::SCREEN_BG);
    g.setTextDatum(ML_DATUM);
    ui::text(g, "VOL", 4, y, ui::BODY);
    const int bx = 40, bw = ROW_W - bx - 48;
    ui::hpBar(g, bx, y - 6, bw, 12, v < 0 ? 0 : v / 100.0f, m ? C_OFF : C_BAR);
    char buf[8];
    if (m) snprintf(buf, sizeof buf, "MUTE");
    else if (v < 0) snprintf(buf, sizeof buf, "--");
    else snprintf(buf, sizeof buf, "%d", v);
    g.setTextDatum(MR_DATUM);
    g.setTextColor(m ? col::RED : col::TEXT, col::SCREEN_BG);
    ui::text(g, buf, ROW_W - 4, y, ui::BODY);
  });
}

void MusicScreen::drawAll() {
  shownOnline = pclink::online();
  shownTrack = media::trackVersion(); shownState = media::stateVersion(); shownVol = media::volumeVersion();
  shownKeyAt = media::lastKeyAt();
  drawHeader(); drawDisc(); drawText(); drawProgress(); drawControls(); drawVolume();
}

void MusicScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  disc.setColorDepth(8);
  titleSpr.setColorDepth(16);
  row.setColorDepth(16);
  if (!disc.createSprite(DISC, DISC) || !titleSpr.createSprite(TITLE_W, TITLE_H) || !row.createSprite(ROW_W, ROW_H))
    Serial.println("music: no RAM for the sprites");
  drawAll();
}

void MusicScreen::leave() { disc.deleteSprite(); titleSpr.deleteSprite(); row.deleteSprite(); }

// only what changed is drawn again
void MusicScreen::update(uint32_t now) {
  const bool online = pclink::online();
  if (online != shownOnline) { drawAll(); return; }   // offline <-> online: everything changes
  if (media::trackVersion() != shownTrack) {
    shownTrack = media::trackVersion();
    drawDisc(); drawText(); drawProgress();
  }
  if (media::stateVersion() != shownState) { shownState = media::stateVersion(); drawControls(); drawProgress(); }
  if (media::volumeVersion() != shownVol) { shownVol = media::volumeVersion(); drawVolume(); }
  if ((int)media::positionS() != shownSecond) drawProgress();
  if (media::lastKeyAt() != shownKeyAt || (keyLit && now - media::lastKeyAt() >= KEY_LIT_MS)) {
    shownKeyAt = media::lastKeyAt();
    drawControls();
  }
  if (titleW > TITLE_W && now - lastTitle >= TITLE_MS) { lastTitle = now; drawTitle(now); }
  if (media::state() == media::State::Playing && online && now - lastDisc >= DISC_MS) {
    const uint32_t dt = min<uint32_t>(now - lastDisc, 200);           // no jump after a pause
    angle = fmodf(angle + RPM * 6 * dt / 1000.f, 360);                  // rpm x 6 = degrees a second
    lastDisc = now;
    drawDisc();
  }
}

bool MusicScreen::onEvent(const InputEvent &e) {
  if (e.type != Ev::Press) return false;
  if (e.button == Btn::Feed) { media::prev(); return true; }
  if (e.button == Btn::Play) { media::next(); return true; }
  return false;
}
