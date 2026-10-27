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
  {"Honedge", 679, "Sword Pokemon", "Steel", "Ghost", 80, 20, "(Entry coming soon.)", 0, 0},
  {"Doublade", 680, "Sword Pokemon", "Steel", "Ghost", 80, 45, "(Entry coming soon.)", 0, 0},
  {"Aegislash", 681, "Royal Sword Pokemon", "Steel", "Ghost", 170, 530, "(Entry coming soon.)", 0, 0},
}, {35, 0}};   // Doublade -> Aegislash: custom rule instead of the Dusk Stone, still to be designed
#else
constexpr PetLine PET_LINE = {0, {
  {"Blobbit", 901, "Blob Pokemon", "Normal", nullptr, 30, 20,
   "It hatched inside a Pokedex. It wobbles when it is happy and glows softly while it sleeps.", 0x4C9F, 0xFE60},
  {"Bloblin", 902, "Jelly Pokemon", "Normal", "Fairy", 80, 95,
   "Half of its body is jelly. It hums along whenever music plays on the computer nearby.", 0x2B5A, 0xFC80},
  {"Blobzard", 903, "Ooze Pokemon", "Normal", "Dragon", 160, 600,
   "It guards its trainer's desk day and night. Its roar sounds like an old dial-up modem.", 0x19B5, 0xE8E4},
}, {16, 36}};
#endif
