# Complete atomic city checkpoints

Cycle 31 implements `cd_world_save` / `cd_world_load`. Save at any closed native
minute, including during travel, productive work, development, care, gatherings
and transport. There is no rest or location restriction. The caller serializes
all native access; Cocoa performs file I/O and staging on its existing worker.

Version 1 is an ABI-qualified gzip stream: magic/version, endian/layout signature,
owned/SDK header fingerprint and audited SDK source identity, exact content
fingerprint/generation, population/source dimensions and authoritative digest;
then a pointer-cleared scalar city image, every owned model array, native land,
timing/remains, all live bodies, registry/work state, monotonic IDs, deterministic
RNG streams, and a stream checksum. Gzip CRC and structural/accounting checks
provide accidental-corruption detection. Files are not authenticated or encrypted.

The exhaustive generated world pointer list rejects an unaccounted pointer field
at build time. Every persisted person reference has a zero runtime token. Borrowed
work basis pointers must be null. Native city checkpoints reject DSL programs,
dynamic Bronze collections and runtime callbacks instead of silently omitting them.
All current Canterbury city state is supported. Route search scratch is rebuilt;
route progress, traffic, door reservations, custody, inventory, escrow, queued
operations, calendars, causal records and transition cursors are preserved.

Save uses a sibling temporary file, closes compression, fsyncs it and atomically
renames it over the target. Failure before rename leaves the prior checkpoint
untouched and removes the temporary file. Parent directory fsync is best effort;
power-loss durability depends on the filesystem. Save does not mutate city state.

Load validates the header before allocations, stages a complete independent city
and native registry, validates bounds/references, unique live/archive IDs, clocks,
finite ledgers and reservations, CRC/checksum, EOF and the full city/body digest.
Only the owning world handle may replace the active city. Failure discards staging
and preserves the active world and existing file. Successful publication replaces
native storage without startup, an extra minute or RNG draws. References are then
rebased to a fresh process-local namespace. Resolve a saved logical ID with
`cd_world_reference`; do not persist runtime `CdPersonRef` borrows externally.
Old snapshots remain valid immutable copies.

Compatibility is deliberately strict: same format/layout, source-qualified SDK
and content generation. Mismatches fail closed. There is no implicit migration,
portable cross-architecture guarantee, or compatibility with another application's
checkpoint. A future migration must be explicit and independently tested. The old
calendar component codec remains a same-instant calendar validation tool; use
this complete city format to resume a session.

Acceptance includes live continuation, an empty city, all allocation failures,
non-owner replacement, failed publication, malformed/truncated/CRC/ABI/content
files and separate-process continuation. The aggregate allocation limit bounds
both live and staged storage. See `tests/observer.c` and Cycle 31 evidence.
