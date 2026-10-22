# Kalos Dex – pinout & wiring plan (v1.1, 2026-10-13 – checked against photos of the real boards)
Board: ESP32 DevKit V1, 38-pin, ESP-WROOM-32 (no PSRAM), PlatformIO `esp32dev`. AMS1117 3.3 V regulator on board.

**ESP32 pin rows as printed on the user's board (USB connector on the left):**
- Top row, left → right: V5, CMD, SD3, SD2, G13, GND, G12, G14, G27, G26, G25, G33, G32, G35, G34, SN, SP, EN, 3V3
- Bottom row, left → right: CLK, SDO, SD1, G15, G2, G0, G4, G16, G17, G5, G18, G19, GND, G21, RXD, TXD, G22, G23, GND
- The 5 V pin is printed **V5** (other boards call it VIN). GND: one on the top row (next to G13), two on the bottom row (after G19, and at the far right end).
- Handy grouping: display + RTC + status LEDs are all on the **bottom row**; strip, encoder and buttons are all on the **top row**.
Rule of thumb: every module that talks to the ESP32 runs its signals at **3.3 V**. Only the LED strip, the display's power input (it has its own 3.3 V regulator) and the RTC's power input use 5 V.

## Pins the plan avoids, and why
| GPIO | Why it's avoided |
|---|---|
| 6–11 | wired to the ESP32's own flash chip – touching them crashes the board |
| 0 | boot button; pulled low = "download mode" |
| 12 | must be LOW at boot, or the board won't start (a pulled-up button here = no boot) |
| 1, 3 | USB serial (uploading code and the Serial Monitor) |
| 34, 35, 36, 39 | input-only, no internal pull-up (none used – G35 freed 2026-10-19) |
Strapping pins that are used anyway, safely: **2** (status LED to GND keeps it low, which is what it wants), **5** (only affects SDIO timing, irrelevant here).

## Pin assignments
### Display – 3.2" ILI9341, SPI (VSPI bus)
| Display pin | ESP32 GPIO | Notes |
|---|---|---|
| VCC | **V5 (5 V)** | photo shows a 3.3 V regulator (U1, marked 662K) and jumper pads J1 left **open**, so the board is built for 5 V in. (3V3 would also work, just less cleanly.) |
| GND | GND | |
| CS | **17** | chip select |
| RESET | **EN** (ESP32 top row, next to 3V3) | display resets together with the ESP32; frees a GPIO and avoids needing two 3V3 wires (the board has only one 3V3 pin). Library: `rst = -1` |
| DC / RS | **16** | data/command |
| SDI (MOSI) | **23** | VSPI MOSI |
| SCK | **18** | VSPI clock |
| LED (backlight) | **GPIO 5** (set HIGH in code = on; PWM = dimming) | this header pin switches the backlight. Photo shows a transistor (Q1, marked J3Y) + resistor R2 behind it, so a GPIO can drive it safely |
| SDO (MISO) | not connected | only needed to read from the display/SD |
| T_CLK, T_CS, T_DIN, T_OUT, T_IRQ | not connected | touch is unused (screen sits behind the case face) |
| SD_CS, SD_MOSI, SD_MISO, SD_SCK (4 empty holes, bottom edge) | not connected | sprites live in the ESP32's flash (LittleFS) instead |

### LED strip – WS2812B
| Strip pad | Connects to | Notes |
|---|---|---|
| +5V | **V5 through a 1N4007** (stripe towards the strip) | drops ~0.7 V → strip runs at ~4.3 V, so a 3.3 V data signal counts as "high" |
| DIN | **GPIO 13 through 330 Ω** | resistor sits close to the strip |
| GND | GND | |
| (across +5V and GND at the strip) | **100 µF**, stripe (−) to GND | smooths the strip's power |

### Rotary encoder – KY-040
| Encoder pin | ESP32 | Notes |
|---|---|---|
| + | **3V3** | ⚠ NOT 5 V – its pull-ups would then put 5 V on ESP32 pins |
| GND | GND | |
| CLK | **32** | module has its own 10 kΩ pull-ups on CLK/DT |
| DT | **33** | |
| SW | **25** | internal pull-up in firmware |

### Roof buttons – 4 × 6x6 tall tact switches (each: one leg to the GPIO, the other leg to GND)
| Button | GPIO | Notes |
|---|---|---|
| Back | **26** | internal pull-up |
| Feed | **27** | internal pull-up |
| Play | **14** | internal pull-up |
| ~~Play/Pause~~ | ~~35~~ | **DROPPED 2026-10-19** (switch glued solid). No wire, no 10 kΩ. Play/pause moves to the knob (later). |
Pressed = reads LOW.

### Status LEDs – 3 × 5 mm (long leg +)
| LED | GPIO | Notes |
|---|---|---|
| Green (Wi-Fi) | **4** | GPIO → 220 Ω → LED long leg; LED short leg → GND |
| Yellow (PC link) | **19** | same |
| Red (pet needs you) | **2** | same; also blinks the board's blue LED – harmless |

### RTC – DS1307 Tiny RTC
| RTC pin | ESP32 | Notes |
|---|---|---|
| VCC | **V5 (5 V)** | DS1307 needs 5 V. Module headers: left DS/SCL/SDA/VCC/GND, right SQ/DS/SCL/SDA/VCC/GND/BAT – either side works |
| GND | GND | |
| SDA | **21** | ⚠ the module's own pull-ups go to 5 V (known issue, see Risks) |
| SCL | **22** | |

### Spare GPIOs
15, 36, 39 (36/39 input-only). 5 is reserved for backlight dimming.

## Power plan
- One USB cable → ESP32 → **V5 rail ≈ 5.0 V** (measured 5.01 V) and **3V3 rail** (measured 3.29 V) from the ESP32's own regulator.
- 5 V users: display VCC, LED strip (through the diode), RTC.
- 3.3 V users: encoder, button pull-up. (Display RESET follows EN, which sits at 3.3 V while running.)
- All grounds joined.
- **Update 2026-10-15 (USB-only, no Wi-Fi):** the gadget must sit on a **PC USB port** (data link to the companion), not a charger. Firmware limiter keeps the whole gadget ≤ 450 mA; 10 min full-power test on the PC port passed (no resets). The note below is from the Wi-Fi plan.
- Rough worst-case current: ESP32 with Wi-Fi ~250 mA + display ~100 mA + strip (17 LEDs, brightness capped ~20%) ~200 mA ≈ **550 mA**. A PC USB-2 port gives 500 mA → use a **phone charger (1 A or more)** for the finished gadget; a PC port is fine for testing with the strip off or dim.

## Before powering (checks, in this order)
1. ~~Pin labels~~ confirmed from photo (2026-10-13). Display header order from the VCC end: VCC, GND, CS, RESET, D/C, SDI, SCK, LED, SDO(MISO), T_CLK, T_CS, T_DIN, T_OUT, T_IRQ.
2. Every new wiring step: **unplug USB before changing any wire**; replug only after re-checking.

## Upload fix: 100 µF on EN (added 2026-10-13)
| Part | From | To | Notes |
|---|---|---|---|
| 100 µF 25 V electrolytic | **EN** (long leg, +) | **GND** (stripe, −) | the home PC is too slow (~2 ms) for the board's automatic download mode; this slows EN down, and `%LOCALAPPDATA%\esptool\esptool.cfg` holds BOOT for 2 s. EN also drives the display RESET, so the display resets together with the ESP32 (fine). Board starts ~1 s later. **Permanent part** (breadboard → perfboard → final build). |

## Risks / open items
- **DS1307 5 V pull-ups on SDA/SCL** (out of spec for ESP32, has worked before; leak current is tiny, ~0.3 mA). Photo: the likely pull-ups are **R2 and R3, marked 472 (4.7 kΩ)** – confirm with a beep test before touching anything. **D1** (charging diode, glass body, top right) is present. Options, to decide in Phase B – no SMD desoldering unless the user chooses it:
  a) leave as is (works, small risk); b) remove R2, R3 and D1 and add 4.7 kΩ to 3V3 (fiddly tiny-part desoldering); c) swap the coin cell for a rechargeable LIR2032 (solves D1 only); d) buy a DS3231 (3.3 V native, solves everything).
- **GPIO 19 vs SPI (corrected 2026-10-15):** the ESP32 SPI driver attaches MISO to GPIO 19 **even when MISO = -1** (`spiAttachMISO` falls back to the default pin 19 and sets it to INPUT) – so `SPI.begin(18, -1, 23)` does NOT keep 19 free (earlier note was wrong). Fix in firmware: set `pinMode(19, OUTPUT)` **after** the display is started (leds::begin() runs after display::begin()). The display never reads, so nothing is lost.
- GPIO 14 and 15 output brief pulses at boot – fine for an input button (14) / unused (15).
- If the display flickers or shows garbage at full SPI speed with long jumper wires, lower the SPI clock for testing (e.g. 27 MHz instead of 40).

## Carrier board (v1, 2026-10-15)
All of the above meets on the carrier board – see `docs/wiring-diagram/`. Change from the breadboard version: the 3 status-LED 220 Ω resistors sit at the LEDs (roof harness), the RTC gets 4.7 kΩ pull-ups to 3V3 on the carrier (its own 5 V pull-ups R2/R3 and D1 get snipped).
