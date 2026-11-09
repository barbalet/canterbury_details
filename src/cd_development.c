#include "cd_city_internal.h"
unsigned cd_capacity(const CdWorld *w, uint32_t b) {
  return w->premises[b].capacity_changed ? w->premises[b].capacity_override
                                         : w->content->polygons[b].capacity;
}
int cd_usable(const CdWorld *w, uint32_t b) {
  return b < w->content->np && w->premises[b].condition > 0 &&
         !w->premises[b].fire;
}
CdStatus cd_world_premises(const CdWorld *w, CdEntityId id, CdPremisesView *out,
                           CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!w || !out || b == CD_NONE || w->content->polygons[b].kind != CD_BUILDING)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown premises");
  CdPremises *p = &w->premises[b];
  memset(out, 0, sizeof *out);
  out->building = id;
  out->capacity = cd_capacity(w, b);
  out->capacity_known = w->content->polygons[b].capacity_known;
  out->condition = p->condition;
  out->work_capacity = p->work_capacity;
  out->scenario_override = p->capacity_changed || p->scenario_use[0];
  out->employer_open = p->employer && !p->closed;
  snprintf(out->use, 96, "%s",
           p->scenario_use[0] ? p->scenario_use : w->content->polygons[b].use);
  snprintf(out->employer, 96, "%s", p->employer_name);
  return cd_diag(d, CD_OK,
                 "Runtime scenario premises overlay; source geometry and "
                 "identity unchanged");
}
CdStatus cd_world_wear(CdWorld *w, CdEntityId id, unsigned amount,
                       const char *basis, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      w->content->polygons[b].kind != CD_BUILDING || !amount || amount > 1000 ||
      !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Wear requires explicit bounded technical input");
  w->premises[b].condition =
      w->premises[b].condition > amount ? w->premises[b].condition - amount : 0;
  return cd_diag(d, CD_OK,
                 "Technical condition changed; zero condition blocks ordinary "
                 "premises work/access");
}
static int active(CdTaskState s) {
  return s == CD_TASK_PENDING || s == CD_TASK_WAITING;
}
CdStatus cd_world_project(CdWorld *w, CdEntityId id, uint64_t key,
                          const CdDevelopmentSpec *s, uint64_t *out,
                          CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE || !s || !out || !key || !s->minutes ||
      s->minutes > 10080 || !cd_basis_valid(s->basis) ||
      (unsigned)s->kind > CD_DEV_GEOMETRY)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Invalid bounded development proposal");
  if (s->use && *s->use && !cd_basis_valid(s->use))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid bounded use label");
  if (s->kind == CD_DEV_GEOMETRY)
    return cd_diag(d, CD_UNSUPPORTED,
                   "New geometry requires a new reviewed content generation, "
                   "not a runtime footprint rewrite");
  CdPolygon *base = &w->content->polygons[b];
  if (base->kind != CD_BUILDING || !base->capacity_known)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Project requires reviewed source premises capacity");
  if (s->kind == CD_DEV_CAPACITY && (!s->capacity || s->capacity > 128))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Capacity proposal outside model bounds");
  if (s->kind == CD_DEV_USE && !cd_basis_valid(s->use))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Use proposal requires bounded explicit scenario label");
  CdProject *p = &w->projects[b];
  const char *use = s->use ? s->use : "";
  if (key == p->view.key) {
    if (p->view.kind != s->kind || p->view.capacity != s->capacity ||
        p->view.minutes != s->minutes || strcmp(p->view.use, use) ||
        strcmp(p->view.basis, s->basis))
      return cd_diag(d, CD_INVALID_ARGUMENT,
                     "Project key reused for different proposal");
    *out = p->view.id;
    return cd_diag(d, CD_OK,
                   "Existing project returned without duplicating materials");
  }
  if (key < p->view.key || active(p->view.state))
    return cd_diag(d, CD_INVALID_STATE,
                   "Stale project key or outstanding site project");
  if (w->next_project == UINT64_MAX)
    return cd_diag(d, CD_CAPACITY, "Project IDs exhausted");
  uint64_t materials[CD_GOODS] = {0};
  materials[CD_WOOD] = s->kind == CD_DEV_CAPACITY ? 2 : 1;
  materials[CD_TOOL] = 1;
  if (s->kind == CD_DEV_CAPACITY)
    materials[CD_CLAY] = 2;
  CdPremises *site = &w->premises[b];
  for (unsigned g = 0; g < CD_GOODS; g++)
    if (site->goods[g] < materials[g])
      return cd_diag(
          d, CD_CAPACITY,
          "Required finite development materials not delivered to worksite");
  memset(p, 0, sizeof *p);
  p->view = (CdDevelopmentView){.id = w->next_project++,
                                .key = key,
                                .building = id,
                                .state = CD_TASK_PENDING,
                                .kind = s->kind,
                                .capacity = s->capacity,
                                .minutes = s->minutes};
  snprintf(p->view.use, 96, "%s", use);
  snprintf(p->view.basis, 96, "%s", s->basis);
  memcpy(p->materials, materials, sizeof materials);
  for (unsigned g = 0; g < CD_GOODS; g++) {
    site->goods[g] -= materials[g];
    site->reserved[g] += materials[g];
  }
  *out = p->view.id;
  return cd_diag(d, CD_OK,
                 "Scenario project staged; capacity/use/condition unchanged "
                 "until labor and validation complete");
}
CdStatus cd_world_project_view(const CdWorld *w, CdEntityId id,
                               CdDevelopmentView *out, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!w || !out || b == CD_NONE || !w->projects[b].view.id)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown worksite project");
  *out = w->projects[b].view;
  return cd_diag(d, CD_OK, out->reason);
}
CdStatus cd_world_project_cancel(CdWorld *w, CdEntityId building, uint64_t id,
                                 CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, building) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE || !id || w->projects[b].view.id != id)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown project");
  CdProject *p = &w->projects[b];
  if (active(p->view.state)) {
    for (size_t i = 0; i < w->count; i++)
      if (w->people[i].work.view.project == id)
        cd_work_release(w, &w->people[i], 1);
    for (unsigned g = 0; g < CD_GOODS; g++) {
      w->premises[b].reserved[g] -= p->materials[g];
      w->premises[b].goods[g] += p->materials[g];
      p->materials[g] = 0;
    }
    p->view.state = CD_TASK_CANCELLED;
    snprintf(p->view.reason, 96,
             "cancelled; unconsumed materials returned, paid labor retained");
  }
  return cd_diag(d, CD_OK, "Project cancellation is idempotent");
}
void cd_development_tick(CdWorld *w) {
  for (uint32_t b = 0; b < w->content->np; b++) {
    CdProject *p = &w->projects[b];
    if (!active(p->view.state) || p->view.progress < p->view.minutes)
      continue;
    CdPremises *site = &w->premises[b];
    if (site->fire) {
      p->view.state = CD_TASK_WAITING;
      snprintf(p->view.reason, 96,
               "active fire blocks premises-work publication");
      continue;
    }
    if (p->view.kind == CD_DEV_CAPACITY &&
        (cd_housing_load(w, b) > p->view.capacity ||
         site->work_capacity > p->view.capacity ||
         site->beds > p->view.capacity)) {
      p->view.state = CD_TASK_WAITING;
      snprintf(p->view.reason, 96,
               "completed labor waits for occupancy/work/clinic capacity "
               "validation");
      continue;
    }
    if (p->view.kind == CD_DEV_REPAIR)
      site->condition = 1000;
    else if (p->view.kind == CD_DEV_CAPACITY) {
      site->capacity_changed = 1;
      site->capacity_override = p->view.capacity;
    } else
      snprintf(site->scenario_use, 96, "%s", p->view.use);
    for (unsigned g = 0; g < CD_GOODS; g++) {
      site->reserved[g] -= p->materials[g];
      w->economy.consumed[g] += p->materials[g];
      p->materials[g] = 0;
    }
    p->view.state = CD_TASK_DONE;
    p->view.reason[0] = 0;
  }
}
