# Canterbury DSL compiler

Cycles 01–04 provide authoring, semantic compilation and pack publication. Cycle 05 connects the population startup profile to the CLI and AppKit world lifecycle.

Run `make scenario-pack` after preparing the audited city content. The compiler writes `build/content/CanterburyScenario.json`. `make scenario-check` checks the current pack and runs the DSL acceptance suite. Both native app build paths compile and bundle the same pack. To compile another source against a particular city pack:

```sh
python3 tools/compile_canterbury_dsl.py input.canterbury \
  --city build/content/CanterburyCity.bin --output build/custom-scenario.json
```

`--require-activation` checks authored prerequisites before publication. The bundled draft intentionally reports blockers. Runtime admission and execution belong to later cycles.

## Schema and migration

The [typed schema](../../content/canterbury-dsl-schema.json) lists the accepted fields for each declaration. Fields are explicit. Nullable fields accept `unset` or `null` for an unconfigured value. Numeric zero is a configured value and is checked against the field's domain. The parser enforces an 8 MiB file ceiling, 64 nested list levels, signed 64-bit integer representation and finite numeric values. It reports syntax and typed field errors with file, line and column.

A 0.1 source must match the pinned 300-role draft catalogue. Compilation preserves its authored version and normalizes its scenario declaration to schema 1.0. To author 1.0, set `language_version "1.0";` and add the required execution declarations with their typed fields. The initial schema describes household membership, employers, funded posts, shifts, curricula, credentials, stock and task programs. It preserves draft quantities and unset values. Wages, rates, facilities and qualifications receive concrete scenario values in the cycles that implement their behavior.

Programs reference named task records. The initial supported binding is `task cook { handler "work"; recipe "meal"; units 1; }`. The compiler resolves it to the existing work opcode and recipe index. The future dispatcher supplies the live actor and assigned job. Training and authorization operations remain explicit interventions. New production and service bindings extend this schema in their domain cycles.

The pack contains sorted declarations, typed bindings, authored and schema versions, and hashes for the schema, actual city pack, occupation records, resource records and operation catalogue. Its identity hashes the canonical payload. Comments, whitespace and declaration order do not affect identity. The compiler checks the runtime map header, length and checksum and resolves named premises against its building IDs. A mismatched map or altered pack fails preflight. The compiler stages output in the destination directory and replaces it atomically after checking that inputs still match.

## Declaration ownership and acceptance inventory

Every field is enumerated in the schema. The pack retains each declaration ID, including every role, link, resource, recipe and service. Apply the relevant ownership and acceptance row to each record of that kind. Each runtime owner listed below is the current integration point or the intended extension point for the later cycle.

| Declarations | Runtime owner and dependencies | Runtime acceptance |
| --- | --- | --- |
| scenario, limits, policy | City content and `CdScenario`, `cd_world_open`, native clock and AppKit worker | Cycles 05, 16, 19 and 20 verify publication, one clock, budget coverage and packaging |
| cohort, activity, household | `cd_population.c`, `cd_households.c`, lifecycle and placement | Cycles 06–07 reconcile every cohort and household, guardians and premises |
| workforce, employer, post, shift | `cd_jobs.c`, `cd_work.h`, movement and employer funding | Cycle 08 checks each sector, funded posts, shift coverage and both commute directions |
| career, occupation, competence | `cd_occupations.c`, `cd_human.h`, assigned jobs and sparse skill state | Cycles 09 and 13–15 check eligibility and a successful task for each role |
| qualification, curriculum, credential, advance | City job, training and lifecycle state | Cycle 10 tests each link and rejected prerequisites, including capacity and mentor loss |
| resource, stock | `cd_economy.c`, `cd_food.c`, custody and reservations | Cycle 11 tests each resource and its unit, conservation and failed transfer |
| recipe | `cd_work.c`, city inventories, equipment and native minute grants | Cycle 12 executes each recipe and tests interruptions, byproducts and accounting |
| service | `cd_services.c`, community and office state, staff and premises | Cycles 13–15 test delivery, rejection and capacity for every service |
| task, program | `cd_operations.c`, typed native work handlers, stable runtime actor/job references | Cycle 05 runs a small fixture, then Cycles 13–16 qualify each role's task binding |

The baseline draft has 838 declarations. Its previous checker suite and the new compiler suite cover the authoring boundary. Existing city acceptance entry points are `tools/verify_city_cycles.py`, `tools/verify_work_cycles.py`, `tools/verify_civic_cycles.py` and `tools/verify_observer_cycles.py`. The existing scenario performance baseline lives in [performance budgets](../../content/performance-budgets.json), with default and full population cases plus crowded, busy and churn workloads. The full DSL workload and native presentation evidence are Cycle 19 deliverables.


## Native population startup

Compile a population fixture and load the resulting startup pack.

```sh
python3 tools/compile_canterbury_dsl.py content/scenarios/population-32.canterbury   --startup build/content/population-32.cdstart
./build/CanterburyDetails --scenario build/content/population-32.cdstart --minutes 4
```

The AppKit Simulation menu offers **Open scenario startup…** for the same file.
The app displays its identity and Restart reuses its population settings.
The startup compiler accepts a version 1.0 population fixture with five age
bands. A compact versioned record carries the map length and checksum, the
scenario identity, native seed, date and exact cohort counts. The native loader
validates the record before replacing the world. Scenario identity in saved
checkpoints is deferred to Cycle 17.

The 32-person and 27,000-person fixtures are checked in. The same startup
path was exercised at 30,000 with scaled cohorts. Household generation reserves
adult slots, assigns minors to a guardian in their household and uses the native
stable identities and clock. Legacy starts keep their existing population policy.

The population-only profile accepts scenario and cohort declarations. Full
scenario tasks, work posts and supplies still use the draft pack and its
activation diagnostics. School and household activity scheduling remains open.
The base map pack and population fixtures retain their unplaced startup.
The technical access pack below provides an explicit actual-map placement
scenario.


## Native definition catalogue

Run `./build/CanterburyDetails --dsl-definition summary` after building the CLI.
Replace `summary` with a family, role, transition, resource or recipe ID to
inspect the corresponding compiled definition. Examples include `carpenter`,
`carpenter.1`, `grocer_1_to_2`, `wheat_grain` and `mill_wheat`.

Native builds generate these immutable definitions from the bundled DSL. The
definition identity follows canonical authored records. Existing occupation
IDs and job assignment use the existing occupation catalogue. The new records
remain draft entries. The five manual trial recipes specify illustrative
minutes and tools as scenario assumptions. All five initial material recipes and three additional trials have named trade bindings.


The native refined stock API in `src/cd_dsl_catalogue.h` supports explicit
imports and keyed reservation settlement. Each premises has 16 sparse kind
slots and each kind can hold one outstanding reservation there. Settlement
returns the reserved quantity or consumes it exactly once. Checkpoints
include the stock slots and conservation counters, and bind the generated
definition identity through the strict ABI. Earlier checkpoints require the
migration work planned for Cycle 17.

The native tests cover all 72 kinds and an active reservation across save and
load. Batch quality and vehicle integration remain open. Refined hand cargo can
move between qualified premises through the existing street and entrance
APIs. Pickup and delivery each use a native grant while the carrier is inside
the respective building. The trial carrying limit is 5,000 g or ml, or one
counted item. Cancellation clears a pending action and retains carried
stock. Retirement accounts for carried stock as loss.

Native callers submit a named trial with `cd_world_refined_work_submit`.
The work API uses the existing shift, physical workplace, mentor, wage and
native minute checks. The compiler accepts `handler "refined-work"` for the supported material recipes and preserves the recipe ID in the binding.
Future scenario dispatch resolves that ID against the runtime catalogue.
The current DSL startup profile still loads population fixtures.



`cd_world_dsl_job_create` creates named technical trade vacancies, and
`cd_world_dsl_certify` records a role-specific assessment for an adult.
Assign workers with the existing job API. The 50 technical roles are available
through these explicit calls. The draft scenario still carries its authored
activation flags. CLI definition inspection reports the technical recipe
separately. Existing saves follow the strict checkpoint ABI and earlier
formats require migration.


## Actual Canterbury access trial

Run `make access-check` to build the technical access pack and verify both
the 27,000-resident home entry and a paid actual-map commute. The output
report is `build/access-trial-validation.json`. To start the proposed
population against the same pack:

```sh
python3 tools/compile_canterbury_dsl.py content/scenarios/population-27000.canterbury   --city build/content/CanterburyAccessTrial.bin --startup build/content/access-27000.cdstart
./build/CanterburyDetails --city build/content/CanterburyAccessTrial.bin   --scenario build/content/access-27000.cdstart --allocate-homes --place-homes --minutes 0
```

At minute zero the CLI reports the residents placed at their home street
nodes. Native grants complete the queued home entrances as the clock runs.
The acceptance fixture starts at 08:00 and checks interior arrival for
every resident. The staffed fixture uses a qualified industrial premises,
one funded named milling post and the existing native schedule.
