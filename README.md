# Canterbury Details

Model ordinary human Canterbury in 1940 using the pinned `canterbury` data and
`apesdk` engine submodules. Religious information is retained for occupations,
institutions and community services. No supernatural model is part of this host.

All **40 cycles are complete**, including the offline clean-checkout release gate.
The native app starts with **27,000 ordinary humans**, **6,750 synthetic households**
and capacity for **30,000 people**. It implements native street/building movement,
configured births/admissions/retirement, needs/care, finite work/economy/development,
ordinary religious and civic services, operations, causal records and checkpoints.
Command-M opens the native management interface; the same core is available to
an independent frontend through the packaged CanterburyCore library.

The final assembled map with cropped OpenStreetMap connections is rendered by
`json2png2.c` using the upstream core. Zoom/pan and inspection stay independent of
native stepping on the serial worker. Names, households and technical rates are
explicit fixtures. Unreviewed source routes/doors, capacities, housing and initial
map positions remain unknown. Completion of the model does not establish a
historical census or supply missing historical evidence.

The standalone phase is concluded. The next phase is the separately requested
vampire simulation using this released core and its documented boundaries; no
supernatural rules are part of this ordinary city application.

```sh
git submodule update --init --recursive
make check-deps
make all
make app
make content
make content-check
make runtime-content
./build/CanterburyDetails --city build/content/CanterburyCity.bin --minutes 5
```

The qualified platform is macOS with Xcode/Apple command-line tools, Python and
Make. Map generation also needs pkg-config, libpng and FreeType. See the
[build guide](docs/BUILD.md) for dependencies, source admission and commands.
Open **CanterburyDetails.xcodeproj**, choose **Canterbury Details / My Mac**, and
press **Command-R**, or launch `build/CanterburyDetails.app` normally after building.

The app opens paused at 1940-01-01 00:01, including the documented native binding
minute. Run/Pause, simulation speed and Step controls advance the sole native
clock. Zoom, pan, search and inspection remain read-only. Selecting a located
church or resident focuses its map position. **All agents…** shows every resident
as a nongeographic roster mark: `×` is unplaced and `+` is located; color is
supplementary. Unknown positions are never invented.

**City management (Command-M)** provides all 45 ordinary operation verbs,
preview/queue/cancel/query/apply, 11 paged status categories and save/load.
Interventions require explicit permission. Complete checkpoints preserve native
and city state with strict profile/ABI/content compatibility; implicit migration
and cross-architecture raw-layout portability are not claimed.

- [User guide and management controls](docs/USER-GUIDE.md)
- [Map provenance and final assembled rendering](docs/MAP.md)
- [Live inspection and stable-ID selection](docs/INSPECTION.md)
- [Accessibility and tested input scope](docs/ACCESSIBILITY.md)
- [Source uncertainties and historical limits](docs/UNCERTAINTIES.md)
- [Operations](docs/OPERATIONS.md), [causal records](docs/RECORDS.md),
  [checkpoints](docs/PERSISTENCE.md) and [replay](docs/REPLAY.md)

Use `make core` to package `build/core/libCanterburyCore.a`, eight public headers,
resources and the independent ordinary frontend. The core has one SDK owner,
clock and population; it links without Cocoa or an application entry point.
The [developer guide](docs/DEVELOPER-GUIDE.md), [architecture](docs/ARCHITECTURE.md)
and [reuse handoff](docs/REUSE-FOUNDATION.md) define IDs, policy boundaries,
checkpoint compatibility and independent extension RNG domains.

The SDK fixes are committed at `3892ece`, with the parent gitlink committed at
`e242c5d`. Exact pins/file hashes are in the [source manifest](content/source-manifest.json);
the reviewed SDK tree is clean. The remaining parent implementation/documentation
changes are left for review/commit. Qualification uses an exact temporary source
snapshot without committing those pending parent files or publishing remotely.

```sh
make test
make final-check
make performance
python3 tools/verify_release.py --check
```

The completed gate passed runtime/fault/replay suites, isolated original SDK
regressions, Xcode Debug/Release builds, independent core linking, normal native
UI checks and five 1,440-minute population workloads. Measured actual-map step
times were 22.3 seconds at 27,000 people and 25.4 seconds at 30,000; the separate
30,000-glyph AppKit capture benchmark measured 18.4 ms p95. These are measurements
on the qualified host, not universal timing or physical-display FPS guarantees.

- [Performance scope and budgets](docs/PERFORMANCE.md)
- [Robustness and SDK isolation](docs/ROBUSTNESS.md)
- [Release procedure and compatibility limits](docs/RELEASE.md)
- [Current native UI evidence](content/cycle-35-40-native-ui.json)
- [Measured performance results](content/cycle-37-validation.json)
- [Versioned evidence index](content/evidence-index.json)
- [Passing release gate](content/cycle-40-validation.json)
- [Completed 40-cycle plan](PLAN.md)

All source, data and documentation required for this ordinary release live here
and in the two declared submodules. No sibling application or deleted web upload
is required. New historical data or game integration is a separate phase with its
own review and qualification; no additional implementation cycle is outstanding.
