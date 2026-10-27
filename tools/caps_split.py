"""Kalos Dex – split the prop caps into parts that FIT TOGETHER when printed in separate colours.

The prop STLs were made for a multi-colour printer: the white rim overlaps the red cap (74 % of
the rim's volume is inside the cap). Printed separately they can't be assembled. This script:
  1. extends the rim straight BACK from its visible front to the cut plane (flat back -> prints
     front-up without supports, and the white goes through the full thickness like the original),
  2. leaves room for the black band,
  3. cuts a matching pocket in the red cap (with glue clearance), open at the back so the rim
     slides in from behind.
Visible front surfaces stay exactly as the prop designer made them.

Input : front-half prop parts exported by OpenSCAD (caps.scad prop_front_half / black_band_front_half)
Output: scad/stl/main/red_cap_x2.stl, white_cap_rim_x2.stl (+ checks printed)
Run   : python caps_split.py <folder with half_cap.stl, half_rim.stl, half_band.stl>
"""
import sys, os
import numpy as np, trimesh, manifold3d

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'scad', 'stl', 'main')
HALF_T = 7.15          # front-half thickness = the cut plane (Y)
STEP = 0.2             # sweep step (rim is >= 1.5 mm thick everywhere, so steps leave no gaps)
GLUE = 0.15            # sideways clearance between the rim and the cap's pocket

def tom(m):
    return manifold3d.Manifold(manifold3d.Mesh(vert_properties=np.asarray(m.vertices, dtype=np.float32),
                                                tri_verts=np.asarray(m.faces, dtype=np.uint32)))
def totri(M):
    mm = M.to_mesh()
    t = trimesh.Trimesh(vertices=np.asarray(mm.vert_properties)[:, :3].astype(np.float64), faces=np.asarray(mm.tri_verts), process=False)
    return t
def clean_export(t, path):
    """gentle cleanup only (no remeshing): merge float-identical corners, drop zero-area slivers and
    zero-volume specks, close hairline holes. The volume must not change."""
    v0 = t.volume
    t = t.copy()
    t.vertices = t.vertices.astype(np.float32).astype(np.float64)
    t.merge_vertices(digits_vertex=5)
    t.update_faces(t.nondegenerate_faces(height=1e-6)); t.remove_unreferenced_vertices()
    parts = [p for p in t.split(only_watertight=False) if abs(p.volume) > 1.0]
    t = trimesh.util.concatenate(parts) if len(parts) > 1 else parts[0]
    trimesh.repair.fill_holes(t); trimesh.repair.fix_normals(t)
    assert abs(t.volume - v0) < 0.5, f'cleanup changed the volume {v0} -> {t.volume}'
    t.export(path)
    return trimesh.load(path)
def shifted(M, dx, dy, dz): return M.translate([dx, dy, dz])

cap, rim, band = [tom(trimesh.load(os.path.join(SRC, f'half_{n}.stl'))) for n in ('cap', 'rim', 'band')]
slab = manifold3d.Manifold.cube([200, HALF_T, 200]).translate([-50, 0, -50])   # Y 0 .. cut plane

# 1. rim swept straight back to the cut plane
sweep = rim
for dy in np.arange(STEP, HALF_T + STEP, STEP):
    sweep = sweep + shifted(rim, 0, dy, 0)
rim_new = (sweep ^ slab) - band
# 2./3. pocket in the cap = rim_new grown sideways by GLUE (8 directions in X-Z)
pocket = rim_new
for dx, dz in [(1, 0), (-1, 0), (0, 1), (0, -1), (0.7, 0.7), (0.7, -0.7), (-0.7, 0.7), (-0.7, -0.7)]:
    pocket = pocket + shifted(rim_new, dx * GLUE, 0, dz * GLUE)
cap_new = cap - pocket

# The rim's 2 mm band along the open edge leaves the cap starting ~2.3 mm up with only paper-thin
# slivers below (they made the cap "float" on the print bed). Make the seam a FLAT cut: everything
# of the cap below Z_SEAM goes to the rim, and the cap is printed standing on that flat face.
def area_at(Mf, z):
    t = totri(Mf); s = t.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1])
    return s.to_2D()[0].area if s is not None else 0.0
full = area_at(cap_new, 4.0)
Z_SEAM = next(z for z in np.arange(1.0, 4.0, 0.05) if area_at(cap_new, z) >= 0.95 * full)
low = manifold3d.Manifold.cube([200, 50, Z_SEAM]).translate([-50, -20, -1 + 1e-3]).translate([0, 0, 1 - 1e-3])
rim_new = rim_new + (cap_new ^ low)
cap_new = cap_new - low
print(f'flat seam between cap and rim at {Z_SEAM:.2f} mm above the open edge')

# ---- checks
def vol(M): return round(M.volume(), 2)
print('rim  :', vol(rim), '->', vol(rim_new), 'mm3')
print('cap  :', vol(cap), '->', vol(cap_new), 'mm3')
print('overlaps after split: cap/rim', vol(cap_new ^ rim_new), ' cap/band', vol(cap_new ^ band), ' rim/band', vol(rim_new ^ band))
blocked = max(vol((shifted(rim_new, 0, d, 0)) ^ cap_new) for d in np.arange(0.5, 10.5, 0.5))
print('rim slides in from the back:', 'YES' if blocked < 0.5 else f'NO ({blocked} mm3 in the way)')
# visible front unchanged? compare what the front looks like: sample rays from the front
xs, zs = np.meshgrid(np.arange(0.5, 83.5, 0.7), np.arange(0.3, 50.5, 0.7))
orig = trimesh.util.concatenate([totri(cap), totri(rim), totri(band)])
new = trimesh.util.concatenate([totri(cap_new), totri(rim_new), totri(band)])
def first_hit(mesh):
    o = np.c_[xs.ravel(), np.full(xs.size, -5.0), zs.ravel()]; d = np.tile([0, 1, 0], (len(o), 1))
    loc, idx, _ = mesh.ray.intersects_location(o, d, multiple_hits=False)
    out = np.full(len(o), np.nan); out[idx] = loc[:, 1]; return out
a, b = first_hit(orig), first_hit(new)
both = ~np.isnan(a) & ~np.isnan(b)
print('front surface depth change (max over the whole front):', round(float(np.nanmax(np.abs(a[both] - b[both]))), 3), 'mm',
      '| points where something appeared/disappeared:', int((np.isnan(a) != np.isnan(b)).sum()))

# ---- export
BUILD = os.path.join(OUT, '..', '_build')
totri(cap_new).export(os.path.join(BUILD, 'cap_asm.stl'))   # assembly position (for tools/assembly_check.py)
totri(rim_new).export(os.path.join(BUILD, 'rim_asm.stl'))
cm = totri(cap_new)                         # standing on its flat open-end face
cm.apply_translation([0, 0, -Z_SEAM])
cm = clean_export(cm, os.path.join(OUT, 'red_cap_x2.stl'))
rm = totri(rim_new)                         # lying on its flat back, rounded front UP
rm.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0]))
rm.apply_translation([0, 0, -rm.bounds[0, 2]])
rm = clean_export(rm, os.path.join(OUT, 'white_cap_rim_x2.stl'))
n = rm.face_normals; c = rm.triangles_center
bed = rm.area_faces[(n[:, 2] < -0.99) & (c[:, 2] < 0.01)].sum()
over = rm.area_faces[(n[:, 2] < -0.866) & (c[:, 2] > 0.3)].sum()
print(f'rim print: height {rm.extents[2]:.2f} mm, flat back on the bed {bed:.0f} mm2, steep overhangs {over:.0f} mm2')
print('cap  watertight', cm.is_watertight, 'bodies', len(cm.split()), '| rim watertight', rm.is_watertight, 'bodies', len(rm.split()))

cap_f, rim_f = tom(cm), tom(trimesh.load(os.path.join(OUT, 'white_cap_rim_x2.stl')))
print('repaired files: cap watertight', cm.is_watertight, 'vol', round(cm.volume, 1), '| rim watertight', rm.is_watertight, 'vol', round(rm.volume, 1))
