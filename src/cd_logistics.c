#include "cd_city_internal.h"
#include "cd_native.h"
static CdVehicle *vehicle(CdWorld *w, CdEntityId id) {
  return w && id && id <= w->vehicle_count ? &w->vehicles[id - 1] : NULL;
}
static int qualified(CdWorld *w, CdVehicle *v, CdCitizen *p) {
  CdJob *j = cd_job(w, v->view.job);
  return p && j && j->view.worker.person_id == p->civil.ref.person_id &&
         cd_job_qualified(w, j, p) && (p->credentials & CD_AUTH_CARRIER) &&
         p->civil.position_known && !p->health.mortality_pending &&
         p->health.severity < 600 &&
         (v->view.kind != CD_STRETCHER || (p->skills & CD_SKILL_CARE));
}
CdStatus cd_world_vehicle(CdWorld *w, const CdVehicleSpec *s, CdEntityId *out,
                          CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !cd_basis_valid(s->basis) ||
      (unsigned)s->kind > CD_RAIL_VEHICLE)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit bounded transport definition required");
  if (s->kind == CD_RAIL_VEHICLE)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Rail geometry is reference only; no qualified operational "
                   "equipment/service supplied");
  if ((s->kind == CD_HANDCART &&
       (!s->cargo_capacity || s->cargo_capacity > 16)) ||
      (s->kind == CD_STRETCHER && s->cargo_capacity))
    return cd_diag(d, CD_CAPACITY,
                   "Technical handcart carries 1..16 cargo units; stretcher "
                   "carries one person only");
  CdJob *j = cd_job(w, s->job);
  if (!j || j->view.apprenticeship)
    return cd_diag(d, CD_UNSUPPORTED, "Transport needs a regular crew job");
  for (size_t i = 0; i < w->vehicle_count; i++)
    if (w->vehicles[i].view.job == s->job)
      return cd_diag(d, CD_INVALID_STATE,
                     "Job already provides a unique vehicle crew");
  if (w->vehicle_count == CD_VEHICLE_LIMIT)
    return cd_diag(d, CD_CAPACITY, "Vehicle catalogue full");
  CdVehicle *v = &w->vehicles[w->vehicle_count++];
  v->view = (CdVehicleView){.id = w->vehicle_count,
                            .job = s->job,
                            .crew = j->view.worker,
                            .kind = s->kind,
                            .capacity = s->cargo_capacity};
  snprintf(v->view.basis, 96, "%s", s->basis);
  CdCitizen *p = cd_citizen(w, j->view.worker);
  if (p && p->civil.position_known) {
    v->view.position_known = 1;
    v->view.position = cd_city_position(w, p);
  }
  *out = v->view.id;
  return cd_diag(
      d, CD_OK,
      "Explicit technical hand-carried vehicle; no historical fleet inferred");
}
CdStatus cd_world_transport(CdWorld *w, CdEntityId id, uint64_t key,
                            CdEntityId source, CdEntityId destination,
                            unsigned good, int meals, unsigned units,
                            CdPersonRef passenger, CdDiagnostic *d) {
  CdVehicle *v = vehicle(w, id);
  if (!cd_model_ready(w) || !v || !key || good >= CD_GOODS ||
      (meals != 0 && meals != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid transport request");
  CdVehicleView *r = &v->view;
  if (key == r->key) {
    if (r->source != source || r->destination != destination ||
        r->good != good || r->meals != meals || r->requested != units ||
        v->requested_passenger.person_id != passenger.person_id ||
        v->requested_passenger.world_token != passenger.world_token)
      return cd_diag(d, CD_INVALID_ARGUMENT, "Transport key conflict");
    return cd_diag(d, CD_OK,
                   "Existing transport result; custody not duplicated");
  }
  if (key < r->key ||
      (r->phase != CD_VEHICLE_IDLE && r->phase != CD_VEHICLE_DONE))
    return cd_diag(d, CD_INVALID_STATE,
                   "Stale key or outstanding vehicle load");
  CdCitizen *crew = cd_citizen(w, r->crew), *p = cd_citizen(w, passenger);
  uint32_t a = cd_reviewed_door(w, source),
           b = cd_reviewed_door(w, destination);
  if (a == CD_NONE || b == CD_NONE || !qualified(w, v, crew) || crew->mission ||
      crew->gathering || crew->refuge || crew->vehicle || crew->driving ||
      crew->inside != w->content->doors[a].building || !crew->goods[CD_TOOL])
    return cd_diag(d, CD_INVALID_STATE,
                   "Qualified equipped crew must physically reach reviewed "
                   "loading premises");
  if (r->position_known &&
      hypot(cd_city_position(w, crew).x - r->position.x,
            cd_city_position(w, crew).y - r->position.y) > 1 / cd_city_scale(w))
    return cd_diag(d, CD_INVALID_STATE,
                   "Crew must physically retrieve the existing vehicle; new "
                   "trip cannot relocate it");
  if ((r->kind == CD_HANDCART &&
       (!units || units > r->capacity || passenger.person_id)) ||
      (r->kind == CD_STRETCHER &&
       (units || meals || !p || p == crew || p->vehicle || p->driving ||
        p->mission || p->gathering || p->refuge || p->door != CD_NONE ||
        p->held_edge != CD_NONE || !cd_colocated(w, crew, p))))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Unique load/passenger, physical boarding and vehicle "
                   "capacities required");
  CdRoute route;
  if (cd_route_indices(w, w->content->doors[a].node, w->content->doors[b].node,
                       &route) != CD_OK)
    return cd_diag(
        d, CD_INVALID_STATE,
        "Transport destination not reachable through qualified source links");
  r->key = key;
  r->source = source;
  r->destination = destination;
  r->good = good;
  r->meals = meals;
  r->requested = units;
  r->loaded = r->delivered = 0;
  r->passenger = (CdPersonRef){0};
  r->phase = CD_VEHICLE_LOADING;
  r->stopped = 0;
  r->reason[0] = 0;
  v->requested_passenger = passenger;
  v->destination_door = b;
  v->unload_building = destination;
  v->aborted = 0;
  if (p)
    p->vehicle = id;
  crew->driving = id;
  return cd_diag(d, CD_OK,
                 "Loading intention published; no cargo/passenger moved before "
                 "a shared crew grant");
}
CdStatus cd_world_vehicle_view(const CdWorld *w, CdEntityId id,
                               CdVehicleView *out, CdDiagnostic *d) {
  CdVehicle *v = vehicle((CdWorld *)w, id);
  if (!v || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown vehicle");
  *out = v->view;
  return cd_diag(d, CD_OK, out->reason);
}
CdStatus cd_world_vehicle_crew(CdWorld *w, CdEntityId id, CdPersonRef ref,
                               CdDiagnostic *d) {
  CdVehicle *v = vehicle(w, id);
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !v || !qualified(w, v, p) || p->vehicle ||
      p->mission || p->gathering || p->refuge ||
      (p->driving && p->driving != id) || !p->goods[CD_TOOL])
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Live qualified equipped replacement crew required");
  if (v->view.position_known &&
      hypot(cd_city_position(w, p).x - v->view.position.x,
            cd_city_position(w, p).y - v->view.position.y) >
          1 / cd_city_scale(w))
    return cd_diag(d, CD_INVALID_STATE,
                   "Replacement must physically reach the stationary vehicle");
  CdCitizen *old = cd_citizen(w, v->view.crew);
  if (old && old != p && old->driving == id)
    old->driving = 0;
  v->view.crew = ref;
  if (v->view.phase != CD_VEHICLE_IDLE && v->view.phase != CD_VEHICLE_DONE)
    p->driving = id;
  return cd_diag(d, CD_OK,
                 "Crew rebound at actual vehicle position; cargo remains in "
                 "unique custody");
}
CdStatus cd_world_vehicle_stop(CdWorld *w, CdEntityId id, int stop,
                               CdDiagnostic *d) {
  CdVehicle *v = vehicle(w, id);
  if (!cd_model_ready(w) || !v || (stop != 0 && stop != 1) ||
      v->view.phase == CD_VEHICLE_IDLE || v->view.phase == CD_VEHICLE_DONE)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Active transport and explicit stop/resume required");
  if (stop && !v->view.stopped) {
    v->resume_phase = v->view.phase;
    v->view.phase = CD_VEHICLE_STOPPED;
    v->view.stopped = 1;
    CdCitizen *p = cd_citizen(w, v->view.crew);
    if (p)
      cd_world_stop(w, p->civil.ref, NULL);
  } else if (!stop && v->view.stopped) {
    v->view.stopped = 0;
    v->view.phase = v->resume_phase ? v->resume_phase : CD_VEHICLE_MOVING;
  }
  return cd_diag(d, CD_OK, "Stop/resume retains physical position and custody");
}
CdStatus cd_world_vehicle_unload_here(CdWorld *w, CdEntityId id,
                                      CdDiagnostic *d) {
  CdVehicle *v = vehicle(w, id);
  CdCitizen *p = v ? cd_citizen(w, v->view.crew) : NULL;
  if (cd_model_ready(w) && v && v->view.phase == CD_VEHICLE_DONE)
    return cd_diag(d, CD_OK, "Vehicle already unloaded");
  if (!cd_model_ready(w) || !v || !qualified(w, v, p) || p->inside == CD_NONE ||
      p->door != CD_NONE ||
      cd_reviewed_door(w, w->content->polygons[p->inside].id) == CD_NONE)
    return cd_diag(d, CD_INVALID_STATE,
                   "Cancellation can unload only at a physically reached "
                   "reviewed endpoint");
  v->aborted = 1;
  v->view.stopped = 0;
  v->unload_building = w->content->polygons[p->inside].id;
  v->view.phase = CD_VEHICLE_UNLOADING;
  return cd_diag(
      d, CD_OK,
      "Cancellation unloads locally through grants; no remote refund/teleport");
}
unsigned cd_vehicle_extra(const CdWorld *w, const CdCitizen *p, uint32_t b) {
  if (!p->driving || p->driving > w->vehicle_count)
    return 0;
  const CdVehicle *v = &w->vehicles[p->driving - 1];
  CdCitizen *s = cd_citizen((CdWorld *)w, v->view.passenger);
  if (!s || s->civil.dwelling == w->content->polygons[b].id ||
      s->presence == w->content->polygons[b].id ||
      cd_housing_transient(w, s, b))
    return 0;
  return 1;
}
void cd_logistics_plan(CdWorld *w) {
  for (size_t i = 0; i < w->vehicle_count; i++) {
    CdVehicle *v = &w->vehicles[i];
    CdVehicleView *r = &v->view;
    CdCitizen *p = cd_citizen(w, r->crew);
    if (r->phase == CD_VEHICLE_IDLE || r->phase == CD_VEHICLE_DONE)
      continue;
    CdCitizen *s = cd_citizen(w, r->passenger);
    if (s) {
      s->activity = CD_ACTIVITY_TRAVEL;
      cd_native_block(w->native, s->civil.ref, 0);
    }
    if (!qualified(w, v, p) || !p->goods[CD_TOOL]) {
      if (p && p->activity == CD_ACTIVITY_IDLE)
        p->activity = CD_ACTIVITY_WAIT;
      r->stopped = 1;
      if (r->phase != CD_VEHICLE_STOPPED)
        v->resume_phase = r->phase;
      r->phase = CD_VEHICLE_STOPPED;
      snprintf(r->reason, 96,
               "valid crew/equipment missing; vehicle and custody remain "
               "stationary");
      continue;
    }
    p->driving = r->id;
    if (r->stopped) {
      p->activity = CD_ACTIVITY_WAIT;
      continue;
    }
    CdJob *j = cd_job(w, r->job);
    uint32_t base = cd_polygon_index(w->content, j->view.workplace);
    if (w->premises[base].closed ||
        !cd_shift(j->view.shift_start, j->view.shift_end, cd_now(w) % 1440)) {
      p->activity = CD_ACTIVITY_WAIT;
      continue;
    }
    if (r->phase == CD_VEHICLE_MOVING && p->activity == CD_ACTIVITY_IDLE)
      cd_commute_to(w, p, v->destination_door, j->has_key,
                    r->kind == CD_STRETCHER);
  }
}
void cd_logistics_tick(CdWorld *w) {
  for (size_t i = 0; i < w->vehicle_count; i++) {
    CdVehicle *v = &w->vehicles[i];
    CdVehicleView *r = &v->view;
    CdCitizen *p = cd_citizen(w, r->crew);
    if (!p || r->phase == CD_VEHICLE_IDLE || r->phase == CD_VEHICLE_DONE)
      continue;
    if (r->stopped || !qualified(w, v, p))
      continue;
    r->position = cd_city_position(w, p);
    r->position_known = p->civil.position_known;
    CdCitizen *s = cd_citizen(w, r->passenger);
    if (s) {
      uint16_t x, y;
      cd_native_position(w->native, p->civil.ref, &x, &y);
      cd_native_write(w->native, s->civil.ref, x, y);
      s->inside = p->inside;
      s->anchor = p->anchor;
      s->door = p->door;
      s->entering = p->entering;
      s->travel = p->travel;
    }
    if (r->stopped || !qualified(w, v, p) || p->activity != CD_ACTIVITY_IDLE)
      continue;
    CdJob *j = cd_job(w, r->job);
    if (w->premises[cd_polygon_index(w->content, j->view.workplace)].closed ||
        !cd_shift(j->view.shift_start, j->view.shift_end, cd_now(w) % 1440))
      continue;
    if (r->phase == CD_VEHICLE_LOADING) {
      uint32_t b = cd_polygon_index(w->content, r->source);
      if (p->inside != b || !cd_usable(w, b))
        continue;
      if (r->kind == CD_STRETCHER) {
        s = cd_citizen(w, v->requested_passenger);
        if (!s || s->vehicle != r->id || !cd_colocated(w, p, s) ||
            !cd_native_claim(w->native, p->civil.ref))
          continue;
        s->vehicle = r->id;
        r->passenger = s->civil.ref;
        s->activity = CD_ACTIVITY_TRAVEL;
        cd_native_block(w->native, s->civil.ref, 0);
        p->activity = CD_ACTIVITY_WORK;
        r->phase = CD_VEHICLE_MOVING;
      } else {
        if (r->loaded < r->requested) {
          CdPremises *site = &w->premises[b];
          if ((r->meals ? !site->meals : !site->goods[r->good]) ||
              !cd_native_claim(w->native, p->civil.ref))
            continue;
          if (r->meals)
            site->meals--;
          else
            site->goods[r->good]--;
          r->loaded++;
          p->activity = CD_ACTIVITY_WORK;
        }
        if (r->loaded == r->requested)
          r->phase = CD_VEHICLE_MOVING;
      }
    } else if (r->phase == CD_VEHICLE_MOVING) {
      uint32_t b = cd_polygon_index(w->content, r->destination);
      if (p->inside == b && p->door == CD_NONE)
        r->phase = CD_VEHICLE_UNLOADING;
    } else if (r->phase == CD_VEHICLE_UNLOADING) {
      uint32_t b = cd_polygon_index(w->content, v->unload_building);
      if (p->inside != b || p->door != CD_NONE)
        continue;
      CdPremises *site = &w->premises[b];
      if (r->loaded) {
        if ((r->meals ? (uint64_t)site->meals + site->reserved_meals >= 4096
                      : site->goods[r->good] + site->reserved[r->good] >=
                            UINT64_C(1000000000)) ||
            !cd_native_claim(w->native, p->civil.ref))
          continue;
        if (r->meals)
          site->meals++;
        else
          site->goods[r->good]++;
        r->loaded--;
        r->delivered++;
        p->activity = CD_ACTIVITY_WORK;
      } else if (r->passenger.person_id) {
        if (!cd_native_claim(w->native, p->civil.ref))
          continue;
        if (s) {
          s->vehicle = 0;
          s->door = CD_NONE;
          s->travel = CD_ROUTE_ARRIVED;
        }
        r->passenger = (CdPersonRef){0};
        p->activity = CD_ACTIVITY_WORK;
      }
      if (!r->loaded && !r->passenger.person_id) {
        r->phase = CD_VEHICLE_DONE;
        CdCitizen *booked = cd_citizen(w, v->requested_passenger);
        if (booked && booked->vehicle == r->id)
          booked->vehicle = 0;
        p->driving = 0;
        snprintf(r->reason, 96, "%s",
                 v->aborted ? "cancelled trip unloaded at actual endpoint"
                            : "delivery completed at actual endpoint");
      }
    }
  }
}
