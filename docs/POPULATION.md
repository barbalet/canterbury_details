# Ordinary resident model — Cycle 12

The default admission is exactly 27,000 ordinary native bodies and 27,000 civil
records. No reserved character slot exists. A scenario must explicitly select
`synthetic-ordinary-v1`; unknown policies fail before native publication. The only
factual inputs are the two local submodules. They supply no representative 1940
census distribution, so the generated demographics are technical fixtures.

Each person has a world-token/stable-ID reference, SDK-vocabulary display name, Gregorian birth date, derived civil age, explicit
category/basis and household/relationship references. A dedicated stateless naming domain selects from the SDK’s 505 distinct given
names and 256 surnames (two-part family names). Initial household members share
a family name. Births inherit their guardian’s surname when available; explicit
admission names are retained. Names contain no numeric uniqueness suffix and
never serve as identity keys. They are synthetic names, not historical residents. Religious affiliation remains unknown for every
person. A nearby church, surname or household never assigns affiliation.

For deterministic family fixtures, complete groups of four have adults aged 35–64
and 33–60, children aged 6–15 and 0–5 at admission. Dates are sampled from valid
months/days and adjusted to match that age at the origin. An incomplete final
group contains adults where a child would lack the complete fixture context.
Every minor has a valid adult guardian in the same household. The mix is disclosed
for reproducible behavioral tests, not asserted as an upstream distribution.

The civil origin must be a valid date in 1940. Birth dates use Gregorian leap
rules; age is derived from the native minute's civil date. Stateless SplitMix-64
sampling uses the full scenario seed and person ordinal without consuming runtime
RNG streams. Native startup uses the explicit 32-bit fold `seed XOR (seed >> 32)`.
World namespace tokens distinguish replacement handles and are not replay data.

Category defaults to resident. Visitor, troop and evacuee categories require an
explicit bounded configuration basis. The technical troop category requires an
adult. Category does not assign a home, workplace, faith or physical location.
A visitor category and a dwelling visitor reservation are different concepts.
Bounded civil birthdays, configured births/admissions and retirement are now implemented in Cycle 14; historical demographic rates remain unsupported.

Map coverage is not the population boundary. All 27,000 people are admitted while
all spatial placements and housing are initially unknown; no random source
footprints are passed off as census residences. Full-map acceptance checks every
ID/date/guardian and reconciles 27,000 records with 6,750 households.

The default population is **27,000**, distinct from the **30,000** native/identity
capacity. The 3,000 spare slots support later growth; births remain Cycle 14 work.

The full-population naming regression measures 26,898 distinct full names among
27,000 residents for the default seed, without counting IDs or display suffixes.
It checks at least 25,000 distinct names, shared initial household surnames,
repeatability for the same seed and variation for a different seed. The SDK
name dictionary selector is not used to assign a civil sex or religious identity.
