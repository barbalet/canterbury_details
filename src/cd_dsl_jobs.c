#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include <string.h>

const CdDslRole *cd_dsl_job_role(CdEntityId occupation) {
  size_t legacy = cd_occupation_count();
  return occupation > legacy && occupation - legacy <= cd_dsl_role_count()
             ? cd_dsl_role_at((size_t)(occupation - legacy - 1))
             : NULL;
}

const char *cd_dsl_role_recipe(const CdDslRole *role) {
  static const struct { const char *career, *recipe; } bindings[] = {
      {"miller", "mill_wheat"},
      {"carpenter", "cut_timber"},
      {"weaver", "cut_cloth"},
      {"printer", "cut_paper"},
      {"metalworker", "form_metalwork"},
      {"joiner", "cut_timber"},
      {"dressmaker", "cut_cloth"},
      {"shoemaker", "trial_trim_leather"},
      {"butcher", "trial_portion_meat"},
      {"fishmonger", "trial_portion_fish"},
  };
  if (role)
    for (size_t i = 0; i < sizeof bindings / sizeof bindings[0]; ++i)
      if (!strcmp(role->career, bindings[i].career))
        return bindings[i].recipe;
  return NULL;
}

int cd_job_role_enabled(CdEntityId occupation) {
  const CdDslRole *role = cd_dsl_job_role(occupation);
  if (role)
    return cd_dsl_role_recipe(role) != NULL;
  CdOccupationView legacy;
  return occupation && occupation <= cd_occupation_count() &&
         cd_occupation((size_t)occupation - 1, &legacy, NULL) == CD_OK &&
         legacy.enabled;
}

static unsigned role_number(const CdDslRole *role) {
  return (unsigned)(role - cd_dsl_role_at(0)) + 1;
}

int cd_dsl_certified(const CdCitizen *person, const CdDslRole *role) {
  unsigned number = role_number(role);
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i)
    if (person->certificates[i].role == number)
      return 1;
  return 0;
}

int cd_dsl_certificate_valid(const CdCitizen *person) {
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i) {
    unsigned number = person->certificates[i].role;
    if (!number)
      continue;
    const CdDslRole *role = cd_dsl_role_at(number - 1);
    if (!cd_dsl_role_recipe(role) || role->training_post || person->civil.age < 18)
      return 0;
    for (size_t j = 0; j < i; ++j)
      if (person->certificates[j].role == number)
        return 0;
  }
  return 1;
}

CdStatus cd_world_dsl_certify(CdWorld *w, CdPersonRef ref, const char *name,
                              const char *basis, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(w, ref);
  const CdDslRole *role = cd_dsl_role(name);
  if (!cd_model_ready(w) || !person || person->civil.age < 18 ||
      person->health.mortality_pending || !cd_dsl_role_recipe(role) ||
      role->training_post || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Explicit adult technical role assessment required");
  if (cd_dsl_certified(person, role))
    return cd_diag(d, CD_OK, "Existing technical certificate");
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i)
    if (!person->certificates[i].role) {
      person->certificates[i].role = role_number(role);
      cd_record_emit(w, CD_EVENT_JOB, ref, (CdPersonRef){0}, 0, 0,
                     CD_RECORD_DIAGNOSTIC, basis, "Technical role certificate recorded");
      return cd_diag(d, CD_OK, "Technical role certificate recorded");
    }
  return cd_diag(d, CD_CAPACITY, "Resident certificate limit reached");
}

CdStatus cd_world_dsl_job_create(CdWorld *w, const CdJobSpec *spec, const char *name,
                                  CdEntityId *out, CdDiagnostic *d) {
  const CdDslRole *role = cd_dsl_role(name);
  if (!spec || !cd_dsl_role_recipe(role))
    return cd_diag(d, CD_UNSUPPORTED, "DSL role has no qualified technical work binding");
  CdJobSpec named = *spec;
  named.occupation = cd_occupation_count() + role_number(role);
  named.apprenticeship = role->training_post;
  return cd_world_job_create(w, &named, out, d);
}

int cd_job_mentor_matches(const CdJob *apprentice, const CdJob *mentor) {
  if (!mentor || !mentor->view.open || mentor->view.apprenticeship ||
      mentor->view.workplace != apprentice->view.workplace)
    return 0;
  const CdDslRole *learner = cd_dsl_job_role(apprentice->view.occupation);
  if (!learner)
    return mentor->view.occupation == apprentice->view.occupation;
  const CdDslRole *teacher = cd_dsl_job_role(mentor->view.occupation);
  return teacher && teacher->stage >= learner->mentor_minimum_stage && !strcmp(learner->career, teacher->career);
}

static const CdDslRole *authority(CdWorld *w, CdCitizen *person,
                                   CdEntityId premises, int owner) {
  uint32_t b = w ? cd_polygon_index(w->content, premises) : CD_NONE;
  if (!cd_model_ready(w) || !person || b == CD_NONE ||
      !person->civil.position_known || person->door != CD_NONE ||
      person->inside != b || w->premises[b].closed || !cd_usable(w, b) ||
      person->sleeping || person->health.severity >= 600)
    return NULL;
  CdClockView clock;
  if (cd_world_clock(w, &clock, NULL) != CD_OK)
    return NULL;
  for (size_t i = 0; i < 2; ++i) {
    CdJob *job = cd_job(w, person->jobs[i]);
    if (!job || job->view.workplace != premises ||
        !cd_shift(job->view.shift_start, job->view.shift_end, clock.civil.minute_of_day))
      continue;
    const CdDslRole *role = cd_dsl_job_role(job->view.occupation);
    if (role && role->stage >= 4 && role->owner == (unsigned)owner &&
        cd_job_qualified(w, job, person))
      return role;
  }
  return NULL;
}

CdStatus cd_world_dsl_supervise(CdWorld *w, CdPersonRef supervisor,
                                 CdPersonRef worker, uint64_t key, CdEntityId job_id,
                                 const char *recipe, unsigned batches, const char *basis,
                                 uint64_t *out, CdDiagnostic *d) {
  CdJob *job = cd_job(w, job_id);
  CdCitizen *person = cd_citizen(w, supervisor);
  const CdDslRole *role = job ? authority(w, person, job->view.workplace, 0) : NULL;
  const CdDslRole *worker_role = job ? cd_dsl_job_role(job->view.occupation) : NULL;
  if (!role || !worker_role || strcmp(role->career, worker_role->career))
    return cd_diag(d, CD_UNSUPPORTED, "On-site supervisor of the same trade required");
  return cd_world_refined_work_submit(w, worker, key, job_id, recipe, batches, basis, out, d);
}

CdStatus cd_world_dsl_fund(CdWorld *w, CdPersonRef ref, CdEntityId premises,
                            uint64_t amount, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(w, ref);
  uint32_t b = w ? cd_polygon_index(w->content, premises) : CD_NONE;
  if (b == CD_NONE || !authority(w, person, premises, 1) ||
      w->owners[b].person_id != ref.person_id ||
      w->owners[b].world_token != ref.world_token)
    return cd_diag(d, CD_UNSUPPORTED, "On-site proprietor and property authority required");
  return cd_world_money_transfer(w,
      (CdAccount){.kind = CD_ACCOUNT_PERSON, .person = ref},
      (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = premises}, amount, d);
}
