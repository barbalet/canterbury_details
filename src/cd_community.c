#include "cd_city_internal.h"
#include "cd_native.h"
static CdGathering *event(CdWorld *w, uint64_t id) {
  return w && id && id <= w->gathering_count ? &w->gatherings[id - 1] : NULL;
}
static int night_closed(const CdWorld *w) {
  unsigned m = (unsigned)(cd_now(w) % 1440);
  return w->blackout && (m < 360 || m >= 1320);
}
static void release(CdWorld *w, CdGathering *g) {
  CdOffice *o = &w->offices[g->view.office - 1];
  CdJob *j = cd_job(w, o->view.job);
  CdPremises *p = &w->premises[cd_polygon_index(w->content, j->view.workplace)];
  p->escrow_money -= g->view.reserved_money;
  p->treasury += g->view.reserved_money;
  g->view.reserved_money = 0;
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].gathering == g->view.id)
      w->people[i].gathering = 0;
}
CdStatus cd_world_gathering(CdWorld *w, uint64_t key, const CdGatheringSpec *s,
                            uint64_t *out, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !key || !s->office ||
      s->office > w->office_count || (unsigned)s->kind >= CD_SERVICE_KINDS ||
      !s->minutes || s->minutes > 10080 || !s->capacity ||
      s->capacity > CD_ATTENDANCE_LIMIT || !s->repeats || s->repeats > 366 ||
      (s->repeats > 1 && (!s->repeat_days || s->repeat_days > 366)) ||
      (s->members_only != 0 && s->members_only != 1) ||
      !cd_basis_valid(s->basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit bounded dated gathering required");
  for (size_t i = 0; i < w->gathering_count; i++)
    if (w->gatherings[i].view.key == key) {
      CdGatheringView *v = &w->gatherings[i].view;
      if (v->office != s->office || v->kind != s->kind ||
          v->starts != s->starts || v->minutes != s->minutes ||
          v->capacity != s->capacity || v->repeats != s->repeats ||
          v->repeat_days != s->repeat_days ||
          v->members_only != s->members_only || strcmp(v->basis, s->basis))
        return cd_diag(d, CD_INVALID_ARGUMENT, "Gathering key conflict");
      *out = v->id;
      return cd_diag(d, CD_OK, "Existing gathering returned");
    }
  if (w->gathering_count == CD_CIVIC_LIMIT || s->starts <= cd_now(w) ||
      (s->repeats > 1 && s->minutes > (uint64_t)s->repeat_days * 1440) ||
      s->starts > UINT64_MAX - (uint64_t)s->repeat_days * 1440 * s->repeats -
                      s->minutes)
    return cd_diag(d, CD_CAPACITY,
                   "Calendar full or start/end outside safe future range");
  CdOffice *o = &w->offices[s->office - 1];
  CdJob *j = cd_job(w, o->view.job);
  if (!(o->view.capabilities & (1u << s->kind)) ||
      s->kind == CD_SERVICE_HEALTH || s->kind == CD_SERVICE_BURIAL ||
      s->kind == CD_SERVICE_FIRE || s->kind == CD_SERVICE_POLICE)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Office/tradition does not support this gathering; use "
                   "individual dispatch for clinical/burial response");
  uint32_t b = cd_polygon_index(w->content, j->view.workplace);
  if (s->capacity + 1 > cd_capacity(w, b))
    return cd_diag(d, CD_CAPACITY,
                   "Gathering plus staff exceeds premises capacity");
  uint64_t cost = (uint64_t)j->view.wage * s->minutes * s->repeats;
  if (w->premises[b].treasury < cost)
    return cd_diag(d, CD_CAPACITY, "Finite gathering wage budget unavailable");
  CdGathering *g = &w->gatherings[w->gathering_count++];
  g->view = (CdGatheringView){.id = w->gathering_count,
                              .key = key,
                              .state = CD_TASK_PENDING,
                              .office = s->office,
                              .kind = s->kind,
                              .starts = s->starts,
                              .minutes = s->minutes,
                              .capacity = s->capacity,
                              .repeat_days = s->repeat_days,
                              .repeats = s->repeats,
                              .members_only = s->members_only,
                              .reserved_money = cost};
  g->rate = j->view.wage;
  snprintf(g->view.basis, 96, "%s", s->basis);
  w->premises[b].treasury -= cost;
  w->premises[b].escrow_money += cost;
  *out = g->view.id;
  return cd_diag(d, CD_OK,
                 "Explicit simulation calendar; no holiday, practice or "
                 "membership inferred");
}
CdStatus cd_world_gathering_view(const CdWorld *w, uint64_t id,
                                 CdGatheringView *out, CdDiagnostic *d) {
  CdGathering *g = event((CdWorld *)w, id);
  if (!g || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown gathering");
  *out = g->view;
  return cd_diag(d, CD_OK, out->reason);
}
CdStatus cd_world_attend(CdWorld *w, uint64_t id, CdPersonRef ref, int attend,
                         CdDiagnostic *d) {
  CdGathering *g = event(w, id);
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !g || !p || (attend != 0 && attend != 1) ||
      !cd_pending(g->view.state))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Live explicit attendee and active schedule required");
  for (size_t i = 0; i < g->count; i++)
    if (g->attendees[i].person_id == ref.person_id) {
      if (!attend) {
        memmove(g->attendees + i, g->attendees + i + 1,
                (g->count - i - 1) * sizeof *g->attendees);
        memmove(g->credited + i, g->credited + i + 1,
                (g->count - i - 1) * sizeof *g->credited);
        g->count--;
        if (p->gathering == id)
          p->gathering = 0;
      }
      return cd_diag(d, CD_OK, "Attendance intention updated once");
    }
  if (!attend)
    return cd_diag(d, CD_OK, "Attendance already absent");
  if (g->count == g->view.capacity)
    return cd_diag(d, CD_CAPACITY, "Explicit attendee list full");
  int member;
  cd_world_member(w, ref, w->offices[g->view.office - 1].view.institution,
                  &member, NULL);
  if (g->view.members_only && !member)
    return cd_diag(
        d, CD_INVALID_STATE,
        "Explicit community membership required by this scenario event");
  if (p->mission || p->vehicle || p->driving || p->gathering || p->refuge)
    return cd_diag(d, CD_INVALID_STATE,
                   "Resident has another civic/transport intention");
  g->attendees[g->count] = ref;
  g->credited[g->count++] = 0;
  return cd_diag(
      d, CD_OK,
      "Attendance registered, without inferring faith or moving a body");
}
CdStatus cd_world_gathering_cancel(CdWorld *w, uint64_t id, CdDiagnostic *d) {
  CdGathering *g = event(w, id);
  if (!cd_model_ready(w) || !g)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown gathering");
  if (cd_pending(g->view.state)) {
    release(w, g);
    g->view.state = CD_TASK_CANCELLED;
    snprintf(g->view.reason, 96,
             "cancelled; unused wages returned, performed attendance retained");
  }
  return cd_diag(d, CD_OK, "Idempotent gathering cancellation");
}
void cd_community_plan(CdWorld *w) {
  uint64_t now = cd_now(w);
  for (size_t i = 0; i < w->gathering_count; i++) {
    CdGathering *g = &w->gatherings[i];
    CdGatheringView *v = &g->view;
    if (!cd_pending(v->state))
      continue;
    uint64_t start =
        v->starts + (uint64_t)v->occurrence * v->repeat_days * 1440;
    if (now >= start + v->minutes) {
      if (v->progress == v->minutes)
        v->completed++;
      v->occurrence++;
      v->progress = 0;
      if (v->occurrence == v->repeats) {
        release(w, g);
        v->state = CD_TASK_DONE;
        snprintf(v->reason, 96, "calendar window ended; completed %u/%u",
                 v->completed, v->repeats);
        continue;
      }
      for (size_t k = 0; k < w->count; k++)
        if (w->people[k].gathering == v->id)
          w->people[k].gathering = 0;
      start = v->starts + (uint64_t)v->occurrence * v->repeat_days * 1440;
    }
    if (now < start || night_closed(w))
      continue;
    CdJob *j;
    CdCitizen *staff;
    unsigned caps;
    if (!cd_provider(w, 1, v->office, &j, &staff, &caps)) {
      v->state = CD_TASK_WAITING;
      snprintf(v->reason, 96, "qualified office vacant or institution closed");
      continue;
    }
    uint32_t b = cd_polygon_index(w->content, j->view.workplace),
             door = cd_door_index(w->content, j->view.entrance);
    if (!cd_usable(w, b))
      continue;
    if (!staff->mission && !staff->vehicle && !staff->driving &&
        (!staff->gathering || staff->gathering == v->id)) {
      staff->gathering = v->id;
      if (staff->activity == CD_ACTIVITY_IDLE)
        cd_commute_to(w, staff, door, j->has_key, 0);
    }
    for (size_t k = 0; k < g->count; k++) {
      CdCitizen *p = cd_citizen(w, g->attendees[k]);
      if (!p || p == staff || p->mission || p->vehicle || p->driving ||
          p->refuge || (p->gathering && p->gathering != v->id))
        continue;
      int member = 0;
      cd_world_member(w, p->civil.ref,
                      w->offices[v->office - 1].view.institution, &member,
                      NULL);
      if (v->members_only && !member)
        continue;
      p->gathering = v->id;
      if (p->activity == CD_ACTIVITY_IDLE)
        cd_commute_to(w, p, door, 0, 0);
    }
  }
}
void cd_community_tick(CdWorld *w) {
  uint64_t now = cd_now(w);
  for (size_t i = 0; i < w->gathering_count; i++) {
    CdGathering *g = &w->gatherings[i];
    CdGatheringView *v = &g->view;
    uint64_t start =
        v->starts + (uint64_t)v->occurrence * v->repeat_days * 1440;
    v->present = 0;
    if (!cd_pending(v->state) || now < start || now >= start + v->minutes ||
        night_closed(w))
      continue;
    CdJob *j;
    CdCitizen *staff;
    unsigned caps;
    if (!cd_provider(w, 1, v->office, &j, &staff, &caps))
      continue;
    uint32_t b = cd_polygon_index(w->content, j->view.workplace);
    if (staff->gathering != v->id || staff->inside != b ||
        staff->door != CD_NONE || staff->activity != CD_ACTIVITY_IDLE ||
        !cd_usable(w, b) ||
        !cd_shift(j->view.shift_start, j->view.shift_end, now % 1440) ||
        !cd_native_claim(w->native, staff->civil.ref))
      continue;
    staff->activity = CD_ACTIVITY_WORK;
    staff->wallet += g->rate;
    w->economy.wages += g->rate;
    w->premises[b].escrow_money -= g->rate;
    v->reserved_money -= g->rate;
    v->paid += g->rate;
    v->progress++;
    v->state = CD_TASK_PENDING;
    v->reason[0] = 0;
    w->offices[v->office - 1].view.outputs++;
    w->service_outputs++;
    for (size_t k = 0; k < g->count; k++) {
      CdCitizen *p = cd_citizen(w, g->attendees[k]);
      if (!p || p == staff || p->gathering != v->id || p->inside != b ||
          !cd_colocated(w, staff, p) || p->activity != CD_ACTIVITY_IDLE)
        continue;
      int member = 0;
      cd_world_member(w, p->civil.ref,
                      w->offices[v->office - 1].view.institution, &member,
                      NULL);
      if (v->members_only && !member)
        continue;
      if (v->kind == CD_SERVICE_MEAL && g->credited[k] != v->occurrence + 1 &&
          (!w->premises[b].meals || p->meals >= 12))
        continue;
      if (v->kind == CD_SERVICE_CHARITY &&
          g->credited[k] != v->occurrence + 1 &&
          (!w->premises[b].goods[CD_TEXTILE] || p->goods[CD_TEXTILE] >= 16))
        continue;
      if (!cd_native_claim(w->native, p->civil.ref))
        continue;
      p->activity = CD_ACTIVITY_CARE;
      v->present++;
      w->attendance_minutes++;
      if (g->credited[k] != v->occurrence + 1) {
        if (v->kind == CD_SERVICE_MEAL) {
          w->premises[b].meals--;
          p->meals++;
        }
        if (v->kind == CD_SERVICE_CHARITY) {
          w->premises[b].goods[CD_TEXTILE]--;
          p->goods[CD_TEXTILE]++;
        }
        g->credited[k] = v->occurrence + 1;
      }
    }
  }
}
