// Kalos Dex – CASE PARTS (printable)
// Export one part at a time, e.g.:
//   openscad -D 'part="tub"' -o stl/case_tub.stl kalos_case.scad
// part = "tub" | "back_lid" | "screen_ring" | "screen_face" | "display_clamp" | "arch_fill"
//      | "switch_retainer" | "button_cap" | "knob" | "weight_cover"
//      | "cap" | "cap_rim" | "cap_band" | "assembly" (preview only)
part = "assembly";
//
// Coordinates: X = left -> right (seen from the front), Y = front -> back (0 = front face),
//              Z = up (0 = desk). Right-handed, so printed parts are NOT mirrored.
//
// Front of the gadget, top to bottom:  top cap | screen zone | bottom cap
// - Caps (red) = the user's prop STLs, front half, 110 % (caps.scad), glued on the tub's front edge.
// - Screen zone: a WHITE ring (side glow channels for the LED strip + thin bands at top/bottom)
//   with the GREY screen face behind its front lip; the display is clamped behind the face.
// - Tub (red) = sides + roof + floor, open front and open back; a removable back lid (red)
//   carries the carrier board + ESP32, the RTC and the weight pocket.

include <caps.scad>
show_preview = false;

$fn = 48;

// =====================================================================================
// TUNABLES – change these after the fit-check print
// =====================================================================================
PILOT       = 2.75;   // pilot hole for M3 x 8 self-tapping screws (fit-check: 2.7 works but needs a lot of force)
PILOT_DIP   = 0.6;    // small funnel at the hole's mouth so the screw starts straight
CLEAR_M3    = 3.4;    // clearance hole for an M3 screw
LED_HOLE    = 5.1;    // 5 mm status LED through the roof (fit-check 2026-10-15: 5.1 perfect)
BUTTON_HOLE = 9.5;    // roof hole for a button cap
CAP_D       = 9.2;    // button cap (fit-check: 9.0 in 9.5 slid fine; user wants 0.1 less gap all round)
CAP_BORE    = 3.5;    // button cap hole for the switch stem (fit-check: 3.3 won't go on, 3.4 very tight, 3.5 OK)
COLLAR_HOLE = 7.2;    // KY-040 threaded collar through the roof
KNOB_BORE   = 6.3;    // knob hole for the Ø5.9 shaft (fit-check 2: 6.3 / 4.8 = good)
KNOB_FLAT   = 4.8;    // knob hole across the D-flat
GLASS_T     = 3.6;    // display glass + frame thickness in front of its PCB
FIT         = 0.25;   // general sliding clearance (user's printer: 0.2 snug, 0.25 moving)

// =====================================================================================
// MAIN SIZES (layout v3: prop scaled 110 %, 55 deep)
// =====================================================================================
W = CAP_W;               // 83.6  length (side to side)
H = 2 * CAP_H + MID_H;   // 189.4 height
D = 55;                  // depth (front to back)
WALL = 2.4;              // tub walls / roof / floor
CORNER_R = 6.5 * S;      // 7.15  rounded corners seen from the front (same as the caps)
TUB_Y0 = HALF_T;         // 7.15  tub starts behind the caps
MID_Z0 = CAP_H;          // 50.7  screen zone bottom
MID_Z1 = CAP_H + MID_H;  // 138.7 screen zone top
ROOF_IN = H - WALL;      // underside of the roof
IN_X0 = WALL; IN_X1 = W - WALL;       // inside of the side walls

// ---- display ----
DISP_W = 55; DISP_H = 89.5;           // PCB
LIT_W = 48.96; LIT_H = 65.28;         // lit area
LIT_BOTTOM = 9.72;                    // lit area bottom edge -> PCB bottom edge (header at the TOP)
GLASS_BOT_GAP = 6.5;                  // PCB bottom edge -> glass
HOLE_DX = 48.95; HOLE_FROM_EDGE = 3.125;   // PCB mounting holes (only the bottom two are used)
WINDOW_MARGIN = 0.5;
lit_z0 = MID_Z0 + (MID_H - LIT_H) / 2;     // lit area centred in the screen zone
disp_z0 = lit_z0 - LIT_BOTTOM;             // PCB bottom edge
disp_x0 = (W - DISP_W) / 2;

// ---- screen zone stack (Y) ----
LIP_T = 3;                // white ring's front lip in front of the face (= how deep the face sits)
FACE_T = 3;               // grey screen face
face_y1 = LIP_T + FACE_T; // face back = glass front
pcb_y0 = face_y1 + GLASS_T;
pcb_y1 = pcb_y0 + 1.6;

// ---- side glow channels (white ring) ----
CH_X0 = 3.7;              // channel outer edge from the case side
CH_W = 3.2;               // strip 2.4–2.8 thick
SEP_T = 1.2;              // thin white wall between the strip and the face (glows)
CH_Y0 = 1.6;              // front skin in front of the channel
// ONE continuous strip (2026-10-15): it sits at the BACK of the channel (behind the caps' plane),
// so it can leave the channel at both ends: up the left, across the top inside the case
// (hidden, above the display + top arch fill), down the right. Glued start + LED 1 hidden
// below the left channel. The air gap in front of the LEDs spreads the glow more evenly.
STRIP_Y0 = TUB_Y0;        // strip's front edge
CH_DEPTH = STRIP_Y0 + 10.5 + 0.2 - CH_Y0;   // channel from the front skin to behind the strip
BAND = 1.5;               // thin white bands at the top and bottom of the screen zone
LIP = 1.0;                // how far the ring's lip overlaps the face edge
FACE_GAP = 0.25;          // face plate clearance inside the ring opening
DISP_GAP = 0.25;          // display clearance between the face's side guides
face_x0 = CH_X0 + CH_W + SEP_T + 0.1;   // 8.1 + clearance
face_z0 = MID_Z0 + BAND + 0.1;
face_z1 = MID_Z1 - BAND - 0.1;

// ---- roof row (layout v3): Feed  Play  KNOB  Back  Play/Pause ; LEDs 1 2 3 ----
KNOB_D = 18; GAP = 2.5;
CAP_PROTRUDE = 2;         // button caps stick out of the roof this much (user: 1 mm less than 3)
inner_dx = KNOB_D / 2 + GAP + CAP_D / 2;     // 16
outer_dx = inner_dx + CAP_D + GAP;           // 27.5
ROW_Y = 33;               // depth of the button/knob row
LED_Y = 15;               // depth of the LED row
button_x = [W / 2 - outer_dx, W / 2 - inner_dx, W / 2 + inner_dx, W / 2 + outer_dx];
led_x = [W / 2 - inner_dx, W / 2, W / 2 + inner_dx];
LED_LEN = 8.6;            // 5 mm LED: flange top -> dome tip
LED_PROTRUDE = 3;         // dome sticks out of the roof this much
led_flange_z = H + LED_PROTRUDE - LED_LEN;

// ---- tact switches (6x6 tall stem) + caps ----
SW_BODY = 6.1; SW_BODY_H = 2.6; SW_STEM_ABOVE = 12.6 - 2.6;  // measured 2026-10-15: body 2.6, total 12.6
SW_LEG_SPAN = 7.6;        // body + legs sticking out on two sides
CAP_FLANGE_D = 11; CAP_FLANGE_T = 1.2; CAP_TRAVEL = 0.8;
CAP_TOP = 2;              // solid top of the cap above the stem hole (fit-check 2 cap had 1 layer – bug)
cap_flange_z0 = ROOF_IN - CAP_FLANGE_T;              // flange bottom (cap resting up against the roof)
cap_len = H + CAP_PROTRUDE - cap_flange_z0;          // whole cap, flange bottom to top
CAP_BORE_DEPTH = cap_len - CAP_TOP;                   // stem hole depth (3.6 with a 2 mm top)
assert(CAP_TOP >= 1.6, "button cap top too thin");
sw_body_top = cap_flange_z0 + CAP_BORE_DEPTH - SW_STEM_ABOVE;
sw_body_z0 = sw_body_top - SW_BODY_H;                // = holder bottom
holder_x = [4, 31.5];     // left holder; the right one is its mirror image
HOLDER_Y0 = ROW_Y - 6.2; HOLDER_Y1 = ROW_Y + 6.2;
RETAINER_T = 2.4;
RET_GAP = 1.0; RET_WASHER_D = 5.6;   // retainer v4: built-in washer = gap under the switches for the wires
RET_X0 = WALL + 0.6;      // retainer outer end (block starts at holder_x[0] = 4)
holder_screw_x = [holder_x[0] + 2, (button_x[0] + button_x[1]) / 2];   // end + between the two switches

// ---- KY-040 under the roof ----
KY_BODY = 12.4;           // encoder body 11.9 x 12.0 (measured) + clearance
KY_HOLE_SIDE = 11;        // board edge with the mounting holes is 11 mm from the shaft (other edge 8 mm)
KY_BOARD_Z0 = ROOF_IN - 8.3 - 1.6;   // board underside when the encoder body touches the roof

// ---- back lid ----
LID_T = 2.4;
lid_y0 = D - LID_T;
LID_BOSS_LEN = 10;
lid_bosses = [[W / 2, ROOF_IN - 4.5], [IN_X0 + 3.5, 160], [IN_X1 - 3.5, 160],
              [IN_X0 + 3.5, 100], [IN_X1 - 3.5, 100], [IN_X1 - 3.5, IN_X0 + 6]];

// ---- carrier board + ESP32 on the lid; USB out the RIGHT wall ----
// The carrier is cut from the perfboard along hole lines: 25 x 16 hole pitches, so hole
// column c (1..24) sits at x = carrier_x0 + c * PITCH and row r (1..15) at z = carrier_z0 + r * PITCH.
// ESP32 headers in rows 2 and 12, columns 2..20 (docs/carrier-board.md); USB end to the right.
PITCH = 2.54;
STANDOFF = 4;                         // gap behind the carrier for solder joints
carrier_y0 = lid_y0 - STANDOFF - 1.6; // carrier front face
CARRIER_W = 25 * PITCH; CARRIER_H = 16 * PITCH;    // 63.5 x 40.64
carrier_x0 = IN_X0 + 1.2; carrier_z0 = WALL + 2.0; // clear of the tub's rounded inside corner
esp_x0 = carrier_x0 + 11 * PITCH - 27.5;            // pins centred on column 11 (board 55 long)
esp_z0 = carrier_z0 + 7 * PITCH - 14;               // centred on row 7 (board 28 wide)
esp_front_y = carrier_y0 - 8.5 - 1.5;               // ESP32 PCB front face (on 8.5 mm female headers)
usb_y = esp_front_y - 1.25;                          // centre of the micro-USB socket
usb_z = esp_z0 + 14;
USB_PLUG_W = 10.8; USB_PLUG_T = 6.5;  // plug overmold (plug passes through a snug hole)

// ---- weight pocket + RTC on the lid ----
WP_X0 = 12; WP_X1 = W - 12; WP_Z0 = 60; WP_Z1 = 100; WP_DEPTH = 10; WP_WALL = 2;
RTC_X0 = 12; RTC_Z0 = 118;   // was 146: the RTC would hit the knob's jumper plugs (printed v1 lids: ignore the fence, tape the RTC here)
RTC_X0_UNUSED = 0; RTC_W = 27.5 + 0.4; RTC_H = 28.6 + 0.4;   // RTC board measured 27.5 x 28.6

// =====================================================================================
// HELPERS
// =====================================================================================
// 2D rounded rectangle, lower-left corner at the origin
module rrect(w, h, r) {
  translate([r, r]) offset(r = r) square([max(w - 2 * r, 0.01), max(h - 2 * r, 0.01)]);
}
// Extrude a 2D shape drawn in the X-Z plane (2D x = X, 2D y = Z) from depth y0 to y1
module ext_y(y0, y1) {
  translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) children();
}
// Pilot hole for an M3 self-tapping screw along +Z, mouth at z = 0, with a small funnel
module pilot_hole(h) {
  translate([0, 0, -0.01]) cylinder(d1 = PILOT + 2 * PILOT_DIP, d2 = PILOT, h = PILOT_DIP + 0.01);
  cylinder(d = PILOT, h = h);
}
// Vertical (Z) hole with a teardrop top pointing to +Y: prints cleanly when the part
// lies front-down (Y up on the printer)
module tear_hole(d, h) {
  linear_extrude(h) hull() { circle(d = d); rotate(45) square(d / 2); }
}
// Wraps a feature that hangs from the roof so its front (-Y) side slopes at 45°:
// no supports when the tub prints front-down.
module hang_from_roof(drop) {
  hull() {
    children();
    // the feature's outline at the roof, shifted forward by its height -> 45° front face
    translate([0, -drop, 0]) intersection() {
      children();
      translate([-500, -500, ROOF_IN - 0.01]) cube([1000, 1000, 0.02]);
    }
  }
}

// =====================================================================================
// TUB
// =====================================================================================
module tub() {
  difference() {
    union() {
      difference() {
        ext_y(TUB_Y0, D) rrect(W, H, CORNER_R);
        ext_y(TUB_Y0 - 1, D + 1) translate([WALL, WALL]) rrect(W - 2 * WALL, H - 2 * WALL, CORNER_R - WALL);
      }
      led_bar();
      for (side = [0, 1]) holder_mount(side);
      ky_ribs();
      for (b = lid_bosses) lid_boss(b);
    }
    // roof holes
    for (x = led_x) translate([x, LED_Y, led_flange_z - 1]) tear_hole(LED_HOLE, 20);
    for (x = button_x) translate([x, ROW_Y, ROOF_IN - 1]) tear_hole(BUTTON_HOLE, 10);
    translate([W / 2, ROW_Y, ROOF_IN - 1]) tear_hole(COLLAR_HOLE, 10);
    // the white ring's back blocks slide in here: widen the opening 0.1 per side (-> 0.25 clearance)
    translate([IN_X0 - 0.1, TUB_Y0 - 1, MID_Z0]) cube([W - 2 * IN_X0 + 0.2, CH_Y0 + CH_DEPTH - TUB_Y0 + 1.3, MID_H]);
    // USB plug hole in the right wall (snug: the wall carries any tug on the cable)
    translate([IN_X1 - 1, usb_y - USB_PLUG_T / 2 - 0.2, usb_z - USB_PLUG_W / 2 - 0.2])
      cube([WALL + 2, USB_PLUG_T + 0.4, USB_PLUG_W + 0.4]);
    // pilot holes for the back lid screws
    for (b = lid_bosses) translate([b[0], lid_y0 + 0.01, b[1]]) rotate([90, 0, 0]) pilot_hole(b[1] > ROOF_IN - 8 ? 8 : 9);   // mouth at the lid (screw tip may poke out of the short roof boss – fine)
    // shallow round recesses under the base for pads / tape / TPU feet later
    for (x = [12, W - 12], y = [TUB_Y0 + 9, D - 9]) translate([x, y, -0.01]) cylinder(d = 10.5, h = 0.6);
  }
}
// Bar under the roof that the LED flanges press against (sets how far the domes stick out)
module led_bar() {
  translate([led_x[0] - 5, TUB_Y0, led_flange_z]) cube([led_x[2] - led_x[0] + 10, LED_Y + 5 - TUB_Y0, ROOF_IN - led_flange_z + 0.01]);
}
// Block under the roof that holds two switches + the caps' flanges (left: side 0, right: 1)
module holder_mount(side) {
  mirror_x(side) hang_from_roof(ROOF_IN - sw_body_z0)
    translate([holder_x[0], HOLDER_Y0, sw_body_z0]) cube([holder_x[1] - holder_x[0], HOLDER_Y1 - HOLDER_Y0, ROOF_IN - sw_body_z0 + 0.01]);
}
module holder_cuts(side) {
  mirror_x(side) {
    for (x = [button_x[0], button_x[1]]) {
      // switch pocket (open at the bottom, legs out on two sides)
      translate([x - SW_LEG_SPAN / 2, ROW_Y - (SW_BODY + 0.4) / 2, sw_body_z0 - 1]) cube([SW_LEG_SPAN, SW_BODY + 0.4, SW_BODY_H + 1]);  // 0.2 per side
      // stem hole
      translate([x, ROW_Y, sw_body_top - 0.01]) cylinder(d = 4, h = 20);
      // flange chamber, open to the back so the caps slide in from behind
      hull() {
        translate([x, ROW_Y, cap_flange_z0 - CAP_TRAVEL]) cylinder(d = CAP_FLANGE_D + 0.6, h = CAP_FLANGE_T + CAP_TRAVEL + 0.01);
        translate([x, HOLDER_Y1 + 1, cap_flange_z0 - CAP_TRAVEL]) cylinder(d = CAP_FLANGE_D + 0.6, h = CAP_FLANGE_T + CAP_TRAVEL + 0.01);
      }
    }
    // pilots for the retainer screws
    for (x = holder_screw_x) translate([x, ROW_Y, sw_body_z0]) pilot_hole(5.5);  // stops below the flange slot
  }
}
module mirror_x(on) { if (on) translate([W, 0, 0]) mirror([1, 0, 0]) children(); else children(); }
// Ribs either side of the KY-040 body so it can't turn when the knob is turned
module ky_ribs() {
  for (dx = [-1, 1]) hang_from_roof(5)
    translate([W / 2 + dx * (KY_BODY / 2 + 0.5) - 0.5, ROW_Y - 5, ROOF_IN - 5]) cube([1, 10, 5.01]);
}
// Boss for a back-lid screw, joined to the nearest wall by a web whose front slopes at
// least 45° – prints without supports. The boss under the roof (behind the knob) is shorter, so its
// web starts BEHIND the KY-040 body (v1 web reached 5 mm into the encoder body – found 2026-10-15
// by tools/assembly_check.py; printed v1 tubs need that wedge trimmed).
module lid_boss(b) {
  roof = b[1] > ROOF_IN - 8;
  len = roof ? 5 : LID_BOSS_LEN;
  web = len + (roof ? ROOF_IN - (b[1] - 3.5) : 9);   // 45°: as far forward as the boss hangs down
  hull() {
    translate([b[0], lid_y0 - len, b[1]]) rotate([-90, 0, 0]) cylinder(d = 7, h = len);
    if (roof) translate([b[0] - 3.5, lid_y0 - web, ROOF_IN - 0.01]) cube([7, web, 0.01]);
    else if (b[0] < W / 2) translate([IN_X0 - 0.01, lid_y0 - web, b[1] - 3.5]) cube([0.01, web, 7]);
    else translate([IN_X1, lid_y0 - web, b[1] - 3.5]) cube([0.01, web, 7]);
  }
}
module tub_final() {
  difference() {
    tub();
    for (side = [0, 1]) holder_cuts(side);
    // room for the KY-040 board's wider (mounting-hole) side, either way round – it overlapped the
    // inner end of a switch block by 0.7 mm (found 2026-10-15 at test fit)
    translate([W / 2 - KY_HOLE_SIDE - 0.5, ROW_Y - 12, KY_BOARD_Z0 - 0.6]) cube([2 * KY_HOLE_SIDE + 1, 28, 1.6 + 1.0]);
  }
}

// =====================================================================================
// BACK LID (printed flat, inside face up): carrier rails, weight pocket, RTC fence
// =====================================================================================
module back_lid() {
  difference() {
    union() {
      ext_y(lid_y0, D) translate([WALL + FIT, WALL + FIT]) rrect(W - 2 * (WALL + FIT), H - 2 * (WALL + FIT), CORNER_R - WALL - FIT);
      intersection() {   // keep the rails inside the tub's rounded inside corners
        carrier_rails();
        ext_y(0, D) translate([WALL + FIT, WALL + FIT]) rrect(W - 2 * (WALL + FIT), H - 2 * (WALL + FIT), CORNER_R - WALL - FIT);
      }
      weight_pocket();
      // RTC fence (the RTC is stuck on with double-sided tape inside it)
      translate([RTC_X0, lid_y0 - 1.5, RTC_Z0]) difference() {
        translate([-1.2, 0, -1.2]) cube([RTC_W + 2.4, 1.5, RTC_H + 2.4]);
        translate([0, -1, 0]) cube([RTC_W, 4, RTC_H]);
      }
    }
    for (b = lid_bosses) translate([b[0], lid_y0 - 1, b[1]]) rotate([-90, 0, 0]) cylinder(d = CLEAR_M3, h = LID_T + 2);
    // finger notch to lift the lid out
    translate([W / 2, D + 0.5, H - 30]) rotate([90, 0, 0]) cylinder(d = 12, h = 1.5);
  }
}
// The carrier perfboard slides in sideways (from the left) between two rails; once the lid
// is in, the tub's left wall stops it sliding out.
module carrier_rails() {
  slot_y0 = carrier_y0 - 0.2; slot_y1 = carrier_y0 + 1.6 + 0.2;
  // the rails' front lips only cover 1 mm of the board's edges, clear of the header bases
  // bottom rail
  difference() {
    translate([carrier_x0, slot_y0 - 1.5, WALL + FIT]) cube([CARRIER_W + 0.7, lid_y0 - slot_y0 + 1.5, carrier_z0 + 1.0 - WALL - FIT]);
    translate([carrier_x0 - 1, slot_y0, carrier_z0 - 0.3]) cube([CARRIER_W + 2, slot_y1 - slot_y0, 5]);
  }
  // top rail
  difference() {
    translate([carrier_x0, slot_y0 - 1.5, carrier_z0 + CARRIER_H - 1.0]) cube([CARRIER_W + 0.7, lid_y0 - slot_y0 + 1.5, 3.5]);
    translate([carrier_x0 - 1, slot_y0, carrier_z0 + CARRIER_H - 1.0 - 0.01]) cube([CARRIER_W + 2, slot_y1 - slot_y0, 1.3]);
  }
  // stop at the right-hand end
  translate([carrier_x0 + CARRIER_W + 0.2, slot_y0 - 1.5, carrier_z0]) cube([2, lid_y0 - slot_y0 + 1.5, CARRIER_H]);
}
module weight_pocket() {
  difference() {
    translate([WP_X0 - WP_WALL, lid_y0 - WP_DEPTH, WP_Z0 - WP_WALL]) cube([WP_X1 - WP_X0 + 2 * WP_WALL, WP_DEPTH, WP_Z1 - WP_Z0 + 2 * WP_WALL]);
    translate([WP_X0, lid_y0 - WP_DEPTH - 1, WP_Z0]) cube([WP_X1 - WP_X0, WP_DEPTH + 1, WP_Z1 - WP_Z0]);
  }
  // two bosses for the cover screws (inside the pocket, top and bottom middle)
  for (z = [WP_Z0 + 3, WP_Z1 - 3]) difference() {
    translate([W / 2, lid_y0 - WP_DEPTH, z]) rotate([-90, 0, 0]) cylinder(d = 7, h = WP_DEPTH);
    translate([W / 2, lid_y0 - WP_DEPTH, z]) rotate([-90, 0, 0]) pilot_hole(WP_DEPTH - 1);
  }
}
module weight_cover() {
  difference() {
    translate([WP_X0 - WP_WALL, lid_y0 - WP_DEPTH - 1.6, WP_Z0 - WP_WALL]) cube([WP_X1 - WP_X0 + 2 * WP_WALL, 1.6, WP_Z1 - WP_Z0 + 2 * WP_WALL]);
    for (z = [WP_Z0 + 3, WP_Z1 - 3]) translate([W / 2, lid_y0 - WP_DEPTH - 3, z]) rotate([-90, 0, 0]) cylinder(d = CLEAR_M3, h = 5);
  }
}

// =====================================================================================
// SCREEN ZONE: white ring (glow channels) + grey face + display clamps
// =====================================================================================
module screen_ring() {
  difference() {
    union() {
      // front part, full width, front side edges rounded like the caps
      intersection() {
        ext_y(0, TUB_Y0) translate([0, MID_Z0]) square([W, MID_H]);
        rounded_front_sides();
      }
      // side blocks that slide into the tub behind the front part (hold the channels)
      for (side = [0, 1]) mirror_x(side) translate([IN_X0 + 0.15, TUB_Y0 - 0.01, MID_Z0])
        cube([face_x0 - 0.1 - IN_X0 - 0.15, CH_Y0 + CH_DEPTH - TUB_Y0 + 0.01, MID_H]);
    }
    // opening for the face, behind the lip
    translate([face_x0 - 0.1, LIP_T, face_z0 - 0.1]) cube([W - 2 * (face_x0 - 0.1), 50, face_z1 - face_z0 + 0.2]);
    // the lip's own opening (lip overlaps the face edge by LIP)
    translate([face_x0 + LIP, -1, face_z0 + LIP]) cube([W - 2 * (face_x0 + LIP), LIP_T + 2, face_z1 - face_z0 - 2 * LIP]);
    // glow channels, open at the back so the strip goes in from behind;
    // behind the caps' plane they're also open at the top and bottom (strip passes through)
    for (side = [0, 1]) mirror_x(side) {
      translate([CH_X0, CH_Y0, MID_Z0 + BAND]) cube([CH_W, 50, MID_H - 2 * BAND]);
      translate([CH_X0, STRIP_Y0 - 0.2, MID_Z0 - 1]) cube([CH_W, 50, MID_H + 2]);
    }
  }
}
// Rounds the two front side edges (R = CORNER_R, same as the caps' edge profile)
module rounded_front_sides() {
  r = CORNER_R;
  translate([0, 0, -1]) linear_extrude(H + 2) hull() {
    translate([r, r]) circle(r = r, $fn = 64);
    translate([W - r, r]) circle(r = r, $fn = 64);
    translate([0, TUB_Y0]) square([W, 1]);
  }
}
// Grey face: sits behind the ring's lip, display glass pressed against its back.
module screen_face() {
  win_x0 = (W - LIT_W) / 2 - WINDOW_MARGIN;
  win_z0 = lit_z0 - WINDOW_MARGIN;
  hole_z = disp_z0 + HOLE_FROM_EDGE;
  difference() {
    union() {
      // 0.25 clearance all round inside the ring's opening (was 0.1 – too tight, found 2026-10-15)
      translate([face_x0 + FACE_GAP - 0.1, LIP_T, face_z0 + FACE_GAP - 0.1]) cube([W - 2 * (face_x0 + FACE_GAP - 0.1), FACE_T, face_z1 - face_z0 - 2 * (FACE_GAP - 0.1)]);
      // two locating posts with pins into the display's bottom mounting holes
      for (dx = [-1, 1]) translate([W / 2 + dx * HOLE_DX / 2, face_y1 - 0.01, hole_z]) rotate([-90, 0, 0]) {
        cylinder(d = 5, h = GLASS_T + 0.01);
        cylinder(d = 2.5, h = GLASS_T + 1.4);
      }
      // side guides (stop the display sliding sideways)
      // side guides, 0.25 clear of the display's edges (was 0 – too tight)
      for (dx = [-1, 1]) translate([W / 2 + dx * (DISP_W / 2 + DISP_GAP + 0.6) - 0.6, face_y1 - 0.01, disp_z0 + 12])
        cube([1.2, GLASS_T + 1.0, 40]);
      // bosses for the two clamp bars
      for (dx = [-1, 1], z = clamp_z) translate([W / 2 + dx * clamp_boss_dx, face_y1 - 0.01, z]) rotate([-90, 0, 0])
        difference() { cylinder(d = 5.6, h = clamp_boss_h); translate([0, 0, clamp_boss_h]) mirror([0, 0, 1]) pilot_hole(clamp_boss_h - 1); }
    }
    translate([win_x0, -1, win_z0]) cube([LIT_W + 2 * WINDOW_MARGIN, 20, LIT_H + 2 * WINDOW_MARGIN]);
  }
}
clamp_z = [lit_z0 + 15, lit_z0 + LIT_H - 15];
clamp_boss_dx = DISP_W / 2 + 3.0;            // just outside the display's side edges
CLAMP_PAD = 3;                               // pads touch the PCB back only near its edges
clamp_boss_h = pcb_y1 + CLAMP_PAD - face_y1; // boss top = clamp bar's front face
// Bar that presses the display PCB against the face (2 needed). Pads only near the edges.
module display_clamp() {
  bar_y0 = pcb_y1 + CLAMP_PAD;
  difference() {
    union() {
      translate([W / 2 - clamp_boss_dx - 3, bar_y0, -4]) cube([2 * clamp_boss_dx + 6, 3, 8]);   // ends stay clear of the glow channels
      for (dx = [-1, 1]) translate([W / 2 + dx * (DISP_W / 2 - 2) - 2, pcb_y1, -4]) cube([4, CLAMP_PAD + 0.01, 8]);
    }
    for (dx = [-1, 1]) translate([W / 2 + dx * clamp_boss_dx, bar_y0 - 1, 0]) rotate([-90, 0, 0]) cylinder(d = CLEAR_M3, h = 6);
  }
}

// =====================================================================================
// SMALL PARTS
// =====================================================================================
// Grey plate glued behind each cap's arch (the "screen" you see inside the Pokeball arch).
// Shown here for the bottom cap: open end at the top.
module arch_fill() {
  // 56 wide: covers the arch and, seen at an angle, the display PCB edges behind it.
  // 23 tall: the hidden strip run crosses above the top one.
  translate([W / 2 - 28, TUB_Y0, MID_Z0 - 23]) cube([56, 1.6, 23]);
}
// Plate under each switch holder: carries the switches when a button is pressed.
module switch_retainer() {
  // v4 (2026-10-19, user's design): flat plate, same footprint, NO leg holes or channels, ONE loose
  // (clearance) hole for the middle screw. A built-in 1 mm washer around that hole holds the plate
  // RET_GAP below the switch block, so the bent legs + wires under the switches aren't crushed.
  // The end of the plate (no screw there any more) is superglued to the block.
  // Washer kept to d 5.6: the switch legs nearest the screw sit 3.0 mm from its centre.
  z0 = sw_body_z0 - RET_GAP - RETAINER_T;
  difference() {
    union() {
      // inner end 1.5 mm short of the block's end: clear of the KY-040 board's wide side
      translate([RET_X0, HOLDER_Y0, z0]) cube([holder_x[1] - 1.5 - RET_X0, HOLDER_Y1 - HOLDER_Y0, RETAINER_T]);
      translate([holder_screw_x[1], ROW_Y, z0 + RETAINER_T - 0.01]) cylinder(d = RET_WASHER_D, h = RET_GAP + 0.01);
    }
    translate([holder_screw_x[1], ROW_Y, z0 - 1]) cylinder(d = CLEAR_M3, h = RETAINER_T + RET_GAP + 2);
  }
}
// Button cap (v2, 2026-10-15): NO flange. The flanged version could not be installed – the
// flange (Ø11) is wider than the roof hole and the switch pocket, and the slot under the roof is
// too low for the cap's body. Now: switch goes in from below (retainer plate), cap is pushed DOWN
// from above through the roof hole onto the switch stem (snug fit, optional dot of superglue).
// v3 (2026-10-19, user): 1 mm SHORTER FROM THE BOTTOM – the caps bottomed out before the switch
// clicked. The top and the stem hole's ceiling stay put (same height on the stem, same 2 mm top), the
// cap's lower rim moves up 1 mm = 1 mm more free travel; the stem hole is 1 mm shallower (2.6 mm grip).
// Also 0.2 mm THINNER (smooth moving fit in the roof hole). Only the printed cap changes –
// CAP_D / CAP_PROTRUDE still set the (already printed) tub.
CAP_V3_SHORTER = 1.0; CAP_V3_THINNER = 0.2;
module button_cap() {
  d = CAP_D - CAP_V3_THINNER;
  difference() {
    cylinder(d = d, h = cap_len - CAP_V3_SHORTER);
    translate([0, 0, -1]) cylinder(d = CAP_BORE, h = CAP_BORE_DEPTH - CAP_V3_SHORTER + 1);
    // small chamfer at the bottom edge so it starts into the roof hole easily
    translate([0, 0, -0.01]) difference() { cylinder(d = d + 1, h = 0.6); cylinder(d1 = d - 1.2, d2 = d + 0.01, h = 0.61); }
  }
}
// Knob v2 (2026-10-19, user): a SKIRT ("hider") 6 mm down from the old bottom covers the silver
// collar thread + nut. Outside = straight continuation (same Ø and grip ribs). Inside = a cone from
// the shaft hole out to KNOB_SKIRT_WALL (3 lines) at the rim: ~38° from vertical -> prints top-up
// with no supports, and stays clear of the thread (Ø6.7) and the nut (11.3 across corners).
// Needs the knob pushed fully down onto the shaft (old bottom then ~7.6 mm above the roof -> rim ~3.6 mm above it,
// room to press the knob's push switch).
KNOB_SKIRT = 4; KNOB_SKIRT_WALL = 1.2; KNOB_CONE_H = 6;   // skirt 6 -> 4 mm (user: room to press the knob)
module knob() {
  difference() {
    union() {
      translate([0, 0, -KNOB_SKIRT]) cylinder(d = KNOB_D, h = 12 + KNOB_SKIRT);
      for (a = [0:15:359]) rotate(a) translate([KNOB_D / 2, 0, -KNOB_SKIRT]) cylinder(d = 1.2, h = 12 + KNOB_SKIRT, $fn = 12);
    }
    translate([0, 0, -1]) intersection() {
      cylinder(d = KNOB_BORE, h = 11);
      translate([-KNOB_BORE / 2, -KNOB_BORE / 2, 0]) cube([KNOB_BORE, KNOB_FLAT, 11]);
    }
    // skirt hollow: cone from the rim (thin wall) up to the shaft hole
    // (cone stays 6 mm tall for the ~38° no-support angle; its top end reaches 2 mm up into the old hub)
    translate([0, 0, -KNOB_SKIRT - 0.01]) cylinder(d1 = KNOB_D - 2 * KNOB_SKIRT_WALL, d2 = KNOB_BORE, h = KNOB_CONE_H + 0.02, $fn = 96);
    translate([0, 0, 11.4]) cylinder(d = 3, h = 1);   // dimple on top
  }
}

// =====================================================================================
// PART SELECTOR (each part placed flat for printing)
// =====================================================================================
// rotate([90,0,0]) puts the part's front (low Y) on the bed; rotate([-90,0,0]) its back.
// -D 'part="asm_<name>"' exports a part where it sits in the assembly (for fit checks).
if (search("asm_", part) == [0, 1, 2, 3]) {
  n = part;
  if (n == "asm_tub") tub_final();
  if (n == "asm_back_lid") back_lid();
  if (n == "asm_screen_ring") screen_ring();
  if (n == "asm_screen_face") screen_face();
  if (n == "asm_clamps") for (z = clamp_z) translate([0, 0, z]) display_clamp();
  if (n == "asm_display") display_model();
  if (n == "asm_esp") { translate([carrier_x0, carrier_y0, carrier_z0]) cube([CARRIER_W, 1.6, CARRIER_H]);
                        translate([esp_x0, esp_front_y, esp_z0]) cube([55, 10, 28]);
                        for (r = [14, 15]) translate([carrier_x0 + 0.5 * PITCH, carrier_y0 - 2.5 - 14, carrier_z0 + r * PITCH - 1.25]) cube([24 * PITCH, 16.5, 2.5]);
                        translate([esp_x0 + 55, usb_y - USB_PLUG_T / 2, usb_z - USB_PLUG_W / 2]) cube([40, USB_PLUG_T, USB_PLUG_W]); }
  if (n == "asm_retainers") for (side = [0, 1]) mirror_x(side) switch_retainer();
  if (n == "asm_strip") strip_path();
  // ---- bought parts under the roof (for the assembly-path check) ----
  if (n == "asm_switches") for (x = button_x) translate([x, ROW_Y, 0]) {
      translate([-SW_BODY / 2, -SW_BODY / 2, sw_body_z0]) cube([SW_BODY, SW_BODY, SW_BODY_H]);          // body
      translate([0, 0, sw_body_top]) cylinder(d = 3.2, h = SW_STEM_ABOVE);                               // stem
      for (lx = [-3.25, 3.25], ly = [-2.25, 2.25]) translate([lx - 0.35, ly - 0.35, sw_body_z0 - 4]) cube([0.7, 0.7, 4.5]);  // legs
    }
  if (n == "asm_buttoncaps") for (x = button_x) translate([x, ROW_Y, cap_flange_z0 + CAP_V3_SHORTER]) button_cap();
  if (n == "asm_leds") for (x = led_x) translate([x, LED_Y, 0]) {
      translate([0, 0, led_flange_z - 1]) cylinder(d = 5.8, h = 1);                 // rim
      translate([0, 0, led_flange_z]) cylinder(d = 5, h = LED_LEN - 2.5);          // body
      translate([0, 0, led_flange_z + LED_LEN - 2.5]) sphere(d = 5, $fn = 24);     // dome
      for (dx = [-1.27, 1.27]) translate([dx - 0.25, -0.25, led_flange_z - 15]) cube([0.5, 0.5, 14.5]);  // legs
    }
  if (n == "asm_ky") {   // KY-040: board under the roof, body between the ribs, collar through the roof
    translate([W / 2 - KY_HOLE_SIDE, ROW_Y - 10.8, ROOF_IN - 8.3 - 1.6]) cube([19, 26.1, 1.6]);   // board: mounting-hole edge on the LEFT, pins edge at the back
    translate([W / 2 - 6, ROW_Y - 6, ROOF_IN - 8.3]) cube([12, 12, 8.3]);                       // body 11.9 x 12
    translate([W / 2, ROW_Y, ROOF_IN - 0.01]) cylinder(d = 6.7, h = 7);                        // collar
    for (i = [0:4]) translate([W / 2 - 5.08 + i * 2.54 - 0.32, ROW_Y + 15.3 - 0.32, ROOF_IN - 8.3 - 1.6 - 9]) cube([0.64, 0.64, 9]);  // pins bent down
    translate([W / 2 - 6.4, ROW_Y + 15.3 - 1.3, ROOF_IN - 8.3 - 1.6 - 8 - 16]) cube([12.8, 2.6, 16]);   // 5 jumper plugs on those pins
  }
  if (n == "asm_lid_with_carrier") {   // the lid as it goes in: lid + carrier + ESP32 (USB plug NOT in yet) + RTC
    back_lid();
    translate([carrier_x0, carrier_y0, carrier_z0]) cube([CARRIER_W, 1.6, CARRIER_H]);
    translate([esp_x0, esp_front_y, esp_z0]) cube([55, 10, 28]);
    for (r = [14, 15]) translate([carrier_x0 + 0.5 * PITCH, carrier_y0 - 2.5 - 14, carrier_z0 + r * PITCH - 1.25]) cube([24 * PITCH, 16.5, 2.5]);
    translate([RTC_X0, lid_y0 - 7.5, RTC_Z0]) cube([RTC_W - 0.4, 6, RTC_H - 0.4]);
  }
  if (n == "asm_carrier_esp") { translate([carrier_x0, carrier_y0, carrier_z0]) cube([CARRIER_W, 1.6, CARRIER_H]);
                                translate([esp_x0, esp_front_y, esp_z0]) cube([55, 10, 28]); }
  if (n == "asm_usb_plug") translate([esp_x0 + 55, usb_y - USB_PLUG_T / 2, usb_z - USB_PLUG_W / 2]) cube([40, USB_PLUG_T, USB_PLUG_W]);
  if (n == "asm_esp_body") translate([esp_x0, esp_front_y, esp_z0]) cube([55, 10, 28]);
  if (n == "asm_caps_front") { translate([0, 0, MID_Z1]) cap_set(); translate([W, 0, MID_Z0]) rotate([0, 180, 0]) cap_set(); }
  if (n == "asm_arch_fills") { arch_fill(); translate([0, 0, MID_Z1 + MID_Z0]) mirror([0, 0, 1]) arch_fill(); }
  // ---- extra pieces for the 3D assembly guide (docs/assembly-guide.html) ----
  if (n == "asm_knob") translate([W / 2, ROW_Y, H + 4.6 + 0.5]) knob();
  if (n == "asm_weight_cover") weight_cover();
  if (n == "asm_carrier") { translate([carrier_x0, carrier_y0, carrier_z0]) cube([CARRIER_W, 1.6, CARRIER_H]);
                            for (r = [14, 15]) translate([carrier_x0 + 0.5 * PITCH, carrier_y0 - 2.5 - 14, carrier_z0 + r * PITCH - 1.25]) cube([24 * PITCH, 16.5, 2.5]); }
  if (n == "asm_rtc") translate([RTC_X0, lid_y0 - 7.5, RTC_Z0]) cube([RTC_W - 0.4, 6, RTC_H - 0.4]);
}
// Display as measured: glass (54.6 wide, 5.5 / 6.5 short of the PCB's top / bottom edge) + PCB with holes
module display_model() {
  translate([(W - 54.6) / 2, face_y1, disp_z0 + GLASS_BOT_GAP]) cube([54.6, GLASS_T, DISP_H - 5.5 - GLASS_BOT_GAP]);
  difference() {
    translate([disp_x0, pcb_y0, disp_z0]) cube([DISP_W, 1.6, DISP_H]);
    for (dx = [-1, 1], dz = [HOLE_FROM_EDGE, DISP_H - HOLE_FROM_EDGE])
      translate([W / 2 + dx * HOLE_DX / 2, pcb_y0 - 1, disp_z0 + dz]) rotate([-90, 0, 0]) cylinder(d = 3.15, h = 4);
  }
}
// The LED strip's path (10.5 wide on edge, 2.8 thick): below + up the left channel, across the top
// at STRIP_TOP_Z (corner radius 8), down the right channel. Used only for the collision check.
STRIP_TOP_Z = MID_Z1 + 23 + 8;   // above the top arch fill
module strip_path() {
  r = 8; x0 = CH_X0 + 0.2; x1 = W - CH_X0 - 0.2 - 2.8;
  translate([0, STRIP_Y0, 0]) {
    translate([x0, 0, MID_Z0 - 20]) cube([2.8, 10.5, STRIP_TOP_Z - r - (MID_Z0 - 20)]);       // left run
    translate([x1, 0, MID_Z0 + 2]) cube([2.8, 10.5, STRIP_TOP_Z - r - MID_Z0 - 2]);           // right run
    translate([x0 + r + 2.8, 0, STRIP_TOP_Z - 2.8]) cube([x1 - x0 - 2 * r - 2.8, 10.5, 2.8]); // top run
    for (c = [[x0 + r + 2.8, 1], [x1 - r, -1]]) translate([c[0], 0, STRIP_TOP_Z - r - 0.01])
      rotate([-90, 0, 0]) intersection() {
        difference() { cylinder(r = r + 2.8, h = 10.5); translate([0, 0, -1]) cylinder(r = r, h = 12.5); }
        translate([c[1] > 0 ? -20 : 0, -20, -1]) cube([20, 20, 12.5]);
      }
  }
}
module cap_set() { prop_front_half("pokedex_case_top.stl"); prop_front_half("pokeball_rim_top.stl"); black_band_front_half(); }
if (part == "tub") rotate([90, 0, 0]) translate([0, -TUB_Y0, 0]) tub_final();          // front edge down
if (part == "back_lid") rotate([-90, 0, 0]) translate([0, -D, 0]) back_lid();           // outside face down
if (part == "screen_ring") rotate([90, 0, 0]) screen_ring();                             // front down (standing, its top band would bridge 65 mm)
if (part == "screen_face") rotate([90, 0, 0]) translate([0, -LIP_T, 0]) screen_face();  // front down
if (part == "display_clamp") rotate([-90, 0, 0]) translate([0, -(pcb_y1 + CLAMP_PAD + 3), 0]) display_clamp();
if (part == "arch_fill") rotate([90, 0, 0]) translate([0, -TUB_Y0, 0]) arch_fill();
if (part == "switch_retainer") translate([0, 0, -(sw_body_z0 - RET_GAP - RETAINER_T)]) switch_retainer();
if (part == "button_cap") button_cap();
if (part == "knob") translate([0, 0, KNOB_SKIRT]) knob();   // top up, skirt rim on the bed
if (part == "weight_cover") rotate([90, 0, 0]) translate([0, -(lid_y0 - WP_DEPTH - 1.6), 0]) weight_cover();
// Test piece (fit-check 3): 50 mm straight slice of the ring's LEFT side, real depths, standing up
if (part == "ring_side_test") translate([0, 0, -75]) intersection() {
  screen_ring();
  translate([-1, -1, 75]) cube([14, 30, 50]);
}
if (part == "cap") prop_front_half("pokedex_case_top.stl");          // stands on its open end
// Rim: a thin ribbon (~3 mm deep) – standing up its arch top curls. Lies on its BACK, rounded
// front facing UP (smooth top surface, no bed texture); needs supports under the hidden back.
if (part == "cap_rim") rotate([-90, 0, 0]) translate([0, -HALF_T, 0]) prop_front_half("pokeball_rim_top.stl");
if (part == "cap_band") rotate([90, 0, 0]) translate([0, -(129 - PROP_Y0) * S, 0]) black_band_front_half();   // flat, on the bed

// Assembled preview (not for printing)
if (part == "assembly") {
  color("firebrick") tub_final();
  color("indianred") back_lid();
  color("white") screen_ring();
  color("dimgrey") screen_face();
  for (z = clamp_z) color("slategrey") translate([0, 0, z]) display_clamp();
  color("royalblue") display_model();
  for (side = [0, 1]) color("gold") mirror_x(side) switch_retainer();
  for (x = button_x) color("yellow") translate([x, ROW_Y, cap_flange_z0]) button_cap();
  color("orange") translate([W / 2, ROW_Y, H + 4.6 + 0.5]) knob();
  color("dimgrey") arch_fill();
  color("dimgrey") translate([0, 0, MID_Z1 + MID_Z0]) mirror([0, 0, 1]) arch_fill();
  color("darkgreen") translate([carrier_x0, carrier_y0, carrier_z0]) cube([CARRIER_W, 1.6, CARRIER_H]);
  color("black") translate([esp_x0, esp_front_y, esp_z0]) cube([55, 1.5, 28]);
  color("purple") translate([RTC_X0, lid_y0 - 7.5, RTC_Z0]) cube([RTC_W, 6, RTC_H]);
}
