# Source uncertainties and downstream decisions

Cycle 01 records these gaps; it does not silently supply historical facts.
“Open” below is a downstream data/model decision, not an incomplete source audit.
Sources and selectors are reproducible through `content/source-manifest.json`.

| ID | Evidence / unknown | Required treatment and owning cycle |
| --- | --- | --- |
| U01 | `universe/universe.h` supplies an SDK default/capacity, not a 1940 census; source map spans city and surrounding settlements | User requires 27,000 ordinary humans (implemented as Cycle 03 default), not a verified census. Do not invent extra troops/evacuees. Define fixture boundaries in 04/12; historical total remains unknown |
| U02 | `derived/road-network.json` explicitly describes modern OSM layout and approximate registration | Keep modern labels/date separate; qualify topology and historical applicability before routing, 08–11 |
| U03 | Historical traces are source-guided, incomplete and not era-qualified; their edges remain non-traversable | Do not infer bridges, walkability or missing joins; explicit review required, 09–11 |
| U04 | Candidate footprints may combine structures, fragments or symbols; entrances are not surveyed | Preserve rings/confidence; review ordinary door connectors and building capacity, 09/13 |
| U05 | Modern building labels/use matches are suggestions; many footprints remain unknown | Preserve unknown use and source identity; do not fabricate historical employers, 08/18/19 |
| U06 | Old Synagogue active Jewish-centre assignment is an explicit upstream scenario override | Retain scenario assignment and historical closure qualification side by side, 23–25; do not report verified active worship |
| U07 | Five religious records are unlocated; approximate points/building matches do not prove precincts | Keep non-spatial records and unmatched/candidate buildings explicit, 09/23/34 |
| U08 | Religious-site presence is not resident affiliation, morality, ethnicity or a census | No geographic inference of membership or invented local communities, 12/23–25 |
| U09 | Complete 1940 calendars, staffing, service frequencies, age/household/job distributions and economic rates are absent | Use only approved inputs or clearly identified technical fixtures; no invented historical precision, 12–25 |
| U10 | Water/rail components are extracted ink/approximate corridors | No automatic navigation, operational rail service or intersection-based track joining, 09/11/27 |
| U11 | Pixel units and landmark-based affine controls are approximate | Do not assert surveyed metres or certified walking time; settle model units explicitly, 08–11 |
| U12 | Root host and SDK license notices exist; Canterbury references a remote license, OSM has its own data notices, and full historical-asset rights evidence is absent | Preserve all local notices/provenance, do not flatten licenses into host MIT, document outstanding per-asset review before distribution, 02/33/39/40 |
| U13 | Snapshot retrieval is 2026; modern restrictions and names are not period evidence | Retain retrieval/provenance and modern tags without backdating them, 08/11/18 |
| U14 | SDK city-profile documentation describes an earlier client and a reserved special actor | Use the actual guarded APIs, not that client's semantics; all agents here are ordinary humans, 02/04/05 |
| U15 | Upstream validators test source assembly, not this application's simulation or historical truth | Keep Cycle 01 evidence separate from later full-host/SDK/UI qualification, 03 onward |
| U16 | City callback bypasses island autonomy; native fields/hooks alone are not a human city model | Establish sole clock, position, energy and work owners and implement required human policy, 02/05/15/20 |
| U17 | No local-source evidence establishes specific wartime events, future damage, transport timetables or demographic rates for this host | Do not add such events from the year label alone; future scenarios require supported inputs, 14/26–28 |

Historical or asset-rights gaps remain visible through release scope decisions;
passing a geometry test does not resolve them. Updates require reviewing the
appropriate upstream source and parent gitlink, then rebaselining the inventory.
