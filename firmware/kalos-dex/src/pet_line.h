// The Pokémon line the pet belongs to: names, evolve levels, Pokédex data. The sprites live in a generated,
// git-ignored header (tools/sprite_convert.py -> sprites/aegislash_line.h, drawn by pet_art.cpp only). Without it
// (e.g. the public repo) the build falls back to the placeholder "Blobbit" line and its blob drawing.
// Text: plain ASCII only (the screen fonts have no accents: write "Pokemon").
#pragma once
#include <Arduino.h>

struct PetStage {
  const char *name;
  uint16_t dexNo;                // shown as No.001
  const char *category;          // "Blob Pokemon"
  const char *type1, *type2;     // type2 = nullptr when there's only one
  uint16_t heightCm, weightHg;   // weight in 0.1 kg, like the games
  const char *entry;             // Pokédex entry text (wrapped on screen, ~4 lines)
  uint16_t colour, shinyColour;  // placeholder blob colours (RGB565) until there are sprites
  // base stats HP / Atk / Def / SpA / SpD / Spe (current generation, pokemondb), shown on the Dex STATS page.
  // A stage with a second form whose stats differ (Aegislash Shield / Blade) gets one more STATS page: form + altForm
  // name it (user, 2026-10-31). Mega evolutions are NOT forms: they're their own stage-like entry later. All 0 = no data.
  uint8_t stats[6];
  const char *form, *altForm;    // nullptr = one form only
  uint8_t altStats[6];
};

struct PetLine {
  uint8_t id;                    // saved with the pet: a different line asks "keep level / new egg" (later)
  PetStage stage[3];
  uint8_t evolveAt[2];           // level that lets stage 1 -> 2 / 2 -> 3 evolve (0 = a special rule, not a level);
                                 // it never evolves by itself – the user confirms (PLAN §3d)
};

#if __has_include("aegislash_line.h")
#define PET_HAS_SPRITES 1
constexpr PetLine PET_LINE = {1, {
  {"Honedge", 679, "Sword Pokemon", "Steel", "Ghost", 80, 20, "(Entry coming soon.)", 0, 0,
   {45, 80, 100, 35, 37, 28}, nullptr, nullptr, {}},
  {"Doublade", 680, "Sword Pokemon", "Steel", "Ghost", 80, 45, "(Entry coming soon.)", 0, 0,
   {59, 110, 150, 45, 49, 35}, nullptr, nullptr, {}},
  {"Aegislash", 681, "Royal Sword Pokemon", "Steel", "Ghost", 170, 530, "(Entry coming soon.)", 0, 0,
   {60, 50, 140, 50, 140, 60}, "Shield Forme", "Blade Forme", {60, 140, 50, 140, 50, 60}},
}, {35, 50}};   // Doublade -> Aegislash: Lv 50 = PLACEHOLDER for the Dusk Stone (non-level rules: later, PLAN §3d)
#else
constexpr PetLine PET_LINE = {0, {
  {"Blobbit", 901, "Blob Pokemon", "Normal", nullptr, 30, 20,
   "It hatched inside a Pokedex. It wobbles when it is happy and glows softly while it sleeps.", 0x4C9F, 0xFE60,
   {50, 40, 45, 40, 45, 40}, nullptr, nullptr, {}},   // made-up stats for the placeholder line
  {"Bloblin", 902, "Jelly Pokemon", "Normal", "Fairy", 80, 95,
   "Half of its body is jelly. It hums along whenever music plays on the computer nearby.", 0x2B5A, 0xFC80,
   {25, 50, 75, 105, 135, 165}, nullptr, nullptr, {}},   // one stat per colour rank (the public STATS render)
  {"Blobzard", 903, "Ooze Pokemon", "Normal", "Dragon", 160, 600,
   "It guards its trainer's desk day and night. Its roar sounds like an old dial-up modem.", 0x19B5, 0xE8E4,
   {95, 85, 90, 110, 100, 80}, nullptr, nullptr, {}},
}, {16, 36}};
#endif
