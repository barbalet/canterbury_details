# Households and dwelling capacity — Cycle 13

A household is a stable technical social group, not a building or a presence list.
The default population forms 6,750 synthetic groups of four. The first two adults
are symmetric fixture spouses; children reference a live adult guardian in that
group. No historical marriage, gender or religious affiliation is inferred.

Housing starts **unknown**, distinct from explicitly known unhoused. A household
move to dwelling ID zero records known unhoused status. A move to a building
requires an explicitly reviewed/fixture capacity; footprint area and modern use
never imply capacity. The real source ledger currently admits no capacities.
Household relocation atomically updates every member's dwelling and reservation
counts, with no physical movement or property-ownership change. Capacity reserves
space for resident members even while absent. Registered visitors reserve extra
space; physical nonmembers and pending entrance trips consume remaining capacity
without counting a reserved member twice. Absence retains the home reservation.
A reviewed entrance with unknown capacity cannot claim a safe admission limit; moving a visitor into residence removes the duplicate visitor reservation.

Membership transfers update both households and dwelling occupancy. A minor must
have an adult guardian in the destination household. A guardian cannot leave
existing dependents without first reassigning them. Partnerships remain symmetric
and may span households; self/child/conflicting partner assignments fail. Foreign
world references are rejected. Cycle 14 now handles lifecycle removal and guardian/spouse repair explicitly,
rather than hiding those changes in a household move.

Ownership, residence, visiting and physical presence are separate. An explicit
adult owner can own a dwelling while residing elsewhere. `cd_world_visit` registers
or releases a visit capacity reservation, not actual entry. Actual presence changes
only by native movement through a reviewed connector. A household move cannot
teleport members, empty a physically occupied building, or imply the old household
ceased to exist. Empty technical groups retain their stable IDs.

Summary totals reconcile `housed + known unhoused + housing unknown == people`.
Visitor reservation totals and known native positions are separate counts. Tests
exercise full dwellings, unknown capacity, atomic rejection, guardians/dependents,
visitor conversion, independent ownership, relocation and absence without capacity
release. Construction failure sweeps preserve the prior world, identity and digest;
closing worlds, snapshots and content returns tracked allocation bytes to zero.
