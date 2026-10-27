# Changelog (firmware)
Newest first. Version = `FW_VERSION` in `firmware/kalos-dex/platformio.ini`; each one is tagged in git (`v0.6.0`, ...) from v0.6.0 on. Problems and fixes: `docs/build-notes.md`.

## 0.7.0 – 2026-10-27
Includes the 2026-10-22 work in the next section, plus (2026-10-27, built with the new step workflow, each step tested on the gadget):
- Pet sprites: one Pokémon line per build from a converter (`tools/sprite_convert.py`, 4-bit, normal + shiny palette); the sprite data is generated locally and never committed – without it the build falls back to the placeholder blob line. Nothing evolves on its own any more (evolution waits for the owner).
- Reactions on the pet: feed, play (own form while playing) and a refusal shake ("too full", angry).
- Mood bubbles over the pet (one at a time, by priority, with hysteresis so they don't flicker) and an angry mood after 24 h without care that needs 3 calm-down tries. Pet save v2 (64 bytes), v1 saves migrate on boot.
- Display: VCOM offset 0x98 at boot (TN panel viewing angle, picked on an on-device test card).
- UI: rounded smooth font (Nunito ExtraBold 12/15/22 px, SIL OFL, `tools/font_convert.py`), Pokémon-style HP bars with a 2 px black rim, Settings rows as cards, deeper bar colours for the TN panel; title bar 44 px with the text higher; Dex entry page (ENTRY / TOGETHER).
- Test build: Display test card (gamma presets + VCOM), pet cheats over serial (`PET STAGE/SHINY/SCALE/NOCARE`).

## 0.7.0, first part – 2026-10-22
- Music screen: spinning Pokéball record (placeholder for album art), scrolling title, artist, progress, prev/play-pause/next, volume, PC-online dot.
- PC protocol: `HELLO`, `TRACK`, `STATE`, `VOL` in; `KEY PLAYPAUSE/NEXT/PREV/MUTE/VOL` out; green LED = PC connected.
- Knob on every screen: turn = volume, press = play/pause, double-press = mute.
- Boot: slower opening (2.2 s), corner grooves drawn as 3 px black rings. Home: hint line no longer clipped.
- Music: slower record, no flicker (off-screen rows, redraw only what changed), title keeps scrolling on pause; knob volume direction reversed.
- Volume pop-up on every screen; fading mute badge in the top-right corner.
- Night dimming 00:15–07:00 (screen 40 %, lights 10 %), on/off in Settings.
- Watchdog (8 s), start reason shown, safe mode (Back held at start, or 3 crashes in a row): no animation, lights off.
- Settings screen: screen + light brightness, 12/24 h, knob direction (volume and settings together), reset pet (hold Feed + Play 3 s).

## 0.6.0 + pet engine – 2026-10-22
- F6a pet engine: stats, sleep, decay while off (catch-up on boot), feed/play, XP, levels, evolution, mood glow, A/B save. Home screen = the pet (placeholder "Blobbit" line).
- One code base, two builds: `esp32dev` (gadget) and `hwtest` (+ test screens, `POWERTEST`).
- Play/Pause button removed (switch died); status LEDs renamed Green/Yellow/Red.

## 0.5.x – 2026-10-15 … 10-22
- 0.5.4: strip set to 16 LEDs as built, new default visible map.
- 0.5.3: test screens without Play/Pause; real Back button replaces the knob stand-in.
- 0.5.2: G35 no longer read (floating pin caused random presses).
- 0.5.1: Minecraft-style square clock face (rotating sky, sword hands, ticking seconds).
- 0.5.0: time core – DS1307 in UTC, PKT timezone, `TIME`/`PING` serial commands, `tools/set_time.py`.

## 0.4.x – 2026-10-15
- Storage: A/B slots + sequence number + CRC32 in an own NVS partition; settings survive power cuts.

## 0.3.x – 2026-10-15
- LEDs: status LEDs, strip effects, whole-gadget current limiter (450 mA), visible-LED map + calibration.

## 0.2.0 and earlier – 2026-10-14/15
- Skeleton, TFT_eSPI setup, Pokédex boot animation, screen manager, knob/button input events.
