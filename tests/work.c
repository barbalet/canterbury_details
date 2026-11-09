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
static CdContent *content;
static CdScenario scenario = {
    8, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
static CdWorld *world(void) {
  CdWorld *w = NULL;
  OK(cd_world_open(content, &scenario, &w, &d));
  return w;
}
static CdPersonRef person(CdWorld *w, unsigned i) {
  return w->people[i].civil.ref;
}
static void step(CdWorld *w, unsigned n) { OK(cd_world_step(w, n, &d)); }
static void arrive(CdWorld *w, CdPersonRef p) {
  for (unsigned n = 0; n < 400; n++) {
    CdTravelView v;
    OK(cd_world_travel_view(w, p, &v, &d));
    if (v.state == CD_ROUTE_ARRIVED)
      return;
    step(w, 1);
  }
  abort();
}
static void inside(CdWorld *w, CdPersonRef p, unsigned b) {
  OK(cd_world_place(w, p, b == 1 ? 107 : 110, &d));
  OK(cd_world_enter(w, p, b == 1 ? 301 : 303, 0, 0, &d));
  arrive(w, p);
}
static uint64_t hash(CdWorld *w) {
  uint64_t h;
  OK(cd_world_digest(w, &h, &d));
  return h;
}
static CdAccount company(unsigned b) {
  return (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = b};
}
static CdAccount home(unsigned id) {
  return (CdAccount){.kind = CD_ACCOUNT_HOME, .id = id};
}
static CdAccount account(CdPersonRef p) {
  return (CdAccount){.kind = CD_ACCOUNT_PERSON, .person = p};
}
static void ledgers(CdWorld *w) {
  CdEconomyLedger e;
  CdFoodLedger f;
  OK(cd_world_economy_ledger(w, &e, &d));
  OK(cd_world_food_ledger(w, &f, &d));
  assert(e.money_created == e.money_stock);
}
static CdEntityId vacancy(CdWorld *w, unsigned b, unsigned role, unsigned start,
                          unsigned end, int apprentice) {
  CdJobSpec s = {.workplace = b,
                 .entrance = b == 1 ? 301 : 303,
                 .occupation = role,
                 .shift_start = start,
                 .shift_end = end,
                 .wage = 2,
                 .apprenticeship = apprentice,
                 .basis = "explicit technical fixture"};
  CdEntityId j;
  OK(cd_world_job_create(w, &s, &j, &d));
  return j;
}
static CdEntityId carpenter(CdWorld *w, CdPersonRef p) {
  OK(cd_world_training(w, p, CD_SKILL_CRAFT, "explicit technical fixture", &d));
  OK(cd_world_employer(w, 2, "Technical workshop", 8, "technical fixture", &d));
  OK(cd_world_money_import(w, company(2), 10000, "technical capital", &d));
  CdEntityId j = vacancy(w, 2, 3, 480, 1020, 0);
  OK(cd_world_job_assign(w, j, p, (CdPersonRef){0}, 0, &d));
  return j;
}
static uint64_t task(CdWorld *w, CdPersonRef p, uint64_t key, CdEntityId j,
                     CdRecipe recipe, unsigned units, CdEntityId project) {
  CdWorkSpec s = {j, project, recipe, units, "technical recipe"};
  uint64_t id;
  OK(cd_world_work_submit(w, p, key, &s, &id, &d));
  return id;
}
static void materials(CdWorld *w) {
  for (unsigned g = 0; g < CD_GOODS; g++)
    OK(cd_world_goods_import(w, 2, g, 20, "technical material import", &d));
}
static void cycle19(void) {
  CdWorld *w = world();
  CdPersonRef a = person(w, 0), b = person(w, 1), child = person(w, 2);
  inside(w, a, 1);
  CdEntityId j = carpenter(w, a);
  assert(!w->people[0].inside); // internal index 0 is building 1
  assert(cd_world_job_assign(w, j, b, (CdPersonRef){0}, 0, &d) == CD_CAPACITY);
  CdEntityId childjob = vacancy(w, 2, 3, 480, 1020, 0);
  assert(cd_world_job_assign(w, childjob, child, (CdPersonRef){0}, 0, &d) !=
         CD_OK);
  CdEntityId second = vacancy(w, 2, 3, 1020, 1200, 0);
  OK(cd_world_job_assign(w, second, a, (CdPersonRef){0}, 1, &d));
  OK(cd_world_job_unassign(w, second, &d));
  CdEntityId overlap = vacancy(w, 2, 3, 600, 1100, 0);
  assert(cd_world_job_assign(w, overlap, a, (CdPersonRef){0}, 1, &d) ==
         CD_INVALID_ARGUMENT);
  OK(cd_world_place(w, b, 105, &d));
  OK(cd_world_training(w, b, CD_SKILL_CRAFT, "fixture", &d));
  assert(cd_world_job_assign(w, childjob, b, (CdPersonRef){0}, 0, &d) ==
         CD_INVALID_STATE);
  CdPersonRef far = person(w, 4);
  CdEntityId unskilled = vacancy(w, 2, 3, 0, 0, 0);
  OK(cd_world_place(w, far, 110, &d));
  assert(cd_world_job_assign(w, unskilled, far, (CdPersonRef){0}, 0, &d) ==
         CD_INVALID_ARGUMENT);
  // Explicit 16-year-old admission, qualified live mentor, same reachable
  // premises.
  OK(cd_world_household_move(w, 1, 1, &d));
  OK(cd_world_household_move(w, 2, 2, &d));
  CdAdmission ad = {"Apprentice Fixture", "technical admission",
                    {1924, 1, 1, 0},      2,
                    CD_RESIDENT,          far};
  CdPersonRef ap;
  OK(cd_world_admit(w, &ad, &ap, &d));
  inside(w, ap, 2);
  CdEntityId apprentice = vacancy(w, 2, 3, 480, 1020, 1);
  OK(cd_world_job_assign(w, apprentice, ap, a, 0, &d));
  for (unsigned i = 0; i < 250 && w->people[0].inside != 1; i++)
    step(w, 1);
  assert(w->people[0].inside == 1); // traversed real source street/door
  materials(w);
  uint64_t apprentice_task = task(w, ap, 1, apprentice, CD_RECIPE_TOOL, 1, 0);
  step(w, 1);
  assert(cd_citizen(w, ap)->work.view.progress == 1);
  assert(w->people[0].activity == CD_ACTIVITY_WORK);
  OK(cd_world_job_unassign(w, j, &d));
  assert(cd_citizen(w, ap)->work.view.state == CD_TASK_CANCELLED);
  OK(cd_world_work_cancel(w, ap, apprentice_task, &d));
  CdJobView v;
  OK(cd_world_job(w, apprentice, &v, &d));
  assert(!v.worker.person_id);
  OK(cd_world_job_close(w, j, &d));
  OK(cd_world_job(w, j, &v, &d));
  assert(!v.open);
  ledgers(w);
  cd_world_close(w);
  scenario.civil_origin.minute_of_day = 1380;
  w = world();
  a = person(w, 0);
  OK(cd_world_place(w, a, 110, &d));
  OK(cd_world_training(w, a, CD_SKILL_CRAFT, "fixture", &d));
  OK(cd_world_employer(w, 2, "Night workshop", 2, "fixture", &d));
  j = vacancy(w, 2, 3, 1320, 360, 0);
  OK(cd_world_job_assign(w, j, a, (CdPersonRef){0}, 0, &d));
  step(w, 1);
  assert(!w->people[0].sleeping);
  cd_world_close(w);
  scenario.civil_origin.minute_of_day = 480;
}
static uint64_t run20(int batch) {
  CdWorld *w = world();
  CdPersonRef a = person(w, 0);
  inside(w, a, 2);
  CdEntityId j = carpenter(w, a);
  materials(w);
  uint64_t id = task(w, a, 1, j, CD_RECIPE_TOOL, 2, 0);
  uint64_t h = hash(w), retry = task(w, a, 1, j, CD_RECIPE_TOOL, 2, 0);
  assert(id == retry && hash(w) == h);
  CdWorkSpec conflict = {j, 0, CD_RECIPE_TOOL, 1, "technical recipe"};
  assert(cd_world_work_submit(w, a, 1, &conflict, &retry, &d) ==
         CD_INVALID_ARGUMENT);
  step(w, 2);
  assert(w->people[0].work.view.progress == 2);
  OK(cd_world_employer_open(w, 2, 0, &d));
  step(w, 3);
  assert(w->people[0].work.view.progress == 2);
  OK(cd_world_employer_open(w, 2, 1, &d));
  OK(cd_world_rest(w, a, 2, &d));
  step(w, 2);
  assert(w->people[0].work.view.progress == 2);
  if (batch)
    step(w, 4);
  else
    for (unsigned i = 0; i < 4; i++)
      step(w, 1);
  assert(w->people[0].work.view.state == CD_TASK_DONE &&
         w->economy.produced[CD_TOOL] == 2 && w->economy.wages == 12);
  h = hash(w);
  task(w, a, 1, j, CD_RECIPE_TOOL, 2, 0);
  assert(hash(w) == h);
  cd_human_tick(w);
  assert(hash(w) == h);
  id = task(w, a, 2, j, CD_RECIPE_TOOL, 2, 0);
  step(w, 1);
  OK(cd_world_work_cancel(w, a, id, &d));
  h = hash(w);
  OK(cd_world_work_cancel(w, a, id, &d));
  assert(hash(w) == h && w->people[0].work.view.progress == 1 &&
         w->economy.produced[CD_TOOL] == 2);
  ledgers(w);
  h = hash(w);
  cd_world_close(w);
  return h;
}
static void cycle20(void) {
  assert(run20(0) == run20(1));
  CdWorld *w = world();
  CdPersonRef a = person(w, 0);
  inside(w, a, 1);
  CdEntityId j = carpenter(w, a);
  materials(w);
  task(w, a, 1, j, CD_RECIPE_TOOL, 1, 0);
  step(w, 5);
  assert(!w->people[0].work.view.progress);
  for (unsigned i = 0; i < 300 && w->people[0].work.view.state != CD_TASK_DONE;
       i++)
    step(w, 1);
  assert(w->people[0].work.view.state == CD_TASK_DONE);
  ledgers(w);
  cd_world_close(w);
}
static void cycle21(void) {
  CdWorld *w = world();
  CdPersonRef a = person(w, 0);
  inside(w, a, 2);
  CdEntityId j = carpenter(w, a);
  uint64_t id;
  CdWorkSpec s = {j, 0, CD_RECIPE_TOOL, 1, "technical recipe"};
  uint64_t h = hash(w);
  assert(cd_world_work_submit(w, a, 1, &s, &id, &d) == CD_CAPACITY &&
         hash(w) == h);
  materials(w);
  task(w, a, 1, j, CD_RECIPE_TOOL, 1, 0);
  step(w, 3);
  OK(cd_world_money_import(w, home(1), 100, "household model tokens", &d));
  OK(cd_world_price(w, 2, CD_TOOL, 0, 7, "technical quote", &d));
  OK(cd_world_purchase(w, a, 1, 2, CD_TOOL, 0, 2, &d));
  ledgers(w);
  h = hash(w);
  OK(cd_world_purchase(w, a, 1, 2, CD_TOOL, 0, 2, &d));
  assert(hash(w) == h);
  step(w, 1);
  assert(w->people[0].goods[CD_TOOL] == 2);
  uint64_t balance;
  OK(cd_world_balance(w, home(1), &balance, &d));
  assert(balance == 86);
  OK(cd_world_trade_cancel(w, a, 1, &d));
  assert(w->people[0].trade.view.state == CD_TASK_DONE);
  OK(cd_world_purchase(w, a, 2, 2, CD_TOOL, 0, 1, &d));
  CdTradeView t;
  OK(cd_world_trade(w, a, &t, &d));
  OK(cd_world_trade_cancel(w, a, t.id, &d));
  h = hash(w);
  OK(cd_world_trade_cancel(w, a, t.id, &d));
  assert(hash(w) == h);
  OK(cd_world_money_transfer(w, account(a), home(1), 2, &d));
  OK(cd_world_goods_order(w, a, 2, CD_TOOL, 1, 1, &d));
  step(w, 1);
  assert(w->people[0].goods[CD_TOOL] == 1);
  OK(cd_world_goods_cancel(w, a, &d));
  OK(cd_world_household_move(w, 1, 2, &d));
  OK(cd_world_home_goods_order(w, a, CD_TOOL, 1, 1, &d));
  step(w, 1);
  assert(!w->people[0].goods[CD_TOOL] && w->home_goods[CD_TOOL] == 1);
  OK(cd_world_home_goods_order(w, a, CD_TOOL, 1, 0, &d));
  step(w, 1);
  assert(w->people[0].goods[CD_TOOL] == 1 && !w->home_goods[CD_TOOL]);
  // Meal recipe uses same finite food ledger and reserved raw inputs.
  OK(cd_world_job_unassign(w, j, &d));
  OK(cd_world_training(w, a, CD_SKILL_FOOD, "fixture", &d));
  j = vacancy(w, 2, 2, 480, 1020, 0);
  OK(cd_world_job_assign(w, j, a, (CdPersonRef){0}, 0, &d));
  OK(cd_world_food_store(w, 2, 1, "fixture", &d));
  OK(cd_world_food_import(w, (CdFoodEndpoint){CD_FOOD_STORE, 2}, 1, 3,
                          "fixture", &d));
  task(w, a, 2, j, CD_RECIPE_MEAL, 2, 0);
  ledgers(w);
  step(w, 4);
  assert(w->food.produced == 2);
  ledgers(w);
  OK(cd_world_retire(w, a, CD_DEATH, "technical fixture", &d));
  assert(w->life[0].goods[CD_TOOL] == 1);
  ledgers(w);
  cd_world_close(w);
}
static void cycle22(void) {
  CdWorld *w = world();
  CdPersonRef a = person(w, 0);
  inside(w, a, 2);
  CdEntityId j = carpenter(w, a);
  materials(w);
  CdBuildingView base;
  OK(cd_content_building(content, 1, &base, &d));
  OK(cd_world_wear(w, 2, 1000, "technical wear", &d));
  CdDevelopmentSpec s = {CD_DEV_REPAIR, 0, 4, NULL, "technical repair"};
  uint64_t id;
  OK(cd_world_project(w, 2, 1, &s, &id, &d));
  uint64_t h = hash(w), retry;
  OK(cd_world_project(w, 2, 1, &s, &retry, &d));
  assert(id == retry && hash(w) == h);
  task(w, a, 1, j, CD_RECIPE_DEVELOPMENT, 4, id);
  step(w, 2);
  CdPremisesView v;
  OK(cd_world_premises(w, 2, &v, &d));
  assert(v.condition == 0 && w->projects[1].view.progress == 2);
  OK(cd_world_employer_open(w, 2, 0, &d));
  step(w, 2);
  assert(w->projects[1].view.progress == 2);
  OK(cd_world_employer_open(w, 2, 1, &d));
  step(w, 2);
  OK(cd_world_premises(w, 2, &v, &d));
  assert(v.condition == 1000);
  s = (CdDevelopmentSpec){CD_DEV_CAPACITY, 2, 2, NULL, "technical capacity"};
  OK(cd_world_project(w, 2, 2, &s, &id, &d));
  task(w, a, 2, j, CD_RECIPE_DEVELOPMENT, 2, id);
  step(w, 2);
  assert(w->projects[1].view.state == CD_TASK_WAITING &&
         cd_capacity(w, 1) == 8);
  OK(cd_world_employer(w, 2, "Technical workshop", 1, "fixture", &d));
  step(w, 1);
  assert(cd_capacity(w, 1) == 2 && w->projects[1].view.state == CD_TASK_DONE);
  assert(content->polygons[1].capacity == base.capacity);
  s = (CdDevelopmentSpec){CD_DEV_USE, 0, 2, "technical workshop use",
                          "fixture"};
  OK(cd_world_project(w, 2, 3, &s, &id, &d));
  task(w, a, 3, j, CD_RECIPE_DEVELOPMENT, 2, id);
  step(w, 1);
  OK(cd_world_project_cancel(w, 2, id, &d));
  h = hash(w);
  OK(cd_world_project_cancel(w, 2, id, &d));
  assert(hash(w) == h && !w->premises[1].scenario_use[0]);
  s.kind = CD_DEV_GEOMETRY;
  assert(cd_world_project(w, 2, 4, &s, &id, &d) == CD_UNSUPPORTED);
  ledgers(w);
  cd_world_close(w);
}
static void cycle23(void) {
  CdWorld *w = world();
  assert(cd_world_institution_count(w) == 3);
  CdInstitutionDetail v;
  OK(cd_world_institution(w, 0, &v, &d));
  assert(!strcmp(v.religion, "Christian") && !v.operation_known &&
         v.review_required);
  assert(cd_world_institution_operate(w, v.id, 1, 0, "office", "fixture", &d) ==
         CD_UNSUPPORTED);
  OK(cd_world_institution_link(w, v.id, 2, "explicit fixture premises review",
                               &d));
  assert(cd_world_institution_operate(w, v.id, 1, 0, "office", "fixture", &d) ==
         CD_INVALID_STATE);
  OK(cd_world_institution_operate(w, v.id, 1, 1, "lay office",
                                  "explicit scenario override", &d));
  CdSnapshot *snap = NULL;
  OK(cd_snapshot_capture(w, &snap, &d));
  OK(cd_world_institution_operate(w, v.id, 0, 0, "office", "closure fixture",
                                  &d));
  OK(cd_snapshot_institution_detail(snap, v.id, &v, &d));
  assert(v.operating && v.premises_count == 1 && !v.congregation_known &&
         !v.precinct_boundary_known && !strcmp(v.status, "closed"));
  cd_snapshot_close(snap);
  OK(cd_world_institution(w, 1, &v, &d));
  assert(!strcmp(v.religion, "Jewish") && !v.located);
  assert(cd_world_institution_link(w, v.id, 2, "fixture", &d) ==
         CD_UNSUPPORTED);
  ledgers(w);
  cd_world_close(w);
}
static void source_records(void) {
  CdWorld *w = world();
  printf("{\"sites\":[");
  for (size_t i = 0; i < cd_world_institution_count(w); i++) {
    CdInstitutionDetail v;
    OK(cd_world_institution(w, i, &v, &d));
    assert(!v.operation_known && !v.premises_count && !v.congregation_known &&
           !v.precinct_boundary_known);
    printf("%s%s", i ? "," : "", v.source_record);
  }
  puts("]}");
  cd_world_close(w);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  unsigned cycle = atoi(argv[1]);
  OK(cd_content_open(cycle == 0 ? "build/content/CanterburyCity.bin"
                                : "build/fixtures/work.bin",
                     &content, &d));
  switch (cycle) {
  case 0:
    source_records();
    break;
  case 19:
    cycle19();
    break;
  case 20:
    cycle20();
    break;
  case 21:
    cycle21();
    break;
  case 22:
    cycle22();
    break;
  case 23:
    cycle23();
    break;
  default:
    abort();
  }
  cd_content_close(content);
  assert(!cd_memory_stats().total);
  printf("{\"cycle\":%u,\"ok\":true,\"closed_bytes\":0}\n", cycle);
}
