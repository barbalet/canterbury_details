#include "cd_city_internal.h"
#include "cd_native.h"
static int leap(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
unsigned cd_days_in_month(int year, unsigned month) {
  static const unsigned d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month >= 1 && month <= 12 ? d[month - 1] + (month == 2 && leap(year))
                                   : 0;
}
int cd_civil_valid(CdCivilTime t) {
  return t.year >= 1800 && t.year <= 9999 && t.month >= 1 && t.month <= 12 &&
         t.day >= 1 && t.day <= cd_days_in_month(t.year, t.month) &&
         t.minute_of_day < 1440;
}
unsigned cd_civil_age(CdCivilTime birth, CdCivilTime now) {
  int age = now.year - birth.year;
  if (now.month < birth.month ||
      (now.month == birth.month && now.day < birth.day))
    age--;
  return age < 0 ? 0 : (unsigned)age;
}
static uint64_t random_at(uint64_t seed, uint64_t i) {
  uint64_t z = seed + UINT64_C(0x9e3779b97f4a7c15) * (i + 1);
  z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
  return z ^ (z >> 31);
}
void cd_population_name(const CdWorld *w, CdPersonId id, CdEntityId household,
                        char *out) {
  uint64_t given = random_at(w->seed ^ UINT64_C(0x4e414d455f474956), id),
           family =
               random_at(w->seed ^ UINT64_C(0x46414d494c595f31), household);
  cd_native_fixture_name(given, family, out, 128);
}
CdStatus cd_population_init(CdWorld *w, const CdScenario *scenario) {
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (cd_smoke_person(w->native, i, &p->civil.ref, NULL) != CD_OK)
      return CD_INVALID_STATE;
    cd_population_name(w, p->civil.ref.person_id, i / 4 + 1, p->civil.name);
    uint64_t r = random_at(scenario->seed, i);
    unsigned role = (unsigned)(i % 4),
             age = role == 0   ? 35 + (unsigned)(r % 30)
                   : role == 1 ? 33 + (unsigned)(r % 28)
                   : role == 2 ? 6 + (unsigned)(r % 10)
                               : (unsigned)(r % 6);
    p->civil.birth = (CdCivilTime){scenario->civil_origin.year - (int)age,
                                   1 + (unsigned)((r >> 8) % 12), 1, 0};
    p->civil.birth.day =
        1 + (unsigned)((r >> 16) % cd_days_in_month(p->civil.birth.year,
                                                    p->civil.birth.month));
    if (p->civil.birth.month > scenario->civil_origin.month ||
        (p->civil.birth.month == scenario->civil_origin.month &&
         p->civil.birth.day > scenario->civil_origin.day)) {
      p->civil.birth.year--;
      if (p->civil.birth.day >
          cd_days_in_month(p->civil.birth.year, p->civil.birth.month))
        p->civil.birth.day =
            cd_days_in_month(p->civil.birth.year, p->civil.birth.month);
    }
    /* Incomplete final groups contain adults only; every generated minor has a
     * guardian. */
    if (i / 4 == w->count / 4 && w->count % 4 && role >= 2) {
      p->civil.birth.year = scenario->civil_origin.year - 35;
      p->civil.birth.month = 1;
      p->civil.birth.day = 1;
    }
    p->civil.age = cd_civil_age(p->civil.birth, scenario->civil_origin);
    p->civil.synthetic = 1;
    p->civil.affiliation_known = 0;
    p->civil.category = CD_RESIDENT;
    snprintf(p->civil.admission_basis, sizeof p->civil.admission_basis,
             "synthetic-ordinary-v1; SDK names, no census claim");
    p->anchor = CD_NONE;
    p->held_edge = CD_NONE;
    p->door = CD_NONE;
    p->inside = CD_NONE;
    p->travel = CD_ROUTE_IDLE;
    cd_native_city_body(w->native, p->civil.ref);
  }
  return CD_OK;
}
void cd_population_view(const CdWorld *w, const CdCitizen *p,
                        CdResidentView *out) {
  *out = p->civil;
  out->energy = cd_native_energy(w->native, p->civil.ref);
  out->fatigue = p->fatigue;
  out->sleeping = p->sleeping;
  out->carried_meals = p->meals;
  out->health_condition = p->health.condition;
  out->health_severity = p->health.severity;
  out->activity = p->activity;
  memcpy(out->jobs, p->jobs, sizeof out->jobs);
  CdJob *job = cd_job((CdWorld *)w, p->jobs[0]);
  out->occupation = job ? job->view.occupation : 0;
  out->workplace = job ? job->view.workplace : 0;
  out->work_state = p->work.view.state;
  out->work_progress = p->work.view.progress;
  out->work_duration = p->work.view.duration;
  out->wallet = p->wallet;
  out->credentials = p->credentials;
  memcpy(out->institution_memberships, p->memberships,
         sizeof out->institution_memberships);
  out->vehicle = p->vehicle;
  out->driving = p->driving;
  out->refuge = p->refuge;
  out->service_request = p->mission;
  out->gathering = p->gathering;
  CdClockView clock;
  if (cd_world_clock(w, &clock, NULL) == CD_OK)
    out->age = cd_civil_age(out->birth, clock.civil);
  if (out->position_known)
    out->position = cd_city_position(w, p);
  if (out->household && out->household <= w->home_count)
    out->housing_known = w->homes[out->household - 1].housing_known;
}
CdCitizen *cd_citizen(CdWorld *w, CdPersonRef ref) {
  if (!w || !ref.person_id)
    return NULL;
  size_t lo = 0, hi = w->count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (w->people[mid].civil.ref.person_id < ref.person_id)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo < w->count && w->people[lo].civil.ref.person_id == ref.person_id &&
                 w->people[lo].civil.ref.world_token == ref.world_token
             ? &w->people[lo]
             : NULL;
}
CdStatus cd_world_resident(const CdWorld *w, size_t i, CdResidentView *out,
                           CdDiagnostic *d) {
  if (!w || !out || i >= w->count)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown resident index");
  cd_population_view(w, &w->people[i], out);
  return cd_diag(
      d, CD_OK,
      "SDK-named, synthetic-demography resident; affiliation unknown");
}
CdStatus cd_world_resolve(const CdWorld *w, CdPersonRef ref,
                          CdResidentView *out, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE,
                   "Missing or foreign-world resident");
  cd_population_view(w, p, out);
  return cd_diag(d, CD_OK, "Live civil record");
}

CdStatus cd_world_category(CdWorld *w, CdPersonRef ref,
                           CdResidentCategory category, const char *basis,
                           CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown admission reference");
  if ((unsigned)category > CD_EVACUEE || !basis || !basis[0] ||
      strlen(basis) >= sizeof p->civil.admission_basis)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Category requires a bounded explicit configuration basis");
  CdResidentView view;
  cd_population_view(w, p, &view);
  if (category == CD_TROOP && view.age < 18)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Technical troop category requires an adult");
  p->civil.category = category;
  snprintf(p->civil.admission_basis, sizeof p->civil.admission_basis, "%s",
           basis);
  return cd_diag(d, CD_OK,
                 "Explicit category recorded; no geographic inference");
}
