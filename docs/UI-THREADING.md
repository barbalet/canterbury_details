# Native UI responsiveness

`CitySession` owns one serial dispatch queue. Content admission, native startup,
stepping, replacement, resident inspection and teardown all run on that queue.
The main thread receives copied clock/summary values and immutable search records.
It never reads a native body, runs a native minute or waits synchronously for the
simulation. One in-flight batch is permitted; timer ticks while busy are skipped
instead of accumulating work. Reported speed is a requested rate, not a promise
that an overloaded machine can meet it. Restart waits for a closed native batch
and publishes the replacement atomically. App termination asynchronously drains
the worker and closes the native singleton before replying to AppKit.

The map draws the bundled 1600×1600 PNG from exact upstream json2png.c, plus source
markers and optional precomputed footprint paths. It does not run json2png or query
the simulation during draw, zoom, pan or picking. The established scroll-view
magnification and explicit church recenter behavior remain intact. Search covers
all 27,000 immutable resident-name records, while the table exposes at most 200
matches at once to bound accessibility/list work. A current-record guard discards
asynchronous inspector replies for an older selection.

A reported zoom beachball prompted stack sampling and native-control verification.
The sampled current build did not show a sustained map drawing loop; repeated fit
to 600% zoom and footprint overlays responded. Population scans and native batches
were nevertheless synchronous main-thread work, and a resident-view clock lookup
also rescanned the population. The clock now reads the native clock directly;
CitySession removes simulation work from AppKit entirely. This fixes demonstrated
blocking paths without guessing that the raster renderer caused the reported hang.
Current visual evidence is recorded separately from runtime acceptance evidence.

Agent presentation uses an immutable packed `CdAgentGlyph` array built on the
simulation worker, keyed by world-scoped person reference. Map drawing reads only
that copy, culls to the viewport and batches all known-position plus signs into
one path. Unplaced people never receive invented geographic positions. The
separate All agents overview assigns each resident one roster-grid cell, explicitly
labeled as nongeographic, with amber unplaced and teal located glyphs. It supports
fit/zoom and clicking a glyph to inspect that live reference. The complete roster
is shown, independently of the searchable sidebar's 200-row display limit.
Unchanged overview data avoids redrawing the grid. No view is created per person.
