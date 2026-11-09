#include "cd_city_internal.h"
#include "cd_native.h"
#include <assert.h>
#include <stdlib.h>
static CdDiagnostic d;
#define OK(x)                                                                  \
  do {                                                                         \
    CdStatus result = (x);                                                     \
    if (result != CD_OK) {                                                     \
      fprintf(stderr, "line %d status %d: %s\n", __LINE__, result, d.message); \
      abort();                                                                 \
    }                                                                          \
  } while (0)
static CdScenario scenario = {
    8, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
static CdContent *content;
static CdWorld *open_world(CdWorld *w) {
  OK(cd_world_open(content, &scenario, &w, &d));
  return w;
}
static CdPersonRef person(CdWorld *w, size_t i) {
  CdResidentView p;
  OK(cd_world_resident(w, i, &p, &d));
  return p.ref;
}
static uint64_t digest(CdWorld *w) {
  uint64_t h;
  OK(cd_world_digest(w, &h, &d));
  return h;
}
static void arrive(CdWorld *w, CdPersonRef ref) {
  for (unsigned i = 0; i < 400; i++) {
    CdTravelView t;
    OK(cd_world_travel_view(w, ref, &t, &d));
    if (t.state == CD_ROUTE_ARRIVED)
      return;
    OK(cd_world_step(w, 1, &d));
  }
  fputs("human arrival timeout\n", stderr);
  abort();
}
static void inside(CdWorld *w, CdPersonRef p, unsigned building) {
  unsigned node = building == 1 ? 107 : 110, door = building == 1 ? 301 : 303;
  OK(cd_world_place(w, p, node, &d));
  OK(cd_world_enter(w, p, door, 0, 0, &d));
  arrive(w, p);
}
static void walk(CdWorld *w, CdPersonRef p, unsigned from, unsigned to) {
  OK(cd_world_exit(w, p, from == 1 ? 301 : 303, 0, 0, &d));
  arrive(w, p);
  OK(cd_world_travel(w, p, to == 1 ? 107 : 110, &d));
  arrive(w, p);
  OK(cd_world_enter(w, p, to == 1 ? 301 : 303, 0, 0, &d));
  arrive(w, p);
}
static void ledger(CdWorld *w) {
  CdFoodLedger f;
  OK(cd_world_food_ledger(w, &f, &d));
  assert(f.initial_meals + f.imported_meals + f.produced ==
             f.meal_stock + f.consumed + f.exported_meals &&
         f.imported_raw == f.raw_stock + f.produced + f.exported_raw);
}
static void cycle14(void) {
  CdWorld *w = open_world(NULL);
  CdPersonRef a = person(w, 0), b = person(w, 1), child = person(w, 2),
              baby = {0};
  uint64_t h = digest(w);
  assert(cd_world_birth(w, a, b, "technical birth fixture", &baby, &d) ==
             CD_UNSUPPORTED &&
         digest(w) == h);
  OK(cd_world_household_move(w, 1, 2, &d));
  size_t attempts = cd_memory_stats().attempts;
  cd_memory_fail_at(1);
  OK(cd_world_birth(w, a, b, "technical birth fixture", &baby, &d));
  cd_memory_fail_at(0);
  assert(cd_memory_stats().attempts == attempts);
  CdResidentView v;
  OK(cd_world_resolve(w, baby, &v, &d));
  assert(v.age == 0 && v.parents[0].person_id == a.person_id &&
         v.parents[1].person_id == b.person_id &&
         v.guardian.person_id == a.person_id && !v.position_known);
  CdCitySummary s;
  OK(cd_world_summary(w, &s, &d));
  assert(s.people == 9 && s.admissions == 1 && s.births == 1);
  CdAdmission bad = {"future", "technical fixture", {1941, 1, 1, 0},
                     1,        CD_RESIDENT,         a};
  h = digest(w);
  assert(cd_world_admit(w, &bad, &baby, &d) == CD_INVALID_ARGUMENT &&
         digest(w) == h);
  OK(cd_world_visit(w, a, 1, 1, &d));
  OK(cd_world_owner(w, 2, a, &d));
  OK(cd_world_place(w, a, 101, &d));
  OK(cd_world_travel(w, a, 103, &d));
  OK(cd_world_step(w, 1, &d));
  assert(w->people[0].held_edge != CD_NONE);
  OK(cd_world_retire(w, a, CD_DEPARTURE, "configured departure", &d));
  assert(cd_world_resolve(w, a, &v, &d) == CD_UNKNOWN_REFERENCE);
  OK(cd_world_resolve(w, child, &v, &d));
  assert(v.guardian.person_id == b.person_id);
  OK(cd_world_resolve(w, b, &v, &d));
  assert(!v.spouse.person_id);
  assert(!w->owners[cd_polygon_index(content, 2)].person_id &&
         !w->visitors[cd_polygon_index(content, 1)]);
  for (unsigned i = 0; i < content->ne; i++)
    assert(!w->traffic[i]);
  assert(cd_world_retire(w, a, CD_DEPARTURE, "duplicate", &d) ==
         CD_UNKNOWN_REFERENCE);
  ledger(w);
  h = digest(w);
  assert(cd_world_retire(w, b, CD_DEATH, "configured death", &d) ==
             CD_INVALID_STATE &&
         digest(w) == h);
  CdAdmission invalid_text = {"\xff", "technical admission", {1900, 1, 1, 0},
                              1,      CD_RESIDENT,           {0}};
  h = digest(w);
  assert(cd_world_admit(w, &invalid_text, &baby, &d) == CD_INVALID_ARGUMENT &&
         digest(w) == h);
  CdSnapshot *snap = NULL;
  OK(cd_snapshot_capture(w, &snap, &d));
  for (;;) {
    size_t found = w->count;
    for (size_t i = 0; i < w->count; i++)
      if (w->people[i].civil.age < 18) {
        found = i;
        break;
      }
    if (found == w->count)
      break;
    OK(cd_world_retire(w, person(w, found), CD_DEPARTURE,
                       "configured family departure", &d));
  }
  while (w->count)
    OK(cd_world_retire(w, person(w, 0), CD_DEPARTURE, "configured departure",
                       &d));
  assert(cd_world_life_count(w) == 9);
  CdSnapshot *empty = NULL;
  OK(cd_snapshot_capture(w, &empty, &d));
  assert(!cd_snapshot_person_count(empty));
  cd_snapshot_close(empty);
  CdPersonView prior;
  OK(cd_snapshot_person(snap, 0, &prior, &d));
  assert(prior.ref.person_id);
  cd_snapshot_close(snap);
  ledger(w);
  CdAdmission arrival = {"Explicit arriving adult",
                         "configured ordinary arrival",
                         {1900, 1, 1, 0},
                         1,
                         CD_RESIDENT,
                         {0}};
  CdPersonRef arriving;
  OK(cd_world_admit(w, &arrival, &arriving, &d));
  assert(w->count == 1);
  ledger(w);
  size_t saved = w->life_count;
  w->life_count = w->life_capacity;
  h = digest(w);
  assert(cd_world_retire(w, arriving, CD_DEPARTURE, "archive full", &d) ==
             CD_CAPACITY &&
         digest(w) == h);
  w->life_count = saved;
  OK(cd_world_step(w, 2, &d));
  cd_world_close(w);
  scenario.initial_people = 29999;
  w = open_world(NULL);
  a = person(w, 0);
  b = person(w, 1);
  OK(cd_world_household_move(w, 1, 2, &d));
  OK(cd_world_birth(w, a, b, "last capacity slot birth", &baby, &d));
  assert(w->count == 30000);
  h = digest(w);
  assert(cd_world_birth(w, a, b, "overflow birth", &baby, &d) == CD_CAPACITY &&
         digest(w) == h);
  cd_world_close(w);
  scenario.initial_people = 8;
  scenario.civil_origin.minute_of_day = 1438;
  w = open_world(NULL);
  CdCitizen *teen = &w->people[2];
  teen->civil.birth = (CdCivilTime){1922, 1, 2, 0};
  teen->civil.age = 17;
  OK(cd_world_step(w, 1, &d));
  assert(teen->civil.age == 18 && !teen->civil.guardian.person_id &&
         w->birthdays == 1);
  cd_world_close(w);
  scenario.civil_origin.minute_of_day = 480;
}
static void cycle15(void) {
  CdWorld *w = open_world(NULL);
  CdPersonRef a = person(w, 0);
  CdNeedsView before, after;
  OK(cd_world_needs(w, a, &before, &d));
  uint64_t h = digest(w);
  OK(cd_world_step(w, 0, &d));
  assert(digest(w) == h);
  OK(cd_world_step(w, 10, &d));
  OK(cd_world_needs(w, a, &after, &d));
  assert(after.minutes == 10 && after.energy == before.energy - 10 &&
         after.fatigue == 10);
  cd_native_energy_set(w->native, a, cd_native_hungry() + 1);
  OK(cd_world_step(w, 1, &d));
  OK(cd_world_needs(w, a, &after, &d));
  assert(after.meals == 1 && after.carried_meals == 2 &&
         after.activity == CD_ACTIVITY_MEAL &&
         after.energy == (int)(cd_native_hungry() + cd_native_hungry() / 2));
  OK(cd_world_rest(w, a, 5, &d));
  OK(cd_world_place(w, a, 101, &d));
  OK(cd_world_travel(w, a, 103, &d));
  CdTravelView t0, t1;
  OK(cd_world_travel_view(w, a, &t0, &d));
  OK(cd_world_step(w, 5, &d));
  OK(cd_world_travel_view(w, a, &t1, &d));
  assert(t0.position.x == t1.position.x);
  arrive(w, a);
  ledger(w);
  w = open_world(w);
  OK(cd_world_step(w, 1440, &d));
  uint64_t batch = digest(w);
  w = open_world(w);
  for (unsigned i = 0; i < 1440; i++)
    OK(cd_world_step(w, 1, &d));
  assert(digest(w) == batch);
  CdCitySummary s;
  OK(cd_world_summary(w, &s, &d));
  assert(s.people == 8);
  ledger(w);
  cd_world_close(w);
  scenario.civil_origin.minute_of_day = 1439;
  w = open_world(NULL);
  a = person(w, 0);
  OK(cd_world_step(w, 4, &d));
  OK(cd_world_needs(w, a, &after, &d));
  assert(after.sleeping && after.activity == CD_ACTIVITY_REST &&
         after.energy == (int)cd_native_full() - 1);
  cd_world_close(w);
  scenario.civil_origin.minute_of_day = 480;
}
static void cycle16(void) {
  CdWorld *w = open_world(NULL);
  CdPersonRef a = person(w, 0), producer = person(w, 4), child = person(w, 2);
  inside(w, a, 1);
  inside(w, producer, 2);
  OK(cd_world_household_move(w, 1, 1, &d));
  OK(cd_world_food_store(w, 2, 1, "technical food producer fixture", &d));
  OK(cd_world_training(w, producer, CD_SKILL_FOOD, "technical qualification",
                       &d));
  CdFoodEndpoint shop = {CD_FOOD_STORE, 2}, home = {CD_FOOD_HOME, 1};
  OK(cd_world_food_import(w, shop, 0, 5, "finite technical raw stock", &d));
  uint64_t h = digest(w);
  assert(cd_world_food_order(w, a, CD_FETCH_MEALS, shop, 3, &d) ==
             CD_INVALID_STATE &&
         digest(w) == h);
  assert(cd_world_food_order(w, child, CD_FETCH_MEALS, shop, 1, &d) ==
         CD_INVALID_STATE);
  OK(cd_world_food_order(w, producer, CD_PRODUCE_MEALS, shop, 3, &d));
  OK(cd_world_step(w, 3, &d));
  CdFoodLedger f;
  OK(cd_world_food_ledger(w, &f, &d));
  assert(f.produced == 3 && f.raw_stock == 2);
  walk(w, a, 1, 2);
  OK(cd_world_food_order(w, a, CD_FETCH_MEALS, shop, 3, &d));
  OK(cd_world_step(w, 3, &d));
  CdNeedsView n;
  OK(cd_world_needs(w, a, &n, &d));
  assert(n.carried_meals == 6);
  walk(w, a, 2, 1);
  OK(cd_world_food_order(w, a, CD_DELIVER_MEALS, home, 3, &d));
  OK(cd_world_step(w, 3, &d));
  assert(w->home_meals[0] == 3);
  inside(w, child, 1);
  CdPersonRef young = person(w, 3);
  inside(w, young, 1);
  assert(cd_world_feed(w, child, young, &d) == CD_INVALID_ARGUMENT);
  unsigned before = w->people[3].meals;
  OK(cd_world_feed(w, a, young, &d));
  assert(w->people[3].meals == before);
  OK(cd_world_step(w, 1, &d));
  assert(w->people[3].meals == before + 1);
  cd_native_energy_set(w->native, young, cd_native_hungry() + 1);
  OK(cd_world_step(w, 1, &d));
  assert(w->people[3].eaten == 1 && w->people[0].activity == CD_ACTIVITY_CARE);

  ledger(w);
  OK(cd_world_food_order(w, a, CD_FETCH_MEALS, home, 5, &d));
  OK(cd_world_step(w, 5, &d));
  assert(w->people[0].food_remaining == 2);
  OK(cd_world_food_cancel(w, a, &d));
  assert(!w->people[0].food_remaining);
  ledger(w);
  cd_world_close(w);
}
static void cycle17(void) {
  CdWorld *w = open_world(NULL);
  CdPersonRef a = person(w, 0), b = person(w, 1), child = person(w, 2),
              young = person(w, 3);
  inside(w, a, 1);
  inside(w, b, 1);
  inside(w, child, 1);
  inside(w, young, 1);
  OK(cd_world_training(w, a, CD_SKILL_CARE, "technical care qualification",
                       &d));
  OK(cd_world_training(w, b, CD_SKILL_CARE, "technical care qualification",
                       &d));
  OK(cd_world_clinic(w, 1, 1, 30,
                     "technical clinic fixture, abstract medicine units", &d));
  OK(cd_world_expose(w, child, CD_ILL, 500, 10, "configured illness experiment",
                     &d));
  OK(cd_world_expose(w, young, CD_INJURED, 300, 100,
                     "configured injury experiment", &d));
  OK(cd_world_care(w, a, child, 1, 0, &d));
  uint64_t h = digest(w);
  assert(cd_world_care(w, b, young, 1, 0, &d) == CD_CAPACITY && digest(w) == h);
  OK(cd_world_step(w, 2, &d));
  CdHealthView health;
  OK(cd_world_health(w, child, &health, &d));
  assert(health.condition == CD_HEALTHY && health.care_minutes == 2 &&
         !health.clinic);
  assert(w->medicine_used == 2 &&
         !w->premises[cd_polygon_index(content, 1)].occupied);
  OK(cd_world_care(w, a, b, 1, 60, &d));
  OK(cd_world_step(w, 1, &d));
  OK(cd_world_expose(w, b, CD_ILL, 1000, 100, "technical prevention experiment",
                     &d));
  OK(cd_world_health(w, b, &health, &d));
  assert(health.severity == 500);
  OK(cd_world_step(w, 100, &d));
  OK(cd_world_health(w, b, &health, &d));
  assert(health.condition == CD_HEALTHY);
  OK(cd_world_expose(w, b, CD_INJURED, 1000, 120,
                     "technical fatal injury experiment", &d));
  OK(cd_world_step(w, 60, &d));
  CdResidentView v;
  assert(cd_world_resolve(w, b, &v, &d) == CD_UNKNOWN_REFERENCE);
  CdCitySummary summary;
  OK(cd_world_summary(w, &summary, &d));
  assert(summary.deaths == 1 && summary.people == 7);
  assert(cd_world_expose(w, b, CD_ILL, 1, 1, "retired actor", &d) != CD_OK);
  assert(w->medicine_imported == 30 && w->medicine_used == 3);
  CdClinicView clinic;
  OK(cd_world_clinic_view(w, 1, &clinic, &d));
  assert(clinic.medicine == 27 && clinic.used == 3 && !clinic.occupied);
  ledger(w);
  cd_world_close(w);
}
static void cycle18(void) {
  CdWorld *w = open_world(NULL);
  CdPersonRef a = person(w, 0), child = person(w, 2);
  size_t enabled = 0, disabled = 0;
  int eligible;
  for (size_t i = 0; i < cd_occupation_count(); i++) {
    CdOccupationView role;
    OK(cd_occupation(i, &role, &d));
    assert(role.id == i + 1 && role.basis[0] && role.output[0]);
    if (role.enabled) {
      enabled++;
      assert(role.minimum_age >= 18 && role.skills);
    } else {
      disabled++;
      OK(cd_world_occupation_eligible(w, a, role.id, &eligible, &d));
      assert(!eligible);
    }
  }
  assert(enabled == 8 && disabled == 58);
  OK(cd_world_occupation_eligible(w, a, 1, &eligible, &d));
  assert(!eligible);
  OK(cd_world_training(w, a, 15, "explicit technical skill fixture", &d));
  OK(cd_world_occupation_eligible(w, a, 1, &eligible, &d));
  assert(eligible);
  assert(cd_world_training(w, child, 15, "child fixture", &d) ==
         CD_INVALID_ARGUMENT);
  CdResidentView v;
  OK(cd_world_resolve(w, a, &v, &d));
  assert(!v.affiliation_known);
  cd_world_close(w);
}
static int name_compare(const void *a, const void *b) { return strcmp(a, b); }
static void name_fixture(void) {
  scenario.initial_people = 27000;
  CdWorld *w = open_world(NULL);
  char (*names)[128] = cd_memory_calloc(27000, 128, CD_MEM_TEMP);
  assert(names);
  char first[64][128], surname[128] = {0};
  for (size_t i = 0; i < 27000; i++) {
    CdResidentView p;
    OK(cd_world_resident(w, i, &p, &d));
    assert(p.name[0] && !strstr(p.name, "fixture") && strchr(p.name, ' '));
    snprintf(names[i], 128, "%s", p.name);
    if (i < 64)
      snprintf(first[i], 128, "%s", p.name);
    const char *family = strchr(p.name, ' ');
    if (i % 4 == 0)
      snprintf(surname, sizeof surname, "%s", family);
    else
      assert(!strcmp(surname, family));
  }
  qsort(names, 27000, 128, name_compare);
  size_t unique = 1;
  for (size_t i = 1; i < 27000; i++)
    unique += strcmp(names[i - 1], names[i]) != 0;
  assert(unique >= 25000);
  cd_memory_free(names);
  w = open_world(w);
  for (size_t i = 0; i < 64; i++) {
    CdResidentView p;
    OK(cd_world_resident(w, i, &p, &d));
    assert(!strcmp(first[i], p.name));
  }
  scenario.seed++;
  w = open_world(w);
  CdResidentView p;
  OK(cd_world_resident(w, 0, &p, &d));
  assert(strcmp(first[0], p.name));
  cd_world_close(w);
  scenario.seed = 1940;
  scenario.initial_people = 8;
  printf("{\"population\":27000,\"unique_names\":%zu,\"deterministic\":true,"
         "\"seed_sensitive\":true}\n",
         unique);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  int cycle = atoi(argv[1]);
  OK(cd_content_open("build/fixtures/human.bin", &content, &d));
  switch (cycle) {
  case 0:
    name_fixture();
    break;
  case 14:
    cycle14();
    break;
  case 15:
    cycle15();
    break;
  case 16:
    cycle16();
    break;
  case 17:
    cycle17();
    break;
  case 18:
    cycle18();
    break;
  default:
    abort();
  }
  cd_content_close(content);
  assert(!cd_memory_stats().total);
  printf("{\"cycle\":%d,\"ok\":true,\"closed_bytes\":0}\n", cycle);
}
