# Canterbury Details 1.0 user guide

Open the native app or the Canterbury Details Xcode scheme. The sole supported
baseline is Canterbury 1940, 27,000 ordinary synthetic humans, capacity 30,000.
The final assembled map uses cropped connected OpenStreetMap road geometry.
These modern/candidate inputs do not prove 1940 identities, occupancy or staffing.
The shipped population is unplaced; agents still run native needs/calendar logic.
No historical jobs, faith, events, homes or positions are automatically inferred.

Run/Pause and speed request native minutes; Step 1/10 pauses and advances exactly
that amount. Restart resets the ordinary baseline. There is one SDK, one population,
one native clock and one shared actor budget for movement, work, needs and care.
Navigation, inspection, text sizing, preview and status pages advance no time.

## City management

Simulation → City management (Command-M) opens the persistent workspace. Choose
jobs, work, premises/inventories, development, service rosters/requests, institution
offices, gatherings, transport, environment or operations. Pages copy up to 100
records, with Previous/Next/Refresh controls. All rows include stable IDs; unknown
source capacity remains labeled UNKNOWN. Counts are actual configured model data,
not invented organization/population totals. Pending tasks keep their real wages,
stocks, reservations and interruption reasons. Religious offices are ordinary
employment/service roles; membership, title qualification and staffing are separate.

Choose any of the 45 ordinary operations. Its help describes the parameter fields.
Enter stable person/entity IDs, a unique positive request key and an explicit basis.
Unused numeric parameters are zero. Units/value/options are operation-specific;
starts uses absolute civil minutes for schedules, earliest/deadline native minutes.
See [OPERATIONS.md](OPERATIONS.md) for every field, capability and finite-resource rule.
The typed form constructs the same command as the CLI; it never modifies arrays.

Observer is default. To preview/queue/apply/cancel an intervention, explicitly
check Enable scenario interventions. Preview is preliminary and reserves nothing.
Queue is idempotent by key; conflicting reuse fails. Due requests execute once
before the next minute or through Apply due, which advances no time. Query ID and
Cancel ID use the separate operation-ID field. Read the resulting status/reason,
then query again for execution results. The queue retains at most 128 results;
use a new scenario/session after that documented bound rather than assuming reuse.
A past successful preview cannot guarantee later stock, capacity, staff or access.

Control shortcuts: Command-P preview, K queue, F apply due, J query ID, B cancel ID,
R refresh, [ previous page, ] next page. The management window must have focus.
Standard Edit shortcuts select/copy/paste field text. Larger/Smaller text sizes the
record pane and form; the form and report scroll. No controller is supported.
The existing advanced pipe-command panel remains available for expert workflows.

## Map, people and inspection

Drag/scroll pans; pinch or Option-scroll and +/− zoom; F/Command-0 fits; arrow keys
pan; Space explicitly runs/pauses when the map has focus. Church selection centers
its approximate source location, including repeated selection at unchanged zoom.
Unlocated institutions stay in the list without markers. Building and institution
cards retain source uncertainty, exact notes/dates and current configured state.

Selecting a located resident centers and highlights the copied native position.
An unplaced resident shows a prominent explanation. All agents is a nongeographic
roster: + means located, × means unplaced, with colors only supplementary. Click a
mark or focus the roster and use arrows, Home/End and Return to inspect. Names are
synthetic and have thousands of combinations; IDs, not names or slots, identify
people. The resident list searches every person although its display is bounded.
View → Larger/Smaller text scales resident rows and the inspector up to 150%.

Save/Load uses complete atomic checkpoints on the serial simulation worker. Save
at any closed minute; no rest/location requirement. A failed load preserves the
active city. Loading refreshes runtime reference tokens while retaining logical
selection. Format/ABI/content mismatches fail closed; earlier source-qualified saves
may need their original build. There is no automatic migration. Export causal
records defaults to public scope; diagnostic export explicitly includes private
and household model records. Files are local, not encrypted or authenticated.

The actual map's street links, doors, capacities and staffing remain unqualified.
Commands needing those facts correctly reject missing setup. Operational test
streets, jobs, care/transport loads and environment scenarios are technical fixtures,
not claims about historical Canterbury. See [MAP.md](MAP.md) and [UNCERTAINTIES.md](UNCERTAINTIES.md).
