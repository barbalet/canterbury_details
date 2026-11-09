# Native energy, daily needs and rest — Cycle 15

`CANTERBURY_SIM` is required by the host headers and all build targets. The SDK
city callback replaces island autonomy. It executes city needs, care, pedestrian
movement and finite food work once per native minute. The island biological,
movement and mortality loops do not execute a second update. The existing guarded
Bronze minute grant is shared by travel, caregiver effort and food work; there is
no independent ticking clock or stored city-energy authority.

The native energy field uses SDK constants `BEING_HUNGRY=1280` and
`BEING_FULL=3840`. The following policy is explicitly a technical fixture:

| Parameter | Model rule |
| --- | --- |
| Awake basal cost | 1 native energy unit/minute |
| Sleeping basal cost | 1 unit on native minutes divisible by 4 |
| Meal | 1 finite portion restores 640 units, capped at native full |
| Initial food | 3 carried portions per initial resident; never replenished automatically |
| Sleep window | 22:00–06:00 civil time; shifts are Cycle 19 work |
| Fatigue | +1 awake, −4 sleeping, −2 resting; bounded 0–1440 |
| Forced sleep threshold | 960 fatigue units |
| Requested rest | 0–1440 performed rest minutes, pausable by higher needs |

There is no claim that SDK energy units are calories or that this schedule matches
a historical population. Rest recovers fatigue, not food energy. The startup
binding minute precedes human-model publication; requested minutes alone increment
human metabolism counters. Zero-step observation never metabolizes or consumes food.

Each body first plans its own survival/rest state. Caregiver assignments are then
resolved against all planned states, so a guardian with a later ID cannot have
its care action overwritten by its own processing. Adults/school-age children can
consume carried food or an available meal in their actual household premises.
Under-six children require a capable, physically co-located adult guardian and
that adult's native minute grant. Logistics remains adult-only. Missing food or
care increments unmet demand rather than fabricating a portion. Eating, sleeping,
resting or receiving severe-illness/clinic care blocks travel/work that minute.
Queued travel resumes from its actual position when the actor is available.

Native position is still the only location authority. Needs do not select random
houses, move someone to a shop, or invent household co-location. Unplaced people
can use their own carried food where self-feeding is supported, while unresolved
physical dependent care remains explicit. The native UI exposes energy, fatigue,
sleep/rest/activity, meals and health severity. Day-length tests compare individual
and batched stepping and reconcile food stock after both.
