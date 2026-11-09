# Ordinary occupation catalogue — Cycle 18

`tools/compile_occupations.py` audits all **63** vocation definitions in the pinned
`apesdk/bronze/content/bronze_age_full.bronze`. It reads only that source, Canterbury
institution records and the explicit local `content/occupation-policy.json`.
Source-inventory admission is checked, each input is hashed, and generated output
is written offline into `build/content/OccupationCatalogue.{inc,json}`. The C
catalogue identity binds the exact source/policy/compiler bytes. Make and every
Xcode target generate the same include before compilation; no sibling project,
web document or remote package is needed.

Eight technical ordinary role definitions are enabled:

| Role | Basis | Qualification | Legitimate output |
| --- | --- | --- | --- |
| Crop farmer | SDK crop-tender vocabulary, explicit adaptation | Adult + food skill | Finite raw food inputs |
| Brewer | SDK brewer vocabulary, explicit adaptation | Adult + food skill | Processed food inputs |
| Carpenter | SDK carpenter vocabulary | Adult + craft skill | Maintained/repaired premises |
| Textile weaver | SDK textile-weaver vocabulary | Adult + craft skill | Textile goods |
| Potter | SDK pottery-maker vocabulary | Adult + craft skill | Containers |
| Care worker | Explicit technical ordinary role | Adult + care skill | Basic care service |
| Clergy | Canterbury institution IDs + technical definition | Adult + community skill | Worship/community service |
| Religious lay caretaker | Canterbury institution IDs + technical definition | Adult + craft skill | Maintenance/community assistance |

These are role definitions, not evidence of actual historical jobs/staffing. There
are **58 disabled** upstream specializations. Unqualified Bronze smith/ritual and
other ancient roles are not renamed into a purported 1940 profession. In
particular, clergy uses Canterbury institution evidence rather than relabeling the
SDK priest-seer. The compiler has an explicitly reviewed allow-list; enabling a
new upstream specialization requires a deliberate qualification change. Invalid
vocations, duplicate keys, missing institution references, unqualified ages/skills,
missing technical basis and stale generated output are rejected by tests.

An occupation, job vacancy, skill, employer, workplace, shift and voluntary duty
are different things. Current eligibility only asks whether a live person's age
and explicitly configured skills satisfy an enabled role definition. It does not
assign employment, invent an employer/vacancy, confer a clinical license or change
religious affiliation. Actual assignments, apprenticeships and shifts are Cycle
19; generic work dispatch and outputs extend this in Cycle 20 and later. The
native catalogue inspector shows enabled/disabled definitions and their source,
qualifications, basis and output separately from resident records.
