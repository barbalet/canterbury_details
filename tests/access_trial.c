#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include "cd_scenario_startup.h"
#include <assert.h>
#include <stdlib.h>

static CdDiagnostic diagnostic;
#define OK(call) do {                                                        \
  if ((call) != CD_OK) {                                                       \
    fprintf(stderr, "Line %d: %s\n", __LINE__, diagnostic.message);            \
    abort();                                                                 \
  }                                                                           \
} while (0)

static uint64_t wait_inside(CdWorld *world, CdPersonRef person, uint32_t building,
                            unsigned limit) {
  for (unsigned minute = 0; minute < limit; ++minute) {
    const CdCitizen *resident = cd_citizen(world, person);
    if (resident && resident->inside == building && resident->door == CD_NONE)
      return cd_now(world);
    OK(cd_world_step(world, 1, &diagnostic));
  }
  fprintf(stderr, "Route timed out: %s\n",
          cd_route_reason(cd_citizen(world, person)->reason));
  abort();
}

static void population_placement(CdContent *content) {
  CdWorld *world = NULL;
  CdScenarioStartup startup = {
      {27000, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"},
      {2160, 4050, 1620, 16470, 2700}, {0}};
  OK(cd_world_open_startup(content, &startup, &world, &diagnostic));
  OK(cd_world_households_allocate(world, &diagnostic));
  OK(cd_world_place_households(world, &diagnostic));
  size_t inside = 0;
  unsigned elapsed = 0;
  for (; elapsed <= 100; ++elapsed) {
    inside = 0;
    for (size_t i = 0; i < world->count; ++i) {
      const CdCitizen *person = &world->people[i];
      inside += person->inside != CD_NONE && person->door == CD_NONE &&
                content->polygons[person->inside].id == person->civil.dwelling;
    }
    if (inside == world->count)
      break;
    OK(cd_world_step(world, 1, &diagnostic));
  }
  if (inside != startup.scenario.initial_people)
    fprintf(stderr, "Only %zu residents completed entrance movement\n", inside);
  assert(inside == startup.scenario.initial_people);
  unsigned bands[5] = {0};
  for (size_t i = 0; i < world->count; ++i) {
    unsigned age = world->people[i].civil.age;
    ++bands[age < 5 ? 0 : age < 15 ? 1 : age < 18 ? 2 : age < 65 ? 3 : 4];
  }
  for (size_t i = 0; i < 5; ++i)
    assert(bands[i] == startup.cohorts[i]);
  printf("{\"residents_at_home\":%zu,\"households\":%zu,\"entry_minutes\":%u,\"ok\":true}\n",
         inside, world->home_count, elapsed);
  cd_world_close(world);
}

int main(int argc, char **argv) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  if (argc == 2 && !strcmp(argv[1], "--population")) {
    population_placement(content);
    cd_content_close(content);
    assert(!cd_memory_stats().total);
    return 0;
  }
  CdScenario scenario = {4, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
  OK(cd_world_open(content, &scenario, &world, &diagnostic));
  uint32_t home = CD_NONE, mill = CD_NONE;
  uint32_t entrance = CD_NONE;
  double best = INFINITY;
  for (uint32_t wi = 0; wi < content->nd; ++wi) {
    const CdDoor *work_door = &content->doors[wi];
    const CdPolygon *work = &content->polygons[work_door->building];
    if (!work_door->reviewed || strcmp(work->use, "industrial"))
      continue;
    for (uint32_t hi = 0; hi < content->nd; ++hi) {
      const CdDoor *home_door = &content->doors[hi];
      const CdPolygon *house = &content->polygons[home_door->building];
      if (!home_door->reviewed || house->capacity < 4 ||
          strcmp(house->use, "residential"))
        continue;
      CdRoute route;
      if (cd_world_plan(world, content->nodes[home_door->node].id,
                         content->nodes[work_door->node].id, &route, NULL) == CD_OK &&
          route.count && route.cost < best) {
        best = route.cost;
        home = home_door->building;
        mill = work_door->building;
        entrance = wi;
      }
    }
  }
  assert(home != CD_NONE && mill != CD_NONE && best < 100);
  CdEntityId dwelling = content->polygons[home].id;
  CdEntityId premises = content->polygons[mill].id;
  OK(cd_world_household_move(world, 1, dwelling, &diagnostic));
  OK(cd_world_place_households(world, &diagnostic));
  for (size_t i = 0; i < world->count; ++i)
    wait_inside(world, world->people[i].civil.ref, home, 100);
  CdPersonRef worker = world->people[0].civil.ref;
  uint64_t home_minute = cd_now(world);
  OK(cd_world_food_import(world, (CdFoodEndpoint){CD_FOOD_HOME, 1}, 16, 0,
                          "technical district meals", &diagnostic));
  OK(cd_world_employer(world, premises, "Technical milling post", 2,
                       "industrial footprint trial", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = premises},
                            2000, "technical district payroll", &diagnostic));
  OK(cd_world_dsl_certify(world, worker, "miller.2", "technical milling assessment", &diagnostic));
  CdJobSpec spec = {premises, content->doors[entrance].id, 0, 0, 480, 900,
                    2, 0, 0, "technical district shift"};
  CdEntityId job;
  OK(cd_world_dsl_job_create(world, &spec, "miller.2", &job, &diagnostic));
  CdPersonRef applicants[] = {world->people[1].civil.ref, worker};
  uint64_t before, after;
  OK(cd_world_digest(world, &before, &diagnostic));
  CdEntityId second;
  OK(cd_world_dsl_job_create(world, &spec, "miller.2", &second, &diagnostic));
  OK(cd_world_digest(world, &before, &diagnostic));
  CdEntityId unfunded_staffing[] = {job, second};
  assert(cd_world_staff_jobs(world, unfunded_staffing, 2, applicants, 2, &diagnostic) == CD_CAPACITY);
  OK(cd_world_digest(world, &after, &diagnostic));
  assert(before == after);
  OK(cd_world_job_close(world, second, &diagnostic));
  OK(cd_world_staff_jobs(world, &job, 1, applicants, 2, &diagnostic));
  CdJobView assigned;
  OK(cd_world_job(world, job, &assigned, &diagnostic));
  assert(assigned.worker.person_id == worker.person_id);
  OK(cd_world_dsl_import(world, premises, "wheat_grain", 1000, "technical district stock", &diagnostic));
  OK(cd_world_dsl_import(world, premises, "hand_tool", 1, "technical hand mill equipment", &diagnostic));
  uint64_t task;
  OK(cd_world_refined_work_submit(world, worker, 1, job, "mill_wheat", 1,
                                  "technical district work order", &task, &diagnostic));
  uint64_t arrival = wait_inside(world, worker, mill, 360);
  assert(arrival > home_minute);
  CdWorkView work;
  for (unsigned i = 0; i < 90; ++i) {
    OK(cd_world_work(world, worker, &work, &diagnostic));
    if (work.state == CD_TASK_DONE)
      break;
    OK(cd_world_step(world, 1, &diagnostic));
  }
  assert(work.id == task && work.state == CD_TASK_DONE && work.paid == 60);
  uint64_t returned = wait_inside(world, worker, home, 720);
  assert(returned > arrival);
  CdDslStockView stock;
  OK(cd_world_dsl_stock(world, premises, "flour", &stock, &diagnostic));
  assert(stock.available == 750);
  OK(cd_world_dsl_stock(world, premises, "bran", &stock, &diagnostic));
  assert(stock.available == 250);
  CdEconomyLedger ledger;
  OK(cd_world_economy_ledger(world, &ledger, &diagnostic));
  assert(ledger.money_created == ledger.money_stock && cd_dsl_stock_valid(world));
  printf("{\"households\":1,\"residents\":4,\"jobs\":1,\"home\":\"%s\","
         "\"workplace\":\"%s\",\"arrival_minute\":%llu,\"return_minute\":%llu,"
         "\"flour_g\":750,\"bran_g\":250,\"wages\":60,\"ok\":true}\n",
         content->polygons[home].source, content->polygons[mill].source,
         (unsigned long long)arrival, (unsigned long long)returned);
  cd_world_close(world);
  cd_content_close(content);
  assert(!cd_memory_stats().total);
}
