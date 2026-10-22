// Kalos Dex – decorative caps from the user's prop STLs (reference/stl, personal use only)
// Keeps only the FRONT HALF of the prop (cut at its mid-plane, Y = 130 in the prop file) and scales it.
//
// Coordinates for all printable parts (right-handed, OpenSCAD's own):
//   X = left -> right (seen from the front), Y = front -> back (0 = front face), Z = up (0 = desk).
// The prop file already uses this: X 92..168 = width, Y 123.5..136.5 = thickness (front face at
// Y = 123.5), Z 0..46.1 = height with the open end (Pokeball arch) at Z = 0.

S = 1.1;                 // scale (layout v3)
STL = "../reference/stl/";

PROP_X0 = 92;            // prop's left edge
PROP_Y0 = 123.5;         // prop's front face
PROP_T = 13;             // prop thickness (mid-plane at half of it)
CAP_W = 76 * S;          // 83.6
CAP_H = 46.1 * S;        // 50.7
HALF_T = PROP_T / 2 * S; // front half thickness = 7.15
MID_H = 80 * S;          // screen zone height = 88

// A prop part, front half only, scaled, left edge at X = 0, front face at Y = 0,
// open end at Z = 0 pointing DOWN (the top cap's orientation).
module prop_front_half(file) {
  scale(S) intersection() {
    translate([-PROP_X0, -PROP_Y0, 0]) import(str(STL, file));
    translate([-1, 0, -1]) cube([200, PROP_T / 2, 100]);   // keep the front half
  }
}

// The black band is stored as a "U" (its circle's centre is at the top, Z = 19).
// It belongs round the arch as a "∩", so turn it upside down (a real flip: rotate about the
// depth axis) so the circle's centre sits at the open end.
module black_band_front_half() {
  translate([CAP_W / 2, 0, 19 * S / 2]) rotate([0, 180, 0]) translate([-CAP_W / 2, 0, -19 * S / 2])
    prop_front_half("pokeball_black1.stl");
}

module top_cap_parts() {
  color("red") prop_front_half("pokedex_case_top.stl");
  color("white") prop_front_half("pokeball_rim_top.stl");
  color("black") black_band_front_half();
}

// The bottom cap is a second top cap turned upside down (rotated about the depth axis).
module bottom_cap_parts() {
  translate([CAP_W, 0, CAP_H]) rotate([0, 180, 0]) top_cap_parts();
}

// Preview: both caps round the screen zone, seen from the front
show_preview = true;   // set false when this file is included from another file
if (show_preview) caps_preview();
module caps_preview() {
  bottom_cap_parts();
  translate([0, 0, CAP_H + MID_H]) top_cap_parts();
  color("lightblue") translate([7, HALF_T - 0.5, CAP_H - 20]) cube([CAP_W - 14, 0.5, MID_H + 40]);  // stand-in screen
}
