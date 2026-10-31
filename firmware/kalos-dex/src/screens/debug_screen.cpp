#include "debug_screen.h"
#include "display.h"
#include "ui.h"
#include "storage.h"
#include "pet.h"
#include "pet_line.h"

enum Row { R_EGG, R_STAGE, R_SHINY, R_FOOD, R_FUN, R_ENERGY, R_LEVEL, R_SKIP, R_ANGRY, ROWS };
static const char *LABEL[ROWS] = {"Egg", "Stage", "Shiny", "Food", "Fun", "Energy", "Level", "Skip time", "Angry"};
static const char *ACT[ROWS][2] = {{"hatch now", "ready now"}, {"-1", "+1"}, {"off", "on"}, {"-10", "+10"},
                                   {"-10", "+10"}, {"-10", "+10"}, {"+1 Lv", "evolve Lv"}, {"+1 h", "+8 h"},
                                   {"calm", "make angry"}};

static constexpr int HEAD_H = TOP_BAND_H, ROW_Y0 = HEAD_H + 4, ROW_H = 26;   // 9 rows end at 282
static constexpr int ROW_W = SCREEN_W - 2 * SAFE_MARGIN, FOOT_Y = ROW_Y0 + ROWS * ROW_H + 2;
static TFT_eSprite row(&tft);
static constexpr uint16_t C_CARD = 0x95B9, C_TRACK_ON = 0x4AD0;   // as in Settings
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static const uint16_t C_BAR[3] = {rgb(240, 110, 20), rgb(235, 70, 150), rgb(40, 175, 70)};   // the Home bar colours

void DebugScreen::drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEAD_H, col::RED);
  tft.setTextColor(col::WHITE, col::RED);
  tft.setTextDatum(ML_DATUM);
  ui::text(tft, "DEBUG", SAFE_MARGIN + 6, TITLE_Y, ui::TITLE);
  tft.setTextDatum(MR_DATUM);
  ui::text(tft, "test pet", SCREEN_W - SAFE_MARGIN - 6, TITLE_Y, ui::BODY);
}

void DebugScreen::drawRow(int i) {
  if (!row.created()) return;
  const bool on = i == sel;
  const uint16_t bg = on ? col::SCREEN_DARK : col::SCREEN_BG, fg = on ? col::WHITE : col::TEXT;
  row.fillSprite(col::SCREEN_BG);
  if (on) row.fillRoundRect(0, 2, ROW_W, ROW_H - 4, 8, bg);
  else row.drawRoundRect(0, 2, ROW_W, ROW_H - 4, 8, C_CARD);
  row.setTextColor(fg, bg);
  row.setTextDatum(ML_DATUM);
  ui::text(row, LABEL[i], 8, ROW_H / 2, ui::BODY);
  row.setTextDatum(MR_DATUM);
  char buf[28] = "";
  switch (i) {
    case R_EGG:
      if (pet::hatched()) snprintf(buf, sizeof buf, "hatched");
      else if (pet::eggReady()) snprintf(buf, sizeof buf, "READY");
      else snprintf(buf, sizeof buf, "ready in %lu min", (unsigned long)(pet::eggSecondsLeft() + 59) / 60);
      break;
    case R_STAGE: if (pet::hatched()) snprintf(buf, sizeof buf, "%u %s", pet::stage(), pet::name()); else snprintf(buf, sizeof buf, "egg"); break;
    case R_SHINY: snprintf(buf, sizeof buf, pet::shiny() ? "on" : "off"); break;
    case R_FOOD: case R_FUN: case R_ENERGY: {
      const float v = i == R_FOOD ? pet::food() : i == R_FUN ? pet::fun() : pet::energy();
      ui::hpBar(row, 88, ROW_H / 2 - 6, 90, 12, v / 100, C_BAR[i - R_FOOD], on ? C_TRACK_ON : ui::TRACK);
      snprintf(buf, sizeof buf, "%d", (int)v);
      break;
    }
    case R_LEVEL:
      if (pet::hatched()) snprintf(buf, sizeof buf, "Lv %u  %lu/%lu", pet::level(), (unsigned long)pet::xp(), (unsigned long)pet::xpNeeded());
      break;
    case R_ANGRY: snprintf(buf, sizeof buf, pet::mood() == pet::Mood::Angry ? "yes" : "no"); break;
  }
  ui::text(row, buf, ROW_W - 8, ROW_H / 2, ui::BODY);
  row.pushSprite(SAFE_MARGIN, ROW_Y0 + i * ROW_H);
}

void DebugScreen::drawFooter() {   // the chosen row's two knob actions + what the last one did
  char buf[48];
  snprintf(buf, sizeof buf, "knob <  %s     > %s", ACT[sel][0], ACT[sel][1]);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.setTextPadding(SCREEN_W - 2 * SAFE_MARGIN);
  ui::text(tft, buf, SCREEN_W / 2, FOOT_Y, ui::fit(tft, buf, SCREEN_W - 4 * SAFE_MARGIN, ui::BODY));
  tft.setTextColor(col::RED_DARK, col::SCREEN_BG);
  ui::text(tft, msg, SCREEN_W / 2, FOOT_Y + 18, ui::SMALL);
  tft.setTextPadding(0);
}

void DebugScreen::act(bool right) {
  using namespace pet;
  const char *done = "";
  switch (sel) {
    case R_EGG:
      if (hatched()) done = "already hatched";
      else if (right) { eggReadyNow(); done = "ready: hold Feed+Play on Home"; }
      else { hatchNow(); done = "hatched (no show)"; }
      break;
    case R_STAGE:
      if (!hatched()) { done = "still an egg"; break; }
      setStage(constrain(stage() + (right ? 1 : -1), 1, 3));
      snprintf(msg, sizeof msg, "stage %u %s", stage(), name());
      break;
    case R_SHINY: setShiny(right); done = right ? "shiny on" : "shiny off"; break;
    case R_FOOD: setStat('f', food() + (right ? 10 : -10)); break;
    case R_FUN: setStat('u', fun() + (right ? 10 : -10)); break;
    case R_ENERGY: setStat('e', energy() + (right ? 10 : -10)); break;
    case R_LEVEL:
      if (!hatched()) { done = "still an egg"; break; }
      if (right) { if (!evolveReadyNow()) { done = nextEvolveLevel() ? "already there" : "no next form"; break; } }
      else levelUp();
      snprintf(msg, sizeof msg, "Lv %u  %lu/%lu", level(), (unsigned long)xp(), (unsigned long)xpNeeded());
      break;
    case R_SKIP: skipMinutes(right ? 480 : 60); done = right ? "skipped 8 h (stats only)" : "skipped 1 h (stats only)"; break;
    case R_ANGRY: if (right) { setCareHoursAgo(25); done = "no care for 25 h"; } else { calmDown(); done = "calm: cared for now"; } break;
  }
  if (*done) snprintf(msg, sizeof msg, "%s", done);
  else if (sel >= R_FOOD && sel <= R_ENERGY) snprintf(msg, sizeof msg, "%s %d", LABEL[sel], (int)(sel == R_FOOD ? food() : sel == R_FUN ? fun() : energy()));
  for (int i = 0; i < ROWS; i++) drawRow(i);   // one cheat can move several values
  shownVersion = version();
  drawFooter();
}

void DebugScreen::enter() {
  if (!storage::rememberPage()) sel = 0;
  tft.fillScreen(col::SCREEN_BG);
  row.setColorDepth(16);
  if (!row.createSprite(ROW_W, ROW_H)) Serial.println("debug: no RAM for the row strip");
  drawHeader();
  snprintf(msg, sizeof msg, "Feed/Play: choose a row");
  for (int i = 0; i < ROWS; i++) drawRow(i);
  shownVersion = pet::version();
  drawFooter();
}

void DebugScreen::leave() { row.deleteSprite(); }

void DebugScreen::update(uint32_t now) {
  if (pet::version() != shownVersion) {   // stats move on their own (decay, egg time)
    shownVersion = pet::version();
    for (int i = 0; i < ROWS; i++) drawRow(i);
  }
}

bool DebugScreen::onEvent(const InputEvent &e) {
  if (e.type == Ev::Turn) {   // same way round as Settings / the volume
    const int s = storage::knobVolDir() * e.steps;
    for (int k = 0; k < abs(s); k++) act(s > 0);
    return true;
  }
  if (e.button == Btn::Knob) return true;   // no play / pause here: the knob is for cheats
  if (e.button == Btn::Feed || e.button == Btn::Play) {
    if (e.type == Ev::Press) {
      const int old = sel;
      sel = (sel + (e.button == Btn::Play ? 1 : ROWS - 1)) % ROWS;
      drawRow(old); drawRow(sel);
      drawFooter();
    }
    return true;
  }
  return false;
}
