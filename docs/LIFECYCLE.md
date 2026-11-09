# Human lifecycle — Cycle 14

The initial population remains 27,000 and capacity remains 30,000. Civil person
arrays reserve all capacity up front, independently of the current count. Live
records remain sorted by monotonically issued native ID. Native slot compaction
never changes civil identity. Every mutation requires the owning closed native
interval; callback mortality is committed immediately after each native minute.

`cd_world_admit` requires a bounded UTF-8 name and explicit basis, a valid birth
date no later than the current civil date, a supported category, established
housing with spare reviewed capacity, and a live adult guardian for a minor.
`cd_world_birth` additionally validates its adult guardian and optional other
parent in the same household. There is no automatic fertility rate: neither
submodule supplies one. Parentage is preserved independently of the live guardian. Newborn names use
the SDK vocabulary and inherit the guardian’s surname when it fits the bounded
name field; the deterministic household naming rule is the fallback.
Births get no invented map location, food or religious affiliation. The technical
birth fixture transfers 1,280 abstract SDK energy units from the guardian to the
new body, requiring a 2,560-unit parental reserve. This is a simulation resource
rule, not a reproductive physiology or historical claim. Arriving adults have an
explicitly initialized native body energy buffer but carry no free meals.

Birth dates and age are Gregorian civil fields. Birthdays derive from native
minutes and update exactly once; age 18 clears a now-unnecessary guardian link.
A February 29 birthday advances age on March 1 in a non-leap year. No unsupported
age-related mortality rate is enabled. Household moves/transfers retain the
Cycle 13 capacity and guardianship rules.

Departure/death first validates archive space and a replacement adult guardian
in the same household for every dependent. If none exists, the event fails
closed: explicitly transfer/reassign dependents before retrying. Native retirement
then compacts bodies, while civil retirement releases route occupancy, visitor
reservations, pending food/feed/care actions, clinic beds, spouse links and property
ownership. Surviving minors receive the replacement guardian. Archived parentage
remains historical information, never a live actor handle. Vacancies are not
silently reassigned. Job/service systems added later must extend this cleanup.

A bounded archive holds 60,000 retirement records, including identity, date of
birth, parentage, age, reason, basis, last known position and possessions. Archive
saturation is diagnosed before mutation; no record is silently dropped. A death
retains carried food in that last-location cache; retrieval is not invented.
A departure exports its carried stock. Neither operation teleports food home.
`initial + admissions == live + departures + deaths`; births are a subset of
admissions. Empty worlds remain inspectable, tickable and admissible into an
existing reviewed empty dwelling. Immutable snapshots survive retirement/close.

There are no allocations after validated admission begins publication. World
construction stages all arrays before replacing the live singleton. Tests cover
invalid UTF-8/dates, invalid/full housing, full archive, guardian repair/refusal,
cleanup, empty-world snapshots, birthday transitions, a 29,999→30,000 birth and
rejection of a further birth without changing state.
