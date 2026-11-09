# Final assembled Canterbury map

Cycle 33 uses the agreed final `canterbury/simulation-map/derived/overview.png`
as its rendering reference. The earlier full-color `newcanterbury/canterbury.json`
is not the native app's display input. The final map combines 3,341 source-derived
building footprints, waterway and rail linework, approximate reference rail
corridors and the cropped, connected OpenStreetMap road network.

`tools/json2png2.c` includes the unmodified upstream `json2png.c` core for strict
JSON parsing, even-odd polygon filling, pixel composition and PNG output. Its
assembled-layer stroke pass reproduces the pinned `complete_roads.py::render_png`
sampling and draw order. `tools/assembled_map.py` takes geometry from the derived
JSON layers and styling/order from upstream `osm_roads.py`; no road is invented
or connected by proximity. Rendering uses 3,341 polygons and 16,471 line records.
The restrained palette uses cream ground, dark buildings, red/brown roads, blue
water and purple rail ink. It excludes the earlier green woodland, orange contour
and grey orchard background fills, and does not reintroduce old raster labels.

Every build checks decoded RGB pixels against the agreed final overview, failing
before publication if any pixel differs. The resulting libpng PNG SHA-256 is
`9c966e034f05d2be66a8a1e1e2271fa0c86cce6a3d3f4e3215ac058ffa1fe257`.
PNG compression differs from the upstream Python writer; all 1,600 × 1,600 RGB
pixels match exactly. The comparison uses the pinned upstream PNG decoder and
records all geometry, renderer, style and reference hashes in `build/map/render.json`.

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
