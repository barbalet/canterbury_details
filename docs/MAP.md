# Final assembled Canterbury map

The app uses the agreed final assembled layers: 3,341 source building records,
waterway and rail linework, reference rail corridors and cropped, connected OSM
roads. The earlier full-colour source-vector image is not the display input.

The street-clearance correction preserves the pinned upstream files. The source
map had 1,094 building footprints crossed by street/path centre lines and 1,117
footprints sharing painted road pixels. The source-locked project overlay in
`content/building-placement-corrections.json` corrects 1,483 footprints, including
near-edge clearance: 1,401 are translated intact to nearby free space; 82 merged
block footprints are separated along street corridors. Intact translations do not
rotate or resize buildings. Merged blocks retain their original source IDs and
original area metadata. These are schematic alignment corrections, not newly
verified historical positions or identities.

`tools/building_placements.py` applies the same corrections to rendered polygons,
the compiled JSON, selectable bounds and runtime geometry. The native app
packages `build/map/buildings-enriched.json`, generated with that same overlay,
for outlines and picking; it never packages the uncorrected enrichment layer. No road is rerouted or
joined by proximity. The audit checks every building and painted street pixel:
zero road/building overlaps, zero building/building overlaps, no new rail
obstructions or increased water overlap, valid on-map polygons and unchanged source identities. All 1,483 cases
have before/after views in `build/map/building-conflicts.html`.

`tools/json2png2.c` retains the unmodified upstream json2png core. Every regenerated
map is compared pixel-for-pixel with an independent Python rendering of the same
corrected layers using the pinned upstream renderer. The original upstream
`derived/overview.png` remains the before image; the corrected map intentionally
differs from it. Renderer, geometry, overlay and PNG hashes are recorded in
`build/map/render.json` and `content/building-placement-validation.json`.

## Conservative surviving structures and smooth display

The overlay also contains an explicit, source-locked structure review. Only one
OSM outline is admitted: Hothe Court (`way/123811376`). Its historical footprint
overlaps the same named structure, and the [Historic England account](https://historicengland.org.uk/listing/the-list/list-entry/1336585)
describes the surviving sixteenth-century-or-earlier T-shaped house and older wings.
This is a modern geometry proxy, not a certified 1940 survey. No modern surrounding
buildings are imported.
Geometric overlap or the existing proximity-based enrichment alone is insufficient.
Old Sessions House is excluded because a [council planning report](https://www.canterbury.gov.uk/sites/default/files/2024-04/Planning%20Committee%20agenda%202%20April%202024.pdf) documents its large 1990s extension. St Martin’s and St Dunstan’s church outlines are rejected because their nearest historical candidates are labelled Priory and Church Hall. The Castle Street multi-storey car park, known 2015 buildings and the surviving
St George tower are explicitly excluded. The Cathedral is retained from the
historical layer because its registered modern outline cannot clear the mapped
paths locally without distortion. All other uncertain identities retain historical
geometry.

2,934 historical contours receive topology-preserving simplification of at most
0.7 source pixels, with tightly bounded area change and no new obstructions.
The structure review generator is `tools/review_building_structures.py` (Shapely,
review-only candidate output); normal builds consume the admitted data offline.

The native view fills cached vector building paths with antialiasing over
`CanterburyMapBackground.png`, rendered by the same json2png2 core with buildings
omitted. Outlines, selection and picking share those exact paths. The full
`CanterburyMap.png` remains the standalone composite and independent raster
reference. High-quality interpolation remains enabled at every zoom; zooming
neither rebuilds geometry nor rerenders the basemap nor advances the simulation.

## Build and use json2png2

```
make json2png2
build/map/json2png2 build/map/AssembledMap.json /tmp/CanterburyMap.png
python3 tools/build_basemap.py --check
```

The build retains the standalone executable and assembled JSON beside the PNG.
To export a new render input from the pinned assembled data:

```
python3 tools/assembled_map.py --write build/map/AssembledMap.json
```

The new C renderer expects `canterbury-assembled-render` version 1: width/height,
RGB palette and ordered features containing either even-odd `rings`, or line
`coordinates` plus an integer stroke `radius` (0 or 1). Colors index the palette.
It validates canvas, points, palette and stroke bounds. Compile it directly with:

```
cc -std=c11 -O2 tools/json2png2.c $(pkg-config --cflags --libs libpng freetype2) -lm -o /tmp/json2png2
```

## Model and presentation separation

The map remains a 1,600 × 1,600 top-left-origin source-pixel canvas. Model coordinates
are source pixels; magnification and pan transform only display. Footprints,
institution candidates, known-position people and selection halos are independent
overlays. Unplaced people have no map marker. The All agents roster is explicitly
nongeographic. Copied data and the cached PNG are drawn without native engine calls.

Connected OSM geometry is a visual/source topology input, not a claim of verified
1940 street identity or automatic operational approval. The shipped content still
has zero qualified street edges/entrances, unknown occupancy/staffing and 27,000
spatially unplaced initial people. Rail is reference geometry. Qualified test
streets remain labeled technical fixtures. Rendering, camera and inspection do
not consume native time/RNG or mutate authoritative state. OSM-derived data is
© OpenStreetMap contributors, ODbL-1.0; original source limitations still apply.

## Reproduce and review street clearance

```
python3 tools/verify_building_placements.py
python3 tools/verify_building_placements.py --check
```

The normal audit and all builds use Python's standard library. An independent
continuous-geometry audit is available with `--geometry` in an environment with
Shapely. `tools/review_building_placements.py` is the deterministic review solver
and requires Shapely; it writes only a candidate under `build/map/`. Admitted
corrections are explicit project data with exact upstream input hashes. Any
upstream source change rejects the overlay until it is reviewed again.

Building cards label these footprint corrections as schematic. Original
institution candidate locations are retained as uncertain source annotations.
Old checkpoints from another content generation are rejected by the existing
strict compatibility gate; no saved world is silently relocated or migrated.

## Canterbury submodule ownership

The reusable correction review, geometry loader, review generators, standalone
C renderer, SVG/PNG builder and exhaustive geometry audit now live in the
`canterbury` submodule. From that repository root:

```
python3 simulation-map/render_map.py
python3 simulation-map/verify_placements.py --geometry  # optional Shapely audit
```

Outputs are under `simulation-map/derived/corrected/`; the raw derived source
layers remain intact. Canterbury Details delegates its geometry loader and C
renderer to those implementations. Its compiler-owned correction mirror is
checked against the canonical submodule review on every load, so divergent
copies fail explicitly. AppKit's cached vector drawing remains application code.

These submodule edits must be committed in Canterbury and its new revision pinned
in the parent before refreshing the parent's strict source/release qualification.
No source-pin or historical review gate is bypassed for an uncommitted submodule.
