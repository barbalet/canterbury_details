# Bounded ordinary scenario operations

Cycle 30 exposes `cd_observer.h`, the CLI and native Simulation → Scenario
operation. Observer is the default. Preview/queue/cancel/flush require explicit
intervention permission (`--intervene` or the native checkbox). Query and export
are observers. There are 128 retained request/results; full queues fail without
allocation or mutation. Results remain available rather than silently recycling IDs.

Preview checks current references, supported inputs and preliminary eligibility/
resources without reserving anything. Queue accepts one immutable request key;
identical retries return its existing ID, conflicting retries fail. Execution
revalidates against current state and invokes the same finite ordinary APIs as
other city work. An earlier preview is no guarantee that access, staff, wages,
stock or capacity will remain available. Ordinary tasks reserve at execution;
the operation queue does not reserve a second copy of stock. Every result stores
status, reason, resulting task/entity ID and causal records. Failed requests have
no fabricated success. Cancelling a pending operation is idempotent; already
executed underlying tasks require the corresponding ordinary cancel operation.

Requests are processed once in insertion order before each next native minute.
`flush` processes due requests at the current closed minute without advancing
time; `step 0` processes nothing. `earliest`/`deadline` use native elapsed minutes,
up to 10,080 minutes ahead; deadline is exclusive. Zero deadline defaults to
queue time +10,080. Missing/evicted queue provenance fails an undated request
closed. Retired/foreign actors fail. Each applied result can be queried by ID;
CLI programs return an error when execution fails, including failures during step.

## CLI program

```
./build/CanterburyDetails --city build/content/CanterburyCity.bin \
  --program experiment.txt --intervene --minutes 0 --save session.city
./build/CanterburyDetails --city build/content/CanterburyCity.bin \
  --load session.city --minutes 60 --records public-records.json
```

Programs support comments beginning `#`, `step N`, `flush`, and commands:

```
key|verb|actorID|target|secondary|units|value|starts|duration|earliest|deadline|flags|auxiliary|label|basis
```

Exactly 15 fields, unsigned decimal numbers, no escaping/evaluation, no `|` in
text, ≤95 bytes each for label/basis, ≤1,023 bytes per command and 10,000 lines.
Basis must be nonempty explicit provenance. Unused numeric fields should be 0.
Actor IDs are stable logical IDs resolved to current runtime references. Actor 0
means no acting person. General units/value are limited to UINT32; more specific
limits apply. `starts` uses absolute simulated civil minutes (native elapsed +
scenario origin) for gatherings/pressures. They are explicit experiments, not
inferred historical events. Programs commit their valid prefix; they are not a
whole-file transaction. Unknown targets/invalid inputs fail with diagnostics.

A source-independent example for any adult synthetic person (ID 1 at startup):

```
1|train|1|0|0|15|0|0|0|0|1000|0|0||explicit ordinary training experiment
flush
step 1
```

The actual source map has no qualified entrances/street edges, known staffing or
occupancy. Related commands correctly reject unsupported setups; do not substitute
invented geometry. For a technical fully operational street fixture use the
replay tests. Source data IDs and runtime IDs differ: runtime IDs are exposed in
native building/institution cards and compiled via `compile_runtime.compile_model`.
Newly created IDs are returned in operation results; native Query accepts that
operation ID. Native Cancel takes an operation ID; Flush needs no command line.

## Command parameters

All verbs below have ordinary human/civic effects through existing validated APIs.
Kinds/masks are the public enums in `cd_city.h`, `cd_human.h`, `cd_work.h` and
`cd_civic.h`. This table names non-default fields in addition to key/basis.

| Verb | Fields and effect |
| --- | --- |
| place / move | actor, target source node; initial placement / physical travel |
| enter / exit | actor, target reviewed door; flags bit0 key, bit1 emergency authorization |
| close-road | target walk edge; flags 0 reopen / 1 close |
| lock | target building; flags 0 unlock / 1 lock |
| train / authorize | actor, units skill mask / civic permission mask |
| employer | target building, units work capacity, label employer name |
| open-employer | target configured employer, flags 0 close / 1 open |
| create-job | target workplace, secondary door, units occupation ID, value wage, starts/duration shift start/end (minute of day), auxiliary institution; flags bit0 apprenticeship, bit1 key |
| assign-job | actor worker, target job, secondary mentor logical ID (0 none), flags secondary-job slot |
| unassign-job / close-job | target job; release worker / close vacancy |
| work | actor, target job, secondary project ID or refined recipe index, value recipe enum (6 for refined work), units production quantity |
| cancel-work | actor, target own work ID |
| import-goods | target building, value good enum, units finite imported quantity |
| import-food | target building, units meals, value raw food, flags producer bool; configures store and imports finite stocks |
| food-store | target building, flags producer bool; explicit store configuration |
| cargo | actor physically at target seller building, value good enum, units load, flags delivery-to-household bool |
| import-money | actor for personal account; target household/employer ID otherwise; units model tokens; flags account kind |
| project | target building, flags development kind (repair/capacity/use), units new capacity, duration labor minutes, label use |
| cancel-project | target building, secondary project ID |
| roster | target job, units capability mask, label ordinary service name |
| office | target institution, secondary job, flags ordinary title enum |
| qualify | actor, target institution, flags supported clerical title |
| membership | actor, target institution, flags join bool |
| dispatch | actor column recipient (burial may reference archive), target roster/office, flags religious-provider bool, secondary destination, value service kind, units deadline horizon, duration work minutes |
| cancel-service | target service request ID |
| gathering | target office, value service kind, starts future civil minute, duration window, units attendee cap, flags members-only bool; one occurrence |
| attend | actor, target gathering, flags participation bool |
| clinic | target building, units beds, value imported medicine |
| import-water | target building, units finite response water |
| wear | target building, units technical condition loss |
| condition | actor, flags condition enum, units severity, value recovery minutes |
| pressure | target building/edge or 0 citywide, flags pressure kind, value intensity, starts future civil minute, duration window |
| cancel-pressure | target pressure ID |
| shelter | target reviewed building, units capacity |
| shelter-person | actor, target shelter building (0 releases intention) |
| link-institution | target institution, secondary reviewed premises |
| operate-institution | target institution, flags bit0 operating / bit1 explicit scenario override, label ordinary office |
| vehicle | target crew job, flags handcart/stretcher kind, units carrying capacity |
| transport | target vehicle, secondary source, auxiliary destination, actor column passenger (0 none), value good enum, units cargo, flags meals bool |
| stop-vehicle | target vehicle, flags stop bool |
| unload | target vehicle; physically unload current custody here |

No supernatural action, campaign route or era selector is included. Diagnostic
records/export require explicit scope (`--diagnostic` with `--records`, or native
export checkbox); privacy defaults to public. See [records](RECORDS.md).
