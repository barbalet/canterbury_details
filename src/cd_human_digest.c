#include "cd_city_internal.h"
static uint64_t mix(uint64_t h, uint64_t x) {
  return (h ^ x) * UINT64_C(1099511628211);
}
static uint64_t text_hash(uint64_t h, const char *s) {
  while (*s)
    h = mix(h, (unsigned char)*s++);
  return mix(h, 0);
}
uint64_t cd_human_digest(const CdWorld *w, uint64_t h) {
#define H(x) h = mix(h, (uint64_t)(x))
  H(w->initial_count);
  H(w->admissions);
  H(w->births);
  H(w->deaths);
  H(w->departures);
  H(w->birthdays);
  H(w->last_human_minute);
  H(w->mortality_blocked);
  H(w->food.initial_meals);
  H(w->food.imported_meals);
  H(w->food.imported_raw);
  H(w->food.produced);
  H(w->food.consumed);
  H(w->food.retired_transfers);
  H(w->food.unmet);
  H(w->food.exported_meals);
  H(w->food.exported_raw);
  H(w->medicine_imported);
  H(w->medicine_used);
  for (size_t i = 0; i < w->count; i++) {
    const CdCitizen *p = &w->people[i];
    H(p->civil.parents[0].person_id);
    H(p->civil.parents[1].person_id);
    H(p->fatigue);
    H(p->rest_left);
    H(p->sleeping);
    H(p->household_care);
    H(p->school_door);
    H(p->school_leave);
    H(p->school_start);
    H(p->school_end);
    H(p->activity);
    H(p->meals);
    H(p->raw);
    H(p->skills);
    H(p->needs_minutes);
    H(p->eaten);
    H(p->unmet_meals);
    H(p->health.condition);
    H(p->health.severity);
    H(p->health.remaining);
    H(p->health.starvation);
    H(p->health.care_minutes);
    H(p->health.prevention_until);
    H(p->health.clinic);
    H(p->health.carer.person_id);
    H(p->health.mortality_pending);
    H(p->health.mortality_blocked);
    H(p->condition_since);
    H(p->care_target.person_id);
    H(p->feed_target.person_id);
    H(p->preventive_minutes);
    H(p->food_action);
    H(p->food_endpoint.kind);
    H(p->food_endpoint.id);
    H(p->food_remaining);
    h = text_hash(h, p->training_basis);
    h = text_hash(h, p->health.basis);
  }
  for (size_t i = 0; i < w->home_count; i++) {
    H(w->home_meals[i]);
    H(w->home_raw[i]);
  }
  for (uint32_t i = 0; i < w->content->np; i++) {
    const CdPremises *p = &w->premises[i];
    H(p->meals);
    H(p->raw);
    H(p->enabled);
    H(p->producer);
    H(p->beds);
    H(p->occupied);
    H(p->medicine);
    h = text_hash(h, p->food_basis);
    h = text_hash(h, p->clinic_basis);
  }
  H(w->life_count);
  for (size_t i = 0; i < w->life_count; i++) {
    const CdLifeRecord *p = &w->life[i];
    H(p->ref.person_id);
    H(p->minute);
    H(p->household);
    H(p->age);
    H(p->reason);
    H(p->meals);
    H(p->raw);
    H(p->position_known);
    if (p->position_known) {
      uint64_t x, y;
      memcpy(&x, &p->position.x, sizeof x);
      memcpy(&y, &p->position.y, sizeof y);
      H(x);
      H(y);
    }
    H(p->birth.year);
    H(p->birth.month);
    H(p->birth.day);
    H(p->parents[0].person_id);
    H(p->parents[1].person_id);
    h = text_hash(h, p->name);
    h = text_hash(h, p->basis);
  }
  h = text_hash(h, cd_occupation_identity());
#undef H
  return h;
}
