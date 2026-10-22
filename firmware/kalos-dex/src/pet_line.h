// The Pokémon line the pet belongs to. PLACEHOLDER until the user picks a real line: then one build env
// per line (pio run -e pet_<name>) selects that line's names, evolve levels and sprites.
#pragma once
#include <Arduino.h>

struct PetLine {
  uint8_t id;                    // saved with the pet: a different line asks "keep level / new egg" (later)
  const char *names[3];          // stage 1, 2, 3
  uint8_t evolveAt[2];           // level that evolves stage 1 -> 2 and 2 -> 3
  uint16_t colour[3], shinyColour[3];   // placeholder blob colours (RGB565) until there are sprites
};

constexpr PetLine PET_LINE = {0, {"Blobbit", "Bloblin", "Blobzard"}, {16, 36},
                              {0x4C9F, 0x2B5A, 0x19B5}, {0xFE60, 0xFC80, 0xE8E4}};
