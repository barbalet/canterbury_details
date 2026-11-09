# Canterbury Details — 40-cycle implementation plan

Prepared 8 November 2026. **Cycles 01–40 are complete within their documented scope.**
A cycle is a reviewable implementation/documentation milestone, not a simulation
tick. Existing submodule capabilities are starting points, not evidence that the
new standalone application has passed its own acceptance gates.

## Objective and scope

Create a standalone CANTERBURY_SIM application modeling ordinary people, homes,
occupations, institutions, services, movement and development in Canterbury in
1940. Retain religious information as historical/community information: places
of worship, traditions where supported, clergy and lay occupations, employment,
services, gatherings, education, charity, care and institutional continuity.
Religion does not confer supernatural protection or determine a person's moral
character, health, occupation or membership automatically.

There is no vampire, supernatural actor, mythology, occult origin, magical faith
mechanic, holy barrier, supernatural resource, campaign era, Master task or
fictional historical itinerary. Ordinary human access, consent, illness, injury,
death, religious identity and institutional records remain legitimate subjects.

The plan was initially created without implementation. Subsequent user requests
authorize the cycles marked complete below. Keep each cycle’s evidence and status
current; do not copy the old application wholesale or claim inherited milestones.

## Reuse objective after Cycle 40

The completed ordinary city is the user's intended foundation for the later
vampire simulation. Preserve a neutral reusable core, separate frontends/policy
layers, stable IDs, one native clock/population and versioned content/save/RNG
contracts throughout these cycles. Do not add supernatural rules to this base.
`docs/REUSE-FOUNDATION.md` specifies the separation and post-cycle-40 handoff.

A native Xcode project and json2png visual map UI were added at the user's request
between Cycles 08 and 09. This pulls forward presentation/build work only; later
physical geometry, access and routing are now implemented in Cycles 09–13 below;
human routines and configured lifecycle behavior were subsequently completed
and qualified in Cycles 14–18.

## Source authority and repository independence

The only upstream implementation/data dependencies are these checked-out
submodules, initially pinned to:

| Submodule | Repository | Observed revision |
| --- | --- | --- |
| `canterbury/` | `barbalet/ds-canterbury1940` | `c8dfee17622fcc903b247afcf4d6df4a70489e2a` |
| `apesdk/` | `barbalet/apesdk` | `abdde4b18b8ac96744b20c47bd9dfeeba96780bb` |

Record later changes explicitly. The public availability of a repository does not
establish historical certainty or remove license/attribution obligations.

Useful initial source locations are `canterbury/newcanterbury/canterbury.json`,
`canterbury/simulation-map/`, its `derived/` data, the historical religious-site
records, and ApeSDK's native, Bronze actor/work/task, drawing and persistence
modules. Inspect their schemas, provenance and flags before importing them.
Upstream documentation currently describes an earlier host; that description
must not become this application's population, authority or feature contract.

The previous nosferatu-era code may be consulted as an implementation reference.
Any reused project code, generator, test, policy or document must be physically
replicated and adapted into **this** repository, with its licensing recorded.
Re-derive factual data from the two submodules; do not inherit the previous
project's content manifests, narrative sources, historical claims, religious
scenario overrides or release results as authority.

No symlinks, includes, imports, build scripts, runtime files, absolute developer
paths or fallback reads may depend on a sibling nosferatu-era checkout, its web
uploads or its build products. A clean clone with recursively initialized
submodules must contain everything needed to build and validate offline after
normal platform toolchain installation. Build-time generators must not fetch
websites. Additional historical material requires an explicit future source
update to the approved submodule data, not silent invention in the host.

Distinguish throughout:

- **Upstream fact/claim:** preserved with source path, identifier and confidence.
- **Derived result:** reproducible transformation with tool/version/input hashes.
- **Simulation policy:** a disclosed technical model or parameter, derived from
  SDK facilities where available; never misrepresented as a historical fact.
- **Unknown or unsupported:** retained as such, not filled with fabricated people,
  institutions, routes, practices or period statistics.

In particular, modern labels do not prove 1940 use; map candidates are not
surveyed entrances; drawn rail is not an operating train service; upstream
`traversable: false` does not become true merely because a route is convenient.
Do not import the previous project's active Jewish-centre override or demographic
assumptions unless the current approved upstream records themselves support them.
Unlocated institutions remain non-spatial until their source location is resolved.

## Ownership and technical contracts

| Concern | ApeSDK responsibility under `CANTERBURY_SIM` | Canterbury Details responsibility |
| --- | --- | --- |
| Population | Native bodies, capacity and guarded admission | Human demographic records, households, lifecycle policy |
| Identity | Stable actor IDs, native-slot mapping, relocation/rebind | Relationships and references by stable ID; retirement cleanup |
| Time | Native minute and observed Bronze clock | Civil-calendar mapping, schedules and deadlines |
| Space | Sole physical native body position | Source geometry, routes, doors and one movement integrator |
| Needs/work | Native energy and guarded work accounting | Human meals, rest, fatigue, care, jobs and service effects |
| Institutions | Reusable SDK storage/hooks where appropriate | Source-derived city institutions, occupations and inventories |
| Persistence | Native/Bronze codecs and staging seams | Atomic city checkpoint envelope and project payload validation |
| Presentation | Existing native drawing/compositing and host support | Canterbury map, camera, selection, overlays and native panels |

Compile all relevant translation units consistently with `CANTERBURY_SIM=1` and,
while the current bridge requires it, `BRONZE_SIM=1`. These are build-wide ABI
choices, not per-file convenience flags. Use the existing profile before adding
new APIs. Any required SDK extension is authorized but must be entirely isolated
by `#ifdef CANTERBURY_SIM` (including fields, declarations, changed bounds and call
sites). With the define absent, ordinary native/Bronze behavior and public layouts
must remain unchanged. Do not disguise a city feature as an unconditional SDK fix.

There is one native population, one actor identity system, one physical position,
one human energy authority and one minute advance. The city callback replaces
island autonomy; it must implement the required human behavior rather than also
running the island's biological/social/movement loops. Presentation, pause and
inspection never consume simulation time or RNG. All stationary work competes
for the same guarded native minute budget.

The required starting population is **27,000 ordinary humans**, matching the
user's requested game population. This is a simulation requirement, not a verified
1940 census. Cycle 03 extends the pinned SDK's former 26,999 admission/default to
all slots under `CANTERBURY_SIM`, with a 27,000-person default and
30,000 capacity for growth; no special actor slot is reserved.
Smaller technical fixtures remain supported. The exact reviewed SDK changes and
uncommitted status are recorded in `content/sdk-working-tree.json`. Cycle 04 qualifies capacity; subsequent cycles implement the full city lifecycle. Measure memory: the existing
native layout occupies roughly 1.90 GB before city state and save staging.

## Proposed repository layout

```text
canterbury_details/
  PLAN.md                         # this plan and subsequent cycle status
  README.md                       # standalone setup and entry points
  canterbury/                     # data submodule
  apesdk/                         # engine submodule
  src/                            # project-owned human city model and CLI
  ui/                             # native application/presentation
  tools/                          # offline import, build and qualification tools
  content/                        # schemas, policy, review overlays, manifests
  docs/                           # source audit, architecture, model and user docs
  tests/                          # fixtures, replay programs and acceptance tests
  build/                          # ignored reproducible outputs and reports
```

Names are proposed; settle them in Cycle 02. Keep project code outside the
submodule directories. Reference SDK code in `apesdk/`; do not maintain a second
private SDK copy. Commit SDK changes in that submodule first, then update the
parent gitlink. Data-source changes follow the same submodule discipline.

## Cycle completion discipline

Implement in this order unless a documented prerequisite permits an independent
read-only prototype. Each cycle updates its source, relevant documentation,
acceptance fixture and machine-readable evidence. A passing old project test is
not a passing new project test. Mark a cycle complete only after its exit checks
pass in this checkout, including applicable ordinary SDK regressions.

Keep evidence and limitations explicit; distinguish a synthetic fixture from a
historical reconstruction. New behavior requires behavioral validation; prose-only
and presentation-only changes should not trigger irrelevant simulation tests.
Failures, queue saturation and malformed data must be diagnosed without partial
publication, dangling references or silently lost authoritative work.

## Cycle 01 — Inventory upstream sources and pin the setting

**Complete — 8 November 2026, source-audit scope.** See `docs/SOURCES.md`,
`docs/UNCERTAINTIES.md`, `content/source-manifest.json` and
`content/cycle-01-validation.json`. Both pinned submodules are inventoried with
per-file hashes and mapped input identifiers. Offline upstream map validators
and tests pass in an isolated copy without modifying their inputs. Cycle 03
subsequently adds the reviewed SDK population patch described above. SDK
regression entry points and license/data gaps are recorded, not claimed as
new-host runtime qualification. Reproduce with `tools/audit_sources.py --check`
and `tools/verify_cycle01.py --run`.

**Depends on:** initialized submodules.
Inventory maps, schemas, enrichment/review records, religious-site records, SDK
profiles, licenses and existing tests at the pinned revisions. Identify 1940,
modern, inferred and unknown fields. Record spatial boundary/population mismatch
and unsupported data. Do not import the old project's extra source requirements.
**Documentation:** `docs/SOURCES.md`, source manifest and uncertainty register.
**Exit:** every proposed factual input maps to an existing upstream path and
identifier; revisions/hashes and applicable licenses are reproducible.

## Cycle 02 — Define the standalone human-city architecture

**Complete — 8 November 2026, architecture/declaration scope.**
`docs/ARCHITECTURE.md`, `docs/REUSE.md`, `content/architecture.json` and
`content/reuse-ledger.json` define the neutral project layout, acyclic local
dependencies, field ownership, public `cd_` API and per-file reuse exclusions.
At Cycle 02, `src/cd_api.h` was declaration-only; Cycles 09–13 subsequently
implement content/world/snapshot and MOVE/node runtime behavior.
`tools/verify_cycle02.py --run` validates the contract, rejects invalid designs,
and checks C/C++ syntax and required-profile rejection. Cycle 02 itself did not
modify submodules; Cycle 03 adds the reviewed population patch. Evidence: `content/cycle-02-validation.json`.

**Depends on:** 01.
Settle project names, module ownership, folder layout, public C interfaces and the
reuse inventory. Review reusable geometry, needs, jobs, IO and native UI code;
exclude supernatural and campaign modules and strip their state/dependencies from
mixed modules. Use neutral project-local types, fixtures and labels.
**Documentation:** `docs/ARCHITECTURE.md` and reuse/attribution ledger.
**Exit:** dependency graph contains only this checkout and the two submodules;
every planned authoritative field has one owner and no supernatural subsystem.

## Cycle 03 — Establish independent build and test entry points

**Status: complete.** Standalone CLI/macOS bundle, guarded shared build flags,
small deterministic smoke tests, 27,000-person default startup, dependency errors
and independent clean-room builds pass. Native controls were observed with all
27,000 bodies bound. See `docs/BUILD.md` and the Cycle 03 validation records.
The SDK changes and parent gitlink are committed; see the final release pin.

**Depends on:** 02.
Create CLI and native-app build definitions, consistent profile flags, local
source references, ignored output folders and a small smoke fixture. Adapt useful
build tools into this repository rather than invoking sibling scripts.
**Documentation:** README setup, supported toolchains and build commands.
**Exit:** clean checkout builds a minimal native city host; missing submodules or
inconsistent definitions fail clearly; no previous application is required.

## Cycle 04 — Qualify capacity and profile isolation

**Complete — 8 November 2026, capacity/ABI scope.** Real native sessions at 1,
27,000, 29,999 and 30,000 people pass binding/count checks. Invalid admission and reset
requests preserve the population; spare-slot admission and 32 full-capacity
identity replacements pass. Native, Bronze and city ABI probes are independent;
exact-source ordinary-profile regression evidence remains current. No additional
SDK edits were needed. See `docs/SDK-INTEGRATION.md`,
`content/cycle-04-validation.json` and `tools/verify_cycle04.py`.
Configured births/admissions and visitor policy were subsequently implemented
in the civil layer; historical demographic rates remain unsupported.

**Depends on:** 03.
The initial 27,000-person admission/default change was implemented in Cycle 03
at the user's request. Validate current guarded capacity/admission APIs. Remove assumptions about a
reserved special actor. If needed, extend initial human admission up to actual
capacity under `CANTERBURY_SIM`; retain an explicitly labeled default fixture and
bounded births/visitors. Probe native, Bronze and city layouts independently.
**Documentation:** `docs/SDK-INTEGRATION.md`, capacity/ABI contract.
**Exit:** minimum/default/full/overflow populations behave correctly, all bodies
are ordinary people, and unflagged layouts/behavior match the pinned baseline.

## Cycle 05 — Establish the sole clock and city lifecycle

**Complete — 8 November 2026, native bootstrap lifecycle scope.** The adapter
checks consecutive native minutes and callbacks, retires callback context before
close, and derives Gregorian scenario time from native elapsed. The explicit
origin convention is 1940-01-01 00:00; opening consumes the documented initial
binding minute and returns paused at 00:01. Real native midnight, pure inspection,
restart/seed isolation, late-call and stale-context tests pass. Native app clock
and Refresh were observed at 27,000 people. No further SDK edits were required.
See `docs/TIME-LIFECYCLE.md`, `content/cycle-05-validation.json` and
`content/cycle-05-native-ui.json`. CdWorld APIs and staged checkpoint replacement were subsequently completed in
Cycles 09/31; concurrent independently ticking SDK worlds remain unsupported.

**Depends on:** 04.
Configure the native city callback, initialization, pause, minute advance, close
and independent session restart. Map the native origin to the 1940 scenario date.
Disable duplicate island behavior through the existing guarded seam, not a second
host timer. Document any minimal guarded SDK hook extension.
**Documentation:** temporal/lifecycle contract and SDK call sequence.
**Exit:** one `sim_cycle` advances one native minute, paused inspection advances
none, and reopen/session switching cannot retain a stale callback or context.

## Cycle 06 — Bind people to stable native identities

**Complete — 8 November 2026, live identity/binding scope.** Project person
records use native actor IDs with runtime world tokens. Admission, retirement,
last-body compaction, whole-body reorder, lookup and full rebind are implemented.
Real 32-, 27,000- and 30,000-person fixtures preserve surviving identities/body markers and
reject duplicate, incomplete, retired, unknown and foreign-world references before
publication. Empty-after-retirement/readmission also passes. Native app startup
and stepping remain valid at 27,000. No SDK edits were needed. See
`docs/IDENTITY.md`, `content/cycle-06-validation.json` and
`content/cycle-06-native-ui.json`. Synthetic civil demographics and archived life records were subsequently
completed in Cycles 12/14/29.

**Depends on:** 05.
Implement project person records bound to native actors, ID lookup, relocation,
compaction, retirement and full rebind. Never use array indices as persistent
person identities or reuse a retired ID for a new resident.
**Documentation:** identity/reference lifecycle and ownership rules.
**Exit:** birth/death/reorder/session fixtures preserve surviving bindings and
reject duplicate, missing and foreign-world references atomically.

## Cycle 07 — Bound memory and state publication

**Complete — 8 November 2026, native host resource-publication scope.** Checked
heap allocations and a 5 GiB payload ceiling cover native bodies, actors, city
records, scratch and startup workspace. Guarded SDK storage seams allow all
fallible allocation before retiring the live owner. Every one of 12 construction
and 12 replacement allocation sites is fault-tested; failure preserves the live
world, publication allocates nothing, and cleanup returns tracked payloads/blocks
to zero. Three deterministic city RNG streams pass vectors/isolation and rejected
admission rollback. Exact memory categories/peak and ordinary-profile regression
evidence are recorded. See `docs/MEMORY.md`, `content/cycle-07-validation.json`
and `content/cycle-07-native-ui.json`. The SDK fixes and parent gitlink are now committed. Content/checkpoint staging
and process RSS qualification were subsequently completed in Cycles 08/31/37.

**Depends on:** 06.
Allocate city-scale arrays safely on the heap, check sizes/overflow, and centralize
cleanup. Establish staged replacement, allocation-failure hooks and deterministic
RNG streams. Profile native-body, actor, city and temporary memory separately.
**Documentation:** state inventory and initial memory budget.
**Exit:** failed construction preserves the live world; repeated open/close and
injected allocation failures leave no leaks or partially active registries.

## Cycle 08 — Compile the upstream Canterbury content offline

**Complete — 8 November 2026, offline candidate-compilation scope.** The local
compiler validates locked source bytes, geometry/IDs/references/flags, provenance,
licenses and independent review annotations against a versioned policy/schema.
All original map classes and 3,341 building candidates, six separate networks and
33 institutions (five unlocated) survive with their uncertainty. Repeated full
compilation is byte-identical. Thirty contract tests cover invalid/stale
inputs, reviews, path boundaries, last-moment input changes and atomic filesystem
failure preservation. See `docs/CONTENT.md`, `content/cycle-08-validation.json`,
`tools/compile_content.py` and generated `build/content/canterbury.json`.
Neither submodule is modified. Runtime C content loading, physical access and
routing remain subsequent-cycle work.

**Depends on:** 01, 03, 07.
Build a deterministic importer for approved upstream geometry and metadata.
Validate IDs, references, dimensions, finite values, provenance and input flags.
Keep review annotations explicit and independent of regenerated upstream files.
Hash actual inputs, tools and revisions; publish outputs atomically.
**Documentation:** `docs/CONTENT.md`, schemas and regeneration commands.
**Exit:** identical inputs reproduce outputs; invalid/stale inputs fail without
replacing the previous valid generation; no web or sibling-file reads occur.

## Cycle 09 — Model footprints, water, doors and ordinary access

**Complete — 8 November 2026, qualified runtime/model scope.**
Immutable runtime pack/loader, rings and holes, building/water/rail clearance,
reviewed versus candidate entrances, ordinary access and source confidence are
implemented. See `docs/GEOMETRY.md`, `content/spatial-review.json` and
`content/cycle-09-validation.json`. The actual source review queue remains empty;
fixtures qualify behavior without inventing historical entrances.

**Depends on:** 08.
Handle polygon rings/holes, building uses, water/rail geometry and candidate door
connectors. Distinguish reviewed entrances from technical candidates. Use ordinary
residence, employment, opening/locking and emergency-access policies; religious
buildings have no magical exclusion. Keep unknown locations unlocated.
**Documentation:** geometry/access model and review queue.
**Exit:** connectors cannot cut through walls, holes or forbidden water; access
and picking respect real geometry and retain source confidence.

## Cycle 10 — Integrate physical pedestrian movement

**Complete — 8 November 2026, qualified runtime/model scope.**
Source-polyline and door travel use the native body as the sole position
authority and one Bronze minute grant. Bounded budgets, arrival, cancellation,
in-place resume and quantization clearance pass native-backed tests. See
`docs/MOVEMENT.md` and `content/cycle-10-validation.json`.

**Depends on:** 05, 06, 09.
Implement movement along actual polylines and entrance connectors using the native
body position as authority. Add bounded travel intentions, arrival, interruption
and source-supported walking parameters or disclosed SDK-derived model values.
**Documentation:** movement integrator, units and coordinate transforms.
**Exit:** people move continuously without teleportation, double integration,
wall penetration or an independent cached position becoming authoritative.

## Cycle 11 — Qualify route topology and dynamic restrictions

**Complete — 8 November 2026, qualified runtime/model scope.**
Qualified components, grade/junction checks, per-edge bridge/crossing rules,
deterministic route costs, closures, crowd capacity, node-boundary replanning and
explicit waits/failures are implemented. Source namespaces remain separate and
unreviewed links disabled. See `docs/ROUTING.md` and
`content/cycle-11-validation.json`.

**Depends on:** 10.
Validate source-node topology, connected components, grade separation, bridges,
closures and route costs. Qualify walkable links explicitly; do not flip upstream
candidate flags globally. Handle blocked endpoints, replanning and crowd load.
**Documentation:** route qualification ledger and diagnostics.
**Exit:** reachable/detour/disconnected/closure/crowd fixtures produce explainable
routes; blocked routes wait or fail clearly and never invent a connection.

## Cycle 12 — Generate ordinary residents from bounded inputs

**Complete — 8 November 2026, qualified runtime/model scope.**
All 27,000 default residents have unique native-backed references, explicit
SDK-name/synthetic demographic fixtures, valid civil dates, ages and configured
categories. Unknown affiliation stays unknown. Maximum 30,000-person admission
also passes. See `docs/POPULATION.md` and `content/cycle-12-validation.json`.

**Depends on:** 06, 08, 11.
Implement names, ages, civil dates, supported demographic fields and scenario
population admission. Use upstream distributions only where supplied; otherwise
identify a synthetic fixture and keep unknown affiliations unknown. Distinguish
residents, visitors, troops or evacuees only when supported/configured explicitly.
**Documentation:** population assumptions, boundary coverage and data dictionary.
**Exit:** all admitted people have valid IDs/dates and plausible internally
consistent records; totals reconcile without a hidden special-character slot.

The repeated-name defect found during Cycles 14–18 is corrected: names now use
a deterministic SDK-vocabulary generator with shared initial household surnames.
The full default population has 26,898 distinct full names without ID suffixes;
repeatability and different-seed variation are tested.

## Cycle 13 — Construct households, housing and relationships

**Complete — 8 November 2026, qualified runtime/model scope.**
The default 6,750 synthetic households have consistent spouses/guardians.
Reviewed capacities account for resident reservations, visitors, physical guests
and pending entrances without double counting. Atomic relocation and membership
updates preserve references; ownership and physical presence remain independent.
Unknown housing is distinct from known unhoused. See `docs/HOUSEHOLDS.md` and
`content/cycle-13-validation.json`. Native UI/threading evidence is in
`content/cycle-09-13-native-ui.json`; the zoom fix was confirmed by the user.

**Depends on:** 09, 12.
Model households, dwellings, occupancy, spouses, guardians, dependents, visitors
and unhoused residents. Keep property ownership separate from current occupation
and presence. Respect capacities and distinguish household structure from census
claims. Adapt only ordinary relationship behavior from the reference code.
**Documentation:** household/housing model and unknown-data policy.
**Exit:** members and occupancy reconcile, guardians remain valid, relocation
updates all relevant references, and absence does not imply an empty household.

## Cycle 14 — Implement human lifecycle changes

**Complete — 8 November 2026, bounded ordinary lifecycle scope.**
Configured admissions/births, birthdays, departure/death, guardian repair,
relationship/property/route/food/care cleanup and bounded life records are
implemented. A birth into slot 30,000 and atomic overflow rejection pass. No
unsupported demographic rate or newborn position is invented. See
`docs/LIFECYCLE.md` and `content/cycle-14-validation.json`.

**Depends on:** 07, 13.
Implement birthdays, household changes, admission, births where model inputs
support them, departure, death, guardianship repair and vacancies. Separate native
lifecycle support from any unsupported demographic rate model. Fail closed when
housing, capacity or parent/guardian requirements cannot be satisfied.
**Documentation:** lifecycle policies and explicitly unsupported rate assumptions.
**Exit:** full-capacity and churn tests conserve identities/population accounting;
retirement cleans work, routes, ownership, services and relationships once.

## Cycle 15 — Model daily needs and rest

**Complete — 8 November 2026, qualified technical-model scope.**
Native energy, finite meal consumption, sleep/rest/fatigue and exactly-once
priority arbitration are implemented. Zero-step pause and individual/batched
day-length stepping agree. See `docs/NEEDS.md` and
`content/cycle-15-validation.json`.

**Depends on:** 05, 10, 13, 14.
Implement finite meals, rest, sleep, fatigue and ordinary human energy using the
native energy field. Arbitrate care/survival/travel before elective work. Derive
baseline constants from SDK facilities or label technical model assumptions;
never copy supernatural survival, dawn death or resource mechanics.
**Documentation:** needs/state transitions and conservation rules.
**Exit:** exactly-once metabolism and rest survive pause/fast stepping, and no
human also receives the island biological update or duplicate needs effects.

## Cycle 16 — Build food supply and household provision

**Complete — 8 November 2026, qualified technical-model scope.**
Finite carried/home/store stocks, scenario imports, qualified production,
physical collection/delivery, dependent feeding and export/death-cache ledgers
are implemented. A two-building fixture traverses reviewed streets and entrances
for both collection and household delivery. See `docs/FOOD.md` and
`content/cycle-16-validation.json`.

**Depends on:** 15.
Model inventories, carried meals, household food, producers/shops, replenishment
and dependent feeding with finite stocks. Track production, consumption, transfer,
spoilage only if supported, and unmet demand. Avoid free replenishment that masks
broken services or invents a historical ration allocation.
**Documentation:** food ledger, units and model/source limits.
**Exit:** stock conservation and real delivery/care constraints hold across a day;
children and dependent adults cannot provision themselves through an invalid actor.

## Cycle 17 — Add illness, clinical care and mortality

**Complete — 8 November 2026, qualified technical-model scope.**
Configured ordinary illness/injury, physical qualified care, finite clinic
beds/medicine, preventive experiments, recovery and closed-minute mortality
cleanup pass. Parameters are explicitly technical/non-predictive; no historical
disease incidence is invented. See `docs/HEALTH.md` and
`content/cycle-17-validation.json`.

**Depends on:** 14–16.
Use native physiology hooks and supported city parameters for ordinary illness,
injury, care and recovery. Model household/clinical capacity and medicine only
with explicit units and provenance. Keep disease scenarios labeled as model
experiments rather than imported supernatural or medieval plague narratives.
**Documentation:** health model, care occupations and non-predictive scope.
**Exit:** prevention/care/recovery/death and compaction preserve causal records,
resource conservation and surviving household bindings without duplicate effects.

## Cycle 18 — Derive the occupation catalogue

**Complete — 8 November 2026, qualified technical-model scope.**
All 63 SDK vocations are audited. Eight explicit ordinary role definitions
are enabled; 58 unsupported upstream specializations remain disabled. Clergy and
religious lay caretakers use Canterbury institution evidence. Age/qualification
eligibility is separate from employment, workplaces and affiliation. The native
catalogue inspector exposes provenance and legitimate outputs. See
`docs/OCCUPATIONS.md` and `content/cycle-18-validation.json`.

**Depends on:** 01, 12, 17.
Audit SDK vocation definitions against Canterbury records. Separate occupation,
job vacancy, role, skill, employer, workplace and voluntary duty. Disable roles
that are unsupported for the setting; do not rename an ancient SDK role and claim
historical evidence. Keep clergy and religious lay occupations in the catalogue.
**Documentation:** `docs/OCCUPATIONS.md`, source mapping and exclusions.
**Exit:** every enabled occupation has a supported/disclosed role definition,
age/skill requirements and a legitimate service or work output.

## Cycle 19 — Assign workplaces, shifts and apprenticeships

**Complete — 8 November 2026, qualified technical-model scope.**
Bounded employers/vacancies, age/skill/reachability checks, real source-polyline
commutes, primary/secondary/overnight shifts and qualified apprenticeships pass.
Mentor loss cancels dependent work safely; unplaced residents are not auto-hired.
See `docs/EMPLOYMENT.md` and `content/cycle-19-validation.json`.

**Depends on:** 11, 13, 18.
Create bounded jobs, employers, worksite capacities, shifts, qualifications,
mentors, apprentices and secondary duties. Employment uses source-appropriate
institutions/buildings, with explicit synthetic schedules where data is absent.
**Documentation:** employment model and capacity/qualification rules.
**Exit:** assignments respect age, skill, capacity and reachability; vacancies and
mentor loss are handled deterministically without assigning every child a job.

## Cycle 20 — Arbitrate work through the native minute

**Complete — 8 November 2026, qualified technical-model scope.**
Resumable work shares the single native grant with travel, survival and care.
Paired apprentice/mentor grants, interruption, shift/closure waits, batch stepping,
idempotent retries, finite wage/input reservations and exactly-once completion
pass. See `docs/WORK.md` and `content/cycle-20-validation.json`.

**Depends on:** 05, 15, 19.
Use guarded Bronze work claims for stationary work and resumable tasks. Resolve
travel, care, fatigue, meals and competing intentions before granting productive
time. Share the native budget; do not create a parallel job-progress clock.
**Documentation:** scheduler, interruption/resume and commit contract.
**Exit:** a person cannot work and travel/care simultaneously; work costs and
completion outputs occur once under normal, interrupted and repeated-tick calls.

## Cycle 21 — Implement production, trade and development inputs

**Complete — 8 November 2026, qualified technical-model scope.**
Finite goods/recipes, physical household purchases/cargo, wages and financial
transfers pass shortage, cancellation, retry and lifecycle fixtures. Goods, food,
money and escrow ledgers reconcile. Prices/yields are explicitly technical model
assumptions. See `docs/ECONOMY.md` and `content/cycle-21-validation.json`.

**Depends on:** 16, 18–20.
Model ordinary goods, material inputs, production chains, household purchases,
wages and transfers using the available source/SDK units. Keep technical prices
and rates explicit rather than presenting invented 1940 economics as fact.
**Documentation:** economy ledgers and dependency diagrams.
**Exit:** conservation/shortage/recovery fixtures explain service availability;
trade and production cannot generate money or goods through retries/cancellation.

## Cycle 22 — Model premises and local development

**Complete — 8 November 2026, qualified technical-model scope.**
Material/labor-gated repair, capacity and use overlays preserve source identity
and geometry. Interrupted projects resume; invalid publication waits without
additional wages; cancellation refunds unused reserves once. Runtime footprint
rewrites remain unsupported pending a reviewed content generation. See
`docs/DEVELOPMENT.md` and `content/cycle-22-validation.json`.

**Depends on:** 09, 13, 20, 21.
Add maintenance, repair, capacity changes, permitted use changes and local works
where upstream data/model contracts permit. Track material/labor requirements,
worksites and completion. Preserve the base historical map and label hypothetical
scenario development separately; never silently rewrite source building identity.
**Documentation:** development rules, review status and project records.
**Exit:** projects consume real capacity/resources, survive interruption, and only
publish valid geometry/use/capacity changes after all prerequisites are met.

## Cycle 23 — Import civic and religious institutions

**Complete — 8 November 2026, qualified technical-model scope.**
All 33 supplied religious records (32 Christian, one Jewish; five unlocated)
retain their exact dates/notes, identity, status and provenance. Reviewed premises
links, offices and configured operations remain separate from ownership,
congregations and source uncertainty. Closure/override and immutable snapshot
fixtures pass; the native inspector exposes the source evidence. See
`docs/INSTITUTIONS.md`, `content/institution-source-matrix.json`,
`content/cycle-23-validation.json` and `content/cycle-19-23-native-ui.json`.

**Depends on:** 08, 09, 18, 22.
Represent source institutions, traditions/denominations where recorded, dates,
status, precincts and spatial confidence. Keep offices, buildings, congregations,
legal ownership and current operations distinct. Preserve Jewish and Christian
records as supplied; do not invent additional local populations or institutions.
**Documentation:** `docs/INSTITUTIONS.md`, religious-site source matrix.
**Exit:** all imported institutions retain traceable identity/status; closures and
unlocated sites remain distinguishable from active, spatially qualified premises.

## Cycle 24 — Develop clergy and religious lay occupations

**Complete — 8 November 2026, qualified technical-model scope.**
Distinct source-compatible clerical and lay offices, explicit institution/title
qualifications, ordinary permissions, staffing vacancies and succession pass.
No affiliation or historical staffing is inferred. See
`docs/RELIGIOUS-EMPLOYMENT.md`, `content/religious-worksite-matrix.json` and
`content/cycle-24-validation.json`.

**Depends on:** 19, 20, 23.
Model source-supported clergy, ministers, rabbis, custodians, sextons, church staff,
teachers, administrators and charitable/voluntary roles as ordinary people and
jobs. Record prerequisites, employers, duties, service outputs and succession.
Keep titles/traditions distinct rather than assuming interchangeable authority.
**Documentation:** religious occupation/worksite matrix and ordinary services.
**Exit:** vacancies affect actual services; replacement staff need qualification
and work capacity, with no supernatural bonus or inferred moral/personality trait.

## Cycle 25 — Model gatherings, calendars and community services

**Complete — 8 November 2026, qualified technical-model scope.**
Explicit dated/day-interval schedules, membership, real attendee/staff travel,
shared grants, finite meals/charity and window/closure rules pass replay/batch and
year-boundary fixtures. Canonical calendar component checkpoints reject corrupt,
stale or failed staging without duplicated effects. Full coordinated city saves
remain Cycle 31; this cycle qualifies the calendar component and same-instant
round-trip, not a standalone whole-city restore. See `docs/COMMUNITY.md` and
`content/cycle-25-validation.json`.

**Depends on:** 05, 13, 17, 23, 24.
Implement supported worship/gathering schedules, education, charity, meals,
visitation and burial administration. Resolve locality/date boundaries from the
approved inputs; unknown holidays or practices are not invented. Membership and
attendance are explicit model data, not inferred automatically from nearby houses.
**Documentation:** calendar/community model and tradition-specific limitations.
**Exit:** gatherings and service demand affect real travel/work/care once; date
boundaries, staff absence and facility closure remain correct across replay/save.

## Cycle 26 — Integrate civic services and emergencies

**Complete — 8 November 2026, qualified technical-model scope.**
Permission-checked ordinary rosters and finite dispatch implement physical
response, health/clinic integration, teaching/administration/site response,
visitation, charity/meals, burial administration and water/tool-gated fire work.
Cancellation, deadline, staff loss/succession, stock shortages and full queues pass.
Historical organizations remain unknown unless sourced. See `docs/SERVICES.md`,
`content/civic-source-register.json` and `content/cycle-26-validation.json`.

**Depends on:** 17, 20–25.
Model source-supported health, education, administration, policing, firefighting
and burial roles, their rosters, equipment and premises. Add observable ordinary
access and service rules. Do not transform occupations into magical abilities or
assume a map label supplies a fully staffed historical organization.
**Documentation:** service rosters, capabilities and gaps.
**Exit:** dispatch/cancel/staff-loss/shortage fixtures consume real shared work,
reach legitimate destinations and generate auditable outcomes.

## Cycle 27 — Add ordinary transport and logistics

**Complete — 8 November 2026, qualified technical-model scope.**
Technical handcarts and stretchers use actual qualified crew paths and shared
loading/unloading grants. Cargo/passenger custody, pause/resume, partial-load crew
replacement, local cancellation and full-building arrival pass without teleport,
overfill or duplication. Rail remains reference-only pending qualified equipment,
routes and service. See `docs/LOGISTICS.md` and `content/cycle-27-validation.json`.

**Depends on:** 11, 20, 21, 26.
Move people and goods through source-qualified roads, carriers and vehicles with
capacity, loading/unloading, crews and interruption. Preserve rail geometry as
reference unless an actual operational route, equipment and service are supplied
and qualified. No inherited earth-box or special-passenger mechanisms remain.
**Documentation:** transport/service qualification and custody ledger.
**Exit:** cargo, passengers and vehicles have unique custody/positions, obey
closures, and cannot teleport, overfill, duplicate cargo or move without valid crew.

## Cycle 28 — Implement environmental and 1940 scenario pressures

**Complete — 8 November 2026, qualified technical-model scope.**
Explicit bounded rain, fire, damage, disruption, alert, blackout, ration and
shelter experiments preserve the historical map. Finite response water, persistent
damage/fire, overlay recovery, physical shelter travel and resource accounting
pass. No event is inferred from the year 1940. Native inspector/zoom/step checks
pass. See `docs/ENVIRONMENT.md`, `content/cycle-28-validation.json` and
`content/cycle-24-28-native-ui.json`.

**Depends on:** 09, 17, 22, 26, 27.
Add bounded supported weather, fire, damaged premises, shelter and disruption
mechanics. Wartime alerts, evacuation, blackout or ration experiments require
explicit scenario inputs; the date 1940 alone does not authorize invented events
or importing damage that occurred in a different year.
**Documentation:** environment/scenario source register and technical parameters.
**Exit:** scripted disturbances have deterministic ordinary causes and responses,
respect geometry/resources, and recover or persist without duplicated minute work.

## Cycle 29 — Build causal records and ordinary social continuity

**Complete — 8 November 2026, qualified ordinary-model scope.**
Bounded causal records, scoped observations/copies, household bereavement support context, and staffing-loss → job → service traces are implemented. Retired actors cannot act. Retention gaps and diagnostic versus household/personal visibility are explicit. See `docs/RECORDS.md` and `content/cycle-29-validation.json`.

**Depends on:** 14, 17, 21–28.
Record events, observations, household ties, service interruptions, institutional
succession and local development consequences. Preserve knowledge/provenance and
privacy/diagnostic distinctions where modeled. Retain ordinary grief and community
care without supernatural fear, mythology or a fictional pursuer lineage.
**Documentation:** event/reference schema and causal inspection examples.
**Exit:** a staffing loss or household event produces traceable downstream effects;
retired actors cannot act and copied ordinary records preserve their provenance.

## Cycle 30 — Expose bounded scenario operations and diagnostics

**Complete — 8 November 2026, qualified ordinary-model scope.**
The 45-command ordinary catalogue supports permission-gated read-only preview, bounded idempotent queue/results, current-state execution validation, cancellation and deadlines in CLI programs and native controls. Finite reservations remain with their ordinary task owners. See `docs/OPERATIONS.md` and `content/cycle-30-validation.json`.

**Depends on:** 20, 26–29.
Provide CLI/native operations for ordinary dispatch, scheduling, closures,
inventory, repairs and scenario experiments. Validate IDs, permissions, target
eligibility, deadlines, resource reservations and cancellation. Clearly label
observer tools versus interventions; no campaign route/era selector is imported.
**Documentation:** `docs/OPERATIONS.md`, CLI formats and failure explanations.
**Exit:** invalid/unknown/full-queue requests fail cleanly; every supported
operation has a real human/civic purpose and deterministic exactly-once effects.

## Cycle 31 — Implement complete atomic city checkpoints

**Complete — 8 November 2026, qualified ordinary-model scope.**
Complete ABI/content-qualified city/native/Bronze checkpoints stage and validate all owners, arrays, routes, reservations, calendars, records and RNGs before no-tick publication. Active-task continuation, full 27,000-person restoration, empty worlds, allocation/corruption/publication failures and owner isolation pass. See `docs/PERSISTENCE.md` and `content/cycle-31-validation.json`.

**Depends on:** 07, 14, 20–30.
Coordinate native/Bronze state and all project arrays, RNGs, tasks, routes,
calendars, institutions, stocks and records in an ABI/content-qualified package.
Stage and validate before install; publish complete generations atomically. Save
policy is ordinary city state, with no special rest/location requirement.
**Documentation:** `docs/PERSISTENCE.md`, schema/version/migration policy.
**Exit:** save during travel/work/development restores equivalent continuation;
corruption, wrong content, allocation failure and failed publication preserve live
state and the preceding checkpoint without pointer/identity confusion.

## Cycle 32 — Prove replay, session and batch equivalence

**Complete — 8 November 2026, qualified ordinary-model scope.**
Batch versus single-minute/pause/snapshot schedules, matching record exports, optimized/unoptimized builds and separate-process checkpoint continuation agree. The native clock remains authoritative. See `docs/REPLAY.md` and `content/cycle-32-validation.json`.

**Depends on:** 05, 29–31.
Add seeded replay inputs, deterministic order/event ordering and separate-process
continuation checks. Compare normal/fast/pause, interrupted tasks, session switches,
exports and redraw schedules. Keep host wall time out of authoritative decisions.
**Documentation:** replay contract and diagnostic digest coverage.
**Exit:** identical input programs match authoritative state/events at selected
boundaries across supported build configurations and checkpoint continuations.

## Cycle 33 — Render the actual Canterbury source map

**Complete — 8 November 2026, qualified ordinary-model scope.**
The requested json2png2.c renderer uses the pinned upstream core to reproduce the agreed final assembled overview pixel for pixel, including cropped connected OSM roads, dark buildings, water and reference rail with its restrained palette. Copied people/model/selection overlays and camera state are separate. Actual source qualification and unplaced residents remain explicit. See `docs/MAP.md` and `content/cycle-33-validation.json`.

Agent presentation was pulled forward at the user’s request: the native app now
has known-position map plus signs and an all-agent nongeographic roster overview.
This does not complete the later autonomy, source placement or replay exit gates.

**Depends on:** 08–11, 12, 23, 32.
Adapt the native SDK rendering/compositing path and upstream json2png/source-map
conventions. Show the actual map, correct colors, buildings, roads, waterways and
reference rail; layer native people and model state separately. Do not replace it
with an invented grid or make unqualified ink into operational infrastructure.
**Documentation:** `docs/MAP.md`, asset generation and coordinate conventions.
**Exit:** labels/colors/geometry visually match the selected upstream reference;
rendering and camera changes cannot modify authoritative simulation state.

## Cycle 34 — Add camera, selection and human/institution inspection

**Complete — 8 November 2026, qualified ordinary-model scope.**
Camera transforms, stable-ID map/name/roster selection, live human/building/institution cards and retained unlocated records are implemented. Agent names center immediately from copied known positions; absent positions are prominently identified. Selection rebinds after compaction/load, revision guards reject stale cards, and contrast remains readable in macOS dark mode. See `docs/INSPECTION.md`, `content/cycle-34-validation.json` and `content/cycle-29-34-native-ui.json`.

**Depends on:** 29, 33.
Implement zoom, pan, fit, stable-ID picking, people/building/institution lists and
inspectors. Expose household, occupation, needs, job, service and source-confidence
information. Religion is ordinary affiliation/institution data; unknowns remain
visible. Support unlocated institutions without inventing map markers.
**Documentation:** inspection controls, legends and source-confidence semantics.
**Exit:** selection survives compaction/load, coordinate transforms round-trip,
and readable cards explain actual model state without stale actor references.

## Cycle 35 — Build the native city management interface

**Status: complete.** Persistent native management exposes all 45 ordinary operations, typed CLI-equivalent requests, preview/queue/cancel/query/apply, 11 paged status views, and worker-owned save/load. Native paused operation checks and CLI/read-model parity pass. Evidence: `content/cycle-35-validation.json` and `content/cycle-35-40-native-ui.json`.

**Depends on:** 30–34.
Expose ordinary scenarios, pause/speed, operation preview/queue/cancel, work and
development status, inventories, service rosters and save/load. Default all route
and scenario controls to Canterbury 1940 and its real map. Use project-local
assets/text and remove inherited campaign, power and special-character controls.
**Documentation:** `docs/USER-GUIDE.md` and supported-action inventory.
**Exit:** each supported operation is reachable in the native UI and agrees with
its CLI/state behavior; decisions and inspections obey the sole-clock contract.

## Cycle 36 — Qualify accessibility and alternative input

**Status: complete.** Native keyboard/focus, editable fields, scalable text and color-independent roster symbols are qualified. End/Left/Return selected resident #26999 without ticking. Synthetic boundary tests cover 1–30000 entries; physical assistive-device/controller certification is not claimed. Evidence: `content/cycle-36-validation.json` and `content/cycle-35-40-native-ui.json`.

**Depends on:** 34, 35.
Use native accessibility labels/focus, scalable readable text, color-independent
legends, reduced motion and keyboard traversal. Add controller navigation if
included in the supported interface; ensure long panels scroll with focus.
Declare synthetic versus physical-device test coverage explicitly.
**Documentation:** accessibility/input matrix and tested limitations.
**Exit:** supported workflows are keyboard-accessible without reflex requirements;
focus, dialogs, paused navigation and input do not corrupt or advance model state.

## Cycle 37 — Measure full-population performance and memory

**Status: complete.** Five 1440-minute workloads pass declared CPU/wall/RSS/owned-memory/save/load budgets at 27000/30000 people, including crowded streets, loaded civic/environment and configured lifecycle churn. The normally launched 30000-glyph AppKit benchmark passes at p95 18.41 ms; this is offscreen capture, not physical display FPS. Evidence: `content/cycle-37-validation.json` and `content/cycle-35-40-native-ui.json`.

**Depends on:** 04, 07, 27–36.
Measure full days, busy travel, care/service loads, lifecycle churn, environmental
experiments, crowded rendering and checkpoint staging at default/full human
capacity. Document machine/toolchain, CPU/wall time, peak RSS and frame metrics.
Set explicit supported budgets; optimize only measured bottlenecks.
**Documentation:** performance report and capacity requirements.
**Exit:** declared budgets pass; optimizations preserve replay equivalence. If
compact native storage is required, implement and requalify it only inside the
city profile, preserving ordinary native/Bronze behavior.

## Cycle 38 — Qualify robustness, isolation and dependency purity

**Status: complete.** Clean offline checkout passes owned allocation/publication, malformed/checkpoint, bounded queue, reference/session and replay suites. Isolated original native/contract/actor/hybrid SDK regressions and independent baseline/profile comparisons pass; SDK and parent pin are committed. Evidence: `content/cycle-38-validation.json` and `content/cycle-35-40-native-ui.json`.

**Depends on:** 31, 32, 36, 37.
Exercise every owned allocation-failure point, malformed content/checkpoints,
queue saturation, invalid IDs, interrupted publication and repeated sessions.
Run original native/Bronze contract, actor and hybrid regressions in an isolated
working copy. Audit new guarded SDK branches and project reuse semantically.
**Documentation:** fault/admission/profile/dependency qualification reports.
**Exit:** no sibling dependency or supernatural runtime/content remains in owned
artifacts; clean-clone/offline tests pass; unflagged SDK layouts/behavior match the
baseline and failure paths preserve authoritative state and published generations.

## Cycle 39 — Complete the standalone documentation and provenance pack

**Status: complete.** User/developer/accessibility/performance/robustness/release documentation and provenance are reconciled. The relocatable CanterburyCore archive, eight public headers, resources and ordinary independent frontend pass without Cocoa or an application entry point; extension RNG remains separate from city streams. Evidence: `content/cycle-39-validation.json` and `content/cycle-35-40-native-ui.json`.

**Depends on:** 01–38.
Reconcile source manifests, model assumptions, supported features, limitations,
SDK changes, architecture, content regeneration, developer/user guides, licenses,
qualification commands and migration notes. Explain religion through ordinary
occupations/services/development. Replace inherited claims with measured results.
Package and document the completed neutral core for an independent consumer,
without the Cocoa app entry point. Include policy seams, stable IDs, single-SDK /
clock ownership, content/save compatibility, extension RNG domains and reusable
json2png resources. Add an ordinary mock frontend to demonstrate linking and
stepping the core; supernatural integration starts only after Cycle 40.
**Documentation:** complete local `docs/` set, README and versioned evidence index,
plus the reusable-core handoff in `docs/REUSE-FOUNDATION.md`.
**Exit:** an independent developer can build, run, diagnose and reproduce results
from this checkout and its submodules; no old web page or parent application is
needed to understand or validate the model.

## Cycle 40 — Run and publish the standalone release gate

**Status: complete.** Fresh independent checkout passes Cycles 01–39, SDK isolation, Xcode Debug/Release targets, all full-population budgets and normal native release-app launch/zoom/selection/management checks. Exact source/pins, native observations and report hashes are published locally in content/evidence-index.json and content/cycle-40-validation.json. No remote publication was performed. Evidence: `content/cycle-40-validation.json` and `content/cycle-35-40-native-ui.json`.

**Depends on:** all preceding cycles.
Run a fresh clean-clone release qualification covering source/ABI identity,
all-human population accounting, daily routines and lifecycle, occupations,
religious/civic services, development, logistics, environment, replay, persistence,
actual-map rendering, input/accessibility, resource budgets and SDK isolation.
Qualify the independent ordinary core consumer, default-policy replay and
checkpoint continuation as the handoff for the later game.
Use real native application launch for GUI checks, not a raw GUI binary launched
as a command-line simulation. Record exact submodule commits and report hashes.
**Documentation:** `docs/RELEASE.md`, acceptance matrix and machine-readable report.
**Exit:** every supported requirement has reproducible passing evidence, every
unknown/unsupported historical claim is explicit, no blocking model/identity/
clock/persistence defect remains, and the release contains only the independent
Canterbury human-city source/documentation with its two declared submodules.

## Completion conclusion

All 40 implementation cycles and the standalone release gate have passed. The
ordinary city, native management UI, reusable core and local documentation are
ready for the separate game integration phase. No implementation cycle remains.
The release qualifies explicit technical scenarios and the final assembled map;
it does not establish missing historical staffing, housing, routes or demographic
rates. Those source uncertainties remain visible and require reviewed evidence.
Current results and exact pins are indexed in `content/evidence-index.json` and
`content/cycle-40-validation.json`; detailed limits are in `docs/RELEASE.md`.

## Final acceptance invariants

1. The standalone checkout builds after its submodules are initialized; sibling
   projects and deleted web uploads are absent and inaccessible during testing.
2. Every agent is an ordinary human native body with a stable identity; there is
   no extra population, special reserved character or second movement/needs loop.
3. The native minute owns time; pause, redraw, inspection and alternative input do
   not advance it. Work, travel, care and needs share bounded actor capacity.
4. Source data, modern suggestions, technical model assumptions and unresolved
   historical questions remain distinguishable in manifests, documentation and UI.
5. Religious occupations/institutions have ordinary staffing, routines, services
   and development consequences; no magical mechanics or invented local community
   is smuggled in through reused fields, defaults, fixtures or interface labels.
6. People, jobs, inventories, services and development effects reconcile after
   interruption, retirement, full capacity, replay and checkpoint continuation.
7. Actual Canterbury geometry is rendered and routing is explicitly qualified;
   unsupported streets, entrances, rail services and institution positions are
   not silently invented to make a demonstration pass.
8. All necessary SDK extensions are `CANTERBURY_SIM`-guarded, are committed in the
   SDK submodule with matching parent pins, and preserve ordinary SDK regressions.
9. Release claims describe this application's measured, supported scope; copied
   code, data labels or old qualification reports alone never count as completion.
