#include "cd_city_internal.h"

#include "../build/content/OccupationCatalogue.inc"
size_t cd_occupation_count(void) {
  return sizeof occupations / sizeof *occupations;
}
const char *cd_occupation_identity(void) { return occupation_identity; }
CdStatus cd_occupation(size_t i, CdOccupationView *out, CdDiagnostic *d) {
  if (!out || i >= cd_occupation_count())
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown occupation definition");
  *out = occupations[i];
  return cd_diag(d, CD_OK,
                 out->enabled
                     ? "Enabled technical role definition; not a job assignment"
                     : "Unsupported upstream vocation disabled");
}
CdStatus cd_world_occupation_eligible(const CdWorld *w, CdPersonRef ref,
                                      CdEntityId id, int *eligible,
                                      CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !eligible || !id || id > cd_occupation_count())
    return cd_diag(d, CD_UNKNOWN_REFERENCE,
                   "Unknown person/occupation definition");
  const CdOccupationView *role = &occupations[id - 1];
  CdResidentView view;
  cd_population_view(w, p, &view);
  *eligible = role->enabled && view.age >= role->minimum_age &&
              (p->skills & role->skills) == role->skills &&
              !p->health.mortality_pending;
  return cd_diag(d, CD_OK,
                 "Qualification is separate from workplace, employer, vacancy "
                 "and assignment");
}
