#include "settings_screen.h"
#include "display.h"
#include "ui.h"
#include "storage.h"
#include "leds.h"
#include "pet.h"
#include "system.h"
#include "night.h"

enum Row { R_SCREEN, R_LIGHTS, R_NIGHT, R_CLOCK, R_KNOB, R_PET, ROWS };
static const char *LABEL[ROWS] = {"Screen", "Lights", "Night dim", "Clock", "Knob direction", "Reset pet"};

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
}

// one row: label left, value right (bar + number, or text); the chosen row is dark with white text
void SettingsScreen::drawRow(int i) {
  if (!row.created()) return;
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
  }
  row.pushSprite(SAFE_MARGIN, ROW_Y0 + i * ROW_H);
}

void SettingsScreen::change(int steps) {
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
    case R_NIGHT: s.flags ^= SET_NO_NIGHT_DIM; break;
    case R_CLOCK: s.flags ^= SET_24H; break;
    case R_KNOB: s.flags ^= SET_KNOB_FLIP; break;
    default: return;   // reset pet: only by holding Feed + Play
  }
  storage::settingsChanged();
  drawRow(sel);
}

void SettingsScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  row.setColorDepth(16);
  if (!row.createSprite(ROW_W, ROW_H)) Serial.println("settings: no RAM for the row strip");
  drawHeader();
  holdStart = doneAt = 0;
  for (int i = 0; i < ROWS; i++) drawRow(i);
  // help + info underneath
  const int y = ROW_Y0 + ROWS * ROW_H + 6;
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
  // reset pet: Feed + Play held together for 3 s
  const bool both = input::isDown(Btn::Feed) && input::isDown(Btn::Play);
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
  if (e.type == Ev::Turn) { change(storage::knobVolDir() * e.steps); return true; }   // same way round as the volume
  if (e.button == Btn::Knob) { if (e.type == Ev::Press) change(1); return true; }   // press = next value too
  if (e.button == Btn::Feed || e.button == Btn::Play) {
    if (e.type == Ev::Press) {
      const int old = sel;
      sel = (sel + (e.button == Btn::Play ? 1 : ROWS - 1)) % ROWS;
      drawRow(old); drawRow(sel);
    }
    return true;   // holds too (reset pet)
  }
  return false;
}
