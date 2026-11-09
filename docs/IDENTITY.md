# Stable people and native bindings — Cycle 06

A live project person has a `CdPersonRef`: a nonzero world token plus the existing
SDK `BronzeActorId`. IDs are never names or array indices. The SDK owns identity
allocation and physical membership. `cd_people.c` owns the project live-person
record and its derived native-slot binding. Native bodies continue to own physical
traits, position and energy. This does not implement civil names, households,
demographics, birth/death narratives or archived life records from later cycles.

## Records and references

Each session allocates room for at most 30,000 live records, sorted by increasing
native actor ID. Lookup is binary search. Removal compacts these private records;
record addresses and indices are not public identities. The highest observed
native ID is retained as a validation high-water mark, so removing the last live
record does not permit a retired ID to be inserted again. New IDs still come only
from the SDK. Retired references resolve as `CD_UNKNOWN_REFERENCE`.

A process-wide monotonically increasing token identifies each independently
opened session. Failed construction may consume a token; tokens are never reused.
Exhaustion rejects construction. A restarted SDK may allocate actor ID 1 again,
but its new world token makes it a different project person. Tokens are runtime
namespaces, not a portable save-file identity. Save/load namespaces are later work.
Do not pass closed session pointers or borrowed native/record pointers; retain
value references and resolve them within a live owning session instead.

`cd_smoke_person` returns the reference at a current native slot.
`cd_smoke_resolve` rejects zero, retired, missing or foreign-world references and
checks that the SDK resolves the actor to the same native body. Outputs are not
written on failure. A current slot is a temporary lookup result, not a persistent
reference. All operations require the owning closed native interval and run on
one serialized owner thread. They consume no native minutes.

## Admission, retirement and relocation

`cd_smoke_admit` initializes an ordinary technical native fixture from an explicit
seed, admits its identity through the SDK birth hook, and publishes a project
record and incremented native count together. Capacity/ID bounds are checked before
body initialization. A full 30,000-person world refuses admission; no reserved
actor or overflow population exists. The explicit seed is fixture input, not a
second simulation RNG stream. Biological birth, immigration rules, visitor queues
and trait distributions are later cycles.

`cd_smoke_retire` validates the complete value reference before mutation. It
retires the native actor, compacts the last surviving native body into the vacant
slot, invokes the SDK relocation hook, erases the old tail, removes the project
live record, and updates surviving slot bindings. The moved person's ID and body
state survive. Native selection follows its surviving actor, or falls back to a
live body/null when the selected person retires. No retired ID is reused.
Zero live people is supported after retirement, including rebind and stepping;
initial construction still requires at least one person.

## Complete order and rebind

`cd_smoke_reorder` accepts exactly one reference for every live person. Before
mutating native bodies or bindings, it validates world tokens, existence,
completeness and uniqueness. Duplicate, missing and foreign references fail
without changing live records, actor IDs, body order, selection or native time.
The SDK validates and publishes its complete binding order before the serialized,
non-failing body permutation and project slot commit. No observer/callback runs
inside that transaction. A cycle permutation uses one body scratch buffer rather
than another full 1.90 GB body array. Selection follows actor identity.

`cd_smoke_rebind` reconstructs the full native binding order from existing project
records, verifies unique complete slot coverage, and uses the SDK's atomic
`bronze_actors_bind_order` API. It can rebind an existing owning registry after
reactivation invalidates physical bindings. It does not reset identities, replace
the registry, or load arbitrary native data. New content/save publication remains
later work. Direct out-of-band SDK births, deaths or body changes bypass project
membership and are unsupported: use the host APIs to keep both sides consistent.
The minute callback and report inspection check project/native correspondence.

## Qualification

Run `make identity`, or `make test` for the complete host suite. The identity test
uses real sessions at 32, 27,000 and 30,000 people. It marks native body RNG fields to prove
that whole bodies follow identity through a cyclic permutation, checks selection,
interior removal/last-body relocation, fresh admission, retirement rejection,
reactivation/full rebind and subsequent native stepping. Small fixtures also
retire to empty and readmit without reusing an ID. Restart tests deliberately use
the same native ID in a new world and reject the old value reference.

Rejected duplicate/incomplete/unknown/foreign permutations and repeated retirement
preserve the population digest and live lookups. Existing capacity tests now use
host admission/retirement so they also maintain project records. The source and
behavioral evidence is `content/cycle-06-validation.json`. No SDK edits were needed.
Allocation-failure injection, staged world replacement and comprehensive memory
accounting remain Cycle 07; no sanitizer or thread-safety claim is made here.
