# Geometry and ordinary access — Cycle 09

`compile_runtime.py` packs the audited Cycle 08 generation into
`build/content/CanterburyCity.bin`. It preserves rings, holes, source identifiers,
confidence, modern suggested uses and institution uncertainty. The immutable
runtime owns those bytes and decoded records; worlds and snapshots retain it.
Stable numeric entity IDs are namespace-qualified FNV-64 values with collision
rejection, not array indices. Source geometry remains in source pixel units.

The little-endian CDCT0002 pack has a byte length and CRC32, bounded counts,
finite coordinates, UTF-8 strings, references, explicit review flags and a
64-character source-generation identity. CRC detects corruption, not authenticity.
The offline compiler binds source bytes to the audited manifest and Cycle 08
artifact evidence. Native admission independently rejects bad references,
duplicate IDs, obstructed qualified paths and unsafe connectors before publication.
A failed load frees every staged allocation and leaves the caller's output empty.

Building picking uses even/odd rings and retains source confidence. Courtyard
interiors are not occupied floor; their walls remain barriers. Segment clearance
checks every relevant ring, endpoint and intersection. Water and rail are barriers
unless a particular reviewed edge explicitly has bridge or rail-crossing access.
An entrance begins at an existing exterior road node, ends on floor, crosses
exactly one exterior wall and crosses no courtyard wall, other building, water or
rail. Candidates pass geometry validation but remain disabled until reviewed.

Access uses residence, employment context, known opening intervals (including
overnight), locks/keys and authorized emergency access. No religion field enters
access decisions. Employment assignment belongs to the later job cycles; current
access callers must supply that context explicitly. Emergency access does not
qualify a door or bypass geometry. Dynamic world locks are separate from source
policy. Unlocated institutions remain unlocated.

`content/spatial-review.json` is the review queue. Its default lists are empty:
3341 footprints are candidates, not 3341 known dwellings. Each admitted edge or
dwelling review cites the input hash, source ID, basis and explanation. Doors cite
building/node IDs and explicit connector points. Changing the Cycle 08 generation
invalidates the review file. `technical-fixture` is an honest model label, never
historical evidence. Add reviews individually; never globally enable candidates.

Run `python3 tools/verify_city_cycles.py --run`. Fixtures cover holes, forbidden
water/rail, bridge exceptions, candidate doors, wall-cut rejection, confidence,
corrupt bytes, duplicate IDs, unknown endpoints and grade joins.
