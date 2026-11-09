# Offline Canterbury content — Cycle 08

`tools/compile_content.py` compiles only the audited files in this checkout into
`build/content/canterbury.json`. It uses Python's standard library, local Git
metadata and the pinned `canterbury` / `apesdk` inventory. It performs no network
request, downloads, sibling-project imports, upstream generation or source edits.
URLs, source filenames and upstream review instructions are preserved as data;
the compiler never follows or executes them.

## Generate and verify

```sh
make content
make content-check
python3 tools/verify_cycle08.py --run
python3 tools/verify_cycle08.py --check
```

`make test` includes the Cycle 08 qualification. Initialize both submodules first
and retain the current audited source manifest and Cycle 01 validation records.
Missing/modified inputs must be reviewed and qualified before rebaselining; the
compiler never repairs or blesses stale inputs automatically. `--check` compares
against a full fresh compilation and writes nothing. A missing, corrupt or stale
bundle fails. Generated content lives under ignored `build/`; `make clean` removes
it and `make content` recreates it. No SDK build or live population is needed to
compile content. No commits are made by these tools.

## Inputs and policy

The compiler's explicit allowlist contains 27 Canterbury files: vectors and source
annotations; original, modern-enriched and institution-enriched buildings;
source road ink, modern OSM roads and historical traces; water/rail ink and
reference corridors; institutions and their curated annotations; review candidates;
assembly/enrichment/connectivity reports; trace inputs, OSM snapshots/control points
and source notices; and five upstream generator scripts (hashed, never executed).
Raw OSM tiles are not supplied locally: their recorded hashes remain provenance,
not a claim that the missing original downloads were independently revalidated.

`content/content-policy.json` declares allowed format versions, year 1940, a
1600×1600 canvas, pixel units and candidate-only import. It supplies no new factual
geometry or historical demographics. The 27,000 starting people remain the user's
simulation requirement, separate from this map's boundary or any census.

Every captured source byte sequence must match its entry in the source inventory.
Current Cycle 01 geometry/enrichment/32-test evidence must match that same manifest
and its local logs. The compiler independently checks dimensions, closed rings,
finite/bounded coordinates, source classes/IDs, enrichment preservation, network
endpoint/object/route references, lengths, known status/boolean flags, institution
links, generator/input lineage, OSM identity/reference integrity and attribution.
Duplicate JSON keys, non-finite numbers, integers outside signed 64-bit range,
unsupported versions, stale annotations and unknown review targets are rejected.
Input size limits are 64 MiB per source file and 256 MiB total source bytes. These
are offline importer limits, separate from the native host payload budget.

This is admission of source candidates, not approval of physical access. Polygon
self-touch, collision/door geometry, grade separation, walkability, routing and
historical applicability remain Cycles 09–11. The existing upstream tests qualify
source assembly; they do not establish historical truth.

## Bundle schema and preserved data

`content/content.schema.json` is the version 1 JSON Schema envelope. A local,
explicit subset validator supports exactly the keywords used by that schema;
unknown keywords fail. The schema's `$schema`/`$id` are metadata and are never
resolved online. Semantic validation above supplies cross-record checks beyond
JSON Schema. Raw upstream metadata retains its versioned source shape.

The bundle contains:

- `setting`, `canvas`, `units` and explicit candidate-only/no-routing admission.
- `provenance`: both pinned revisions, exact source-manifest hash, source path/hash/
  size records, compiler/audit/policy/schema/review hashes and current source-test
  evidence hashes. No host path or current timestamp is added.
- `data.vectors` and `data.annotations`: all original map classes, rings, palette,
  labels and reference annotations, preserving source paint/order information.
- `data.buildings`: source-ID-preserving candidate records, with modern extra fields under
  `modern_annotation` and institution extras under `institution_annotation`.
  Source-locked street-clearance corrections update rings and selectable bounds,
  with per-record placement provenance; merged blocks retain their source area.
  Original vectors and upstream documents remain unchanged.
  Metadata for both annotation layers is preserved separately. Names/use transferred
  from OSM remain modern suggestions, never verified 1940 identity.
- Separate road-ink, modern-road, historical-trace, water-ink, rail-ink and
  reference-corridor documents. Their nodes/edges, topology, status, flags and
  provenance remain separate; the importer creates no joins or connectors.
- Original institution records and scenario policy, upstream review candidates,
  source context/reports and independent project review annotations.
- Counts and a deterministic `generation_id`.

All five unlocated institution records remain non-spatial. The Old Synagogue's
`scenario_active` assignment and historical closure qualification remain together.
Religion is institution/community information; it creates no resident affiliation,
moral score, supernatural rule or geographic exclusion.

Arrays preserve source order. Object keys serialize canonically as UTF-8 with
sorted keys, compact separators, a trailing newline and no non-finite numbers.
`generation_id` hashes the canonical bundle without that field; the CLI's
`generation_sha256` hashes the final complete file. Identical input bytes, tool
bytes, revisions and policies reproduce identical output bytes. Changes to SDK
inventory or source-validation evidence also require a new generation because
those are part of the explicit compilation lock.

## Independent project reviews

Edit `content/review-annotations.json`, never generated upstream files. Its root
pins the vector hash. Each annotation identifies a dataset/record, pins that
input file's hash and records one of `needs_review`, `retain_candidate` or
`exclude_from_simulation`, with a nonempty note. Duplicate, unknown or stale targets
fail compilation. Dataset names for edges are `road_ink`, `modern_roads`,
`historical_roads`, `waterways`, `rail_ink`, `rail_corridors`; other targets are
`buildings`, `institutions`, `review_candidates`.

Reviews are carried as an explicit overlay, without changing original uncertainty
flags or removing provenance. `retain_candidate` never enables traversal or proves
a historical fact; consumers must honor exclusions before admitting simulation
features. The shipped review list is empty: this cycle does not invent reviews.

## Atomic publication and boundaries

Compilation validates a captured snapshot before opening an output transaction.
The complete file is written to a temporary sibling, flushed/fsynced and renamed
atomically. Immediately before rename, source inventory and owned-input hashes
must still match the initial lock. A changed/invalid/stale input or failed write,
fsync or rename leaves the previous generation intact and removes the temporary
file. This promises atomic replacement, not power-loss durability of a directory
entry. Concurrent readers see a complete previous or new generation.

CLI output is confined to JSON files beneath this checkout's real `build/content`
directory; escaped paths and symlinks to outside inputs/outputs are rejected before
source auditing. Metadata references are never used as arbitrary file paths.

## Evidence and runtime boundary

`tests/test_content.py` uses small real-shape reference fixtures for malformed
geometry, IDs, links, flags, provenance, reviews and schema checks; temporary output
fixtures inject stale input and filesystem failures. Neither real submodule is
modified. `content/cycle-08-validation.json` records the contract-test count, full
real-input compilation repeated byte-for-byte, CLI freshness check, generation
hash, artifact size/counts and preserved institution uncertainty.

Cycle 08 itself qualifies the offline compiler/artifact only. Cycles 09–13 now
extend it with the `CanterburyCity.bin` pack, immutable C loader and native-backed
city runtime. See `GEOMETRY.md`, `MOVEMENT.md`, `ROUTING.md`, `POPULATION.md` and
`HOUSEHOLDS.md`. Those cycles have separate native-backed acceptance evidence;
a successful source compilation alone does not establish runtime behavior.
