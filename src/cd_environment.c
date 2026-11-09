#include "cd_city_internal.h"
#include "cd_native.h"
static int active_at(const CdWorld *w, const CdPressureView *v) {
  return cd_pending(v->state) && cd_now(w) >= v->starts &&
         cd_now(w) - v->starts < v->minutes;
}
int cd_edge_closed(const CdWorld *w, uint32_t e) {
  if (w->closed[e])
    return 1;
  CdEntityId id = w->content->edges[e].id;
  for (size_t i = 0; i < w->pressure_count; i++) {
    const CdPressureView *v = &w->pressures[i].view;
    if (v->kind == CD_PRESSURE_EDGE_CLOSURE && v->target == id &&
        active_at(w, v))
      return 1;
  }
  return 0;
}
int cd_ration_available(const CdWorld *w, const CdCitizen *p) {
  return !w->ration_limit || p->ration_day != cd_now(w) / 1440 ||
         p->ration_used < w->ration_limit;
}
CdStatus cd_world_pressure(CdWorld *w, uint64_t key, const CdPressureSpec *s,
                           uint64_t *out, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !key ||
      (unsigned)s->kind >= CD_PRESSURE_KINDS || !s->minutes ||
      s->minutes > 10080 || s->starts > UINT64_MAX - s->minutes ||
      !cd_basis_valid(s->basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit bounded scenario pressure required");
  for (size_t i = 0; i < w->pressure_count; i++) {
    CdPressureView *v = &w->pressures[i].view;
    if (v->key == key) {
      if (v->kind != s->kind || v->target != s->target ||
          v->starts != s->starts || v->minutes != s->minutes ||
          v->value != s->value || strcmp(v->basis, s->basis))
        return cd_diag(d, CD_INVALID_ARGUMENT, "Pressure key conflict");
      *out = v->id;
      return cd_diag(d, CD_OK,
                     "Existing pressure returned without repeated disturbance");
    }
  }
  if (s->starts <= cd_now(w) || w->pressure_count == CD_CIVIC_LIMIT)
    return cd_diag(d, CD_CAPACITY,
                   "Pressure queue full or start is in the past");
  unsigned limit =
      s->kind == CD_PRESSURE_RAIN                                    ? 3
      : s->kind == CD_PRESSURE_FIRE || s->kind == CD_PRESSURE_DAMAGE ? 1000
      : s->kind == CD_PRESSURE_RATION                                ? 12
                                                                     : 1;
  if (!s->value || s->value > limit)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Pressure intensity outside technical bounds");
  if (s->kind == CD_PRESSURE_FIRE || s->kind == CD_PRESSURE_DAMAGE) {
    uint32_t b = cd_polygon_index(w->content, s->target);
    if (b == CD_NONE || w->content->polygons[b].kind != CD_BUILDING)
      return cd_diag(d, CD_UNKNOWN_REFERENCE, "Source building required");
  } else if (s->kind == CD_PRESSURE_EDGE_CLOSURE) {
    uint32_t e = cd_edge_index(w->content, s->target);
    if (e == CD_NONE || !w->content->edges[e].qualified)
      return cd_diag(d, CD_UNSUPPORTED,
                     "Disruption requires a qualified source walk link");
  } else if (s->target)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "This bounded pressure is city-wide");
  CdPressureView *v = &w->pressures[w->pressure_count++].view;
  *v = (CdPressureView){.id = w->pressure_count,
                        .key = key,
                        .kind = s->kind,
                        .target = s->target,
                        .starts = s->starts,
                        .minutes = s->minutes,
                        .value = s->value,
                        .state = CD_TASK_PENDING};
  snprintf(v->basis, 96, "%s", s->basis);
  *out = v->id;
  return cd_diag(d, CD_OK,
                 "Explicit experiment; no event inferred from the year 1940");
}
CdStatus cd_world_pressure_view(const CdWorld *w, uint64_t id,
                                CdPressureView *out, CdDiagnostic *d) {
  if (!w || !out || !id || id > w->pressure_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown scenario pressure");
  *out = w->pressures[id - 1].view;
  return cd_diag(d, CD_OK, "Scripted ordinary cause and phase");
}
CdStatus cd_world_pressure_cancel(CdWorld *w, uint64_t id, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !id || id > w->pressure_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown pressure");
  CdPressureView *v = &w->pressures[id - 1].view;
  if (cd_pending(v->state))
    v->state = CD_TASK_CANCELLED;
  return cd_diag(
      d, CD_OK,
      "Pressure overlay cancelled; already-caused fire/damage persists");
}
void cd_environment_tick(CdWorld *w) {
  w->rain = w->alert = w->blackout = w->ration_limit = 0;
  uint64_t now = cd_now(w);
  for (size_t i = 0; i < w->pressure_count; i++) {
    CdPressureView *v = &w->pressures[i].view;
    if (!cd_pending(v->state) || now < v->starts)
      continue;
    if (now - v->starts >= v->minutes) {
      v->state = CD_TASK_DONE;
      continue;
    }
    if (!v->applied) {
      v->applied = 1;
      if (v->kind == CD_PRESSURE_FIRE || v->kind == CD_PRESSURE_DAMAGE) {
        uint32_t b = cd_polygon_index(w->content, v->target);
        CdPremises *p = &w->premises[b];
        if (v->kind == CD_PRESSURE_FIRE) {
          if (p->fire < v->value)
            p->fire = v->value;
          snprintf(p->fire_basis, 96, "%s", v->basis);
        } else {
          unsigned loss = p->condition < v->value ? p->condition : v->value;
          p->condition -= loss;
          w->damage += loss;
        }
      }
    }
    if (v->kind == CD_PRESSURE_RAIN && w->rain < v->value)
      w->rain = v->value;
    if (v->kind == CD_PRESSURE_ALERT)
      w->alert = 1;
    if (v->kind == CD_PRESSURE_BLACKOUT)
      w->blackout = 1;
    if (v->kind == CD_PRESSURE_RATION &&
        (!w->ration_limit || w->ration_limit > v->value))
      w->ration_limit = v->value;
  }
  for (uint32_t b = 0; b < w->content->np; b++)
    if (w->premises[b].fire) {
      w->fire_minutes++;
      if (now % 10 == 0 && w->premises[b].condition) {
        w->premises[b].condition--;
        w->damage++;
      }
    }
}
CdStatus cd_world_water_import(CdWorld *w, CdEntityId id, unsigned units,
                               const char *basis, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      w->content->polygons[b].kind != CD_BUILDING || !units || units > 16384 ||
      w->premises[b].water > 16384 - units ||
      UINT64_MAX - w->water_imported < units || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit finite response water import required");
  w->premises[b].water += units;
  w->water_imported += units;
  snprintf(w->premises[b].water_basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Technical water units; no historical water network inferred");
}
CdStatus cd_world_shelter(CdWorld *w, CdEntityId id, unsigned cap,
                          const char *basis, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      cd_reviewed_door(w, id) == CD_NONE || !cd_usable(w, b) || !cap ||
      cap > cd_capacity(w, b) || !cd_basis_valid(basis))
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Shelter requires reviewed safe premises and explicit capacity");
  size_t assigned = 0;
  for (size_t i = 0; i < w->count; i++)
    assigned += w->people[i].refuge == id;
  if (assigned > cap)
    return cd_diag(d, CD_CAPACITY,
                   "Existing shelter assignments exceed proposed capacity");
  w->premises[b].shelter_capacity = cap;
  snprintf(w->premises[b].shelter_basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Technical shelter, not a historical wartime designation");
}
CdStatus cd_world_shelter_assign(CdWorld *w, CdPersonRef ref, CdEntityId id,
                                 const char *basis, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !p || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit shelter/evacuation intention required");
  if (!id) {
    if (p->refuge && p->presence == p->refuge)
      cd_world_visit(w, ref, p->refuge, 0, NULL);
    p->refuge = 0;
    return cd_diag(d, CD_OK,
                   "Shelter reservation released without moving a body");
  }
  uint32_t b = cd_polygon_index(w->content, id);
  if (b == CD_NONE || !w->premises[b].shelter_capacity || !cd_usable(w, b))
    return cd_diag(d, CD_UNSUPPORTED, "Safe configured shelter required");
  if (p->refuge == id)
    return cd_diag(d, CD_OK, "Existing shelter assignment returned");
  if (p->mission || p->vehicle || p->driving || p->gathering || p->refuge ||
      !p->civil.position_known)
    return cd_diag(
        d, CD_INVALID_STATE,
        "Resolve existing civic intentions and establish real position first");
  if (p->civil.age < 18) {
    CdCitizen *g = cd_citizen(w, p->civil.guardian);
    if (!g || g->refuge != id)
      return cd_diag(
          d, CD_INVALID_STATE,
          "Minor needs a guardian explicitly assigned to the same shelter");
  }
  size_t assigned = 0;
  for (size_t i = 0; i < w->count; i++)
    assigned += w->people[i].refuge == id;
  if (assigned >= w->premises[b].shelter_capacity)
    return cd_diag(d, CD_CAPACITY, "Shelter assignment capacity full");
  if (p->civil.dwelling != id) {
    CdStatus s = cd_world_visit(w, ref, id, 1, d);
    if (s != CD_OK)
      return s;
  }
  p->refuge = id;
  return cd_diag(
      d, CD_OK,
      "Shelter place reserved; actual source-street travel still required");
}
void cd_shelter_plan(CdWorld *w) {
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (p->refuge && p->activity == CD_ACTIVITY_IDLE) {
      uint32_t b = cd_polygon_index(w->content, p->refuge);
      if (cd_usable(w, b))
        cd_commute_to(w, p, cd_reviewed_door(w, p->refuge), 0, 0);
    }
  }
}
CdStatus cd_world_civic_summary(const CdWorld *w, CdCivicSummary *out,
                                CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing civic summary");
  *out = (CdCivicSummary){.rain = w->rain,
                          .alert = w->alert,
                          .blackout = w->blackout,
                          .ration_limit = w->ration_limit,
                          .water_imported = w->water_imported,
                          .water_used = w->water_used,
                          .water_exported = w->water_exported,
                          .fire_minutes = w->fire_minutes,
                          .damage = w->damage,
                          .offices = w->office_count,
                          .gatherings = w->gathering_count,
                          .rosters = w->roster_count,
                          .requests = w->service_count,
                          .vehicles = w->vehicle_count,
                          .pressures = w->pressure_count,
                          .service_outputs = w->service_outputs,
                          .attendance_minutes = w->attendance_minutes};
  for (size_t i = 0; i < w->count; i++)
    out->water_stock += w->people[i].water;
  for (size_t i = 0; i < w->life_count; i++)
    out->water_stock += w->life[i].water;
  for (uint32_t i = 0; i < w->content->np; i++)
    out->water_stock += w->premises[i].water;
  if (out->water_imported !=
      out->water_stock + out->water_used + out->water_exported)
    return cd_diag(d, CD_INVALID_STATE, "Response water conservation mismatch");
  return cd_diag(d, CD_OK,
                 "Ordinary civic state and finite response water reconcile");
}

CdStatus cd_snapshot_civic_summary(const CdSnapshot *s, CdCivicSummary *out,
                                   CdDiagnostic *d) {
  if (!s || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing civic snapshot");
  *out = s->civic;
  return cd_diag(d, CD_OK, "Immutable civic and scenario summary");
}
