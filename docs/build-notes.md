# Build notes
About two weeks from idea to a closed, working box. These are the main problems, how they were found and what I chose to do about them. The plan and the decisions behind it: PLAN.md.

## Starting point and constraints
- **First electronics project of this size.** I knew code, not soldering or CAD. Every step had to be something I could do safely by hand and check with a multimeter.
- **Fixed budget.** All parts were bought in one order up front, so later problems had to be solved with what I already had, not by buying more.
- **Single-colour printer.** Every part is printed in one colour and glued or screwed together, so the case is split into parts by colour.
- **USB only.** The first plan used Wi-Fi for the time and PC stats. Since the gadget sits on my desk plugged into the PC anyway, I dropped Wi-Fi: the PC sends the time and stats over the serial port, and the RTC keeps time while it's off. That meant no passwords on the device and one less thing to break.

## Measuring before designing
- I measured every part with calipers in 3 batches before any case design: ESP32, screen front and back, knob, switches, LEDs, RTC, LED strip, USB cable. The screen's lit area came out at 48.96 x 65.28 mm, which set the window in the face.
- Things that didn't match the datasheets: the screen hides 2 pixel rows under its frame on the header side, and the KY-040 board sits off-centre on its shaft (8 mm / 11 mm).
- Two rounds of small test prints set the numbers the whole case uses: screw pilot hole 2.75 mm (self-tapping M3, no inserts), knob bore 6.3 mm with a 4.8 mm flat, button cap Ø9.2 mm, and the LED channel corner.
- Before printing the real parts I asked for a part-on-part fit audit against my printer's tolerances (0.2 mm snug, 0.25 mm sliding). It found 4 joints that would have fused or jammed, e.g. the screen face inside the white ring had 0.1 mm. All opened up before printing.

## Case
- **Layout planned too late.** The pin plan was done early, but where each part sits in the housing wasn't. The RTC and the knob board ended up fighting for space: the RTC holder on the printed lid sat where the knob's jumper plugs needed to go, so I taped the RTC lower instead; and my first test fit of the knob wasn't flush, because the model had the off-centre board centred and a screw-post wedge under the roof was in its way. Both were fixed in the model, and the already printed tub was trimmed in 2 places instead of reprinted.
- **Button caps that couldn't go in.** The agent's first design had flanged caps sliding in from the back. When I asked how they would actually get there, it turned out they couldn't (flange wider than the hole, only a 2 mm slot). The tub was already printed, so the fix had to be in the caps: flangeless, pushed in from above. After printing I found they bottomed out before the switches clicked → 1 mm shorter, 0.2 mm thinner.
- **Assembly check.** After the cap mistake I asked for every part to be checked the same way. Now a script moves each part along its real path into the case and checks for collisions; it caught a screw post that would have hit the knob board.
- **Switch retainer plate – 4 versions.** I found the switch legs ended flush with the plate, leaving nothing to solder to. v2 added notches; my slicer showed a 0-thickness wall in it (v2b); v3 was my idea, shallow channels instead of holes; v4 is my design, a flat plate with a built-in 1 mm washer that leaves room underneath for bent legs and wires.
- **LED glow.** When the glow became sides-only, I asked whether that meant cutting the strip into pieces and soldering each one, which I wanted to avoid. It was redesigned so it doesn't: one piece of strip runs up the left side, across the top hidden inside the case and down the right side. The firmware switches the hidden LEDs off; a calibration screen lights them one at a time so I could mark which ones are visible.
- **Printing problems.** The white rims curled up and got hit by the nozzle when printed standing, so they print lying on their back. Combining my "flat back" idea with the prop model showed its parts overlapped each other, and a script splits them cleanly now.
- **Knob.** Added a skirt to hide the thread and nut, then shortened it so the knob could still be pressed.

## Soldering and wiring
- **First attempt failed.** My iron's tip wouldn't take solder, the long heating ripped the GND pads off the LED strip and ruined the first 2 LEDs. I cleaned and tinned the tip, practised on scrap, and redid it with stranded wire from jumper leads, which was easier to work with than the solid-core wire. Second try passed the light test.
- **Carrier board: plan A → plan B.** Plan A was a neat perfboard layout with bare wire rails and solder bridges, too fine for my skill level and too slow. I asked for a plan B and chose longer insulated wires from pin to pin on the back instead. It's less pretty, but every joint is easy to make and easy to check, and nothing can bridge by accident. Every stage had a beep test before moving on.
- **GND as a star.** Rather than chaining ground along the board, I soldered one stranded wire to the ESP32's GND pin and split it into leads to every module: fewer joints on the board and easier to check.
- **Building it my way.** I changed things while building (one pin row went in one column over, my own wire routing on the back). Instead of redoing the work, the plan and the wiring diagram were updated to match what was actually built.
- **First power-up: white screen.** Cause: two display data wires (SDI/SCK) swapped. Found and fixed it.

## The glued switch
- While fitting the roof buttons, superglue got into one switch and jammed it solid. Acetone didn't free it.
- Replacing it meant getting at a switch glued into the roof of an already assembled tub, and probably reprinting the tub, which is the longest and most expensive print of the case.
- I decided to design around it: the dead switch stays in its pocket, unplugged, and the gadget runs on 3 buttons. Its job (play/pause) moves to the knob (double-press, or press on the media screen). The firmware, wiring plan and diagram were updated, and the spare input pin and its resistor removed.
- That change caused one more bug: random button presses, which I reported after the next upload. The unused input pin had been left floating; the firmware now ignores it.
- Lesson kept for later: no glue near switches.

## Firmware
- **Uploads failed on my home PC** but worked on the lab PC with the same board and cable. Several reset sequences and a 100 nF capacitor didn't help; measuring showed the USB driver takes 2 ms per signal change, too slow for the board's reset timing. Fixed with a 100 µF capacitor on EN plus a custom reset sequence.
- **RTC module.** It was built to charge its coin cell (which isn't rechargeable) and pulled the I2C lines up to 5 V. I measured the pull-ups (4.7k to VCC) before removing them and the charging diode, then added 3.3 V pull-ups.
- **Clock face.** The first version was round; I wanted it square like the Minecraft clock. My feedback on v2 (numbers uneven, hands hard to read, numbers confused with stars) led to v2.1: numbers on a circle, iron and diamond swords as hands.
- **Memory.** Keeping it memory-efficient was one of my requirements from the start (no PSRAM, biggest free block ~107 KB). Same for the pet sprites: one firmware build per Pokémon line, so the ESP32 only holds the sprites it needs, and I can flash a different line later. It still went wrong once: a 57 KB buffer starved the boot animation, which needs ~150 KB while it runs. Fixed, and turned into a rule for every screen since: no full-screen buffers, big buffers only while their screen is shown, and RAM is logged after every change.
- **Saving state.** Also my requirement: the pet's state is saved on the device (2 slots + CRC, so a power cut mid-save can't lose it), with a backup to the PC planned in the companion app, so the ESP32 stays focused on the gadget.
- **Partition table.** An early layout moved a system partition and the board boot-looped. Fixed and noted in PLAN.md.

## Display and UI (v0.7.0)
- **The screen is a TN panel.** From my desk angle colours shift: the orange food bar looked green or dark red and the pet's grey shadow nearly vanished. A tilt stand would have made the buttons and roof LEDs harder to use, so I chose to design for it instead. An on-device test card tried gamma presets (no help) and the panel's VCOM setting (big difference); I picked the value by eye. The UI then got deeper colours, thick dark outlines, and nothing that relies on near-white or near-black shades.
- **Less blocky.** The built-in fonts are 1 px strokes and the bars were thin squares. I compared 3 rounded fonts in real-size previews and picked Nunito ExtraBold, a 2 px rim on Pokémon-style HP bars, and card outlines for the Settings rows. Smooth fonts need a known background colour on this display (it can't be read back), which shaped how every text is drawn.
- **Title bar.** The bottom of the "g" in the pet's name sat 1 px above the end of the red bar. I stepped through 9 variants on a test screen on the device and picked a mix of two, then nudged it once more by eye.
- **Reactions not showing.** Feed/play animations never appeared: the start time was taken after the loop's "now", so an unsigned subtraction wrapped around to a huge number. Fixed with a signed, clamped difference.
- **Flicker.** Blanking an area before redrawing it flickered on every value change; everything now paints over in place.

## Agent workflow (v0.7.0)
- Rules in a text file weren't enough, so I paused the firmware until the important ones were enforced by hooks (docs/agent-workflow.md).
- The first guard blocked real work (wildcards inside quotes, running the project's Python environment). Each fix was proposed with pros and cons and only applied after I approved it; the guard's tests grew with every fix (65 → 166 cases). One false block reached 3 hits under my 3-strike rule and got fixed; another is still in the tally.
- A script once wrote into a locked folder because the guard only checks commands. The agent reported it to me first. Now the guard reads scripts the agent wrote before they run, and the step check fails on any change inside a locked zone until I clear it. Every command from that day was replayed through the old and new guard to check for false blocks first.

## Testing
- Each firmware step had its own test before the next one started: serial logs, on-screen test screens (input, lights, screen edges, info) and a separate hardware-test build.
- Before closing the box: a 10-minute full-power test (strip full white, screen on), then 1 minute again inside the closed case. ~450 mA, no resets, not warm.
- Final checklist in the closed box: every screen, button, the knob direction, status LEDs, the glow and the clock. All passed.
