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
enum class Reaction : uint8_t { None, Feed, Play, Refuse };
void react(Reaction r, uint32_t now);
// the reaction for what pet::feed() / play() answered: Ok -> ifOk, Full / TooTired -> Refuse, Egg -> none
void reactTo(pet::Result result, Reaction ifOk, uint32_t now);
// Changes whenever draw() would draw something different (form, mood, shiny, animation frame): redraw on change.
uint32_t frameKey(uint32_t ms);
// testing (hwtest serial "PET SCALE <30..45>"): size of the current form on Home, in tenths; returns false if refused
bool setScale(uint8_t stage, int s10);
}
