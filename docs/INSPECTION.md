# Camera and live inspection

Drag/scroll to pan; pinch, Option-scroll, +/− or toolbar controls zoom;
Fit / Command-0 restores map fit. Selecting a located institution explicitly
centers it even when magnification is unchanged. Unlocated institutions stay in
the searchable list with no invented marker. Buildings, institutions, occupations
and all people have separate searchable lists. The All agents window displays
every live resident with × for unplaced and + for located, supplementary colors
and stable-ID keyboard selection.

Click a known-position + to select the nearest native person (lowest stable ID
breaks equal-distance ties). The card shows synthetic identity/birth/household,
housing, native energy/fatigue/activity, meals/health, jobs/work/pay, service,
gathering, carriage/shelter and explicit permissions. Job roles and ordinary
bereavement support context come from current worker-owned views. No affiliation
is inferred. Building cards join source IDs to runtime capacity, condition/use/
employer, overrides and source confidence; institution cards retain exact source
status/dates, operating state, premise links and unknown congregation/precinct.

Cards refresh after each published state. Asynchronous results carry a revision
and selection identity, so stale results cannot overwrite a newer card. Background
refresh never recenters the camera. A selected logical person ID survives native
compaction and load; current references are reacquired after load rather than
reusing invalid tokens. Retired/missing selections show an inactive card and
remain available for re-selection if a matching saved city is restored. Loading
always rebuilds the cached people list even if population size is unchanged.

Simulation menu: Save/Load checkpoint, Scenario operation and Export causal
records. Saves/loads and exports execute on the serial worker while AppKit stays
responsive. Operation mode defaults to observer; interventions require the explicit
checkbox. Export defaults to public scope; its diagnostic checkbox explicitly
includes private/household data. There are no campaign or supernatural selectors.

Agent-name selection centers immediately from the currently displayed immutable
glyph by stable ID, independent of asynchronous inspector completion. Located
selections retain a halo; unplaced selections show a prominent location explanation
above the card. Slot reordering or renewed runtime tokens cannot break this link.
