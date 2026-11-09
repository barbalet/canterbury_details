# Cycle 01 — upstream source and setting audit

Audited 8 November 2026. Scope: inventory and provenance, not implementation of a
city model or independent historical research. Only the two local submodules were
used. No web upload, sibling application, remote lookup or prior release report
is an input. `content/source-manifest.json` records the exact pins, every tracked
upstream file's size/SHA-256, selected input schemas/identifiers, and test entry
points. Its checker refuses dirty tracked submodules or a checkout differing from
the parent's committed gitlink. Changes require review before rebaselining.

## Setting and source authority

The requested setting is Canterbury, Kent, England in 1940. The approved source
register explicitly sets `setting_year: 1940`. Modern geographical snapshots and
approximate registration coexist with historical vector tracing; they do not
become 1940 evidence by inclusion in the same repository.

The audit establishes no historical population count, age distribution, household
composition or religious census. The pinned SDK originally declared a 26,999
default and 27,000 capacity, reserving the final slot during initialization.
At the user's request, Cycle 03 changes the city-only default/admission to 27,000
ordinary humans with no reserved actor. Exact working SDK changes are recorded in
`content/sdk-working-tree.json` and included in the source inventory; they remain
uncommitted. This required simulation total is not historical census evidence.
Broader capacity/lifecycle qualification remains Cycle 04.
The map includes surrounding settlements; a city/borough total must not be spread
across this canvas and described as a recovered enumeration without a supported
boundary/population mapping.

## Source/data inventory

All paths below are relative to the repository root. JSON selectors and sample
stable IDs are recorded in the machine-readable manifest; no array position is a
proposed persistent object identity.

| Input | Appropriate use | Qualification |
| --- | --- | --- |
| `canterbury/newcanterbury/canterbury.json` | Original classified vector geometry, colors and provenance | Image-derived map representation; preserve its source geometry |
| `canterbury/newcanterbury/README.md` | Vector/renderer and orchard derivation notes | Technical derivation, not survey evidence |
| `canterbury/simulation-map/assemble.py`, `road-traces.json` | Candidate assembly and source-guided tracing | Keep parameters and trace origin; do not invent proximity joins |
| `derived/buildings.json`, `buildings-enriched.json` under `simulation-map/` | Candidate footprints and enrichment | 3,341 candidates; modern labels/use suggestions are separate from 1940 identity |
| `derived/buildings-1940.json` | Candidate institution/building associations | Preserve all geometry/IDs and association uncertainty |
| `derived/road-network.json` | Modern OSM topology, names, grades and restrictions | 12,318 nodes/15,236 links; explicitly modern, approximately registered |
| `derived/road-network-historical-traces.json` | Alternative source-guided road subset | 142 nodes/177 links; neither survey- nor historical-era-qualified |
| `derived/waterways.json` | 211 candidate water components and inferred network | No certified channel hydraulics, crossing legality or navigation service |
| `derived/rail.json`, `rail-corridors.json` | 345 rail-ink components and five reference links | Reference geometry does not establish track junctions, equipment or operating service |
| `derived/report.json`, `building-enrichment-report.json`, `road-connectivity-report.json` | Reported derivation, counts and topology checks | Preserve their limitations; connectivity is not permission to traverse |
| `derived/review-queue.json` | 300 review candidates at this pin | Outstanding review, not automatically approved connections |
| `modern/sources.json`, `osm-canterbury.json`, `osm-roads.json` under `simulation-map/` | Stored snapshot, original OSM IDs/tags and download provenance | Retrieved in 2026; not a 1940 census or historical road inventory |
| `modern/control-points.json`, `control-point-index.json` | Registration/control evidence | Approximate landmark-based registration; rejected controls stay rejected |
| `canterbury/simulation-map/religious-sites-1940.json` | Curated site register, references and setting assignments | Read explicit status, location basis, notes and scenario overrides |
| `derived/religious-sites-1940.json` | Reproducible institution register for later import | 32 Christian records and one Jewish centre; not resident demographics |

The shortened `derived/` and `modern/` paths in this table are inside
`canterbury/simulation-map/`. The primary inventory file records fully qualified
paths. Existing candidate networks retain `traversable: false`; neither display
nor a successful geometry validator approves their use for pedestrian movement.
Do not default to the smaller road set merely because the prior host used it;
network selection/qualification belongs to Cycles 08–11.

## Religion retained as human institutions and work

The register retains religion, denomination, site type, 1940 worship status,
source references, setting assignment and historical-identity confidence. Import
these fields without supernatural effects. Religious roles, employment, premises,
gatherings, education, care, charity, maintenance and burial administration are
future ordinary social/economic uses, subject to source availability.

At this pin, the Old Synagogue is explicitly assigned Judaism and active
religious/community use by an **upstream scenario override**. The upstream README
states that this overrides its historical closure/church-hall record. Preserve
both the selected scenario status and this qualification; do not describe active
1940 worship as verified fact. This is present in the approved submodule itself,
not an instruction imported from a sibling project.

Five records remain unlocated; approximate positions are not surveyed precincts.
Building matches are candidates, and a positioned institution need not have a
confirmed footprint. Closed/review-required Christian sites remain distinct from
active worship. The dataset does not prove that other faiths were absent, and no
nearby person's membership is inferred from a site's affiliation. Full calendars,
clergy headcounts, lay staffing and local service routines are not supplied here.

## ApeSDK integration inventory

| Source/API | Available mechanism | What it does not establish |
| --- | --- | --- |
| `universe/universe.h`, `universe/sim.c` | `CANTERBURY_SIM` capacity, `canterbury_cycle_handler`, `sim_canterbury_configure` | A complete city or verified human demography |
| `entity/being.c`, native entity headers | Physical bodies, positions, energy and guarded city awake state | Two simultaneous movement/needs authorities are not permitted |
| `bronze/actors/bronze_actors.{h,c}` | World-scoped IDs, lookup, reset, compaction/rebind, staged sidecars | Persistent pointers or cross-world ID equivalence |
| `bronze/work/bronze_work.{h,c}` | Guarded city claim/close and one native work interval | A separate per-job ticking clock |
| `bronze/tasks/bronze_tasks.{h,c}` | Bounded resumable task infrastructure | Period-correct city jobs or employer/service policy |
| `bronze/content/bronze_age_full.bronze` | Vocation IDs and engine occupation capability examples | An approved 1940 occupation distribution; audit period suitability later |
| `bronze/hybrid/bronze_hybrid.{h,c}`, `gui/draw.c` | Guarded overlay/native drawing bridge | A ready-made standalone Canterbury UI |
| `sim/sim.h` and actor/sidecar modules | Landscape/weather continuation and serialized actor mechanisms | A complete city household/job/route/inventory checkpoint |
| `bronze/CANTERBURY.md`, actors/work/tasks/contract READMEs | Existing profile and module contracts | Prior client-specific special actors or qualification results are not new-project requirements |

The current bridge requires `CANTERBURY_SIM=1` with `BRONZE_SIM=1` consistently
across translation units. Native time and the observed actor clock advance before
the city callback; the callback replaces island autonomy. Human semantics and
ordinary city policy must be implemented by the future host. Further SDK changes
are authorized only behind the city guard and require baseline native/Bronze
regressions in their implementation cycles.

## Licenses and attribution inventory

| Material | Local evidence | Recorded disposition |
| --- | --- | --- |
| Standalone project | Root `LICENSE` | MIT; retain its notice in distributed substantial copies |
| ApeSDK software | `apesdk/LICENSE` | ApeSDK Open Source License; permissive notice/conditions retained verbatim upstream |
| Canterbury project | `canterbury/README.md`, Data and attribution | References an ApeSDK license URL; no root LICENSE/COPYING is present at this pin. The linked revision's full text is not vendored here |
| OSM snapshots and derived enrichment/roads | `modern/sources.json`, `modern/README.md`, JSON attribution/license fields | Preserve “© OpenStreetMap contributors”, `ODbL-1.0`, provenance and existing license links; do not represent these datasets as covered solely by the host's MIT license |
| Historical raster/tracing and third-party embedded material | Source files and upstream notes | Detailed per-asset rights/redistribution clearance is not established by this audit; carry U12 forward before packaging |

This is a record of the local notices, not a claim of legal clearance for every
asset. Public repository access does not resolve missing asset-license evidence.
Original filenames, IDs, comments and notices remain available at the pinned
upstream revisions and in the file inventory. Later copied code needs an explicit
reuse/attribution ledger in Cycle 02.

## Existing tests and reproducibility

Canterbury exposes offline geometry/building validators and Python tests for
assembly, road completion, OSM topology, enrichment and religious-site generation.
Run them in an isolated copy: some fixtures/regeneration tools write derived
outputs. Existing SDK entry points are `bash test.sh`, the Bronze contract checker,
and the actor/hybrid scripts. They are inventoried, not claimed as new-host runtime
qualification. Compiling SDK profile probes and native runtime tests starts with
the subsequent build/profile cycles.

Cycle 01 evidence is recorded in `content/cycle-01-validation.json`: 547 tracked
upstream files hashed, 27 input contracts mapped, both map validators passed, and
all 32 upstream Python tests passed. Reproduce:

```sh
python3 tools/audit_sources.py --check
python3 tools/verify_cycle01.py --check
```

The second command checks the stored evidence, not a fresh execution of every
upstream test. Use `python3 tools/verify_cycle01.py --run` to repeat the offline
Canterbury validator/test suite in a temporary copy, regenerate evidence, and
confirm the original submodules remain unchanged. No network or sibling checkout
is consulted. A source/pin change invalidates the inventory until reviewed.

## Cycle 07 guarded storage extension

The reviewed SDK working tree additionally provides city-only
`sim_canterbury_storage_bytes` / `sim_canterbury_attach_storage` in
`universe/universe.h` and caller-owned registry size/init functions in
`bronze/actors/bronze_actors.h`. Native city groups now use heap storage; normal
native/Bronze preprocessing and layouts remain baseline-equivalent. Exact file
hashes remain in the inventory and working-tree review. See `docs/MEMORY.md` for
ownership, payload accounting and publication limits. No additional factual
Canterbury source or uploaded web document is required.
