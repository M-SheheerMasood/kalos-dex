# AGENTS.md – rules for the AI coding agent on Kalos Dex
Read this, then PLAN.md, before doing anything.

## The project
A desk gadget shaped like the expanded Kalos Pokédex: ESP32 DevKit V1, 3.2" ILI9341 SPI screen, KY-040 knob, 3 buttons, WS2812B strip around the screen, 3 status LEDs, DS1307 RTC, 3D-printed case. USB only, no Wi-Fi: the PC talks to it over the serial port. Features: virtual pet, clock, PC dashboard and media/volume control through a Python companion app.

## Roles
- **The owner** designs the gadget, makes the decisions, measures parts, does all wiring, soldering, printing and assembly, and tests on the real hardware.
- **The agent** plans, writes the firmware, tools and OpenSCAD, checks its own work (builds, STL checks, serial logs), and keeps the docs current.

## How we work
- **Plan first.** New features and design changes go into PLAN.md and get approved before any code.
- **Small, tested steps.** One feature at a time, each with a test that proves it works on the device. Don't start the next step until this one passes.
- **Safety warnings go before the step they apply to, never after.** The owner is new to electronics: never ask them to probe between closely spaced live pins; always unplug USB before changing wires and re-check power wires before plugging in.
- **The owner does all wiring.** Before an upload that needs new wiring, give the exact wiring list (part pin → ESP32 pin, names as printed on the boards) and the safety checks, then wait for confirmation.
- **Don't assume hardware details** (jumpers, pin names, sizes) – confirm from photos or measurements. Ask for measurements as fill-in-the-blank lists, a few at a time.
- **Record the as-built state.** The owner often changes a layout for something easier; the docs follow what was actually built.
- **Log every meaningful step** (decision, test result, mistake) so a new session can pick up where the last one stopped.
- **Ask before installing anything** on the owner's PC.

## Enforced workflow (agent-harness/, docs/agent-workflow.md)
- **Every task is a step:** plan (2–3 options incl. the smallest, costs, the test) → questions → wait for the owner's answer → build → test. Visual work gets previews first, and side-by-side variants on the device for small choices.
- **Zones:** locked parts of the project are off limits until the owner unlocks them; ask, say why. Never work around a block. A block you think is wrong: note it in a tally; only after 3 of the same kind propose a fix (pros, cons, how often) – the owner decides.
- **The harness is the owner's:** never change hooks or zones unless asked and unlocked. Any problem with it goes to the owner first.
- **Hard gates:** never push, merge, rebase, reset, amend or delete branches/tags; prepare the exact command for the owner to run.
- **Closing a step:** `step.py check` (all tests, code map fresh, both builds) then `step.py done <id> "what"` – it only commits the tree that passed. New files: ask the owner before adding them.
- **Learning outcomes:** when a fixed problem taught something, write it down (what happened, what changed, what to do next time).

## Firmware
- PlatformIO, `board = esp32dev`, `framework = arduino`, `monitor_speed = 115200`. One code base, two builds: `-e esp32dev` (the gadget) and `-e hwtest` (+ hardware test screens). Both must compile before a commit.
- All pins and tunables live in `include/config.h`. Pins: docs/pinout.md.
- **Memory budget (PLAN.md §8):** no PSRAM, biggest free block ~107 KB. Never a 16-bit full-screen buffer; big buffers only while their screen is shown. Log free RAM / biggest block after every screen change.
- **Per-Pokémon modularity:** the firmware will be built for other Pokémon lines. Anything specific to one line (evolve levels, forms, a show-off move per form, art) is data in `pet_line.h` or the converter's table, with a generic fallback in the engine. Every plan that touches the pet says what it costs per new line / stage / evolution and what happens when that data is missing.
- **Nothing hatches or evolves without the owner** (hold Feed + Play). Test on the debug test pet, not the real one.
- **Read a script's code to learn its options; never run it just to see them** (a build script once ran for real when given a flag it didn't know).

## Case (OpenSCAD)
- Parametric, variables at the top with plain explanations. One part per STL, named `colour_part_xQTY.stl`.
- Printer tolerances: 0.2 mm snug, 0.25 mm smooth moving fit, 0.1 mm fuses. No heat-set inserts: self-tapping M3 into printed holes, snap and friction fits. Single-colour prints.
- After any case change, run `tools/assembly_check.py` (every part must have a clear path in).
- The decorative caps come from a third-party prop model that isn't redistributed; never commit it or anything made from it.
