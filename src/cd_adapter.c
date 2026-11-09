#include "cd_bootstrap.h"
#include "cd_people.h"
#include "cd_memory.h"
#include "cd_random.h"
#include "cd_native.h"
#include "bronze/work/bronze_work.h"
#include "bronze/actors/bronze_actors.h"
#include "entity/entity.h"
#include "sim/sim.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct CdSmokeSession {
  BronzeActorRegistry *registry;
  size_t requested;
  CdPeople *people;
  CdPersonId *ids;
  size_t *sources;
  unsigned char *visited;
  simulated_being *body_scratch;
  int seeded;
  simulated_group *native_storage;
  void *sdk_buffer;
  size_t sdk_buffer_bytes;
  CdRandom random;
  uint64_t callbacks; /* diagnostic observation count, never a ticking clock */
  int failed, started;
  CdNativeMinute client;
  void *client_context;
};
struct CdSmokeStage {
  CdSmokeSession *session;
  uint32_t seed;
  n_land *land_image;
  simulated_timing timing_image;
};
static CdSmokeSession *active_session;
static uint64_t
    last_world_token; /* Process namespace, never a serialized actor ID. */
static void free_session(CdSmokeSession *s) {
  if (!s)
    return;
  if (s->registry)
    bronze_actors_destroy(s->registry);
  cd_memory_free(s->registry);
  cd_people_close(s->people);
  cd_memory_free(s->ids);
  cd_memory_free(s->sources);
  cd_memory_free(s->visited);
  cd_memory_free(s->body_scratch);
  cd_memory_free(s->native_storage);
  cd_memory_free(s->sdk_buffer);
  cd_memory_free(s);
}
static CdStatus diagnostic(CdDiagnostic *out, CdStatus code,
                           const char *message) {
  if (out) {
    out->status = code;
    snprintf(out->message, sizeof(out->message), "%s", message);
  }
  return code;
}
static void native_minute(simulated_group *group, void *context) {
  /* Never dereference an inactive callback context. SDK close also clears it.
   */
  if (!active_session || context != active_session || !active_session->started)
    return;
  CdSmokeSession *session = active_session;
  const BronzeClock *clock = bronze_actors_clock(session->registry);
  if (!clock || !clock->open || clock->elapsed != session->callbacks + 1) {
    session->failed = 1;
    return;
  }
  session->callbacks++;
  if (group->num != session->requested ||
      bronze_actors_count(session->registry) != group->num) {
    session->failed = 1;
    return;
  }
  for (size_t i = 0; i < group->num; i++) {
    BronzeActor *actor =
        bronze_actors_for_being(session->registry, &group->beings[i]);
    if (!actor || !actor->id ||
        bronze_actors_resolve(session->registry, actor->id) !=
            &group->beings[i]) {
      session->failed = 1;
      continue;
    }
    if (!session->seeded) {
      if (cd_people_add(session->people, actor->id, i) != CD_OK)
        session->failed = 1;
    }
    const CdPersonRecord *record = cd_people_find(
        session->people,
        (CdPersonRef){cd_people_token(session->people), actor->id});
    if (!record || record->native_slot != i)
      session->failed = 1;
  }
  session->seeded = 1;
  if (cd_people_count(session->people) != group->num)
    session->failed = 1;
  if (!session->failed && session->client)
    session->client(session, session->client_context);
  /* No movement, physiology or city routines in this bootstrap fixture.
     SDK city mode does not also execute island autonomy. */
}
size_t cd_smoke_default_people(void) { return CANTERBURY_POPULATION; }
size_t cd_smoke_capacity(void) { return LARGE_SIM; }
CdStatus cd_smoke_prepare(size_t people, uint32_t seed, CdSmokeStage **out,
                          CdDiagnostic *e) {
  if (!out || *out || !people || people > LARGE_SIM)
    return diagnostic(
        e, CD_INVALID_ARGUMENT,
        "Initial people must be 1..30000 and stage output must be empty");
  if (last_world_token == UINT64_MAX)
    return diagnostic(e, CD_CAPACITY, "World token namespace exhausted");
  CdSmokeStage *stage = cd_memory_calloc(1, sizeof *stage, CD_MEM_CITY);
  if (!stage)
    return diagnostic(e, CD_CAPACITY, "Cannot allocate staging handle");
  CdSmokeSession *s = cd_memory_calloc(1, sizeof *s, CD_MEM_CITY);
  stage->session = s;
  stage->seed = seed;
  if (!s)
    goto failed;
  s->requested = people;
  s->people = cd_people_create(LARGE_SIM, ++last_world_token);
  s->ids = cd_memory_calloc(LARGE_SIM, sizeof *s->ids, CD_MEM_TEMP);
  s->sources = cd_memory_calloc(LARGE_SIM, sizeof *s->sources, CD_MEM_TEMP);
  s->visited = cd_memory_calloc(LARGE_SIM, 1, CD_MEM_TEMP);
  s->body_scratch = cd_memory_calloc(1, sizeof *s->body_scratch, CD_MEM_TEMP);
  s->native_storage =
      cd_memory_calloc(1, sizeof *s->native_storage, CD_MEM_NATIVE);
  s->sdk_buffer_bytes = sim_canterbury_storage_bytes(MAP_AREA);
  s->sdk_buffer = cd_memory_calloc(1, s->sdk_buffer_bytes, CD_MEM_SDK_BUFFER);
  size_t registry_bytes = bronze_actors_canterbury_storage_bytes();
  void *registry = cd_memory_calloc(1, registry_bytes, CD_MEM_ACTORS);
  if (registry) {
    s->registry =
        bronze_actors_canterbury_init_storage(registry, registry_bytes);
    if (!s->registry)
      cd_memory_free(registry);
  }
  if (!s->people || !s->ids || !s->sources || !s->visited || !s->body_scratch ||
      !s->native_storage || !s->sdk_buffer || !s->registry)
    goto failed;
  cd_random_seed(&s->random, seed);
  *out = stage;
  return diagnostic(
      e, CD_OK, "Replacement resources prepared without native publication");
failed:
  cd_smoke_discard(stage);
  return diagnostic(
      e, CD_CAPACITY,
      "Replacement allocation/budget failed; live session preserved");
}
void cd_smoke_discard(CdSmokeStage *stage) {
  if (stage) {
    cd_memory_free(stage->land_image);
    free_session(stage->session);
    cd_memory_free(stage);
  }
}
CdStatus cd_smoke_publish(CdSmokeStage **staged, CdSmokeSession **current,
                          CdDiagnostic *e) {
  if (!staged || !*staged || !current)
    return diagnostic(e, CD_INVALID_ARGUMENT,
                      "Missing staged replacement or current handle");
  if (*current != active_session ||
      (!active_session && bronze_actors_current()))
    return diagnostic(e, CD_INVALID_STATE,
                      "Replacement does not own the active session");
  if (active_session) {
    const BronzeClock *c = bronze_actors_clock(active_session->registry);
    if (!c || c->open)
      return diagnostic(e, CD_INVALID_STATE,
                        "Publication requires a closed native interval");
  }
  CdSmokeStage *stage = *staged;
  CdSmokeSession *s = stage->session;
  /* Every required allocation and bound was checked before retiring the old
     owner. Fresh registry + fixed city layout make startup non-fallible. */
  cd_smoke_close(active_session);
  int attached = sim_canterbury_attach_storage(s->native_storage, s->sdk_buffer,
                                               (n_uint)s->sdk_buffer_bytes);
  bronze_actors_activate(s->registry);
  int configured =
      sim_canterbury_configure((n_uint)s->requested, native_minute, s);
  void *initialized = sim_init(KIND_START_UP, stage->seed, MAP_AREA, 0);
  if (!attached || !configured || !initialized) {
    fputs("Internal staged startup invariant failed\n", stderr);
    abort();
  }
  s->started = 1;
  active_session = s;
  sim_cycle();
  if (s->failed || s->callbacks != 1) {
    fputs("Internal staged binding invariant failed\n", stderr);
    abort();
  }
  stage->session = NULL;
  cd_memory_free(stage);
  *staged = NULL;
  *current = s;
  return diagnostic(e, CD_OK, "Prepared native session published");
}
CdStatus cd_smoke_open(size_t people, uint32_t seed, CdSmokeSession **out,
                       CdDiagnostic *e) {
  if (!out || !people || people > LARGE_SIM)
    return diagnostic(e, CD_INVALID_ARGUMENT,
                      "Initial people must be 1..30000");
  if (active_session || bronze_actors_current())
    return diagnostic(e, CD_INVALID_STATE,
                      "Only one native session can be active");
  *out = NULL;
  CdSmokeStage *stage = NULL;
  CdStatus status = cd_smoke_prepare(people, seed, &stage, e);
  if (status != CD_OK)
    return status;
  status = cd_smoke_publish(&stage, out, e);
  if (stage)
    cd_smoke_discard(stage);
  return status;
}
CdStatus cd_smoke_step(CdSmokeSession *session, unsigned minutes,
                       CdDiagnostic *error) {
  if (!session || session != active_session || session->failed)
    return diagnostic(error, CD_INVALID_STATE,
                      "No valid active bootstrap session");
  if (minutes > 10000)
    return diagnostic(error, CD_INVALID_ARGUMENT,
                      "Bootstrap requests are bounded to 10000 minutes");
  for (unsigned i = 0; i < minutes; i++) {
    const BronzeClock *clock = bronze_actors_clock(session->registry);
    uint64_t before = clock->elapsed;
    if (clock->open || before == UINT64_MAX)
      return diagnostic(error, CD_INVALID_STATE,
                        "Native interval is not ready");
    sim_cycle();
    if (session->failed || clock->open || clock->elapsed != before + 1 ||
        session->callbacks != clock->elapsed) {
      session->failed = 1;
      return diagnostic(error, CD_INVALID_STATE,
                        "Native minute/callback mismatch");
    }
  }
  return diagnostic(error, CD_OK, "Native step complete");
}
static uint64_t mix(uint64_t hash, uint64_t value) {
  return (hash ^ value) * UINT64_C(1099511628211);
}
CdStatus cd_smoke_read(const CdSmokeSession *session, CdSmokeReport *out,
                       CdDiagnostic *error) {
  if (!session || !out || session != active_session || session->failed)
    return diagnostic(error, CD_INVALID_STATE,
                      "No valid active bootstrap session");
  const BronzeClock *clock = bronze_actors_clock(session->registry);
  simulated_group *group = sim_group();
  if (!clock || clock->open || clock->elapsed != session->callbacks)
    return diagnostic(error, CD_INVALID_STATE,
                      "Inspection requires a closed native interval");
  uint64_t hash = mix(UINT64_C(1469598103934665603), clock->elapsed);
  int valid = cd_people_count(session->people) == group->num;
  for (size_t i = 0; i < group->num; i++) {
    simulated_being *body = &group->beings[i];
    BronzeActor *actor = bronze_actors_for_being(session->registry, body);
    const CdPersonRecord *record =
        actor ? cd_people_find(
                    session->people,
                    (CdPersonRef){cd_people_token(session->people), actor->id})
              : NULL;
    if (!actor || !record || record->native_slot != i ||
        bronze_actors_resolve(session->registry, actor->id) != body) {
      valid = 0;
      break;
    }
    hash = mix(hash, actor->id);
    hash = mix(hash, being_location_x(body));
    hash = mix(hash, being_location_y(body));
    hash = mix(hash, being_energy(body));
  }
  *out = (CdSmokeReport){session->requested,
                         (size_t)group->num,
                         bronze_actors_count(session->registry),
                         clock->elapsed,
                         session->callbacks,
                         hash,
                         valid};
  return diagnostic(error, valid ? CD_OK : CD_INVALID_STATE,
                    valid ? "Read-only bootstrap report"
                          : "Invalid native binding");
}
void cd_smoke_close(CdSmokeSession *session) {
  if (!session || session != active_session)
    return;
  active_session =
      NULL; /* Retire borrowed callback context before SDK teardown. */
  if (session->started) {
    session->started = 0;
    sim_close();
  }
  free_session(session);
}

static int leap(unsigned year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
CdStatus cd_smoke_civil_time(uint64_t elapsed, CdCivilTime *out,
                             CdDiagnostic *error) {
  /* Last supported civil year is 9999. Reject huge values before looping. */
  const uint64_t days_limit = (10000 - 1940) * UINT64_C(365) +
                              (9999 / 4 - 9999 / 100 + 9999 / 400) -
                              (1939 / 4 - 1939 / 100 + 1939 / 400);
  if (!out || elapsed / 1440 >= days_limit)
    return diagnostic(error, CD_INVALID_ARGUMENT,
                      "Scenario date must be within years 1940..9999");
  uint64_t days = elapsed / 1440;
  unsigned year = 1940, month = 1;
  while (days >= (unsigned)(365 + leap(year))) {
    days -= 365 + leap(year);
    year++;
  }
  static const unsigned lengths[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
  for (;;) {
    unsigned length = lengths[month - 1] + (month == 2 && leap(year));
    if (days < length)
      break;
    days -= length;
    month++;
  }
  *out = (CdCivilTime){(int)year, month, (unsigned)days + 1,
                       (unsigned)(elapsed % 1440)};
  return diagnostic(error, CD_OK, "Derived scenario date");
}
CdStatus cd_smoke_time(const CdSmokeSession *session, CdCivilTime *out,
                       CdDiagnostic *error) {
  if (!session || session != active_session || session->failed)
    return diagnostic(error, CD_INVALID_STATE,
                      "No valid active native session");
  const BronzeClock *clock = bronze_actors_clock(session->registry);
  if (!clock || clock->open || clock->elapsed != session->callbacks)
    return diagnostic(error, CD_INVALID_STATE,
                      "Scenario inspection requires a closed native interval");
  return cd_smoke_civil_time(clock->elapsed, out, error);
}

static int identity_ready(const CdSmokeSession *s) {
  if (!s || s != active_session || s->failed || !s->seeded ||
      bronze_actors_current() != s->registry)
    return 0;
  const BronzeClock *c = bronze_actors_clock(s->registry);
  return c && !c->open && c->elapsed == s->callbacks;
}
CdStatus cd_smoke_person(const CdSmokeSession *s, size_t slot, CdPersonRef *out,
                         CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(e, CD_INVALID_STATE,
                      "Person inspection requires an active closed interval");
  if (!out || slot >= sim_group()->num)
    return diagnostic(e, CD_INVALID_ARGUMENT, "Invalid native slot");
  BronzeActorId id = bronze_actors_canterbury_slot_id(s->registry, slot);
  CdPersonRef ref = {cd_people_token(s->people), id};
  const CdPersonRecord *r = cd_people_find(s->people, ref);
  if (!r || r->native_slot != slot)
    return diagnostic(e, CD_INVALID_STATE, "Person/native binding mismatch");
  *out = ref;
  return diagnostic(e, CD_OK, "Person reference");
}
CdStatus cd_smoke_resolve(const CdSmokeSession *s, CdPersonRef ref,
                          size_t *slot, CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(
        e, CD_INVALID_STATE,
        "Reference resolution requires an active closed interval");
  if (!slot)
    return diagnostic(e, CD_INVALID_ARGUMENT, "Missing slot output");
  const CdPersonRecord *r = cd_people_find(s->people, ref);
  if (!r)
    return diagnostic(e, CD_UNKNOWN_REFERENCE,
                      "Retired, missing or foreign-world person");
  if (r->native_slot >= sim_group()->num ||
      bronze_actors_resolve(s->registry, ref.person_id) !=
          &sim_group()->beings[r->native_slot])
    return diagnostic(e, CD_INVALID_STATE, "Native reference binding mismatch");
  *slot = r->native_slot;
  return diagnostic(e, CD_OK, "Resolved live person");
}
static CdStatus refresh_slots(CdSmokeSession *s) {
  size_t n = sim_group()->num;
  for (size_t i = 0; i < n; i++)
    s->ids[i] = bronze_actors_canterbury_slot_id(s->registry, i);
  CdStatus status = cd_people_validate_order(s->people, s->ids, n);
  if (status == CD_OK)
    cd_people_commit_order(s->people, s->ids, n);
  return status;
}
CdStatus cd_smoke_admit(CdSmokeSession *s, uint32_t seed, CdPersonRef *out,
                        CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(e, CD_INVALID_STATE,
                      "Admission requires an active closed interval");
  if (!out)
    return diagnostic(e, CD_INVALID_ARGUMENT,
                      "Missing admitted reference output");
  simulated_group *g = sim_group();
  BronzeActorId next = bronze_actors_next_id(s->registry);
  if (g->num >= g->max || cd_people_count(s->people) >= LARGE_SIM || !next ||
      next == UINT64_MAX)
    return diagnostic(e, CD_CAPACITY, "No native/person identity capacity");
  simulated_being *b = &g->beings[g->num];
  n_byte2 rng[2] = {(n_byte2)(seed >> 16), (n_byte2)seed};
  if (being_init(g->beings, 0, b, NULL, rng)) {
    being_erase(b);
    return diagnostic(e, CD_INVALID_STATE,
                      "Native fixture initialization failed");
  }
  if (!bronze_actors_birth(g, b)) {
    being_erase(b);
    return diagnostic(e, CD_CAPACITY, "Native identity admission failed");
  }
  BronzeActor *actor = bronze_actors_for_being(s->registry, b);
  if (!actor || cd_people_add(s->people, actor->id, g->num) != CD_OK) {
    bronze_actors_death(g, b);
    being_erase(b);
    s->failed = 1;
    return diagnostic(e, CD_INVALID_STATE,
                      "Person identity admission mismatch");
  }
  g->num++;
  s->requested = g->num;
  *out = (CdPersonRef){cd_people_token(s->people), actor->id};
  return diagnostic(e, CD_OK, "Ordinary fixture person admitted");
}
CdStatus cd_smoke_retire(CdSmokeSession *s, CdPersonRef ref, CdDiagnostic *e) {
  size_t slot;
  CdStatus status = cd_smoke_resolve(s, ref, &slot, e);
  if (status != CD_OK)
    return status;
  simulated_group *g = sim_group();
  size_t last = g->num - 1;
  int selected_removed = g->select == &g->beings[slot],
      selected_moved = g->select == &g->beings[last];
  bronze_actors_death(g, &g->beings[slot]);
  if (slot != last) {
    bronze_actors_relocated(g, &g->beings[last], &g->beings[slot]);
    g->beings[slot] = g->beings[last];
  }
  being_erase(&g->beings[last]);
  g->num--;
  s->requested = g->num;
  cd_people_remove(s->people, ref);
  if (selected_moved && slot != last)
    g->select = &g->beings[slot];
  else if (selected_removed || selected_moved)
    g->select = g->num ? &g->beings[0] : NULL;
  if (refresh_slots(s) != CD_OK) {
    s->failed = 1;
    return diagnostic(e, CD_INVALID_STATE, "Compaction binding mismatch");
  }
  return diagnostic(e, CD_OK, "Person retired; surviving bindings preserved");
}
CdStatus cd_smoke_reorder(CdSmokeSession *s, const CdPersonRef *order,
                          size_t count, CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(e, CD_INVALID_STATE,
                      "Reorder requires an active closed interval");
  if ((!order && count) || count != sim_group()->num)
    return diagnostic(e, CD_INVALID_ARGUMENT,
                      "A complete person permutation is required");
  for (size_t i = 0; i < count; i++) {
    const CdPersonRecord *r = cd_people_find(s->people, order[i]);
    if (!r)
      return diagnostic(e, CD_UNKNOWN_REFERENCE,
                        "Missing or foreign-world permutation reference");
    s->ids[i] = order[i].person_id;
    s->sources[i] = r->native_slot;
  }
  CdStatus status = cd_people_validate_order(s->people, s->ids, count);
  if (status != CD_OK)
    return diagnostic(e, status, "Invalid person permutation");
  simulated_group *g = sim_group();
  CdPersonId selected = 0;
  BronzeActor *a = bronze_actors_for_being(s->registry, g->select);
  if (a)
    selected = a->id;
  /* SDK validates completely before publication. No fallible work follows. */
  if (!bronze_actors_bind_order(s->registry, g, s->ids, count))
    return diagnostic(e, CD_INVALID_STATE, "Native permutation rejected");
  memset(s->visited, 0, LARGE_SIM);
  for (size_t i = 0; i < count; i++)
    if (!s->visited[i]) {
      *s->body_scratch = g->beings[i];
      size_t j = i;
      while (s->sources[j] != i) {
        g->beings[j] = g->beings[s->sources[j]];
        s->visited[j] = 1;
        j = s->sources[j];
      }
      g->beings[j] = *s->body_scratch;
      s->visited[j] = 1;
    }
  cd_people_commit_order(s->people, s->ids, count);
  g->select = selected ? bronze_actors_resolve(s->registry, selected) : NULL;
  return diagnostic(e, CD_OK, "Native population reordered atomically");
}
CdStatus cd_smoke_rebind(CdSmokeSession *s, CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(e, CD_INVALID_STATE,
                      "Rebind requires the owning closed native interval");
  size_t count = sim_group()->num;
  if (count != cd_people_count(s->people))
    return diagnostic(e, CD_INVALID_DATA, "Incomplete live person membership");
  memset(s->visited, 0, LARGE_SIM);
  for (size_t i = 0; i < count; i++) {
    const CdPersonRecord *r = cd_people_at(s->people, i);
    if (r->native_slot >= count || s->visited[r->native_slot])
      return diagnostic(e, CD_INVALID_DATA, "Duplicate or missing native slot");
    s->visited[r->native_slot] = 1;
    s->ids[r->native_slot] = r->ref.person_id;
  }
  if (!bronze_actors_bind_order(s->registry, sim_group(), s->ids, count))
    return diagnostic(e, CD_INVALID_DATA, "Native full rebind rejected");
  return diagnostic(e, CD_OK, "Full person/native rebind complete");
}

CdStatus cd_smoke_random(CdSmokeSession *s, CdRandomStream stream,
                         uint64_t *out, CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(e, CD_INVALID_STATE,
                      "Random draw requires the owning closed interval");
  if (!out || (unsigned)stream >= CD_RANDOM_STREAMS)
    return diagnostic(e, CD_INVALID_ARGUMENT,
                      "Invalid deterministic random stream");
  *out = cd_random_next(&s->random, (unsigned)stream);
  return diagnostic(e, CD_OK, "Explicit city random draw");
}
CdStatus cd_smoke_admit_next(CdSmokeSession *s, CdPersonRef *out,
                             CdDiagnostic *e) {
  if (!identity_ready(s))
    return diagnostic(e, CD_INVALID_STATE,
                      "Admission requires the owning closed interval");
  CdRandom staged = s->random;
  uint64_t seed = cd_random_next(&staged, CD_RNG_ADMISSION);
  CdStatus status = cd_smoke_admit(s, (uint32_t)seed, out, e);
  if (status == CD_OK)
    s->random = staged;
  return status;
}

static simulated_being *native_body(const CdSmokeSession *s, CdPersonRef ref) {
  if (!s || s != active_session || s->failed ||
      bronze_actors_current() != s->registry || !cd_people_find(s->people, ref))
    return NULL;
  return bronze_actors_resolve(s->registry, ref.person_id);
}
CdStatus cd_native_client(CdSmokeSession *s, CdNativeMinute hook,
                          void *context) {
  if (!identity_ready(s) || !hook || s->client)
    return CD_INVALID_STATE;
  s->client = hook;
  s->client_context = context;
  return CD_OK;
}
CdStatus cd_native_position(const CdSmokeSession *s, CdPersonRef ref,
                            uint16_t *x, uint16_t *y) {
  simulated_being *b = native_body(s, ref);
  if (!b || !x || !y)
    return CD_UNKNOWN_REFERENCE;
  *x = (uint16_t)being_location_x(b);
  *y = (uint16_t)being_location_y(b);
  return CD_OK;
}
CdStatus cd_native_write(CdSmokeSession *s, CdPersonRef ref, uint16_t x,
                         uint16_t y) {
  simulated_being *b = native_body(s, ref);
  if (!b)
    return CD_UNKNOWN_REFERENCE;
  n_byte2 p[2] = {x, y};
  being_set_location(b, p);
  return CD_OK;
}
int cd_native_claim(CdSmokeSession *s, CdPersonRef ref) {
  simulated_being *b = native_body(s, ref);
  return b && bronze_work_canterbury_claim(b, BRONZE_WORK_READY);
}
CdStatus cd_native_name(const CdSmokeSession *s, CdPersonRef ref, char *out,
                        size_t size) {
  simulated_being *b = native_body(s, ref);
  if (!b || !out || !size)
    return CD_UNKNOWN_REFERENCE;
  n_string_block name;
  being_name_simple(b, name);
  snprintf(out, size, "%s", name);
  return CD_OK;
}
CdStatus cd_native_city_body(CdSmokeSession *s, CdPersonRef ref) {
  simulated_being *b = native_body(s, ref);
  if (!b)
    return CD_UNKNOWN_REFERENCE;
  b->delta.awake = FULLY_AWAKE;
  being_set_speed(b, 0);
  return CD_OK;
}
unsigned cd_native_walk_units(void) { return FATIGUE_SPEED_THRESHOLD; }

uint64_t cd_native_minutes(const CdSmokeSession *s) {
  if (!s || s != active_session)
    return 0;
  const BronzeClock *clock = bronze_actors_clock(s->registry);
  return clock ? clock->elapsed : 0;
}

int cd_native_closed(const CdSmokeSession *s) { return identity_ready(s); }
int cd_native_energy(const CdSmokeSession *s, CdPersonRef ref) {
  simulated_being *b = native_body(s, ref);
  return b ? being_energy(b) : 0;
}
void cd_native_energy_set(CdSmokeSession *s, CdPersonRef ref, int energy) {
  simulated_being *b = native_body(s, ref);
  if (b) {
    if (energy < 0)
      energy = 0;
    if (energy > BEING_FULL)
      energy = BEING_FULL;
    being_energy_delta(b, energy - being_energy(b));
  }
}
void cd_native_awake(CdSmokeSession *s, CdPersonRef ref, int awake) {
  simulated_being *b = native_body(s, ref);
  if (b)
    b->delta.awake = awake ? FULLY_AWAKE : FULLY_ASLEEP;
}
void cd_native_block(CdSmokeSession *s, CdPersonRef ref, int sleeping) {
  simulated_being *b = native_body(s, ref);
  if (b)
    bronze_work_canterbury_claim(b, sleeping ? BRONZE_WORK_SLEEP
                                             : BRONZE_WORK_SURVIVAL);
}
unsigned cd_native_hungry(void) { return BEING_HUNGRY; }
unsigned cd_native_full(void) { return BEING_FULL; }

/* Pure vocabulary formatting: no body traits, identity, clock or RNG mutation.
 */
void cd_native_fixture_name(uint64_t given_key, uint64_t family_key, char *out,
                            size_t size) {
  if (!out || !size)
    return;
  n_string_block name;
  n_byte2 first =
      (n_byte2)((given_key & 255) |
                ((given_key & 256) ? ((n_byte2)SEX_FEMALE << 8) : 0));
  being_name_byte2(first, (n_byte2)(family_key & 65535), name);
  snprintf(out, size, "%s", name);
}

int cd_native_can_work(const CdSmokeSession *s, CdPersonRef ref) {
  simulated_being *b = native_body(s, ref);
  if (!b)
    return 0;
  BronzeActor *a = bronze_actors_for_being(s->registry, b);
  const BronzeClock *c = bronze_actors_clock(s->registry);
  BronzeNeeds needs = bronze_needs_read(b);
  return a && c && c->open && a->work.sampled &&
         a->work.sampled_tick == c->tick && !a->work.consumed &&
         a->work.initial_reason == BRONZE_WORK_READY &&
         bronze_needs_reason(&needs) == BRONZE_WORK_READY &&
         a->work.x == being_location_x(b) && a->work.y == being_location_y(b);
}

/* Closed city checkpoint seam; all staging allocations precede publication. */
typedef struct {
  uint64_t magic, count, callbacks, selected;
  CdRandom random;
} CdNativeImage;
static int compare_id(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}
static uint64_t native_hash_bytes(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  while (n--)
    h = mix(h, *p++);
  return h;
}
static uint64_t complete_native_digest(const CdSmokeSession *s,
                                       const simulated_group *g) {
  uint64_t h = mix(UINT64_C(1469598103934665603), s->callbacks);
  h = mix(h, bronze_actors_next_id(s->registry));
  h = native_hash_bytes(h, &s->random, sizeof s->random);
  const BronzeClock *c = bronze_actors_clock(s->registry);
  h = native_hash_bytes(h, c, sizeof *c);
  for (size_t i = 0; i < g->num; i++) {
    BronzeActorId id = bronze_actors_canterbury_slot_id(s->registry, i);
    h = mix(h, id);
    h = native_hash_bytes(h, &g->beings[i], sizeof g->beings[i]);
    BronzeActor *a = bronze_actors_find(s->registry, id);
    if (a) {
      h = native_hash_bytes(h, &a->work, sizeof a->work);
      h = mix(h, a->remaining_ticks);
      h = mix(h, a->task);
    }
  }
  return h;
}
uint64_t cd_native_full_digest(const CdSmokeSession *s) {
  return identity_ready(s) ? complete_native_digest(s, sim_group()) : 0;
}
uint64_t cd_native_stage_digest(const CdSmokeStage *stage) {
  return stage && stage->session
             ? complete_native_digest(stage->session,
                                      stage->session->native_storage)
             : 0;
}
uint64_t cd_native_stage_token(const CdSmokeStage *stage) {
  return stage ? cd_people_token(stage->session->people) : 0;
}
CdStatus cd_native_checkpoint(const CdSmokeSession *s, CdCheckpointIo io,
                              void *ctx, CdDiagnostic *d) {
  if (!identity_ready(s) || !io)
    return diagnostic(d, CD_INVALID_STATE,
                      "Checkpoint needs sole closed native owner");
  simulated_group *g = sim_group();
  CdNativeImage h = {0};
  h.magic = UINT64_C(0x314e544443);
  h.count = g->num;
  h.callbacks = s->callbacks;
  h.random = s->random;
  BronzeActor *selected = bronze_actors_for_being(s->registry, g->select);
  h.selected = selected ? selected->id : 0;
  size_t size = bronze_actors_canterbury_checkpoint_bytes(g->num);
  void *blob = cd_memory_calloc(1, size, CD_MEM_TEMP);
  if (!blob)
    return diagnostic(d, CD_CAPACITY,
                      "Native checkpoint buffer allocation failed");
  int valid = bronze_actors_canterbury_checkpoint(s->registry, blob, size) &&
              io(ctx, &h, sizeof h) &&
              io(ctx, (void *)land_canterbury_snapshot(), sizeof(n_land)) &&
              io(ctx, sim_timing(), sizeof(simulated_timing)) &&
              io(ctx, &g->remains, sizeof g->remains);
  for (size_t i = 0; valid && i < g->num; i++)
    valid = io(ctx, &g->beings[i], sizeof g->beings[i]);
  if (valid)
    valid = io(ctx, blob, size);
  cd_memory_free(blob);
  return diagnostic(
      d, valid ? CD_OK : CD_IO_ERROR,
      valid ? "Complete native/Bronze checkpoint written"
            : "Unsupported native extension or checkpoint write failed");
}
CdStatus cd_native_checkpoint_stage(CdCheckpointIo io, void *ctx,
                                    CdSmokeStage **out, CdDiagnostic *d) {
  CdNativeImage h;
  if (!out || *out || !io || !io(ctx, &h, sizeof h) ||
      h.magic != UINT64_C(0x314e544443) || h.count > LARGE_SIM || !h.callbacks)
    return diagnostic(d, CD_INVALID_DATA, "Invalid native checkpoint header");
  CdSmokeStage *stage = NULL;
  CdStatus status = cd_smoke_prepare(h.count ? h.count : 1, 0, &stage, d);
  if (status != CD_OK)
    return status;
  CdSmokeSession *s = stage->session;
  s->requested = h.count;
  s->callbacks = h.callbacks;
  s->random = h.random;
  s->native_storage->num = h.count;
  s->native_storage->max = LARGE_SIM;
  stage->land_image = cd_memory_calloc(1, sizeof(n_land), CD_MEM_TEMP);
  size_t bytes = bronze_actors_canterbury_checkpoint_bytes(h.count);
  void *blob = cd_memory_calloc(1, bytes, CD_MEM_TEMP);
  if (!stage->land_image || !blob) {
    status =
        diagnostic(d, CD_CAPACITY, "Native restore staging allocation failed");
    goto done;
  }
  int valid =
      io(ctx, stage->land_image, sizeof(n_land)) &&
      io(ctx, &stage->timing_image, sizeof stage->timing_image) &&
      io(ctx, &s->native_storage->remains, sizeof s->native_storage->remains);
  for (size_t i = 0; valid && i < h.count; i++)
    valid = io(ctx, &s->native_storage->beings[i], sizeof(simulated_being));
  valid = valid && io(ctx, blob, bytes) &&
          bronze_actors_canterbury_checkpoint_stage(s->registry, blob, bytes,
                                                    s->native_storage);
  const BronzeClock *clock = bronze_actors_clock(s->registry);
  if (!valid || clock->elapsed != h.callbacks ||
      stage->land_image->time >= 1440 ||
      clock->tick !=
          (uint64_t)stage->land_image->date * 1440 + stage->land_image->time) {
    status = diagnostic(d, CD_INVALID_DATA,
                        "Native/Bronze clock or actor checkpoint mismatch");
    goto done;
  }
  for (size_t i = 0; i < h.count; i++)
    s->ids[i] = bronze_actors_canterbury_slot_id(s->registry, i);
  qsort(s->ids, h.count, sizeof *s->ids, compare_id);
  for (size_t i = 0; i < h.count; i++) {
    size_t slot = bronze_actors_canterbury_record_slot(s->registry, s->ids[i]);
    if (slot >= h.count || cd_people_add(s->people, s->ids[i], slot) != CD_OK) {
      status = diagnostic(d, CD_INVALID_DATA,
                          "Duplicate or invalid restored actor IDs");
      goto done;
    }
  }
  if (h.selected) {
    size_t slot = bronze_actors_canterbury_record_slot(s->registry, h.selected);
    if (slot >= h.count) {
      status = diagnostic(d, CD_INVALID_DATA, "Invalid native selection");
      goto done;
    }
    s->native_storage->select = &s->native_storage->beings[slot];
  }
  s->seeded = 1;
  *out = stage;
  stage = NULL;
  status = CD_OK;
done:
  cd_memory_free(blob);
  cd_smoke_discard(stage);
  return status;
}
int cd_native_checkpoint_owner(const CdSmokeSession *s) {
  return s ? identity_ready(s) : !active_session && !bronze_actors_current();
}
void cd_native_checkpoint_publish(CdSmokeStage **out,
                                  CdSmokeSession **current) {
  CdSmokeStage *stage = *out;
  CdSmokeSession *s = stage->session;
  cd_smoke_close(active_session);
  if (!sim_canterbury_install_storage(s->native_storage, s->sdk_buffer,
                                      s->sdk_buffer_bytes, native_minute, s)) {
    fputs("Validated native storage install invariant failed\n", stderr);
    abort();
  }
  land_canterbury_install(stage->land_image);
  *sim_timing() = stage->timing_image;
  bronze_actors_activate(s->registry);
  if (!bronze_actors_rebind(s->registry, s->native_storage)) {
    fputs("Validated native rebind invariant failed\n", stderr);
    abort();
  }
  s->started = 1;
  active_session = s;
  *current = s;
  stage->session = NULL;
  cd_memory_free(stage->land_image);
  cd_memory_free(stage);
  *out = NULL;
}

size_t cd_native_stage_count(const CdSmokeStage *s) {
  return s ? s->session->requested : 0;
}
int cd_native_stage_has(const CdSmokeStage *s, CdPersonId id) {
  return s && bronze_actors_canterbury_record_slot(s->session->registry, id) <
                  s->session->requested;
}
uint64_t cd_native_checkpoint_abi(void) {
  return sizeof(simulated_being) ^ ((uint64_t)sizeof(n_land) << 20) ^
         ((uint64_t)sizeof(BronzeActor) << 40) ^
         bronze_actors_canterbury_checkpoint_bytes(0);
}

uint64_t cd_native_stage_minutes(const CdSmokeStage *s) {
  return s ? s->session->callbacks : 0;
}
