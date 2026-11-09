# Native work arbitration — Cycle 20

Each native minute runs needs, health/care planning, commute planning, physical
movement, food intentions, purchases/cargo, productive work and project publication.
All physical work uses the same guarded Bronze grant as movement. No parallel
job clock advances work. Rest, sleep, eating, illness, care and travel preempt it.
An apprentice and mentor must both be co-located, on shift and grant-ready; a pure
readiness check precedes paired claims. One mentor cannot perform two jobs in the
same minute. Closed or unavailable premises preserve task progress.

The caller supplies a monotonically increasing key per actor. Retrying the latest
identical request returns its existing ID/result without another reservation;
conflicting or stale keys fail. Only one productive task is pending per actor.
Inputs and the full worker/mentor wage budget are reserved before publication.
Every successful minute pays its reserved wages once. Completion consumes the
reserved inputs and publishes outputs once. Cancellation returns unused materials
and unpaid wages once, retaining earned wages and completed outputs. Work and
trade keep their latest result; privacy-scoped causal history is implemented in [RECORDS.md](RECORDS.md).

Requests allocate no memory at publication. The world preallocates bounded job,
project, account and institution state. Restart stages an entire candidate world
before replacement. Snapshots contain copied civil/job facts and institution
operations; native UI reads stay on its serial worker.
