# Performance and capacity qualification

Cycle 37 budgets are declared in `content/performance-budgets.json`. Measurements
and machine/compiler details are in `content/cycle-37-validation.json`; native
presentation timings are in `build/presentation-metrics.json`. They describe the
measured machine/profile, not a guarantee for every Mac or an invented FPS figure.

Before any optimization, the prior 27,000-person full-day CLI run measured 20.68 s
wall / 20.41 s user CPU and 2,406,400,000 bytes peak RSS on this host. The final gate
remeasures the new source/pin. That baseline did not justify compacting native
bodies or changing the unflagged SDK layout. No such compact-storage change is made.

Final scenarios advance 1,440 sole native minutes at 27,000 and 30,000 people.
Actual-map cohorts retain unknown positions and staffing. Technical workloads
measure a saturated 30,000-person qualified street trip, a full cohort with busy
work/development/gathering/transport, 512 bounded care requests and scripted fire,
and 100 retirements plus 100 configured births at full capacity. Initial stocks,
wages, equipment, permissions and test geography remain explicit fixture inputs.
Finite shortages/interruption/waiting are valid results; no successful response is
fabricated to improve timing. Ledgers, traffic limits, population and checkpoint
continuation are checked after each workload.

Reports separate world initialization, minute stepping, full digest, save and staged
load wall times; CPU and peak RSS cover the process. Content admission happens before
the timed world initialization. Owned payload accounting has a 5 GiB aggregate cap,
including simultaneous live/staged state. RSS includes allocator/framework/native
compatibility overhead. Budgets allow 90/110 s per day for actual default/full
cohorts, 180 s for full technical loads, 60 s each for save/load, and 6 GiB RSS.
A 16 GiB or larger host is recommended for simultaneous checkpoint staging and UI.

The normally launched PresentationBenchmark.app measures 30 full-map offscreen
AppKit raster captures with 30,000 copied technical glyphs, after warmup. It has no
SDK population or simulation clock and cannot alter the city. Mean/p95/max are
reported; p95 must be ≤250 ms. These are software drawing/capture costs, not physical
display-refresh frame times. Ordinary UI responsiveness is independently checked by
native zoom/selection while worker operations run. Full sources/pins and budgets
are part of the evidence fingerprint. Any later optimization must rerun replay,
ledger, identity, failure and profile checks before changing supported claims.
