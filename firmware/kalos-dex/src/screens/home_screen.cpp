#include "home_screen.h"
#include "display.h"
#include "storage.h"
#include "pet.h"
#include "pet_art.h"
#include "timekeeper.h"
#include "ui.h"

// layout (portrait 240 x 320)
static constexpr int HEAD_H = TOP_BAND_H, CLOCK_Y = HEAD_H + 4;
static constexpr int PET_X = SAFE_MARGIN, PET_Y = 66, PET_W = SCREEN_W - 2 * SAFE_MARGIN, PET_H = 142;   // ends at 208
static constexpr int MSG_Y = 216, BARS_Y = 232, BAR_ROW = 16;   // bars end at 296, the hint line starts at 298
static_assert(CLOCK_Y + 18 <= PET_Y, "the clock row (18 px font) runs into the pet area: move PET_Y down");
static constexpr uint32_t MSG_MS = 4000;

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
// deeper than before: the TN panel drifts light colours towards yellow / white off-axis (step 07)
static const uint16_t C_FOOD = rgb(240, 110, 20), C_FUN = rgb(235, 70, 150), C_ENERGY = rgb(40, 175, 70),
                      C_XP = rgb(50, 120, 240), C_SPOT = rgb(110, 190, 120);

static TFT_eSprite spr(&tft);   // the pet area: made in enter(), freed in leave() (keeps RAM free elsewhere)

static void bar(int row, const char *label, float v, float max, uint16_t c, const char *right) {   // HP-style row
  const int y = BARS_Y + row * BAR_ROW, bx = 66, bw = 106, bh = 12, rx = SCREEN_W - SAFE_MARGIN - 4;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  ui::text(tft, label, SAFE_MARGIN + 6, y + bh / 2, ui::BODY);
  ui::hpBar(tft, bx, y, bw, bh, v / max, c);
  tft.setTextDatum(MR_DATUM);
  tft.setTextPadding(rx - bx - bw - 2);   // paints over the old number in place
  ui::text(tft, right, rx, y + bh / 2, ui::fit(tft, right, rx - bx - bw - 4, ui::BODY));
  tft.setTextPadding(0);
}

void HomeScreen::drawHeader() {   // name (+ star when shiny) and level; off-screen so the mute badge can fade over it
  TFT_eSprite h(&tft);
  h.setColorDepth(16);
  const bool spr = h.createSprite(SCREEN_W, HEAD_H);   // 21 KB, only for this moment
  TFT_eSPI &g = spr ? (TFT_eSPI &)h : (TFT_eSPI &)tft;
  g.fillRect(0, 0, SCREEN_W, HEAD_H, col::RED);
  g.setTextColor(col::WHITE, col::RED);
  g.setTextDatum(ML_DATUM);
  char buf[24];
  snprintf(buf, sizeof buf, "%s%s", pet::name(), pet::shiny() ? " *" : "");
  ui::text(g, buf, SAFE_MARGIN + 6, TITLE_Y, ui::TITLE);
  if (pet::hatched()) {
    snprintf(buf, sizeof buf, "Lv %u", pet::level());
    g.setTextDatum(MR_DATUM);
    ui::text(g, buf, SCREEN_W - SAFE_MARGIN - 6, TITLE_Y, ui::TITLE);
  }
  if (spr) { screens::blendCorner(h, 0, 0); h.pushSprite(0, 0); h.deleteSprite(); }
}

void HomeScreen::drawInfo() {
  const uint32_t head = pet::stage() << 16 | pet::level() << 1 | pet::shiny();   // title bar only when these change
  if (head != shownHead) { shownHead = head; drawHeader(); }
  char buf[40];
  // Everything below paints over the old picture in place (text with padding, bars fully redrawn) instead of
  // blanking first: blanking + redrawing showed as a flicker on every message / value change.
  const int8_t layout = pet::hatched();   // egg <-> pet: different rows, the only time the area is cleared
  if (layout != shownLayout) {
    shownLayout = layout;
    tft.fillRect(0, HEAD_H, SCREEN_W, PET_Y - HEAD_H, col::SCREEN_BG);
    tft.fillRect(0, BARS_Y - 4, SCREEN_W, 4 * BAR_ROW, col::SCREEN_BG);   // must not touch the hint line below
  }
  // clock + mood row
  struct tm lt;
  if (timekeeper::localTime(lt)) {
    const int h12 = lt.tm_hour % 12 ? lt.tm_hour % 12 : 12;
    if (!storage::clock24h()) snprintf(buf, sizeof buf, "%d:%02d %s", h12, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM");
    else snprintf(buf, sizeof buf, "%02d:%02d", lt.tm_hour, lt.tm_min);
    lastMinute = lt.tm_min;
  } else {
    snprintf(buf, sizeof buf, "--:--");
    lastMinute = -1;
  }
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.setTextPadding(100);   // covers the longest old text ("12:59 PM", "Sleeping")
  tft.setTextDatum(TL_DATUM);
  ui::text(tft, buf, SAFE_MARGIN + 6, CLOCK_Y, ui::BODY);
  tft.setTextDatum(TR_DATUM);
  ui::text(tft, pet::moodName(pet::mood()), SCREEN_W - SAFE_MARGIN - 6, CLOCK_Y, ui::BODY);
  // what it just said (between the pet area, ends 208, and the bars, 228)
  const bool say = *pet::message() && millis() - pet::messageAt() < MSG_MS;
  tft.setTextColor(col::RED_DARK, col::SCREEN_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(SCREEN_W - 2 * SAFE_MARGIN);
  if (say) ui::text(tft, pet::message(), SCREEN_W / 2, MSG_Y, ui::BODY);
  else if (msgShown) tft.fillRect(0, MSG_Y - 9, SCREEN_W, 18, col::SCREEN_BG);   // message just ended (= the font's line)
  msgShown = say;
  tft.setTextPadding(0);
  // bars
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
    tft.setTextPadding(SCREEN_W - 2 * SAFE_MARGIN);   // smooth text only paints its letters: cover "10 min" -> "9 min"
    ui::text(tft, buf, SCREEN_W / 2, BARS_Y + 20, ui::BODY);
    tft.setTextPadding(0);
    bar(3, "Egg", PET_EGG_SECONDS - left, PET_EGG_SECONDS, C_SPOT, "");
  }
}

void HomeScreen::drawPet(uint32_t now) {
  if (!spr.created()) return;
  spr.fillSprite(col::SCREEN_BG);
  petart::draw(spr, PET_W / 2, PET_H - 14, now, true);
  spr.pushSprite(PET_X, PET_Y);
}

void HomeScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  shownHead = UINT32_MAX;
  shownLayout = -1;
  msgShown = false;
  spr.setColorDepth(16);
  if (!spr.createSprite(PET_W, PET_H)) Serial.println("home: no RAM for the pet sprite");
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  ui::text(tft, "Feed  Play     Back: next", SCREEN_W / 2, SCREEN_H - SAFE_MARGIN - 2, ui::BODY);
  drawInfo();
  shownVersion = pet::version();
  drawPet(millis());
  shownFrame = petart::frameKey(millis());
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
  const uint32_t frame = petart::frameKey(now);   // redraw only when the picture changes (sprite timings)
  if (frame != shownFrame) { shownFrame = frame; drawPet(now); }
}

bool HomeScreen::onEvent(const InputEvent &e) {
  if (e.type != Ev::Press) return false;
  if (e.button == Btn::Feed) { petart::reactTo(pet::feed(), petart::Reaction::Feed, millis()); return true; }
  if (e.button == Btn::Play) { petart::reactTo(pet::play(), petart::Reaction::Play, millis()); return true; }
  return false;
}
