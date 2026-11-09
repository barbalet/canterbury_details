# Explicit ordinary scenario pressures — Cycle 28

There are no automatically inferred 1940 raids, weather, blackout orders,
evacuations, ration levels, fires or historical damage. In particular, a source
note about 1942 is not an instruction to damage the 1940 map. The source register
identifies missing historical event/rate inputs. Every enabled disturbance needs
an explicit bounded future start, duration, intensity, target and technical basis.
Up to 256 keyed records provide retry-safe causes and observable phases.

| Pressure | Disclosed technical rule |
|---|---|
| Rain | Level 1–3 divides native walking budget by 2–4; no terrain rewrite |
| Fire | Ignite named source premises at intensity 1–1000; no invented spread |
| Damage | One-shot condition loss, at most 1000; source geometry unchanged |
| Street disruption | Overlay closure on a qualified source walk edge |
| Alert | Observable explicit alert state; no automatic population movement |
| Blackout | Stops scheduled public gatherings during technical night 22:00–06:00 |
| Ration | Explicit 1–12 personal meal-consumption limit per simulated civil day; creates no supply |

Fire persists after the ignition script ends until a real response removes it.
Each burning premises loses one condition point per ten simulation minutes; that
rate is a technical experiment, not a historical estimate. Zero condition or
active fire blocks ordinary use/entry. Authorized fire response can use the same
reviewed emergency connector. Response removes 20 intensity units per native work
grant and consumes one carried water unit. Damage persists until ordinary repair;
restoring a pressure overlay never silently restores material damage.

Weather, alert, blackout and ration overlays expire deterministically. Overlapping
rain uses the maximum level; ration uses the stricter limit; closure overlays
compose with existing manual closures rather than overwriting them. Cancellation
removes future pressure, not already-caused damage/fire. Source map identity,
footprints, barriers and route qualification stay immutable.

Shelters require explicit safe premises, a reviewed entrance and known capacity.
An evacuation/shelter assignment reserves a legitimate visitor place and uses
actual native street travel; it does not move the body immediately. Minors need a
guardian explicitly assigned to the same shelter. Requests exceeding shelter or
building capacity fail cleanly. Membership, ownership and normal household
assignment remain separate. Releasing shelter cancels the reservation, not the
person's actual position. The civic summary exposes pressures, water accounting,
response outputs and component counts; the UI consumes copied worker-side data.
