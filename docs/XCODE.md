# Xcode and visual city interface

Open `CanterburyDetails.xcodeproj` and choose the shared **Canterbury Details**
scheme, **My Mac**, then Run (Command-R). The checked-in project uses native C and
Objective-C build phases; it is not an external Make target. Debug and Release
have the same city profile, local sources, public header guard and Cocoa entry
point as the command-line build. No development team/account is required for
local unsigned builds. macOS 14+ is the deployment target.

The shared **Canterbury Details CLI** and **Canterbury Details Smoke** schemes
provide a console host and the existing native smoke fixture. CLI Run arguments
use the 32-person test fixture; the visual app always defaults to 27,000 people.
The full behavioral suite remains `make test`; the Smoke scheme is not an XCTest
bundle, and Command-U is not a replacement for that suite.

## Renderer and resources

The map is produced by compiling and executing the **exact upstream
`canterbury/newcanterbury/json2png.c`**, with its `map.h`, original vector JSON,
`--text` and the macOS Times New Roman Italic font. This is the same renderer
pipeline used for the earlier simulation. There is no dependency on that sibling
application or its files. Background/foreground passes, even-odd polygons, grey
orchard fields, full source palette and all 89 labels come from the renderer.
The map is not a screenshot of the old game or a substitute modern basemap.

Build-time requirements are Xcode, Python 3, pkg-config, libpng and FreeType.
On macOS with Homebrew these libraries are normally installed with:

```sh
brew install pkg-config libpng freetype
```

No dependency installation/download occurs during a build. The renderer uses
`/System/Library/Fonts/Supplemental/Times New Roman Italic.ttf`, whose bytes are
hashed with source files, library/compiler versions and the render settings.
Missing dependencies produce a named build error. Xcode searches the standard
Apple Silicon and Intel Homebrew binary directories for Python/pkg-config.
`CANTERBURY_PYTHON` can select an installed Python executable in build settings.

The cached rendered map and manifest are `build/map/CanterburyMap.png` and
`build/map/render.json`. `tools/build_basemap.py --write` generates them;
`--check` validates the cache. The Xcode app pre-build phase audits the pinned
submodules/profile and renders when needed, then the Resources phase bundles the
PNG plus original institution and modern building-candidate JSON. The ordinary
`make app` path bundles the same resources. The finished app launches independently
of the checkout and Homebrew: libpng/FreeType are used by the build-time helper,
not linked into the application. AppKit decodes the bundled PNG.

Source and asset-license limitations in `docs/SOURCES.md` still apply to any
redistribution. OSM-derived annotations retain their ODbL attribution/uncertainty;
rendering does not resolve historical-asset rights questions.

## Visual controls

- Run/Pause controls the sole native clock. Speed selects 1, 5 or 20 native
  minutes per presentation pulse. Step 1/10 pauses and advances exactly that
  number. The scheduler calls the existing native step API; no second simulation
  clock or catch-up integration is introduced. Presentation/selection does not
  consume minutes or RNG.
- Restart 1940 uses prepared allocation/publication and creates a fresh paused
  27,000-person session at the documented initial binding minute, 00:01.
- Drag or ordinary scroll pans. Pinch, Option-scroll, +/− and toolbar buttons zoom.
  Fit map and City centre restore useful views. Arrow keys pan when the map has
  focus; Space toggles Run/Pause and F fits. View menu shortcuts are also present.
- Institution markers and building-footprint outlines can be toggled. Click a
  marker or a footprint (even-odd rings preserve holes), or choose a sidebar row,
  to inspect the source record. Every located church selection explicitly recenters
  the viewport, including clicks on an already selected row at unchanged zoom. Sidebar search covers institutions, modern
  building suggestions and resident IDs. Selecting a located institution or
  building focuses the map; five unlocated institutions stay unlocated.
- The resident register uses live world-scoped native person references. Names, civil dates and households
  are explicit synthetic fixtures, and the inspector shows their basis. Source
  housing and spatial positions remain unknown until admitted. Jobs and human
  routines remain later cycles; the map draws no invented resident positions.

Modern building names/uses are labeled suggestions. Institution locations require
review and the Old Synagogue scenario override retains its historical note.
Religion is institution/community information only. Cycles 09–13 implement ordinary doors/access and native physical navigation with
qualified fixtures. Visual picking is not approval for physical movement: real
source links, capacities and entrances still require per-feature review. Human
routines remain later cycles.

## Project maintenance and verification

`tools/native_sources.py` is the shared Make/Xcode source and resource definition.
After adding/removing native source files, regenerate the deterministic project:

```sh
python3 tools/generate_xcode.py --write
python3 tools/generate_xcode.py --check
xcodebuild -project CanterburyDetails.xcodeproj -scheme 'Canterbury Details' \
  -configuration Debug -derivedDataPath build/Xcode build
python3 tools/verify_xcode.py --run
```

All generated build products remain ignored under `build/`; per-user Xcode state
is ignored. Shared schemes and the project file are versioned. No global Xcode
settings, signing identities or submodule revisions are changed. The app script
phase runs without the Xcode user-script sandbox because the offline source audit
must inspect Git metadata and all audited inputs; it does not modify source files.
Verification covers Debug/Release app builds, CLI/smoke builds and execution,
profile rejection, resource parity and system-only app linkage. Native visual
interaction evidence is recorded separately in `content/native-ui-validation.json`.

Cycles 09–13 add `CanterburyCity.bin` to both build systems. App preflight builds
the runtime pack from the current audited Cycle 08 artifact and per-feature review
ledger. `CitySession` performs native initialization, stepping, inspection and
replacement on one serial queue; AppKit receives copied states only. See
`UI-THREADING.md` for the responsiveness and shutdown contract.

The **Agents (+)** checkbox displays known native map positions. **All agents…**
opens a fit/zoom roster grid with one plus sign per agent, including those without
map positions. Its legend explicitly says that grid cells are not city locations.
Click a glyph to inspect the corresponding resident. The population is present
and ticking now; daily needs and employment autonomy are implemented in Cycles
15–20, rather than inferred from a moving-dot display.

Cycles 14–18 add live needs fields to resident inspection and an **Occupation
catalogue** mode with enabled/disabled role definitions, qualifications, outputs
and source basis. These definitions do not assign jobs or religious affiliation.
Every target generates the same audited `OccupationCatalogue.inc` before compile.
The serial worker refreshes resident-name caches after any admission/retirement,
including equal-count churn, and glyphs continue to represent only live references.
