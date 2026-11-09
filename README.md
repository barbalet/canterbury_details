# Canterbury Details

Model ordinary human Canterbury in 1940 using the pinned `canterbury` data and
`apesdk` engine submodules. Religious information is retained for occupations,
institutions and community services. No supernatural model is part of this host.

The project implements **Cycles 01–18**. The native visual app admits **27,000
ordinary humans** in **6,750 explicitly synthetic households**. Geometry, ordinary
access, continuous native movement, qualified routing and civil records now have
runtime implementations and acceptance fixtures. Lifecycle, daily needs, finite
food, configured ordinary health/care and the audited occupation catalogue are
also implemented. The repeated-name bug is fixed: the default population has
26,898 distinct full names, with stable IDs kept separate from display names. The real map retains unreviewed
routes/entrances and unknown housing; it does not invent a historical census.
The exact upstream json2png map remains zoomable and pannable. A serial simulation
worker keeps map interaction independent of native stepping and population scans.

```sh
git submodule update --init --recursive
make all
make app
make content
make content-check
make runtime-content
make city-check
./build/CanterburyDetails --city build/content/CanterburyCity.bin --minutes 5
make test
```

Open **CanterburyDetails.xcodeproj**, choose **Canterbury Details / My Mac**, and
press **Command-R**. Or launch `build/CanterburyDetails.app` after `make app`.
The visual map supports zoom, pan, church/building inspection and resident search.
**All agents…** shows every resident as a zoomable `+` roster glyph; amber means
unplaced, teal means a known map position. **Agents (+)** toggles known-position
map markers. Roster cells are explicitly nongeographic; no locations are invented.
Selecting a located church recenters the map even at unchanged zoom. Run/Pause
and Step controls advance the sole native clock; map inspection stays read-only. The scenario calendar
derives from native elapsed time, with an explicit 1940-01-01 00:00 origin.
Startup includes one binding minute, so the paused opening display is 00:01. See [build instructions](docs/BUILD.md).

- [Geometry, ordinary access and entrance review](docs/GEOMETRY.md)
- [Native movement, coordinates and budgets](docs/MOVEMENT.md)
- [Qualified topology, closures and crowds](docs/ROUTING.md)
- [Synthetic population and historical limits](docs/POPULATION.md)
- [Households, housing, visitors and ownership](docs/HOUSEHOLDS.md)
- [UI worker ownership and responsiveness](docs/UI-THREADING.md)
- [Current native UI validation](content/cycle-14-18-native-ui.json)
- [Xcode setup, visual controls and json2png renderer](docs/XCODE.md)
- [Reusable foundation for the later game](docs/REUSE-FOUNDATION.md)
- [Offline content schema, provenance and generation commands](docs/CONTENT.md)
- [Cycle 08 content compiler evidence](content/cycle-08-validation.json)
- [Memory inventory, publication and RNG contract](docs/MEMORY.md)
- [Cycle 07 fault-injection and memory evidence](content/cycle-07-validation.json)
- [Cycle 07 native host observation](content/cycle-07-native-ui.json)
- [Person identity and binding contract](docs/IDENTITY.md)
- [Cycle 06 identity evidence](content/cycle-06-validation.json)
- [Cycle 06 native host observation](content/cycle-06-native-ui.json)
- [Native time and lifecycle contract](docs/TIME-LIFECYCLE.md)
- [Cycle 05 clock/lifecycle evidence](content/cycle-05-validation.json)
- [Cycle 05 native clock observation](content/cycle-05-native-ui.json)
- [SDK capacity, admission and ABI contract](docs/SDK-INTEGRATION.md)
- [Cycle 04 capacity/profile evidence](content/cycle-04-validation.json)
- [Cycle 03 build/startup evidence](content/cycle-03-validation.json)
- [Native UI observation](content/cycle-03-native-ui.json)
- [SDK regression evidence](content/cycle-03-sdk-validation.json)

The reviewed population changes in `apesdk` remain uncommitted. Commit those
before updating the parent submodule pointer; see the build instructions for
refreshing the audited source baseline afterward. No user-repository commits
were made during qualification.

- [Standalone architecture and public API contract](docs/ARCHITECTURE.md)
- [Reuse and attribution review](docs/REUSE.md)
- [Cycle 02 validation evidence](content/cycle-02-validation.json)
- [Sources, licenses and available SDK contracts](docs/SOURCES.md)
- [Historical/data uncertainties and downstream decisions](docs/UNCERTAINTIES.md)
- [Exact source/pin/file inventory](content/source-manifest.json)
- [Cycle 01 validation evidence](content/cycle-01-validation.json)

After initializing the submodules, verify the recorded source baseline offline:

```sh
python3 tools/audit_sources.py --check
python3 tools/verify_cycle01.py --check
```

Repeat the upstream map validators and tests in an isolated temporary copy:

```sh
python3 tools/verify_cycle01.py --run
```

Review source/pin changes before using `python3 tools/audit_sources.py --write`
to record a new baseline. Validation does not establish historical truth or
approve candidate routes for movement. Broader simulation and lifecycle checks
belong to later cycles. All project code/data inputs stay in this checkout and
its two submodules; no prior application or uploaded web page is required.

Validate the Cycle 02 ownership/dependency contract and declaration-only API:

```sh
python3 tools/verify_cycle02.py --run
python3 tools/verify_cycle02.py --check
```

The checks compile C/C++ header probes without linking a city implementation;
these Cycle 02 checks alone do not qualify runtime behavior.

The default population is **27,000**, distinct from the **30,000** native/identity
capacity. The 3,000 spare slots support later growth; configured births/admissions are implemented in Cycle 14.

Human model acceptance: `make human-check`. See [lifecycle](docs/LIFECYCLE.md),
[needs](docs/NEEDS.md), [food](docs/FOOD.md), [health](docs/HEALTH.md) and
[occupation definitions](docs/OCCUPATIONS.md). The native resident inspector shows
current energy/fatigue/activity, and the catalogue distinguishes role eligibility
from workplace assignments and shifts implemented in Cycles 19–20.

Cycles 19–23 add bounded qualified employment, actual street commutes, resumable
shared-minute work, finite goods/wages/purchases, scenario premises works and all
33 exact institution records. See [employment](docs/EMPLOYMENT.md),
[work](docs/WORK.md), [economy](docs/ECONOMY.md),
[development](docs/DEVELOPMENT.md) and [institutions](docs/INSTITUTIONS.md).
Run `make work-check` for native acceptance fixtures. Technical scenario rates
remain explicit; the real map still requires spatial qualification before staffing.

Cycles 24–28 add distinct ordinary religious offices and succession, explicit
community calendars and attendance, permission-checked civic dispatch, finite
handcart/stretcher transport and explicit environmental experiments. See
[religious employment](docs/RELIGIOUS-EMPLOYMENT.md), [community](docs/COMMUNITY.md),
[services](docs/SERVICES.md), [logistics](docs/LOGISTICS.md) and
[environment](docs/ENVIRONMENT.md). `make civic-check` runs the native fixtures.
No historical staffing, fleet, holiday, raid, weather or damage is inferred.
Calendar component checkpoints are available; full atomic city saves remain Cycle 31.
