# Robustness, isolation and dependency purity

Cycle 38 combines the existing executable gates with final source qualification.
`make final-check` tests bounded management reads and malformed references, keyboard
purity, save buffer allocation failure, every snapshot allocation, failed loads
preserving the owning world, and repeated world construction/teardown. Earlier
gates enumerate every content/world/native allocation and load-stage failure,
interrupted publication, source/schema/CRC/ABI/content rejection, queue saturation,
finite ledgers, lifecycle compaction, cancellation and conserved custody. Injected
failures must leave the active world and prior published files intact and all
owned allocation payloads must return to zero after closure.

`tools/verify_sdk_profile.py --run` tests original native, contract, actor and hybrid
suites in an isolated SDK copy. Its independent pinned comparison revision is in
`content/sdk-profile-baseline.json`, so committing the guarded fix does not turn
the test into a comparison against itself. Unflagged preprocessed behavior and
native/Bronze layouts remain unchanged. Current guarded changes are committed in
ApeSDK and the parent pin matches; no reviewed dirty SDK overlay is needed.

Dependency purity is checked at source admission and in fresh offline clones.
The only implementation/data submodules are Canterbury and ApeSDK. Public core
headers expose no native structs or borrowed SDK pointers; the ordinary consumer
links only the packaged archive and public headers. No Cocoa/host main is in that
archive. The released ordinary runtime has no special actor population, mythology
mechanics, campaign itineraries, supernatural action catalogue or sibling imports.
Disabled upstream vocabulary and historical reuse/provenance discussion are not
active simulation rules. Religion remains explicit ordinary institutions, offices,
qualifications, memberships, employment and finite services.

Checksums detect accidental corruption; they do not authenticate hostile files or
replace OS permissions. Compatibility is strict, and unsupported payloads are
rejected rather than silently migrated. Parser/fuzz tests are bounded fixtures,
not a claim that arbitrary malicious inputs have been exhaustively proven safe.
