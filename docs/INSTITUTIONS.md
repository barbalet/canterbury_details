# Source institutions — Cycle 23

The runtime imports all 33 supplied religious-site records: 32 Christian and one
Jewish; five are unlocated. `content/institution-source-matrix.json` retains every
original field, date/note, tradition, denomination, worship status, candidate
association and provenance. The matrix and runtime are checked against the audited
Cycle 08 bundle, not an external webpage. Runtime format CDCT0002 adds the exact
source record, site type, setting year, location basis and explicit review flags.
Older packs must be regenerated, rather than silently dropping evidence.

Dates supplied only in prose remain in the source record; unknown dates remain
unknown. Historical closure and later development notes do not automatically
create current operations. St Peter's 1928 closure and 1953 resumption of services remain
traceable; the Old Synagogue's historical notes remain alongside its supplied
1940 scenario status. No additional populations or institutions are invented.
Candidate points are approximate source locations; unlocated sites have no marker.
Missing confidence is explicitly unknown, not a fabricated numeric certainty.

Institution identity, office, source point, reviewed premises membership, religious
congregation, legal building ownership and current operations are distinct. A point
is not a precinct boundary or congregation census. Those remain unknown. Reviewed
scenario links require a located institution, known premises capacity, a reviewed
physical door and a separate retained basis per link (maximum eight). Neither
linking nor operation upgrades historical identity or confidence. Ownership uses
the existing independent property API. Ordinary people retain unknown affiliation.

An explicit office and basis configure operations; closed/review-status sources
require an explicit scenario override. Unlocated/unlinked sites cannot be activated
by guessing. Jobs attached to an institution require its linked, operating premises;
closures suspend their productive tasks. Configured operation intent is separate
from premises condition, which independently gates actual work. Snapshots preserve
source plus operation state. The native inspector displays exact source evidence
and configured/unknown operations without reading live native state on the UI thread.
