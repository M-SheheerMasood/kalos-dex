#include "home_screen.h"
#include "display.h"
#include "pet.h"
#include "pet_line.h"
#include "timekeeper.h"

// layout (portrait 240 x 320)
static constexpr int HEAD_H = 34, CLOCK_Y = 38;
static constexpr int PET_X = SAFE_MARGIN, PET_Y = 58, PET_W = SCREEN_W - 2 * SAFE_MARGIN, PET_H = 150;
static constexpr int MSG_Y = 220, BARS_Y = 238, BAR_ROW = 16;
static constexpr uint32_t FRAME_MS = 500, MSG_MS = 4000;

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static const uint16_t C_FOOD = rgb(255, 150, 40), C_FUN = rgb(255, 90, 170), C_ENERGY = rgb(60, 200, 90),
                      C_XP = rgb(60, 140, 255), C_EGG = rgb(250, 240, 210), C_SPOT = rgb(110, 190, 120);

static TFT_eSprite spr(&tft);   // the pet area: made in enter(), freed in leave() (keeps RAM free elsewhere)

static void bar(int row, const char *label, float v, float max, uint16_t c, const char *right) {
  const int y = BARS_Y + row * BAR_ROW, bx = 64, bw = 120, bh = 10;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  tft.drawString(label, SAFE_MARGIN + 6, y + bh / 2, 2);
  const int fill = constrain(lroundf(v / max * (bw - 2)), 0, bw - 2);
  tft.drawRect(bx, y, bw, bh, col::SCREEN_DARK);
  tft.fillRect(bx + 1, y + 1, fill, bh - 2, c);
  tft.fillRect(bx + 1 + fill, y + 1, bw - 2 - fill, bh - 2, col::SILVER);
  tft.setTextDatum(MR_DATUM);
  tft.fillRect(bx + bw + 2, y - 3, SCREEN_W - SAFE_MARGIN - bx - bw - 2, bh + 6, col::SCREEN_BG);
  tft.drawString(right, SCREEN_W - SAFE_MARGIN - 4, y + bh / 2, 2);
}

void HomeScreen::drawInfo() {
  // header: name (+ star when shiny) and level
  tft.fillRect(0, 0, SCREEN_W, HEAD_H, col::RED);
  tft.setTextColor(col::WHITE, col::RED);
  tft.setTextDatum(ML_DATUM);
  char buf[40];
  snprintf(buf, sizeof buf, "%s%s", pet::name(), pet::shiny() ? " *" : "");
  tft.drawString(buf, SAFE_MARGIN + 6, HEAD_H / 2 + 2, 4);
  if (pet::hatched()) {
    snprintf(buf, sizeof buf, "Lv %u", pet::level());
    tft.setTextDatum(MR_DATUM);
    tft.drawString(buf, SCREEN_W - SAFE_MARGIN - 6, HEAD_H / 2 + 2, 4);
  }
  // clock + mood row
  tft.fillRect(0, HEAD_H, SCREEN_W, PET_Y - HEAD_H, col::SCREEN_BG);
  struct tm lt;
  if (timekeeper::localTime(lt)) {
    const int h12 = lt.tm_hour % 12 ? lt.tm_hour % 12 : 12;
    if (CLOCK_12H) snprintf(buf, sizeof buf, "%d:%02d %s", h12, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM");
    else snprintf(buf, sizeof buf, "%02d:%02d", lt.tm_hour, lt.tm_min);
    lastMinute = lt.tm_min;
  } else {
    snprintf(buf, sizeof buf, "--:--");
    lastMinute = -1;
  }
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(buf, SAFE_MARGIN + 6, CLOCK_Y, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(pet::moodName(pet::mood()), SCREEN_W - SAFE_MARGIN - 6, CLOCK_Y, 2);
  // what it just said
  tft.fillRect(0, MSG_Y - 10, SCREEN_W, 20, col::SCREEN_BG);
  msgShown = *pet::message() && millis() - pet::messageAt() < MSG_MS;
  if (msgShown) {
    tft.setTextColor(col::RED_DARK, col::SCREEN_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(pet::message(), SCREEN_W / 2, MSG_Y, 2);
  }
  // bars
  tft.fillRect(0, BARS_Y - 4, SCREEN_W, 4 * BAR_ROW + 4, col::SCREEN_BG);
  if (pet::hatched()) {
    snprintf(buf, sizeof buf, "%d", (int)pet::food()); bar(0, "Food", pet::food(), 100, C_FOOD, buf);
    snprintf(buf, sizeof buf, "%d", (int)pet::fun()); bar(1, "Fun", pet::fun(), 100, C_FUN, buf);
    snprintf(buf, sizeof buf, "%d", (int)pet::energy()); bar(2, "Energy", pet::energy(), 100, C_ENERGY, buf);
    snprintf(buf, sizeof buf, "%lu/%lu", (unsigned long)pet::xp(), (unsigned long)pet::xpNeeded());
    bar(3, "XP", pet::xp(), pet::xpNeeded(), C_XP, buf);
  } else {
    const uint32_t left = pet::eggSecondsLeft();
    snprintf(buf, sizeof buf, "hatches in %lu min", (unsigned long)(left + 59) / 60);
    tft.setTextColor(col::TEXT, col::SCREEN_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buf, SCREEN_W / 2, BARS_Y + 20, 2);
    bar(3, "Egg", PET_EGG_SECONDS - left, PET_EGG_SECONDS, C_SPOT, "");
  }
}

// ---- the placeholder pet: a blob whose face shows the mood (sprites replace this later)
static void eye(int x, int y, pet::Mood m, int frame) {
  using M = pet::Mood;
  const uint16_t k = col::BLACK;
  switch (m) {
    case M::Sleeping: spr.fillRect(x - 6, y, 12, 2, k); break;                                  // closed
    case M::Happy: spr.drawLine(x - 6, y + 2, x, y - 4, k); spr.drawLine(x, y - 4, x + 6, y + 2, k);
                   spr.drawLine(x - 6, y + 3, x, y - 3, k); spr.drawLine(x, y - 3, x + 6, y + 3, k); break;  // ^ ^
    case M::Tired: spr.fillCircle(x, y + 2, 5, k); spr.fillRect(x - 6, y - 5, 13, 6, spr.readPixel(x, y - 8)); break;  // half shut
    default:
      if (frame % 8 == 7) { spr.fillRect(x - 6, y, 12, 2, k); break; }                         // blink
      spr.fillCircle(x, y, 6, k); spr.fillCircle(x + 2, y - 2, 2, col::WHITE);
  }
}

void HomeScreen::drawPet(uint32_t now) {
  if (!spr.created()) return;
  const int frame = now / FRAME_MS;
  const pet::Mood m = pet::mood();
  spr.fillSprite(col::SCREEN_BG);
  const int cx = PET_W / 2, ground = PET_H - 14;
  spr.fillEllipse(cx, ground, 46, 7, col::SILVER);   // shadow

  if (!pet::hatched()) {   // egg: wobbles more as it gets close
    const int wob = pet::eggSecondsLeft() < 300 ? (frame % 2 ? 3 : -3) : (frame % 4 == 0 ? 1 : 0);
    const int ex = cx + wob, ey = ground - 44;
    spr.fillEllipse(ex, ey, 32, 42, C_EGG);
    spr.drawEllipse(ex, ey, 32, 42, col::SCREEN_DARK);
    spr.fillCircle(ex - 12, ey - 14, 7, C_SPOT); spr.fillCircle(ex + 14, ey + 2, 9, C_SPOT); spr.fillCircle(ex - 6, ey + 22, 6, C_SPOT);
    spr.pushSprite(PET_X, PET_Y);
    return;
  }

  const int st = pet::stage() - 1;
  const uint16_t body = pet::shiny() ? PET_LINE.shinyColour[st] : PET_LINE.colour[st];
  const int rx = 34 + st * 9, ry = rx * 4 / 5;
  const int bob = (m == pet::Mood::Sleeping) ? 0 : (frame % 2 ? -3 : 0);
  const int by = ground - ry + 2 + bob;
  spr.fillEllipse(cx, by, rx, ry, body);
  spr.fillEllipse(cx, by + ry / 3, rx * 3 / 5, ry / 2, col::WHITE);   // belly
  if (st >= 1) { spr.fillTriangle(cx - rx + 6, by - ry + 14, cx - rx + 2, by - ry - 10, cx - rx + 22, by - ry + 6, body);   // ears grow with
                 spr.fillTriangle(cx + rx - 6, by - ry + 14, cx + rx - 2, by - ry - 10, cx + rx - 22, by - ry + 6, body); } // each stage
  if (st >= 2) spr.fillTriangle(cx - 8, by - ry + 2, cx + 8, by - ry + 2, cx, by - ry - 16, body);            // horn
  const int eyeY = by - ry / 3;
  eye(cx - rx / 3, eyeY, m, frame);
  eye(cx + rx / 3, eyeY, m, frame);
  const int my = eyeY + 16;
  switch (m) {   // mouth
    case pet::Mood::Hungry: spr.fillEllipse(cx, my + 2, 5, 6, col::RED_DARK); break;
    case pet::Mood::Sad: case pet::Mood::Tired:
      spr.drawLine(cx - 6, my + 4, cx, my, col::BLACK); spr.drawLine(cx, my, cx + 6, my + 4, col::BLACK); break;
    case pet::Mood::Sleeping: break;
    default: spr.drawLine(cx - 7, my, cx, my + 5, col::BLACK); spr.drawLine(cx, my + 5, cx + 7, my, col::BLACK);
  }
  if (m == pet::Mood::Sad) spr.fillEllipse(cx + rx / 3 + 4, eyeY + 10 + frame % 3 * 2, 2, 3, C_XP);   // tear
  if (m == pet::Mood::Sleeping) {   // floating Zz
    spr.setTextColor(col::SCREEN_DARK);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(frame % 2 ? "z Z" : "Z z", cx + rx + 14, by - ry - 4 - frame % 2 * 4, 4);
  }
  if (pet::shiny() && frame % 2) {   // sparkle
    const int sx = cx - rx - 10, sy = by - ry;
    spr.drawLine(sx - 5, sy, sx + 5, sy, C_FOOD); spr.drawLine(sx, sy - 5, sx, sy + 5, C_FOOD);
  }
  spr.pushSprite(PET_X, PET_Y);
}

void HomeScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  spr.setColorDepth(16);
  if (!spr.createSprite(PET_W, PET_H)) Serial.println("home: no RAM for the pet sprite");
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.drawString("Feed  Play     Back: next", SCREEN_W / 2, SCREEN_H - SAFE_MARGIN - 2, 2);
  drawInfo();
  shownVersion = pet::version();
  drawPet(millis());
  lastFrame = millis();
}

void HomeScreen::leave() { spr.deleteSprite(); }

void HomeScreen::update(uint32_t now) {
  struct tm lt;
  const bool minuteTick = timekeeper::localTime(lt) ? lt.tm_min != lastMinute : lastMinute != -1;
  const bool msgExpired = msgShown && now - pet::messageAt() >= MSG_MS;
  if (pet::version() != shownVersion || minuteTick || msgExpired) {
    shownVersion = pet::version();
    drawInfo();
  }
  if (now - lastFrame >= FRAME_MS) { lastFrame = now; drawPet(now); }
}

bool HomeScreen::onEvent(const InputEvent &e) {
  if (e.type != Ev::Press) return false;
  if (e.button == Btn::Feed) { pet::feed(); return true; }
  if (e.button == Btn::Play) { pet::play(); return true; }
  return false;
}
