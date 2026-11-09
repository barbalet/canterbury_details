# Memory, publication and RNG — Cycle 07

All city-scale host arrays are heap allocations through `cd_memory_calloc`.
The allocator checks multiplication, header addition, category validity and a
5 GiB aggregate payload budget before allocation. It returns aligned zeroed
storage and never publishes a partial allocation. `cd_memory_free` and centralized
session teardown own cleanup. The test-only `CD_TESTING` build can fail exactly
the Nth subsequent allocation; production builds contain no failure switch.

## Storage inventory and budget

The native fixed-capacity group is allocated on the heap under `CANTERBURY_SIM`.
Capacity is now 30,000 while startup remains 27,000. This intentionally changes
only the city group/registry layout; ordinary profile layouts remain unchanged.
The SDK supplies guarded size/attach APIs for native startup and size/init APIs
for the actor registry. The host owns their borrowed storage until close.
Legacy direct city callers retain the guarded SDK-owned heap fallback.

The five accounting categories are native group, actors/registry/work sidecars,
city metadata, temporary permutation/scratch storage, and SDK startup buffer.
Exact current category counts and the live-plus-stage peak are recorded by the
current 30,000-capacity run in `content/cycle-07-validation.json`. Two staged
resource sets must fit under the unchanged 5 GiB payload ceiling. Runtime city
arrays and snapshots are also charged to that aggregate ceiling.

These are allocated payload bytes, not RSS or a production performance result.
Allocator headers, platform allocator overhead, SDK static land/render buffers,
Cocoa objects, stacks and OS caches are excluded. Header overhead is one aligned
header per tracked block. No body/snapshot compression is claimed.

The 5 GiB payload ceiling permits one live resource set and one staged replacement
at the current layout. More staging may be refused. An unchanged small population
still reserves full capacity. Prepared native storage is zeroed allocation, not an
extra simulated population: only the published resource set binds the SDK. Peak
RSS, save staging and full-day budgets are measured in Cycle 37; see
[PERFORMANCE.md](PERFORMANCE.md).

## Prepare and publish

`cd_smoke_prepare` requires an initially null stage output; it rejects overwriting
an owned stage. Rejected opens likewise preserve an existing live handle.
It validates bounds and allocates every required session, person,
registry, native body and SDK startup buffer before touching the live SDK. It
assigns a fresh runtime world token and deterministic seed state. Stages own their
resources and can be discarded in any order. Failure releases the whole stage and
preserves the live registry, bodies, references, digest, native clock and RNG.
A failed attempt may consume an unpublished world token and change diagnostic
allocation counters/peaks; these do not constitute simulation state.

`cd_smoke_publish` requires the correct current owner and a closed native interval.
Rejected publication leaves both stage and live owner intact. After validation it
closes the previous owner, attaches already allocated storage, activates the fresh
registry and initializes the native bodies/clock with the prepared seed. This
serialized commit performs no allocation. The prepared, fixed-layout fresh city
profile has no recoverable startup operation left: callback/initialization failure
here indicates an internal invariant defect and terminates rather than returning
a partially valid world. Content admission and checkpoint restoration stage all fallible work before
publication. This low-level bootstrap transaction is resource staging; full
checkpoint restoration is documented in [PERSISTENCE.md](PERSISTENCE.md). Only one
native SDK owner is active.

The caller's handle changes only on successful publication; the stage pointer is
consumed and cleared. Old value references fail in the new world. `cd_smoke_open`
uses the same prepare/publish path when no world is active. SDK borrowed storage
never frees host buffers itself. The host tears down SDK bindings before freeing
its storage. All operations and accounting are serialized on the owner thread.

## Deterministic city RNG

Three explicit SplitMix64 streams are seeded from the scenario seed with stable
numeric stream domains: admission (0), routines (1), services (2). Each initial
state mixes `seed + golden_ratio * (stream+1)` modulo 2^64; each draw adds the same
golden-ratio increment and applies the SplitMix64 finalizer. Native body RNG is
still owned by the SDK. Runtime world tokens, wall time, addresses, allocation
attempts and staging/discard do not enter either seed.

`cd_smoke_random` explicitly consumes one requested city stream. Queries and UI
refresh never draw. `cd_smoke_admit_next` stages a draw on the admission stream,
uses its low 32 bits as the technical native fixture seed, and commits that draw
only after successful admission. A refused full-capacity admission consumes no RNG.
The explicit-seed admission API remains available for fixtures. The three streams and native RNG state are included in complete checkpoints and
replay qualification. The independent extension-RNG function is documented in
[DEVELOPER-GUIDE.md](DEVELOPER-GUIDE.md).

## Qualification

Run `make memory`, or `make test`. Tests inject failure at every allocation in
empty construction and replacement preparation, check zero outstanding blocks
on failure/close, preserve live digest/references after every rejected stage,
and arm the next allocation to fail during successful publication to prove that
commit allocates nothing. Repeated reopen/close, overflow/category/budget rejection,
known RNG vectors, stream isolation and refused-admission RNG rollback are tested.
The tracker covers all host startup allocations including SDK borrowed storage;
this is deterministic ownership accounting, not a general process leak detector.
SDK regression and ordinary-layout evidence is regenerated for the guarded seams.
