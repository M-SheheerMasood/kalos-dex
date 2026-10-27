#include "dex_screen.h"
#include "display.h"
#include "ui.h"
#include "pet.h"
#include "pet_line.h"
#include "pet_art.h"
#include "timekeeper.h"

static constexpr int PAGES = 2;
static constexpr int PIC = 112, PIC_X = SAFE_MARGIN + 4, PIC_Y = TOP_BAND_H + 8;   // picture box
static constexpr int INFO_X = PIC_X + PIC + 10;                                       // right column
static constexpr int TEXT_Y = PIC_Y + PIC + 8, TEXT_H = 84;                           // entry text box
static constexpr int ROW_W = SCREEN_W - 2 * SAFE_MARGIN, ROW_H = 20, ROWS_Y = TOP_BAND_H + 8;

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static TFT_eSprite pic(&tft), row(&tft);

// type badge colours, like the games (unknown types: grey)
static uint16_t typeColour(const char *t) {
  static const struct { const char *n; uint16_t c; } T[] = {
    {"Normal", rgb(168, 168, 120)}, {"Fire", rgb(240, 128, 48)}, {"Water", rgb(104, 144, 240)}, {"Grass", rgb(120, 200, 80)},
    {"Electric", rgb(248, 208, 48)}, {"Ice", rgb(152, 216, 216)}, {"Fighting", rgb(192, 48, 40)}, {"Poison", rgb(160, 64, 160)},
    {"Ground", rgb(224, 192, 104)}, {"Flying", rgb(168, 144, 240)}, {"Psychic", rgb(248, 88, 136)}, {"Bug", rgb(168, 184, 32)},
    {"Rock", rgb(184, 160, 56)}, {"Ghost", rgb(112, 88, 152)}, {"Dragon", rgb(112, 56, 248)}, {"Dark", rgb(112, 88, 72)},
    {"Steel", rgb(150, 150, 176)}, {"Fairy", rgb(238, 153, 172)}};
  for (auto &x : T) if (!strcmp(x.n, t)) return x.c;
  return col::SILVER;
}

// word-wrap `text` into lines no wider than `w` (font 2); calls line(text, y) for each
template <typename F> static void wrap(const char *text, int w, int y0, int lineH, int maxLines, F line) {
  char buf[64];
  const char *p = text;
  for (int n = 0; *p && n < maxLines; n++) {
    int len = 0, lastSpace = -1;
    while (p[len]) {
      if (p[len] == ' ') lastSpace = len;
      if (len >= (int)sizeof buf - 1) break;
      memcpy(buf, p, len + 1); buf[len + 1] = 0;
      if (ui::width(tft, buf, ui::BODY) > w) break;
      len++;
    }
    if (p[len] && lastSpace > 0) len = lastSpace;   // break at the last space that fits
    memcpy(buf, p, len); buf[len] = 0;
    line(buf, y0 + n * lineH);
    p += len;
    while (*p == ' ') p++;
  }
}

static void fmtDate(char *buf, size_t n, uint32_t utc) {
  if (!utc) { snprintf(buf, n, "unknown"); return; }
  time_t t = utc;
  struct tm lt;
  localtime_r(&t, &lt);
  strftime(buf, n, "%d %b %Y", &lt);
}

static void fmtSpan(char *buf, size_t n, uint32_t s) {   // 3 d 4 h / 4 h 12 min / 12 min
  if (s >= 86400) snprintf(buf, n, "%lu d %lu h", (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24));
  else if (s >= 3600) snprintf(buf, n, "%lu h %lu min", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  else snprintf(buf, n, "%lu min", (unsigned long)(s / 60));
}

void DexScreen::drawHeader() {   // "No.901  Blobbit" (egg: "No.???  Egg")
  tft.fillRect(0, 0, SCREEN_W, TOP_BAND_H, col::RED);
  tft.setTextColor(col::WHITE, col::RED);
  tft.setTextDatum(ML_DATUM);
  char buf[32];
  if (pet::hatched()) snprintf(buf, sizeof buf, "No.%03u  %s", PET_LINE.stage[pet::stage() - 1].dexNo, pet::name());
  else snprintf(buf, sizeof buf, "No.???  Egg");
  ui::text(tft, buf, SAFE_MARGIN + 6, TITLE_Y, ui::TITLE);
}

// page dots + hint at the bottom
static void drawFooter(int page) {
  const int y = SCREEN_H - SAFE_MARGIN - 8;
  tft.fillRect(0, y - 9, SCREEN_W, 18, col::SCREEN_BG);   // = the font's line
  for (int i = 0; i < PAGES; i++) {
    const int x = SCREEN_W / 2 - 8 + i * 16;
    if (i == page) tft.fillCircle(x, y, 4, col::RED); else tft.drawCircle(x, y, 4, col::SCREEN_DARK);
  }
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  tft.setTextDatum(ML_DATUM);
  ui::text(tft, page == 0 ? "ENTRY" : "TOGETHER", SAFE_MARGIN + 6, y, ui::BODY);
  tft.setTextDatum(MR_DATUM);
  ui::text(tft, "Feed / Play", SCREEN_W - SAFE_MARGIN - 6, y, ui::BODY);
}

// ---- page 1: the Pokédex entry
void DexScreen::drawEntry() {
  // picture: still, on a white card
  if (pic.created()) {
    pic.fillSprite(col::WHITE);
    petart::draw(pic, PIC / 2, PIC - 12, 0, false);
    pic.drawRect(0, 0, PIC, PIC, col::SILVER);
    pic.pushSprite(PIC_X, PIC_Y);
  }
  const bool h = pet::hatched();
  const PetStage *st = h ? &PET_LINE.stage[pet::stage() - 1] : nullptr;
  char buf[40];
  int y = PIC_Y + 2;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col::SCREEN_DARK, col::SCREEN_BG);
  {   // category: one line, smaller if needed; still too wide ("Royal Sword Pokemon") -> two small lines
    const char *cat = h ? st->category : "Egg";
    const int cw = SCREEN_W - SAFE_MARGIN - 2 - INFO_X;
    const ui::Font cf = ui::fit(tft, cat, cw, ui::BODY);
    if (ui::width(tft, cat, cf) <= cw) { ui::text(tft, cat, INFO_X, y, cf); y += 22; }
    else {
      char a[32];
      snprintf(a, sizeof a, "%s", cat);
      char *sp = strrchr(a, ' ');
      if (sp) *sp = 0;
      ui::text(tft, a, INFO_X, y, ui::SMALL);
      if (sp) ui::text(tft, sp + 1, INFO_X, y + 13, ui::SMALL);
      y += 30;
    }
  }
  // type badges
  if (h) {
    int x = INFO_X;
    for (const char *t : {st->type1, st->type2}) {
      if (!t) continue;
      const int w = ui::width(tft, t, ui::SMALL) + 12;   // small font: two badges fit on one line
      if (x > INFO_X && x + w > SCREEN_W - SAFE_MARGIN) { x = INFO_X; y += 20; }   // second badge on its own line
      tft.fillRoundRect(x, y, w, 17, 8, typeColour(t));
      tft.setTextColor(col::WHITE, typeColour(t));
      tft.setTextDatum(MC_DATUM);
      ui::text(tft, t, x + w / 2, y + 9, ui::SMALL);
      x += w + 4;
    }
  }
  y += 28;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col::TEXT, col::SCREEN_BG);
  if (h) snprintf(buf, sizeof buf, "HT  %u.%u m", st->heightCm / 100, st->heightCm % 100 / 10); else snprintf(buf, sizeof buf, "HT  ???");
  ui::text(tft, buf, INFO_X, y, ui::BODY);
  y += 20;
  if (h) snprintf(buf, sizeof buf, "WT  %u.%u kg", st->weightHg / 10, st->weightHg % 10); else snprintf(buf, sizeof buf, "WT  ???");
  ui::text(tft, buf, INFO_X, y, ui::BODY);
  y += 20;
  if (pet::shiny()) { tft.setTextColor(col::RED, col::SCREEN_BG); ui::text(tft, "* shiny *", INFO_X, y, ui::BODY); }

  // the entry text in a white box
  tft.fillRoundRect(SAFE_MARGIN + 4, TEXT_Y, SCREEN_W - 2 * SAFE_MARGIN - 8, TEXT_H, 6, col::WHITE);
  tft.drawRoundRect(SAFE_MARGIN + 4, TEXT_Y, SCREEN_W - 2 * SAFE_MARGIN - 8, TEXT_H, 6, col::SILVER);
  char eggText[96];
  if (!h) snprintf(eggText, sizeof eggText, "Something is moving inside. It will hatch in about %lu min if it stays switched on.",
                   (unsigned long)(pet::eggSecondsLeft() + 59) / 60);
  tft.setTextColor(col::TEXT, col::WHITE);
  tft.setTextDatum(TL_DATUM);
  wrap(h ? st->entry : eggText, SCREEN_W - 2 * SAFE_MARGIN - 28, TEXT_Y + 8, 18, 4,
       [](const char *line, int ly) { ui::text(tft, line, SAFE_MARGIN + 14, ly, ui::BODY); });
}

// ---- page 2: our story – one row strip, label left, value right
void DexScreen::drawTogether() {
  struct Row { const char *label; char value[28]; };
  Row r[12];
  int n = 0;
  auto add = [&](const char *l) -> char * { r[n].label = l; r[n].value[0] = 0; return r[n++].value; };
  const bool h = pet::hatched();
  if (h) {
    snprintf(add("Level"), 28, "Lv %u", pet::level());
    snprintf(add("XP"), 28, "%lu / %lu", (unsigned long)pet::xp(), (unsigned long)pet::xpNeeded());
    if (pet::nextEvolveLevel()) snprintf(add("Evolves"), 28, "at Lv %u", pet::nextEvolveLevel());
    else snprintf(add("Evolves"), 28, pet::stage() < 3 ? "special (soon)" : "final form");
  } else {
    snprintf(add("Hatches in"), 28, "%lu min", (unsigned long)(pet::eggSecondsLeft() + 59) / 60);
  }
  fmtSpan(add("Together"), 28, pet::togetherS());
  fmtDate(add("Met"), 28, pet::metUtc());
  if (h) {
    fmtDate(add("Hatched"), 28, pet::hatchedUtc());
    if (pet::hatchedUtc() && timekeeper::valid()) {
      const uint32_t days = (timekeeper::nowUtc() - pet::hatchedUtc()) / 86400;
      snprintf(add("Age"), 28, "%lu day%s", (unsigned long)days, days == 1 ? "" : "s");
    }
    snprintf(add("Fed"), 28, "%lu times", (unsigned long)pet::feeds());
    snprintf(add("Played"), 28, "%lu times", (unsigned long)pet::plays());
    snprintf(add("Nights"), 28, "%u slept", pet::nights());
    snprintf(add("Shiny"), 28, "%s", pet::shiny() ? "yes *" : "no");
    snprintf(add("Mood"), 28, "%s", pet::moodName(pet::mood()));
  }
  for (int i = 0; i < 12; i++) {
    const int y = ROWS_Y + i * ROW_H;
    if (!row.created()) break;
    row.fillSprite(col::SCREEN_BG);
    if (i < n) {
      if (i % 2 == 0) row.fillRect(0, 0, ROW_W, ROW_H, col::WHITE);   // zebra rows, like a stat sheet
      const uint16_t bg = i % 2 == 0 ? col::WHITE : col::SCREEN_BG;
      row.setTextColor(col::SCREEN_DARK, bg);
      row.setTextDatum(ML_DATUM);
      ui::text(row, r[i].label, 8, ROW_H / 2, ui::BODY);
      row.setTextColor(col::TEXT, bg);
      row.setTextDatum(MR_DATUM);
      ui::text(row, r[i].value, ROW_W - 8, ROW_H / 2, ui::BODY);
    }
    row.pushSprite(SAFE_MARGIN, y);
  }
}

void DexScreen::drawPage() {
  tft.fillRect(0, TOP_BAND_H, SCREEN_W, SCREEN_H - TOP_BAND_H - 18, col::SCREEN_BG);
  if (page == 0) drawEntry(); else drawTogether();
  drawFooter(page);
  shownVersion = pet::version();
  struct tm lt;
  shownMinute = timekeeper::localTime(lt) ? lt.tm_min : -1;
}

void DexScreen::enter() {
  tft.fillScreen(col::SCREEN_BG);
  pic.setColorDepth(16);
  row.setColorDepth(16);
  if (!pic.createSprite(PIC, PIC) || !row.createSprite(ROW_W, ROW_H)) Serial.println("dex: no RAM for the sprites");
  drawHeader();
  drawPage();
}

void DexScreen::leave() { pic.deleteSprite(); row.deleteSprite(); }

void DexScreen::update(uint32_t now) {
  // the page changes slowly: redraw when the pet changed (level, mood, ...) or, on page 2, once a minute
  struct tm lt;
  const int minute = timekeeper::localTime(lt) ? lt.tm_min : -1;
  if (pet::version() != shownVersion && page == 0) {
    shownVersion = pet::version();
    drawHeader();
    drawEntry();
  } else if ((pet::version() != shownVersion || minute != shownMinute) && page == 1) {
    shownVersion = pet::version();
    shownMinute = minute;
    drawHeader();
    drawTogether();
  }
}

bool DexScreen::onEvent(const InputEvent &e) {
  if (e.type != Ev::Press || (e.button != Btn::Feed && e.button != Btn::Play)) return false;
  page = (page + (e.button == Btn::Play ? 1 : PAGES - 1)) % PAGES;
  drawPage();
  return true;
}
