# Canterbury as the foundation for the later game

The user intends to complete the 40 Canterbury Details cycles first, then use the
finished city simulation as the foundation of the vampire simulation. This is a
reuse requirement for the current architecture, not an additional supernatural
mode to implement in the ordinary city model.

## Boundaries to preserve through all cycles

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
from the Cocoa frontend. Future release packaging must make the completed core
linkable by another frontend without pulling in `CityApp.m` or its main function.
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

Cycle 39 must document and package the complete neutral C core for reuse, including
its source/link boundaries, SDK profile/pin, headers, content schema, stable-ID
contract, policy seams, save compatibility, RNG domains and renderer resources.
Provide an ordinary mock consumer (no supernatural mechanics) to demonstrate that
the core links and runs independently of the Canterbury application entry point.

Cycle 40 must qualify that consumer's startup, one-minute advance, read-only
inspection and checkpoint continuation along with the standalone city release.
The base-default replay must remain unchanged when no extension policy is present.
The later game integration begins after this gate, using the released contracts
and evidence rather than copying the city implementation or inheriting unchecked
historical claims.

These are future acceptance requirements. They do not claim that the current
bootstrap APIs, the full `CdWorld` implementation or the reusable release library
are already complete.
