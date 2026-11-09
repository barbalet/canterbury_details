# Ordinary civic rosters and dispatch — Cycle 26

A source map label is not a historical organization, equipment supply or staffed
roster. `content/civic-source-register.json` records these gaps. Explicit technical
rosters attach service capability masks to real regular jobs, named employers,
worksites and qualifications. No historical police force, fire station, school or
hospital is auto-created. Religious offices and civic authority use distinct jobs.

| Service | Additional requirement | Observable effect |
|---|---|---|
| Health | Care skill, physical patient/clinic, finite bed and medicine | Existing shared-minute care and recovery |
| Education | Community skill and teaching permission | Recorded ordinary teaching response/attendance |
| Administration | Community skill and administration permission | Recorded administrative response |
| Policing | Community skill and explicit police permission | Recorded site response; no invented detention/combat power |
| Fire | Craft skill and explicit fire permission; carried tool and water | 20 technical fire-intensity units removed per response grant |
| Burial administration | Explicit burial permission or compatible religious office; archived death | Recorded administration; no corpse teleport |
| Meals / charity / visitation | Compatible roster or religious office; co-located recipient | Finite meal/textile transfer or recorded ordinary visit |

Dispatch names a provider, qualified destination, subject if required, duration,
future deadline (within 10080 minutes) and basis. A bounded queue retains up to 512
requests/results, with explicit keys for retry safety. Unsupported capabilities,
unknown/foreign subjects, over-capacity queues and insufficient wage funds fail
before publication. Requests reserve wages once, acquire no free supplies and
allocate no memory at publication. One worker handles one response at a time;
all productive progress uses existing Bronze grants. Clinical dispatch delegates
to the existing care system and credits only that worker's actual care grant.

Responders commute on real qualified paths and reviewed connectors. Fire response
physically collects a finite tool and water from its base, then carries them to
the incident. Tools remain in the worker's inventory; water is consumed by actual
response. Stock shortages wait without output. Clinical beds/medicine remain at
the actual configured clinic; no medical stock teleports. Employer/institution
closure, off-shift work, permission loss and staff absence stop service. A valid
successor can continue the remaining request. Dead actors cannot act. Deaths
retain carried possessions at their archived actual location; departures export
water and goods. Water imports = stocks + use + exports.

Cancellation/deadline expiry refunds unused wages once and keeps performed work.
Cancelled response travel stops in place; a journey interrupted between nodes
must resume its existing legitimate leg before changing destination. Completed
responses expose paid wages, progress, subject, provider and reason. Counters and
latest bounded outcomes are observable now; richer causal history is Cycle 29.
