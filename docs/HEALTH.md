# Ordinary health and clinical care — Cycle 17

This is an explicitly configured, non-predictive technical simulation. The sources
supply no Canterbury incidence, treatment efficacy or mortality rates. No random
epidemic, injury rate or historical disease outcome is enabled. `cd_world_expose`
accepts an explicit illness/injury kind, severity 1–1000, duration 1–10,080 native
minutes and a bounded basis. An existing condition cannot be overwritten to erase
its history or pending mortality.

The native body energy/wake state remains authoritative physiology. City condition,
severity, care history and reservations are additional ordinary records. At native
minutes divisible by 60, severity ≥500 costs five extra native energy units.
Unassisted conditions expire at the explicitly configured duration; no actual
medical prognosis is asserted. A severity ≥900 persisting 60 minutes nominates
mortality. Located people at zero energy for 360 minutes also nominate mortality.
Unplaced people's missing physical provision is not turned into an invented
historical famine/death: unresolved placement remains explicit in their unmet
needs. All such thresholds are technical test parameters.

Clinics require a real building with an explicitly reviewed/fixture capacity.
Beds cannot exceed that capacity. Configured medicine is finite abstract units,
not a drug or dosage. The provider must be an adult explicitly trained in care,
capable, co-located with the patient inside the clinic, and available after its
own survival/rest needs. One provider/one patient reserve one bed. State is
revalidated each minute; a native grant consumes one medicine unit and reduces
severity by 10 and duration by four, in addition to elapsed native time. An
improving condition is marked recovering; completion releases the reservation.
Receiving clinic care/severe illness blocks the patient's movement and elective
work. The provider cannot also travel or produce food that minute.

A configured preventive-care task uses the same physical provider, bed, medicine
and grant. It establishes a bounded technical protection interval; an explicitly
introduced severity is halved during that interval. This is an abstract experiment,
not advice or a historical efficacy claim. No religious identity, building type or
moral trait grants protection. No supernatural health rules are used.

Mortality is committed after each native minute so batch sizes do not change
outcomes. Lifecycle cleanup archives death and releases care/food/work/route,
occupancy and ownership references exactly once. Missing guardian/archive
prerequisites stop with an explicit error; the pending person cannot keep acting.
`cd_world_clinic_view` checks `medicine imports == current medicine + used`.
Tests cover physical/qualification/bed rejection, finite treatment, prevention,
recovery, configured fatal injury, compaction and retirement cleanup.
