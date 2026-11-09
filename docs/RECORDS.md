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

## Native eating and conversation events

Native activity integration is tracked in [EVENTS_PLAN.md](../EVENTS_PLAN.md).
A finite Canterbury meal now creates native `EVENT_EAT` with the prepared-food
code and a personal `CD_EVENT_EAT` record. A successful conversation creates
paired native `EVENT_CHAT` memories and two personal `CD_EVENT_CHAT` records,
using the residents' civil names in city history. Completed production and
physically delivered services also use existing native profession memory codes.

The local-chat opportunity is an explicitly schematic policy: once per native
hour, consider at most eight following residents in stable-ID order. Each may
chat at most once, only when both are awake, at least six, healthy, idle, known to
be physically co-located and have unused native action budgets. This is bounded
O(8N), consumes each participant's real native minute grant, and infers neither
positions nor conversations for unplaced people. It establishes no automatic
friendship, congregation or historical interaction frequency.

The resident inspector reads retained native activity memories even if the global
city history has evicted that resident's earlier entries. This requires the same
live personal observer or explicit diagnostic access. It separately shows named
conversations from retained personal city records. Native aliases may repeat in
Canterbury, so a lost city record is never reconstructed by guessing identities
from native name bytes: the subjective memory says a conversation happened,
while named counterparts are shown only where the city record retains them.
The Bronze actor inspector shows copied native episodic descriptions. Native
subjective memories use the existing 12 slots and forgetting; city causal records
use their independent 8,192-entry bounded observer history. Both persist through
existing native/body and city checkpoint paths. Incompatible old checkpoints are
rejected by the existing ABI/content gate rather than silently reinterpreted.
