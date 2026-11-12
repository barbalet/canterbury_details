# Canterbury DSL implementation plan

Implement a runnable Canterbury 1940 scenario from the [DSL draft](../../content/scenarios/canterbury1940-draft.canterbury). The release target includes 60 career families, 300 roles, 240 advancement links, 72 resource kinds, 21 services and the five draft recipes. The scenario starts with 27,000 residents and supports growth to 30,000. It runs through the CLI and native app using the existing Canterbury map, native bodies and simulation clock.

The [draft specification](../CANTERBURY-DSL-DRAFT.txt) and checker are the authoring baseline. The current runtime supplies placement, jobs, training, services, inventories, operations and checkpoints. The new work connects and extends those systems through a versioned scenario compiler. Cycles 01–05 cover compilation and native population startup. Cycle 06 has exact age cohorts and guarded households, with activity schedules still open. Cycle 07 has passed actual-map housing and entry acceptance in the technical access trial. Cycle 08 is complete for the opt-in citywide startup, funded shifts and recurring school sessions. Cycle 09 is complete for catalogue inspection, role qualification and funded-job binding. See the [compiler guide](COMPILER.md) for the ownership inventory, schema migration and build commands.

The current compiler qualifies draft parsing, typed binding, deterministic publication and activation preflight. Runtime execution begins in Cycle 05.

Cycles 09–12 have a compiled definition layer. The CLI can inspect all 60
families, 300 roles, 240 links, 72 resource kinds and 29 production recipes by
stable ID. The existing eight enabled occupations retain their IDs. The
compiler rejects invalid mentor stages, duplicate stock rows and equipment
references to non-equipment resources. Each recipe now has a labelled manual
trial with finite inputs, outputs, equipment and labour minutes.

Cycle 10 is complete for supervised training, independent qualifications and the 240 declared transitions. Cycle 11 is complete for refined stock and batch custody. Cycle 12 is complete for the compiled production recipes and funded supply-chain trials. Cycle 13 is complete for the 155 trades, production and commerce roles.
Native premises stock supports finite imports, reservations, consumption,
condition-rated equipment and native hand-cargo custody. The five original
recipes and three additional trade trials have native execution in staffed
fixtures. Activity work remaining in Cycle 06 still applies to actual-map integration.

Cycles 13–16 have an initial production integration. The existing carpenter
and weaver posts can execute the timber and cloth trials through native work.
The job reserves inputs, tools and wages, waits for physical arrival and the
assigned shift, then commits the products and offcuts. Cancellation returns
unconsumed materials and tools. The qualification uses the existing reviewed
technical workshop fixture. Hand cargo uses native pickup and delivery grants
and preserves custody through street travel and checkpoints. The remaining
trade, transport, hospitality, public
and professional roles, and full city integration still need implementation.



Each cycle closes when its exit check passes on the current source and pinned dependencies. Use small deterministic fixtures for edge cases and the actual Canterbury map for integration. Document assumptions as scenario policy, retain source provenance, and record any draft entries that require a revised design before advancing the dependent cycle.

## Cycle 01 — Inventory and traceability

Map every declaration, field, role, transition, resource, recipe and service to its compiler representation, runtime owner and acceptance case. Audit the current `cd_*` entry points, fixed limits, checkpoint sections, app build resources and SDK profile guards. Record baseline tests and performance budgets. Use the inventory to track implementation through release.

**Exit.** Every draft element has an owner, a dependency and a concrete acceptance check. Any feature split across city and SDK has a single authority for its state.

## Cycle 02 — Executable schema and migration

Version the executable language and define migration from draft 0.1. Specify typed declarations for households, employers, funded posts, shifts, curricula, credentials, stock, recipes, services and task bindings. Define required fields, integer units, stable IDs, references and scenario provenance. Supply explicit, labelled technical values for unresolved wages, durations, yields and qualifications as their implementation cycles land.

**Exit.** The schema covers all draft concepts and defines the difference between an omitted value, zero and an intentionally inactive entry. Missing execution prerequisites produce actionable diagnostics.

## Cycle 03 — Parser and diagnostics

Build the production parser from the draft grammar. Add source spans, typed fields, duplicate detection, bounded input, checked numeric conversion and deterministic errors. Use the draft budgets of 8 MiB, 512 roles, 128 families, resources and services, and 512 recipes as initial authoring bounds. Reject malformed or unsupported declarations before runtime allocation.

**Exit.** The full draft parses, boundary and malformed fixtures produce expected diagnostics, and equivalent formatting preserves compiled meaning.

## Cycle 04 — Semantic compiler and scenario pack

Resolve references in a second pass. Validate units, capacities, cross-field requirements, ownership and career graphs. Bind supported task verbs to typed handlers. Produce a reproducible, versioned pack with exact content, schema, role and resource identities. Integrate its compiler into the local build and Xcode resource pipeline.

**Exit.** Repeated builds produce identical packs. Invalid input preserves the preceding pack. Activation rejects unresolved execution prerequisites and mismatched map content.

## Cycle 05 — Scenario loading and native lifecycle

Wire scenario selection through the CLI and AppKit session. Stage scenario and city state before publishing through the existing worker and world lifecycle. Use one native minute, body position and energy authority. Guard any necessary shared SDK extensions with `CANTERBURY_SIM` and qualify the existing native and Bronze profiles.

**Exit.** Both frontends load the same small executable DSL fixture, expose its identity and advance it identically. Failed replacement preserves the current world. Existing CLI programs and scenario entry points pass their regressions.

## Cycle 06 — Population and households

Apply the proposed age bands of 2,160 aged 0–4, 4,050 aged 5–14, 1,620 aged 15–17, 16,470 aged 18–64 and 2,700 aged 65+. Generate households and guardians jointly. Add explicit school, household care, retirement and other non-employment activities. Preserve the existing birth, death, admission and stable-ID contracts.

**Exit.** The seeded 27,000-person population matches every cohort total. Guardianship, names, household membership and lifecycle changes reconcile at both default and maximum capacity.

The native household-care designation is available to co-resident parents
and guardians without an assigned paid job. It reports overlapping stages
by dependent age: infant (under one), preschool (1–4), school age (5–12)
and teenager (13–17). Designated carers with dependents are unavailable
for paid job assignment until released. The designation is explicit and
uses the existing parent and guardian relationships.

## Cycle 07 — Homes, premises and walkable routes

Use the applied provisional building uses to generate candidate homes and workplaces. Define dwelling, school and work capacities with explicit simulation provenance. Qualify street links and door connectors against the actual geometry, including barriers and crossings. Allocate households deterministically, with separate home and work capacity for mixed uses.

**Exit.** The target scenario accommodates its households through valid premises and routes. Any shortage is reported and resolved through an explicit scenario capacity decision before release. Geometry and native movement checks reject blocked entrances and invalid shortcuts.

## Cycle 08 — Employers, shifts and workforce allocation

Build employers and funded vacancies from declared premises, budgets and shifts. Allocate the proposed 12,000 employed working-age residents across the nine sectors, using reachable routes, qualifications and capacity. Track the other 4,470 working-age residents by activity. Integrate home-to-work and return travel, school journeys and rest through existing scheduling and movement.

**Exit.** Published assignments reconcile with sector targets, employer funding, shift coverage and reachability. Assigned workers travel from home and back on the actual map. Interrupted shifts and closed routes have visible outcomes.

### Cycles 07–08 actual-map access trial

`make access-check` compiles and checks the opt-in Canterbury access pack
from the audited streets, corrected buildings and applied provisional uses.
The trial accepts clear surface walking links with eligible source tags.
Doors are technical single-wall connectors to qualified source nodes.
Each connector is screened against native coordinate rounding in both dry
walking directions. Bridges, tunnels and protected crossings need their own
qualification.

The trial qualifies 12,675 walking links and 1,590 entrances across 299
components. Residential capacity is 39,632 places under the two-floor
model. The opt-in workforce model uses two floors, with a third model
floor at commercial, civic and leisure premises. It assumes one person per
floor area unit, with a 128-person building cap. It reserves three quarters of
education premises capacity for pupils. It provides 18,175 candidate
workplaces and 3,461 school seats per session. Mixed-use premises still
require distinct residential and workplace capacity declarations.

**Cycle 07 housing and entry acceptance.** The native test assigns all
6,750 households and verifies that the 27,000 proposed residents, with the
exact age cohorts, complete physical entrance movement. The run completed
entry in 69 native minutes. Placement preflights all residents before
publishing source-street positions and entry intentions.

**Cycle 08 staffed district acceptance.** The native test finds a reachable
home and industrial premises on the actual map. It assigns an explicitly
qualified resident to a funded named milling post, completes paid production
and verifies the return journey. A two-vacancy batch with one eligible
applicant fails before any assignment is published. The workforce allocator
uses ordered applicants, current route reachability and shift payroll checks.

**Cycle 08 school journey acceptance.** A native fixture sends one child
from home to a reviewed education building and back at dismissal. The
outbound journey took 178 native minutes and the complete trip, including
time at school, took 433. The fixture supplies the schedule. It uses
the same native movement and door policies as the work commute.

**Cycle 08 citywide allocation.** A capacity match using the draft
premises types covers 9,397 of 12,000 jobs. The opt-in model can cover all
nine sector totals when commercial premises can host light bench work and
construction offices. These are simulation permissions. With households in
their current assigned homes, component-level matching lowers the upper
bound to 11,634 jobs and 2,618 of 4,050 pupils across two school sessions.
The joint planning check now assigns addresses to 6,750 intact households,
12,000 worker destinations across all nine sectors, and 4,050 school places
across two sessions. It prefers nearby premises within each connected
component and exports the proposed assignments to
`build/access-trial-placement.json`. The normal CLI loads the plan through the opt-in duty startup.

The opt-in pack uses the map registration of 3.9896 metres per pixel and
an assumed dry walking pace of 75 metres per simulated minute. It converts
that pace to 769 native coordinate units per minute. Native movement and
route previews use the same budget, and entrance qualification checks
rounded movement at that budget. Earlier pilot timings used the SDK
fallback budget of eight units per minute.

Swapping 119 workplace assignments within their sectors and walking
components resolves the native route length failures. All 12,000 workers
and 4,050 pupils have routes in both directions. The longest estimated dry
street round trips are 224 minutes for a worker and 136 for a pupil.
The opt-in pack models up to 128 concurrent walkers per qualified link.
With that capacity and staggered shifts, normal duty startup binds all
12,000 jobs across the nine sectors and enrolls all 4,050 pupils.
The 1,440-minute native day verifies on-time arrival, continuous
attendance and physical return home for every assigned worker and pupil.
All workers receive 480 model tokens for their first eight-hour shift.
The ledger conserves all 5,760,000 tokens imported for that day.
Sector jobs model paid attendance. Named production and service duties
are covered by the later cycles.

## Cycle 09 — Career catalogue and competence

Load all 300 stable role IDs, their 60 families and workplace requirements. Preserve existing occupation references through a versioned mapping. Store sparse transferable competence and completed experience, with explicit family rules. Give each role a usable task binding and measurable output through the subsequent domain cycles.

**Exit.** The compiler and runtime can resolve every role and inspect its eligibility. Old occupation references remain intelligible. Full-population memory measurements include career state.

The funded scenario binds 12,000 jobs to compatible named entry roles in
the nine workforce sectors. It supplies explicit synthetic assessments.
Independent licences and appointments, mentor requirements, and
proprietor authority remain separate qualification checks. The full
catalogue is resolvable in both directions between stable DSL names and
the existing numeric job mapping. The compiled definition identity
versions that mapping. The runtime records completed experience in
bounded per-person records. Family totals combine experience only across
roles within the same family and confer no automatic promotion.

**Cycle 09 acceptance.** The full native day resolves and inspects all
300 roles. All 12,000 funded workers remain eligible, complete their
shifts, receive full wages, and record 480 minutes of role and family
experience. All 4,050 pupils also complete their sessions and round
trips. The 30,000-slot runtime reserves 3,840,000 bytes for bounded
competence and experience records. Peak owned allocation was
2,423,133,213 bytes. Task and service bindings continue in later cycles.

## Cycle 10 — Training, qualifications and advancement

Implement curricula, training posts, supervised minutes, mentor capacity, skill assessments and credentials. Enforce age, supervision, independent qualifications and tradition-specific appointments. Implement all 240 candidate transitions with consent, funding, a destination vacancy and any required ownership or appointment. Commit transfers atomically.

**Exit.** Each transition has an executable success fixture and a rejected prerequisite case. Mentor loss preserves earned evidence, failed advancement preserves the current post, and ownership and credential checks have independent coverage.

The compiled transitions use authored minute requirements where available.
Unset requirements use technical defaults of 60 supervised minutes for
apprentices and 480 completed source-role minutes for later transitions.
Training reserves the learner's and mentor's native minute together.
An interrupted assignment preserves the learner's completed evidence.
Advancement requires consent, the destination assessment or independent
credential, any required appointment or ownership, a reachable vacancy,
and funded payroll. Validation precedes releasing the current post.

The transition matrix exercises all 240 links with seeded completed
experience so it can isolate each prerequisite. The funded Canterbury
mill trial measures training in native minutes, including mentor loss,
resumption, and advancement from apprentice to mill worker.

**Cycle 10 acceptance.** All 240 transitions passed their success and
unchanged-state rejection fixtures. The funded mill apprentice retained
15 minutes across mentor loss, resumed to 60 minutes, and advanced.
The 1,440-minute Canterbury run passed for all 12,000 funded workers and
4,050 pupils. A funded worker then used completed shift experience to
advance into a reachable named vacancy after an explicit assessment.

## Cycle 11 — Refined resources and custody

Implement all 72 finite kinds with explicit units, storage and custody. Support batch quality, expiry, equipment condition and provenance where the scenario defines them. Separate resources, money and delivered services. Bind resources to sparse inventories and checked reservation ledgers.

**Exit.** Every kind can be admitted, reserved, transferred, consumed or retained according to its declared policy. Invalid units, insufficient stock, overflow and duplicate transfers preserve ledger balances.

Tagged lots carry provenance, quality and an optional expiry. A condition
rating applies to one equipment item, and a zero-condition item cannot
satisfy a recipe equipment hold. Transfers retain batch identity through
native hand custody. The most recent keyed cargo request can be retried
without moving stock twice. Expiry records available or carried stock as
loss. An expired reservation cannot be consumed and records its loss when
released. Resources with unset expiry and quality keep those attributes
unknown. Inventory remains bounded per premises, and depleted lot slots
can be reused.

**Cycle 11 acceptance.** The funded city verifies admission in the declared
unit, reservation and settlement for all 72 kinds, retaining equipment and
rejecting bad units, insufficient stock and overflow. The funded mill trial
moves 1,000 g of tagged wheat in native custody, saves and restores it in
transit, rejects a conflicting delivery retry and mills the delivered lot.
The same trial accounts for 100 g of expired wheat and retains its rated
hand tool after production. The full city day passes for all 12,000 workers
and 4,050 pupils, and the stock, work and checkpoint regressions pass.

## Cycle 12 — Production and supply chains

Complete the five draft recipes with configured equipment, energy and work time. Add the production chains required by the role matrix, including materials, food preparation and counted goods with bills of materials. Account for byproducts, waste, spoilage and imports. Attach producers, processors, retailers and consumers to funded premises and actual deliveries.

**Exit.** Every recipe has a successful native-minute execution and interruption case. End-to-end chains reconcile raw inputs, outputs, byproducts, money, stock and transport. Cooking accounts for water and food energy.

The compiled catalogue adds baking, meal preparation, school-book
binding, garment sewing and shoemaking. Food recipes account for water
and wastewater with a technical conversion of one gram per millilitre.
Fuel consumption and ash are accounted separately from food and water.
Counted recipes specify a gram bill of materials, one finished item and
measured scrap. Bread and prepared meals have an explicit model energy
rate. Paid purchases move money and stock together on a native minute.
Transport retains the Cycle 11 stock and custody accounting.

The funded district follows wheat through milling, bakery delivery, a
paid retail sale and consumption. A second chain cuts imported paper,
binds a counted school book and delivers it to a mapped school. Inputs
that lack a funded upstream producer enter through recorded imports.
Role-specific duties, production breadth and recurring commercial
operations continue in the domain cycles.

**Cycle 12 acceptance.** All 13 recipes complete with reconciled outputs
and wages after an in-progress workplace interruption. The funded bakery
uses 750 g flour, 300 ml water and 120 g fuel. It delivers 1,000 g bread
and accounts for 50 ml wastewater and 12 g ash. The buyer pays 12 model
tokens and consumes 100 g of bread. The counted-goods chain delivers one
school book and accounts for 70 g of paper offcuts. A funded startup job
also completes a paid cloth order during the 1,440-minute city run.
All 12,000 workers and 4,050 pupils complete their schedules and return.

## Cycle 13 — Trades, production and commerce roles

Bind retail, food production, skilled trades, manufacturing, construction and agricultural roles to reusable tasks. Distinguish apprentice, independent worker, supervisor and owner through responsibilities and permissions. Connect work to demand, equipment, maintenance, wages and deliverable outputs.

**Exit.** Every role in these families completes an appropriate task in a staffed fixture. Supervisory and ownership posts affect work allocation, funding or premises through their declared authority.

## Cycle 14 — Transport, hospitality and domestic roles

Implement distribution, storage, postal, passenger, hospitality and domestic work through existing movement and finite custody. Qualify equipment, crews and service routes. Provide a defined operational scenario for each transport family, including rail qualification wherever the role requires railway operation.

**Exit.** Every role in these families has a funded post and successful work fixture. Delivery and passenger state survive delays, crew changes and cancellations with unique custody.

**Cycle 14 complete.** Funded role fixtures exercise all 25 hotel,
licensed hospitality, kitchen, laundry and domestic posts. The named
hospitality roster uses the native paid meal and visit service. Hotel
check-in and checkout use physical presence and finite visitor capacity.
Laundry accounts for clean stock and wastewater.

The funded actual-map trial exercises goods and postal carts, a road
freight vehicle, and a one-passenger road service. Road crews require
matching named posts, explicit credentials and equipment. The technical
trial routes road vehicles only on clear two-way mapped road links and
excludes ways involved in mapped turn restrictions. It uses the shared
native movement pace. The trial also checks stop/resume custody, named
dispatch authority and a paid hotel meal after passenger arrival.
Existing custody regressions cover crew replacement and cancellation.

The funded native matrix exercises all 25 stages across goods carriers,
postal service, road haulage, bus operations and railway operations on
the registered Canterbury map. It covers supervised training, station
assistance, qualified operators and dispatch by both supervisory stages.
The rail test boards at Canterbury West, checks custody through a stop and
restart, disembarks at Sturry and returns the passenger to Canterbury West.

The route uses 39 registered OpenStreetMap track segments. The geometry
review applies 21 bounded footprint translations and one track-side trim,
leaving zero building and rail-ink conflicts. The funded map admits one
reviewed grade-separated water crossing and both track-side station
connectors. Canterbury West uses the registered OpenStreetMap station
footprint. Sturry uses the mapped station building. The modeled crossing
is informed by the existing railway culvert described in the Kent
Statement of Case, section 4.32. Its alignment with the historic water
polygon and the station operating hours are technical scenario assumptions.

[Track source](https://api.openstreetmap.org/api/0.6/map?bbox=1.073%2C51.283%2C1.125%2C51.302)
[Canterbury West station footprint](https://www.openstreetmap.org/way/151293646)
[OpenStreetMap attribution and licence](https://www.openstreetmap.org/copyright)
[Kent Statement of Case](https://www.kent.gov.uk/__data/assets/pdf_file/0010/167059/Statement-of-Case.pdf)

## Cycle 15 — Public, professional and community roles

Bind health, education, civic, religious and community careers to the 21 service definitions. Specify recipient eligibility, permissions, facilities, staff qualifications, equipment and finite supplies. Use ordinary social, work and activity events at successful commit. Preserve each institution's explicit operating status and source provenance.

**Exit.** Every remaining role completes its declared work in a qualified scenario. Each service demonstrates delivery, demand accounting, capacity and failure handling. The traceability inventory covers all 300 roles and 21 services.

**Cycle 15 complete.** The [role traceability inventory](ROLE-TRACEABILITY.json)
lists all 300 compiled occupations and links all 21 service declarations
to funded native checks. The funded map fixtures exercise the 38 newly
bound public, professional and community posts and three institution-linked
ministry support posts. The latter check that their jobs can deliver
pastoral visits or education and that those same jobs cannot occupy a
priest or rabbi office.

The remaining service cases exercise finite clinic beds and medicine,
neonatal care for a recorded newborn, household visits, fire tools and
water, water distribution, stock-backed cleaning, on-site library access,
charity textiles, paid building repair and an attended worship gathering.
Pharmacy transfers a refined dose into the clinic supply ledger. Library
access requires a usable on-site book and keeps it at the premises.

The verification uses explicit technical staffing, operating status and
supplies on the reviewed Canterbury map. The Catholic support fixture
uses Canterbury Cathedral Lodge and the rabbinical education fixture uses
Beerling Hall as temporary venues. The water, charity and cleaning fixtures
use explicit premises-use overrides. Neonatal eligibility uses the first
28 days after a recorded birth. These are scenario assumptions.

## Cycle 16 — Scheduling, economy and daily integration

Integrate staffing, supply orders, production, wages, work, care, instruction, travel and promotion through the existing minute arbitration. Resolve conflicts and shortages deterministically. Add diagnostics for vacancies, absent mentors, insufficient supplies, unavailable services and unreachable premises.

**Exit.** A full city day produces actual commutes, completed work, services and balanced ledgers. Inspections preserve clock and RNG state. Training and work compete for the same native time budget.

### Cycle 13 staffed trade fixtures

The production and repair matrix covers 130 roles across 26 families.
Each family exercises an apprentice, qualified workers, supervisor and
owner or senior manager at a funded, reviewed premises. Production
reserves wages, refined inputs and equipment. Named repair trades use
the existing development project and its finite worksite materials to
restore premises condition. These repair posts cannot change site use or
capacity.

The retail matrix covers the five grocery, drapery, ironmongery,
bookselling and coal merchant families across all 25 roles. An on-shift
clerk must be present for a named-post sale. Apprentices require a
qualified on-site mentor and consume a shared native minute. Sales move
the buyer's money and the stock together and pay the clerk. Managers
control storefront operation, and proprietors need the required property
authority.

The recipe quantities and trade assessments are technical scenario
inputs. Agricultural tasks prepare explicitly imported harvest, livestock
and dairy stocks. Construction and skilled trade recipes use accounted
materials and tools alongside native worksite repair orders.

**Cycle 13 acceptance.** All 155 roles pass the staffed role fixtures.
All 28 recipes pass native execution and interruption checks. The
actual-map district completes a named staffed bread sale. The full
1,440-minute funded city completes 10 paid production orders on normal
startup jobs, and all 12,000 workers and 4,050 pupils complete their
schedules and return home.

### Cycle 16 integration fixture

The staffed-day fixture allocates a fresh household against reviewed
residential capacity, runs outbound and homeward commutes, completes paid
production and an administrative service, and checks finite food, money and
refined stock. It exercises a pending pickup before commuting and a service
that waits for the assigned worker's community qualification. It checks
inspection purity and compares 1,440 batched and individual native minutes
from the same checkpoint. Run it with `python3 tools/verify_work_cycles.py --run`.

`--allocate-homes` exposes initial household allocation in the CLI. It
allocates all households before publishing any change. A qualified walking
link, reviewed entrance and explicit residential capacity are required.
Physical entrance uses the existing native placement and movement APIs.

**Cycle 16 complete.** The funded Canterbury verification runs 1,440
native minutes from normal startup with 27,000 residents. All 12,000
workers and 4,050 pupils complete their scheduled attendance and
return home. Twelve production orders and three same-workplace service
requests complete during the day. The service requests cover
administration, a social-care visit and instruction. They wait for
physical arrival and an active shift, then use the scheduled post's
payroll and experience credit. A separate funded post still uses its
own response wage reservation.

The full-day test reconciles 5,760,000 currency units in payroll,
checks the ordinary and refined stock, food and water ledgers, and
requires zero outstanding wage escrow. It verifies inspection purity
with the complete world digest, including the native clock and RNG.
The funded district check uses a delivered supply batch and confirms
that an active milling order takes precedence over the same worker's
mentoring duty. Training resumes when the order completes. A
qualified worker advances to a funded vacancy at the city-day
boundary.

Run the combined checks with `make access-check`. The service requests
and production orders use explicit technical demand and supplies in
the verifier.

### Dependencies for closing Cycles 13–16

| Cycle | Remaining prerequisite |
| --- | --- |
| 13 | Cycles 09–12 supply active role IDs, qualifications, authority and the remaining executable production chains. |
| 14 | Cycles 07–08 supply qualified routes and staffed premises. Funded native fixtures verify the transport, railway, hospitality and domestic task bindings. |
| 15 | Cycles 08–10 supply funded posts, qualifications and appointment gates. The funded verifier exercises all 21 services with staffed premises and finite supplies. |
| 16 | The funded full-day check verifies commutes, production, services and conserved ledgers. |

Cycles 17–20 extend persistence, authoring and release qualification. The
remaining requirements above can be implemented against the current runtime.
The timber fixture now joins import, pickup, street travel, delivery, paid
processing and byproduct accounting. It compares one-minute and batched
continuation from the same checkpoint. Activation checks report missing
funded posts per role and missing native bindings per service.

## Cycle 17 — Persistence and replay

Version scenario and checkpoint identities. Persist roles, skills, credentials, employer and school state, inventory batches, active work, training, reservations and pending advancements. Define compatibility and migration for existing checkpoints. Exercise transactional load, failure recovery and separate-process replay.

**Exit.** Saves during travel, production, training and promotion resume equivalently. Incompatible or corrupt saves preserve the live world. Batch, single-minute and CLI/app schedules agree.

**Cycle 17 complete.** The [version 2 checkpoint format](../PERSISTENCE.md) binds saves to
the compiled DSL catalogue, map, native ABI and startup scenario
identity. Funded duty assignments, role credentials, school enrollment,
inventory batches, training, work and service reservations, and
advancement prerequisites are preserved. Promotion commits atomically.
A pre-promotion checkpoint retains the learner, qualification and
funded vacancy needed to repeat it.

The funded verifier saves the 27,000-resident scenario with active
commutes, production inputs and wages reserved, and services queued.
It compares the saved instant and the resumed end-of-day digest against
the uninterrupted run in separate CLI processes. The funded district
replays an active production order with a tagged batch and queued
training, compares batch and single-minute continuation, and repeats
a prepared promotion after reload. Truncated checkpoints and incompatible
version, catalogue and startup identities are rejected transactionally.

## Cycle 18 — Native authoring and inspection workflow

Expose scenario selection, compiler diagnostics and provenance in the app and CLI. Show homes, employers, shifts, role history, training, qualifications, inventories and service delivery. Provide a documented path from editing the DSL to loading the compiled scenario and observing its results.

**Exit.** A user can change a scenario value, compile it, run it through both frontends and see the expected behavioural change. Native keyboard navigation, selection and checkpoint restore pass live checks.

## Cycle 19 — City-scale qualification

Run 27,000- and 30,000-person workloads, including crowded travel, simultaneous shifts, resource churn, training and lifecycle changes. Measure memory, startup, simulation time, checkpoint latency and presentation against declared budgets. Refresh the presentation benchmark against current source. Run isolated native and Bronze regressions.

**Exit.** The current build meets the measured budgets, preserves deterministic replay and passes profile isolation. Every role, link, resource, service and recipe has current executable evidence.

## Cycle 20 — Standalone release

Build and qualify the complete scenario from a fresh offline checkout. Ship the runnable DSL source, compiled pack, grammar, editing instructions, scenario assumptions and acceptance evidence. Exercise default launch, full-day simulation and checkpoint continuation with the packaged app and CLI.

**Exit.** Canterbury 1940 runs from the DSL on the actual map with 27,000 residents, staffed workplaces, functioning production and services, and demonstrated advancement. All 300 roles are supported and demonstrable in eligible posts. Both frontends load the same scenario and produce equivalent authoritative state. The release record binds exact source, pins, scenario and fresh live UI evidence.
