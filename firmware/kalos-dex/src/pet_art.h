// How the pet looks – the ONE place that draws it (Home and the Dex entry both call this).
// With sprites (sprites/aegislash_line.h present): the line's PMD sprites x3, idle / sleep animation, shiny palette.
// Without: placeholder blob (ears at stage 2, horn at stage 3, shiny colours). The egg is drawn either way.
#pragma once
#include <TFT_eSPI.h>
#include "pet.h"

namespace petart {
// Draw the current pet into `s`, standing on y = ground, centred on x = cx (needs ~112 x 112 around that).
// ms = time for the animation (millis()); anim = false for a still picture (first frame, no Zz, tear, sparkle).
void draw(TFT_eSprite &s, int cx, int ground, uint32_t ms, bool anim);
// One-shot reaction on Home, played once over the idle loop: Feed = eat, Play = hop (Aegislash: Sword form for the
// hop + 3 s), Refuse = a slow side-to-side shake (full / too tired). No-op without sprites.
enum class Reaction : uint8_t { None, Feed, Play, Refuse, Evolve };   // Evolve: the show-off move after evolving
void react(Reaction r, uint32_t now);
// the reaction for what pet::feed() / play() answered: Ok -> ifOk, Full / TooTired -> Refuse, Egg -> none
void reactTo(pet::Result result, Reaction ifOk, uint32_t now);
// a reaction is still playing: Home ignores Feed / Play until it ends (no spamming the bars up)
bool busy(uint32_t now);
// Changes whenever draw() would draw something different (form, mood, shiny, animation frame): redraw on change.
uint32_t frameKey(uint32_t ms);
// The egg (S4): Home tells it when Feed + Play are held (it trembles) and when the hatch starts (cracks + rocking for
// HATCH_CRACK_MS, then Home flashes the screen white and pet::hatchNow() is called). stopHatch() after the hatch.
constexpr uint32_t HATCH_CRACK_MS = 2600;
void eggHeld(bool held);
void startHatch(uint32_t now);
void stopHatch();
void celebrate(uint32_t now);   // the new pet's first moment: hop + heart
// Evolving (S5): Home tells how far Feed + Play are held (0..1, -1 = let go: the pet settles on its base frame and turns
// half white at the end); during the show Home calls drawEvolving (t = ms since the show started, evolveShowMs long,
// old form still current); after pet::evolveNow() + the white flash: celebrateEvolution (show-off move + confetti).
void evolveHold(float k);
uint32_t evolveShowMs();
void drawEvolving(TFT_eSprite &s, int cx, int ground, uint32_t t);
void celebrateEvolution(uint32_t now);
// testing (hwtest serial "PET SCALE <30..45>"): size of the current form on Home, in tenths; returns false if refused
bool setScale(uint8_t stage, int s10);
}
