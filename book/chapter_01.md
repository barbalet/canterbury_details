# Chapter 1 — Building an Ordinary City on ApeSDK

## Two stages of development

Canterbury Details turns a historical map and a native artificial-life engine
into an ordinary human-city model. The work begins with a practical separation:
the map supplies candidates and evidence, ApeSDK supplies bodies and shared
simulation machinery, and this project supplies the meaning of homes, jobs,
services and institutions. Its people are ordinary humans. Religion appears
through community membership, occupations and organizations, with supernatural
rules reserved for a separate consumer of the core.

There are two development records to read. The original forty-cycle foundation
completed its standalone release gate, including native presentation, operations,
checkpoints and an independently linkable core. Subsequent work adds the
Canterbury scenario language, broader careers, refined inventories and funded
city trials. The [DSL plan](../docs/canterbury-dsl/PLAN.md) records completion
through cycle 17; authoring workflow, renewed scale qualification and the final
DSL release remain cycles 18–20. Foundation completion does not close that later
programme.

This distinction also explains apparently conflicting descriptions of startup.
The legacy base population can remain unplaced and unemployed. Explicit access
and funded-duty scenarios now allocate homes, jobs and school attendance on the
actual map. Both statements describe implemented configurations. Reading a
successful trial as the default launch policy would erase an important part of
the model's contract.

## Making ownership explicit

The [architecture](../docs/ARCHITECTURE.md) assigns each authoritative field to
one owner. Native bodies hold physical position and energy; native elapsed time
drives the city. Project records hold civil identity, households, contracts,
health, resources and institutions. Rendering and inspection receive copied
views. A displayed coordinate or employment description is therefore an
observation of owned state rather than an additional place where that state
can be changed.

`CdContent` admits immutable inputs, `CdWorld` owns a running ordinary city, and
snapshots provide observations with defined lifetimes. The public
[`cd_api.h`](../src/cd_api.h) avoids exposing native SDK structure layouts.
Clients can request operations, inspect results and advance the world through
this boundary. They cannot safely keep pointers into the native population or
privately edit arrays discovered during an inspection.

The world coordinator orders transactions across modules. Movement, employment
and resources contribute intentions or validated effects; they do not each run
an independent simulation. This matters when a death releases a bed, a shift
loses its worker or a cancelled purchase returns money. A coherent result
requires all affected owners to agree at one publication boundary. The same
ownership map makes persistence auditable: every pointer and mutable field must
have an explicit serialization or reconstruction rule.

## Scaling bodies while preserving identity

This checkout's SDK profile starts with 27,000 ordinary people and capacity for
30,000. The extra space supports bounded admissions; it reserves no special
character. All linked host and SDK units use `CANTERBURY_SIM` and `BRONZE_SIM`
consistently. These definitions affect native group layout, so mixed-profile
objects cannot be treated as compatible merely because the linker accepts them.
The [SDK integration contract](../docs/SDK-INTEGRATION.md) records the limits.

A Bronze actor ID identifies a native being within its owning registry. Native
array slots only locate bodies. Compaction after retirement can move a body to
another slot without changing the person's identity. The city adds civil records
and world-qualified references, allowing archived people to remain inspectable
after they no longer occupy a native slot. Names are display data and can repeat.

Runtime references include a world token. Restarting or loading replaces that
namespace, preventing an old selection or queued borrow from silently resolving
against another world. Persistent state uses logical IDs that are validated and
rebound on load. Capacity rejection likewise preserves the active registry and
world. Scaling is consequently more than increasing an array bound: it includes
lookup, retirement, memory ownership and failure behavior at the population
limit.

## Letting one minute govern the city

The guarded city callback replaces the island's autonomous update path.
`cd_world_step` delegates advancement to native `sim_cycle`, while the project
maps native elapsed minutes to its civil calendar. The date begins from the
configured 1940 origin; the interface's initial binding minute is documented.
Wall time controls how often a client asks to step, never how much unrecorded
work a person has accomplished.

During a minute the coordinator plans survival, care, travel and ordinary work,
integrates movement through the city policy, and consumes the shared native
work budget. Completion and lifecycle effects publish in a stable order. A
worker walking to a building cannot also receive a stationary production minute
there. A caregiver occupied with a dependent cannot spend that same grant on
a second task.

This ordering makes batch size a testable implementation detail. Advancing a
day in one call should match advancing the same number of individual minutes
from the same checkpoint. Read-only inspection consumes neither minutes nor
simulation random draws. Pausing, searching or taking a snapshot therefore does
not alter what the resumed simulation will do.

## Admitting a map with its uncertainties intact

The Canterbury submodule contributes candidate geometry, modern enrichment and
institution annotations. The city compiles those into immutable, fingerprinted
content. Hashes identify the source revision and admitted policy; stale review
inputs cannot quietly qualify different geometry. The map-processing tools
retain original evidence separately from corrected building placement and
consumer-specific runtime decisions.

The shared correction layer clears buildings from displayed streets and gives
rendering and runtime geometry the same source basis. A selected footprint,
its outline and its collision representation should describe the same admitted
shape. Nevertheless, a useful drawing does not establish an entrance, legal
access, dwelling capacity or a historically correct employer. Those require
explicit content or scenario qualification.

Later building-use estimation covers 1,790 previously unknown uses. Its
neighbour-based policy records anchors, alternatives and confidence labels,
and applies provisional uses to runtime metadata while retaining the original
unknown source values. Guesses do not propagate as new evidence. Suggested
occupations remain suggestions until a scenario provides eligible people,
premises, funding and actual assignments. The
[estimate documentation](../docs/BUILDING-USE-ESTIMATES.md) explains the method;
the later DSL plan records the additional allocation work that followed it.

## Turning geometry into physical access

Movement uses qualified street links, building entrances and explicit access
state. A route is an intention with progress; the native body remains the
physical location authority. The city integrator moves that body along admitted
geometry once per interval. Assigning a destination, home or job does not itself
transport anyone there.

This distinction lets failures remain meaningful. A closed door can suspend a
journey. An unreachable workplace can reject an assignment. A route interrupted
between nodes must respect its existing legitimate leg before changing direction.
Capacity and reservations account for people entering buildings instead of
allowing every request to assume that an empty-looking polygon has unlimited
room.

The opt-in access trial qualifies real-map connections and residential and
workplace capacities. Its assumptions are scenario inputs, not a recovery of
every historical doorway. Transport qualification is also specific to its
scenario: the later funded railway fixture admits reviewed connections between
Canterbury West and Sturry, while general reference rail geometry does not
automatically become an operating network. The latest completion record in
the DSL plan supersedes earlier notes that rail operation was still pending.

## Giving households civil meaning

The foundation's default fixture contains 6,750 synthetic households for its
27,000 people. Household membership, guardianship, residence and legal property
are separate relationships. A person can belong to a household before that
household receives qualified housing. The model keeps that lack of placement
visible instead of inventing a coordinate to make the map look populated.

Configured births and admissions use available native capacity and the same
stable identity machinery as existing residents. Guardian and housing conditions
remain independent requirements. Retirement archives the civil history and
coordinates removal of native bindings and live commitments. Resource custody,
care, employment and route references must be reconciled during that operation;
deleting an array entry alone would leave obligations attached to a missing
person.

Later population startup packs introduce explicit age cohorts and guard against
placing minors in households without a suitable adult. The 27,000-person
scenario distinguishes young children, school-age children, adolescents,
working-age adults and older residents. These numbers are authored choices.
They support reproducible experiments in provision and scheduling without
claiming to reconstruct a census. The home allocator stages all assignments
before publishing, so a failed capacity or access check cannot leave a partly
allocated city behind.

## Needs and health as finite transactions

City needs interpret the native energy field through explicit technical rates.
Meals consume finite portions; sleep and rest change fatigue; travel and work
compete with survival and care. There is no second human-energy pool whose
value can drift away from the body. Initial carried food is a bounded fixture,
and missing provision creates unmet demand rather than automatic replenishment.

Young dependents require a capable, physically co-located guardian for supported
feeding. Planning all residents' needs before resolving caregiver actions avoids
an ordering bug in which the guardian's later update overwrites the care already
assigned to them. Eating, severe illness and active care can suspend travel or
productive work. The abandoned intention retains the state needed for legitimate
resumption.

Health experiments introduce explicit conditions with bounded severity and
duration. Clinics require configured beds, medicine, qualified carers and
physical presence. Treatment consumes resources and native grants; mortality
cleanup commits at minute boundaries. The rates are model parameters rather
than historical incidence or medical predictions. Unplaced residents' unresolved
physical provision is kept distinct from an invented historical famine. These
rules make care measurable while preserving the limits of the source evidence.

## Employment needs more than a role name

An occupation describes capabilities; an employer, funded vacancy and shift
make an actual job. Premises must have admitted capacity, the worker must meet
eligibility requirements, and the workplace must be reachable. Assignment does
not create attendance, move a body or fabricate productive output. Ordinary
commuting uses the same movement system as every other journey.

Apprenticeship adds a real dependency on another person's time. The mentor must
be qualified, available and co-located where the task requires it. Losing that
relationship can cancel dependent work and release outstanding reservations.
Secondary duties require compatible schedules; overnight shifts change the
worker's sleep opportunity instead of assuming that all work occurs in daylight.

The later DSL integration supplies named careers and compiled occupation IDs.
Funded startup jobs bind explicit entry roles and technical assessments. Roles
requiring independent licensing or appointment retain their separate gates.
This prevents a generic assessment from becoming permission to occupy every
professional or religious office. The same distinction is visible in inspection:
a definition can exist, an actor can qualify, a vacancy can be funded, and yet
no work occurs until assignment, arrival and a usable native minute coincide.

## Reserving effort and committing work once

Productive work begins by validating inputs and reserving materials and wages.
The actor supplies a monotonically increasing request key. Repeating the latest
identical request returns its existing result; a conflicting or stale retry
fails. This is important for both native controls and future frontends, where
a delayed response should not make a second click purchase the same work twice.

Each granted minute pays earned wages once. Completion consumes the reserved
inputs and publishes outputs once. Cancellation returns unused materials and
unpaid wages while retaining completed work and transfers. A closure or absent
mentor can preserve progress without fabricating labor during the interruption.
The [work contract](../docs/WORK.md) describes these boundaries.

Apprentice and mentor readiness is checked before paired claims, so the system
does not take one person's minute and then discover that the other cannot
participate. Output capacity is also checked before the final effect publishes.
Bounded, preallocated records keep request publication from introducing an
uncontrolled allocation path into the native callback. The resulting work model
connects physical presence, time, money and goods in one observable transaction.

## Conserving the ordinary economy

The foundation economy uses explicit stocks and technical currency tokens.
Imports require a recorded basis, production consumes named inputs, and wages
move existing funds. Household, person and employer accounts have different
purposes. Owning money is not the same as holding goods, and reserving stock
does not create another copy of it.

Goods must move through physical custody. Household purchases require an
eligible adult at the seller, stock and a shared minute. Delivery and collection
operate through carrying limits and qualified premises. Cancellation leaves
already moved items with their current owner rather than teleporting them back.
Retirement and departure have explicit accounting paths for possessions and
exports.

Conservation equations connect imports, production, live stock, reservations,
consumption and exports. They make errors such as duplicated wages or vanished
reserved materials detectable even when a visual simulation looks plausible.
Development uses the same discipline: a repair or capacity change consumes
materials and labor and must leave valid occupancy and workplace relationships.
Changes are scenario overlays on immutable source geometry. A newly usable
workshop is therefore a model event with a cost and provenance, not a rewrite
of the historical map.

## Institutions, services and transport

Religious affiliation, institutional denomination, operating status, membership
and office holding remain separate facts. A nearby church does not assign a
resident's beliefs, and a source label does not create staff or supplies.
The Old Synagogue's scenario override remains identifiable. Ordinary worship,
education, charity and visitation use actual premises, staff, schedules and
finite resources.

Gatherings reserve staff wages and bounded attendance places. Participants
travel to the venue and consume their own shared time when credited. A missed
window is not backfilled after the event. Civic responses similarly require
qualified rosters, equipment and physical arrival. Fire response needs tools
and water; clinical service delegates to the existing bed and medicine model.
Burial administration records ordinary service without claiming that paperwork
physically moved a body.

Transport adds vehicles, crews and unique passenger or cargo custody. A carried
person still has one native body and cannot independently walk or work while
in custody. Stops, cancellations and crew replacement preserve physical state.
Later named road, rail, postal and hospitality fixtures exercise these contracts
through funded posts. Their operating assumptions remain explicit even where
the underlying geometry comes from the real Canterbury map.

## Operations and causal records

The ordinary management interface exposes 45 operation verbs through a bounded
request system. Observation is the default; intervention permission enables
preview, queue, cancellation and application. Preview checks present conditions
without reserving stock or promising future success. Execution validates again
because an entrance may close, a worker may leave or another request may consume
the available funds.

Requests run in insertion order at the documented boundary. `flush` processes
due requests at the current closed minute, whereas a zero-minute step performs
no work. Repeated keys identify the same operation. Results retain status,
reason and the task or entity produced, so failure can be explained without
silently reporting success.

Causal records connect ordinary actions to outcomes and provenance. They are
bounded records with documented observation scope, not an unlimited omniscient
history. Read-only exports and snapshots expose what their contracts permit.
Together, operations and records give an independent frontend a useful control
surface: it can explain a failed service request, inspect a reservation and
follow an accepted action without accessing private model arrays or advancing
a second clock.

## Moving from a catalogue to a scenario language

The Canterbury DSL began with 300 draft roles and grew into typed authoring,
semantic compilation and pack publication. The compiler checks declared fields,
IDs, bounded inputs and references to the actual city pack. Canonical identities
bind the schema, occupation and resource catalogues, operations and admitted
map. Changes in comments or declaration order do not create a different logical
scenario.

The [compiler guide](../docs/canterbury-dsl/COMPILER.md) distinguishes definition
packs from the population startup profile. A compiled declaration is not, by
itself, proof that the full authored scenario can activate. Missing funded posts,
facilities or executable bindings remain diagnostics. The bundled draft retains
explicit activation blockers while named native fixtures exercise implemented
behavior.

Population startup records carry exact cohorts, date, seed, map identity and
scenario identity. The CLI and AppKit loader validate them before replacing a
world. Access and duty startup inputs add reviewed homes, employers, school
enrollment and funding through separate admission steps. This staged approach
lets the implementation qualify one boundary at a time while preserving a
clear difference between an authored idea, an admitted configuration and an
executed, measured city day.

## Careers, materials and services in the later cycles

Career progression now connects experience, supervised curricula, credentials
and funded vacancies. Training consumes real mentor and learner time. An
independent credential records the authorization appropriate to a role rather
than treating accumulated experience as universal permission. Advancement
revalidates prerequisites and commits atomically, allowing a prepared promotion
to be reproduced after checkpoint restore.

Refined resources extend abstract goods with declared units, tagged lots,
quality, optional expiry and equipment condition. Keyed reservations and cargo
requests operate on stable lot IDs. Aggregate views still total each kind, but
the underlying lots retain distinctions needed for consumption and wear.
Recipes account for material balances, water or counted components as their
definitions require. Paid purchases and carried-food consumption join these
stocks to the same native-time and financial rules.

The later acceptance records cover 28 compiled recipes, all 300 roles and
21 service declarations through eligible funded fixtures. That coverage should
be read alongside the [role traceability inventory](../docs/canterbury-dsl/ROLE-TRACEABILITY.json).
It does not mean every resident automatically performs every occupation, or that
all fixtures are historical reconstructions. Temporary venues, technical
qualifications, explicit demand and imported supplies remain disclosed parts
of the experiments.

## What a funded city day demonstrates

Cycle 16's actual-map scenario runs 27,000 residents through 1,440 native
minutes. Its 12,000 workers and 4,050 pupils complete scheduled attendance and
return home. Twelve production orders and three service requests complete
during that day. The services wait for physical arrival and active shifts,
then use the appropriate payroll and experience path.

The verifier reconciles 5,760,000 currency units in wages and requires no
outstanding wage escrow at the end. It checks ordinary and refined inventories,
food and water accounting, and inspection purity against the complete world
digest. Separate district fixtures expose conflicts that aggregate totals could
hide: active milling takes precedence over the same worker's mentoring duty,
and training resumes after the order completes.

These results establish behavior under an explicit funded trial. They do not
establish a self-sufficient historical economy or make the earlier default
unplaced population disappear. `make access-check` is the entry point for the
combined trial, while the plan records its assumptions and acceptance details.
The value of this milestone is causal continuity: departure, arrival, attendance,
payment, production and return are consequences of one running world.

## Saving the entire causal state

Complete checkpoints coordinate native bodies, land, timing, actor registry
and every owned city array. Version 2 also binds the compiled DSL catalogue and
startup scenario identity. Active travel, training, production reservations,
inventory lots, services and promotion prerequisites must survive together;
restoring only positions would lose the obligations that explain the next minute.

Save writes a temporary compressed stream, closes and synchronizes it, then
atomically replaces the target. Load validates compatibility, checksums,
references, accounting and the authoritative digest in staged storage before
publication. Failure preserves the live world. Successful replacement issues
fresh runtime reference tokens and does not run startup or consume an extra
simulation minute.

The [persistence contract](../docs/PERSISTENCE.md) is deliberately strict about
format, content, SDK source and ABI identity. Version 1 requires its originating
build; cross-architecture conversion is not implemented. Cycle 17 verifies
separate-process continuation of funded commutes, reserved production and queued
services, together with batch-versus-single-minute replay and transactional
rejection of incompatible or truncated files. Compatibility is consequently a
tested admission rule, not an assumption that any file named a city save can
be resumed by any later executable.

## Following one production order through the source

A practical way to read the implementation is to follow a single order rather
than opening every module in directory order. Begin with the compiled recipe
definition and identify its required role, inputs, equipment and work duration.
Then inspect the assigned job and shift, the worker's qualifications, the actual
premises and the accounts funding the task. Each relationship should resolve
through stable IDs and admitted content before any stock is reserved.

Next follow the request into work arbitration. Observe the difference between
accepting an intention and earning its first minute. The body may need to
commute, collect material, eat or wait for a mentor. Inspecting the request at
each point should reveal the same physical and financial facts as the relevant
owner's views. When a grant finally arrives, the corresponding progress and
wage transfer should explain one another.

Finally, interrupt the order with a closure or checkpoint and inspect the
remaining reservations. Resuming should continue the existing obligation;
cancelling should return only the uncommitted portion. The work, economy,
movement and persistence modules are easier to understand when approached
through this concrete chain of effects. It also provides a useful review method:
an implementation that cannot explain which owner committed an effect probably
needs a clearer transaction boundary.

## Reading evidence without flattening the history

The repository contains cycle plans, design contracts, generated inventories,
verification scripts and recorded results. They answer different questions.
A plan states intended behavior; a completion note identifies what was achieved;
a verifier defines the assertions used; a result binds those assertions to
particular inputs. A filename containing a cycle number is not sufficient
evidence that the current working tree has passed that cycle again.

For example, the initial architecture describes forthcoming work arbitration
and persistence. Later foundation records implement both. The early building
estimator describes a proposal for allocating jobs; the later access and duty
trials execute allocations under explicit qualifications. The compiler guide
likewise retains early notes about reference-only rail while the cycle 14
completion record documents a reviewed operating fixture. These are changes
in the implementation's history, and should be read in that order.

When changing a policy, start from the current owner and its tests, then identify
which evidence depends on that policy or content hash. Regenerate only the
relevant artifacts and rerun the appropriate acceptance path. An offline map
audit cannot establish paid attendance, and a funded work fixture cannot
establish native UI responsiveness. Keeping those claims separate gives later
maintainers a reliable way to decide what a change invalidates.

The same approach applies to reproducibility. Record the parent and submodule
revisions, scenario identity, map generation and executable profile alongside
the seed and workload. Two runs with equal seeds but different qualifications
or content are different experiments. Comparing their totals without those
inputs could misattribute a changed route, wage or service outcome to randomness.

## Presenting and reusing the model

The AppKit client keeps native work on `CitySession`'s serial worker. Map
navigation and controls remain responsive through snapshots rather than direct
array access from the UI thread. Stable-ID selection avoids changing the
inspected person after compaction. Unplaced residents remain explicitly
nongeographic roster entries; placing a mark in that roster does not give its
person a city location.

Command-M opens ordinary management. The CLI and native app share source
definitions, content and core behavior, while `make core` packages
`libCanterburyCore.a`, public headers and resources for an independent frontend.
That consumer can link without Cocoa or either application entry point. A later
game should reuse this population and clock and keep its companion rules,
random streams and saved state under explicit ownership.

The original release evidence includes isolated SDK regressions, native builds,
runtime and failure suites, core linking and measured population workloads.
Those measurements describe their qualified source and machine. They are not
fresh measurements of the later DSL implementation. The remaining DSL cycles
must renew authoring, presentation, performance and release evidence against
the final scenario. This book therefore ends at the implemented boundary:
a reusable ordinary city with a substantial, replayable scenario extension,
and clearly identified qualification work still ahead.
