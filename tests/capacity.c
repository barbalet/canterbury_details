/* Capacity contracts only: no demographic birth/visitor simulation. */
#include "bronze/actors/bronze_actors.h"
#include "cd_bootstrap.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef NDEBUG
#error Capacity qualification requires active assertions
#endif
static void never_called(simulated_group *g, void *c) {
  (void)g;
  (void)c;
  assert(0);
}
static void bindings(simulated_group *g, BronzeActorRegistry *r) {
  assert(g->num == bronze_actors_count(r));
  for (n_uint i = 0; i < g->num; i++) {
    BronzeActor *a = bronze_actors_for_being(r, &g->beings[i]);
    assert(a && a->id && bronze_actors_resolve(r, a->id) == &g->beings[i]);
    assert(bronze_actors_canterbury_slot_id(r, i) == a->id);
  }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  size_t people = (size_t)strtoul(argv[1], NULL, 10);
  assert(people == 1 || people == 27000 || people == 29999 || people == 30000);
  assert(CANTERBURY_POPULATION == 27000 && LARGE_SIM == 30000);
  CdSmokeSession *s = NULL, *invalid = NULL;
  CdDiagnostic error;
  CdSmokeReport before, after;
  assert(cd_smoke_open(people, 1940, &s, &error) == CD_OK);
  assert(cd_smoke_read(s, &before, &error) == CD_OK &&
         before.native_people == people);
  simulated_group *g = sim_group();
  BronzeActorRegistry *r = bronze_actors_current();
  assert(g && r && g->max == LARGE_SIM);
  bindings(g, r);
  assert(cd_smoke_open(0, 1940, &invalid, &error) == CD_INVALID_ARGUMENT);
  assert(cd_smoke_open(30001, 1940, &invalid, &error) == CD_INVALID_ARGUMENT);
  assert(cd_smoke_open(SIZE_MAX, 1940, &invalid, &error) ==
         CD_INVALID_ARGUMENT);
  assert(!sim_canterbury_configure(30001, never_called, NULL));
  assert(cd_smoke_read(s, &after, &error) == CD_OK &&
         after.digest == before.digest);
  BronzeActorId next = bronze_actors_next_id(r),
                first = bronze_actors_for_being(r, &g->beings[0])->id;
  n_uint saved_num = g->num, saved_max = g->max;
  g->num = LARGE_SIM + 1;
  assert(!bronze_actors_reset(r, g));
  g->num = saved_num;
  g->max = LARGE_SIM + 1;
  assert(!bronze_actors_reset(r, g));
  g->max = saved_max;
  g->max = g->num - 1;
  assert(!bronze_actors_reset(r, g));
  g->max = saved_max;
  assert(bronze_actors_next_id(r) == next &&
         bronze_actors_resolve(r, first) == &g->beings[0]);
  bindings(g, r);
  /* Existing-body notifications are idempotent, not additional admissions. */
  assert(bronze_actors_birth(g, &g->beings[0]));
  assert(bronze_actors_next_id(r) == next && bronze_actors_count(r) == people);
  assert(!bronze_actors_birth(g, &g->beings[LARGE_SIM]));
  assert(bronze_actors_count(r) == people && bronze_actors_next_id(r) == next);
  unsigned replacement_admissions = 0;
  unsigned replacements = people == LARGE_SIM ? 32 : 1;
  for (unsigned i = 0; i < replacements; i++) {
    CdPersonRef retired, fresh;
    assert(cd_smoke_person(s, g->num - 1, &retired, &error) == CD_OK);
    assert(cd_smoke_retire(s, retired, &error) == CD_OK);
    assert(!bronze_actors_resolve(r, retired.person_id));
    assert(cd_smoke_admit(s, 1940 + i, &fresh, &error) == CD_OK);
    assert(fresh.person_id > retired.person_id &&
           !bronze_actors_find(r, retired.person_id));
    assert(bronze_actors_count(r) == people);
    replacement_admissions++;
  }
  unsigned growth_admissions = 0;
  size_t expected_people = people;
  if (people == CANTERBURY_POPULATION) {
    while (g->num < LARGE_SIM) {
      CdPersonRef born;
      assert(cd_smoke_admit(s, 1940 + growth_admissions, &born, &error) ==
             CD_OK);
      growth_admissions++;
    }
    expected_people = LARGE_SIM;
    CdSmokeReport full;
    assert(cd_smoke_read(s, &full, &error) == CD_OK);
    CdPersonRef overflow;
    assert(cd_smoke_admit(s, 1, &overflow, &error) == CD_CAPACITY);
    assert(cd_smoke_read(s, &after, &error) == CD_OK &&
           after.digest == full.digest);
    assert(growth_admissions == 3000);
  }
  bindings(g, r);
  assert(cd_smoke_step(s, 2, &error) == CD_OK);
  assert(cd_smoke_read(s, &after, &error) == CD_OK &&
         after.native_people == expected_people && after.bindings_valid &&
         after.native_minutes == before.native_minutes + 2);
  cd_smoke_close(s);
  assert(!bronze_actors_current());
  printf("{\"people\":%zu,\"capacity\":%u,\"replacement_admissions\":%u,"
         "\"growth_admissions\":%u,\"final_population\":%zu,\"bindings_valid\":"
         "true,\"overflow_atomic\":true,\"ok\":true}\n",
         people, (unsigned)LARGE_SIM, replacement_admissions, growth_admissions,
         expected_people);
  return 0;
}
