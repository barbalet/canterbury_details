#include "cd_city_internal.h"
#include "cd_native.h"
#include "cd_dsl_catalogue.h"
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
static void refined_work(void) {
  CdWorld *w = world();
  CdPersonRef worker = person(w, 0);
  OK(cd_world_place(w, worker, 110, &d));
  CdEntityId job = carpenter(w, worker);
  uint64_t id;
  uint64_t before = hash(w);
  assert(cd_world_refined_work_submit(w, worker, 1, job, "cut_timber", 1,
                                      "manual trial", &id, &d) == CD_CAPACITY);
  assert(hash(w) == before);
  OK(cd_world_dsl_import(w, 2, "timber", 1000, "manual trial", &d));
  OK(cd_world_dsl_import(w, 2, "carpentry_tool_set", 1, "manual trial", &d));
  OK(cd_world_refined_work_submit(w, worker, 1, job, "cut_timber", 1,
                                  "manual trial", &id, &d));
  step(w, 2);
  CdWorkView task_view;
  OK(cd_world_work(w, worker, &task_view, &d));
  assert(!task_view.progress);
  OK(cd_world_work_cancel(w, worker, id, &d));
  CdDslStockView stock;
  OK(cd_world_dsl_stock(w, 2, "timber", &stock, &d));
  assert(stock.available == 1000 && stock.reserved == 0);
  OK(cd_world_enter(w, worker, 303, 0, 0, &d));
  arrive(w, worker);
  OK(cd_world_refined_work_submit(w, worker, 2, job, "cut_timber", 1,
                                  "manual trial", &id, &d));
  step(w, 4);
  OK(cd_world_work(w, worker, &task_view, &d));
  assert(task_view.progress == 4);
  uint64_t saved = hash(w);
  OK(cd_world_save(w, "build/refined-work.city", &d));
  OK(cd_world_load(&w, content, "build/refined-work.city", &d));
  assert(hash(w) == saved);
  worker = person(w, 0);
  step(w, 16);
  OK(cd_world_work(w, worker, &task_view, &d));
  assert(task_view.state == CD_TASK_DONE && task_view.paid == 40);
  OK(cd_world_dsl_stock(w, 2, "timber", &stock, &d));
  assert(stock.available == 850 && stock.reserved == 0);
  OK(cd_world_dsl_stock(w, 2, "wood_offcuts", &stock, &d));
  assert(stock.available == 150);
  OK(cd_world_dsl_stock(w, 2, "carpentry_tool_set", &stock, &d));
  assert(stock.available == 1 && stock.reserved == 0);
  assert(cd_dsl_stock_valid(w));
  ledgers(w);
  cd_world_close(w);
}

static void refined_cloth(void) {
  CdWorld *w = world();
  CdPersonRef worker = person(w, 0);
  inside(w, worker, 2);
  OK(cd_world_training(w, worker, CD_SKILL_CRAFT, "manual trial", &d));
  OK(cd_world_employer(w, 2, "Technical cloth workshop", 8, "manual trial", &d));
  OK(cd_world_money_import(w, company(2), 1000, "manual trial", &d));
  CdEntityId job = vacancy(w, 2, 4, 480, 1020, 0);
  OK(cd_world_job_assign(w, job, worker, (CdPersonRef){0}, 0, &d));
  OK(cd_world_dsl_import(w, 2, "woven_cloth", 1000, "manual trial", &d));
  OK(cd_world_dsl_import(w, 2, "hand_tool", 1, "manual trial", &d));
  uint64_t work;
  assert(cd_world_refined_work_submit(w, worker, 1, job, "cut_timber", 1,
                                      "manual trial", &work, &d) == CD_UNSUPPORTED);
  OK(cd_world_refined_work_submit(w, worker, 1, job, "cut_cloth", 1,
                                  "manual trial", &work, &d));
  step(w, 8);
  CdWorkView view;
  OK(cd_world_work(w, worker, &view, &d));
  assert(view.state == CD_TASK_DONE && view.paid == 16);
  CdDslStockView stock;
  OK(cd_world_dsl_stock(w, 2, "woven_cloth", &stock, &d));
  assert(stock.available == 900 && stock.reserved == 0);
  OK(cd_world_dsl_stock(w, 2, "textile_offcuts", &stock, &d));
  assert(stock.available == 100);
  assert(cd_dsl_stock_valid(w));
  cd_world_close(w);
}

static void refined_delivery(void) {
  CdWorld *w = world();
  CdPersonRef carrier = person(w, 0);
  inside(w, carrier, 1);
  OK(cd_world_dsl_import(w, 1, "timber", 1200, "delivery trial", &d));
  assert(cd_world_dsl_cargo_order(w, carrier, 2, "timber", 1000, 1, &d) ==
         CD_INVALID_ARGUMENT);
  OK(cd_world_dsl_cargo_order(w, carrier, 1, "timber", 1000, 0, &d));
  OK(cd_world_dsl_cargo_cancel(w, carrier, &d));
  step(w, 1);
  CdDslCargoView cargo;
  OK(cd_world_dsl_cargo(w, carrier, &cargo, &d));
  assert(!cargo.carried);
  OK(cd_world_dsl_cargo_order(w, carrier, 1, "timber", 1000, 0, &d));
  step(w, 1);
  OK(cd_world_dsl_cargo(w, carrier, &cargo, &d));
  assert(cargo.carried == 1000 && !cargo.pending_quantity);
  uint64_t saved = hash(w);
  OK(cd_world_save(w, "build/refined-delivery.city", &d));
  OK(cd_world_load(&w, content, "build/refined-delivery.city", &d));
  assert(hash(w) == saved);
  carrier = person(w, 0);
  OK(cd_world_exit(w, carrier, 301, 0, 0, &d));
  arrive(w, carrier);
  OK(cd_world_travel(w, carrier, 110, &d));
  arrive(w, carrier);
  OK(cd_world_enter(w, carrier, 303, 0, 0, &d));
  arrive(w, carrier);
  OK(cd_world_dsl_cargo_order(w, carrier, 2, "timber", 1000, 1, &d));
  step(w, 1);
  OK(cd_world_dsl_cargo(w, carrier, &cargo, &d));
  assert(!cargo.carried && !cargo.pending_quantity);
  CdDslStockView stock;
  OK(cd_world_dsl_stock(w, 1, "timber", &stock, &d));
  assert(stock.available == 200);
  OK(cd_world_dsl_stock(w, 2, "timber", &stock, &d));
  assert(stock.available == 1000);
  CdEntityId job = carpenter(w, carrier);
  OK(cd_world_dsl_import(w, 2, "carpentry_tool_set", 1, "delivery trial", &d));
  uint64_t work;
  OK(cd_world_refined_work_submit(w, carrier, 1, job, "cut_timber", 1,
                                  "delivered timber", &work, &d));
  OK(cd_world_save(w, "build/refined-chain.city", &d));
  step(w, 20);
  uint64_t completed = hash(w);
  CdWorkView completed_work;
  OK(cd_world_work(w, carrier, &completed_work, &d));
  assert(completed_work.id == work && completed_work.state == CD_TASK_DONE &&
         completed_work.paid == 40);
  OK(cd_world_load(&w, content, "build/refined-chain.city", &d));
  for (unsigned minute = 0; minute < 20; ++minute)
    step(w, 1);
  assert(hash(w) == completed);
  OK(cd_world_dsl_stock(w, 2, "timber", &stock, &d));
  assert(stock.available == 850 && stock.reserved == 0);
  OK(cd_world_dsl_stock(w, 2, "wood_offcuts", &stock, &d));
  assert(stock.available == 150);
  assert(cd_dsl_stock_valid(w));
  ledgers(w);
  cd_world_close(w);
}

static void finish_trade(CdWorld *w, CdPersonRef worker, uint64_t id) {
  for (unsigned minute = 0; minute < 120; ++minute) {
    CdWorkView work;
    OK(cd_world_work(w, worker, &work, &d));
    assert(work.id == id && work.state != CD_TASK_CANCELLED);
    if (work.state == CD_TASK_DONE) {
      assert(work.paid && work.progress == work.duration);
      return;
    }
    step(w, 1);
  }
  abort();
}

static void named_trade_roles(void) {
  static const struct { const char *family, *recipe, *use; } trials[] = {
      {"brewer", "brew_small_beer", "industrial"},
      {"plumber", "fit_pipe", "workshop"},
      {"electrician", "form_electrical_fitting", "workshop"},
      {"motor_mechanic", "recondition_vehicle_part", "workshop"},
      {"potter", "form_pottery", "industrial"},
      {"bricklayer", "prepare_brickwork", "workshop"},
      {"stonemason", "dress_stone", "workshop"},
      {"roofer", "cut_roofing", "workshop"},
      {"plasterer", "mix_plaster", "workshop"},
      {"painter_decorator", "prepare_paint", "workshop"},
      {"crop_farming", "clean_harvest", "agricultural"},
      {"livestock_farming", "sort_eggs", "agricultural"},
      {"dairy", "churn_butter", "agricultural"},
      {"horticulture", "grade_vegetables", "agricultural"},
      {"woodland", "split_firewood", "agricultural"},
      {"baker", "bake_bread", "commercial"},
      {"hotel_service", "cook_meal", "commercial"},
      {"licensed_hospitality", "cook_meal", "commercial"},
      {"kitchen", "cook_meal", "commercial"},
      {"laundry", "wash_linen", "industrial"},
      {"domestic_service", "cook_meal", "residential"},
      {"miller", "mill_wheat", "industrial"},
      {"carpenter", "cut_timber", "workshop"},
      {"weaver", "cut_cloth", "workshop"},
      {"printer", "cut_paper", "workshop"},
      {"metalworker", "form_metalwork", "workshop"},
      {"joiner", "cut_timber", "workshop"},
      {"dressmaker", "cut_cloth", "workshop"},
      {"shoemaker", "trial_trim_leather", "workshop"},
      {"butcher", "trial_portion_meat", "commercial"},
      {"fishmonger", "trial_portion_fish", "commercial"},
  };
  cd_content_close(content);
  scenario.initial_people = 12;
  for (size_t trial = 0; trial < sizeof trials / sizeof trials[0]; ++trial) {
    char path[96];
    snprintf(path, sizeof path, "build/fixtures/dsl-%s.bin", trials[trial].use);
    content = NULL;
    OK(cd_content_open(path, &content, &d));
    CdWorld *w = world();
    CdPersonRef workers[5] = {person(w, 0), person(w, 1), person(w, 4),
                              person(w, 5), person(w, 8)};
    CdEntityId jobs[5] = {0};
    CdJobSpec spec = {2, 303, 0, 0, 480, 1020, 2, 0, 0, "named trade trial"};
    char names[5][48];
    for (unsigned stage = 0; stage < 5; ++stage)
      snprintf(names[stage], sizeof names[stage], "%s.%u", trials[trial].family, stage + 1);
    OK(cd_world_employer(w, 2, "Technical workshop", 8, "named trade trial", &d));
    assert(cd_world_dsl_job_create(w, &spec, names[1], &jobs[1], &d) == CD_UNSUPPORTED);
    OK(cd_world_money_import(w, company(2), 10000, "named trade trial", &d));
    for (size_t stage = 0; stage < 5; ++stage) {
      inside(w, workers[stage], 2);
      OK(cd_world_dsl_job_create(w, &spec, names[stage], &jobs[stage], &d));
    }
    assert(cd_world_job_assign(w, jobs[0], workers[0], workers[1], 0, &d) != CD_OK);
    for (size_t stage = 1; stage < 5; ++stage) {
      assert(cd_world_job_assign(w, jobs[stage], workers[stage], (CdPersonRef){0}, 0, &d) != CD_OK);
      if (cd_dsl_role(names[stage])->independent)
        OK(cd_world_dsl_credential(w, workers[stage], names[stage], "independent fixture credential", &d));
      else
        OK(cd_world_dsl_certify(w, workers[stage], names[stage], "technical assessment", &d));
      if (cd_dsl_role(names[stage])->owner) {
        assert(cd_world_job_assign(w, jobs[stage], workers[stage], (CdPersonRef){0}, 0, &d) != CD_OK);
        OK(cd_world_owner(w, 2, workers[stage], &d));
      }
      OK(cd_world_job_assign(w, jobs[stage], workers[stage], (CdPersonRef){0}, 0, &d));
    }
    assert(cd_world_job_assign(w, jobs[0], workers[0], workers[1], 0, &d) != CD_OK);
    OK(cd_world_job_assign(w, jobs[0], workers[0], workers[2], 0, &d));
    const CdDslRecipe *recipe = cd_dsl_recipe(trials[trial].recipe);
    uint64_t id, before = hash(w);
    assert(cd_world_refined_work_submit(w, workers[1], 1, jobs[1], recipe->id, 1,
                                        "missing materials", &id, &d) == CD_CAPACITY);
    assert(hash(w) == before);
    for (size_t i = 0; i < recipe->input_count; ++i)
      OK(cd_world_dsl_import(w, 2, recipe->inputs[i].resource,
                              recipe->inputs[i].quantity * 6, "trade stock", &d));
    before = hash(w);
    assert(cd_world_refined_work_submit(w, workers[1], 1, jobs[1], recipe->id, 1,
                                        "missing equipment", &id, &d) == CD_CAPACITY);
    assert(hash(w) == before);
    for (size_t i = 0; i < recipe->equipment_count; ++i)
      OK(cd_world_dsl_import(w, 2, recipe->equipment[i], 1, "trade equipment", &d));
    uint64_t funds;
    OK(cd_world_balance(w, company(2), &funds, &d));
    OK(cd_world_money_transfer(w, company(2), account(workers[4]), funds, &d));
    before = hash(w);
    assert(cd_world_refined_work_submit(w, workers[1], 1, jobs[1], recipe->id, 1,
                                        "unfunded work", &id, &d) == CD_CAPACITY);
    assert(hash(w) == before);
    OK(cd_world_money_transfer(w, account(workers[4]), company(2), funds, &d));
    if (trial == 0) {
      before = hash(w);
      OK(cd_world_save(w, "build/named-trade.city", &d));
      OK(cd_world_load(&w, content, "build/named-trade.city", &d));
      assert(hash(w) == before);
      unsigned indices[] = {0, 1, 4, 5, 8};
      for (size_t stage = 0; stage < 5; ++stage)
        workers[stage] = person(w, indices[stage]);
    }
    for (size_t stage = 0; stage < 3; ++stage) {
      OK(cd_world_refined_work_submit(w, workers[stage], 1, jobs[stage], recipe->id, 1,
                                      "named trade trial", &id, &d));
      finish_trade(w, workers[stage], id);
    }
    assert(cd_world_dsl_supervise(w, workers[1], workers[2], 2, jobs[2], recipe->id,
                                  1, "unauthorized dispatch", &id, &d) == CD_UNSUPPORTED);
    OK(cd_world_dsl_supervise(w, workers[3], workers[2], 2, jobs[2], recipe->id,
                                1, "supervised dispatch", &id, &d));
    finish_trade(w, workers[2], id);
    if (cd_dsl_role(names[4])->owner) {
      OK(cd_world_money_import(w, account(workers[4]), 100, "technical capital", &d));
      uint64_t treasury;
      OK(cd_world_balance(w, company(2), &treasury, &d));
      assert(cd_world_dsl_fund(w, workers[3], 2, 20, &d) == CD_UNSUPPORTED);
      OK(cd_world_dsl_fund(w, workers[4], 2, 40, &d));
      uint64_t after;
      OK(cd_world_balance(w, company(2), &after, &d));
      assert(after == treasury + 40);
    } else {
      OK(cd_world_dsl_supervise(w, workers[4], workers[1], 2, jobs[1], recipe->id,
                                  1, "manager dispatch", &id, &d));
      finish_trade(w, workers[1], id);
    }
    if (cd_dsl_role_repairs(cd_dsl_role(names[1]))) {
      materials(w);
      OK(cd_world_wear(w, 2, 200, "Trade repair trial wear", &d));
      CdDevelopmentSpec repair = {CD_DEV_REPAIR, 0, 4, NULL, "Named trade repair"};
      uint64_t project;
      OK(cd_world_project(w, 2, 1, &repair, &project, &d));
      CdWorkSpec repair_task = {jobs[1], project, CD_RECIPE_DEVELOPMENT, 4, "Named trade repair"};
      OK(cd_world_work_submit(w, workers[1], 3, &repair_task, &id, &d));
      finish_trade(w, workers[1], id);
      CdPremisesView premises;
      OK(cd_world_premises(w, 2, &premises, &d));
      assert(premises.condition == 1000);
    }
    assert(cd_dsl_stock_valid(w));
    ledgers(w);
    cd_world_close(w);
    cd_content_close(content);
    content = NULL;
  }
  scenario.initial_people = 8;
}

static void staffed_day(void) {
  cd_content_close(content);
  content = NULL;
  OK(cd_content_open("build/fixtures/staffed-day.bin", &content, &d));
  CdWorld *w = world();
  uint64_t shortage = hash(w);
  assert(cd_world_households_allocate(w, &d) == CD_CAPACITY);
  assert(hash(w) == shortage);
  cd_world_close(w);
  scenario.initial_people = 4;
  w = world();
  CdPersonRef worker = person(w, 0), administrator = person(w, 1);
  OK(cd_world_households_allocate(w, &d));
  for (unsigned i = 0; i < 4; ++i)
    inside(w, person(w, i), 1);
  OK(cd_world_food_import(w, (CdFoodEndpoint){CD_FOOD_HOME, 1}, 16, 0,
                           "staffed day meals", &d));
  OK(cd_world_training(w, worker, CD_SKILL_CRAFT, "staffed day fixture", &d));
  OK(cd_world_employer(w, 2, "Day workshop", 8, "staffed day fixture", &d));
  OK(cd_world_money_import(w, company(2), 10000, "staffed day payroll", &d));
  CdEntityId trade = vacancy(w, 2, 3, 480, 600, 0);
  OK(cd_world_job_assign(w, trade, worker, (CdPersonRef){0}, 0, &d));
  OK(cd_world_training(w, administrator, CD_SKILL_CRAFT, "staffed day fixture", &d));
  OK(cd_world_authorize(w, administrator, 63, "staffed day permissions", &d));
  CdEntityId office = vacancy(w, 2, 8, 480, 600, 0);
  OK(cd_world_job_assign(w, office, administrator, (CdPersonRef){0}, 0, &d));
  CdRosterSpec roster = {office, 1u << CD_SERVICE_ADMIN, "Day office", "staffed day fixture"};
  CdEntityId provider;
  OK(cd_world_roster(w, &roster, &provider, &d));
  CdServiceSpec request = {provider, 0, CD_SERVICE_ADMIN, 2, {0},
                           cd_now(w) + 90, 3, "staffed day request"};
  uint64_t service;
  OK(cd_world_dispatch(w, 1, &request, &service, &d));
  OK(cd_world_dsl_import(w, 2, "timber", 1000, "staffed day stock", &d));
  OK(cd_world_dsl_import(w, 2, "carpentry_tool_set", 1, "staffed day tools", &d));
  uint64_t work;
  OK(cd_world_refined_work_submit(w, worker, 1, trade, "cut_timber", 1,
                                  "staffed day demand", &work, &d));
  OK(cd_world_dsl_import(w, 1, "wood_offcuts", 100, "staffed day pickup", &d));
  OK(cd_world_dsl_cargo_order(w, worker, 1, "wood_offcuts", 100, 0, &d));
  step(w, 1);
  CdDslCargoView cargo;
  OK(cd_world_dsl_cargo(w, worker, &cargo, &d));
  assert(cargo.carried == 100 && !cargo.pending_quantity);
  assert(cd_citizen(w, worker)->inside == cd_polygon_index(content, 1));
  CdServiceView waiting;
  OK(cd_world_service(w, service, &waiting, &d));
  assert(waiting.state == CD_TASK_WAITING && !waiting.progress &&
         strstr(waiting.reason, "qualification"));
  OK(cd_world_training(w, administrator, CD_SKILL_CRAFT | CD_SKILL_COMMUNITY,
                        "staffed day administrative qualification", &d));

  CdClockView clock;
  OK(cd_world_clock(w, &clock, &d));
  uint64_t before = hash(w);
  CdSnapshot *snapshot = NULL;
  OK(cd_snapshot_capture(w, &snapshot, &d));
  cd_snapshot_close(snapshot);
  assert(hash(w) == before);
  OK(cd_world_save(w, "build/staffed-day.city", &d));

  /* Inspect return journeys before the following day's shift begins. */
  step(w, 360);
  CdWorkView production;
  OK(cd_world_work(w, worker, &production, &d));
  assert(production.id == work && production.state == CD_TASK_DONE && production.paid == 40);
  CdServiceView delivered;
  OK(cd_world_service(w, service, &delivered, &d));
  if (delivered.state != CD_TASK_DONE || delivered.progress != 3 || delivered.paid != 6)
    fprintf(stderr, "Staffed day service state %u, progress %u, pay %llu, reason %s\n",
            (unsigned)delivered.state, delivered.progress,
            (unsigned long long)delivered.paid, delivered.reason);
  assert(delivered.state == CD_TASK_DONE && delivered.progress == 3 && delivered.paid == 6);
  assert(cd_citizen(w, worker)->inside == cd_polygon_index(content, 1));
  assert(cd_citizen(w, administrator)->inside == cd_polygon_index(content, 1));
  CdDslStockView stock;
  OK(cd_world_dsl_stock(w, 2, "wood_offcuts", &stock, &d));
  assert(stock.available == 150 && !stock.reserved);
  assert(cd_dsl_stock_valid(w));
  ledgers(w);
  step(w, 1080);
  uint64_t expected = hash(w);
  CdClockView end;
  OK(cd_world_clock(w, &end, &d));
  assert(end.native_minutes == clock.native_minutes + 1440);
  ledgers(w);

  OK(cd_world_load(&w, content, "build/staffed-day.city", &d));
  for (unsigned i = 0; i < 1440; ++i)
    step(w, 1);
  assert(hash(w) == expected);
  cd_world_close(w);
  scenario.initial_people = 8;
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
  case 24:
    refined_work();
    refined_cloth();
    refined_delivery();
    break;
  case 25:
    named_trade_roles();
    break;
  case 26:
    staffed_day();
    break;
  default:
    abort();
  }
  cd_content_close(content);
  assert(!cd_memory_stats().total);
  printf("{\"cycle\":%u,\"ok\":true,\"closed_bytes\":0}\n", cycle);
}
