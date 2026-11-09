# SDK capacity and profile contract — Cycle 04

The host starts with **27,000 ordinary humans**, as required by the user. This is
a simulation total, not a recovered 1940 census. The default fills every available
native slot; no special actor is appended or reserved. Smaller seeded fixtures
are technical tests. City routines, demographic rules and visitor scheduling
remain later cycles.

## Profile and ABI

All linked host and SDK translation units must use `CANTERBURY_SIM=1` and
`BRONZE_SIM=1`. Never link objects built under different profiles. The build tool
forces the same flags and public profile guard throughout; contradictory settings
fail before compilation. Profile macros are presence switches in the upstream
SDK, so defining either to zero is not a supported way to disable it.

The city profile has `CANTERBURY_CAPACITY=30000`, `LARGE_SIM=30000` and
`CANTERBURY_POPULATION=27000`. Ordinary
native and Bronze profiles retain capacity 256. `simulated_group` contains a fixed
body array, so its size and subsequent field offsets depend on the profile.
The native body layout remains common. Exact sizes and selected offsets for the
verified compiler/platform are recorded in `content/cycle-04-validation.json`.
They are observations, not a portable binary serialization format or ABI promise.

The city native group alone occupies 1,897,884,088 bytes on the qualified arm64
platform, before actor records, other SDK state, host data or snapshot staging.
Capacity qualification does not establish production memory/performance targets;
those remain Cycle 37. Do not put this group on the stack.

## Initial admission and rejected operations

`sim_canterbury_configure` requires a non-null callback and a population in
1..30000, before startup. Zero, overflow and reconfiguration while running fail.
The host validates `size_t` population before narrowing to the SDK integer type.
At startup, native group count, requested total and actor registry count must
agree, including the final capacity body at slot 29999. Each body has one nonzero stable
actor ID that resolves back to the same native body. Names are not identity.

The registry's guarded reset rejects counts beyond capacity, a maximum beyond
capacity, or a count above maximum without invalidating the currently bound
population. Tests verify counts, existing identity resolution and next-ID state
survive rejection. Host zero/overflow requests likewise preserve the active
session and its read-only digest.

## Bounded additional people

A birth or visitor must use the same native array and actor registry as every
other ordinary person. There is no overflow population. At the 27,000 default,
additional admissions must wait for a released slot or be refused by later host
policy; the capacity is not automatically increased.

At a serialized native lifecycle boundary, the existing SDK identity hook
`bronze_actors_birth(group, &group->beings[group->num])` admits only a valid next
slot while `num < max`. The caller initializes the native body and increments
`num` only after successful admission. An existing body's repeated notification
is idempotent. A pointer outside the fixed body array is rejected without changing
count or consuming an ID. The hook is an identity operation, not a complete
biological birth or visitor implementation.

Removal retires the identity through `bronze_actors_death`; the owner then adjusts
the native array/count and uses the relocation hook if moving surviving bodies.
A later person occupying a released slot receives a new ID; retired IDs must not
resolve. The capacity fixture uses the last slot to avoid relocation and performs
32 remove/readmit replacements at full population. Small/near-full fixtures also
exercise spare-slot admission. These fixtures manipulate only identity capacity,
not demographic physiology, histories, queues or visitor policy. Those features
must implement their own complete lifecycle in later cycles.

## Upstream change and regression evidence

Cycle 03 already made the user-requested city-only default/configure/body-bound
change. Cycle 04 makes no further SDK edits. Cycle 07 subsequently adds guarded heap/storage seams; see `docs/MEMORY.md`.
The exact six reviewed SDK files
and base revision are in `content/sdk-working-tree.json`; the source audit rejects
unreviewed changes. Changes are still uncommitted. Commit the SDK before updating
the parent gitlink, then refresh the source baseline and evidence as described in
`docs/BUILD.md`.

`content/cycle-03-sdk-validation.json` remains valid for the exact current source
manifest. It compares native/Bronze preprocessed changed code with the pinned
baseline, excluding preprocessing indentation/blank lines and shifted diagnostic source line numbers, verifies baseline
layouts, and records passing isolated native, contract, actors and hybrid suites.
Cycle 04 checks that evidence is current and records its hash rather than rerunning
unchanged regression suites. Independent native/Bronze/city executables additionally
probe current capacity, body/group sizes, group offsets and actor record size.

Run:

```sh
python3 tools/verify_cycle04.py --run
python3 tools/verify_cycle04.py --check
```

The verifier compiles `tests/capacity.c` against the same SDK source set/profile
as the CLI, with assertions enabled. It runs real native sessions at 1, 26,999
and 27,000, checks the separate default CLI startup, and runs independent ABI
probes. Evidence is in `content/cycle-04-validation.json`; generated binaries and
logs remain ignored under `build/`. No sanitizer or cross-platform qualification
is claimed by this cycle.

The starting population remains 27,000. Capacity is 30,000, leaving 3,000 native
and identity slots for growth. Birth/demographic admission is Cycle 14; this
change provides headroom without claiming that lifecycle implementation.
