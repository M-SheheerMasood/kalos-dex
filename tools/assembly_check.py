"""Kalos Dex – ASSEMBLY PATH CHECK.
Not just "do the finished parts overlap" – can every part actually get INTO its place?
Each part is moved out along the way it goes in (reverse direction), in small steps, while
everything that is already installed at that moment stays put. Any overlap on the way = the
part can't be installed that way.

Input: parts exported in assembly position (kalos_case.scad part="asm_*") in folder ASM.
Run:   python assembly_check.py <ASM folder> <scad/stl folder>
"""
import sys, os
import numpy as np, trimesh, manifold3d

ASM, STL = sys.argv[1], sys.argv[2]
W, MID_Z0, MID_Z1 = 83.6, 46.1 * 1.1, 46.1 * 1.1 + 80 * 1.1

def M(mesh):
    return manifold3d.Manifold(manifold3d.Mesh(vert_properties=np.asarray(mesh.vertices, dtype=np.float32),
                                               tri_verts=np.asarray(mesh.faces, dtype=np.uint32)))
def load(name): return M(trimesh.load(os.path.join(ASM, name + '.stl')))

P = {n: load(n) for n in ['tub', 'back_lid', 'screen_ring', 'screen_face', 'clamps', 'display', 'retainers',
                          'arch_fills', 'strip', 'switches', 'buttoncaps', 'leds', 'ky', 'lid_with_carrier',
                          'carrier_esp', 'usb_plug', 'esp_body']}

# ---- cap sets (top + bottom) from the split print files, put back in assembly position
def cap_piece(fname, unrotate=False):
    m = trimesh.load(os.path.join(STL, fname))
    if unrotate:   # the rim is exported lying on its back: rotate back and seat at y 0, z 0
        m.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
        m.apply_translation([0, -m.bounds[0, 1], -m.bounds[0, 2]])
    return m
cap, rim = cap_piece('_build/cap_asm.stl'), cap_piece('_build/rim_asm.stl')   # assembly-position copies
band = trimesh.load(os.path.join(STL, '_build/half_band.stl'))
def top(m): t = m.copy(); t.apply_translation([0, 0, MID_Z1]); return M(t)
def bottom(m):
    t = m.copy(); t.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [0, 1, 0]))
    t.apply_translation([W, 0, MID_Z0]); return M(t)
for nm, m in (('cap', cap), ('rim', rim), ('band', band)):
    P[nm + '_top'], P[nm + '_bot'] = top(m), bottom(m)
# arch fills: one export holds both; split by height
af = trimesh.load(os.path.join(ASM, 'arch_fills.stl')).split(only_watertight=False)
af = sorted(af, key=lambda m: m.bounds[0, 2])
P['archfill_bot'], P['archfill_top'] = M(af[0]), M(af[1])

def union(names):
    out = None
    for n in names: out = P[n] if out is None else out + P[n]
    return out

def sweep_path(label, moving, fixed, segments, step=0.5, tol=0.5):
    """segments: list of (direction, distance) done one after the other"""
    mv = union(moving); fx = union(fixed); pos = np.zeros(3); worst = (0.0, '')
    for direction, dist in segments:
        d = np.array(direction, float); d /= np.linalg.norm(d)
        for s in np.arange(0, dist + step, step):
            v = (mv.translate(list(pos + d * s)) ^ fx).volume()
            if v > worst[0]: worst = (v, f'{direction} at {s:.1f} mm')
        pos = pos + d * dist
    ok = worst[0] < tol
    print(f"{'OK ' if ok else 'XX '} {label:62s}{'' if ok else ' hit ' + worst[1] + f' ({worst[0]:.1f} mm3)'}")
    return ok

def sweep(label, moving, fixed, direction, dist, step=0.5, tol=0.5):
    mv = union(moving); fx = union(fixed)
    d = np.array(direction, float); d /= np.linalg.norm(d)
    worst = (0.0, None)
    for s in np.arange(0, dist + step, step):
        v = (mv.translate(list(d * s)) ^ fx).volume()
        if v > worst[0]: worst = (v, s)
    ok = worst[0] < tol
    who = ''
    if not ok:  # which fixed part is in the way?
        hits = []
        for n in fixed:
            v = max((mv.translate(list(d * s)) ^ P[n]).volume() for s in np.arange(0, dist + step, step))
            if v > tol: hits.append(f'{n} ({v:.1f} mm3)')
        who = ' – blocked by ' + ', '.join(hits)
    print(f"{'OK ' if ok else 'XX '} {label:62s}{'' if ok else f' first hit at {worst[1]:.1f} mm' + who}")
    return ok

FRONT = ['screen_ring', 'screen_face', 'display', 'clamps', 'strip', 'cap_top', 'cap_bot', 'rim_top', 'rim_bot',
         'band_top', 'band_bot', 'archfill_top', 'archfill_bot']
ROOF = ['switches', 'retainers', 'buttoncaps', 'leds', 'ky']
results = []
print('== cap sets (built on the bench before gluing) ==')
for t in ('top', 'bot'):
    results.append(sweep(f'{t}: arch fill off the back of the cap (+Y)', [f'archfill_{t}'], [f'cap_{t}', f'rim_{t}', f'band_{t}'], (0, 1, 0), 15))
    results.append(sweep(f'{t}: rim slides out of the cap to the back (+Y)', [f'rim_{t}'], [f'cap_{t}', f'band_{t}'], (0, 1, 0), 15))
    results.append(sweep(f'{t}: band out of the cap to the back (+Y), rim already in', [f'band_{t}'], [f'cap_{t}', f'rim_{t}'], (0, 1, 0), 15))
    results.append(sweep(f'{t}: band out to the back (+Y), rim NOT in yet', [f'band_{t}'], [f'cap_{t}'], (0, 1, 0), 15))
print('== roof (from the open back, before the lid) ==')
results.append(sweep('button caps up out of the roof holes (+Z)', ['buttoncaps'], ['tub', 'switches', 'retainers', 'leds', 'ky'], (0, 0, 1), 12))
results.append(sweep_path('retainer plates: down 6 mm, then out the back', ['retainers'], ['tub', 'leds', 'ky'], [((0, 0, -1), 6), ((0, 1, 0), 60)]))
results.append(sweep('switches down out of their pockets (-Z)', ['switches'], ['tub', 'leds', 'ky'], (0, 0, -1), 15))
results.append(sweep('LEDs down out of their holes (-Z)', ['leds'], ['tub', 'switches', 'retainers', 'ky'], (0, 0, -1), 20))
results.append(sweep('KY-040 down out of the roof (-Z), nut off', ['ky'], ['tub', 'switches', 'retainers', 'leds'], (0, 0, -1), 20))
print('== front (screen zone) ==')
results.append(sweep('display clamp bars off to the back (+Y)', ['clamps'], ['tub', 'screen_ring', 'screen_face', 'display', 'strip'] + ROOF, (0, 1, 0), 45))
results.append(sweep('display off the face to the back (+Y)', ['display'], ['tub', 'screen_ring', 'screen_face', 'strip', 'archfill_top', 'archfill_bot', 'cap_top', 'cap_bot'] + ROOF, (0, 1, 0), 45))
results.append(sweep('face out of the ring to the back (+Y) – on the bench', ['screen_face'], ['screen_ring'], (0, 1, 0), 25))
results.append(sweep('ring + face out of the tub to the front (-Y) – BEFORE the caps', ['screen_ring', 'screen_face'], ['tub'] + ROOF, (0, -1, 0), 30))
results.append(sweep('display (then clamps) in from the back after the ring is in', ['display'], ['tub', 'screen_ring', 'screen_face', 'cap_top', 'cap_bot', 'rim_top', 'rim_bot', 'band_top', 'band_bot', 'archfill_top', 'archfill_bot', 'strip'] + ROOF, (0, 1, 0), 50))
for t in ('top', 'bot'):
    results.append(sweep(f'{t} cap set out of the tub to the front (-Y)', [f'cap_{t}', f'rim_{t}', f'band_{t}', f'archfill_{t}'], ['tub', 'screen_ring', 'screen_face', 'display'] + ROOF, (0, -1, 0), 30))
print('== back lid ==')
results.append(sweep('carrier + ESP32 slide out of the lid rails to the left (-X)', ['carrier_esp'], ['back_lid'], (-1, 0, 0), 80))
results.append(sweep('lid with carrier + ESP32 + RTC out of the tub to the back (+Y)', ['lid_with_carrier'], ['tub'] + FRONT + ROOF, (0, 1, 0), 70))
results.append(sweep('USB plug out through the right wall (+X)', ['usb_plug'], ['tub', 'esp_body', 'back_lid'], (1, 0, 0), 50))
print(f"\n{sum(results)} / {len(results)} paths clear")
