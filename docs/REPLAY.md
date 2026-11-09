# Deterministic replay

Cycle 32 uses `tests/observer.c` and `tools/verify_observer_cycles.py`. A seeded
20-person technical street fixture starts travel, tool production, capacity
construction, physical care, a worship gathering, stretcher carriage, weather
and future road closures. It saves while these are active, then compares 60
minutes as a batch against single minutes with pauses and snapshot captures,
restored continuation and continuation in a separately launched process.
The complete authoritative digest and diagnostic event export must agree.

The native clock is the only simulation clock. Operations execute in insertion
order before the next native minute, or at an explicit closed-minute flush.
Duplicate request keys cannot duplicate effects. `step 0`, inspection, exports,
redraw and camera changes consume neither native time nor simulation RNG.
Wall-clock speed only chooses how many native minutes to request.

Digest coverage includes every live native body, native clock/work grants/tasks,
monotonic actor IDs and city RNG streams, civil/household/lifecycle, movement,
needs/health/food, jobs/work/escrow/economy/development, institutional/civic/
transport/environment/calendar state, records/cursors and queued results.
Process pointers, runtime reference tokens, route-search scratch, camera/UI
preferences and wall-clock rendering counters are excluded. The checkpoint
checksum additionally covers its entire serialized payload, including retained
native landscape/timing compatibility state. The city profile does not run the
SDK island biology/weather/movement model.

Replay is guaranteed for the same admitted content, ABI/profile and ordered
inputs. This is not a cross-platform floating-point equivalence claim. The
verifier also runs the fixture at `-O0` and `-O2`; Xcode Debug/Release builds
qualify the supported native configurations. See Cycle 32 evidence.
