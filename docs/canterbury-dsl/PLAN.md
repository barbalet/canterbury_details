# Canterbury DSL implementation plan

Implement a runnable Canterbury 1940 scenario from the [DSL draft](../../content/scenarios/canterbury1940-draft.canterbury). The release target includes 60 career families, 300 roles, 240 advancement links, 72 resource kinds, 21 services and the five draft recipes. The scenario starts with 27,000 residents and supports growth to 30,000. It runs through the CLI and native app using the existing Canterbury map, native bodies and simulation clock.

The [draft specification](../CANTERBURY-DSL-DRAFT.txt) and checker are the authoring baseline. The current runtime supplies placement, jobs, training, services, inventories, operations and checkpoints. The new work connects and extends those systems through a versioned scenario compiler. Cycles 01–05 cover compilation and native population startup. Cycle 06 has exact age cohorts and guarded households, with activity schedules still open. Cycle 07 has passed actual-map housing and entry acceptance in the technical access trial. Cycle 08 is complete for the opt-in citywide startup, funded shifts and recurring school sessions. See the [compiler guide](COMPILER.md) for the ownership inventory, schema migration and build commands.

The current compiler qualifies draft parsing, typed binding, deterministic publication and activation preflight. Runtime execution begins in Cycle 05.

Cycles 09–12 have a compiled definition layer. The CLI can inspect all 60
families, 300 roles, 240 links, 72 resource kinds and five material recipes by
stable ID. The existing eight enabled occupations retain their IDs. The
compiler rejects invalid mentor stages, duplicate stock rows and equipment
references to non-equipment resources. Each recipe now has a labelled manual
trial with finite inputs, outputs, equipment and labour minutes.

The exit checks for Cycles 09–12 remain open. Role assignment, resident competence and supervised progression remain open.
Native premises stock now supports finite imports, reservations, consumption
and return. Refined hand-cargo transfers work between qualified premises. Batch condition remains open. The five original recipes and three additional trade trials now have native execution in staffed fixtures. The unresolved activity, placement and staffing
work from Cycles 06–08 still applies to actual-map integration.

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

## Cycle 10 — Training, qualifications and advancement

Implement curricula, training posts, supervised minutes, mentor capacity, skill assessments and credentials. Enforce age, supervision, independent qualifications and tradition-specific appointments. Implement all 240 candidate transitions with consent, funding, a destination vacancy and any required ownership or appointment. Commit transfers atomically.

**Exit.** Each transition has an executable success fixture and a rejected prerequisite case. Mentor loss preserves earned evidence, failed advancement preserves the current post, and ownership and credential checks have independent coverage.

## Cycle 11 — Refined resources and custody

Implement all 72 finite kinds with explicit units, storage and custody. Support batch quality, expiry, equipment condition and provenance where the scenario defines them. Separate resources, money and delivered services. Bind resources to sparse inventories and checked reservation ledgers.

**Exit.** Every kind can be admitted, reserved, transferred, consumed or retained according to its declared policy. Invalid units, insufficient stock, overflow and duplicate transfers preserve ledger balances.

## Cycle 12 — Production and supply chains

Complete the five draft recipes with configured equipment, energy and work time. Add the production chains required by the role matrix, including materials, food preparation and counted goods with bills of materials. Account for byproducts, waste, spoilage and imports. Attach producers, processors, retailers and consumers to funded premises and actual deliveries.

**Exit.** Every recipe has a successful native-minute execution and interruption case. End-to-end chains reconcile raw inputs, outputs, byproducts, money, stock and transport. Cooking accounts for water and food energy.

## Cycle 13 — Trades, production and commerce roles

Bind retail, food production, skilled trades, manufacturing, construction and agricultural roles to reusable tasks. Distinguish apprentice, independent worker, supervisor and owner through responsibilities and permissions. Connect work to demand, equipment, maintenance, wages and deliverable outputs.

**Exit.** Every role in these families completes an appropriate task in a staffed fixture. Supervisory and ownership posts affect work allocation, funding or premises through their declared authority.

## Cycle 14 — Transport, hospitality and domestic roles

Implement distribution, storage, postal, passenger, hospitality and domestic work through existing movement and finite custody. Qualify equipment, crews and service routes. Provide a defined operational scenario for each transport family, including rail qualification wherever the role requires railway operation.

**Exit.** Every role in these families has a funded post and successful work fixture. Delivery and passenger state survive delays, crew changes and cancellations with unique custody.

## Cycle 15 — Public, professional and community roles

Bind health, education, civic, religious and community careers to the 21 service definitions. Specify recipient eligibility, permissions, facilities, staff qualifications, equipment and finite supplies. Use ordinary social, work and activity events at successful commit. Preserve each institution's explicit operating status and source provenance.

**Exit.** Every remaining role completes its declared work in a qualified scenario. Each service demonstrates delivery, demand accounting, capacity and failure handling. The traceability inventory covers all 300 roles and 21 services.

## Cycle 16 — Scheduling, economy and daily integration

Integrate staffing, supply orders, production, wages, work, care, instruction, travel and promotion through the existing minute arbitration. Resolve conflicts and shortages deterministically. Add diagnostics for vacancies, absent mentors, insufficient supplies, unavailable services and unreachable premises.

**Exit.** A full city day produces actual commutes, completed work, services and balanced ledgers. Inspections preserve clock and RNG state. Training and work compete for the same native time budget.

### Cycle 13 staffed trade fixtures

The native trial covers ten families and 50 of the 155 roles. Milling,
carpentry, weaving, printing, metalworking, joinery, dressmaking, shoemaking,
butchery and fishmongery each have an executable material task. The five
original recipes run alongside three labelled technical trials for leather,
meat and fish. Each family exercises all five posts in a suitable reviewed
fixture. Named roles retain stable string IDs and extend the existing native
numeric role range.

Qualification is an explicit technical assessment tied to the resident and
role. Apprentices require a qualified mentor at the same workplace and share
the native work grant. Supervisors and managers can dispatch appropriate
work for their family. Proprietors with property authority can transfer
finite personal funds to the employer. Opening named vacancies checks one
shift of combined payroll, and individual work orders reserve their own
wages, inputs and tools. Qualification, funding, stock and authority
rejections and checkpoint continuation are covered by the work suite.

Cycle 13 remains open for 105 roles in 21 families. Run the staffed
fixtures with `python3 tools/verify_work_cycles.py --run`.

| Sector | Families still requiring task bindings |
| --- | --- |
| Retail | Grocery, drapery, ironmongery, bookselling and coal merchant |
| Food production | Bakery and brewing |
| Skilled trades | Plumbing, electrical work and motor mechanics |
| Manufacturing | Pottery |
| Construction | Bricklaying, stonemasonry, roofing, plastering and painting/decorating |
| Agriculture | Crop farming, livestock farming, dairy, horticulture and woodland |

The unresolved tasks include stock-backed sales, cooking and brewing,
worksite repairs and agricultural production. Default city staffing and
career advancement remain separate work.

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

Cycle 16 remains open for actual-map workforce scale, school and care
schedules, full demand and service chains, and career progression. The
technical access trial now completes initial home entry for 27,000 residents.

### Dependencies for closing Cycles 13–16

| Cycle | Remaining prerequisite |
| --- | --- |
| 13 | Cycles 09–12 supply active role IDs, qualifications, authority and the remaining executable production chains. |
| 14 | Cycles 07–08 supply qualified routes and staffed premises. Cycle 14 itself still needs vehicle, railway, hospitality and domestic task bindings. |
| 15 | Cycles 08–10 supply funded posts, qualifications and appointment gates. Cycle 15 itself needs each DSL service to dispatch an implemented handler with real staff, supplies and premises. |
| 16 | Cycles 06–15 supply the population routines, placements, jobs, training and service chains needed to execute a full city day. |

Cycles 17–20 extend persistence, authoring and release qualification. The
remaining requirements above can be implemented against the current runtime.
The timber fixture now joins import, pickup, street travel, delivery, paid
processing and byproduct accounting. It compares one-minute and batched
continuation from the same checkpoint. Activation checks report missing
funded posts per role and missing native bindings per service.

## Cycle 17 — Persistence and replay

Version scenario and checkpoint identities. Persist roles, skills, credentials, employer and school state, inventory batches, active work, training, reservations and pending advancements. Define compatibility and migration for existing checkpoints. Exercise transactional load, failure recovery and separate-process replay.

**Exit.** Saves during travel, production, training and promotion resume equivalently. Incompatible or corrupt saves preserve the live world. Batch, single-minute and CLI/app schedules agree.

## Cycle 18 — Native authoring and inspection workflow

Expose scenario selection, compiler diagnostics and provenance in the app and CLI. Show homes, employers, shifts, role history, training, qualifications, inventories and service delivery. Provide a documented path from editing the DSL to loading the compiled scenario and observing its results.

**Exit.** A user can change a scenario value, compile it, run it through both frontends and see the expected behavioural change. Native keyboard navigation, selection and checkpoint restore pass live checks.

## Cycle 19 — City-scale qualification

Run 27,000- and 30,000-person workloads, including crowded travel, simultaneous shifts, resource churn, training and lifecycle changes. Measure memory, startup, simulation time, checkpoint latency and presentation against declared budgets. Refresh the presentation benchmark against current source. Run isolated native and Bronze regressions.

**Exit.** The current build meets the measured budgets, preserves deterministic replay and passes profile isolation. Every role, link, resource, service and recipe has current executable evidence.

## Cycle 20 — Standalone release

Build and qualify the complete scenario from a fresh offline checkout. Ship the runnable DSL source, compiled pack, grammar, editing instructions, scenario assumptions and acceptance evidence. Exercise default launch, full-day simulation and checkpoint continuation with the packaged app and CLI.

**Exit.** Canterbury 1940 runs from the DSL on the actual map with 27,000 residents, staffed workplaces, functioning production and services, and demonstrated advancement. All 300 roles are supported and demonstrable in eligible posts. Both frontends load the same scenario and produce equivalent authoritative state. The release record binds exact source, pins, scenario and fresh live UI evidence.
