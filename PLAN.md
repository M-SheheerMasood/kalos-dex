# Kalos Dex – firmware PLAN (v2.1, 2026-10-22 – hardware built, box closed, pet engine in)
The gadget firmware, built in small steps. **Each step ends with a test that proves it works** before the next one starts.
Hardware facts: `docs/pinout.md`, wiring diagram `docs/wiring-diagram/`. Problems and fixes: `docs/build-notes.md`.

## 1. What it does (v1, "all in")
| Feature | Summary |
|---|---|
| Virtual pet | a real Pokémon line (user's images), hunger / happiness / energy, XP + levels, evolves, mood → glow colour, keeps living while off (capped catch-up), saved across power cuts |
| Clock | DS1307 RTC (coin cell) + time from the PC over USB, Pakistan time (UTC+5), 12 h; Minecraft-style square clock face done, more faces later |
| PC dashboard | CPU / GPU temperature + load, RAM, now playing – from a small Python companion on the Windows PC, **over the USB cable** |
| Media control | knob = PC volume, mute, play/pause, next / previous – sent to the companion |
| Lights | one strip, 16 LEDs: 1–6 left channel, 7–10 hidden (always off), 11–16 right channel; mood colours. Status LEDs: green = PC connected (F7), red = pet needs you (F6), yellow = free |

## 2. Lessons already learned (built in)
- **USB only, no Wi-Fi.** PC link, time sync and uploads all go over the USB cable (always in the PC). Opening the COM port with DTR/RTS off does **not** reset the board. Close the companion before uploading.
- **Display:** rotation 2 (header at the top in the case). Keep anything important **≥ 4 px from every edge**, and **≥ 12 px from the top** (`TOP_INSET`: on the desk the case lip hides ~9 rows). **Backlight PWM ~63 %** (160/255). Draw changes **off-screen first, then push** (no flicker). Big static buffers starve the boot animation of RAM – draw into one shared buffer.
- **Knob:** **polled decoder** (0.5 ms timer) – interrupts lost clicks with this KY-040.
- **Buttons:** Feed G27, Play G14, Back G26 (internal pull-ups, pressed = LOW). Play/Pause (G35) was **dropped** – G35 and G15 are free.
- **Strip:** visible-LED map (calibrated in the hwtest build, saved); hidden LEDs always off. **Power limiter**: whole gadget ≤ 450 mA (USB 2 port), measured ~449 mA max with the strip full white in the closed box.
- **SPI without MISO** (GPIO 19 is the yellow LED; `leds::begin()` must run after `display::begin()`).
- **Partitions:** otadata must stay at 0xE000; own NVS partition `kalos` (64 KB) for settings + pet. Never move the layout (saved data would be lost).

## 3. Software structure
PlatformIO project `firmware/kalos-dex/` (`esp32dev`, Arduino). **Two builds from one code base:**
`pio run -e esp32dev` = the gadget · `pio run -e hwtest` = + hardware-test screens (`src/hwtest/`) + `POWERTEST`.

| File | Job |
|---|---|
| `include/config.h` | pins, sizes, tunables (brightness, budgets, strip map default) |
| `display.*` | TFT_eSPI setup (in `platformio.ini`), backlight PWM + fades, colours |
| `boot.*` | Pokédex boot animation (closed dex opens) |
| `input.*` | knob (polled) + 3 buttons → events: turn ±n, press, hold, double (double is per button, off by default) |
| `leds.*` | strip (effects, visible map, calibration mode, limiter) + 3 status LEDs |
| `storage.*` | settings (+ pet later) in NVS: two slots A/B, sequence + CRC32, power-cut safe |
| `timekeeper.*` | DS1307 (UTC inside), TZ, time from the PC, drift |
| `pclink.*` | serial line protocol: `TIME <unix>`, `PING` (+ `POWERTEST ON/OFF` in hwtest) |
| `pet.*`, `pet_line.h` | the pet engine (F6a): stats, sleep, catch-up, XP/levels/evolution, mood glow, save; the line (names, evolve levels – placeholder "Blobbit" line) |
| `screens/*` | screen manager (Back = next, hold = Home, logs RAM on every change), Home = the pet, Clock |
| `hwtest/*` | test screens: Clock test, Input, Lights + strip calibration, Edges, Info |
| to come | pet sprites + converter (F6b), Dex-entry/PC/Media/Settings screens, more watch faces, companion protocol (F7) |

## 3b. Decisions from the user
- **Controls (decided 2026-10-22):** knob on **every** screen: turn = **PC volume** (pop-up bar on other screens – to do), **press = play/pause**, **double-press = mute** (user uses play/pause more). Test screens that use the knob keep it. **Back** = next screen (looping), hold = Home. **Feed / Play** act on the Pet screen; on the Media screen they are previous / next song; on the Clock they switch watch faces (Play hold = fast-day demo for now); Settings: move / change.
- **Clock:** 12 h, UTC+5. Face 1 = square **Minecraft clock** (rotating day/night sky, gold border, iron-sword hour hand, diamond-sword minute hand, ticking red seconds) – accepted "for now". The old round sky dial was archived. 2–3 faces planned.
- **PC:** Ryzen 5 5600X + RX 6600 XT, music = Spotify. Volume/mute = Windows-wide; play/pause/next/prev = Windows media controls. **Ask before installing anything; only verified software from official sources; the user installs it.**
- **Pet = a real Pokémon** (user supplies images, personal use), levels up, evolves. Ideas: feed/play, idle mini-battles for XP, sleep at night, capped catch-up while off (never dies), egg hatched by ~5 min of music playing (fallback ~1 h powered), **shiny 1 in 20**, PC-load reactions, mood glow. Sprites → 16-colour palette (~2 KB per 64x64 frame), scaled 2–3x, converter script. **Which Pokémon: still open.**
- **Glow:** day = warm yellow, night = dark blue + dim, battle = red, fed = green/pink pulse, played = sparkle/rainbow ripple, hungry = yellow-orange pulse, sad = soft blue, level-up = rainbow flash, evolution = white flash.
- **Power:** on/off with the PC. Survive power cuts anytime: pet saved every ~3 min + after events (two copies + CRC); off-time from the RTC; capped catch-up; nothing blocks on the PC; watchdog.
- **Reset pet** in Settings: hold Feed + Play 3 s → new egg (PC keeps a backup).
- **One build env per Pokémon line** (`pio run -e pet_<name>`): only that line's sprites compiled in; after switching lines the gadget asks keep level / new egg.
- **PC backup of the pet save** every hour + after hatch / evolution / level-up.

## 3d. Pet design decisions (user, 2026-10-27) – F6b onwards
- **Per-Pokémon modularity (user, 2026-10-31, top-level rule in the agent instructions):** later firmwares are other lines: line-specific choices are data with a generic fallback, and every plan says what they cost per new line / stage / evolution.
- **Line: Honedge → Doublade → Aegislash** (PMD Sprite Collab fan sprites; the sheets and their artist credits are kept locally with the sprites, never committed). Shiny 1 in 20 at hatch (kept).
- **Public repo: no sprite data** (done for v0.7.0) – the sprite sheets and the generated sprite / bubble data are git-ignored; without them the build uses the placeholder blob line, so the public firmware still compiles.
- **Public placeholder art – later (user, 2026-10-31):** give the placeholder blob line a very simple, plain look per stage (a small circle → an oval → the oval with wings) so anyone can see renders and GIFs of hatching / evolving in the public repo without the private sprites.
- **Nothing evolves by itself.** When it *can* evolve, a symbol shows; the user confirms (planned: hold Feed + Play) and watches the animation. Same for mega evolutions / form changes later.
- **Honedge → Doublade at Lv 35.** Stone / trade evolutions get **custom mechanics** instead (e.g. Doublade → Aegislash "Dusk Stone" → at night + fed, or similar) – designed one by one, noted here when decided.
- **Non-level evolutions – decide later (user, 2026-10-31):** how stones, friendship, held items, trades etc. work on the gadget, per Pokémon. Until then they use a **placeholder level** (Doublade → Aegislash: Lv 50).
- **Forms:** Aegislash = Shield normally, Sword while playing. Open for later: small random battles (→ Sword form), mega evolutions with conditions (e.g. food + fun high → a symbol → confirm).
- **Dex entries:** placeholders now; the user writes the real ones later.
- **Dex stats page – done 2026-10-31-04:** 3rd Dex page STATS: base stats (current generation, pokemondb) as our rounded bars in pokemondb's rank colours (bar = stat / 180; <30 red, <60 orange, <90 yellow, <120 lime, <150 green, 150+ turquoise) + Total. Data: 6 numbers per stage in `pet_line.h`.
- **Forms vs Megas on the STATS page (user, 2026-10-31, decision):** a stage with a second form whose stats differ gets **one extra STATS page** (Aegislash Shield / Blade – a unique case; any other Pokémon with forms found later: the same). **Megas are evolutions, not forms:** no extra page – while Mega, its Mega stats show; otherwise the base stats.
- **Buttons:** Feed + Play are reliable; **Back works as a pressure button (no click) – screens only, never for pet actions**; knob turn/press/double = media everywhere. Pet "big actions" (hatch now, evolve) = **hold Feed + Play** (like Reset pet in Settings).
- **Hatch – done 2026-10-29-01:** pixel egg (own drawing, games' style, private like the sprites), ready = hop loop, hold Feed + Play 3 s = cracks + rocking → whole-screen white 1 s → the pet fades in, full bars, awake 5 min, hop + heart; Settings page 2 "Hatch time". Plan was: time set in Settings: 1, 10, 20 … 60 min; instant hatch on Home = hold Feed + Play. **The egg timer is real time (RTC): it keeps counting while the gadget/PC is off.** When the time is up the egg is **ready, not hatched** – it waits for the user (hold Feed + Play) so the hatch is always seen, never in the background (same rule as evolving).
- **Debug mode (wanted, with S4):** the often-used cheats (hatch, set stats, stage, shiny, fast-forward) in the normal build behind a hidden combo or a Settings "debug" row, so testing doesn't need the hwtest upload every time – if flash allows (it does: ~1 MB free).
- **Debug mode design (user, 2026-10-29, step 2026-10-29-06):** a **test pet** – turning Debug on copies the real pet; every cheat changes only the copy, nothing is saved; turning it off throws the copy away and the real pet resumes from its save (catching up the time). Controls like Settings: Feed/Play = choose a row, knob left/right = the row's two actions. Home shows a "TEST" tag while on.
- **Debug rows for later (user, 2026-10-29, noted):** evolve ready (with S5), Mega (later), sleep now / wake, **set mood** (sets food / fun / energy to that mood's conditions so its bubble + glow show).
- **Cheats on the gadget (normal build, wanted):** fast-forward and friends, not only the hwtest serial commands – design with S4 (few buttons: Feed, Play, Back = screens only, knob = media). Hatch animation: wobble → small cracks → bigger cracks → white flash → the sprite (+ sparkle if shiny).
- **Clock demo:** **done 2026-10-28-05** – spins one full day (~60 s) then stops by itself; no seconds hand during the demo (it looked backwards = wagon-wheel effect; normal ticking is fine).
- **Feed / Play act on release** on Home (**already so** – input sends Press on release; checked 2026-10-28-05) (lets the hand pull back and frees the Feed + Play hold). **Later (user wants to play with it):** tune the delay between the press and the animation – what feels good, not sluggish.
- **Big moments get a "show" (later, like the games):** hatch / evolution / form change / shiny reveal play as a full-screen moment – everything else (name, level, bars) hidden for its length, a short lead-in that also hides the redraw (today the screen flickers when the stage changes), **not interruptible** except holding the Back button or the safety (watchdog / safe mode).
- **Sizes:** Aegislash drawn bigger than 3x on Home (it's twice Honedge's height in the games) – user tries sizes on the gadget (`PET SCALE`, hwtest) and picks.
- **Mood bubbles (S3):** expressive, rounded, filled emote bubbles above the sprite (e.g. the red anger mark with 4 rounded corners) – not thin lines; the user picks the style from previews. **Refusal shake:** slow enough to see, timed like the PMD animations (blends in).
- **When bubbles show (user, 2026-10-27, first ideas – to try out):** base state = **no bubble**. Mood bubbles stay **as long as the mood lasts**; special ones are timed.
  - **Hungry:** berry stays on, gently bobbing or fading in/out, until food is back above a set level.
  - **Tired:** battery until it's back above a set level.
  - **Gloom:** when two needs are low at once (hungry + low fun), until both are good again.
  - **Angry:** no feed or play for ~24 h → angry: refuses feed and play (shake + anger mark), the mark flashes faster than the others. **Calming down:** 3 attempts (feed or play), each after a cooldown (~1 min, spam doesn't count) → on the 3rd it accepts, mood back to base.
  - **Heart (timed):** shows for a while when food and fun are both ≥ 85, then goes away.
- **Lights and mood – done 2026-10-28-06:** every mood its own glow (Happy green breathe, Okay warm white steady, Hungry orange beat, Tired teal slow, Sad purple slow, Angry fast red, Sleeping navy, Egg warm white breathe) on every screen; hwtest "Glow" screen steps through them. Earlier note: the side glow follows the pet's mood while Home is shown; the roof status LEDs show pet trouble (red = hungry etc.) and stay **off when all is fine**.
- **UI polish step (user, 2026-10-27, plan before code):** less rigid look on every screen **except the clock**: Food/Fun/Energy/XP bars like the Pokémon HP bars (rounded, black border 2–3x today's); more readable fonts (today's are 1 px wide strokes) – sizes about the same, rounder/cleaner. Big cross-screen change: check every text still fits (nothing off-screen, TOP_INSET / edges), Home, Dex, Music, Settings, overlays.
  **Done 2026-10-27 (step 08):** Nunito ExtraBold smooth font 12/15/22 px (user's pick of 3, `tools/font_convert.py`, OFL), HP bars with a 2 px black rim (`src/ui.*`), Settings rows as outlined cards; user-checked on every screen.
- **Follow-ups from the UI step (user, 2026-10-27):**
  - **Title bar too tight at the bottom:** the "g" of "Aegislash" sits ~1 px above the red bar's end – add a few red rows (user guessed 3–5); pick from side-by-side variants on the gadget, check every screen's layout (Home, Dex, Music, Settings).
  - **No feed/play spam:** **done 2026-10-28-02** – while a feed / play / refusal animation runs, ignore Feed and Play completely (the bar must not go up either) – the next press counts only after the animation ends.
  - **Mute badge:** **done 2026-10-28-02 (style A dark card, 0.9/0.3 s blink at 92 %)** – redraw it in the new style (it's still the old blocky icon); blink peak 90–95 % opacity, hold ~0.3 s at the peak and ~0.3 s at 0 before fading the other way.
  - **Pet shadow:** **done 2026-10-28-02 (one step darker, user: looks good)** – more contrast (mid-dark, TN panel) – later, in the pet art.
  - Volume pop-up: can't be tested until the PC companion sends volume (knob only sends keys today) – check then in the new style.
  - **Sleeping mood bubble:** **done 2026-10-28-03 (2 frames z Z ↔ Z z, 52x47)** – a "z Z z" emote in the same white speech bubble as the other moods (sleeping currently shows no bubble – the sleep animation alone isn't always clear). Fits the bubble rules: shown while the mood is Sleeping.
  - **Dex "Nights … slept" is wrong (user, 2026-10-28):** **fixed 2026-10-28-02 (once per night)** – it counts every time the pet falls asleep, not nights – a wake-up at night (Feed / Play, 10 min) or a restart at night counts again. Fix later: count a night once per calendar night (e.g. remember the date of the last counted night in the save), or rename it to what it really counts.
  - **Dex "Met" and "Hatched" (user, 2026-10-28):** **done 2026-10-28-02 (one row "Met/Hatched", date only)** – two saved times (egg arrived / egg hatched), but with a 1–60 min egg they're the same date, so showing both is noise. Decide later: one row (e.g. "Hatched"), or "Met" as the date + hatch only as a time.
  - Title bar: **done in step 09** – band 42 → 44 px + title text 4 px up (`TITLE_Y` 25), mute badge 6 rows up; picked on the gadget. User may still move the text 2 more rows up later (undecided).
- **Demo mode build (user, 2026-10-27) – LAST, after everything else:** a third firmware env (like `hwtest`, separate from the main build) for showing the gadget away from this PC (power bank / someone else's PC, so no companion – a demo screen instead of Music). Everything to show off on its own: all the cheats on the buttons (hatch, evolve, shiny, stages), all sprite animations, food/fun decay and energy regain sped up so the moods, bubbles, "too full" refusals and angry show quickly, the clock fast-spin demo – a full showcase mode.
- **Viewing angle (user, 2026-10-27):** the panel is TN – from below/above the colours wash out or darken (orange food bar looks green / dark red, the grey pet shadow nearly vanishes); straight on it looks best. User's choice: **no tilt stand** (buttons + roof LEDs harder to use) → **B: design for TN** in the UI polish step (darker shadow, thicker dark outlines, bar colours that survive the angle, no meaning in subtle shades) + **C: gamma test** (hwtest screen with a few gamma presets for the ILI9341, user picks by eye at the desk angle). Accepting the rest – "I still like it a lot".
- **Shiny sparkles – done 2026-10-28-06:** 5 around the pet, 3 sizes, gold + white core + dark outline, each fades in/out over ~1.6 s, staggered. Earlier note: several sparkles around the model instead of the single one (user, 2026-10-27).
- **Evolve-ready badge = blinking "!"** next to the pet. Later maybe different symbols for mega / other form changes.
- **Evolving – done 2026-10-31-03:** blinking "!" at the pet's top-left + "It can evolve! hold Feed+Play"; hold 3 s (the pet keeps moving, settles + turns half white at the end); show: dark screen, "What? X is evolving!", white silhouettes of the base frames swapping faster and faster (650 ms x0.82 down to 85 ms), the whole screen fades to white (0.6 s), 1 s white, fades back (0.6 + 0.8 s) with the new form: show-off move (per form in the converter: Doublade SpAttack, Aegislash Rotate; none = its hop) under party-popper confetti (bits 2 squares long). No cancel. Generic for any line (data: evolveAt + optional move per form). Doublade -> Aegislash = placeholder Lv 50.
- **Low priority (user, 2026-10-31) – FIXED 2026-10-31-04 (on/off rows flipped once per knob event; a quick twist = 2-3 events):** once, after the first evolution in Debug, "evolve Lv" seemed not to work until the stage was changed by hand; not reproducible (serial + a retest worked). Look again only if it comes back.

## 3c. Pet rules (F6a, built 2026-10-22 – numbers in `config.h`, first guesses, tune after living with it)
- **Balance (user, 2026-10-31, `.scratch/previews/balance/balance_sim.py`):** a normal owner (5 check-ins/day) reaches Lv 35 in ~6 days and Lv 50–55 in ~10–11 days; the pet wakes up hungry (food ~20) and a bit less happy.
- **Stats 0–100:** food (empty in **11 h** awake, **0.9x** speed asleep), fun (empty in 20 h awake, 2x when hungry, **¼ speed asleep**), energy (**since 2026-10-28, user:** awake and not hungry it comes back, 0 → 100 in 15 h; only when hungry (food < 25) it drains, empty in 18 h; refills in 8 h asleep; **a real morning wake-up = 100**, a 10-min wake-up at night keeps what it has). Never dies.
- **Sleep 23:00–07:00** (only when the time is known). Feed / Play at night wakes it for 10 min. While asleep the only mood bubble is the sleepy "z Z Z" (style A, user's pick 2026-10-28).
- **Feed:** +30 food, +3 fun, **+10 energy** (2026-10-28); refuses at food ≥ 90 ("I'm full!"); **+10 XP** (every accepted feed, 2026-10-31). **Play:** +15 fun, −8 energy, −3 food; refuses at energy < 15 ("Too tired..."); **+10 XP** (every accepted play). **+1 XP per 10 min** awake with every stat ≥ 50.
- **Levels:** level n → n+1 needs **30 + n** XP (2026-10-31; was 10 + 5n = Lv 35 in > 2 weeks), max Lv 100. Evolves at the line's levels (Honedge → Doublade Lv 35; the final form's rule is S5, aimed at ~Lv 50–55).
- **Egg:** ready after the Settings "Hatch time" (1, 10 … 60 min, default 60), counted in real time (also while switched off); then it waits and hops until Feed + Play are held 3 s on Home (§3d Hatch). A new pet starts at 100 / 100 / 100 and stays awake 5 min. **Shiny 1 in 20** at hatch. (Music hatching: idea for F7.)
- **Switched off:** catch-up from the RTC, minute by minute (sleep hours count), **max 48 h**, never pushes a stat below 10.
- **Mood** (first match): Egg · Sleeping · Angry (no care 24 h) · Hungry (food < 25) · Tired (energy < 15) · Sad (fun < 30) · Happy (all ≥ 60) · Okay. **Glow** (2026-10-28-06): egg warm-white breathe, sleeping navy, angry fast red, hungry orange beat, tired teal slow, sad purple slow, happy green breathe, okay warm white steady; feed = green flash, play / level-up = rainbow flash (Feed / Play wait for it), hatch / evolve = white. **Red LED on = hungry or sad.**
- **Saved** (64-byte record v2, A/B + CRC; v1 saves are migrated): 3 s after an action + every 3 min; never while Debug's test pet is on. Test commands (hwtest): `PET`, `PET SKIP <min>`, `PET XP <n>`, `PET HATCH`, `PET RESET`, `PET FEED`, `PET PLAY`, `PET SET f|u|e <v>`, `PET STAGE/SHINY/SCALE/NOCARE` (`tools/serial_send.py`); in every build: the Debug screen (§3d).

## 4. Screens (2026-10-22)
Back = next screen (looping), hold Back = Home. Offline is normal: companion not running → PC/Media say "PC offline", clock from the RTC, pet works.

| Screen | Status | Contents / buttons |
|---|---|---|
| Boot animation | **kept, locked** | closed Pokédex opens (~3.3 s) |
| ~~Home placeholder~~ (uptime counter "time since boot") | **scrapped** 2026-10-22 | replaced by the pet Home |
| **Home = pet** | engine **done**, sprites wait for the Pokémon | name + level, small clock + mood, the pet (blob placeholder), what it said, Food / Fun / Energy / XP bars. Feed / Play buttons |
| **Clock** (Minecraft face) | **kept** (face 1) | more faces later (Feed / Play switch faces). The "fast day" demo (hold Play) moves to the hwtest build at F8 |
| **Dex entry** (pet details) | **built** 2026-10-23 (2nd screen) | page ENTRY: picture, No. + name, category, types, HT/WT, entry text (all from `pet_line.h`); page TOGETHER: level, XP, evolves at, time together, met, hatched, age, fed, played, nights, shiny, mood. Feed/Play = page |
| **Music** (was "Media") | **screen + protocol done** 2026-10-22 (tested with `tools/fake_companion.py`); real companion = F7 | spinning Pokéball record (placeholder for album art), scrolling title, artist, progress, prev / play-pause / next icons, volume bar, PC dot. Feed / Play = previous / next song |
| **PC** | F7 | CPU / GPU temp + load, RAM |
| **Settings** | **done** 2026-10-23 | screen + light brightness, night dim (00:15–07:00: screen 40 %, lights 10 %), 12/24 h, knob direction, reset pet (hold Feed + Play 3 s); Feed/Play = row, knob = change |
| Test screens (Clock test, Input, Lights + calibration, Edges, Info) | **hwtest build only** | unchanged |

Order: Home → Dex → Clock → Music → (PC) → Settings – built: Home, Dex, Clock, Music, Settings.

## 4b. Locked vs open (2026-10-22)
**Locked:** hardware + pins · USB only · two builds from one code base · controls (§3b) · 12 h Pakistan time · boot animation · Minecraft clock face 1 · storage A/B + CRC (never move the partitions) · power limiter 450 mA, backlight ≤ 63 % · pet rules above (the numbers can still be tuned) · mood glow colours · red LED = pet needs you, green = PC connected.
**Open:** which Pokémon line + images (blocks F6b sprites) · egg hatching by music (F7) · mini-battles for XP (idea) · which extra watch faces · companion installs (Python packages, LibreHardwareMonitor – ask first) · Settings contents, night dimming, screensaver (F8) · maybe small album art on Media (fits the budget, see §8).

## 5. PC companion (Python, Windows) – F7
**Protocol (built on the gadget side, `media.h` / `pclink.h`):** one text line each, 115200 baud, COM port opened with DTR/RTS off.
PC → gadget: `HELLO <ver>` → `OK HELLO <fw>` · `TRACK <title>\t<artist>` (on change, ASCII only – the companion transliterates; empty = nothing playing) · `STATE playing|paused|stopped <pos s> <dur s>` (every second = also the heartbeat) · `VOL <0-100> <muted 0|1>` · `TIME <unix>`. No message for 4 s → "PC offline", green LED off.
Gadget → PC: `KEY PLAYPAUSE` · `KEY NEXT` · `KEY PREV` · `KEY MUTE` · `KEY VOL <±clicks>` (bundled, at most one per 80 ms; companion: 2 % per click). Everything else on the port is log text – ignore lines that don't start with `KEY ` / `OK ` / `ERR `.
- Runs in the background (tray icon later), opens the gadget's COM port (DTR/RTS off), sends `TIME` at start, stats once a second, receives button/knob commands.
- **Stats:** CPU/GPU temps via **LibreHardwareMonitor** (its web server gives JSON); load / RAM via `psutil`.
- **Now playing + play/pause/next/prev:** Windows media controls via `winsdk`. **Volume / mute:** `pycaw`.
- Nothing installed without asking (see 3b).

## 6. Steps (each with its proof)
| Step | What | Status |
|---|---|---|
| F0 | skeleton, config, boot screen, serial log | **done** |
| F1 | TFT_eSPI, margins, backlight, screen manager, off-screen drawing | **done** |
| F2 | input: knob + buttons → events | **done** |
| F3 | LEDs: status, strip effects, limiter, calibration | **done** (calibrated in the case: map `1111110000111111`) |
| F4 | storage (A/B + CRC) | **done** |
| F5 | time: RTC fix, PC time over USB, clock face | **done** (RTC drift ~4–5 s/day → the companion re-syncs) |
| F6a | pet engine: stats, sleep, decay (incl. off-time), feed / play, XP, levels, evolution, mood → LEDs, save; Home = pet with a placeholder blob | **done** 2026-10-22 (serial test: all rules ok, save + reset ok) |
| F6b | sprites: converter script, 4-bit pet area, one build env per line, Dex-entry screen | next part needs the Pokémon line + images |
| F7 | PC companion + `pclink` stats/commands + PC & Media screens + volume / media keys | |
| F8 | polish: Settings screen, night dimming, screensaver, more watch faces, error screens | |
| F9 | long run (24 h): no resets, no heat, pet + clock right | box closed + 1-min full-power test passed 2026-10-22 |

## 7. Nooks & crannies (handled on purpose)
- **Flash wear:** pet saved only on change, at most every few minutes (+ events).
- **Time jumps / dead coin cell:** detected (clock-halt bit or date < 2025-10-09) → "time not set" until the PC sends it; pet decay capped and sanity-checked.
- **PC link drops / companion closed:** "PC offline", nothing blocks; green LED off.
- **Power:** limiter keeps the gadget ≤ 450 mA; backlight capped.
- **Boot with Back held, or 3 crashes in a row** → safe mode (no animation, LEDs off) – **done** 2026-10-23 (`system.*`).
- **Watchdog** 8 s on the loop – **done**, tested with `CRASH` / `PANIC` (hwtest).

## 8. Memory budget (measured on the device 2026-10-22) – keep it this way
| What | Size |
|---|---|
| Flash for the program (app slot) | 1,792 KB · **used 403 KB** (pet included) · **~1.39 MB free** |
| Other flash | `kalos` saves 64 KB (pet 48 B + settings: decades at one save / 3 min) · `spiffs` 320 KB unused (spare for data packs) · `app1` 1.75 MB unused but must stay (layout never moves) |
| RAM at start | **323 KB free**, **biggest single block 107 KB** (the ESP32's RAM is split in pieces – this is the real limit for one buffer) |
| RAM in use | static 33 KB · boot animation 2 x 38 + 77 KB (8-bit, freed after) · Home/pet area 68 KB (16-bit 232 x 150) → **251 KB free on Home** · Clock 19 KB strip + 9.6 KB static |
| Costs to plan with | full-screen buffer: 16-bit **150 KB = impossible**, 8-bit 77 KB, 4-bit 38 KB · pet sprite frame 64 x 64 at 16 colours = **2 KB flash** (a whole line, 3 stages x ~10 frames ≈ 60 KB) · shiny = second palette (32 B) · smooth font 10–40 KB flash each · album art 64 x 64 = 8 KB RAM (~0.7 s over USB at 115200) · PC stats < 1 KB |

**Rules:**
1. Only the current screen owns big buffers: make them in `enter()`, free them in `leave()`. One screen ≤ ~100 KB in one block, ≤ ~150 KB in total.
2. Never a 16-bit full-screen buffer: draw in strips (like the clock), or use 8-bit / 4-bit sprites.
3. Images live in flash (`const` arrays, 4-bit palette) and are drawn from there, never copied whole into RAM. With sprites the pet area becomes a **4-bit sprite (17 KB instead of 68 KB)**.
4. No Arduino `String` in the loop, no big local arrays (loop stack is 8 KB) – fixed `char` buffers / `static`.
5. Every screen change logs `RAM free / biggest block`: after a tour of all screens, back on Home, the biggest block must still be ~107 KB (no leaks / fragmentation). Check this after every new screen.
6. Keep the program under ~1.4 MB (≈ 75 % of the slot): ~1 MB left for sprites, fonts and faces.
