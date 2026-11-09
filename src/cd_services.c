#include "cd_city_internal.h"
#include "cd_native.h"
static CdService *request(CdWorld *w, uint64_t id) {
  return w && id && id <= w->service_count ? &w->services[id - 1] : NULL;
}
static void finish(CdWorld *w, CdService *t, CdTaskState state,
                   const char *why) {
  CdJob *j = NULL;
  CdCitizen *p = NULL;
  unsigned caps;
  cd_provider(w, t->view.religious, t->view.provider, &j, &p, &caps);
  if (j) {
    CdPremises *b =
        &w->premises[cd_polygon_index(w->content, j->view.workplace)];
    b->escrow_money -= t->view.reserved_money;
    b->treasury += t->view.reserved_money;
  }
  t->view.reserved_money = 0;
  CdCitizen *old = cd_citizen(w, t->view.worker);
  if (old && old->mission == t->view.id) {
    if (t->care_owned)
      cd_care_release(w, old);
    if (state == CD_TASK_CANCELLED &&
        (old->travel == CD_ROUTE_TRAVELLING || old->travel == CD_ROUTE_WAITING))
      cd_world_stop(w, old->civil.ref, NULL);
    old->mission = 0;
  }
  t->care_owned = 0;
  t->view.state = state;
  snprintf(t->view.reason, 96, "%s", why);
  if (state == CD_TASK_DONE) {
    w->service_outputs++;
    if (t->view.religious)
      w->offices[t->view.provider - 1].view.outputs++;
    else
      w->rosters[t->view.provider - 1].view.outputs++;
  }
}
CdStatus cd_world_roster(CdWorld *w, const CdRosterSpec *s, CdEntityId *out,
                         CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !cd_basis_valid(s->title) ||
      !cd_basis_valid(s->basis) || !s->capabilities ||
      (s->capabilities &
       ~(((1u << CD_SERVICE_KINDS) - 1) & ~(1u << CD_SERVICE_WORSHIP))))
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Explicit ordinary service roster and capability mask required");
  CdJob *j = cd_job(w, s->job);
  if (!j || j->view.apprenticeship)
    return cd_diag(
        d, CD_UNSUPPORTED,
        "Service roster requires a regular configured job, not a map label");
  for (size_t i = 0; i < w->roster_count; i++)
    if (w->rosters[i].view.job == s->job)
      return cd_diag(d, CD_INVALID_STATE, "Job already has a civic roster");
  for (size_t i = 0; i < w->office_count; i++)
    if (w->offices[i].view.job == s->job)
      return cd_diag(
          d, CD_INVALID_STATE,
          "Religious office is not interchangeable with civic authority");
  if (w->roster_count == CD_CIVIC_LIMIT)
    return cd_diag(d, CD_CAPACITY, "Roster catalogue full");
  CdRoster *r = &w->rosters[w->roster_count++];
  r->view = (CdRosterView){
      .id = w->roster_count, .job = s->job, .capabilities = s->capabilities};
  snprintf(r->view.title, 96, "%s", s->title);
  snprintf(r->view.basis, 96, "%s", s->basis);
  *out = r->view.id;
  return cd_diag(d, CD_OK,
                 "Technical service roster; no historical organization or "
                 "staffing inferred");
}
CdStatus cd_world_roster_view(const CdWorld *w, CdEntityId id,
                              CdRosterView *out, CdDiagnostic *d) {
  if (!w || !id || id > w->roster_count || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown service roster");
  *out = w->rosters[id - 1].view;
  return cd_diag(d, CD_OK,
                 "Ordinary service capabilities and recorded outcomes");
}
CdStatus cd_world_dispatch(CdWorld *w, uint64_t key, const CdServiceSpec *s,
                           uint64_t *out, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !key ||
      (s->religious != 0 && s->religious != 1) ||
      (unsigned)s->kind >= CD_SERVICE_KINDS || !s->minutes ||
      s->minutes > 10080 || !cd_basis_valid(s->basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit bounded service request required");
  for (size_t i = 0; i < w->service_count; i++)
    if (w->services[i].view.key == key) {
      CdService *t = &w->services[i];
      CdServiceView *v = &t->view;
      if (v->provider != s->provider || v->religious != s->religious ||
          v->kind != s->kind || v->building != s->building ||
          v->subject.person_id != s->subject.person_id ||
          v->subject.world_token != s->subject.world_token ||
          v->minutes != s->minutes || t->deadline != s->deadline ||
          strcmp(v->basis, s->basis))
        return cd_diag(d, CD_INVALID_ARGUMENT, "Dispatch key conflict");
      *out = v->id;
      return cd_diag(d, CD_OK, "Existing dispatch result; no second charge");
    }
  CdJob *j = NULL;
  CdCitizen *p;
  unsigned caps;
  cd_provider(w, s->religious, s->provider, &j, &p, &caps);
  if (!j || !(caps & (1u << s->kind)) ||
      cd_reviewed_door(w, s->building) == CD_NONE)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Capability or reviewed service destination missing");
  if (s->deadline <= cd_now(w) || s->deadline - cd_now(w) > 10080 ||
      w->service_count == CD_DISPATCH_LIMIT)
    return cd_diag(d, CD_CAPACITY, "Dispatch deadline/queue bound exceeded");
  CdCitizen *subject = cd_citizen(w, s->subject);
  if (s->kind == CD_SERVICE_BURIAL) {
    int archived = 0;
    for (size_t i = 0; i < w->life_count; i++)
      archived |= w->life[i].reason == CD_DEATH &&
                  w->life[i].ref.person_id == s->subject.person_id &&
                  w->life[i].ref.world_token == s->subject.world_token;
    if (!archived)
      return cd_diag(d, CD_INVALID_ARGUMENT,
                     "Burial administration requires an actual archived death");
  } else if ((s->kind == CD_SERVICE_HEALTH || s->kind == CD_SERVICE_VISIT ||
              s->kind == CD_SERVICE_EDUCATION ||
              s->kind == CD_SERVICE_CHARITY || s->kind == CD_SERVICE_MEAL ||
              s->subject.person_id) &&
             !subject)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Live service subject required");
  uint32_t b = cd_polygon_index(w->content, j->view.workplace);
  uint64_t cost = (uint64_t)j->view.wage * s->minutes;
  if (w->premises[b].treasury < cost)
    return cd_diag(d, CD_CAPACITY, "Finite response wage budget unavailable");
  CdService *t = &w->services[w->service_count++];
  t->view = (CdServiceView){.id = w->service_count,
                            .key = key,
                            .state = CD_TASK_PENDING,
                            .provider = s->provider,
                            .religious = s->religious,
                            .kind = s->kind,
                            .building = s->building,
                            .subject = s->subject,
                            .minutes = s->minutes,
                            .reserved_money = cost};
  t->deadline = s->deadline;
  t->rate = j->view.wage;
  snprintf(t->view.basis, 96, "%s", s->basis);
  w->premises[b].treasury -= cost;
  w->premises[b].escrow_money += cost;
  *out = t->view.id;
  return cd_diag(d, CD_OK,
                 "Dispatch staged with finite wages; travel, stock and "
                 "qualifications remain required");
}
CdStatus cd_world_service(const CdWorld *w, uint64_t id, CdServiceView *out,
                          CdDiagnostic *d) {
  CdService *t = request((CdWorld *)w, id);
  if (!t || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown dispatch");
  *out = t->view;
  return cd_diag(d, CD_OK, out->reason);
}
CdStatus cd_world_service_cancel(CdWorld *w, uint64_t id, CdDiagnostic *d) {
  CdService *t = request(w, id);
  if (!cd_model_ready(w) || !t)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown dispatch");
  if (cd_pending(t->view.state))
    finish(w, t, CD_TASK_CANCELLED,
           "cancelled; unused wages returned, performed service retained");
  return cd_diag(d, CD_OK, "Idempotent dispatch cancellation");
}
static int on_shift(CdWorld *w, CdJob *j) {
  return cd_shift(j->view.shift_start, j->view.shift_end, cd_now(w) % 1440);
}
void cd_services_plan(CdWorld *w) {
  for (size_t i = 0; i < w->service_count; i++) {
    CdService *t = &w->services[i];
    CdServiceView *v = &t->view;
    if (!cd_pending(v->state))
      continue;
    if (cd_now(w) >= t->deadline) {
      finish(w, t, CD_TASK_CANCELLED,
             "deadline expired; unperformed wages refunded");
      continue;
    }
    CdJob *j;
    CdCitizen *p;
    unsigned caps;
    if (!cd_provider(w, v->religious, v->provider, &j, &p, &caps)) {
      CdCitizen *old = cd_citizen(w, v->worker);
      if (old && t->care_owned) {
        cd_care_release(w, old);
        t->care_owned = 0;
      }
      v->state = CD_TASK_WAITING;
      snprintf(
          v->reason, 96,
          "staff vacancy, lost qualification or closed employer/institution");
      continue;
    }
    if (v->worker.person_id && v->worker.person_id != p->civil.ref.person_id) {
      CdCitizen *old = cd_citizen(w, v->worker);
      if (old && old->mission == v->id) {
        if (t->care_owned)
          cd_care_release(w, old);
        old->mission = 0;
      }
      t->care_owned = 0;
    }
    if ((p->mission && p->mission != v->id) || p->gathering || p->vehicle ||
        p->driving || p->refuge)
      continue;
    p->mission = v->id;
    v->worker = p->civil.ref;
    if (!on_shift(w, j)) {
      if (t->care_owned) {
        cd_care_release(w, p);
        t->care_owned = 0;
      }
      continue;
    }
    uint32_t target = cd_reviewed_door(w, v->building);
    if (v->kind == CD_SERVICE_FIRE && (!p->water || !p->goods[CD_TOOL]))
      target = cd_door_index(w->content, j->view.entrance);
    if (p->activity == CD_ACTIVITY_IDLE)
      cd_commute_to(w, p, target, j->has_key, v->kind == CD_SERVICE_FIRE);
    if (v->kind == CD_SERVICE_HEALTH &&
        p->inside == w->content->doors[target].building) {
      CdCitizen *s = cd_citizen(w, v->subject);
      if (s && !p->care_target.person_id &&
          cd_care_begin(w, p->civil.ref, s->civil.ref, v->building,
                        s->health.condition == CD_HEALTHY ? 1 : 0,
                        NULL) == CD_OK) {
        t->care_start = s->health.care_minutes;
        t->care_owned = 1;
      }
    }
  }
}
void cd_services_tick(CdWorld *w) {
  for (size_t i = 0; i < w->service_count; i++) {
    CdService *t = &w->services[i];
    CdServiceView *v = &t->view;
    if (!cd_pending(v->state))
      continue;
    CdJob *j;
    CdCitizen *p;
    unsigned caps;
    if (!cd_provider(w, v->religious, v->provider, &j, &p, &caps) ||
        p->mission != v->id || !on_shift(w, j))
      continue;
    uint32_t b = cd_polygon_index(w->content, v->building),
             base = cd_polygon_index(w->content, j->view.workplace);
    CdCitizen *s = cd_citizen(w, v->subject);
    if (v->kind != CD_SERVICE_BURIAL && v->subject.person_id && !s) {
      finish(w, t, CD_TASK_CANCELLED,
             "subject retired; no action by stale reference");
      continue;
    }
    if (v->kind == CD_SERVICE_FIRE && (!p->water || !p->goods[CD_TOOL])) {
      if (p->inside != base || p->activity != CD_ACTIVITY_IDLE)
        continue;
      if (!p->goods[CD_TOOL] && w->premises[base].goods[CD_TOOL] &&
          cd_native_claim(w->native, p->civil.ref)) {
        w->premises[base].goods[CD_TOOL]--;
        p->goods[CD_TOOL]++;
        p->activity = CD_ACTIVITY_WORK;
      } else if (!p->water && w->premises[base].water &&
                 cd_native_claim(w->native, p->civil.ref)) {
        w->premises[base].water--;
        p->water++;
        p->activity = CD_ACTIVITY_WORK;
      }
      continue;
    }
    if (p->inside != b || p->door != CD_NONE || !p->civil.position_known ||
        (!cd_usable(w, b) && v->kind != CD_SERVICE_FIRE)) {
      CdCitizen *old = cd_citizen(w, v->worker);
      if (old && t->care_owned) {
        cd_care_release(w, old);
        t->care_owned = 0;
      }
      v->state = CD_TASK_WAITING;
      snprintf(v->reason, 96,
               "physical destination arrival or safe premises required");
      continue;
    }
    if (v->kind == CD_SERVICE_HEALTH) {
      if (!s || p->last_care_grant != cd_native_minutes(w->native) ||
          p->last_care_subject != s->civil.ref.person_id)
        continue;
    } else {
      if (p->activity != CD_ACTIVITY_IDLE || p->health.severity >= 600 ||
          (s && v->kind != CD_SERVICE_BURIAL && !cd_colocated(w, p, s)))
        continue;
      if (v->kind == CD_SERVICE_MEAL &&
          (!w->premises[b].meals || s->meals >= 12))
        continue;
      if (v->kind == CD_SERVICE_CHARITY &&
          (!w->premises[b].goods[CD_TEXTILE] || s->goods[CD_TEXTILE] >= 16))
        continue;
      if (v->kind == CD_SERVICE_FIRE && !w->premises[b].fire) {
        finish(w, t, CD_TASK_DONE,
               "fire no longer active; no further water/work consumed");
        continue;
      }
      if (!cd_native_claim(w->native, p->civil.ref))
        continue;
      p->activity = CD_ACTIVITY_WORK;
      if (v->kind == CD_SERVICE_MEAL) {
        w->premises[b].meals--;
        s->meals++;
      }
      if (v->kind == CD_SERVICE_CHARITY) {
        w->premises[b].goods[CD_TEXTILE]--;
        s->goods[CD_TEXTILE]++;
      }
      if (v->kind == CD_SERVICE_FIRE) {
        p->water--;
        w->water_used++;
        w->premises[b].fire =
            w->premises[b].fire > 20 ? w->premises[b].fire - 20 : 0;
      }
    }
    v->state = CD_TASK_PENDING;
    v->reason[0] = 0;
    v->progress++;
    v->paid += t->rate;
    v->reserved_money -= t->rate;
    w->premises[base].escrow_money -= t->rate;
    p->wallet += t->rate;
    w->economy.wages += t->rate;
    if (v->progress == v->minutes) {
      if (s)
        cd_native_remember_service(w->native, p->civil.ref, s->civil.ref);
      finish(w, t, CD_TASK_DONE,
             "ordinary service completed through native work; no inferred "
             "authority beyond roster");
    }
  }
}
void cd_civic_retire(CdWorld *w, CdCitizen *p, CdRetirement reason) {
  for (size_t i = 0; i < w->service_count; i++) {
    CdService *t = &w->services[i];
    if (!cd_pending(t->view.state))
      continue;
    if (t->view.subject.person_id == p->civil.ref.person_id &&
        t->view.kind != CD_SERVICE_BURIAL)
      finish(w, t, CD_TASK_CANCELLED, "live service subject retired");
    else if (t->view.worker.person_id == p->civil.ref.person_id) {
      p->mission = 0;
      t->care_owned = 0;
      t->view.worker = (CdPersonRef){0};
      t->view.state = CD_TASK_WAITING;
      snprintf(t->view.reason, 96, "staff lost; qualified successor required");
    }
  }
  for (size_t i = 0; i < w->vehicle_count; i++) {
    CdVehicle *v = &w->vehicles[i];
    if (v->view.passenger.person_id == p->civil.ref.person_id)
      v->view.passenger = (CdPersonRef){0};
    if (v->view.crew.person_id == p->civil.ref.person_id) {
      if (v->view.phase != CD_VEHICLE_STOPPED)
        v->resume_phase = v->view.phase;
      v->view.crew = (CdPersonRef){0};
      v->view.stopped = 1;
      v->view.phase = CD_VEHICLE_STOPPED;
    }
  }
  if (reason == CD_DEPARTURE)
    w->water_exported += p->water;
  p->water = 0;
}
