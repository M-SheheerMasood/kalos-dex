#include "settings_screen.h"
#include "display.h"
#include "ui.h"
#include "storage.h"
#include "leds.h"
#include "pet.h"
#include "system.h"
#include "night.h"

enum Row { R_SCREEN, R_LIGHTS, R_NIGHT, R_CLOCK, R_KNOB, R_PET, R_HATCH, R_DEBUG, R_PAGE, ROWS };
static const char *LABEL[ROWS] = {"Screen", "Lights", "Night dim", "Clock", "Knob direction", "Reset pet", "Hatch time", "Debug", "Remember page"};
static constexpr int PAGE_ROWS = 6, PAGES = (ROWS + PAGE_ROWS - 1) / PAGE_ROWS;   // 2nd page (user, 2026-10-29)
static const uint8_t HATCH_MIN[] = {1, 10, 20, 30, 40, 50, 60};

static constexpr int HEAD_H = TOP_BAND_H, ROW_Y0 = HEAD_H + 6, ROW_H = 32;
static constexpr int ROW_W = SCREEN_W - 2 * SAFE_MARGIN;
static constexpr uint32_t RESET_HOLD_MS = 3000, DONE_MS = 2000;
static TFT_eSprite row(&tft);
static constexpr uint16_t C_CARD = 0x95B9, C_TRACK_ON = 0x4AD0;   // card outline rgb(150,180,205), bar track on the dark row

static int screenPct() { return (storage::backlightLevel() * 100 + BACKLIGHT_NORMAL / 2) / BACKLIGHT_NORMAL; }
static int lightsPct() { return (storage::settings().stripBrightness * 100 + 127) / 255; }

void SettingsScreen::drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEAD_H, col::RED);
  tft.setTextColor(col::WHITE, col::RED);
  tft.setTextDatum(ML_DATUM);
  ui::text(tft, "SETTINGS", SAFE_MARGIN + 6, TITLE_Y, ui::TITLE);
  char buf[8];
  snprintf(buf, sizeof buf, "%d/%d", sel / PAGE_ROWS + 1, PAGES);
  tft.setTextDatum(MR_DATUM);
  ui::text(tft, buf, SCREEN_W - SAFE_MARGIN - 6, TITLE_Y, ui::BODY);
}

void SettingsScreen::drawPage() {
  drawHeader();
  const int first = sel / PAGE_ROWS * PAGE_ROWS;
  for (int k = 0; k < PAGE_ROWS; k++) {
    if (first + k < ROWS) drawRow(first + k);
    else tft.fillRect(SAFE_MARGIN, ROW_Y0 + k * ROW_H, ROW_W, ROW_H, col::SCREEN_BG);   // page 2 has fewer rows
  }
}

// one row: label left, value right (bar + number, or text); the chosen row is dark with white text
void SettingsScreen::drawRow(int i) {
  if (!row.created() || i / PAGE_ROWS != sel / PAGE_ROWS) return;   // not on the page shown
  const bool on = i == sel;
  const uint16_t bg = on ? col::SCREEN_DARK : col::SCREEN_BG, fg = on ? col::WHITE : col::TEXT;
  row.fillSprite(col::SCREEN_BG);
  if (on) row.fillRoundRect(0, 2, ROW_W, ROW_H - 4, 8, bg);
  else row.drawRoundRect(0, 2, ROW_W, ROW_H - 4, 8, C_CARD);   // the others: a thin outline, like cards
  row.setTextColor(fg, bg);
  row.setTextDatum(ML_DATUM);
  ui::text(row, LABEL[i], 8, ROW_H / 2, ui::BODY);
  row.setTextDatum(MR_DATUM);
  char buf[24];
  const uint16_t track = on ? C_TRACK_ON : ui::TRACK;
  auto bar = [&](int pct, const char *txt) {   // value bar 70 px + text
    ui::hpBar(row, 104, ROW_H / 2 - 6, 70, 12, pct / 100.0f, on ? col::WHITE : col::RED, track);
    ui::text(row, txt, ROW_W - 8, ROW_H / 2, ui::BODY);
  };
  switch (i) {
    case R_SCREEN: snprintf(buf, sizeof buf, "%d%%", screenPct()); bar(screenPct(), buf); break;
    case R_LIGHTS: snprintf(buf, sizeof buf, lightsPct() ? "%d%%" : "off", lightsPct()); bar(lightsPct(), buf); break;
    case R_NIGHT:
      if (storage::settings().flags & SET_NO_NIGHT_DIM) ui::text(row, "off", ROW_W - 8, ROW_H / 2, ui::BODY);
      else ui::text(row, night::active() ? "on (now)" : "on 0:15-7:00", ROW_W - 8, ROW_H / 2, ui::BODY);
      break;
    case R_CLOCK: ui::text(row, storage::clock24h() ? "24 h" : "12 h", ROW_W - 8, ROW_H / 2, ui::BODY); break;
    case R_KNOB: ui::text(row, storage::settings().flags & SET_KNOB_FLIP ? "flipped" : "normal", ROW_W - 8, ROW_H / 2, ui::BODY); break;
    case R_PET:
      if (doneAt) ui::text(row, "New egg!", ROW_W - 8, ROW_H / 2, ui::BODY);
      else if (holdStart) {   // filling up while Feed + Play are held
        const int pct = min<uint32_t>(100, (millis() - holdStart) * 100 / RESET_HOLD_MS);
        ui::hpBar(row, 104, ROW_H / 2 - 6, ROW_W - 104 - 8, 12, pct / 100.0f, col::RED, track);
      } else ui::text(row, "hold Feed+Play", ROW_W - 8, ROW_H / 2, ui::BODY);
      break;
    case R_PAGE: ui::text(row, storage::rememberPage() ? "on" : "off", ROW_W - 8, ROW_H / 2, ui::BODY); break;
    case R_DEBUG: ui::text(row, pet::testMode() ? "on: next screen" : "off", ROW_W - 8, ROW_H / 2, ui::BODY); break;
    case R_HATCH:
      snprintf(buf, sizeof buf, "%u min", (unsigned)(storage::hatchSeconds() / 60));
      ui::text(row, buf, ROW_W - 8, ROW_H / 2, ui::BODY);
      break;
  }
  row.pushSprite(SAFE_MARGIN, ROW_Y0 + i % PAGE_ROWS * ROW_H);
}

// On / off rows follow the knob's DIRECTION (right = on, left = off), a press flips them: a quick twist can arrive as
// 2-3 events, and flipping per event cancelled itself out (felt like a slow / dead knob, user 2026-10-31).
// `raw` = the physical direction (Knob direction itself must not depend on the flag it changes).
void SettingsScreen::change(int steps, int raw, bool press) {
  auto setFlag = [&](uint8_t &flags, uint8_t bit, bool on) { if (on) flags |= bit; else flags &= ~bit; };
  Settings &s = storage::settings();
  switch (sel) {
    case R_SCREEN: {
      const int pct = constrain(screenPct() + steps * 10, 10, 100);
      s.backlight = max<int>(BACKLIGHT_MIN, BACKLIGHT_NORMAL * pct / 100);
      display::setBacklight(s.backlight);   // shown at once; at night night::update() takes it back down to the cap
      break;
    }
    case R_LIGHTS: {
      const int pct = constrain(lightsPct() + steps * 5, 0, 100);
      s.stripBrightness = pct * 255 / 100;
      break;
    }
    case R_NIGHT: setFlag(s.flags, SET_NO_NIGHT_DIM, press ? !(s.flags & SET_NO_NIGHT_DIM) : steps < 0); break;   // right = dim on
    case R_CLOCK: setFlag(s.flags, SET_24H, press ? !(s.flags & SET_24H) : steps > 0); break;                      // right = 24 h
    case R_PAGE: setFlag(s.flags, SET_REMEMBER_PAGE, press ? !storage::rememberPage() : steps > 0); break;   // right = on
    case R_KNOB: setFlag(s.flags, SET_KNOB_FLIP, press ? !(s.flags & SET_KNOB_FLIP) : raw > 0); break;            // clockwise = flipped
    case R_HATCH: {   // 1, 10, 20 .. 60 min, stops at the ends
      int k = 0;
      while (k < (int)sizeof HATCH_MIN - 1 && HATCH_MIN[k] < storage::hatchSeconds() / 60) k++;
      s.hatchMin = HATCH_MIN[constrain(k + steps, 0, (int)sizeof HATCH_MIN - 1)];
      break;
    }
    case R_DEBUG: {   // not a setting: off after a restart; only switches when it really changes (copy / reload)
      const bool on = press ? !pet::testMode() : steps > 0;
      if (on != pet::testMode()) pet::setTestMode(on);
      drawRow(sel);
      return;
    }
    default: return;   // reset pet: only by holding Feed + Play
  }
  storage::settingsChanged();
  drawRow(sel);
}

void SettingsScreen::enter() {
  if (!storage::rememberPage()) sel = 0;   // "Remember page" off: open on the first row
  tft.fillScreen(col::SCREEN_BG);
  row.setColorDepth(16);
  if (!row.createSprite(ROW_W, ROW_H)) Serial.println("settings: no RAM for the row strip");
  holdStart = doneAt = 0;
  drawPage();
  // help + info underneath
  const int y = ROW_Y0 + PAGE_ROWS * ROW_H + 6;   // under one page of rows (the 2nd page has fewer)
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  // the case hides the screen's edges: a line wider than the safe width drops to the small font instead of being cut
  auto line = [](const char *s, int ly) {
    ui::text(tft, s, SCREEN_W / 2, ly, ui::fit(tft, s, SCREEN_W - 4 * SAFE_MARGIN, ui::BODY));
  };
  line("Feed/Play: choose   knob: change", y);
  char buf[48];
  snprintf(buf, sizeof buf, "fw %s  #%lu  RAM %u/%u KB", FW_VERSION, (unsigned long)storage::settings().bootCount,
           ESP.getFreeHeap() / 1024, heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
  line(buf, y + 22);
  snprintf(buf, sizeof buf, "last start: %s", sys::resetReason());
  if (sys::safeMode()) { tft.setTextColor(col::RED, col::SCREEN_BG); snprintf(buf, sizeof buf, "SAFE MODE (%s)", sys::safeReason()); }
  else if (sys::lastWasCrash()) tft.setTextColor(col::RED, col::SCREEN_BG);
  line(buf, y + 40);
}

void SettingsScreen::leave() { row.deleteSprite(); }

void SettingsScreen::update(uint32_t now) {
  // reset pet: Feed + Play held together for 3 s (only while its page is shown, so the bar is seen)
  const bool both = input::isDown(Btn::Feed) && input::isDown(Btn::Play) && sel / PAGE_ROWS == R_PET / PAGE_ROWS;
  if (doneAt && now - doneAt >= DONE_MS && !both) { doneAt = 0; drawRow(R_PET); }
  if (both && !doneAt) {
    if (!holdStart) holdStart = now;
    const int step = (now - holdStart) / 100;   // redraw the bar 10 times a second
    if (step != shownHold) { shownHold = step; drawRow(R_PET); }
    if (now - holdStart >= RESET_HOLD_MS) {
      pet::reset();
      holdStart = 0; doneAt = now;
      drawRow(R_PET);
    }
  } else if (!both && holdStart) {   // let go too early: nothing happens
    holdStart = 0; shownHold = -1;
    drawRow(R_PET);
  }
}

bool SettingsScreen::onEvent(const InputEvent &e) {
  if (e.type == Ev::Turn) { change(storage::knobVolDir() * e.steps, e.steps, false); return true; }   // same way round as the volume
  if (e.button == Btn::Knob) { if (e.type == Ev::Press) change(1, 1, true); return true; }   // press = next value / flip
  if (e.button == Btn::Feed || e.button == Btn::Play) {
    if (e.type == Ev::Press) {
      const int old = sel;
      sel = (sel + (e.button == Btn::Play ? 1 : ROWS - 1)) % ROWS;
      if (old / PAGE_ROWS != sel / PAGE_ROWS) drawPage();   // walked off the page: show the other one
      else { drawRow(old); drawRow(sel); }
    }
    return true;   // holds too (reset pet)
  }
  return false;
}
