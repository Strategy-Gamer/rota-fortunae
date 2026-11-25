"""Writes provinces_locations.txt and provinces_straits.txt from the EU4 map data.
Lakes count as sea; canals are skipped (no canals at the start date).
Terrain: EU4's province override, else the commonest land terrain in terrain.bmp.
Scale: metres per pixel in x and y at each centroid, from a local fit to known places,
because the EU4 map has no consistent projection."""
import csv, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
from collections import Counter
from known_places import KNOWN
from eu4_projection import local_fit, metres_per_pixel

M, OUT = sys.argv[1], sys.argv[2]
SIGMA = 400  # pixels; best leave-one-out error over the anchors

strip = lambda t: re.sub(r'#[^\n]*', '', t)
def block(text, key):
    m = re.search(key + r'\s*=\s*\{([^}]*)\}', text)
    return {int(x) for x in m.group(1).split()}

dm = strip(open(M + '/default.map', encoding='latin-1').read())
sea = block(dm, 'sea_starts') | block(dm, 'lakes')
impassable = block(strip(open(M + '/climate.txt', encoding='latin-1').read()), 'impassable')

# terrain.txt: categories (with province overrides) and terrain.bmp index -> category
tt = strip(open(M + '/terrain.txt', encoding='latin-1').read())
cats_text = tt[tt.index('categories'):tt.index('\nterrain = {')]
override = {}
depth, name, start = 0, None, 0
for m in re.finditer(r'(\w+)\s*=\s*\{|\{|\}', cats_text[cats_text.index('{') + 1:]):
    if m.group(0).endswith('{'):
        if depth == 0: name, start = m.group(1), m.end()
        depth += 1
    else:
        depth -= 1
        if depth == 0:
            body = cats_text[cats_text.index('{') + 1:][start:m.start()]
            ov = re.search(r'terrain_override\s*=\s*\{([^}]*)\}', body)
            for pid in (ov.group(1).split() if ov else []): override[int(pid)] = name
        if depth < 0: break
index_type = {int(c): t for t, c in re.findall(r'type\s*=\s*(\w+)\s+color\s*=\s*\{\s*(\d+)\s*\}', tt[tt.index('\nterrain = {'):])}
WATER = {'ocean', 'inland_ocean'}

px = np.asarray(Image.open(OUT + '/provinces.png').convert('RGB'))
packed = (px[..., 0].astype(np.int64) << 16) | (px[..., 1].astype(np.int64) << 8) | px[..., 2]
terrain_px = np.asarray(Image.open(M + '/terrain.bmp'))
assert terrain_px.shape == packed.shape

order = np.argsort(packed, axis=None)
flat = packed.ravel()[order]
starts = np.flatnonzero(np.r_[True, flat[1:] != flat[:-1]])
pixels_of = {int(flat[s]): order[s:e] for s, e in zip(starts, np.r_[starts[1:], flat.size])}

anchors, anchor_ll = [], []
color_of, rows, name_color = {}, [], {}
defs = []
with open(M + '/definition.csv', encoding='latin-1') as f:
    r = csv.reader(f, delimiter=';'); next(r)
    for row in r:
        c = (int(row[1]) << 16) | (int(row[2]) << 8) | int(row[3])
        if c in pixels_of: defs.append((int(row[0]), c, row[4]))
for pid, c, name in defs:
    name_color.setdefault(name, c)
for name, ll in KNOWN.items():
    if name in name_color:
        idx = pixels_of[name_color[name]]
        anchors.append(((idx % packed.shape[1]).mean(), (idx // packed.shape[1]).mean())); anchor_ll.append(ll)
anchors, anchor_ll = np.array(anchors), np.array(anchor_ll)

for pid, c, name in sorted(defs):
    idx = pixels_of[c]
    kind = 'sea' if pid in sea else 'impassable' if pid in impassable else 'land'
    terrain = ''
    if kind == 'land':
        terrain = override.get(pid)
        if terrain is None:
            counts = Counter(index_type.get(int(i), 'grasslands') for i in terrain_px.ravel()[idx])
            land = [(n, t) for t, n in counts.items() if t not in WATER]
            terrain = max(land)[1] if land else 'coastline'
    q = ((idx % packed.shape[1]).mean(), (idx // packed.shape[1]).mean())
    mx, my = metres_per_pixel(local_fit(q, anchors, anchor_ll, SIGMA))
    assert ';' not in name
    rows.append(f'#{c:06x};{name};{kind};{terrain};{round(mx)};{round(my)}')
    color_of[pid] = f'#{c:06x}'

with open(OUT + '/provinces_locations.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('color;name;kind;terrain;metres_per_pixel_x;metres_per_pixel_y\n')
    f.write('\n'.join(rows) + '\n')

straits = []
with open(M + '/adjacencies.csv', encoding='latin-1') as f:
    r = csv.reader(f, delimiter=';'); next(r)
    for row in r:
        if len(row) < 3 or row[2] not in ('sea', 'lake'): continue
        a, b = int(row[0]), int(row[1])
        if a in color_of and b in color_of:
            straits.append(f'{color_of[a]};{color_of[b]};{row[8] if len(row) > 8 else ""}')
with open(OUT + '/provinces_straits.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('from;to;name\n' + '\n'.join(straits) + '\n')
kinds = Counter(r.split(';')[2] for r in rows)
print(len(rows), 'locations', dict(kinds), len(straits), 'straits', len(anchors), 'anchors')
print(Counter(r.split(';')[3] for r in rows if r.split(';')[2] == 'land').most_common())
