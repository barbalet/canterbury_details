# Source topology and route qualification — Cycle 11

The runtime graph imports modern-road, historical-road and road-ink polylines
without connecting those three source namespaces. Coordinate coincidence is not
a junction. Edges retain source endpoints and every geometry vertex. Qualified
components use only explicitly admitted edges. A shared node joining distinct
grades requires an explicit junction/ramp review. Distinct nodes at the same
location remain distinct. Every qualified edge is checked against building,
water and rail geometry; bridge/crossing exceptions are per edge.

`content/spatial-review.json` is the per-link ledger, locked to source-generation
and input hashes. A review names network, edge, basis/note, capacity, grade,
junction, bridge and rail crossing. The checked-in source ledger is deliberately
empty: all 16,279 source candidates remain unqualified. This preserves upstream
uncertainty; it is not evidence that historical Canterbury lacked walkable roads.

Deterministic Dijkstra searches existing adjacency, with source-node ID tie order.
Cost is source-polyline length multiplied by `1 + load/capacity`. Active edge
occupancy and explicit background crowds contribute to routing load; saturated
links and closures are unavailable. Walking also slows under background load.
Cancelled bodies still occupy a link while physically on it. A changed restriction
causes replanning at the next actual node or a wait on the affected link. Reopening
allows continuation. No endpoint snapping, fabricated connection or straight-line
fallback occurs. A bounded 128-edge result fails clearly instead of truncating.

Reasons distinguish unknown endpoint, unqualified links, disconnected components,
closure, crowd, geometry/precision, route limit, unplaced person and ordinary
access. Closure/crowd conflict reasons describe a blocking condition, not a proof
that it is the only obstruction. Technical fixtures cover direct reachability,
source-polyline detours, disconnected pairs, unqualified endpoints, grade rejection,
full crowd capacity, mid-link closure and recovery. These fixtures do not qualify
any edge of the actual map.
