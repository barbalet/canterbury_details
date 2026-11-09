# Standalone builds

The completed ordinary Canterbury 1940 application provides the CLI, native macOS
UI and independently linkable CanterburyCore package. The native SDK owns all
people, positions and time; the project implements civil records, movement, needs,
jobs, services, development, operations and complete checkpoints. The bootstrap
adapter remains a separate low-level qualification interface.

## Toolchain and setup

The verified platform is macOS with the Apple C/C++ compiler, command-line build
tools, macOS SDK, Python 3.10+ and Make. The native app uses the system Cocoa
framework. zlib, math and pthread linkage use the platform toolchain. A portable
CLI is intended, but other operating systems have not been qualified for this release. The native app target diagnoses non-macOS platforms explicitly.

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
The default is 27,000 and capacity is 30,000, with no reserved special-character
slot. Smaller 32-person and one-person fixtures keep smoke checks inexpensive.
Capacity/ABI and bootstrap lifecycle are qualified in Cycles 04–05. Human routines,
configured lifecycle changes and services are qualified in Cycles 14–28.

Small populations still reserve the full 30,000-body native layout. See
[MEMORY.md](MEMORY.md) for the 5 GiB owned allocation ceiling and
[PERFORMANCE.md](PERFORMANCE.md) for measured RSS and full-day budgets.

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

`content/sdk-working-tree.json` records a clean committed SDK tree. The guarded
checkpoint staging/validation fixes are committed at `3892ece`; the parent gitlink
pins that revision. `content/sdk-profile-baseline.json` separately retains the
pre-fix revision for meaningful ordinary native/Bronze isolation comparisons.
After any future SDK edits, review their scope and refresh source/profile evidence.

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
CLI report to the working checkout. This verifier makes no commit in the user's repository.
It then removes each dependency *only in the disposable copy* to verify explicit
missing-submodule errors. No network or previous application checkout is needed.
This tests the uncommitted working snapshot as a clean-checkout equivalent; actual
remote reproduction additionally requires the user to commit these owned files.

Evidence: `content/cycle-03-validation.json`, ignored build logs and a separate
normal native-app observation record (`content/cycle-03-native-ui.json`).
The SDK evidence records isolated native, contract, actor and hybrid suites, layout
probes, and ordinary-profile preprocessor comparisons excluding preprocessing indentation/blank lines and diagnostic line
numbers that shift when guarded source lines are added. CLI/build qualification is distinct from
normal GUI launch evidence. The completed release also qualifies runtime,
profile isolation and performance; see [RELEASE.md](RELEASE.md).

## Offline candidate content

Cycle 08 supplies `make content` and `make content-check` without starting a
native population. The ignored output is `build/content/canterbury.json`;
`docs/CONTENT.md` defines its versioned schema, locked source/review inputs and
atomic publication. `make test` includes full reproducibility and rejection tests.
The visual client displays the final assembled map through json2png2 and the
upstream json2png core. The C runtime loader and physical access/routing are
implemented; source routes, doors and capacities still require explicit review.

The default population is **27,000**, distinct from the **30,000** native/identity
capacity. The 3,000 spare slots support configured births/admissions implemented in Cycle 14.
