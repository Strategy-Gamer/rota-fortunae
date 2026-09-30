# 06 — opengs: map labels, SDF borders, LUT+palette rendering

Source: **opengs** — https://github.com/Thomas-Holtvedt/opengs (MIT, pure GDScript, Godot 4),
read 2026-08. Also a separate editor: https://github.com/Thomas-Holtvedt/opengs-maptool.
A local copy lives under `Documents/My Stuff/opengs`. See memory [[rf-opengs-reference]].

**How to use this note:** mine it for **techniques, not code**. opengs is AoS/OOP GDScript
(per-`Province`/`Country` objects, `.filter()` over arrays) — the *opposite* of RF's DoD/C++
sim, and its ~3–4 min load on a small dataset is exactly why RF is C++. Everything below is
**presentation**, so it uses float and GPU order-dependent passes freely — **no Fixed-point,
no determinism, not hashed.** Port the algorithms into RF as C++ queries feeding GDScript
renderers; do not import the object model.

---

## 1. Curved country labels  ← the thing to steal first

Two cleanly-separated files:

**`map/country_label.gd` — computes the curve** (the label's spine):
1. Gather the country's owned province **centers** (skip `NNN`/no-center; need ≥2).
2. Fit a **quadratic least-squares regression** `y = a + b·x + c·x²` over those centers,
   solved with **Cramer's rule** on the 3×3 normal-equation matrix
   (`calculate_quadratic_regression`). Returns `null` if the determinant ~0 (degenerate).
3. **Recenter** the coefficients around `mean_x` so that `c` expresses *only curvature*
   (independent of horizontal position): evaluate `y_at_mean`, `slope_at_mean`, keep `c`.
4. **Clamp curvature by a "sagitta ratio":** `max_c = 4 · MAX_SAGITTA_RATIO / width`
   (`MAX_SAGITTA_RATIO = 0.15`). Sagitta = the bow height of an arc; this caps bow ÷ width so
   sprawling countries don't get comically curled labels. If clamped, **re-anchor** using the
   linear-regression slope through the centroid (graceful fallback).
5. Sample `PATH_SAMPLES(=20)+1` points across `[x_lo, x_hi]` of the parabola → a
   `PackedVector2Array`. `<3` provinces or degenerate ⇒ plain **linear regression** 2-point line.
6. Font size auto-fills the path: `font_size = clamp(path_len / name_len · FONT_FILL_RATIO,
   MIN…)`, plus a thin outline (`~4%` of font size).

**`core/utils/curved_label.gd` — draws it** (~90 lines, a `@tool extends Path2D`):
- Builds a `Curve2D` from the points; walks it with `curve.sample_baked_with_rotation(offset)`.
- Draws glyphs **one at a time** via the low-level `TextServer`
  (`shaped_text_get_glyphs` → `font_draw_glyph` / `_outline`), advancing `offset` by each
  glyph's advance, centered via `(curve_length - text_width)/2`.
- Baseline is nudged so the glyph row's visual middle sits on the curve
  (`(ascent - descent)·0.5`). `debug_draw` polylines the baked curve.

**RF adaptation:** a C++ query `build_country_label_curve(World&, country) -> points` (regression
math is cheap; do it over province centroids you already store), handed to a GDScript Path2D
drawer like `curved_label.gd`. **Float is fine — presentation.** There's also a 3D variant
(`country_label_3d.gd`) for the height-map view.

---

## 2. Map rendering: LUT + palette indirection  ← validates DESIGN §6.5

`map/map_shaders/map2d.gdshader` (canvas_item), rendered into a SubViewport and projected onto
a **3D height-displaced mesh** (`map3d.gdshader`, toggle `height_scale` 0↔3; labels swap 2D↔3D).

The core pattern (this is RF's §6.5, proven in the wild):
- **`lookup_image`** — per-pixel `(r,g)` bytes = integer **coordinates** into a palette texture.
  Static "which province is this pixel," never rebuilt on mode change. (`filter_nearest`,
  decoded as `int(r·255+0.5)`.)
- **`color_map_image`** — the **palette**; a `MapMode` object *is* one palette texture. Change
  mode = **swap the palette**, per-pixel lookup untouched. `set_map_mode()` just rebinds.
- **3 vertical layers** packed in the palette (`num_rows = height/3`): row-band 0 = primary
  color, +1 = secondary (stripe), +2 = selection. Checkerboard/stripe =
  `mix(primary, secondary, checkers_pattern)`.
- Per-mode palettes are updated **per province** and `commit()`-ed (uploaded) — see `map.gd`
  `update_map_modes` / `commit_map_modes`. `MapHighlight` overlays highlight colors on the
  active palette (`apply/remove_highlights`).

**RF touch point:** this is precisely "C++ owns *what the palette is* (query); GDScript owns
*active-mode selection + texture*." Give the map-mode enum the single-source `BIND_ENUM_CONSTANT`
treatment (DESIGN §6.5) and generate palettes in `map_modes.cpp`.

---

## 3. Borders / selection / fade as SDF bands (GPU Jump Flooding)  ← answers the unwired-border debt

`map/map_shaders/jfa.glsl` is a **textbook Jump Flooding Algorithm** compute shader: seed each
border pixel with its own coord (SENTINEL = 0xFFFF elsewhere); for step sizes halving each pass
(`w/2, w/4, …, 1`), each pixel checks its 8 neighbors at `±step` and keeps the **nearest seed**
(squared distance). Result after `O(log n)` passes = a nearest-seed / Voronoi map, from which a
**signed-distance field** is derived. `jfa_encode.glsl` encodes it into a texture the map shader
reads (inferred from consumption: `rg` = offset vector `±128`, a channel = scalar distance
`/ DIST_MAX`). SDFs exist for province, territory, and (lower-res) country levels.

`map2d.gdshader` then uses those SDFs for three effects, all `smoothstep` distance bands:
- **Province borders:** darken (`·0.85`) where a border texture says so.
- **Selection ring:** `1 - smoothstep(thickness-.5, thickness+.5, border_dist)` from the province
  (or territory) SDF — a crisp, resolution-independent outline. Green = province, white = territory.
- **Country-interior fade:** in political/ideology modes the owner color is full-opacity near
  borders and **fades to transparent inward** (via the country SDF distance) so 3D terrain shows
  through. Independent inward/outward fade distances.

**⚠️ Bug-lesson to internalize:** don't compute `length()` on a *linearly-filtered* SDF **offset
vector** — opposing offsets along a country's medial axis cancel to ~0, faking a bright "on the
border" line down big interiors. Fix: store **scalar distance in its own channel** and read that
(their shader comment documents this the hard way).

**RF touch point:** the unwired border shader (DESIGN §9) — an SDF ring from a JFA pass gives you
borders + selection + fades from one distance texture, far nicer than a naive edge check.

---

## 4. Smaller gems
- **Localized texture updates:** on ownership change, re-rasterize only the province's **bounding
  box** (`update_map_texture(db, center, bounding_box, refresh)`) then refresh the SDF — not the
  whole map. Pairs well with RF's dirty-flag refresh.
- **Half-life exponential smoothing** (`core/utils/pva_calculator.gd`) for camera/value easing:
  `value *= exp(-ln2 · dt / half_life)` — framerate-independent, no overshoot. UI/camera only,
  keep out of sim.
- **Selection pulse:** tween the shader's `selection_thickness` param baseline↔peak for cheap
  "look here" polish (`map.gd::pulse_selection`).

## 5. File map (in the opengs repo)
- `map/country_label.gd` — regression → curve points (+ `country_label_3d.gd`).
- `core/utils/curved_label.gd` — Path2D glyph-along-curve drawer.
- `map/map_shaders/jfa.glsl`, `jfa_encode.glsl` — JFA + SDF encode (compute).
- `map/map_shaders/map2d.gdshader`, `map3d.gdshader` — the map fragment shaders.
- `map/map_texture_generator.gd`, `map_texture_sdf.gd`, `map_texture_cache.gd` — texture build/cache.
- `map/map.gd` — ties it together: textures, map modes, labels, selection, height toggle.
- `map/map_mode.gd` (palette-per-mode), `map/map_highlight.gd` (overlay highlights).
- `core/database/*` — string-def → object importers (RF does ID-registry instead).

## 6. What NOT to carry over
AoS/OOP GDScript object graph (per-entity `Province`/`Country`), string-keyed dictionaries of
objects, `.filter()` over object arrays. RF stays DoD/C++/SoA with integer IDs. Take the math and
the shader techniques; leave the data model. All of §1–4 is presentation → **float, not Fixed;
never hashed; determinism irrelevant.**
