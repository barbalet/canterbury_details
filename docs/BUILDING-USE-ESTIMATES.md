# Provisional building uses and occupation links

The user authorized guesses for the 1,790 buildings whose enriched source use is
unknown on 9 November 2026. These are **simulation hypotheses**, not additions to
the historical source record. The reproducible per-building output is
[building-use-estimates.json](../content/building-use-estimates.json); its weights
and role suggestions are in [the policy](../content/building-use-estimate-policy.json).
The source submodules, geometry and source-use evidence remain unchanged. At the
user's subsequent request, the runtime compiler now applies these proposed uses
to unknown building records and labels their policy basis as provisional. It
retains the original source use in the source bundle and advisory report. Actors
and job assignments are not changed.

For each unknown footprint, take at most eight classified source buildings within
120 map pixels of its bounding-box centre. Weight these neighbours by distance.
A disclosed affinity table combines similar uses with complementary services:
industry suggests workshops/storage, schools suggest education support, healthcare
suggests care/laundry/kitchen support, and housing suggests housing/local shops.
Only original classified source records are anchors. Guesses never propagate
as evidence, and no random seed or current simulation state affects the result.
Modern labels remain uncertain anchors rather than verified 1940 uses.

The highest score supplies the provisional use. The report retains up to three
alternatives, anchor IDs/distances and source identity-verification flags.
Support shares and moderate/low labels describe agreement under this policy,
**not calibrated probabilities of historical correctness**. Sparse/conflicting
support remains low; no anchor yields an explicit very-low-confidence residential
fallback. Distance is Euclidean: water, rail, walls and actual routes must be
checked separately before any workplace becomes operational.

| Suggested use | Unknown buildings |
| --- | ---: |
| Residential | 1,353 |
| Agricultural support | 205 |
| Commercial | 146 |
| Education support | 47 |
| Workshop | 25 |
| Care support | 10 |
| Utility support | 2 |
| Civic support | 1 |
| Hospitality support | 1 |

965 guesses have moderate neighbourhood agreement, 633 are low and 192 are
very-low-confidence fallbacks. Changing the policy requires regeneration;
this table reports the initial policy, not a staffing target.

Each hypothesis includes suggested workplace roles. Examples include shops with
shopkeepers/assistants/delivery workers, workshops with carpenters/repair workers,
and agricultural premises with farmers/labourers/produce carriers. Most suggested
roles are not yet enabled in the eight-role runtime catalogue. Religious-site
proximity does not assign clergy, affiliation or institutional authority.
Residential buildings have no implied workplace role: their occupants may commute.

The proposed uses are now applied to runtime building metadata, while the
occupation suggestions remain advisory. **The default 27,000 residents still have no
assigned jobs or placed homes.** It does not create employers, safe doors, routes,
capacity, staffing counts or professional qualifications. The next implementation
must explicitly allocate households, qualify access and capacity, expand ordinary
roles, create workplaces and shifts, then assign reachable jobs. Inferring useful
service coverage is a modelling input, not proof of a staffed historical service.

Regenerate and verify offline:

```sh
python3 tools/estimate_building_uses.py --write
python3 tools/estimate_building_uses.py --check
python3 -m unittest discover -s tests -p test_building_use_estimates.py -v
```

The output hashes source bytes, policy, generator and audited source manifest.
The source inventory must be current; publication is atomic and checks inputs
again before replacing the preceding output.

## Population allocation proposal

[The machine-readable proposal](../content/population-allocation-proposal.json)
allocates 2,160 residents aged 0–4, 4,050 aged 5–14, 1,620 aged 15–17, 16,470
aged 18–64 and 2,700 aged 65+. Of the working-age adults, 12,000 are proposed
for employment and 4,470 for household care, study, unavailable or seeking-work
status. Elderly residents are not assumed incapable; optional work/volunteering
would require explicit policy rather than an age-based ability penalty. These
figures are explicit scenario choices, not historical statistics.

The proposal distributes 12,000 jobs across nine sectors and lists candidate
premises types for each. It is not yet consumed by the population generator: the
existing four-person household fixture still has 13,500 minors, 13,500 adults and
no age-65+ residents. Implementing the proposed ages must preserve guardianship
and household consistency before any placement or commute is enabled.

Canterbury currently enables eight occupation definitions: crop farmer, brewer,
carpenter, textile weaver, potter, care worker, clergy and religious lay caretaker.
The broader proposed sectors need additional role implementations. The 63 audited
SDK vocations are not 63 enabled Canterbury occupations.

There is no Bronze-style declarative Canterbury scenario DSL. The current
45-command pipe-delimited operation language supports explicit scenario actions,
`step N`, `flush` and comments; see [OPERATIONS.md](OPERATIONS.md). JSON policies
currently define compilation and modelling inputs. The allocation proposal is
not an executable language and does not create 12,000 jobs by being present.
