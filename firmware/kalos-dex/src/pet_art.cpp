#include "pet_art.h"
#include "display.h"
#include "pet.h"
#include "pet_line.h"

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static const uint16_t C_FOOD = rgb(255, 150, 40), C_XP = rgb(60, 140, 255), C_EGG = rgb(250, 240, 210), C_SPOT = rgb(110, 190, 120);

// ---- placeholder pet: a blob whose face shows the mood
static void eye(TFT_eSprite &spr, int x, int y, pet::Mood m, int frame, bool anim) {
  using M = pet::Mood;
  const uint16_t k = col::BLACK;
  switch (m) {
    case M::Sleeping: spr.fillRect(x - 6, y, 12, 2, k); break;                                  // closed
    case M::Happy: spr.drawLine(x - 6, y + 2, x, y - 4, k); spr.drawLine(x, y - 4, x + 6, y + 2, k);
                   spr.drawLine(x - 6, y + 3, x, y - 3, k); spr.drawLine(x, y - 3, x + 6, y + 3, k); break;  // ^ ^
    case M::Tired: spr.fillCircle(x, y + 2, 5, k); spr.fillRect(x - 6, y - 5, 13, 6, spr.readPixel(x, y - 8)); break;  // half shut
    default:
      if (anim && frame % 8 == 7) { spr.fillRect(x - 6, y, 12, 2, k); break; }                         // blink
      spr.fillCircle(x, y, 6, k); spr.fillCircle(x + 2, y - 2, 2, col::WHITE);
  }
}

static constexpr uint32_t BLOB_FRAME_MS = 500;   // placeholder + egg: one animation step per 0.5 s

#ifdef PET_HAS_SPRITES
#include "aegislash_line.h"   // only here: the data is static, other files including it would copy 54 KB
static const SprForm *const FORM[3] = {&HONEDGE, &DOUBLADE, &AEGISLASH_SHIELD};
static constexpr uint8_t STILL_SCALE10 = 30;   // Dex card: always 3x (112 px card)
static uint8_t scale10[3] = {30, 30, 35};      // Home, per stage, in tenths (35 = 3.5x: blocks of 3 and 4 px);
                                               // Aegislash 3.5x picked by the user on the gadget (4x felt out of place)
static constexpr int BIG_GROUND = 6;           // bigger than 3x: stand this much lower so the head stays in the area

static const SprAnim &animFor(const SprForm &f, pet::Mood m) {
  return m == pet::Mood::Sleeping && f.sleep ? *f.sleep : *f.idle;
}

static int frameAt(const SprAnim &a, uint32_t ms) {   // which frame is showing at `ms` (loops, PMD timings)
  uint32_t total = 0;
  for (int i = 0; i < a.count; i++) total += a.ms[i];
  uint32_t t = ms % total;
  for (int i = 0; i < a.count; i++) {
    if (t < a.ms[i]) return i;
    t -= a.ms[i];
  }
  return 0;
}

static uint32_t lengthMs(const SprAnim &a) {
  uint32_t total = 0;
  for (int i = 0; i < a.count; i++) total += a.ms[i];
  return total;
}

// ---- reactions: played once on top of the idle loop, then back to idle (PLAN §3d)
static const SprForm *const PLAY_FORM[3] = {nullptr, nullptr, &AEGISLASH_SWORD};   // form while playing (else the same)
static constexpr uint32_t PLAY_FORM_AFTER_MS = 3000;   // Aegislash stays in Sword form this long after the hop
static constexpr uint32_t FEED_SLOW_PCT = 130;         // eating plays at 130 % of its PMD length (user: a tad slower)
static const int8_t SHAKE[] = {-7, 7, -7, 7, -4, 4, 0};   // refusal: side to side (screen px) ...
static constexpr uint32_t SHAKE_STEP_MS = 200;            // ... 12 PMD ticks per step (user: slower, a bit wider)
static petart::Reaction reaction = petart::Reaction::None;
static uint32_t reactedAt = 0, reactUntil = 0;

static uint32_t reactionLength(petart::Reaction r) {
  using R = petart::Reaction;
  const int st = pet::stage() - 1;
  const SprForm *f = FORM[st], *pf = PLAY_FORM[st] ? PLAY_FORM[st] : f;
  switch (r) {
    case R::Feed: return f->feed ? lengthMs(*f->feed) * FEED_SLOW_PCT / 100 : 0;
    case R::Play: return (pf->play ? lengthMs(*pf->play) : 0) + (PLAY_FORM[st] ? PLAY_FORM_AFTER_MS : 0);
    case R::Refuse: return sizeof SHAKE * SHAKE_STEP_MS;
    default: return 0;
  }
}

struct Pose { const SprForm *f; const SprAnim *a; int frame, dx; uint8_t tag; };   // tag: what's playing (for redraws)

static Pose pose(uint32_t ms) {   // what the pet shows at `ms`
  using R = petart::Reaction;
  const int st = pet::stage() - 1;
  const SprForm *f = FORM[st];
  Pose p{f, &animFor(*f, pet::mood()), 0, 0, 0};
  // the loop reads millis() once, then handles buttons (react() stamps a later millis()), then draws with the older
  // value: a start a few ms "in the future" means "just started", not a wrapped-around huge time
  const int32_t dt = (int32_t)(ms - reactedAt);
  const uint32_t t = dt < 0 ? 0 : dt;
  if (reaction == R::Feed && f->feed && t < lengthMs(*f->feed) * FEED_SLOW_PCT / 100) {
    p.a = f->feed; p.frame = frameAt(*p.a, t * 100 / FEED_SLOW_PCT); p.tag = 1;
    return p;
  }
  if (reaction == R::Play) {
    const SprForm *pf = PLAY_FORM[st] ? PLAY_FORM[st] : f;
    const uint32_t hop = pf->play ? lengthMs(*pf->play) : 0;
    if (t < hop) { p.f = pf; p.a = pf->play; p.frame = frameAt(*p.a, t); p.tag = 2; return p; }
    if (PLAY_FORM[st] && t < hop + PLAY_FORM_AFTER_MS) { p.f = pf; p.a = pf->idle; p.frame = frameAt(*p.a, ms); p.tag = 3; return p; }
  }
  if (reaction == R::Refuse && t < sizeof SHAKE * SHAKE_STEP_MS) {
    p.frame = frameAt(*p.a, ms); p.dx = SHAKE[t / SHAKE_STEP_MS]; p.tag = 4 + t / SHAKE_STEP_MS;
    return p;
  }
  reaction = R::None;
  p.frame = frameAt(*p.a, ms);
  return p;
}

#if __has_include("emotes.h")
#include "emotes.h"   // tools/emote_convert.py, git-ignored like the sprites
#define PET_HAS_EMOTES 1
// mood bubbles (PLAN §3d): one at a time, none while asleep or as an egg. A bar is "low" below 30 (energy 20) and
// stays low until 50 (energy 40), so the bubble doesn't blink on and off at the edge.
static bool lowFood = false, lowFun = false, lowEnergy = false, heartArmed = true;
static uint32_t heartUntil = 0;
static constexpr uint32_t HEART_MS = 5000;   // heart: shown this long when all 3 bars reach 85, then not again ...
                                             // ... until one drops below 85 and comes back
static const Emote *bubbleFor(uint32_t ms) {
  using M = pet::Mood;
  const M m = pet::mood();
  if (!pet::hatched() || m == M::Sleeping) return nullptr;
  const float f = pet::food(), u = pet::fun(), e = pet::energy();
  lowFood = lowFood ? f < 50 : f < 30;
  lowFun = lowFun ? u < 50 : u < 30;
  lowEnergy = lowEnergy ? e < 40 : e < 20;
  if (!(f >= 85 && u >= 85 && e >= 85)) heartArmed = true;
  else if (heartArmed) { heartArmed = false; heartUntil = ms + HEART_MS; }
  if (m == M::Angry) return &EMOTE_ANGRY;
  if (lowFood + lowFun + lowEnergy >= 2) return &EMOTE_SAD;   // gloom: two or more bars low
  if (lowFood) return &EMOTE_HUNGRY;
  if (lowEnergy) return &EMOTE_TIRED;
  if (lowFun) return &EMOTE_SAD;
  if ((int32_t)(heartUntil - ms) > 0) return &EMOTE_HAPPY;
  return nullptr;
}

// hungry bobs 2 px, angry flashes fast; false while a flashing bubble is off
static bool bubbleShown(const Emote *e, uint32_t ms, int &dy) {
  dy = e == &EMOTE_HUNGRY && (ms / 400) % 2 ? -2 : 0;
  return e != &EMOTE_ANGRY || ms % 500 < 330;
}

static void drawEmote(TFT_eSprite &spr, const Emote &e, int x0, int y0) {   // 4-bit, own palette, 0 = clear
  for (int n = 0; n < e.w * e.h; n++) {
    const uint8_t c = n & 1 ? e.px[n >> 1] & 15 : e.px[n >> 1] >> 4;
    if (c) spr.drawPixel(x0 + n % e.w, y0 + n / e.w, e.pal[c]);
  }
}
#endif

static int sc(int v, int s10) { return (int)floorf(v * s10 / 10.f); }   // source px -> screen px (also negative)

// one frame, every source pixel a block of s10/10 screen px (3.5x = mixed 3 and 4 px blocks); returns the top y
static int drawFrame(TFT_eSprite &spr, const SprForm &f, const SprAnim &a, int i, int cx, int ground, const uint16_t *pal,
                     int s10) {
  const uint8_t *px = a.px + i * ((a.w * a.h + 1) / 2);   // 4-bit, 2 px per byte, high nibble first
  const int x0 = cx + sc(a.dx, s10), rest = ground + sc(a.dy - f.ground, s10);
  int lift = 0;
  if (a.lift) {   // jumps: drawn lower than the art; big forms jump only as high as the pet area allows (top >= 2)
    uint8_t most = 0;
    for (int k = 0; k < a.count; k++) most = max(most, a.lift[k]);
    const float k = most ? min(SPR_LIFT_SCALE, max(0, rest - 2) / (float)most) : 0;
    lift = lroundf(a.lift[i] * k);
  }
  const int y0 = rest - lift;
  for (int n = 0; n < a.w * a.h; n++) {
    const uint8_t c = n & 1 ? px[n >> 1] & 15 : px[n >> 1] >> 4;
    if (!c) continue;
    const int col = n % a.w, row = n / a.w, x = sc(col, s10), y = sc(row, s10);
    spr.fillRect(x0 + x, y0 + y, sc(col + 1, s10) - x, sc(row + 1, s10) - y, pal[c]);
  }
  return y0;
}
#endif

namespace petart {

bool setScale(uint8_t stage, int s10) {
#ifdef PET_HAS_SPRITES
  if (stage < 1 || stage > 3 || s10 < 20 || s10 > 45) return false;
  scale10[stage - 1] = s10;
  return true;
#else
  return false;
#endif
}

void react(Reaction r, uint32_t now) {
#ifdef PET_HAS_SPRITES
  if (!pet::hatched()) return;
  if (reaction != Reaction::None && (int32_t)(now - reactUntil) < 0) return;   // one still playing: let it finish
  reaction = r;
  reactedAt = now;
  reactUntil = now + reactionLength(r);
#endif
}

void reactTo(pet::Result result, Reaction ifOk, uint32_t now) {
  if (result == pet::Result::Ok) react(ifOk, now);
  else if (result != pet::Result::Egg) react(Reaction::Refuse, now);
}

uint32_t frameKey(uint32_t ms) {
  const uint32_t state = (pet::stage() * 8 + (uint8_t)pet::mood()) * 2 + pet::shiny();
#ifdef PET_HAS_SPRITES
  if (pet::hatched()) {
    const Pose p = pose(ms);
    uint32_t key = (uint32_t)scale10[pet::stage() - 1] << 24 | state << 16 | p.tag << 10 | (p.dx + 8) << 6 | p.frame;
#ifdef PET_HAS_EMOTES
    static const Emote *const ALL[] = {&EMOTE_HUNGRY, &EMOTE_SAD, &EMOTE_TIRED, &EMOTE_ANGRY, &EMOTE_HAPPY};
    const Emote *e = bubbleFor(ms);
    int dy = 0, id = 0;
    for (int k = 0; k < 5; k++) if (ALL[k] == e) id = k + 1;
    const bool on = e && bubbleShown(e, ms, dy);
    key ^= (uint32_t)(id * 4 + on * 2 + (dy != 0)) << 14;   // bubble changes -> redraw
#endif
    return key;
  }
#endif
  return state << 8 | (ms / BLOB_FRAME_MS & 0xFF);
}

void draw(TFT_eSprite &spr, int cx, int ground, uint32_t ms, bool anim) {
  const pet::Mood m = pet::mood();
  const int frame = ms / BLOB_FRAME_MS;
  spr.fillEllipse(cx, ground, 46, 7, col::SILVER);   // shadow

  if (!pet::hatched()) {   // egg: wobbles more as it gets close
    const int wob = !anim ? 0 : pet::eggSecondsLeft() < 300 ? (frame % 2 ? 3 : -3) : (frame % 4 == 0 ? 1 : 0);
    const int ex = cx + wob, ey = ground - 44;
    spr.fillEllipse(ex, ey, 32, 42, C_EGG);
    spr.drawEllipse(ex, ey, 32, 42, col::SCREEN_DARK);
    spr.fillCircle(ex - 12, ey - 14, 7, C_SPOT); spr.fillCircle(ex + 14, ey + 2, 9, C_SPOT); spr.fillCircle(ex - 6, ey + 22, 6, C_SPOT);
    return;
  }

  const int st = pet::stage() - 1;
#ifdef PET_HAS_SPRITES
  {
    const Pose p = anim ? pose(ms) : Pose{FORM[st], FORM[st]->idle, 0, 0, 0};   // still picture: first idle frame
    const SprForm &f = *p.f;
    const SprAnim &a = *p.a;
    const int s10 = anim ? scale10[st] : STILL_SCALE10;
    const int top = drawFrame(spr, f, a, p.frame, cx + p.dx, ground + (s10 > 30 ? BIG_GROUND : 0),
                              pet::shiny() ? f.shiny : f.pal, s10);
    if (!anim) return;
    const int right = cx + sc(a.dx + a.w, s10);
#ifdef PET_HAS_EMOTES
    if (const Emote *e = bubbleFor(ms)) {   // mood bubble at the sprite's top right
      int dy;
      if (bubbleShown(e, ms, dy))
        drawEmote(spr, *e, min(spr.width() - e->w - 2, right - 6), max(2, top - e->h + 8) + dy);
    }
#endif
    if (m == pet::Mood::Sleeping) {   // floating Zz
      spr.setTextColor(col::SCREEN_DARK);
      spr.setTextDatum(MC_DATUM);
      spr.drawString(frame % 2 ? "z Z" : "Z z", right + 14, top - 4 - frame % 2 * 4, 4);
    }
    if (pet::shiny() && frame % 2) {   // sparkle
      const int sx = cx + sc(a.dx, s10) - 10, sy = top + 6;
      spr.drawLine(sx - 5, sy, sx + 5, sy, C_FOOD); spr.drawLine(sx, sy - 5, sx, sy + 5, C_FOOD);
    }
    return;
  }
#endif
  const uint16_t body = pet::shiny() ? PET_LINE.stage[st].shinyColour : PET_LINE.stage[st].colour;
  const int rx = 34 + st * 9, ry = rx * 4 / 5;
  const int bob = (!anim || m == pet::Mood::Sleeping) ? 0 : (frame % 2 ? -3 : 0);
  const int by = ground - ry + 2 + bob;
  spr.fillEllipse(cx, by, rx, ry, body);
  spr.fillEllipse(cx, by + ry / 3, rx * 3 / 5, ry / 2, col::WHITE);   // belly
  if (st >= 1) { spr.fillTriangle(cx - rx + 6, by - ry + 14, cx - rx + 2, by - ry - 10, cx - rx + 22, by - ry + 6, body);   // ears grow with
                 spr.fillTriangle(cx + rx - 6, by - ry + 14, cx + rx - 2, by - ry - 10, cx + rx - 22, by - ry + 6, body); } // each stage
  if (st >= 2) spr.fillTriangle(cx - 8, by - ry + 2, cx + 8, by - ry + 2, cx, by - ry - 16, body);            // horn
  const int eyeY = by - ry / 3;
  eye(spr, cx - rx / 3, eyeY, m, frame, anim);
  eye(spr, cx + rx / 3, eyeY, m, frame, anim);
  const int my = eyeY + 16;
  switch (m) {   // mouth
    case pet::Mood::Hungry: spr.fillEllipse(cx, my + 2, 5, 6, col::RED_DARK); break;
    case pet::Mood::Sad: case pet::Mood::Tired:
      spr.drawLine(cx - 6, my + 4, cx, my, col::BLACK); spr.drawLine(cx, my, cx + 6, my + 4, col::BLACK); break;
    case pet::Mood::Sleeping: break;
    default: spr.drawLine(cx - 7, my, cx, my + 5, col::BLACK); spr.drawLine(cx, my + 5, cx + 7, my, col::BLACK);
  }
  if (!anim) return;
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
}

}  // namespace petart
