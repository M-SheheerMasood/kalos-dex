#include "pet_art.h"
#include "leds.h"
#include "display.h"
#include "pet.h"
#include "pet_line.h"

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static constexpr uint16_t C_SHADOW = 0x9537;   // rgb(150,164,184): a step darker than silver (TN panel, step 12)
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

static constexpr uint32_t BLOB_FRAME_MS = 500;
// evolving (S5): the show's timings, the show-off move after it, the "settle" at the end of the hold
static constexpr uint32_t EVO_TURN_MS = 1200, EVO_SWAP_FIRST = 650, EVO_SWAP_LAST = 85, EVO_CELEBRATE_MS = 2400;
static constexpr float EVO_SWAP_K = 0.82f;          // each swap 82 % of the one before (user: slow start, v3)
static constexpr uint32_t EVO_SETTLE_MS = 800;      // last part of the hold: back to the base frame, half white   // placeholder + egg: one animation step per 0.5 s

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
static constexpr uint32_t REACT_GLOW_TAIL_MS = 250;   // ... and a little longer, so it's seen to the end
static uint32_t reactedAt = 0, reactUntil = 0;

static uint32_t reactionLength(petart::Reaction r) {
  using R = petart::Reaction;
  const int st = pet::stage() - 1;
  const SprForm *f = FORM[st], *pf = PLAY_FORM[st] ? PLAY_FORM[st] : f;
  switch (r) {
    case R::Feed: return f->feed ? lengthMs(*f->feed) * FEED_SLOW_PCT / 100 : 0;
    case R::Play: return (pf->play ? lengthMs(*pf->play) : 0) + (PLAY_FORM[st] ? PLAY_FORM_AFTER_MS : 0);
    case R::Evolve: return EVO_CELEBRATE_MS;
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
  if (reaction == R::Evolve && t < EVO_CELEBRATE_MS) {   // the new form shows off (its own move, else its hop)
    p.a = f->evolve ? f->evolve : f->play; p.frame = frameAt(*p.a, t); p.tag = 30;
    return p;
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
  if (!pet::hatched()) return nullptr;
  if (m == M::Sleeping) return (ms / 700) % 2 ? &EMOTE_SLEEPY2 : &EMOTE_SLEEPY;   // the only bubble while asleep:
                                                                               // z Z <-> Z z, like the old floating text
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

// hungry bobs 2 px, angry flashes fast; false while a flashing bubble is off (sleepy animates by swapping frames)
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
#endif  // PET_HAS_SPRITES (sparkles below are drawn in the public build too)

static int sc(int v, int s10) { return (int)floorf(v * s10 / 10.f); }   // source px -> screen px (also negative)

// ---- shiny sparkles (user, 2026-10-28): 5 around the pet in 3 sizes, each fades in, holds, fades out (Pokemon GO
// style), staggered so 1-3 show at once. Positions are fractions of the pet's box (outside its edges).
static constexpr uint32_t SPARK_LOOP_MS = 4000, SPARK_STEP_MS = 100;   // redraw ~10x/s while shiny
static const struct { int8_t fx, fy; uint8_t size; uint16_t start, life; } SPARKS[] = {   // fx/fy in % of the box
  {85, 5, 2, 0, 1700}, {-12, 30, 0, 600, 1500}, {110, 62, 1, 1250, 1600}, {-8, 78, 1, 2100, 1600}, {45, -10, 0, 2900, 1500}};
// deeper gold + white core + a dark amber outline: pale gold vanished on the light blue (user, 2026-10-28)
static constexpr uint16_t C_SPARK = 0xFDE5, C_SPARK_CORE = 0xFFFF, C_SPARK_EDGE = 0x8A40;   // (255,190,40) (136,72,0)

static float sparkFade(uint32_t ms, uint16_t start, uint16_t life) {   // 0..1: in 35 %, hold 30 %, out 35 %
  const float u = ((ms + SPARK_LOOP_MS - start) % SPARK_LOOP_MS) / (float)life;
  if (u >= 1) return 0;
  return u < 0.35f ? u / 0.35f : u < 0.65f ? 1 : (1 - u) / 0.35f;
}

static void sparkPx(TFT_eSprite &spr, int x, int y, uint16_t c, float a) {   // blended with what's already there
  if (x < 0 || y < 0 || x >= spr.width() || y >= spr.height()) return;
  spr.drawPixel(x, y, spr.alphaBlend((uint8_t)(a * 255), c, spr.readPixel(x, y)));
}

static void sparkShape(TFT_eSprite &spr, int x, int y, int size, uint16_t c, float a) {   // the gold star at x, y
  const int arm = 4 + 2 * size, half = arm / 2;
  for (int k = -arm; k <= arm; k++) { sparkPx(spr, x + k, y, c, a); if (k) sparkPx(spr, x, y + k, c, a); }
  if (size >= 1)   // thicker towards the middle
    for (int k = -half; k <= half; k++) {
      sparkPx(spr, x + k, y - 1, c, a); sparkPx(spr, x + k, y + 1, c, a);
      sparkPx(spr, x - 1, y + k, c, a); sparkPx(spr, x + 1, y + k, c, a);
    }
  if (size == 2)
    for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) sparkPx(spr, x + dx, y + dy, c, a);
}

static void drawSparkles(TFT_eSprite &spr, int l, int t, int r, int b, uint32_t ms) {
  for (const auto &s : SPARKS) {
    const float a = sparkFade(ms, s.start, s.life);
    if (a <= 0) continue;
    const int x = l + (r - l) * s.fx / 100, y = t + (b - t) * s.fy / 100;
    for (int d = -1; d <= 1; d += 2) {   // outline: the shape shifted 1 px each way in the dark colour, under the gold
      sparkShape(spr, x + d, y, s.size, C_SPARK_EDGE, a);
      sparkShape(spr, x, y + d, s.size, C_SPARK_EDGE, a);
    }
    sparkShape(spr, x, y, s.size, C_SPARK, a);
    const int c = s.size >= 1;
    for (int dy = -c; dy <= c; dy++) for (int dx = -c; dx <= c; dx++) sparkPx(spr, x + dx, y + dy, C_SPARK_CORE, a);
  }
}

#ifdef PET_HAS_SPRITES
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

// ---- the egg (S4, user 2026-10-29, previews .scratch/previews/hatch): not ready = a small nudge now and then;
// ready = hops (crouch, jump stretched, land squashed) until hatched; held = trembles; hatching = crack, rocks twice,
// bigger crack, rocks twice, biggest crack, two quick rocks -> Home takes over (white flash) after HATCH_CRACK_MS.
static constexpr uint32_t EGG_HOP_MS = 1500;
static constexpr int EGG_HOP_PX = 18, EGG_SCALE = 4;
static uint32_t hatchAt = 0;   // millis() the crack sequence started, 0 = not hatching
static bool eggHeldNow = false;
struct EggPose { int8_t dx, lift, deg, cracks; int16_t sx100, sy100; };

static int rockDeg(int32_t u, int n, int period, int deg) {   // n rocks left-right, getting a bit smaller
  if (u < 0 || u >= n * period) return 0;
  return lroundf(deg * sinf(u * 2 * PI / period) * (1 - 0.4f * u / (n * period)));
}

static EggPose eggPose(uint32_t ms, bool anim) {
  EggPose p{0, 0, 0, 0, 100, 100};
  if (!anim) return p;
  if (hatchAt) {
    const int32_t t = ms - hatchAt;
    p.cracks = t >= 2000 ? 3 : t >= 1000 ? 2 : 1;
    p.deg = t < 1000 ? rockDeg(t - 200, 2, 400, 12) : t < 2000 ? rockDeg(t - 1200, 2, 400, 12) : rockDeg(t - 2100, 2, 250, 8);
  } else if (eggHeldNow) {
    p.dx = (ms / 100) % 2 ? 1 : -1;
  } else if (pet::eggReady()) {
    const uint32_t u = ms % EGG_HOP_MS;
    if (u < 150) { p.sx100 = 100 + 12 * u / 150; p.sy100 = 100 - 12 * u / 150; }                 // crouch
    else if (u < 750) { const float k = (u - 150) / 600.f; p.lift = lroundf(EGG_HOP_PX * 4 * k * (1 - k));
                        p.sx100 = 94; p.sy100 = 108; }                                               // up + down
    else if (u < 900) { const int k = 900 - u; p.sx100 = 100 + 10 * k / 150; p.sy100 = 100 - 10 * k / 150; }   // land
  } else {
    p.dx = (ms / 500) % 4 == 0 ? 1 : 0;   // waiting: a small nudge every 2 s
  }
  // in steps of a whole egg pixel's worth so the frame key only changes when the picture does
  p.sx100 = p.sx100 / 2 * 2; p.sy100 = p.sy100 / 2 * 2;
  return p;
}

static void drawEgg(TFT_eSprite &spr, int cx, int ground, const EggPose &p) {
  const int shrink100 = 100 - 18 * p.lift / EGG_HOP_PX;   // the shadow shrinks while it's in the air
  spr.fillEllipse(cx, ground, 46 * shrink100 / 100, 7 * shrink100 / 100, C_SHADOW);
#ifdef PET_HAS_EMOTES
  const Emote &e = EGG_PIXELS;
  uint8_t idx[EGG_PIXELS.w * EGG_PIXELS.h];   // unpacked + cracks (index 1 = the outline colour)
  for (int n = 0; n < e.w * e.h; n++) idx[n] = n & 1 ? e.px[n >> 1] & 15 : e.px[n >> 1] >> 4;
  for (const auto &c : EGG_CRACK) if (c[0] <= p.cracks) idx[c[2] * e.w + c[1]] = 1;
  // every screen pixel looks up its egg pixel: tilt around the bottom centre, then squash / stretch from the bottom
  const float a = p.deg * PI / 180, ca = cosf(a), sa = sinf(a);
  const float kx = EGG_SCALE * p.sx100 / 100.f, ky = EGG_SCALE * p.sy100 / 100.f;
  const int px = cx + p.dx, py = ground + 2 - p.lift;
  const int halfW = e.w * kx / 2 + 12, top = e.h * ky + 10;
  for (int y = py - top; y < py + 12; y++) {
    for (int x = px - halfW; x <= px + halfW; x++) {
      const float rx = x - px + 0.5f, ry = y - py + 0.5f;
      const int ex = (int)floorf((rx * ca + ry * sa) / kx + e.w / 2.f), ey = (int)floorf((ry * ca - rx * sa) / ky + e.h);
      if (ex < 0 || ex >= e.w || ey < 0 || ey >= e.h) continue;
      if (const uint8_t c = idx[ey * e.w + ex]) spr.drawPixel(x, y, e.pal[c]);
    }
  }
#else   // public build: the placeholder egg, moving the same way (no tilt / cracks)
  const int ex = cx + p.dx, ey = ground - 44 - p.lift;
  spr.fillEllipse(ex, ey, 32 * p.sx100 / 100, 42 * p.sy100 / 100, C_EGG);
  spr.drawEllipse(ex, ey, 32 * p.sx100 / 100, 42 * p.sy100 / 100, col::SCREEN_DARK);
  spr.fillCircle(ex - 12, ey - 14, 7, C_SPOT); spr.fillCircle(ex + 14, ey + 2, 9, C_SPOT); spr.fillCircle(ex - 6, ey + 22, 6, C_SPOT);
#endif
}

// ---- evolving (S5, user 2026-10-31, previews .scratch/previews/evolve) – the same for every line: the forms' base frame
// (idle frame 0, or the placeholder blob) as a silhouette blended to white, swaps speeding up, then Home's white flash;
// afterwards the new form's show-off move (sprite data `evolve`, else its hop) under party-popper confetti.
static float evoHold = -1;          // Feed + Play held for an evolution: 0..1 (-1 = not held)
static uint32_t confettiAt = 0;     // millis() the confetti started, 0 = none

static uint32_t swapsMs() {
  uint32_t t = 0;
  for (uint32_t s = EVO_SWAP_FIRST; s >= EVO_SWAP_LAST; s = s * EVO_SWAP_K) t += s;
  return t;
}

// form `st` (0-based) standing still on its base frame, blended `white` (0..256) towards white
static void drawBase(TFT_eSprite &spr, int st, int cx, int ground, int white) {
  const uint8_t a = min(255, white);
#ifdef PET_HAS_SPRITES
  const SprForm &f = *FORM[st];
  const uint16_t *src = pet::shiny() ? f.shiny : f.pal;
  uint16_t pal[16];
  for (int i = 0; i < 16; i++) pal[i] = spr.alphaBlend(a, col::WHITE, src[i]);
  const int s10 = scale10[st];
  drawFrame(spr, f, *f.idle, 0, cx, ground + (s10 > 30 ? BIG_GROUND : 0), pal, s10);
#else
  const uint16_t c = spr.alphaBlend(a, col::WHITE, pet::shiny() ? PET_LINE.stage[st].shinyColour : PET_LINE.stage[st].colour);
  const int rx = 34 + st * 9, ry = rx * 4 / 5, by = ground - ry + 2;
  spr.fillEllipse(cx, by, rx, ry, c);
  if (st >= 1) { spr.fillTriangle(cx - rx + 6, by - ry + 14, cx - rx + 2, by - ry - 10, cx - rx + 22, by - ry + 6, c);
                 spr.fillTriangle(cx + rx - 6, by - ry + 14, cx + rx - 2, by - ry - 10, cx + rx - 22, by - ry + 6, c); }
  if (st >= 2) spr.fillTriangle(cx - 8, by - ry + 2, cx + 8, by - ry + 2, cx, by - ry - 16, c);
#endif
}

static bool bangOn(uint32_t ms) { return (ms / 400) % 3 != 2; }   // the "!" blinks: on 0.8 s, off 0.4 s

static void drawBang(TFT_eSprite &spr, int x, int y) {   // "it can evolve": white bubble, dark rim, red "!" (23 x 27)
  static constexpr uint16_t C_BANG = 0xE124;   // rgb(226,36,36), the angry mark's red
  spr.fillRoundRect(x, y, 23, 27, 7, 0x1083);
  spr.fillRoundRect(x + 3, y + 3, 17, 21, 5, col::WHITE);
  spr.fillRect(x + 9, y + 6, 5, 10, C_BANG);
  spr.fillRect(x + 9, y + 18, 5, 4, C_BANG);
}

// party popper (user, 2026-10-31): bits 2 squares long shoot up out of the top of the pet in a fan, slow down, then
// flutter down swaying, fading at the end. Fixed "random" per bit (a hash), so every redraw agrees.
static void drawConfetti(TFT_eSprite &spr, int cx, int top, uint32_t u) {
  static constexpr uint16_t C[5] = {0xF28A, 0xFE45, 0x55FF, 0x76ED, 0xE37C};   // red, yellow, blue, green, pink
  const float k = u / 50.f, up = min(k, 9.f), sway = k > 9 ? 1 : 0;
  const uint8_t fade = u < 1600 ? 255 : (uint8_t)max(0.f, 255 * (1 - (u - 1600) / 800.f));
  for (int i = 0; i < 28; i++) {
    const uint32_t h = (i + 1) * 2654435761u;
    const float vx = ((h >> 8 & 255) / 255.f * 2 - 1) * 6.5f, vy = -3 - (h >> 16 & 255) / 255.f * 3.5f;
    const float ph = (h >> 24) / 40.f;
    const int x = cx + vx * up + sway * 7 * sinf(ph + k / 3);
    const int y = top + 2 + vy * up + 0.36f * up * up + max(0.f, k - 9) * 1.8f;
    const bool lying = (i & 1) ^ ((int)(ph * 10 + k / 2) & 1);   // tumbling: lying / standing
    const int w = lying ? 6 : 3, hh = lying ? 3 : 6;
    if (y < 0 || y + hh > spr.height() || x < 0 || x + w > spr.width()) continue;
    spr.fillRect(x, y, w, hh, spr.alphaBlend(fade, C[i % 5], col::SCREEN_BG));
  }
}

namespace petart {

void evolveHold(float k) { evoHold = k; }
uint32_t evolveShowMs() { return EVO_TURN_MS + swapsMs(); }
void drawEvolving(TFT_eSprite &spr, int cx, int ground, uint32_t t) {
  const int st = pet::stage() - 1;   // still the old form: pet::evolveNow() comes at the white
  if (st < 0 || st > 1) return;
  if (t < EVO_TURN_MS) { drawBase(spr, st, cx, ground, 128 + 128 * t / EVO_TURN_MS); return; }   // half white -> white
  uint32_t u = t - EVO_TURN_MS, s = EVO_SWAP_FIRST;
  int which = 0;
  while (s >= EVO_SWAP_LAST && u >= s) { u -= s; which ^= 1; s = s * EVO_SWAP_K; }
  drawBase(spr, st + which, cx, ground, 256);
}
void celebrateEvolution(uint32_t now) {
  confettiAt = now ? now : 1;
  react(Reaction::Evolve, now);
}

void eggHeld(bool held) { eggHeldNow = held; }
void startHatch(uint32_t now) { hatchAt = now ? now : 1; eggHeldNow = false; }
void stopHatch() { hatchAt = 0; }
void celebrate(uint32_t now) {   // right after the hatch show: a hop + the heart
#ifdef PET_HAS_EMOTES
  heartArmed = false; heartUntil = now + HEART_MS;
#endif
  react(Reaction::Play, now);
}

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
  if ((int32_t)(now - reactUntil) < 0) return;   // one still playing: let it finish
  reaction = r;
  reactedAt = now;
  // busy until the animation AND its glow are over: Honedge's hop is only 0.4 s, the rainbow 0.8 s (user: Feed / Play
  // could be spammed on Honedge, 2026-10-29)
  const uint32_t glow = r == Reaction::Feed ? PET_FEED_GLOW_MS : r == Reaction::Play ? PET_PLAY_GLOW_MS : 0;
  reactUntil = now + max(reactionLength(r), glow);
  leds::extendFlash(reactUntil + REACT_GLOW_TAIL_MS);   // feed / play glow lasts the whole animation (user, 2026-10-28)
#endif
}

bool busy(uint32_t now) {
#ifdef PET_HAS_SPRITES
  return (int32_t)(now - (reactUntil + REACT_GLOW_TAIL_MS)) < 0;   // by time, glow tail included (pose() ends the picture on its own)
#else
  return false;
#endif
}

void reactTo(pet::Result result, Reaction ifOk, uint32_t now) {
  if (result == pet::Result::Ok) react(ifOk, now);
  else if (result != pet::Result::Egg) react(Reaction::Refuse, now);
}

static uint32_t baseKey(uint32_t ms) {
  const uint32_t state = (pet::stage() * 8 + (uint8_t)pet::mood()) * 2 + pet::shiny();
#ifdef PET_HAS_SPRITES
  if (pet::hatched()) {
    const Pose p = pose(ms);
    uint32_t key = (uint32_t)scale10[pet::stage() - 1] << 24 | state << 16 | p.tag << 10 | (p.dx + 8) << 6 | p.frame;
#ifdef PET_HAS_EMOTES
    static const Emote *const ALL[] = {&EMOTE_HUNGRY, &EMOTE_SAD, &EMOTE_TIRED, &EMOTE_ANGRY, &EMOTE_HAPPY, &EMOTE_SLEEPY,
                                       &EMOTE_SLEEPY2};
    const Emote *e = bubbleFor(ms);
    int dy = 0, id = 0;
    for (int k = 0; k < 7; k++) if (ALL[k] == e) id = k + 1;
    const bool on = e && bubbleShown(e, ms, dy);
    key ^= (uint32_t)(id * 4 + on * 2 + (dy != 0)) << 14;   // bubble changes -> redraw
#endif
    if (pet::shiny()) key ^= (ms / SPARK_STEP_MS & 0xFF) << 24;   // sparkles fade: redraw every step
    return key;
  }
#endif
  if (!pet::hatched()) {
    const EggPose p = eggPose(ms, true);
    return state << 24 ^ (uint32_t)(uint8_t)p.dx << 16 ^ (uint32_t)(uint8_t)p.lift << 8 ^ (uint8_t)p.deg ^
           (uint32_t)p.cracks << 30 ^ (uint32_t)(p.sx100 ^ p.sy100 << 3) << 12;
  }
  return (state << 8 | (ms / BLOB_FRAME_MS & 0xFF)) ^ (pet::shiny() && pet::hatched() ? (ms / SPARK_STEP_MS & 0xFF) << 20 : 0);
}

uint32_t frameKey(uint32_t ms) {
  uint32_t key = baseKey(ms);
  if (pet::hatched()) {   // evolving: the "!" blink, the hold settling in, the confetti
    if (pet::canEvolve()) key ^= (uint32_t)bangOn(ms) << 31;
    if (evoHold >= 0) key ^= (uint32_t)(evoHold * 64) << 1;
    if (confettiAt) key ^= ms - confettiAt < EVO_CELEBRATE_MS ? (ms / 50 & 0xFF) << 22 : 0;
  }
  return key;
}

void draw(TFT_eSprite &spr, int cx, int ground, uint32_t ms, bool anim) {
  const pet::Mood m = pet::mood();
  const int frame = ms / BLOB_FRAME_MS;
  if (!pet::hatched()) { drawEgg(spr, cx, ground, eggPose(ms, anim)); return; }
  spr.fillEllipse(cx, ground, 46, 7, C_SHADOW);   // shadow

  const int st = pet::stage() - 1;
  const uint32_t settleLeft = (1 - evoHold) * PET_HATCH_HOLD_MS;
  if (anim && evoHold >= 0 && settleLeft < EVO_SETTLE_MS) {   // end of the hold: base frame, turning half white
    drawBase(spr, st, cx, ground, 128 - 128 * settleLeft / EVO_SETTLE_MS);
    return;
  }
  if (confettiAt && ms - confettiAt >= EVO_CELEBRATE_MS) confettiAt = 0;
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
#ifndef PET_HAS_EMOTES
    if (m == pet::Mood::Sleeping) {   // floating Zz (with the bubbles: the sleepy bubble instead)
      spr.setTextColor(col::SCREEN_DARK);
      spr.setTextDatum(MC_DATUM);
      spr.drawString(frame % 2 ? "z Z" : "Z z", right + 14, top - 4 - frame % 2 * 4, 4);
    }
#endif
    if (pet::shiny()) drawSparkles(spr, cx + sc(a.dx, s10), top, right, ground, ms);
    if (pet::canEvolve() && bangOn(ms)) drawBang(spr, max(2, cx + sc(a.dx, s10) - 28), max(2, top - 4));   // top-left
    if (confettiAt) drawConfetti(spr, cx, top, ms - confettiAt);
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
  if (pet::shiny()) drawSparkles(spr, cx - rx, by - ry - (st >= 2 ? 16 : st >= 1 ? 10 : 0), cx + rx, by + ry, ms);
  if (pet::canEvolve() && bangOn(ms)) drawBang(spr, max(2, cx - rx - 26), max(2, by - ry - 10));
  if (confettiAt) drawConfetti(spr, cx, by - ry, ms - confettiAt);
}

}  // namespace petart
