#include "cd_city_internal.h"
#include "cd_native.h"
int cd_shift(unsigned start, unsigned end, unsigned minute) {
  return start == end || (start < end ? (minute >= start && minute < end)
                                      : (minute >= start || minute < end));
}
CdJob *cd_job(CdWorld *w, CdEntityId id) {
  return w && id && id <= w->job_count ? &w->jobs[id - 1] : NULL;
}
static unsigned minute(const CdWorld *w) {
  CdCivilTime t;
  cd_smoke_civil_time(cd_native_minutes(w->native) + w->origin_offset, &t,
                      NULL);
  return t.minute_of_day;
}
static int overlap(unsigned a, unsigned b, unsigned c, unsigned d) {
  for (unsigned m = 0; m < 1440; m++)
    if (cd_shift(a, b, m) && cd_shift(c, d, m))
      return 1;
  return 0;
}
int cd_job_qualified(const CdWorld *world, const CdJob *j, const CdCitizen *p) {
  if (!p || !j || !j->view.open || p->health.mortality_pending)
    return 0;
  CdOccupationView role;
  if (cd_occupation(j->view.occupation - 1, &role, NULL) != CD_OK ||
      !role.enabled)
    return 0;
  if (!j->view.apprenticeship)
    return p->civil.age >= role.minimum_age &&
           (p->skills & role.skills) == role.skills;
  if (p->civil.age < 16)
    return 0;
  CdCitizen *mentor = cd_citizen((CdWorld *)world, j->view.mentor);
  if (!mentor || mentor == p || mentor->civil.age < role.minimum_age ||
      mentor->health.mortality_pending ||
      (mentor->skills & role.skills) != role.skills)
    return 0;
  for (unsigned s = 0; s < 2; s++) {
    CdJob *mj = cd_job((CdWorld *)world, mentor->jobs[s]);
    if (mj && mj->view.open && mj->view.workplace == j->view.workplace &&
        !mj->view.apprenticeship && mj->view.occupation == j->view.occupation)
      return 1;
  }
  return 0;
}
int cd_employee(const CdWorld *w, const CdCitizen *p, uint32_t b) {
  for (unsigned s = 0; s < 2; s++) {
    CdJob *j = cd_job((CdWorld *)w, p->jobs[s]);
    if (j && j->view.open && j->view.workplace == w->content->polygons[b].id &&
        !w->premises[b].closed)
      return 1;
  }
  return 0;
}
static int institution_open(const CdWorld *w, const CdJob *j) {
  if (!j->view.institution)
    return 1;
  uint32_t i = cd_site_index(w->content, j->view.institution);
  return i != CD_NONE && w->institutions[i].known &&
         w->institutions[i].operating;
}
int cd_job_sleep(const CdWorld *w, const CdCitizen *p, unsigned m) {
  for (unsigned s = 0; s < 2; s++) {
    CdJob *j = cd_job((CdWorld *)w, p->jobs[s]);
    if (j && j->view.open && institution_open(w, j) &&
        !w->premises[cd_polygon_index(w->content, j->view.workplace)].closed &&
        cd_job_qualified(w, j, p) &&
        cd_shift(j->view.shift_start, j->view.shift_end, m))
      return 0;
  }
  for (unsigned s = 0; s < 2; s++) {
    CdJob *j = cd_job((CdWorld *)w, p->jobs[s]);
    if (j && j->view.open && j->view.shift_end < j->view.shift_start &&
        (m + 1440 - j->view.shift_end) % 1440 < 480)
      return 1;
  }
  return m < 360 || m >= 1320;
}
CdStatus cd_world_employer(CdWorld *w, CdEntityId id, const char *name,
                           unsigned capacity, const char *basis,
                           CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      w->content->polygons[b].kind != CD_BUILDING ||
      !w->content->polygons[b].capacity_known || !cd_basis_valid(name) ||
      !cd_basis_valid(basis) || !capacity || capacity > cd_capacity(w, b))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Employer requires known premises, bounded work capacity, "
                   "name and technical basis");
  size_t jobs = 0;
  for (size_t i = 0; i < w->job_count; i++)
    jobs += w->jobs[i].view.open && w->jobs[i].view.workplace == id;
  if (jobs > capacity)
    return cd_diag(d, CD_CAPACITY,
                   "Existing job slots exceed proposed workplace capacity");
  CdPremises *p = &w->premises[b];
  p->employer = 1;
  p->work_capacity = capacity;
  snprintf(p->employer_name, 96, "%s", name);
  snprintf(p->employer_basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Employer defined separately from property ownership and "
                 "institution identity");
}
CdStatus cd_world_employer_open(CdWorld *w, CdEntityId id, int open,
                                CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE || !w->premises[b].employer ||
      (open != 0 && open != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown employer/open state");
  w->premises[b].closed = !open;
  return cd_diag(d, CD_OK,
                 "Employer operation changed; assigned work waits without "
                 "losing progress");
}
CdStatus cd_world_job_create(CdWorld *w, const CdJobSpec *s, CdEntityId *out,
                             CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !cd_basis_valid(s->basis) ||
      !s->occupation || s->shift_start >= 1440 || s->shift_end >= 1440 ||
      s->wage > 1000 || (s->apprenticeship != 0 && s->apprenticeship != 1) ||
      (s->has_key != 0 && s->has_key != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid explicit job definition");
  uint32_t b = cd_polygon_index(w->content, s->workplace),
           door = cd_door_index(w->content, s->entrance);
  CdOccupationView role;
  if (b == CD_NONE || !w->premises[b].employer || door == CD_NONE ||
      w->content->doors[door].building != b ||
      !w->content->doors[door].reviewed ||
      cd_occupation(s->occupation - 1, &role, NULL) != CD_OK || !role.enabled)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Job requires enabled role, configured employer and "
                   "reviewed source entrance");
  if (s->institution) {
    uint32_t si = cd_site_index(w->content, s->institution);
    int linked = 0;
    if (si != CD_NONE) {
      CdInstitutionRuntime *institution = &w->institutions[si];
      for (size_t i = 0; i < institution->count; i++)
        linked |= institution->premises[i] == s->workplace;
      if (!institution->known || !institution->operating)
        linked = 0;
    }
    if (!linked)
      return cd_diag(
          d, CD_UNSUPPORTED,
          "Institution employer must be explicitly linked and operating");
  }
  if (w->job_count >= cd_smoke_capacity())
    return cd_diag(d, CD_CAPACITY, "Bounded job catalogue full");
  size_t slots = 0;
  for (size_t i = 0; i < w->job_count; i++)
    slots += w->jobs[i].view.open && w->jobs[i].view.workplace == s->workplace;
  if (slots >= w->premises[b].work_capacity)
    return cd_diag(d, CD_CAPACITY, "Workplace job capacity full");
  CdJob *j = &w->jobs[w->job_count];
  memset(j, 0, sizeof *j);
  j->view = (CdJobView){.id = w->job_count + 1,
                        .workplace = s->workplace,
                        .entrance = s->entrance,
                        .occupation = s->occupation,
                        .institution = s->institution,
                        .shift_start = s->shift_start,
                        .shift_end = s->shift_end,
                        .wage = s->wage,
                        .open = 1,
                        .apprenticeship = s->apprenticeship};
  j->has_key = s->has_key;
  snprintf(j->view.basis, 96, "%s", s->basis);
  w->job_count++;
  *out = j->view.id;
  return cd_diag(d, CD_OK,
                 "Bounded vacancy created; no person automatically assigned");
}
uint32_t cd_source_node(CdWorld *w, CdCitizen *p) {
  if (!p->civil.position_known)
    return CD_NONE;
  CdPoint pos = cd_city_position(w, p);
  if (p->inside != CD_NONE) {
    for (uint32_t i = 0; i < w->content->nd; i++) {
      CdDoor *d = &w->content->doors[i];
      CdPoint end = w->content->points[d->point + d->count - 1];
      if (d->reviewed && d->building == p->inside &&
          hypot(pos.x - end.x, pos.y - end.y) <= 1 / cd_city_scale(w))
        return d->node;
    }
    return CD_NONE;
  }
  if (p->anchor < w->content->nn) {
    CdPoint target = w->content->nodes[p->anchor].p;
    if (hypot(pos.x - target.x, pos.y - target.y) <= 1 / cd_city_scale(w))
      return p->anchor;
  }
  return CD_NONE;
}
CdStatus cd_world_job_assign(CdWorld *w, CdEntityId id, CdPersonRef ref,
                             CdPersonRef mentor, int secondary,
                             CdDiagnostic *d) {
  CdJob *j = cd_job(w, id);
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !j || !p || !j->view.open ||
      (secondary != 0 && secondary != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Unknown actor, vacancy or duty slot");
  if (j->view.worker.person_id) {
    if (j->view.worker.person_id == ref.person_id &&
        j->view.worker.world_token == ref.world_token &&
        p->jobs[secondary] == id)
      return cd_diag(d, CD_OK, "Assignment already published");
    return cd_diag(d, CD_CAPACITY, "Vacancy is occupied");
  }
  if (p->jobs[secondary])
    return cd_diag(d, CD_CAPACITY, "Primary/secondary duty slot occupied");
  CdJob candidate = *j;
  candidate.view.mentor = mentor;
  if (!cd_job_qualified(w, &candidate, p))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Age, skill or live mentor qualification not met");
  CdJob *other = cd_job(w, p->jobs[!secondary]);
  if (other && overlap(j->view.shift_start, j->view.shift_end,
                       other->view.shift_start, other->view.shift_end))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Primary and secondary shifts overlap");
  uint32_t b = cd_polygon_index(w->content, j->view.workplace),
           start = cd_source_node(w, p);
  if (start == CD_NONE)
    return cd_diag(d, CD_INVALID_STATE,
                   "Assign a real source position before employment");
  CdRoute route;
  uint32_t door = cd_door_index(w->content, j->view.entrance);
  if (cd_route_indices(w, start, w->content->doors[door].node, &route) != CD_OK)
    return cd_diag(d, CD_INVALID_STATE,
                   "Workplace is not reachable on currently qualified links");
  if (!cd_usable(w, b) || w->premises[b].closed)
    return cd_diag(d, CD_INVALID_STATE, "Employer premises unavailable");
  j->view.worker = ref;
  j->view.mentor = mentor;
  p->jobs[secondary] = id;
  return cd_diag(
      d, CD_OK,
      "Reachable qualified assignment committed; no position changed");
}
static void unassign_one(CdWorld *w, CdJob *j) {
  CdCitizen *p = cd_citizen(w, j->view.worker);
  if (p) {
    if (p->work.view.job == j->view.id)
      cd_work_release(w, p, 1);
    for (unsigned slot = 0; slot < 2; slot++)
      if (p->jobs[slot] == j->view.id)
        p->jobs[slot] = 0;
  }
  j->view.worker = (CdPersonRef){0};
  j->view.mentor = (CdPersonRef){0};
}
CdStatus cd_world_job_unassign(CdWorld *w, CdEntityId id, CdDiagnostic *d) {
  CdJob *j = cd_job(w, id);
  if (!cd_model_ready(w) || !j)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown job");
  unassign_one(w, j);
  for (size_t i = 0; i < w->job_count; i++) {
    CdJob *a = &w->jobs[i];
    if (a->view.apprenticeship && a->view.worker.person_id &&
        !cd_job_qualified(w, a, cd_citizen(w, a->view.worker)))
      unassign_one(w, a);
  }
  return cd_diag(d, CD_OK,
                 "Vacancy released; invalid mentor assignments cancelled once");
}
CdStatus cd_world_job_close(CdWorld *w, CdEntityId id, CdDiagnostic *d) {
  CdJob *j = cd_job(w, id);
  if (!cd_model_ready(w) || !j)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown job");
  cd_world_job_unassign(w, id, NULL);
  j->view.open = 0;
  return cd_diag(d, CD_OK, "Job closed without deleting its identity");
}
size_t cd_world_job_count(const CdWorld *w) { return w ? w->job_count : 0; }
CdStatus cd_world_job(const CdWorld *w, CdEntityId id, CdJobView *out,
                      CdDiagnostic *d) {
  CdJob *j = cd_job((CdWorld *)w, id);
  if (!j || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown job");
  *out = j->view;
  return cd_diag(d, CD_OK,
                 j->view.worker.person_id ? "Assigned job" : "Vacancy");
}
void cd_commute_to(CdWorld *w, CdCitizen *p, uint32_t target, int key,
                   int repair) {
  CdContent *c = w->content;
  CdDoor *destination = &c->doors[target];
  if (p->inside == destination->building)
    return;
  if (p->door != CD_NONE) {
    if (p->travel == CD_ROUTE_CANCELLED) {
      CdDoor *d = &c->doors[p->door];
      if (p->entering)
        cd_world_enter(w, p->civil.ref, d->id, key, repair, NULL);
      else
        cd_world_exit(w, p->civil.ref, d->id, key, repair, NULL);
    }
    return;
  }
  if (p->travel == CD_ROUTE_TRAVELLING || p->travel == CD_ROUTE_WAITING)
    return;
  if (p->inside != CD_NONE) {
    CdPoint pos = cd_city_position(w, p);
    for (uint32_t i = 0; i < c->nd; i++) {
      CdDoor *exit = &c->doors[i];
      CdPoint end = c->points[exit->point + exit->count - 1];
      if (exit->reviewed && exit->building == p->inside &&
          hypot(pos.x - end.x, pos.y - end.y) <= 1 / cd_city_scale(w)) {
        cd_world_exit(w, p->civil.ref, exit->id, key, repair, NULL);
        return;
      }
    }
    return;
  }
  if (cd_source_node(w, p) == destination->node)
    cd_world_enter(w, p->civil.ref, destination->id, key, repair, NULL);
  else
    cd_world_travel(w, p->civil.ref, c->nodes[destination->node].id, NULL);
}
void cd_jobs_plan(CdWorld *w) {
  unsigned m = minute(w);
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (cd_civic_busy(p) || p->activity != CD_ACTIVITY_IDLE ||
        p->food_remaining || p->feed_target.person_id ||
        p->care_target.person_id || p->cargo.remaining ||
        p->trade.view.state == CD_TASK_PENDING ||
        p->trade.view.state == CD_TASK_WAITING)
      continue;
    CdJob *chosen = NULL;
    for (unsigned s = 0; s < 2; s++) {
      CdJob *j = cd_job(w, p->jobs[s]);
      if (j && institution_open(w, j) && cd_job_qualified(w, j, p) &&
          cd_shift(j->view.shift_start, j->view.shift_end, m)) {
        chosen = j;
        break;
      }
    }
    if (chosen) {
      uint32_t b = cd_polygon_index(w->content, chosen->view.workplace);
      int repair = p->work.view.project &&
                   p->work.view.project == w->projects[b].view.id &&
                   w->projects[b].view.kind == CD_DEV_REPAIR &&
                   !w->premises[b].fire;
      if (w->premises[b].closed || (!cd_usable(w, b) && !repair))
        continue;
      cd_commute_to(w, p, cd_door_index(w->content, chosen->view.entrance),
                    chosen->has_key, repair);
    } else if ((p->jobs[0] || p->jobs[1]) && p->civil.dwelling) {
      uint32_t home = cd_polygon_index(w->content, p->civil.dwelling);
      if (p->inside == home)
        continue;
      for (uint32_t di = 0; di < w->content->nd; di++)
        if (w->content->doors[di].building == home &&
            w->content->doors[di].reviewed) {
          cd_commute_to(w, p, di, 1, 0);
          break;
        }
    }
  }
}
