#include "cd_scenario_startup.h"
#include "cd_city_internal.h"
#include "cd_native.h"
#include <stdint.h>
#include <stdlib.h>
static CdWorld *active_world;
int cd_world_is_owner(const CdWorld *w) { return w == active_world; }
static void destroy(CdWorld *w) {
  if (!w)
    return;
  if (w->native)
    cd_smoke_close(w->native);
  cd_content_close(w->content);
  cd_memory_free(w->records);
  cd_memory_free(w->record_cursors);
  cd_memory_free(w->operations);
  cd_memory_free(w->offices);
  cd_memory_free(w->rosters);
  cd_memory_free(w->services);
  cd_memory_free(w->gatherings);
  cd_memory_free(w->vehicles);
  cd_memory_free(w->pressures);
  cd_memory_free(w->jobs);
  cd_memory_free(w->projects);
  cd_memory_free(w->institutions);
  cd_memory_free(w->home_wallet);
  cd_memory_free(w->home_goods);
  cd_memory_free(w->premises);
  cd_memory_free(w->home_meals);
  cd_memory_free(w->home_raw);
  cd_memory_free(w->life);
  cd_memory_free(w->people);
  cd_memory_free(w->homes);
  cd_memory_free(w->housing);
  cd_memory_free(w->visitors);
  cd_memory_free(w->owners);
  cd_memory_free(w->crowd);
  cd_memory_free(w->traffic);
  cd_memory_free(w->closed);
  cd_memory_free(w->locks);
  cd_memory_free(w->distance);
  cd_memory_free(w->previous);
  cd_memory_free(w->heap);
  cd_memory_free(w->heap_pos);
  cd_memory_free(w);
}
CdStatus cd_world_allocate(const CdContent *content, const CdScenario *s,
                           CdWorld **out, CdDiagnostic *d) {
  CdWorld *w = cd_memory_calloc(1, sizeof *w, CD_MEM_CITY);
  if (!w)
    return cd_diag(d, CD_CAPACITY, "Cannot allocate world");
  w->content = (CdContent *)content;
  cd_content_retain(w->content);
  w->count = w->initial_count = s->initial_people;
  w->seed = s->seed;
  w->next_request = w->next_work = w->next_trade = w->next_project = 1;
  for (unsigned month = 1; month < s->civil_origin.month; month++)
    w->origin_offset += cd_days_in_month(1940, month) * 1440;
  w->origin_offset +=
      (s->civil_origin.day - 1) * 1440 + s->civil_origin.minute_of_day;
#define ALLOC(field, count, category)                                          \
  do {                                                                         \
    if (count) {                                                               \
      w->field =                                                               \
          cd_memory_calloc((size_t)(count), sizeof *w->field, category);       \
      if (!w->field)                                                           \
        goto capacity;                                                         \
    }                                                                          \
  } while (0)
  w->life_capacity = cd_smoke_capacity() * 2;
  w->record_next = 1;
  ALLOC(records, CD_RECORD_LIMIT, CD_MEM_CITY);
  ALLOC(record_cursors, cd_record_cursor_count(content), CD_MEM_CITY);
  ALLOC(operations, CD_OPERATION_LIMIT, CD_MEM_CITY);
  ALLOC(people, cd_smoke_capacity(), CD_MEM_CITY);
  ALLOC(homes, cd_smoke_capacity(), CD_MEM_CITY);
  ALLOC(home_meals, cd_smoke_capacity(), CD_MEM_CITY);
  ALLOC(home_raw, cd_smoke_capacity(), CD_MEM_CITY);
  ALLOC(life, w->life_capacity, CD_MEM_CITY);
  ALLOC(premises, content->np, CD_MEM_CITY);
  ALLOC(offices, CD_CIVIC_LIMIT, CD_MEM_CITY);
  ALLOC(rosters, CD_CIVIC_LIMIT, CD_MEM_CITY);
  ALLOC(services, CD_DISPATCH_LIMIT, CD_MEM_CITY);
  ALLOC(gatherings, CD_CIVIC_LIMIT, CD_MEM_CITY);
  ALLOC(vehicles, CD_VEHICLE_LIMIT, CD_MEM_CITY);
  ALLOC(pressures, CD_CIVIC_LIMIT, CD_MEM_CITY);
  ALLOC(jobs, cd_smoke_capacity(), CD_MEM_CITY);
  ALLOC(projects, content->np, CD_MEM_CITY);
  ALLOC(institutions, content->ns, CD_MEM_CITY);
  ALLOC(home_wallet, cd_smoke_capacity(), CD_MEM_CITY);
  ALLOC(home_goods, cd_smoke_capacity() * CD_GOODS, CD_MEM_CITY);
  ALLOC(housing, content->np, CD_MEM_CITY);
  ALLOC(visitors, content->np, CD_MEM_CITY);
  ALLOC(owners, content->np, CD_MEM_CITY);
  ALLOC(locks, content->np, CD_MEM_CITY);
  ALLOC(closed, content->ne, CD_MEM_CITY);
  ALLOC(crowd, content->ne, CD_MEM_CITY);
  ALLOC(traffic, content->ne, CD_MEM_CITY);
  ALLOC(distance, content->nn, CD_MEM_TEMP);
  ALLOC(previous, content->nn, CD_MEM_TEMP);
  ALLOC(heap, content->nn, CD_MEM_TEMP);
  ALLOC(heap_pos, content->nn, CD_MEM_TEMP);
#undef ALLOC
  for (uint32_t i = 0; i < content->np; i++) {
    w->locks[i] = (unsigned char)content->polygons[i].locked;
    w->premises[i].condition = 1000;
  }
  *out = w;
  return CD_OK;
capacity:
  destroy(w);
  return cd_diag(d, CD_CAPACITY, "City allocation failed; live world retained");
}
void cd_world_destroy_staged(CdWorld *w) { destroy(w); }
void cd_world_publish_checkpoint(CdWorld **out, CdWorld *w,
                                 CdSmokeStage **stage) {
  CdWorld *old = *out;
  CdSmokeSession *native = old ? old->native : NULL;
  cd_native_checkpoint_publish(stage, &native);
  w->native = native;
  cd_native_client(native, cd_city_minute, w);
  if (old) {
    old->native = NULL;
    destroy(old);
  }
  active_world = w;
  *out = w;
}
static CdStatus open_world(const CdContent *, const CdScenario *,
                           const unsigned *, CdWorld **, CdDiagnostic *);

CdStatus cd_world_open(const CdContent *content, const CdScenario *scenario,
                       CdWorld **out, CdDiagnostic *d) {
  return open_world(content, scenario, NULL, out, d);
}

CdStatus cd_world_open_startup(const CdContent *content, const CdScenarioStartup *startup,
                               CdWorld **out, CdDiagnostic *d) {
  if (!startup)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing scenario startup");
  size_t count = startup->scenario.initial_people;
  uint64_t total = 0, minors = 0;
  for (unsigned i = 0; i < 5; ++i) {
    total += startup->cohorts[i];
    if (i < 3)
      minors += startup->cohorts[i];
  }
  size_t child_slots = 2 * (count / 4) + (count % 4 == 3);
  if (total != count || minors > child_slots)
    return cd_diag(d, CD_INVALID_DATA, "Scenario cohorts cannot form guarded households");
  CdStatus status = open_world(content, &startup->scenario, startup->cohorts, out, d);
  if (status == CD_OK)
    memcpy((*out)->scenario_identity, startup->identity, sizeof startup->identity);
  return status;
}

static CdStatus open_world(const CdContent *content, const CdScenario *s,
                           const unsigned *cohorts, CdWorld **out, CdDiagnostic *d) {
  if (!content || !s || !out || (*out && *out != active_world) ||
      (active_world && *out != active_world) || !s->initial_people ||
      s->initial_people > cd_smoke_capacity() || s->civil_origin.year != 1940 ||
      !cd_civil_valid(s->civil_origin) || !s->policy_id)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "World requires bounded population, valid 1940 origin and "
                   "the owning output handle");
  if (strcmp(s->policy_id, "synthetic-ordinary-v1"))
    return cd_diag(d, CD_UNSUPPORTED,
                   "No sourced historical demographic distribution is "
                   "supplied; select synthetic-ordinary-v1 explicitly");
  CdWorld *w = NULL;
  CdStatus allocation = cd_world_allocate(content, s, &w, d);
  if (allocation != CD_OK)
    return allocation;
  CdSmokeStage *stage = NULL;
  CdStatus status = cd_smoke_prepare(
      w->count, (uint32_t)(s->seed ^ (s->seed >> 32)), &stage, d);
  if (status != CD_OK) {
    destroy(w);
    return status;
  }
  CdWorld *old = *out;
  CdSmokeSession *native = old ? old->native : NULL;
  status = cd_smoke_publish(&stage, &native, d);
  if (status != CD_OK) {
    cd_smoke_discard(stage);
    destroy(w);
    return status;
  }
  w->native = native;
  /* All model arrays and policy validation were staged before native
   * publication. */
  if (cd_population_init(w, s, cohorts) != CD_OK || cd_housing_init(w) != CD_OK ||
      cd_native_client(native, cd_city_minute, w) != CD_OK) {
    fputs("Internal city publication invariant failed\n", stderr);
    abort();
  }
  if (old) {
    old->native = NULL;
    destroy(old);
  }
  active_world = w;
  cd_human_init(w);
  cd_record_emit(w, CD_EVENT_WORLD, (CdPersonRef){0}, (CdPersonRef){0},
                 w->count, 0, CD_RECORD_PUBLIC, "synthetic-ordinary-v1",
                 "Ordinary city created with explicit synthetic demographics");
  *out = w;
  return cd_diag(d, CD_OK,
                 "Ordinary city world published; demographics are an explicit "
                 "synthetic fixture");
}

void cd_world_close(CdWorld *w) {
  if (!w)
    return;
  if (w == active_world)
    active_world = NULL;
  destroy(w);
}
CdStatus cd_world_step(CdWorld *w, unsigned minutes, CdDiagnostic *d) {
  if (!w || w != active_world)
    return cd_diag(d, CD_INVALID_STATE, "Inactive city world");
  if (minutes > 10000)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Minute request exceeds 10000");
  for (unsigned i = 0; i < minutes; i++) {
    cd_operations_tick(w);
    CdStatus status = cd_smoke_step(w->native, 1, d);
    if (status != CD_OK)
      return status;
    status = cd_mortality_commit(w, d);
    if (status != CD_OK)
      return status;
  }
  return cd_diag(d, CD_OK, "Native human minutes committed");
}
CdStatus cd_world_clock(const CdWorld *w, CdClockView *out, CdDiagnostic *d) {
  if (!w || w != active_world || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing active city clock output");
  uint64_t minutes = cd_native_minutes(w->native);
  CdCivilTime time;
  CdStatus s = cd_smoke_civil_time(minutes + w->origin_offset, &time, d);
  if (s == CD_OK)
    *out = (CdClockView){minutes, time};
  return s;
}
void cd_city_minute(CdSmokeSession *native, void *context) {
  CdWorld *w = context;
  if (w->native == native)
    cd_human_tick(w);
}
CdStatus cd_world_summary(const CdWorld *w, CdCitySummary *out,
                          CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing city summary output");
  CdClockView clock;
  CdStatus s = cd_world_clock(w, &clock, d);
  if (s != CD_OK)
    return s;
  if (w->initial_count + w->admissions != w->count + w->deaths + w->departures)
    return cd_diag(d, CD_INVALID_STATE,
                   "Lifecycle population accounting mismatch");
  CdCitySummary r = {0};
  r.people = w->count;
  r.capacity = cd_smoke_capacity();
  r.households = w->home_count;
  r.admissions = w->admissions;
  r.births = w->births;
  r.deaths = w->deaths;
  r.departures = w->departures;
  r.birthdays = w->birthdays;
  r.native_minutes = clock.native_minutes;
  for (size_t i = 0; i < w->count; i++) {
    const CdCitizen *p = &w->people[i];
    r.asleep += p->sleeping;
    r.hungry +=
        cd_native_energy(w->native, p->civil.ref) <= (int)cd_native_hungry();
    r.ill += p->health.condition != CD_HEALTHY;
    r.receiving_care += p->health.clinic != 0;
    r.located += p->civil.position_known;
    r.visitors += p->presence != 0;
    if (p->civil.dwelling)
      r.housed++;
    else if (w->homes[p->civil.household - 1].housing_known)
      r.unhoused++;
    else
      r.housing_unknown++;
  }
  *out = r;
  return cd_diag(d, CD_OK, "Population and household totals reconciled");
}
static uint64_t hash(uint64_t h, uint64_t v) {
  return (h ^ v) * UINT64_C(1099511628211);
}
CdStatus cd_world_digest(const CdWorld *w, uint64_t *out, CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing world digest output");
  CdSmokeReport native;
  CdStatus s = cd_smoke_read(w->native, &native, d);
  if (s != CD_OK)
    return s;
  *out = cd_world_state_digest(w, cd_native_full_digest(w->native));
  return cd_diag(d, CD_OK, "Authoritative complete city digest");
}
uint64_t cd_world_state_digest(const CdWorld *w, uint64_t native_hash) {
  uint64_t h = hash(native_hash, w->seed);
  h = hash(h, w->origin_offset);
  h = hash(h, w->next_request);
  for (size_t i = 0; i < w->count; i++) {
    const CdCitizen *p = &w->people[i];
    h = hash(h, p->civil.ref.person_id);
    for (const unsigned char *c = (const unsigned char *)p->civil.name; *c; c++)
      h = hash(h, *c);
    h = hash(h, p->civil.birth.year);
    h = hash(h, p->civil.birth.month);
    h = hash(h, p->civil.birth.day);
    h = hash(h, p->civil.category);
    h = hash(h, p->civil.household);
    h = hash(h, p->civil.dwelling);
    h = hash(h, p->civil.guardian.person_id);
    h = hash(h, p->civil.spouse.person_id);
    h = hash(h, p->civil.position_known);
    h = hash(h, p->presence);
    h = hash(h, p->request);
    h = hash(h, p->door);
    h = hash(h, p->door_vertex);
    h = hash(h, p->held_edge);
    h = hash(h, p->inside);
    h = hash(h, p->entering);
    h = hash(h, p->door_key);
    h = hash(h, p->door_emergency);
    for (const unsigned char *c =
             (const unsigned char *)p->civil.admission_basis;
         *c; c++)
      h = hash(h, *c);
    h = hash(h, p->travel);
    h = hash(h, p->reason);
    h = hash(h, p->destination);
    h = hash(h, p->route_index);
    h = hash(h, p->vertex);
    h = hash(h, p->anchor);
    for (size_t j = 0; j < p->route.count; j++)
      h = hash(h, p->route.edges[j]);
  }
  for (uint32_t i = 0; i < w->content->ne; i++) {
    h = hash(h, w->closed[i]);
    h = hash(h, w->crowd[i]);
    h = hash(h, w->traffic[i]);
  }
  for (uint32_t i = 0; i < w->content->np; i++) {
    h = hash(h, w->housing[i]);
    h = hash(h, w->visitors[i]);
    h = hash(h, w->locks[i]);
    h = hash(h, w->owners[i].person_id);
  }
  for (size_t i = 0; i < w->home_count; i++) {
    h = hash(h, w->homes[i].housing_known);
    h = hash(h, w->homes[i].dwelling);
    h = hash(h, w->homes[i].members);
  }
  for (const char *c = w->content->generation; *c; c++)
    h = hash(h, (unsigned char)*c);
  h = cd_human_digest(w, h);
  h = cd_work_digest(w, h);
  h = cd_civic_digest(w, h);
  return cd_observer_digest(w, h);
}

CdStatus cd_snapshot_capture(const CdWorld *w, CdSnapshot **out,
                             CdDiagnostic *d) {
  if (!w || !out || *out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Snapshot output must be empty");
  CdSnapshot *s = cd_memory_calloc(1, sizeof *s, CD_MEM_CITY);
  if (!s)
    return cd_diag(d, CD_CAPACITY, "Snapshot owner allocation failed");
  s->count = w->count;
  s->records =
      cd_memory_calloc(CD_RECORD_LIMIT, sizeof *s->records, CD_MEM_CITY);
  if (!s->records) {
    cd_snapshot_close(s);
    return cd_diag(d, CD_CAPACITY, "Record snapshot allocation failed");
  }
  memcpy(s->records, w->records, CD_RECORD_LIMIT * sizeof *s->records);
  cd_world_record_summary(w, &s->record_summary, NULL);
  s->people = cd_memory_calloc(s->count, sizeof *s->people, CD_MEM_CITY);
  s->travel = cd_memory_calloc(s->count, sizeof *s->travel, CD_MEM_CITY);
  s->institutions =
      cd_memory_calloc(w->content->ns, sizeof *s->institutions, CD_MEM_CITY);
  if ((s->count && (!s->people || !s->travel)) ||
      (w->content->ns && !s->institutions)) {
    cd_snapshot_close(s);
    return cd_diag(d, CD_CAPACITY, "Snapshot allocation failed");
  }
  s->content = w->content;
  cd_content_retain(s->content);
  if (w->content->ns)
    memcpy(s->institutions, w->institutions,
           w->content->ns * sizeof *s->institutions);
  for (size_t i = 0; i < s->count; i++) {
    cd_population_view(w, &w->people[i], &s->people[i]);
    s->travel[i] = w->people[i].travel;
  }
  cd_world_civic_summary(w, &s->civic, NULL);
  *out = s;
  return cd_diag(d, CD_OK, "Immutable resident snapshot");
}
void cd_snapshot_close(CdSnapshot *s) {
  if (s) {
    cd_content_close(s->content);
    cd_memory_free(s->people);
    cd_memory_free(s->travel);
    cd_memory_free(s->institutions);
    cd_memory_free(s->records);
    cd_memory_free(s);
  }
}
size_t cd_snapshot_person_count(const CdSnapshot *s) {
  return s ? s->count : 0;
}
CdStatus cd_snapshot_person(const CdSnapshot *s, size_t i, CdPersonView *out,
                            CdDiagnostic *d) {
  if (!s || !out || i >= s->count)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown snapshot resident");
  const CdResidentView *p = &s->people[i];
  *out = (CdPersonView){p->ref,           p->name,      p->household,
                        p->occupation,    p->workplace, p->position,
                        p->age,           p->activity,  1,
                        p->position_known};
  return cd_diag(d, CD_OK, "Immutable resident view");
}
CdStatus cd_snapshot_institution(const CdSnapshot *s, CdEntityId id,
                                 CdInstitutionView *out, CdDiagnostic *d) {
  if (!s || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Missing institution snapshot output");
  for (uint32_t i = 0; i < s->content->ns; i++) {
    CdSite *site = &s->content->sites[i];
    if (site->id == id) {
      *out = (CdInstitutionView){id,
                                 site->building,
                                 site->name,
                                 site->religion,
                                 site->denomination,
                                 site->status,
                                 (int)site->located,
                                 s->institutions[i].known};
      return cd_diag(d, CD_OK, "Institution source uncertainty retained");
    }
  }
  return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown institution source ID");
}
CdStatus cd_world_preview(const CdWorld *world, const CdRequest *r,
                          CdPreview *out, CdDiagnostic *d) {
  if (!world || !r || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing movement preview");
  CdWorld *w = (CdWorld *)world;
  CdCitizen *p = cd_citizen(w, r->actor);
  if (!p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown/foreign actor");
  if (r->kind != CD_OPERATION_MOVE || r->target.kind != CD_TARGET_NODE)
    return cd_diag(
        d, CD_UNSUPPORTED,
        "Only qualified source-node movement is implemented in this cycle");
  CdClockView clock;
  cd_world_clock(w, &clock, NULL);
  if (r->earliest_native_minute > clock.native_minutes ||
      (r->deadline_native_minute &&
       r->deadline_native_minute < clock.native_minutes))
    return cd_diag(
        d, CD_UNSUPPORTED,
        "Future scheduling/expired requests require the operation cycle");
  if (!p->civil.position_known)
    return cd_diag(d, CD_INVALID_STATE, "Resident position is unplaced");
  if (p->door != CD_NONE || p->anchor >= w->content->nn ||
      hypot(cd_city_position(w, p).x - w->content->nodes[p->anchor].p.x,
            cd_city_position(w, p).y - w->content->nodes[p->anchor].p.y) >
          1.0 / cd_city_scale(w))
    return cd_diag(d, CD_INVALID_STATE,
                   "Movement preview requires an actual source-node boundary");
  CdRoute route;
  CdStatus s = cd_route_indices(
      w, p->anchor, cd_node_index(w->content, r->target.entity_id), &route);
  *out = (CdPreview){s,
                     s == CD_OK
                         ? (unsigned)ceil(route.cost / (cd_city_walk_units(w) /
                                                        cd_city_scale(w)))
                         : 0,
                     0};
  return cd_diag(d, s, cd_route_reason(route.reason));
}
CdStatus cd_world_submit(CdWorld *w, const CdRequest *r, CdRequestId *id,
                         CdDiagnostic *d) {
  if (!id)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing movement request output");
  CdPreview preview;
  CdStatus s = cd_world_preview(w, r, &preview, d);
  if (s != CD_OK)
    return s;
  if (w->next_request == UINT64_MAX)
    return cd_diag(d, CD_CAPACITY, "Request ID namespace exhausted");
  s = cd_world_travel(w, r->actor, r->target.entity_id, d);
  if (s == CD_OK) {
    CdCitizen *p = cd_citizen(w, r->actor);
    p->request = w->next_request++;
    *id = p->request;
  }
  return s;
}
CdStatus cd_world_cancel(CdWorld *w, CdRequestId id, CdDiagnostic *d) {
  if (!w || !id)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid movement request");
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].request == id) {
      CdStatus s = cd_world_stop(w, w->people[i].civil.ref, d);
      w->people[i].request = 0;
      return s;
    }
  return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown movement request");
}

void cd_human_tick(CdWorld *w) {
  uint64_t minute = cd_native_minutes(w->native);
  if (minute <= w->last_human_minute)
    return;
  w->last_human_minute = minute;
  cd_environment_tick(w);
  cd_needs_tick(w);
  cd_offices_tick(w);
  cd_services_plan(w);
  cd_community_plan(w);
  cd_logistics_plan(w);
  cd_shelter_plan(w);
  cd_health_tick(w);
  cd_jobs_plan(w);
  cd_movement_tick(w);
  cd_logistics_tick(w);
  cd_services_tick(w);
  cd_community_tick(w);
  cd_food_tick(w);
  cd_dsl_expiry_tick(w);
  cd_economy_tick(w);
  cd_jobs_tick(w);
  cd_development_tick(w);
  cd_social_tick(w);
  cd_records_tick(w);
}
