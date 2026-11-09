# Finite household provision and food work — Cycle 16

Food uses abstract raw units and prepared meal portions. These units and the
one-raw→one-meal recipe are disclosed technical assumptions, not historical
rations or production yields. No spontaneous supply, spoilage rate or price is
inferred. Unsupported spoilage remains disabled.

Separate stocks belong to carried inventories, stable households, configured
stores/producers, and archived death possessions. Initial residents carry three
portions each; births and later arrivals receive none. Scenario imports require an
explicit basis, are recorded in the ledger and are bounded to 4,096 units per
endpoint. Carried meal/raw capacity is 12 units each. Household/store transfers
and production move one unit per granted native minute. Production requires an
explicit food qualification, configured producer, available raw input and output
space. It consumes an input before publishing an output, exactly once.

`cd_world_food_order` queues a bounded fetch/deliver/produce intention, separately
from travel. A capable adult must physically reach the actual endpoint through a
reviewed entrance. Home access is restricted to current household membership and
is rechecked every minute. Changing membership/location/capability blocks the
remaining order. Saturation or shortage waits with remaining units intact;
cancellation discards only unperformed units. No inventory mutation substitutes
for street or entrance traversal. Shops are scenario configurations, not inferred
historical businesses from a modern annotation.

A direct dependent-feeding request likewise queues a physical transfer for a
capable, authorized adult's native grant. The dependent subsequently eats during
needs processing. Invalid children/foreign actors/remote caregivers cannot order
provisioning. Automatic household meal consumption is restricted to physically
being inside one's assigned premises. No meal teleports from home to a person
walking elsewhere.

The read-only ledger verifies:

```
initial meals + meal imports + produced = meal stock + consumed + exported meals
raw imports = raw stock + produced + exported raw
```

Stock includes cached possessions of deceased people, separately reported as
unavailable caches with last-position provenance. Departures export carried
stock. Retirement cannot silently lose it or move it to another building.
Imports and ledger totals are checked for bounds. There is no free replenishment
masking an unmet demand.

The two-building fixture makes a person exit a reviewed door, walk the street
polyline around both footprints, enter the other building, collect finite meals,
walk back and deliver to its household. Other fixtures prove qualified production,
underage rejection, capacity/shortage waits, cancellation, physical dependent
feeding and day-length conservation. The actual Canterbury source map still has
no automatically approved shops, dwellings or connectors.
