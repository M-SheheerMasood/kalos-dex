#!/usr/bin/env python3
"""PMD Sprite Collab zips -> one firmware header for a Pokémon line (4-bit frames, normal + shiny palette).

usage (needs Pillow: .scratch/venv/bin/python):
  sprite_convert.py            write firmware/kalos-dex/sprites/aegislash_line.h + previews in .scratch/previews/sprites/
  sprite_convert.py --check    exit 1 if the header on disk differs from what the zips give

Reads kalos-dex/assets/sprites-src/<Name>.zip and <Name>(Shiny).zip straight from the zip (no extracting).
Only row 0 of each sheet is used (the Pokémon facing us). Every frame of an animation is cropped to the same box,
placed relative to the PMD frame centre, so switching animations doesn't make the sprite jump.
Sprites and the header are git-ignored (third-party art, personal use): never commit or publish them.
"""
import io, sys, zipfile
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path
from PIL import Image

KD = Path(__file__).resolve().parents[1]
SRC = KD / "assets/sprites-src"
OUT = KD / "firmware/kalos-dex/sprites/aegislash_line.h"
PREVIEW = KD.parent / ".scratch/previews/sprites"
SCALE, BOX_W, BOX_H, GROUND = 3, 232, 142, 128          # Home pet area (home_screen.cpp), ground = PET_H - 14
JUMPY = {"play"}       # body cropped per frame + a "lift" per frame, so the firmware can draw the jump lower
LIFT_SCALE = 1.5       # screen px per source px of lift (the body itself is drawn x SCALE)

ROLES = ["idle", "sleep", "play", "hurt", "feed", "evolve"]   # what the firmware asks for -> PMD animation (first found)
PICK = {"idle": ["Idle"], "sleep": ["Sleep"], "play": ["Hop"], "hurt": ["Pain", "Hurt"], "feed": ["Eat", "Charge"],
        "evolve": []}   # evolve = the show-off move right after evolving INTO this form: picked per form below
OPTIONAL = {"evolve"}   # missing -> nullptr, the firmware falls back (evolve -> play)
# ---- per line (the only part to change for another Pokemon line): C name, zip, roles, per-form picks
FORMS = [
    ("HONEDGE", "Honedge", ROLES, {}),                                  # first form: never evolved into
    ("DOUBLADE", "Doublade", ROLES, {"evolve": ["SpAttack"]}),          # user's picks, 2026-10-31
    ("AEGISLASH_SHIELD", "Aegislash(Shield)", ROLES, {"evolve": ["Rotate"]}),
    ("AEGISLASH_SWORD", "Aegislash(Sword)", ["idle", "play"], {}),     # only shown while playing
]


def sheet(zip_name):
    z = zipfile.ZipFile(SRC / f"{zip_name}.zip")
    anims = {a.findtext("Name"): a for a in ET.fromstring(z.read("AnimData.xml")).iter("Anim")}
    return z, anims


def frames(z, anims, name):   # row 0 of an animation: list of RGBA frames + durations (ms) + frame size
    a = anims[name]
    while a.find("CopyOf") is not None:
        a = anims[a.findtext("CopyOf")]
    fw, fh = int(a.findtext("FrameWidth")), int(a.findtext("FrameHeight"))
    im = Image.open(io.BytesIO(z.read(a.findtext("Name") + "-Anim.png"))).convert("RGBA")
    fr = [im.crop((i * fw, 0, i * fw + fw, fh)) for i in range(im.width // fw)]
    ms = [round(int(d.text) * 1000 / 60) for d in a.iter("Duration")]   # PMD durations are 1/60 s ticks
    return fr, ms, fw, fh


def visible(p):
    return p[3] >= 128


def build_form(cname, zip_name, roles, picks):
    zn, an = sheet(zip_name)
    zs, as_ = sheet(zip_name + "(Shiny)")
    out, colours, shiny_votes = {}, set(), defaultdict(Counter)
    for role in roles:
        tries = picks.get(role, PICK[role])
        name = next((n for n in tries if n in an), None)
        if name is None:
            if role in OPTIONAL:
                continue
            sys.exit(f"{zip_name}: no animation for '{role}' (tried {tries})")
        fr, ms, fw, fh = frames(zn, an, name)
        frs, _, _, _ = frames(zs, as_, name)
        boxes = [f.getbbox() for f in fr if f.getbbox()]
        u = (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))
        lift = None
        if role in JUMPY:   # same body every frame, only moved up: crop up from each frame's feet, keep the lift
            bh = max(f.getbbox()[3] - f.getbbox()[1] for f in fr)
            feet = [f.getbbox()[3] for f in fr]
            rest = max(feet)
            u = (u[0], rest - bh, u[2], rest)
            crops = [f.crop((u[0], b - bh, u[2], b)) for f, b in zip(fr, feet)]
            scrops = [f.crop((u[0], b - bh, u[2], b)) for f, b in zip(frs, feet)]
            lift = [rest - b for b in feet]
        else:
            crops, scrops = [f.crop(u) for f in fr], [f.crop(u) for f in frs]
        for c, s in zip(crops, scrops):
            for p, q in zip(c.get_flattened_data(), s.get_flattened_data()):
                if visible(p):
                    colours.add(p[:3])
                    shiny_votes[p[:3]][q[:3]] += 1
        out[role] = dict(pmd=name, frames=crops, ms=ms, lift=lift, dx=u[0] - fw // 2, dy=u[1] - fh // 2,
                         w=u[2] - u[0], h=u[3] - u[1])
    if len(colours) > 15:
        sys.exit(f"{zip_name}: {len(colours)} colours, max 15 (index 0 = transparent)")
    pal = sorted(colours)
    shiny = [shiny_votes[c].most_common(1)[0][0] for c in pal]
    off = sum(sum(v.values()) - v.most_common(1)[0][1] for v in shiny_votes.values())
    tot = sum(sum(v.values()) for v in shiny_votes.values())
    ground = out["idle"]["dy"] + out["idle"]["h"]   # idle feet, relative to the frame centre
    return dict(cname=cname, zip=zip_name, roles=out, pal=pal, shiny=shiny, ground=ground,
                shiny_off=100 * off / tot)


def rgb565(c):
    return ((c[0] & 0xF8) << 8) | ((c[1] & 0xFC) << 3) | (c[2] >> 3)


def pack(img, pal):   # 4-bit, 2 pixels per byte (high nibble first), row by row; 0 = transparent
    idx = {c: i + 1 for i, c in enumerate(pal)}
    px = [idx[p[:3]] if visible(p) else 0 for p in img.get_flattened_data()]
    if len(px) % 2:
        px.append(0)
    return bytes((px[i] << 4) | px[i + 1] for i in range(0, len(px), 2))


def header(forms):
    L = ["// GENERATED by kalos-dex/tools/sprite_convert.py from kalos-dex/assets/sprites-src/*.zip – do not edit, do not read",
         "// (data only). Sprites: PMD Sprite Collab, credits in kalos-dex/assets/sprites-src/CREDITS.md. Personal use only:",
         "// git-ignored, never published. Frames: 4-bit, 2 px per byte, high nibble first, index 0 = transparent.",
         "#pragma once", "#include <stdint.h>", "",
         "struct SprAnim { uint8_t w, h; int8_t dx, dy; uint8_t count; const uint8_t *px; const uint16_t *ms; const uint8_t *lift; };",
         "// lift (jumps only, else nullptr): source px the body is raised in each frame, drawn at SPR_LIFT_SCALE screen px each",
         f"#define SPR_LIFT_SCALE {LIFT_SCALE}f",
         "// dx/dy: top-left of the box relative to the PMD frame centre; ground = idle feet relative to that centre",
         "// evolve = optional show-off move after evolving into this form (nullptr: the firmware uses play)",
         "struct SprForm { const uint16_t *pal, *shiny; int8_t ground; const SprAnim *idle, *sleep, *play, *hurt, *feed, *evolve; };", ""]
    total = 0
    for f in forms:
        n = f["cname"].lower()
        L.append(f"// ---- {f['zip']} (shiny = palette swap, {f['shiny_off']:.1f} % of pixels approximated)")
        L.append(f"static const uint16_t {n}_pal[16] = {{0, " + ", ".join(f"0x{rgb565(c):04X}" for c in f["pal"])
                 + ", 0" * (15 - len(f["pal"])) + "};")
        L.append(f"static const uint16_t {n}_shiny[16] = {{0, " + ", ".join(f"0x{rgb565(c):04X}" for c in f["shiny"])
                 + ", 0" * (15 - len(f["shiny"])) + "};")
        for role, a in f["roles"].items():
            data = b"".join(pack(fr, f["pal"]) for fr in a["frames"])
            total += len(data)
            L.append(f"// {role} = PMD '{a['pmd']}': {len(a['frames'])} frames {a['w']}x{a['h']}")
            L.append(f"static const uint16_t {n}_{role}_ms[] = {{{', '.join(map(str, a['ms']))}}};")
            if a["lift"]:
                L.append(f"static const uint8_t {n}_{role}_lift[] = {{{', '.join(map(str, a['lift']))}}};")
            L.append(f"static const uint8_t {n}_{role}_px[{len(data)}] = {{")
            for i in range(0, len(data), 32):
                L.append("  " + ",".join(f"0x{b:02X}" for b in data[i:i + 32]) + ",")
            L.append("};")
            L.append(f"static const SprAnim {n}_{role} = {{{a['w']}, {a['h']}, {a['dx']}, {a['dy']}, "
                     f"{len(a['frames'])}, {n}_{role}_px, {n}_{role}_ms, "
                     + (f"{n}_{role}_lift" if a["lift"] else "nullptr") + "};")
        ptr = lambda r: f"&{n}_{r}" if r in f["roles"] else "nullptr"
        L.append(f"static const SprForm {f['cname']} = {{{n}_pal, {n}_shiny, {f['ground']}, "
                 + ", ".join(ptr(r) for r in ROLES) + "};")
        L.append("")
    L.append(f"// frame data: {total} bytes")
    return "\n".join(L) + "\n", total


def unpack(data, w, h, pal):   # decode what the firmware gets (proves the packing round-trips)
    im = Image.new("RGBA", (w, h))
    px = []
    for b in data:
        px += [b >> 4, b & 15]
    im.putdata([(0, 0, 0, 0) if i == 0 else pal[i - 1] + (255,) for i in px[:w * h]])
    return im


def previews(forms):
    PREVIEW.mkdir(parents=True, exist_ok=True)
    bg, warn = (232, 232, 222), []
    still = Image.new("RGB", (len(forms) * (BOX_W + 8), 2 * (BOX_H + 8)), (40, 44, 52))
    for k, f in enumerate(forms):
        gif, durs = [], []
        for shiny in (False, True):
            pal = f["shiny"] if shiny else f["pal"]
            for role, a in f["roles"].items():
                loops = 2 if role in ("idle", "sleep") else 1
                for _ in range(loops):
                    for i, fr in enumerate(a["frames"]):
                        img = unpack(pack(fr, f["pal"]), a["w"], a["h"], pal)
                        img = img.resize((a["w"] * SCALE, a["h"] * SCALE), Image.NEAREST)
                        x = BOX_W // 2 + a["dx"] * SCALE
                        y = GROUND + (a["dy"] - f["ground"]) * SCALE - (round(a["lift"][i] * LIFT_SCALE) if a["lift"] else 0)
                        if x < 0 or y < 0 or x + img.width > BOX_W or y + img.height > BOX_H:
                            warn.append(f"{f['zip']} {role}: {img.width}x{img.height} at ({x},{y}) leaves the {BOX_W}x{BOX_H} box")
                        box = Image.new("RGBA", (BOX_W, BOX_H), bg + (255,))
                        box.alpha_composite(img, (max(0, x), max(0, y)))
                        gif.append(box.convert("RGB")); durs.append(max(20, a["ms"][i]))
                        if role == "idle" and i == 0:
                            still.paste(box.convert("RGB"), (k * (BOX_W + 8) + 4, (BOX_H + 8) * shiny + 4))
        gif[0].save(PREVIEW / f"sprite_{f['cname'].lower()}.gif", save_all=True, append_images=gif[1:],
                    duration=durs, loop=0)
    still.save(PREVIEW / "sprites_idle_box.png")
    return sorted(set(warn))


def main():
    forms = [build_form(*f) for f in FORMS]
    text, total = header(forms)
    if "--check" in sys.argv:
        same = OUT.exists() and OUT.read_text() == text
        print(f"sprites {'fresh' if same else 'STALE – run sprite_convert.py'}")
        sys.exit(0 if same else 1)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text)
    for f in forms:
        print(f"{f['zip']:<20} {len(f['pal']):>2} colours  shiny approx {f['shiny_off']:.1f} %  "
              + "  ".join(f"{r}:{a['pmd']} {len(a['frames'])}f {a['w']}x{a['h']}" for r, a in f["roles"].items()))
    print(f"frame data {total / 1024:.1f} KB -> {OUT.relative_to(KD.parent)}")
    for w in previews(forms):
        print("WARN", w)
    print(f"previews -> {PREVIEW.relative_to(KD.parent)}/sprite_*.gif, sprites_idle_box.png")


if __name__ == "__main__":
    main()
