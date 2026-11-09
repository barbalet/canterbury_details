#include "cd_city_internal.h"
#include "cd_native.h"
void cd_human_init(CdWorld *w) {
  w->last_human_minute = cd_native_minutes(w->native);
  w->food.initial_meals = w->count * 3;
  CdCivilTime t;
  cd_smoke_civil_time(w->last_human_minute + w->origin_offset, &t, NULL);
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    p->meals = 3;
    p->civil.age = cd_civil_age(p->civil.birth, t);
    cd_native_energy_set(w->native, p->civil.ref, cd_native_full());
  }
}
CdStatus cd_world_needs(const CdWorld *world, CdPersonRef ref, CdNeedsView *out,
                        CdDiagnostic *d) {
  CdWorld *w = (CdWorld *)world;
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown needs reference");
  *out = (CdNeedsView){p->fatigue,       cd_native_energy(w->native, ref),
                       p->sleeping,      p->activity,
                       p->needs_minutes, p->eaten,
                       p->unmet_meals,   p->meals,
                       p->raw,           p->skills};
  return cd_diag(d, CD_OK, "Native energy and ordinary needs");
}
CdStatus cd_world_rest(CdWorld *w, CdPersonRef ref, unsigned minutes,
                       CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w) || minutes > 1440)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Rest request needs live actor and 0..1440 minutes");
  p->rest_left = minutes;
  return cd_diag(d, CD_OK, "Bounded rest intention");
}
CdStatus cd_world_training(CdWorld *w, CdPersonRef ref, unsigned skills,
                           const char *basis, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w) || p->civil.age < 18 || skills > 15 ||
      !cd_basis_valid(basis))
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Training requires an adult, known skills and explicit basis");
  p->skills = skills;
  snprintf(p->training_basis, sizeof p->training_basis, "%s", basis);
  return cd_diag(
      d, CD_OK,
      "Explicit technical qualification, not a job or inferred faith");
}
static int eat(CdWorld *w, CdCitizen *p) {
  if (!cd_food_personal(w, p))
    return 0;
  if (w->ration_limit) {
    uint64_t day = cd_now(w) / 1440;
    if (p->ration_day != day) {
      p->ration_day = day;
      p->ration_used = 0;
    }
    p->ration_used++;
  }
  p->meals--;
  p->eaten++;
  w->food.consumed++;
  int before = cd_native_energy(w->native, p->civil.ref);
  cd_native_energy_set(w->native, p->civil.ref,
                       before + (int)cd_native_hungry() / 2);
  cd_native_awake(w->native, p->civil.ref, 1);
  cd_native_remember_food(w->native, p->civil.ref,
                          cd_native_energy(w->native, p->civil.ref) - before);
  cd_record_emit(w, CD_EVENT_EAT, p->civil.ref, (CdPersonRef){0}, 0, 0,
                 CD_RECORD_PERSONAL, "native EVENT_EAT; consumed finite meal",
                 "Ate a prepared meal");
  p->activity = CD_ACTIVITY_MEAL;
  cd_native_block(w->native, p->civil.ref, 0);
  return 1;
}
void cd_needs_tick(CdWorld *w) {
  CdCivilTime t;
  cd_smoke_civil_time(cd_native_minutes(w->native) + w->origin_offset, &t,
                      NULL);
  unsigned minute = t.minute_of_day;
  /* Plan every body's survival state before any cross-person caregiving. */
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    p->activity = CD_ACTIVITY_IDLE;
    p->sleeping = cd_job_sleep(w, p, minute) || p->fatigue >= 960;
    p->needs_minutes++;
    unsigned age = cd_civil_age(p->civil.birth, t);
    if (age != p->civil.age) {
      w->birthdays++;
      p->civil.age = age;
      if (age >= 18)
        p->civil.guardian = (CdPersonRef){0};
    }
    cd_native_awake(w->native, p->civil.ref, 1);
    if (!p->sleeping || cd_native_minutes(w->native) % 4 == 0)
      cd_native_energy_set(w->native, p->civil.ref,
                           cd_native_energy(w->native, p->civil.ref) - 1);
    if (p->health.mortality_pending) {
      p->activity = CD_ACTIVITY_WAIT;
      cd_native_block(w->native, p->civil.ref, 0);
      continue;
    }
    if (p->civil.age >= 6 &&
        cd_native_energy(w->native, p->civil.ref) <= (int)cd_native_hungry() &&
        eat(w, p)) {
      p->sleeping = 0;
      continue;
    }
    if (p->sleeping || p->rest_left) {
      p->activity = CD_ACTIVITY_REST;
      cd_native_awake(w->native, p->civil.ref, !p->sleeping);
      cd_native_block(w->native, p->civil.ref, p->sleeping);
    } else if (p->health.severity >= 600 || p->health.clinic) {
      p->activity = CD_ACTIVITY_CARE;
      cd_native_block(w->native, p->civil.ref, 0);
    }
  }
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (p->civil.age >= 6 || p->health.mortality_pending ||
        !cd_ration_available(w, p) ||
        cd_native_energy(w->native, p->civil.ref) > (int)cd_native_hungry())
      continue;
    CdCitizen *g = cd_citizen(w, p->civil.guardian);
    int food =
        p->meals || (p->inside != CD_NONE &&
                     p->inside == w->homes[p->civil.household - 1].dwelling &&
                     w->home_meals[p->civil.household - 1]);
    if (g && food && g->civil.age >= 18 && g->health.severity < 600 &&
        !g->health.mortality_pending && g->activity == CD_ACTIVITY_IDLE &&
        cd_colocated(w, g, p) && cd_native_claim(w->native, g->civil.ref)) {
      if (eat(w, p)) {
        g->activity = CD_ACTIVITY_CARE;
        p->sleeping = 0;
        cd_native_awake(w->native, p->civil.ref, 1);
      }
    }
  }
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (p->health.mortality_pending)
      continue;
    if (p->activity == CD_ACTIVITY_REST) {
      if (p->rest_left)
        p->rest_left--;
      unsigned recovery = p->sleeping ? 4 : 2;
      p->fatigue = p->fatigue > recovery ? p->fatigue - recovery : 0;
    } else
      p->fatigue = p->fatigue < 1440 ? p->fatigue + 1 : 1440;
    if (cd_native_energy(w->native, p->civil.ref) <= (int)cd_native_hungry()) {
      p->unmet_meals++;
      w->food.unmet++;
    }
  }
}
