#include "cd_city_internal.h"
#include "cd_native.h"
void cd_care_release(CdWorld *w, CdCitizen *a) {
  if (a->care_target.person_id) {
    CdCitizen *p = cd_citizen(w, a->care_target);
    if (p && p->health.carer.person_id == a->civil.ref.person_id) {
      uint32_t b = cd_polygon_index(w->content, p->health.clinic);
      if (b != CD_NONE && w->premises[b].occupied)
        w->premises[b].occupied--;
      p->health.clinic = 0;
      p->health.carer = (CdPersonRef){0};
    }
    a->care_target = (CdPersonRef){0};
    a->preventive_minutes = 0;
  }
  if (a->health.carer.person_id) {
    CdCitizen *carer = cd_citizen(w, a->health.carer);
    uint32_t b = cd_polygon_index(w->content, a->health.clinic);
    if (b != CD_NONE && w->premises[b].occupied)
      w->premises[b].occupied--;
    if (carer) {
      carer->care_target = (CdPersonRef){0};
      carer->preventive_minutes = 0;
    }
    a->health.clinic = 0;
    a->health.carer = (CdPersonRef){0};
  }
}
CdStatus cd_world_health(const CdWorld *w, CdPersonRef ref, CdHealthView *out,
                         CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown health reference");
  *out = p->health;
  return cd_diag(d, CD_OK, "Technical non-predictive health state");
}
CdStatus cd_world_expose(CdWorld *w, CdPersonRef ref, CdCondition condition,
                         unsigned severity, unsigned minutes, const char *basis,
                         CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w) ||
      (condition != CD_ILL && condition != CD_INJURED) || !severity ||
      severity > 1000 || !minutes || minutes > 10080 || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Exposure needs bounded explicit technical inputs");
  if (p->health.condition != CD_HEALTHY || p->health.mortality_pending)
    return cd_diag(d, CD_INVALID_STATE,
                   "Existing condition cannot be overwritten");
  p->health.condition = condition;
  p->health.severity = cd_native_minutes(w->native) < p->health.prevention_until
                           ? ((severity + 1) / 2)
                           : severity;
  p->health.remaining = minutes;
  p->condition_since = cd_native_minutes(w->native);
  snprintf(p->health.basis, 96, "%s", basis);
  return cd_diag(
      d, CD_OK,
      "Configured health event; no historical disease incidence inferred");
}
CdStatus cd_world_clinic(CdWorld *w, CdEntityId id, unsigned beds,
                         unsigned medicine, const char *basis,
                         CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      w->content->polygons[b].kind != CD_BUILDING ||
      !w->content->polygons[b].capacity_known || !beds ||
      beds > cd_capacity(w, b) || beds < w->premises[b].occupied ||
      medicine > 4096 || !cd_basis_valid(basis) ||
      (uint64_t)w->premises[b].medicine + medicine > 4096 ||
      UINT64_MAX - w->medicine_imported < medicine)
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Clinic needs explicit reviewed capacity and finite medicine input");
  w->premises[b].beds = beds;
  w->premises[b].medicine += medicine;
  w->medicine_imported += medicine;
  snprintf(w->premises[b].clinic_basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Technical clinic configured; medicine imports recorded");
}
CdStatus cd_care_begin(CdWorld *w, CdPersonRef ar, CdPersonRef pr,
                       CdEntityId id, unsigned preventive, CdDiagnostic *d) {
  CdCitizen *a = cd_citizen(w, ar), *p = cd_citizen(w, pr);
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!a || !p || a == p || b == CD_NONE || preventive > 10080 ||
      a->civil.age < 18 || !(a->skills & CD_SKILL_CARE) ||
      a->health.severity >= 600 || a->health.mortality_pending ||
      p->health.mortality_pending || !cd_colocated(w, a, p) || a->inside != b)
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Care requires trained capable adult and physical clinic co-location");
  if (a->care_target.person_id || p->health.carer.person_id)
    return cd_diag(d, CD_INVALID_STATE, "Care assignment already reserved");
  CdPremises *s = &w->premises[b];
  if (!s->beds || s->occupied >= s->beds || !s->medicine)
    return cd_diag(d, CD_CAPACITY, "No clinic bed/medicine capacity");
  if (!preventive && p->health.condition == CD_HEALTHY)
    return cd_diag(d, CD_INVALID_ARGUMENT, "No active condition to treat");
  a->care_target = pr;
  a->preventive_minutes = preventive;
  p->health.carer = ar;
  p->health.clinic = id;
  s->occupied++;
  return cd_diag(d, CD_OK,
                 "Care reserved, not teleported or instantly performed");
}
CdStatus cd_world_care(CdWorld *w, CdPersonRef a, CdPersonRef p, CdEntityId b,
                       unsigned preventive, CdDiagnostic *d) {
  if (!cd_model_ready(w))
    return cd_diag(d, CD_INVALID_STATE, "Care needs closed interval");
  return cd_care_begin(w, a, p, b, preventive, d);
}
CdStatus cd_world_care_cancel(CdWorld *w, CdPersonRef ar, CdDiagnostic *d) {
  CdCitizen *a = cd_citizen(w, ar);
  if (!a || !cd_model_ready(w))
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown care actor");
  cd_care_release(w, a);
  return cd_diag(d, CD_OK, "Care/bed reservation released once");
}
void cd_health_tick(CdWorld *w) {
  uint64_t minute = cd_native_minutes(w->native);
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *a = &w->people[i];
    if (!a->care_target.person_id)
      continue;
    CdCitizen *p = cd_citizen(w, a->care_target);
    uint32_t b = p ? cd_polygon_index(w->content, p->health.clinic) : CD_NONE;
    if (!p || b == CD_NONE || !cd_colocated(w, a, p) || a->inside != b ||
        a->civil.age < 18 || !(a->skills & CD_SKILL_CARE) ||
        a->health.severity >= 600 || a->health.mortality_pending ||
        p->health.mortality_pending) {
      cd_care_release(w, a);
      continue;
    }
    CdPremises *clinic = &w->premises[b];
    if (a->activity != CD_ACTIVITY_IDLE || !clinic->medicine ||
        !cd_usable(w, b))
      continue;
    if (!cd_native_claim(w->native, a->civil.ref))
      continue;
    a->last_care_grant = minute;
    a->last_care_subject = p->civil.ref.person_id;
    clinic->medicine--;
    w->medicine_used++;
    a->activity = CD_ACTIVITY_CARE;
    p->health.care_minutes++;
    if (a->preventive_minutes) {
      if (p->health.prevention_until < minute + a->preventive_minutes)
        p->health.prevention_until = minute + a->preventive_minutes;
      cd_care_release(w, a);
      continue;
    }
    p->health.severity = p->health.severity > 10 ? p->health.severity - 10 : 0;
    if (p->health.severity < 600 && p->health.severity)
      p->health.condition = CD_RECOVERING;
    if (p->health.remaining > 4)
      p->health.remaining -= 4;
    else
      p->health.remaining = 0;
  }
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (p->health.mortality_pending)
      continue;
    if (!cd_native_energy(w->native, p->civil.ref)) {
      if (p->health.starvation < UINT32_MAX)
        p->health.starvation++;
    } else
      p->health.starvation = 0;
    if (p->health.condition != CD_HEALTHY) {
      if (p->health.remaining)
        p->health.remaining--;
      if (!p->health.severity || !p->health.remaining) {
        p->health.condition = CD_HEALTHY;
        p->health.severity = 0;
        p->health.remaining = 0;
        cd_care_release(w, p);
      } else {
        if (minute % 60 == 0 && p->health.severity >= 500)
          cd_native_energy_set(w->native, p->civil.ref,
                               cd_native_energy(w->native, p->civil.ref) - 5);
        if (p->health.severity >= 900 && minute - p->condition_since >= 60)
          p->health.mortality_pending = 1;
      }
    }
    if (p->civil.position_known && p->health.starvation >= 360)
      p->health.mortality_pending = 1;
    if (p->health.mortality_pending) {
      p->health.condition = CD_CRITICAL;
      p->activity = CD_ACTIVITY_WAIT;
      cd_native_block(w->native, p->civil.ref, 0);
    }
  }
}

CdStatus cd_world_clinic_view(const CdWorld *w, CdEntityId id,
                              CdClinicView *out, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!w || !out || b == CD_NONE || !w->premises[b].beds)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown configured clinic");
  uint64_t stock = 0;
  for (uint32_t i = 0; i < w->content->np; i++)
    stock += w->premises[i].medicine;
  if (stock + w->medicine_used != w->medicine_imported)
    return cd_diag(d, CD_INVALID_STATE, "Medicine conservation mismatch");
  const CdPremises *p = &w->premises[b];
  *out = (CdClinicView){p->beds, p->occupied, p->medicine, w->medicine_imported,
                        w->medicine_used};
  return cd_diag(d, CD_OK,
                 "Abstract medicine units reconcile; bed reservation is "
                 "independent of residence");
}
