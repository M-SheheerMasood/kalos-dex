# Changelog (firmware)
Newest first. Version = `FW_VERSION` in `firmware/kalos-dex/platformio.ini`; each one is tagged in git (`v0.6.0`, ...) from v0.6.0 on. Problems and fixes: `docs/build-notes.md`.

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
