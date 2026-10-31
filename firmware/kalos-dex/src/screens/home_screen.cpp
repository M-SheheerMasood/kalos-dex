#include "home_screen.h"
#include "display.h"
#include "storage.h"
#include "pet.h"
#include "pet_art.h"
#include "timekeeper.h"
#include "ui.h"
#include "input.h"
#include "night.h"
#include "leds.h"

// layout (portrait 240 x 320)
static constexpr int HEAD_H = TOP_BAND_H, CLOCK_Y = HEAD_H + 4;
static constexpr int PET_X = SAFE_MARGIN, PET_Y = 66, PET_W = SCREEN_W - 2 * SAFE_MARGIN, PET_H = 142;   // ends at 208
static constexpr int MSG_Y = 216, BARS_Y = 232, BAR_ROW = 16;   // bars end at 296, the hint line starts at 298
static_assert(CLOCK_Y + 18 <= PET_Y, "the clock row (18 px font) runs into the pet area: move PET_Y down");
static constexpr uint32_t MSG_MS = 4000;
// the hatch show (user, 2026-10-29): quick fade to white, 1 s white, then the screen fades back with the new pet
static constexpr uint32_t WHITE_IN_MS = 600, WHITE_MS = 1000, DIM_MS = 600, BRIGHT_MS = 800;   // slower fades (user, 2026-10-31)
static constexpr int WHITE_STEPS = 8;
// the evolution show (S5): dark screen, the pet a bit higher, one line of text under it
static const uint16_t C_NIGHT = ((24 & 0xF8) << 8) | ((30 & 0xFC) << 3) | (52 >> 3);
static constexpr int EVO_Y = 80, EVO_TEXT_Y = 262;

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
// deeper than before: the TN panel drifts light colours towards yellow / white off-axis (step 07)
static const uint16_t C_FOOD = rgb(240, 110, 20), C_FUN = rgb(235, 70, 150), C_ENERGY = rgb(40, 175, 70),
                      C_XP = rgb(50, 120, 240), C_SPOT = rgb(110, 190, 120);

static TFT_eSprite spr(&tft);   // the pet area: made in enter(), freed in leave() (keeps RAM free elsewhere)

static constexpr int BAR_X = 66, BAR_W = 106, BAR_H = 12;

// the bar alone, drawn off-screen and pushed in one go (drawn straight on the screen, its black + grey layers showed
// as a jitter while it changed fast: the hatch "Hold" bar, user 2026-10-29)
static void barOnly(int row, float frac, uint16_t c) {
  const int y = BARS_Y + row * BAR_ROW;
  TFT_eSprite b(&tft);
  b.setColorDepth(16);
  if (!b.createSprite(BAR_W, BAR_H)) { ui::hpBar(tft, BAR_X, y, BAR_W, BAR_H, frac, c); return; }   // 2.5 KB
  b.fillSprite(col::SCREEN_BG);
  ui::hpBar(b, 0, 0, BAR_W, BAR_H, frac, c);
  b.pushSprite(BAR_X, y);
  b.deleteSprite();
}

static void bar(int row, const char *label, float v, float max, uint16_t c, const char *right) {   // HP-style row
  const int y = BARS_Y + row * BAR_ROW, bx = BAR_X, bw = BAR_W, bh = BAR_H, rx = SCREEN_W - SAFE_MARGIN - 4;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  tft.setTextPadding(bx - SAFE_MARGIN - 8);   // the egg's label changes ("Egg" <-> "Hold")
  ui::text(tft, label, SAFE_MARGIN + 6, y + bh / 2, ui::BODY);
  tft.setTextPadding(0);
  barOnly(row, v / max, c);
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
  int right = SCREEN_W - SAFE_MARGIN - 6;
  if (pet::hatched()) {
    snprintf(buf, sizeof buf, "Lv %u", pet::level());
    g.setTextDatum(MR_DATUM);
    ui::text(g, buf, right, TITLE_Y, ui::TITLE);
    right -= ui::width(g, buf, ui::TITLE) + 8;
  }
  if (pet::testMode()) {   // Debug: a TEST tag, so the test pet isn't taken for the real one
    const int tw = ui::width(g, "TEST", ui::SMALL) + 10;
    g.fillRoundRect(right - tw, TITLE_Y - 9, tw, 18, 6, col::WHITE);
    g.setTextColor(col::RED, col::WHITE);
    g.setTextDatum(MC_DATUM);
    ui::text(g, "TEST", right - tw / 2, TITLE_Y, ui::SMALL);
  }
  if (spr) { screens::blendCorner(h, 0, 0); h.pushSprite(0, 0); h.deleteSprite(); }
}

void HomeScreen::drawInfo() {
  const uint32_t head = pet::testMode() << 24 | pet::stage() << 16 | pet::level() << 1 | pet::shiny();   // title bar only on change
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
  const bool said = *pet::message() && millis() - pet::messageAt() < MSG_MS;
  const char *hint = holdStart ? "keep holding..." : pet::canEvolve() ? "It can evolve! hold Feed+Play" : nullptr;
  const bool say = said || hint;   // a message first, else the evolve hint (stays while it can evolve)
  tft.setTextColor(col::RED_DARK, col::SCREEN_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(SCREEN_W - 2 * SAFE_MARGIN);
  if (say) ui::text(tft, said && !holdStart ? pet::message() : hint, SCREEN_W / 2, MSG_Y, ui::BODY);
  else if (msgShown) tft.fillRect(0, MSG_Y - 9, SCREEN_W, 18, col::SCREEN_BG);   // message just ended (= the font's line)
  msgShown = say;
  tft.setTextPadding(0);
  // bars
  if (pet::hatched()) {
    snprintf(buf, sizeof buf, "%d", (int)pet::food()); bar(0, "Food", pet::food(), 100, C_FOOD, buf);
    snprintf(buf, sizeof buf, "%d", (int)pet::fun()); bar(1, "Fun", pet::fun(), 100, C_FUN, buf);
    snprintf(buf, sizeof buf, "%d", (int)pet::energy()); bar(2, "Energy", pet::energy(), 100, C_ENERGY, buf);
    snprintf(buf, sizeof buf, "%lu/%lu", (unsigned long)pet::xp(), (unsigned long)pet::xpNeeded());
    if (holdStart) bar(3, "Hold", min<uint32_t>(PET_HATCH_HOLD_MS, millis() - holdStart), PET_HATCH_HOLD_MS, col::RED, "");
    else bar(3, "XP", pet::xp(), pet::xpNeeded(), C_XP, buf);
  } else {
    drawEggRows();
  }
}

void HomeScreen::drawEggRows() {
  char buf[40];
  const uint32_t left = pet::eggSecondsLeft();
  if (hatch != Hatch::None) snprintf(buf, sizeof buf, "Oh? It's hatching!");
  else if (holdStart) snprintf(buf, sizeof buf, "keep holding...");
  else if (pet::eggReady()) snprintf(buf, sizeof buf, "Ready! hold Feed+Play");
  else snprintf(buf, sizeof buf, "hatches in %lu min", (unsigned long)(left + 59) / 60);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(SCREEN_W - 2 * SAFE_MARGIN);   // smooth text only paints its letters: cover "10 min" -> "9 min"
  ui::text(tft, buf, SCREEN_W / 2, BARS_Y + 20, ui::BODY);
  tft.setTextPadding(0);
  if (holdStart) {   // filling up while Feed + Play are held; let go early = nothing happens
    const float k = min<uint32_t>(PET_HATCH_HOLD_MS, millis() - holdStart) / (float)PET_HATCH_HOLD_MS;
    bar(3, "Hold", k, 1, col::RED, "");
  } else {
    const uint32_t need = storage::hatchSeconds();
    bar(3, "Egg", need - left, need, C_SPOT, "");
  }
}

void HomeScreen::drawPet(uint32_t now, bool push) {
  if (!spr.created()) return;
  spr.fillSprite(col::SCREEN_BG);
  petart::draw(spr, PET_W / 2, PET_H - 14, now, true);
  if (push) spr.pushSprite(PET_X, PET_Y);
}

void HomeScreen::enter() {
  spr.setColorDepth(16);
  if (!spr.createSprite(PET_W, PET_H)) Serial.println("home: no RAM for the pet sprite");
  drawAll();
}

void HomeScreen::drawAll() {
  tft.fillScreen(col::SCREEN_BG);
  shownHead = UINT32_MAX;
  shownLayout = -1;
  msgShown = false;
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  ui::text(tft, "Feed  Play     Back: next", SCREEN_W / 2, SCREEN_H - SAFE_MARGIN - 2, ui::BODY);
  drawInfo();
  shownVersion = pet::version();
  drawPet(millis());
  shownFrame = petart::frameKey(millis());
}

void HomeScreen::leave() {
  spr.deleteSprite();
  if (hatch == Hatch::None) return;
  // left in the middle of the show (Back): the hatch still happens, just without the rest of the show
  // (the manager fades the backlight in for the next screen)
  if (!evolving && !pet::hatched()) pet::hatchNow();
  if (evolving && hatch != Hatch::Bright && pet::canEvolve()) pet::evolveNow();   // confirmed by the hold: it evolves
  petart::stopHatch();
  hatch = Hatch::None;
  if (holdStart) { holdStart = 0; petart::eggHeld(false); }
  petart::evolveHold(-1);
}

int HomeScreen::showY() const { return evolving ? EVO_Y : PET_Y; }

void HomeScreen::fillShow(uint32_t now) {
  if (!evolving) { drawPet(now, false); return; }
  spr.fillSprite(C_NIGHT);
  petart::drawEvolving(spr, PET_W / 2, PET_H - 14, min(now - phaseAt, petart::evolveShowMs()));
}

// the hatch show, step by step (never blocks the loop)
void HomeScreen::updateHatch(uint32_t now) {
  const uint32_t t = now - phaseAt;
  switch (hatch) {
    case Hatch::Cracks:
      if (t >= petart::HATCH_CRACK_MS) { hatch = Hatch::WhiteIn; phaseAt = now; }
      break;
    case Hatch::Evolving:   // silhouettes swapping on the dark screen, ~25 frames a second
      if (t >= petart::evolveShowMs()) { fillShow(now); hatch = Hatch::WhiteIn; phaseAt = now; break; }
      if (spr.created() && now - lastShowDraw >= 40) { lastShowDraw = now; fillShow(now); spr.pushSprite(PET_X, EVO_Y); }
      break;
    case Hatch::WhiteIn: {   // the pet area turns white in a few steps, then the whole screen
      if (t >= WHITE_IN_MS) {
        tft.fillScreen(col::WHITE);
        leds::flash(0xFFFFFF, WHITE_MS + DIM_MS);   // the strip goes white with the screen
        petart::stopHatch();
        hatch = Hatch::White; phaseAt = now;
        break;
      }
      if (!spr.created()) break;
      static int lastStep = -1;
      const int step = t * WHITE_STEPS / WHITE_IN_MS;   // 0 .. WHITE_STEPS-1
      if (step == lastStep && t) break;
      lastStep = step;
      const int k = (step + 1) * 256 / WHITE_STEPS;   // of 256
      if (evolving) {   // the whole dark screen fades with the pet (only the pet area did: a white block first, user)
        tft.fillScreen(tft.alphaBlend(min(255, k), col::WHITE, C_NIGHT));
        spr.fillSprite(C_NIGHT);
        petart::drawEvolving(spr, PET_W / 2, PET_H - 14, petart::evolveShowMs());
      } else drawPet(now, false);
      uint16_t *px = (uint16_t *)spr.getPointer();   // 16-bit sprite, bytes swapped: blend each pixel towards white
      for (int n = 0; n < PET_W * PET_H; n++) {
        const uint16_t c = __builtin_bswap16(px[n]);
        int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
        r += (31 - r) * k >> 8; g += (63 - g) * k >> 8; b += (31 - b) * k >> 8;
        px[n] = __builtin_bswap16(r << 11 | g << 5 | b);
      }
      spr.pushSprite(PET_X, showY());
      break;
    }
    case Hatch::White:
      if (t >= WHITE_MS) { hatch = Hatch::Dim; phaseAt = now; backlightTo = display::backlight(); }
      break;
    case Hatch::Dim:   // fade the white out with the backlight, draw the new Home in the dark, fade it in
      if (t < DIM_MS) { display::setBacklight(backlightTo * (DIM_MS - t) / DIM_MS); break; }
      display::setBacklight(0);
      if (evolving) pet::evolveNow();   // now, so "X evolved into Y!" is seen on the new screen
      else pet::hatchNow();             // now, so "It hatched!" is seen on the new screen (+ white glow, saved)
      drawAll();         // the new pet, everything redrawn (hidden: backlight off)
      hatch = Hatch::Bright; phaseAt = millis();
      break;
    case Hatch::Bright: {
      const uint8_t to = night::backlightTarget();
      if (t < BRIGHT_MS) { display::setBacklight(max<int>(1, to * t / BRIGHT_MS)); break; }
      display::setBacklight(to);
      hatch = Hatch::None;
      if (evolving) petart::celebrateEvolution(now);   // show-off move + confetti
      else petart::celebrate(now);
      break;
    }
    case Hatch::None: break;
  }
}

void HomeScreen::update(uint32_t now) {
  if (hatch != Hatch::None) {
    updateHatch(now);
    if (hatch == Hatch::Cracks) {
      const uint32_t frame = petart::frameKey(now);
      if (frame != shownFrame) { shownFrame = frame; drawPet(now); }
    }
    return;
  }
  // hold Feed + Play for PET_HATCH_HOLD_MS: the egg hatches (ready or not: "hatch now"), or a pet that can evolve evolves
  const bool egg = !pet::hatched(), canEvo = !egg && pet::canEvolve();
  const bool both = (egg || canEvo) && input::isDown(Btn::Feed) && input::isDown(Btn::Play);
  if (both) {
    if (!holdStart) { holdStart = now; if (egg) petart::eggHeld(true); }
    const float k = min<uint32_t>(PET_HATCH_HOLD_MS, now - holdStart) / (float)PET_HATCH_HOLD_MS;
    if (canEvo) petart::evolveHold(k);   // it keeps moving, then settles + turns white at the end
    const int step = (now - holdStart) / 100;   // the bar 10 times a second (only the bar: the text stays)
    if (step != shownHold) {
      if (shownHold < 0) { if (egg) drawEggRows(); else drawInfo(); }
      else barOnly(3, k, col::RED);
      shownHold = step;
    }
    if (now - holdStart >= PET_HATCH_HOLD_MS) {
      holdStart = 0; shownHold = -1;
      evolving = canEvo;
      phaseAt = now;
      if (egg) { hatch = Hatch::Cracks; petart::startHatch(now); drawEggRows(); }
      else {   // the evolution show: dark screen, the line of text, then the silhouettes (update)
        hatch = Hatch::Evolving;
        char buf[40];
        snprintf(buf, sizeof buf, "What? %s is evolving!", pet::name());
        tft.fillScreen(C_NIGHT);
        tft.setTextColor(col::WHITE, C_NIGHT);
        tft.setTextDatum(MC_DATUM);
        ui::text(tft, buf, SCREEN_W / 2, EVO_TEXT_Y, ui::fit(tft, buf, SCREEN_W - 4 * SAFE_MARGIN, ui::BODY));
        petart::evolveHold(-1);
        lastShowDraw = 0;
      }
      return;   // the show owns the screen from here: no normal redraw (it painted light bars over the dark screen)
    }
  } else if (holdStart) {   // let go early: nothing happens
    holdStart = 0; shownHold = -1;
    if (egg) { petart::eggHeld(false); drawEggRows(); }
    else { petart::evolveHold(-1); drawInfo(); }
  }
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
  if (hatch != Hatch::None && (e.button == Btn::Feed || e.button == Btn::Play)) return true;
  if (!pet::hatched() && (e.button == Btn::Feed || e.button == Btn::Play)) return true;   // egg: only hold both
  if (e.type != Ev::Press) return false;
  if ((e.button == Btn::Feed || e.button == Btn::Play) && petart::busy(millis())) return true;   // let it finish
  if (e.button == Btn::Feed) { petart::reactTo(pet::feed(), petart::Reaction::Feed, millis()); return true; }
  if (e.button == Btn::Play) { petart::reactTo(pet::play(), petart::Reaction::Play, millis()); return true; }
  return false;
}
