# Causal records and ordinary social continuity

Implemented in Cycle 29. `cd_observer.h` exposes copied records, scoped reads,
explicit observations, social context and stable logical-ID resolution.

Each record contains a monotonic ID, closed native minute, causal parent ID,
event kind, actor/subject stable IDs, entity/household, privacy, explicit basis
and copied message. Parents precede children. IDs are never reused. The preallocated
8,192-record ring evicts oldest entries, reports dropped/first/next IDs, and returns
unknown for evicted records. A retained child may therefore name an evicted parent;
this is an explicit historical gap, never a fabricated reconstruction. ID exhaustion
sets saturation and prevents further queued interventions.

Public records are visible to observers. Household records require a live viewer
in that household; personal records require the same actor. Diagnostic scope is
an explicit local administrator request and includes all records. Export defaults
to public scope. This is model-level visibility, not encrypted storage or OS account
authorization. An observation copies only an already visible record into the
observer's personal history, retaining the source ID and supplied observation basis.
Snapshots retain their own record copies and old reference namespace.

Retirement records cause released-job records; monitored job transitions cause
service interruption records. The staffing-loss acceptance fixture walks that
chain after a worker dies. A household survivor receives a household-private
bereavement record and a one-day (1,440 native minutes) ordinary support context.
This duration is a labeled technical parameter, not an inferred psychological
rate, diagnosis, new energy pool or forced behavior. Existing care/community
services remain finite ordinary activities. Archived people cannot train, travel,
work, observe or receive a fresh live reference.

Transition cursors cover work/health, jobs, development, premise disruption,
institutions/offices, dispatch, gatherings, transport and scripted pressures.
They record observed phase changes, not every native minute or every internal
substep. No historical event/staffing/faith is synthesized from missing source data.
See `tests/observer.c` Cycle 29 and `content/cycle-29-validation.json`.
