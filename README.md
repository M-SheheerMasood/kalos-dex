# Kalos Dex
A desk gadget I designed and built, shaped like the expanded Kalos Pokédex from Pokémon X/Y. An ESP32 with a 3.2" screen, a knob, 3 buttons and an LED glow around the screen, always plugged into my PC over USB. It has a virtual pet and a Minecraft-style clock; a PC dashboard and media/volume control is next.

<img width="300" alt="Kalos Dex on my desk showing the Minecraft clock, with the side glow on" src="media/finished-clock-face.jpg" /> <img width="240" alt="Boot animation: the Pokédex opens and loads" src="media/boot-animation.gif" />

The finished box on my desk with the Minecraft clock, and the boot animation.

**Status:** hardware built and the box is closed, firmware v0.6.0 on the device. Pet sprites and the PC companion app are in progress.

**Why I made it:**
- I used to watch Pokémon and Kalos was my favourite region, mostly because of the animation. When I was thinking about what to build, a Pokédex was the obvious shape. Of all of them, the Kalos one is the cleanest to design and print around: it's basically a screen and its two sides.
- I like Minecraft, and I like watches and clocks, but a plain clock gets boring. I took inspiration from art-piece watches with moving parts and proper articulation. The Minecraft clock already moves (the sky turns with the time of day), so the two fit together: a clock that's functional, interactive and has something moving in it.

**What I did:**
- The idea and the design: what it does, which parts to use, what each button does, which screens to have, and where everything sits inside an 84 x 189 x 55 mm case.
- The firmware requirements: keep it memory-efficient from the start, save the pet's state so nothing is lost, and build one firmware per Pokémon line so the ESP32 only holds the sprites it needs (planned for the pet sprites step).
- Measured every part with calipers and ran test prints to set the printer tolerances (0.2 mm snug, 0.25 mm sliding).
- All the soldering, wiring, 3D printing and assembly, on a fixed parts budget and a single-colour printer.
- Tested every step on the real hardware and caught the problems along the way.

<img width="300" alt="Inside the case: ESP32 on its carrier, wires to every module, screen face folded out" src="media/inside-wiring.jpg" />

Inside, with the screen face folded out: the ESP32 on its perfboard carrier at the bottom, hand-wired to every module.

**Build notes** (docs/build-notes.md): the problems and what I chose to do about them. A few of them:
- One roof switch got jammed solid by superglue. Reprinting the tub would have been the longest, most expensive fix, so I designed around it: 3 buttons, and play/pause moved to the knob.
- The neat perfboard layout was too fine for my first real soldering job, so I switched to longer insulated wires pin to pin: less neat, but quicker and easy to check.
- Uploads failed on my home PC but not at the lab; tracked down to USB driver timing and fixed with a capacitor and a custom reset sequence.

**Hardest and most fun:** the wiring and soldering were the hardest part, long and slow. The most fun was the design side: deciding what each button does, which screens to add, and how to manage memory on a chip with no PSRAM.

**Next time:**
- More test prints before the real ones.
- Plan the physical layout as early as the wiring. This time the pin plan was done early but where each part sits in the housing wasn't, and the RTC and the knob board both ended up fighting for space: the RTC holder on the lid sat where the knob's plugs needed to go, and the knob board needed two trims in the printed tub.

**How I worked (agentic workflow):**
The code is vibe coded with an AI coding assistant, which also turned my layout and measurements into parametric OpenSCAD. The workflow:
- AGENTS.md: the rules the agent follows (plan first, one small tested step at a time, safety warnings before each step, I do all the wiring).
- PLAN.md: design and decisions written down and approved before any code. Firmware steps F0–F9, each with a test that proves it works.
- Every step was built, uploaded and checked on the device (serial logs, test screens, a power test) before the next one. Things that didn't work got redone; docs/build-notes.md has the main ones.

**Hardware:**
- ESP32 DevKit V1, 3.2" ILI9341 SPI screen, KY-040 knob, 3 buttons (Feed, Play, Back), 16 WS2812B LEDs, 3 status LEDs, DS1307 RTC (modified so it doesn't charge its coin cell).
- Pins: docs/pinout.md, wiring diagram: docs/wiring-diagram/wiring-diagram.html

**Firmware (C++, PlatformIO, Arduino, TFT_eSPI, Adafruit NeoPixel):**
- Virtual pet: stats drop in real time, even while unplugged (it catches up on boot), XP, levels, evolutions, mood shown on the LEDs.
- Power-cut-safe saves: every record goes to 2 slots with a sequence number + CRC32. A backup to the PC comes with the companion app.
- Memory budget: no PSRAM and the biggest free RAM block is ~107 KB, so no full-screen buffer; screens draw in strips and only hold big buffers while they're shown.
- LED current limiter keeps the whole gadget under ~450 mA (USB 2 gives 500 mA).
- PC link: line commands over serial (TIME, PING, PET); tools/set_time.py syncs the clock from the PC.

**Not included:** the decorative end caps are cut from a third-party Pokédex prop model that isn't mine to share. Everything else in scad/ is my design.

**To run** (in firmware/kalos-dex): "pio run -e esp32dev -t upload --upload-port COM3" (or -e hwtest for the hardware test screens)

Fan project, not affiliated with Nintendo, Game Freak or The Pokémon Company. MIT license.
