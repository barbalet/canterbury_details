# Standalone developer guide

Initialize the two pinned submodules, install the local compiler/Xcode plus
pkg-config, libpng and FreeType, then run `make app`. Builds never download code or
read sibling application/web-upload files. Model data comes from the checked-out
Canterbury submodule; CANTERBURY_SIM-only SDK seams provide native bodies, the sole
clock, shared work grants, storage and staged checkpoints. All host/native units
use CANTERBURY_SIM=1 and BRONZE_SIM=1 consistently.

```
make cli app
make json2png2
make core
make test
make final-check
make performance
python3 tools/verify_sdk_profile.py --run
python3 tools/verify_xcode.py --run
python3 tools/verify_release.py --prepare
```

`make performance` runs full-day/30,000-person workloads; launch the generated
PresentationBenchmark.app normally for AppKit timing, then run
`tools/verify_final_cycles.py --finalize-performance`. The release prepare command
creates a separate committed source snapshot, clones it with local pinned
submodules and runs the offline gate there. It does not commit the user's pending
parent source changes or push a release. Its report names the exact qualified
source tree, which may differ from the current parent HEAD until the user commits.
Native release UI checks use the resulting .app through normal application launch.

## Independent core

`tools/build_core.py` produces `build/core/libCanterburyCore.a`, the eight public
headers under `include/`, copied qualified resources, an ordinary mock consumer
source and its standalone executable. It excludes CityApp/CLI/renderer mains and
all Cocoa code. C clients link with the city profile, zlib, math and pthread:

```
cc -std=c11 -DCANTERBURY_SIM=1 -DBRONZE_SIM=1 -Ibuild/core/include \
  examples/ordinary_frontend.c build/core/libCanterburyCore.a -lz -lm -lpthread -o /tmp/OrdinaryFrontend
/tmp/OrdinaryFrontend build/core/resources/CanterburyCity.bin /tmp/ordinary.city 32
```

The gate copies the package outside the checkout and recompiles/runs that source
there, verifies deterministic startup and checkpoint continuation, and checks that
no Cocoa/AppKit/Homebrew runtime library or application main was pulled in. Use
one bundled SDK instance only; do not link a second differently configured SDK.
`CdWorld` is single-owner/single-threaded. A frontend serializes reads and writes;
Cocoa does so on CitySession's worker, publishing immutable views to main.

Public layering: cd_api → city/human/work/civic → observer/management → cd_core.
Stable IDs are persistent identities; slots and world tokens are runtime borrows.
Load replaces a complete generation without a minute and refreshes tokens. Queue
ordinary intents at closed boundaries; never run another update clock or edit
native arrays. `cd_extension_random(seed, domain, counter)` is a pure, version-1
extension-domain function; it does not consume city RNG streams. Consumers own its
nonzero named domain/counter and companion-save coordination. No speculative
policy callbacks or mythology fields were added to the ordinary model.

Checkpoint v1 admits only matching ABI/profile/content; earlier source-qualified
saves may be rejected. No implicit migration is supplied. Changes to public/private
layout or admitted source metadata require regenerated evidence and compatibility
review. `tools/checkpoint_abi.py` accounts for every world pointer; native/Bronze
payloads reject unsupported dynamic DSL extensions. See PERSISTENCE.md.

Management is an allocation-free bounded public read model; UI and CLI consume
the same pages. New commands require real ordinary APIs, input validation,
idempotence/cancel rules, a documented field contract and regenerated controls.
Source feature review remains separate from map rendering. Modern road topology
must never become claimed 1940 traversal/occupancy merely because it draws well.
See ARCHITECTURE.md, SOURCES.md, REUSE-FOUNDATION.md and RELEASE.md.
