# Native time and session lifecycle — Cycle 05

The SDK native minute is the only ticking authority. Calendar time is a pure
view of `BronzeClock.elapsed`, not a host timer or separately accumulated counter.
The host performs no background stepping: a paused UI, Refresh, a clock query,
a report query and a zero-minute request consume neither minutes nor RNG.

## Scenario origin

Native elapsed zero maps to **1940-01-01 00:00**, an explicit scenario convention.
The upstream data supplies setting year 1940, not a documented opening date or
time. January 1 does not assert an event, season-specific schedule or historical
weather condition. The SDK's native absolute origin remains untouched. It is
recorded at registry reset, before the first native minute; its island epoch is
not interpreted as a civil year.

`cd_smoke_civil_time` maps elapsed minutes into the existing public `CdCivilTime`
value type (year, month, day, minute of day). It uses the Gregorian calendar,
including leap year 1940, century exception 2100 and leap century 2000. Valid
output years are 1940..9999; overflow/null requests fail without writing output.
There is no host timezone, daylight-saving adjustment or wall-clock dependency.
`cd_smoke_time` reads the active closed native interval and derives that value.
The CLI emits `scenario_time`; the native window displays it beside native time.

The bootstrap's initial binding `sim_cycle` is also a real native minute. Therefore
opening a session returns paused at native minute 1 / 1940-01-01 00:01. An additional
1,439 requested minutes reach 1940-01-02 00:00. This preserves the Cycle 03 minute
contract; it does not hide a simulation tick inside a civil offset.

## Serialized lifecycle

All APIs run on one serialized owner thread. Neither the SDK nor this adapter
provides concurrent sessions or thread-safe stepping. This bootstrap lifecycle
is implemented separately from the future `CdWorld` model/content APIs.

Cycle 07 wraps this ordering in prepared resources and zero-allocation publication;
`docs/MEMORY.md` governs the current allocation-failure/publication contract.

1. Validate population (1..30000), output pointer and exclusive native ownership.
   Rejected opens preserve an existing output handle and do not replace the live session.
2. Allocate the host session and actor registry; activate the registry.
3. Configure `sim_canterbury_configure` with the native-minute callback and this
   session's context. This must precede SDK startup.
4. Call `sim_init(KIND_START_UP, seed, MAP_AREA, 0)`. Recoverable construction failures unwind all
   prepared resources before native ownership changes.
5. Publish the active session before the initial `sim_cycle`. The SDK initializes
   bodies and stable identities, records its clock origin, calls `land_cycle`,
   opens the native work interval, calls the city handler once, then closes it.
6. Return paused only after exactly one validated callback. Each explicit step
   calls `sim_cycle` once per requested minute. The adapter requires a closed
   interval before and after, an elapsed increase of exactly one, and a matching
   callback count. Any clock/callback mismatch fails the session closed to further
   reads/steps. The callback counter is diagnostic, never used to advance time.
7. Pure report/time reads require an active session and a closed interval.
8. Close retires the active callback context before calling `sim_close`, then
   destroys the owned registry and frees the session. SDK close clears its city
   handler/context/started state and native population. A new session must
   configure its own callback/context, population and seed.

The callback checks active context before dereferencing it, verifies that the
native interval is open and consecutive, and validates all native actor bindings.
The adapter's initial callback performs no human routines yet. SDK city mode
returns before island autonomy; no additional native movement, metabolism or
social loop is run. Native land/tide time continues through the native minute.

The current session handle is invalid after close; the caller must clear it.
Null close is harmless. Do not retain or call freed handles, borrowed native
pointers, registry pointers or callbacks across a close/reopen. Persistent
cross-world person references belong to Cycle 06. No save/session switching or
simultaneously resident world is implemented here; switching means complete
serialized close followed by independently configured open.

## Evidence and limits

`tests/lifecycle.c` runs a real native session across midnight, checks callback
and native-minute equality, verifies repeated paused reads and rejected/zero
steps preserve state, and checks native position, energy and RNG remain unchanged
under the empty city callback. Alternating population/seed sessions restore the
same initial digest on return. Direct SDK sessions with distinct context counters
prove that a late SDK cycle after close cannot call the old context, reopen
replaces that context, and startup without a
new configuration fails after close. Calendar-only fixtures cover minute/hour/day,
leap-day/year and century boundaries without advancing a second clock.

Run `make lifecycle`, or `make test` for Cycles 03–05. The verifier also checks the
CLI's midnight display. Evidence: `content/cycle-05-validation.json`; native app
observations are separate in `content/cycle-05-native-ui.json`. No SDK edits were
needed; the reviewed Cycle 03 SDK patch and ordinary-profile evidence are
unchanged. Full human behavior, persistence, memory-failure injection and concurrent
world switching remain in their later cycles.
