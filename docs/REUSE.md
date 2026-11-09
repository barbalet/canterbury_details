# Reuse and attribution review

Cycle 02 review, 8 November 2026. The user authorized consulting the earlier host
as an implementation basis. Factual authority remains the local `canterbury` and
`apesdk` submodules. No implementation text was copied from the reference host in
this cycle. The declaration-only `src/cd_api.h` is a new local interface design.

`content/reuse-ledger.json` records the reviewed reference repository/revision,
per-file SHA-256, selected patterns and required removals. These are archival
review facts. The local architecture checker never opens that reference checkout;
its removal cannot break this project's checks, build or runtime.

## Dispositions

| Component | Disposition and required adaptation |
| --- | --- |
| Shared model header / world initialization | Define a new opaque human-city aggregate and lifecycle; do not retain special actor kinds or rename the old aggregate |
| Polygon rings/holes, projection and connectors | Selected geometry patterns can be adapted, preserving ordinary collision and upstream review confidence |
| Road routing / movement | Use ordinary topology and one native position; remove form/holy/tide-carriage exceptions; design for the larger OSM graph rather than importing the old small-graph allocation policy |
| Population / households | Derive inputs locally; retain civil/relationship patterns only; no special admission, invented demographics or inherited faith assignments |
| Meals, sleep, care, retirement | Ordinary native energy/care patterns only; eliminate supernatural branches and narrative appointment exceptions |
| Jobs, skills, shifts and mentors | Ordinary work-budget patterns; remove special actor rules and hardcoded religion-to-job shortcuts; use sourced qualifications |
| Goods, money and logistics | Reuse conservation/reservation ideas; implement ordinary custody and vehicles without earth/soil/passenger exceptions or assumed rail operations |
| Record/history and community code | Preserve ordinary provenance and explicit membership/office patterns; remove supernatural pursuit, Dominion, magical protection and corruption gameplay |
| Orders / checkpoints | New ordinary operation catalogue and standalone schema; no old actor authority, payloads or compatibility aliases |
| Rendering, camera and native panels | Source-map/read-model/control patterns only; neutral titles and ordinary selection/operations; no power, story or campaign UI |
| Content/map generators | Reimplement against this checkout's submodules; no sibling paths, old generated tables, uploaded chapter hashes or developer-local asset paths |
| Supernatural, campaign, episode, form/access and campaign-generation subsystems | Excluded; no runtime dependencies or compatibility stubs |

Mixed modules must be reviewed function by function when implementation starts.
A renamed field, uncalled code path or hidden menu item is not removal. Replace
special-type branches with explicit ordinary human policy; keep no dormant
capability that can be enabled by a stale scenario file.

## Dependency and attribution rules

Use SDK implementation through `apesdk/`; preserve its local notices and the
Cycle 01 source inventory. Imported map records keep source IDs, confidence and
OSM attribution/license metadata. The host's MIT license does not replace those
notices. The previous reference checkout has no top-level LICENSE/COPYING in the
reviewed snapshot; no implementation text is copied here on that basis. Prefer
new code written against the approved submodule contracts; retain applicable
origins/notices for any later selected implementation copy and record it in this
ledger before claiming it local and qualified.

Record an actual copy with source identity/hash, local destination, retained
notices, semantic changes, removed dependencies and local acceptance evidence.
Until then, `adapt_selected_algorithms` and `reimplement_around_selected_algorithms`
are review decisions, not claims that adaptation is complete. Historical data
must still be re-derived from the approved submodules, not copied from an older
host's policy JSON or transcriptions.

SDK updates remain inside the submodule with all city-only declarations, storage
and behavior protected by `CANTERBURY_SIM`. Update the parent's gitlink only after
that SDK change is committed and ordinary regressions pass. Cycle 02 changes
neither submodule and imports no previous project's release status.
