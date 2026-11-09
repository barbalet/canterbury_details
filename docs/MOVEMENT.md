# Native pedestrian movement — Cycle 10

The native ApeSDK body is the only position authority. `cd_native_position` and
`cd_native_write` resolve a world-token/person-ID binding before reading/writing
that body. Civil records contain a position-known flag; view positions are copied
from native state only. Initial native body coordinates are not asserted to be
city locations. `cd_world_place` performs explicit initial admission on a qualified
source node and refuses every second placement.

Scale is `65535 / max(canvas width, canvas height)` native coordinate units per
source pixel. The walking budget is eight native units per minute, derived from
ApeSDK's `FATIGUE_SPEED_THRESHOLD`, a disclosed technical model choice rather than
a historical speed or metres-per-second claim. No unsupported geographic scale
is inferred. Movement rounds toward its starting position and tests the actual
quantized segment, so each native minute spends at most that budget. Quantized
arrival tolerance is one native unit. Obstruction at that precision yields an
explicit geometry wait rather than a wall shortcut.

A native-minute callback claims the shared Bronze work grant exactly once before
spending a travel budget. Native city mode disables island autonomy, and city
bodies begin with native speed zero. There is no second UI integrator or ticking
clock. Source polylines are traversed vertex by vertex, in either direction;
reviewed entrance connectors use the same budget and validate each wall crossing.
A trip has at most 128 edges. The per-minute vertex loop is also bounded.

Stop/cancel preserves the native point and occupied edge. Resuming the same trip
continues in place. A different destination or movement preview requires an actual
source-node boundary; it cannot rejoin by teleporting. Closures encountered mid-link
wait in place; node-boundary restrictions can trigger a qualified detour. Entry
requires physically reaching the exterior endpoint; exit requires the interior
endpoint and the matching building. Ordinary lock/hour policy is checked again
each minute. Door interruption retains position; repeating the same entry/exit intention
resumes its connector in place. A new endpoint intention cannot skip it.

The public MOVE/node preview reads state and uses scratch routing arrays, consumes
no RNG or native time, and publishes no intention. General scheduling and other
operation kinds belong to later cycles and return unsupported now. Save/load remain
reserved for their persistence cycle. Immutable snapshots outlive their worlds.

Native-backed fixtures verify paused steps, bounded displacement, exact native
position agreement, interruption/resume, arrival, keys and continuous door travel.

Cycles 14–18 preserve this `CANTERBURY_SIM` integrator: needs/care can preempt a
travel minute without changing position or releasing occupied street space. Food
and clinical actions require physical arrival via street polylines and reviewed
entrances. They never substitute a building ID assignment for native movement.
The integration fixture demonstrates exit, street traversal, entry, collection,
return and household delivery between two separate footprints.
