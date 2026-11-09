# Standalone builds — Cycle 03

Cycle 03 implements a **minimal native bootstrap**, not the human city routines
or map import. It starts with 27,000 ordinary-person native bodies, as requested
by the user, binds the actual Bronze actor IDs, observes the native callback
and clock, and provides CLI and native macOS step controls. There is no special
actor, second population, independent simulation timer or inherited campaign.

The public `CdWorld`/content/operation/snapshot API in `src/cd_api.h` remains a
future implementation contract. The small `src/cd_bootstrap.h` adapter is a separate
bootstrap interface implemented in `src/cd_adapter.c`. Its callback validates
native binding/counts and deliberately performs no city movement, metabolism,
work, historical demography or institution simulation. Those have later cycles.
Native island autonomy is already bypassed by the pinned SDK city profile.

## Toolchain and setup

The verified platform is macOS with the Apple C/C++ compiler, command-line build
tools, macOS SDK, Python 3.10+ and Make. The native app uses the system Cocoa
framework. zlib, math and pthread linkage use the platform toolchain. A portable
CLI is intended, but other operating systems have not been qualified in this
cycle. The native app target diagnoses non-macOS platforms explicitly.

```sh
git submodule update --init --recursive
make check-deps
make all
make app
make test
```

`make all` builds `build/CanterburyDetails`. `make app` builds
`build/CanterburyDetails.app`, an ordinary macOS app bundle. Launch that bundle
normally through Finder or macOS Launch Services, not by invoking its GUI binary
as a headless executable. The visual window starts paused and offers Run/Pause, Step 1/10, zoom/pan,
source-record inspection and search. The presentation scheduler calls the native
step API only while running; no second simulation clock is introduced.
See `docs/XCODE.md` for the Xcode project and exact json2png map pipeline.

```sh
./build/CanterburyDetails --people 32 --minutes 5 --seed 1940
./build/CanterburyDetails --people 32 --minutes 0
```

The initial native binding cycle is observed and included in the result: five
requested additional minutes produce six callbacks and native elapsed minutes.
Zero requested minutes leaves that initial binding unchanged. The seed and people
count are technical fixtures, not historical Canterbury population evidence.
The default and capacity are both 27,000, with no reserved special-character
slot. Smaller 32-person and one-person fixtures keep smoke checks inexpensive.
The default full-population startup is checked explicitly; capacity/ABI qualification is recorded in Cycle 04, and the full city lifecycle
is qualified for the bootstrap in Cycle 05; human routines remain in later cycles.

The native fixed-capacity body layout still requests roughly 1.90 GB at the city
profile even for a small initial population. This cycle does not claim compact
memory, full-population performance or production memory qualification.

## Build ownership and isolation

`Makefile` delegates to the project-local `tools/build.py`. That tool admits the
local audited submodule revisions and required input files before compiling.
Missing data/engine submodules produce a named diagnostic with the initialization
command. It never searches a developer sibling directory or downloads a fallback.

All SDK and project translation units use `CANTERBURY_SIM=1` and `BRONZE_SIM=1`.
The public profile guard is forcibly included in every translation unit. Explicit
contradictory `CPPFLAGS` are rejected before compilation; compiler diagnostics
also reject inconsistent settings. `CC`, `CFLAGS` and `CPPFLAGS` are supported
explicit toolchain inputs. They do not justify unreviewed external source inputs.

Project and SDK files are referenced only inside this checkout. The exact
source/header/include hashes, compiler identity, command and output hash are
recorded in ignored `build/*-build.json`. Rebuild caching requires both matching
inputs and matching output bytes. Compilation uses a temporary binary and only
replaces the last executable on success. Build outputs and Python caches are
ignored. `make clean` removes only this checkout's `build/` directory.

The native app is compiled with the same model/SDK source set and profile as the
CLI, with its own project-local entry point and Info.plist. SDK files stay in the
submodule. Six reviewed SDK files implement the requested city-only population
and host-owned heap/startup storage seams; ordinary native/Bronze branches retain
their behavior and layouts. See `docs/MEMORY.md` for ownership and accounting.

`content/sdk-working-tree.json` records the exact reviewed SDK file hashes against
the pinned base. The audit rejects extra or changed SDK modifications. These SDK
changes are uncommitted: commit them in `apesdk` before updating the parent gitlink.
After that, update the recorded revision, retire the working-tree review, refresh
the source manifest and regenerate validation evidence before committing the host.
The temporary clean-room build includes the exact reviewed SDK changes.

## Qualification commands

```sh
make smoke
./build/smoke-test
python3 tools/verify_cycle03.py --run
python3 tools/verify_cycle03.py --check
make audit
make capacity
make lifecycle
make identity
make memory
make content
make content-check
python3 tools/verify_sdk_profile.py --run
python3 tools/verify_sdk_profile.py --check
```

The C smoke fixture checks actual native identity resolution, population counts,
read-only repeated inspection, zero-step pause, exact callback/minute increments,
second-live-session rejection and close/reopen. Fresh-process CLI repeats must
produce identical reports. Invalid CLI values and conflicting profile flags fail.
Project C sources also pass strict syntax checks.

The verifier creates a disposable independent repository containing the current
owned files and real local clones of the pinned dependencies, with their origins
restored to the recorded upstream URLs. It commits only that temporary fixture,
with hooks/signing disabled, builds CLI and native app from it, and compares the
CLI report to the working checkout. No commit is made in the user's repository.
It then removes each dependency *only in the disposable copy* to verify explicit
missing-submodule errors. No network or previous application checkout is needed.
This tests the uncommitted working snapshot as a clean-checkout equivalent; actual
remote reproduction additionally requires the user to commit these owned files.

Evidence: `content/cycle-03-validation.json`, ignored build logs and a separate
normal native-app observation record (`content/cycle-03-native-ui.json`).
The SDK evidence records isolated native, contract, actor and hybrid suites, layout
probes, and ordinary-profile preprocessor comparisons excluding preprocessing indentation/blank lines and diagnostic line
numbers that shift when guarded source lines are added. CLI/build qualification is distinct from
GUI launch evidence and from later simulation/profile/performance qualification.
Cycle 01 input inventory and Cycle 02 ownership/API checks are revalidated; later
cycles must continue extending their contracts as implementation grows.

## Offline candidate content

Cycle 08 supplies `make content` and `make content-check` without starting a
native population. The ignored output is `build/content/canterbury.json`;
`docs/CONTENT.md` defines its versioned schema, locked source/review inputs and
atomic publication. `make test` includes full reproducibility and rejection tests.
The visual client now separately displays the exact upstream json2png map resource
and source-record candidates. Physical access/routing and the C content loader
remain later-cycle work.

The default population is **27,000**, distinct from the **30,000** native/identity
capacity. The 3,000 spare slots support later growth; births remain Cycle 14 work.
