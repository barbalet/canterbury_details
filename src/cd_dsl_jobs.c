#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include "cd_native.h"
#include <string.h>
#include <stdlib.h>

const CdDslRole *cd_dsl_job_role(CdEntityId occupation) {
  size_t legacy = cd_occupation_count();
  return occupation > legacy && occupation - legacy <= cd_dsl_role_count()
             ? cd_dsl_role_at((size_t)(occupation - legacy - 1))
             : NULL;
}

const char *cd_dsl_role_recipe(const CdDslRole *role) {
  static const struct { const char *career, *recipe; } bindings[] = {
      {"brewer", "brew_small_beer"},
      {"plumber", "fit_pipe"},
      {"electrician", "form_electrical_fitting"},
      {"motor_mechanic", "recondition_vehicle_part"},
      {"potter", "form_pottery"},
      {"bricklayer", "prepare_brickwork"},
      {"stonemason", "dress_stone"},
      {"roofer", "cut_roofing"},
      {"plasterer", "mix_plaster"},
      {"painter_decorator", "prepare_paint"},
      {"crop_farming", "clean_harvest"},
      {"livestock_farming", "sort_eggs"},
      {"dairy", "churn_butter"},
      {"horticulture", "grade_vegetables"},
      {"woodland", "split_firewood"},
      {"miller", "mill_wheat"},
      {"baker", "bake_bread"},
      {"kitchen", "cook_meal"},
      {"hotel_service", "cook_meal"},
      {"licensed_hospitality", "cook_meal"},
      {"domestic_service", "cook_meal"},
      {"laundry", "wash_linen"},
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

int cd_dsl_role_can_make(const CdDslRole *role, const char *recipe) {
  const char *primary = cd_dsl_role_recipe(role);
  return role && recipe && ((primary && !strcmp(primary, recipe)) ||
      (!strcmp(role->career, "printer") && !strcmp(recipe, "bind_school_book")) ||
      (!strcmp(role->career, "dressmaker") && !strcmp(recipe, "sew_garment")) ||
      (!strcmp(role->career, "shoemaker") && !strcmp(recipe, "make_shoes")));
}

int cd_sector_job(CdEntityId occupation) {
  return occupation > CD_SECTOR_JOB_BASE && occupation <= CD_SECTOR_JOB_BASE + 9;
}

int cd_job_role_enabled(CdEntityId occupation) {
  if (cd_sector_job(occupation))
    return 1;
  const CdDslRole *role = cd_dsl_job_role(occupation);
  if (role)
    return 1;
  CdOccupationView legacy;
  return occupation && occupation <= cd_occupation_count() &&
         cd_occupation((size_t)occupation - 1, &legacy, NULL) == CD_OK &&
         legacy.enabled;
}

static unsigned role_number(const CdDslRole *role) {
  return (unsigned)(role - cd_dsl_role_at(0)) + 1;
}

CdEntityId cd_dsl_role_occupation(const CdDslRole *role) {
  return role ? cd_occupation_count() + role_number(role) : 0;
}

int cd_dsl_role_premises(const CdDslRole *role, const char *use) {
  const CdDslCareer *career = role ? cd_dsl_career(role->career) : NULL;
  for (size_t i = 0; career && i < career->premises_count; ++i)
    if (!strcmp(career->premises[i], use))
      return 1;
  /* Technical startup permissions for construction offices, school support and leisure. */
  return role && role->stage == 2 &&
      ((!strcmp(role->career, "painter_decorator") && !strcmp(use, "commercial")) ||
       (!strcmp(role->career, "hotel_service") && !strcmp(use, "leisure")) ||
       (!strcmp(role->career, "technical_instruction") && !strcmp(use, "education_support")));
}

const CdDslRole *cd_dsl_startup_role(unsigned sector, const char *use, CdPersonId person) {
  static const char *const roles[][6] = {
    {"grocer.2", "draper.2", "ironmonger.2", "bookseller.2", "coal_merchant.2"},
    {"weaver.2", "dressmaker.2", "shoemaker.2", "printer.2", "metalworker.2", "potter.2"},
    {"painter_decorator.2", "bricklayer.2", "roofer.2", "plasterer.2", "electrician.2"},
    {"crop_farming.2", "livestock_farming.2", "dairy.2", "horticulture.2", "miller.2"},
    {"technical_instruction.2"},
    {"social_care.2"},
    {"hotel_service.2", "kitchen.2", "laundry.2"},
    {"office_administration.2", "municipal_administration.2", "postal_service.2"},
    {"religious_lay_service.2", "charitable_service.2"},
  };
  if (!sector || sector > sizeof roles / sizeof roles[0])
    return NULL;
  const CdDslRole *compatible[6];
  size_t count = 0;
  for (size_t i = 0; i < 6 && roles[sector - 1][i]; ++i) {
    const CdDslRole *role = cd_dsl_role(roles[sector - 1][i]);
    if (role && !role->independent && cd_dsl_role_premises(role, use))
      compatible[count++] = role;
  }
  return count ? compatible[person % count] : NULL;
}

int cd_dsl_appointment(const CdWorld *world, const CdCitizen *person,
                       const CdDslRole *role, CdEntityId institution) {
  const char *tradition = NULL;
  if (!strcmp(role->career, "anglican_ministry"))
    tradition = "anglican";
  else if (!strcmp(role->career, "roman_catholic_ministry"))
    tradition = "roman_catholic";
  else if (!strcmp(role->career, "jewish_ministry"))
    tradition = "judaism";
  if (!tradition)
    return 1;
  uint32_t site = cd_site_index(world->content, institution);
  if (site == CD_NONE || person->ordination_institution != institution)
    return 0;
  const CdSite *source = &world->content->sites[site];
  if (!strcmp(tradition, "judaism"))
    return person->ordination_kind == CD_OFFICE_RABBI &&
           (!strcmp(source->religion, "judaism") || !strcmp(source->religion, "Jewish"));
  return person->ordination_kind == CD_OFFICE_PRIEST &&
         (!strcmp(source->denomination, tradition) ||
          (!strcmp(tradition, "roman_catholic") && !strcmp(source->denomination, "catholic")));
}

uint64_t cd_dsl_role_minutes(const CdCitizen *person, const CdDslRole *role) {
  unsigned number = role_number(role);
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i)
    if (person->certificates[i].role == number)
      return person->certificates[i].completed_minutes;
  return 0;
}

static CdDslCertificate *role_record(CdCitizen *person, const CdDslRole *role) {
  CdDslCertificate *free_slot = NULL;
  unsigned number = role_number(role);
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i) {
    CdDslCertificate *record = &person->certificates[i];
    if (record->role == number)
      return record;
    if (!record->role && !free_slot)
      free_slot = record;
  }
  return free_slot;
}

CdStatus cd_world_dsl_training_start(CdWorld *world, CdEntityId post,
                                      unsigned minutes, CdDiagnostic *d) {
  CdJob *job = cd_job(world, post);
  const CdDslRole *role = job ? cd_dsl_job_role(job->view.occupation) : NULL;
  CdCitizen *learner = job ? cd_citizen(world, job->view.worker) : NULL;
  if (!cd_model_ready(world) || !role || !role->training_post || !minutes ||
      minutes > 10080 || !cd_job_qualified(world, job, learner) ||
      !role_record(learner, role))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Training needs a funded apprentice and qualified mentor");
  if (job->training_remaining)
    return cd_diag(d, CD_INVALID_STATE, "Training is already scheduled");
  job->training_remaining = minutes;
  job->training_learner = learner->civil.ref.person_id;
  return cd_diag(d, CD_OK, "Supervised curriculum scheduled");
}

void cd_dsl_training_tick(CdWorld *world, CdCitizen *learner, CdJob *job, unsigned minute) {
  const CdDslRole *role = job ? cd_dsl_job_role(job->view.occupation) : NULL;
  if (!role || !role->training_post || !job->training_remaining ||
      job->training_learner != learner->civil.ref.person_id ||
      !cd_job_qualified(world, job, learner) ||
      !cd_shift(job->view.shift_start, job->view.shift_end, minute) ||
      learner->activity != CD_ACTIVITY_IDLE || learner->door != CD_NONE ||
      learner->inside == CD_NONE || learner->health.severity >= 600 ||
      world->content->polygons[learner->inside].id != job->view.workplace ||
      world->premises[learner->inside].closed || !cd_usable(world, learner->inside))
    return;
  CdCitizen *mentor = cd_citizen(world, job->view.mentor);
  if (!mentor || mentor->inside != learner->inside || mentor->door != CD_NONE ||
      mentor->activity != CD_ACTIVITY_IDLE || mentor->health.severity >= 600 ||
      cd_pending(mentor->work.view.state))
    return;
  int on_shift = 0;
  for (unsigned slot = 0; slot < 2; ++slot) {
    CdJob *post = cd_job(world, mentor->jobs[slot]);
    if (cd_job_mentor_matches(job, post) &&
        cd_shift(post->view.shift_start, post->view.shift_end, minute))
      on_shift = 1;
  }
  CdDslCertificate *record = role_record(learner, role);
  CdPremises *employer = &world->premises[learner->inside];
  if (!on_shift || !record || employer->treasury < job->view.wage ||
      !cd_native_can_work(world->native, learner->civil.ref) ||
      !cd_native_can_work(world->native, mentor->civil.ref))
    return;
  if (!cd_native_claim(world->native, learner->civil.ref))
    return;
  if (!cd_native_claim(world->native, mentor->civil.ref))
    abort();
  learner->activity = mentor->activity = CD_ACTIVITY_WORK;
  if (!--job->training_remaining)
    job->training_learner = 0;
  record->role = role_number(role);
  cd_dsl_record_experience(learner, role, 1);
  employer->treasury -= job->view.wage;
  learner->wallet += job->view.wage;
  world->economy.wages += job->view.wage;
}

void cd_dsl_record_experience(CdCitizen *person, const CdDslRole *role, unsigned minutes) {
  if (!role)
    return;
  CdDslCertificate *record = role_record(person, role);
  if (!record || (!record->role && !role->training_post))
    return;
  record->role = role_number(role);
  uint64_t *completed = &record->completed_minutes;
  *completed = minutes > UINT64_MAX - *completed ? UINT64_MAX : *completed + minutes;
}

CdStatus cd_world_dsl_eligibility(const CdWorld *world, CdPersonRef ref,
                                  const char *name, CdEntityId premises,
                                  int *eligible, uint64_t *minutes,
                                  CdDiagnostic *d) {
  CdCitizen *person = cd_citizen((CdWorld *)world, ref);
  const CdDslRole *role = cd_dsl_role(name);
  uint32_t b = world ? cd_polygon_index(world->content, premises) : CD_NONE;
  if (!person || !role || b == CD_NONE || !eligible || !minutes)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Eligibility requires a role, resident and premises");
  CdJob candidate = {.view = {.occupation = cd_dsl_role_occupation(role),
      .workplace = premises, .institution = person->ordination_institution,
      .open = 1, .apprenticeship = role->training_post}};
  const char *use = world->premises[b].scenario_use[0] ? world->premises[b].scenario_use :
      world->content->polygons[b].use;
  *eligible = cd_dsl_role_premises(role, use) && cd_job_qualified(world, &candidate, person);
  *minutes = 0;
  unsigned number = role_number(role);
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i)
    if (person->certificates[i].role == number)
      *minutes = person->certificates[i].completed_minutes;
  return cd_diag(d, CD_OK, "Role qualification and recorded experience");
}

CdStatus cd_world_dsl_family_experience(const CdWorld *world, CdPersonRef ref,
                                        const char *family, uint64_t *minutes,
                                        CdDiagnostic *d) {
  CdCitizen *person = cd_citizen((CdWorld *)world, ref);
  if (!person || !cd_dsl_career(family) || !minutes)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown resident or competence family");
  *minutes = 0;
  for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i) {
    const CdDslCertificate *record = &person->certificates[i];
    const CdDslRole *role = record->role ? cd_dsl_role_at(record->role - 1) : NULL;
    if (role && !strcmp(role->career, family))
      *minutes = record->completed_minutes > UINT64_MAX - *minutes
                     ? UINT64_MAX : *minutes + record->completed_minutes;
  }
  return cd_diag(d, CD_OK, "Experience shared within the named family");
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
    if (!number) {
      if (person->certificates[i].completed_minutes)
        return 0;
      continue;
    }
    const CdDslRole *role = cd_dsl_role_at(number - 1);
    if (!role || person->civil.age < (role->training_post ? 16u : 18u) ||
        (role->training_post && !person->certificates[i].completed_minutes))
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
      person->health.mortality_pending || !role ||
      role->training_post || role->independent || !cd_basis_valid(basis))
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

CdStatus cd_world_dsl_credential(CdWorld *world, CdPersonRef ref, const char *name,
                                  const char *authority, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(world, ref);
  const CdDslRole *role = cd_dsl_role(name);
  if (!cd_model_ready(world) || !person || person->civil.age < 18 ||
      person->health.mortality_pending || !role || !role->independent ||
      role->training_post || !cd_basis_valid(authority) ||
      !cd_dsl_appointment(world, person, role, person->ordination_institution))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Independent qualification or matching appointment required");
  CdDslCertificate *record = role_record(person, role);
  if (!record)
    return cd_diag(d, CD_CAPACITY, "Resident qualification record limit reached");
  if (!record->role) {
    record->role = role_number(role);
    cd_record_emit(world, CD_EVENT_JOB, ref, (CdPersonRef){0}, 0, 0,
                   CD_RECORD_DIAGNOSTIC, authority, "Independent role credential recorded");
  }
  return cd_diag(d, CD_OK, "Explicit independent role credential");
}

CdStatus cd_world_dsl_job_create(CdWorld *w, const CdJobSpec *spec, const char *name,
                                  CdEntityId *out, CdDiagnostic *d) {
  const CdDslRole *role = cd_dsl_role(name);
  if (!spec || !role)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown DSL occupation");
  CdJobSpec named = *spec;
  named.occupation = cd_dsl_role_occupation(role);
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

CdStatus cd_world_dsl_transport_dispatch(CdWorld *w, CdPersonRef ref, CdEntityId vehicle,
                                           uint64_t key, CdEntityId source,
                                           CdEntityId destination, unsigned good, int meals,
                                           unsigned units, CdPersonRef passenger, CdDiagnostic *d) {
  CdVehicleView view;
  if (cd_world_vehicle_view(w, vehicle, &view, d) != CD_OK)
    return CD_UNKNOWN_REFERENCE;
  CdJob *post = cd_job(w, view.job);
  const CdDslRole *crew = post ? cd_dsl_job_role(post->view.occupation) : NULL;
  const CdDslCareer *family = crew ? cd_dsl_career(crew->career) : NULL;
  CdCitizen *manager = cd_citizen(w, ref);
  const CdDslRole *role = post ? authority(w, manager, post->view.workplace, 0) : NULL;
  if (!family || strcmp(family->sector, "Transport and distribution") ||
      !role || strcmp(role->career, crew->career))
    return cd_diag(d, CD_UNSUPPORTED, "On-site transport supervisor from the crew family required");
  return cd_world_transport(w, vehicle, key, source, destination, good, meals, units, passenger, d);
}

CdStatus cd_world_dsl_retail_open(CdWorld *w, CdPersonRef ref, CdEntityId premises,
                                  int open, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(w, ref);
  uint32_t building = w ? cd_polygon_index(w->content, premises) : CD_NONE;
  if (!cd_model_ready(w) || !person || building == CD_NONE ||
      person->inside != building || person->door != CD_NONE ||
      (open != 0 && open != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "On-site retail management required");
  CdClockView clock;
  if (cd_world_clock(w, &clock, d) != CD_OK)
    return CD_INVALID_STATE;
  for (unsigned slot = 0; slot < 2; ++slot) {
    CdJob *job = cd_job(w, person->jobs[slot]);
    const CdDslRole *role = job ? cd_dsl_job_role(job->view.occupation) : NULL;
    const CdDslCareer *career = role ? cd_dsl_career(role->career) : NULL;
    if (career && !strcmp(career->sector, "Retail") && role->stage >= 4 &&
        job->view.workplace == premises && cd_job_qualified(w, job, person) &&
        cd_shift(job->view.shift_start, job->view.shift_end, clock.civil.minute_of_day))
      return cd_world_employer_open(w, premises, open, d);
  }
  return cd_diag(d, CD_UNSUPPORTED, "Qualified retail manager or proprietor required");
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
