#include "cd_city_internal.h"
static uint64_t mix(uint64_t h, uint64_t x) {
  return (h ^ x) * UINT64_C(1099511628211);
}
static uint64_t text(uint64_t h, const char *s) {
  while (*s)
    h = mix(h, (unsigned char)*s++);
  return mix(h, 0);
}
uint64_t cd_civic_digest(const CdWorld *w, uint64_t h) {
#define H(x) h = mix(h, (uint64_t)(x))
#define T(x) h = text(h, x)
  H(w->office_count);
  H(w->roster_count);
  H(w->service_count);
  H(w->gathering_count);
  H(w->vehicle_count);
  H(w->pressure_count);
  H(w->rain);
  H(w->alert);
  H(w->blackout);
  H(w->ration_limit);
  H(w->water_imported);
  H(w->water_used);
  H(w->water_exported);
  H(w->fire_minutes);
  H(w->damage);
  H(w->service_outputs);
  H(w->attendance_minutes);
  for (size_t i = 0; i < w->count; i++) {
    const CdCitizen *p = &w->people[i];
    H(p->credentials);
    H(p->ordination_kind);
    H(p->ordination_institution);
    H(p->water);
    H(p->ration_used);
    H(p->ration_day);
    H(p->mission);
    H(p->gathering);
    H(p->vehicle);
    H(p->driving);
    H(p->refuge);
    H(p->last_care_grant);
    H(p->last_care_subject);
    T(p->credential_basis);
    T(p->ordination_basis);
    for (unsigned j = 0; j < 4; j++)
      H(p->memberships[j]);
  }
  for (uint32_t i = 0; i < w->content->np; i++) {
    const CdPremises *p = &w->premises[i];
    H(p->water);
    H(p->fire);
    H(p->shelter_capacity);
    T(p->water_basis);
    T(p->fire_basis);
    T(p->shelter_basis);
  }
  for (size_t i = 0; i < w->life_count; i++)
    H(w->life[i].water);
  for (size_t i = 0; i < w->office_count; i++) {
    const CdOfficeView *v = &w->offices[i].view;
    H(v->id);
    H(v->institution);
    H(v->job);
    H(v->kind);
    H(v->last_holder.person_id);
    H(v->successions);
    H(v->outputs);
    H(v->capabilities);
    T(v->basis);
  }
  for (size_t i = 0; i < w->roster_count; i++) {
    const CdRosterView *v = &w->rosters[i].view;
    H(v->id);
    H(v->job);
    H(v->capabilities);
    H(v->outputs);
    T(v->title);
    T(v->basis);
  }
  for (size_t i = 0; i < w->service_count; i++) {
    const CdService *r = &w->services[i];
    const CdServiceView *v = &r->view;
    H(v->id);
    H(v->key);
    H(v->state);
    H(v->provider);
    H(v->building);
    H(v->worker.person_id);
    H(v->subject.person_id);
    H(v->kind);
    H(v->religious);
    H(v->minutes);
    H(v->progress);
    H(v->paid);
    H(v->reserved_money);
    H(r->deadline);
    H(r->rate);
    H(r->care_start);
    H(r->care_owned);
    T(v->basis);
    T(v->reason);
  }
  for (size_t i = 0; i < w->gathering_count; i++) {
    const CdGathering *r = &w->gatherings[i];
    const CdGatheringView *v = &r->view;
    H(v->id);
    H(v->key);
    H(v->state);
    H(v->office);
    H(v->kind);
    H(v->starts);
    H(v->minutes);
    H(v->capacity);
    H(v->repeat_days);
    H(v->repeats);
    H(v->occurrence);
    H(v->progress);
    H(v->present);
    H(v->completed);
    H(v->members_only);
    H(v->paid);
    H(v->reserved_money);
    H(r->count);
    H(r->rate);
    T(v->basis);
    T(v->reason);
    for (size_t k = 0; k < r->count; k++) {
      H(r->attendees[k].person_id);
      H(r->credited[k]);
    }
  }
  for (size_t i = 0; i < w->vehicle_count; i++) {
    const CdVehicle *r = &w->vehicles[i];
    const CdVehicleView *v = &r->view;
    H(v->id);
    H(v->job);
    H(v->source);
    H(v->destination);
    H(v->crew.person_id);
    H(v->passenger.person_id);
    H(v->kind);
    H(v->phase);
    H(v->capacity);
    H(v->good);
    H(v->requested);
    H(v->loaded);
    H(v->delivered);
    H(v->meals);
    H(v->stopped);
    H(v->position_known);
    H(v->key);
    H(r->destination_door);
    H(r->requested_passenger.person_id);
    H(r->unload_building);
    H(r->resume_phase);
    H(r->aborted);
    uint64_t x, y;
    memcpy(&x, &v->position.x, 8);
    memcpy(&y, &v->position.y, 8);
    H(x);
    H(y);
    T(v->basis);
    T(v->reason);
  }
  for (size_t i = 0; i < w->pressure_count; i++) {
    const CdPressureView *v = &w->pressures[i].view;
    H(v->id);
    H(v->key);
    H(v->starts);
    H(v->kind);
    H(v->target);
    H(v->state);
    H(v->minutes);
    H(v->value);
    H(v->applied);
    T(v->basis);
  }
#undef H
#undef T
  return h;
}
