// Hardware-test screens: built only in the "hwtest" firmware (pio run -e hwtest).
#include "test_screens.h"
#include "display.h"
#include "storage.h"
#include "leds.h"
#include "timekeeper.h"
#include "pet.h"
#include "pet_art.h"

static void title(const char *t) {
  tft.fillRect(0, 0, SCREEN_W, 34, col::RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(col::WHITE, col::RED);
  tft.drawString(t, SCREEN_W / 2, 20, 4);
}
static void hint(const char *t) {
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.drawString(t, SCREEN_W / 2, SCREEN_H - SAFE_MARGIN - 2, 2);
}

// ---- Edges: coloured rings on the outermost pixels + the safe-margin frame
void EdgeScreen::enter() {
  tft.fillScreen(TFT_BLACK);
  // outermost 3 pixels: red, green, blue (the panel border hides ~2 of them), then a black gap,
  // then the safe-margin frame in yellow, 3 px thick (everything important stays inside it)
  const uint16_t ring[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  for (int i = 0; i < 3; i++) tft.drawRect(i, i, SCREEN_W - 2 * i, SCREEN_H - 2 * i, ring[i]);
  for (int i = 0; i < 3; i++)
    tft.drawRect(SAFE_MARGIN + i, SAFE_MARGIN + i, SCREEN_W - 2 * (SAFE_MARGIN + i), SCREEN_H - 2 * (SAFE_MARGIN + i), TFT_YELLOW);
  // top ruler (desk viewing angle): rows 0..23 in 2-px bands, alternating colours, numbered on the right.
  // "the first band I can fully see at my desk" = how many rows the case's top lip hides.
  const uint16_t band[] = {TFT_RED, TFT_WHITE, TFT_GREEN, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
  tft.setTextDatum(MR_DATUM);
  for (int y = 0; y < 24; y += 2) {
    const uint16_t c = band[(y / 2) % 6];
    tft.fillRect(40, y, SCREEN_W - 80, 2, c);
    if (y % 4 == 0) { tft.setTextColor(c, TFT_BLACK); tft.drawNumber(y, 36, y + 2, 1); }
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("EDGES", SCREEN_W / 2, 60, 4);
  tft.drawString("top ruler: rows 0-23, numbers left", SCREEN_W / 2, 180, 2);
  tft.drawString("header pins = TOP", SCREEN_W / 2, 100, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("yellow = safe margin", SCREEN_W / 2, 130, 2);
  tft.drawString("must be visible on all 4 sides", SCREEN_W / 2, 150, 2);
}

// ---- Input monitor: every event, live button states, knob click total
void InputScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  title("INPUT");
  hint("Screen btn: next   hold: home");
  dirty = true;
  lastClicks = INT32_MIN;
  for (bool &d : lastDown) d = !d;  // force the button boxes to redraw
  update(millis());
}
bool InputScreen::onEvent(const InputEvent &e) {
  for (int i = 5; i > 0; i--) memcpy(log[i], log[i - 1], sizeof log[0]);
  if (e.type == Ev::Turn) snprintf(log[0], sizeof log[0], "turn %+d", e.steps);
  else snprintf(log[0], sizeof log[0], "%s %s", input::name(e.button),
                e.type == Ev::Press ? "press" : e.type == Ev::Hold ? "HOLD" : "DOUBLE");
  Serial.printf("input: %s\n", log[0]);
  dirty = true;
  // logged, but the Back button still works for getting around
  if (e.type == Ev::Turn) return true;
  return !(e.button == Btn::Screen || e.button == Btn::Knob);
}
void InputScreen::update(uint32_t now) {
  // live button boxes
  // (Play/Pause button dropped 2026-10-19)
  static const Btn order[] = {Btn::Feed, Btn::Play, Btn::Knob, Btn::Screen};
  static const char *lab[] = {"Feed", "Play", "Knob", "Back"};
  for (int i = 0; i < 4; i++) {
    bool d = input::isDown(order[i]);
    if (d == lastDown[i]) continue;
    lastDown[i] = d;
    int x = SAFE_MARGIN + 6 + i * 46, y = 44;
    tft.fillRoundRect(x, y, 42, 26, 5, d ? col::RED : col::SILVER);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(d ? col::WHITE : col::TEXT, d ? col::RED : col::SILVER);
    tft.drawString(lab[i], x + 21, y + 13, 2);
  }
  // knob total + event log, drawn off-screen
  int32_t c = input::totalClicks();
  if (!dirty && c == lastClicks) return;
  dirty = false;
  lastClicks = c;
  TFT_eSprite s(&tft);
  s.setColorDepth(16);
  if (!s.createSprite(SCREEN_W - 2 * SAFE_MARGIN - 8, 200)) return;
  s.fillSprite(col::SCREEN_BG);
  s.setTextDatum(TL_DATUM);
  s.setTextColor(col::RED_DARK, col::SCREEN_BG);
  char buf[24];
  snprintf(buf, sizeof buf, "knob %ld", (long)c);
  s.drawString(buf, 0, 0, 4);
  s.setTextColor(col::TEXT, col::SCREEN_BG);
  for (int i = 0; i < 6; i++) s.drawString(log[i], 0, 40 + i * 24, 2);
  s.pushSprite(SAFE_MARGIN + 8, 80);
  s.deleteSprite();
}

// ---- Lights test: strip effects, brightness, limiter, calibration; status LEDs blinking
static const uint32_t COLOURS[] = {0xFFB040, 0xFF2000, 0x2040FF, 0x20FF40, 0xFF40A0, 0xFFFFFF};
static const char *COLOUR_NAMES[] = {"day yellow", "battle red", "night blue", "green", "pink", "white"};
void LightsScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  title("LIGHTS");
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  const char *help[] = {"Feed / Play: effect - / +", "knob: brightness (test only)",
                        "knob press: colour  hold: calibrate", "calibrate: knob = LED, Feed = hide/show"};
  for (int i = 0; i < 4; i++) tft.drawString(help[i], SAFE_MARGIN + 8, 236 + i * 18, 2);
  pet::holdGlow(true);   // borrow the strip + status LEDs from the pet's mood glow
  leds::setStatus(leds::Status::Green, leds::Mode::On);
  leds::setStatus(leds::Status::Yellow, leds::Mode::BlinkSlow);
  leds::setStatus(leds::Status::Red, leds::Mode::BlinkFast);
  leds::setEffect((leds::Effect)effect, COLOURS[colourIdx]);
  dirty = true;
}
void LightsScreen::leave() {
  calIdx = -1; leds::calibrate(-1);
  for (auto s : {leds::Status::Green, leds::Status::Yellow, leds::Status::Red}) leds::setStatus(s, leds::Mode::Off);
  pet::holdGlow(false);  // the pet's mood glow + red LED come back
}
bool LightsScreen::onEvent(const InputEvent &e) {
  const int N = (int)leds::Effect::COUNT;
  if (e.type == Ev::Turn) {
    if (calIdx >= 0) { calIdx = constrain(calIdx + e.steps, 0, STRIP_LEN - 1); leds::calibrate(calIdx); }
    else {
      leds::setBrightness(constrain(leds::brightness() + e.steps * 5, 0, 255));
      storage::settings().stripBrightness = leds::brightness();
      storage::settingsChanged();
    }
  } else if (e.type == Ev::Press && e.button == Btn::Feed) {
    if (calIdx >= 0) leds::setVisible(calIdx, !leds::isVisible(calIdx));
    else effect = (effect + N - 1) % N;
  } else if (e.type == Ev::Press && e.button == Btn::Play) {
    effect = (effect + 1) % N;
  } else if (e.type == Ev::Press && e.button == Btn::Knob) {   // was Play/Pause (dropped)
    colourIdx = (colourIdx + 1) % 6;
  } else if (e.type == Ev::Hold && e.button == Btn::Knob) {    // was Play/Pause hold
    calIdx = calIdx < 0 ? 0 : -1;
    leds::calibrate(calIdx);
    if (calIdx < 0) {   // leaving calibration: save the map
      storage::settings().visibleMask = leds::visibleMask();
      storage::settingsChanged();
      Serial.print("visible map: ");
      for (int i = 0; i < STRIP_LEN; i++) Serial.print(leds::isVisible(i) ? '1' : '0');   // LED 1 first
      Serial.println();
    }
  } else if (e.type == Ev::Hold && e.button == Btn::Play) {
    leds::flash(0xFFFFFF, 300);   // "level up" style flash
  } else {
    return false;
  }
  leds::setEffect((leds::Effect)effect, COLOURS[colourIdx]);
  dirty = true;
  return true;
}
void LightsScreen::update(uint32_t now) {
  if (!dirty && now - lastDraw < 500) return;   // mA numbers refresh twice a second
  dirty = false; lastDraw = now;
  TFT_eSprite s(&tft);
  s.setColorDepth(16);
  if (!s.createSprite(SCREEN_W - 2 * SAFE_MARGIN - 8, 180)) return;
  s.fillSprite(col::SCREEN_BG);
  s.setTextDatum(TL_DATUM);
  s.setTextColor(col::TEXT, col::SCREEN_BG);
  char line[40];
  int y = 0;
  auto row = [&](const char *k, const char *v) { s.drawString(k, 0, y, 2); s.drawString(v, 100, y, 2); y += 24; };
  if (calIdx >= 0) {
    s.setTextColor(col::RED_DARK, col::SCREEN_BG);
    snprintf(line, sizeof line, "CALIBRATE: LED %d", calIdx + 1);   // people count from 1
    s.drawString(line, 0, y, 4); y += 36;
    s.setTextColor(col::TEXT, col::SCREEN_BG);
    row("this LED", leds::isVisible(calIdx) ? "VISIBLE" : "hidden");
    row("hold knob", "to finish");
  } else {
    row("effect", leds::effectName((leds::Effect)effect));
    row("colour", COLOUR_NAMES[colourIdx]);
    snprintf(line, sizeof line, "%u / 255", leds::brightness()); row("brightness", line);
    snprintf(line, sizeof line, "%d mA", leds::stripMilliamps()); row("strip now", line);
    snprintf(line, sizeof line, "%d mA (max %d)", leds::totalMilliamps(), TOTAL_BUDGET_MA); row("whole gadget", line);
    snprintf(line, sizeof line, "%d LEDs", STRIP_LEN); row("strip length", line);
  }
  s.pushSprite(SAFE_MARGIN + 8, 46);
  s.deleteSprite();
}

// ---- Info: version, free memory, uptime (refreshes twice a second)
void InfoScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  title("INFO");
  hint("knob press: next   hold: home");
  lastDraw = 0;
  update(millis());
}
void InfoScreen::update(uint32_t now) {
  if (lastDraw && now - lastDraw < 500) return;
  lastDraw = now;
  TFT_eSprite s(&tft);
  s.setColorDepth(16);
  if (!s.createSprite(SCREEN_W - 2 * SAFE_MARGIN - 8, 250)) return;
  s.fillSprite(col::SCREEN_BG);
  s.setTextColor(col::TEXT, col::SCREEN_BG);
  s.setTextDatum(TL_DATUM);
  char line[48];
  int y = 0;
  auto row = [&](const char *k, const char *v) { s.drawString(k, 0, y, 2); s.drawString(v, 110, y, 2); y += 22; };
  row("firmware", FW_VERSION);
  snprintf(line, sizeof line, "%u KB", ESP.getFreeHeap() / 1024); row("free RAM", line);
  snprintf(line, sizeof line, "%u KB", ESP.getMinFreeHeap() / 1024); row("lowest RAM", line);
  snprintf(line, sizeof line, "%lu s", now / 1000); row("uptime", line);
  snprintf(line, sizeof line, "%u MHz", getCpuFrequencyMhz()); row("CPU", line);
  snprintf(line, sizeof line, "%u KB", ESP.getSketchSize() / 1024); row("program", line);
  snprintf(line, sizeof line, "%lu", (unsigned long)storage::settings().bootCount); row("boot count", line);
  snprintf(line, sizeof line, "seq %lu / slot %c", (unsigned long)storage::settingsSeq(), storage::settingsSlot()); row("saved copy", line);
  snprintf(line, sizeof line, "%u (strip)", storage::settings().stripBrightness); row("brightness", line);
  {  // hidden strip LEDs as saved in flash, counted from 1
    char hid[40] = ""; int n = 0;
    for (int i = 0; i < STRIP_LEN && n < 34; i++)
      if (!((storage::settings().visibleMask >> i) & 1)) n += snprintf(hid + n, sizeof hid - n, "%d ", i + 1);
    row("hidden LEDs", n ? hid : "none");
  }
  size_t used, total; storage::nvsUsage(used, total);
  snprintf(line, sizeof line, "%u / %u entries", (unsigned)used, (unsigned)total); row("flash store", line);
  s.pushSprite(SAFE_MARGIN + 8, 50);
  s.deleteSprite();
}

// ---- Clock test (F5): big Pakistan time + where it came from, RTC state, last PC sync
void ClockTestScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  title("CLOCK TEST");
  hint("PC sets the time over USB");
  lastDraw = 0; lastShown = -1;
  update(millis());
}
void ClockTestScreen::update(uint32_t now) {
  const time_t t = timekeeper::nowUtc();
  if (t == lastShown && now - lastDraw < 2000) return;   // once a second (+ the RTC line every 2 s)
  lastShown = t; lastDraw = now;
  static const char *DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char hm[12] = "--:--", ss[8] = "", date[32] = "time not set";
  struct tm lt;
  if (timekeeper::localTime(lt)) {
    const int h = !storage::clock24h() ? (lt.tm_hour % 12 ? lt.tm_hour % 12 : 12) : lt.tm_hour;
    snprintf(hm, sizeof hm, "%d:%02d", h, lt.tm_min);
    snprintf(ss, sizeof ss, ":%02d %s", lt.tm_sec, !storage::clock24h() ? (lt.tm_hour < 12 ? "AM" : "PM") : "");
    snprintf(date, sizeof date, "%s %d %s %d", DAYS[lt.tm_wday], lt.tm_mday, MONTHS[lt.tm_mon], lt.tm_year + 1900);
  }
  TFT_eSprite s(&tft);
  s.setColorDepth(16);
  if (!s.createSprite(SCREEN_W - 2 * SAFE_MARGIN - 8, 230)) return;
  s.fillSprite(col::SCREEN_BG);
  s.setTextDatum(TC_DATUM);
  s.setTextColor(col::RED_DARK, col::SCREEN_BG);
  s.drawString(hm, s.width() / 2, 4, 7);
  s.setTextColor(col::TEXT, col::SCREEN_BG);
  s.drawString(ss, s.width() / 2, 60, 4);
  s.drawString(date, s.width() / 2, 92, 4);
  s.setTextDatum(TL_DATUM);
  char v[40];
  int y = 136;
  auto row = [&](const char *k, const char *val) { s.drawString(k, 0, y, 2); s.drawString(val, 96, y, 2); y += 22; };
  row("time from", timekeeper::sourceName());
  row("RTC chip", timekeeper::rtcStateName());
  if (timekeeper::lastPcSyncMs()) {
    snprintf(v, sizeof v, "%lu s ago", (now - timekeeper::lastPcSyncMs()) / 1000); row("PC sync", v);
    if (timekeeper::hasLastDrift()) { snprintf(v, sizeof v, "%+ld s", timekeeper::lastDrift()); row("RTC was off", v); }
    else row("RTC was off", "(RTC had no time)");
  } else {
    row("PC sync", "not yet");
  }
  s.pushSprite(SAFE_MARGIN + 4, 44);
  s.deleteSprite();
}

// ---- Display: gamma presets + VCOM for the TN panel (user picks by eye at the desk angle, PLAN §3d)
// ILI9341 commands: 0xC5 VCOM 1 (2 bytes), 0xC7 VCOM 2 / offset (1 byte), 0x26 gamma curve select (1 = 2.2,
// 2 = 1.8, 4 = 2.5, 8 = 1.0 – not every module has all four), 0xE0 / 0xE1 positive / negative gamma (15 bytes each).
struct GammaPreset { const char *name; bool libraryInit; uint8_t c5[2], c7, curve, e0[15], e1[15]; };
static const GammaPreset PRESETS[] = {
  {"library, no VCOM fix", true, {}, 0, 0, {}, {}},   // the look before DISPLAY_VCOM
  {"standard set", false, {0x3E, 0x28}, 0x86, 0x01,
   {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00},
   {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}},
  {"alternative set", false, {0x3E, 0x28}, 0x86, 0x01,
   {0x0F, 0x26, 0x24, 0x0B, 0x0E, 0x09, 0x54, 0xA8, 0x46, 0x0C, 0x17, 0x09, 0x0F, 0x07, 0x00},
   {0x00, 0x19, 0x1B, 0x04, 0x10, 0x07, 0x2A, 0x47, 0x39, 0x03, 0x06, 0x06, 0x30, 0x38, 0x0F}},
  {"curve 1.8", false, {0x3E, 0x28}, 0x86, 0x02, {}, {}},
  {"curve 2.5", false, {0x3E, 0x28}, 0x86, 0x04, {}, {}},
  {"curve 1.0", false, {0x3E, 0x28}, 0x86, 0x08, {}, {}},
};
static constexpr int PRESET_COUNT = sizeof PRESETS / sizeof PRESETS[0];
static TFT_eSprite petPic(&tft);   // the pet + shadow, as on Home (made in enter, freed in leave)

static void cmd(uint8_t c, const uint8_t *d, int n) {
  tft.writecommand(c);
  for (int i = 0; i < n; i++) tft.writedata(d[i]);
}

void DisplayScreen::apply() {
  const GammaPreset &g = PRESETS[preset];
  if (g.libraryInit) {   // exactly what boot does: a fresh init (clears the screen)
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
  } else {
    cmd(0xC5, g.c5, 2);
    cmd(0x26, &g.curve, 1);
    if (g.e0[0]) { cmd(0xE0, g.e0, 15); cmd(0xE1, g.e1, 15); }   // curve presets keep the gamma tables as they are
  }
  if (vcom >= 0) { const uint8_t v = vcom; cmd(0xC7, &v, 1); }
  else if (!g.libraryInit) cmd(0xC7, &g.c7, 1);
  Serial.printf("display: preset %d '%s', VCOM %s\n", preset, g.name, vcom >= 0 ? String(vcom, HEX).c_str() : "preset");
}

void DisplayScreen::draw() {
  tft.fillScreen(col::SCREEN_BG);
  title("DISPLAY");
  const int y0 = 40, w = 14;
  for (int i = 0; i < 16; i++) {   // grey ramp + red / green / blue ramps, 16 steps each
    const uint8_t v = i * 17;
    tft.fillRect(8 + i * w, y0, w, 14, tft.color565(v, v, v));
    tft.fillRect(8 + i * w, y0 + 16, w, 10, tft.color565(v, 0, 0));
    tft.fillRect(8 + i * w, y0 + 28, w, 10, tft.color565(0, v, 0));
    tft.fillRect(8 + i * w, y0 + 40, w, 10, tft.color565(0, 0, v));
  }
  // the Home bars in their real colours (home_screen.cpp)
  const uint16_t BAR[4] = {tft.color565(255, 150, 40), tft.color565(255, 90, 170), tft.color565(60, 200, 90),
                           tft.color565(60, 140, 255)};
  static const char *NAME[4] = {"Food", "Fun", "Energy", "XP"};
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  for (int r = 0; r < 4; r++) {
    const int y = 100 + r * 16;
    tft.drawString(NAME[r], 10, y + 5, 2);
    tft.drawRect(64, y, 120, 10, col::SCREEN_DARK);
    tft.fillRect(65, y + 1, 70, 8, BAR[r]);
    tft.fillRect(135, y + 1, 48, 8, col::SILVER);
  }
  if (petPic.created()) {   // the pet with its shadow on the real background
    petPic.fillSprite(col::SCREEN_BG);
    petart::draw(petPic, petPic.width() / 2, petPic.height() - 10, 0, false);
    petPic.pushSprite((SCREEN_W - petPic.width()) / 2, 166);
  }
  char buf[48];
  snprintf(buf, sizeof buf, "%d/%d %s", preset + 1, PRESET_COUNT, PRESETS[preset].name);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.drawString(buf, SCREEN_W / 2, 284, 2);
  if (vcom >= 0) snprintf(buf, sizeof buf, "VCOM 0x%02X (knob)", vcom);
  else snprintf(buf, sizeof buf, "VCOM: preset's own (turn knob)");
  tft.drawString(buf, SCREEN_W / 2, 300, 2);
}

void DisplayScreen::enter() {
  petPic.setColorDepth(16);
  if (!petPic.createSprite(140, 112)) Serial.println("display test: no RAM for the pet picture");
  draw();
}

void DisplayScreen::leave() { petPic.deleteSprite(); }

bool DisplayScreen::onEvent(const InputEvent &e) {
  if (e.type == Ev::Press && (e.button == Btn::Feed || e.button == Btn::Play)) {
    preset = (preset + (e.button == Btn::Play ? 1 : PRESET_COUNT - 1)) % PRESET_COUNT;
    vcom = -1;
    apply();
    draw();
    return true;
  }
  if (e.type == Ev::Turn) {   // VCOM offset: 0x80 = none; above / below shifts the panel's contrast
    vcom = constrain((vcom < 0 ? DISPLAY_VCOM : vcom) + e.steps, 0x80, 0xBF);   // one step per click
    apply();
    draw();
    return true;
  }
  return false;
}

// ---- glow per mood (step 2026-10-28-06)
static const struct { pet::Mood m; const char *look; } GLOWS[] = {
  {pet::Mood::Happy, "green, breathing 3 s"}, {pet::Mood::Okay, "warm white, steady"},
  {pet::Mood::Hungry, "deep orange beat"}, {pet::Mood::Tired, "teal, slow breathe 7 s"},
  {pet::Mood::Sad, "purple, slow breathe 6 s"}, {pet::Mood::Angry, "fast red beat"},
  {pet::Mood::Sleeping, "dim navy, steady"}, {pet::Mood::Egg, "warm white, slow breathe"}};
static constexpr int GLOW_COUNT = sizeof(GLOWS) / sizeof(GLOWS[0]);

void GlowScreen::draw() {
  tft.fillScreen(col::SCREEN_BG);
  title("Glow");
  char buf[24];
  snprintf(buf, sizeof buf, "%d / %d", idx + 1, GLOW_COUNT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.drawString(buf, SCREEN_W / 2, 90, 2);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  tft.drawString(pet::moodName(GLOWS[idx].m), SCREEN_W / 2, 140, 4);
  tft.drawString(GLOWS[idx].look, SCREEN_W / 2, 180, 2);
  hint("Feed / Play: previous / next");
  pet::showGlow(GLOWS[idx].m);
  Serial.printf("glow test: %s (%s)\n", pet::moodName(GLOWS[idx].m), GLOWS[idx].look);
}

void GlowScreen::enter() { pet::holdGlow(true); draw(); }
void GlowScreen::leave() { pet::holdGlow(false); }   // the pet's real mood glow comes back

bool GlowScreen::onEvent(const InputEvent &e) {
  if (e.type == Ev::Press && (e.button == Btn::Feed || e.button == Btn::Play)) {
    idx = (idx + (e.button == Btn::Play ? 1 : GLOW_COUNT - 1)) % GLOW_COUNT;
    draw();
    return true;
  }
  return false;
}
