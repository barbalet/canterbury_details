# Canterbury as the foundation for the later game

All 40 Canterbury Details cycles are complete. The finished ordinary city is
ready to serve as the foundation for the separately requested vampire simulation. This is a
reuse requirement for the current architecture, not an additional supernatural
mode to implement in the ordinary city model.

## Released reuse boundaries

The Canterbury core owns ordinary people, stable native identities, physical
movement, time, work budgets, housing, occupations, institutions, economy,
services, environment, content admission and persistence. The later game consumes
that core through the versioned `CdWorld` / `CdContent` / snapshot / operation APIs.
Do not create a second population, native SDK instance, position/energy authority
or clock in the game frontend. The base starts with 27,000 ordinary people and
reserves no special-character slot. Any later game admission/transformation policy
must explicitly respect the released capacity and identity contract.

Keep presentation separate: `CdMapView` receives map records and selection
callbacks; it neither runs simulation minutes nor implements game rules. The
current shared Make/Xcode source definitions already keep native city code apart
from the Cocoa frontend. Cycle 39 packages the completed core in `build/core/libCanterburyCore.a`,
linkable without `CityApp.m`, the CLI main or any Cocoa code.
The exact `json2png.c` reference-rendering pipeline and map provenance can be
reused independently of either game interface.

Use neutral policy interfaces when ordinary city features need variation, such
as schedules, access, resource consumption or scenario events. A later game may
supply a separate policy implementation and companion state. There must be one
ordered native-minute transaction and one work/movement budget: an extension must
contribute intents or replace an approved policy seam, not run duplicate human
and game update loops. Do not add speculative mythology fields, game-specific
conditionals or generic hooks with no present use to base modules now.

All references crossing that boundary use stable person/entity IDs and runtime
world tokens where appropriate. Borrowed body pointers and array slots must not
escape into persistent game state. Companion save data must name its base save /
content generation, policy version and owned schema; admission/publication of the
combined state must be atomic. Keep extension RNG domains independently named and
versioned so adding a narrative draw does not perturb ordinary city RNG streams.
Scenario policy and human defaults remain explicit and independently testable.

The later game owns mythology, narrative, special abilities and any supernatural
rules in its own layer. The Canterbury core retains religion as ordinary community,
occupation, institution and development information. A game can interpret that
information through its own rules without rewriting the source facts or changing
the base city's institutional semantics.

## Handoff after Cycle 40

Cycle 39 packages and documents the neutral C core for reuse, including
its source/link boundaries, SDK profile/pin, headers, content schema, stable-ID
contract, policy seams, save compatibility, RNG domains and renderer resources.
The ordinary mock consumer demonstrates linking and running independently of
the Canterbury application entry point.

Cycle 40 qualified that consumer's startup, one-minute advance, read-only
inspection and checkpoint continuation along with the standalone city release.
Base-default replay is preserved when no extension policy is present.
The later game integration begins after this gate, using the released contracts
and evidence rather than copying the city implementation or inheriting unchecked
historical claims.

The packaged core, public headers and ordinary mock consumer now implement this
handoff contract. Cycle 40 release evidence qualifies their independent linking,
sole ownership, extension-RNG purity and checkpoint continuation. The later game
remains a separate consumer; no supernatural rules are added to this base.
