#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include "cd_scenario_startup.h"
#include <assert.h>
#include <inttypes.h>
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
  unsigned (*demands)[2] = calloc(content->np, sizeof *demands);
  assert(demands);
  for (size_t i = 0; i < world->count; ++i) {
    const CdCitizen *person = &world->people[i];
    unsigned age = person->civil.age;
    demands[person->inside][0] += age >= 18 && age < 65;
    demands[person->inside][1] += age >= 5 && age < 15;
    ++bands[age < 5 ? 0 : age < 15 ? 1 : age < 18 ? 2 : age < 65 ? 3 : 4];
  }
  for (size_t i = 0; i < 5; ++i)
    assert(bands[i] == startup.cohorts[i]);
  printf("{\"residents_at_home\":%zu,\"households\":%zu,\"entry_minutes\":%u,"
         "\"ok\":true,\"demands\":[",
         inside, world->home_count, elapsed);
  const char *separator = "";
  for (uint32_t i = 0; i < content->np; ++i) {
    if (!demands[i][0] && !demands[i][1])
      continue;
    printf("%s[\"%" PRIu64 "\",%u,%u]", separator, (uint64_t)content->polygons[i].id,
           demands[i][0], demands[i][1]);
    separator = ",";
  }
  puts("]}");
  free(demands);
  cd_world_close(world);
}

static void school_journey(CdContent *content) {
  CdWorld *world = NULL;
  CdScenarioStartup startup = {
      {4, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"},
      {1, 1, 0, 2, 0}, {0}};
  OK(cd_world_open_startup(content, &startup, &world, &diagnostic));
  uint32_t home_door = CD_NONE, school_door = CD_NONE;
  double best = INFINITY;
  for (uint32_t si = 0; si < content->nd; ++si) {
    const CdDoor *school = &content->doors[si];
    const CdPolygon *premises = &content->polygons[school->building];
    if (!school->reviewed || !premises->opening_known ||
        strcmp(premises->use, "education"))
      continue;
    for (uint32_t hi = 0; hi < content->nd; ++hi) {
      const CdDoor *home = &content->doors[hi];
      const CdPolygon *dwelling = &content->polygons[home->building];
      if (!home->reviewed || dwelling->capacity < 4 ||
          strcmp(dwelling->use, "residential") ||
          content->components[home->node] != content->components[school->node])
        continue;
      if (hypot(content->nodes[home->node].p.x - content->nodes[school->node].p.x,
                content->nodes[home->node].p.y - content->nodes[school->node].p.y) >= best)
        continue;
      CdRoute route;
      if (cd_route_indices(world, home->node, school->node, &route) == CD_OK &&
          route.count && route.cost < best) {
        home_door = hi;
        school_door = si;
        best = route.cost;
      }
    }
  }
  assert(home_door != CD_NONE && school_door != CD_NONE);
  uint32_t dwelling = content->doors[home_door].building;
  uint32_t school = content->doors[school_door].building;
  CdCitizen *pupil = &world->people[3];
  assert(pupil->civil.age >= 5 && pupil->civil.age <= 14);
  OK(cd_world_household_move(world, 1, content->polygons[dwelling].id, &diagnostic));
  OK(cd_world_place_households(world, &diagnostic));
  OK(cd_world_food_import(world, (CdFoodEndpoint){CD_FOOD_HOME, 1}, 16, 0,
                          "technical school trial meals", &diagnostic));
  uint64_t departure = wait_inside(world, pupil->civil.ref, dwelling, 120);
  uint64_t arrival = 0, returned = 0;
  const unsigned dismissal = 750;
  for (unsigned minute = 0; minute < 960; ++minute) {
    CdClockView clock;
    OK(cd_world_clock(world, &clock, &diagnostic));
    if (pupil->inside == school && pupil->door == CD_NONE && !arrival) {
      assert(clock.civil.minute_of_day < dismissal);
      arrival = cd_now(world);
    }
    if (clock.civil.minute_of_day < dismissal)
      cd_commute_to(world, pupil, school_door, 0, 0);
    else if (arrival && pupil->inside == dwelling && pupil->door == CD_NONE) {
      returned = cd_now(world);
      break;
    } else
      cd_commute_to(world, pupil, home_door, 0, 0);
    OK(cd_world_step(world, 1, &diagnostic));
  }
  if (!arrival || !returned)
    fprintf(stderr, "School journey timed out: %s\n", cd_route_reason(pupil->reason));
  assert(arrival > departure && returned > arrival);
  printf("{\"pupils\":1,\"outbound_minutes\":%llu,\"round_trip_minutes\":%llu,"
         "\"school\":\"%s\",\"ok\":true}\n",
         (unsigned long long)(arrival - departure),
         (unsigned long long)(returned - departure), content->polygons[school].source);
  cd_world_close(world);
}

static void household_profiles(CdContent *content) {
  CdWorld *world = NULL;
  CdScenarioStartup startup = {
      {27000, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"},
      {2160, 4050, 1620, 16470, 2700}, {0}};
  OK(cd_world_open_startup(content, &startup, &world, &diagnostic));
  for (uint32_t d = 0; d < content->nd; ++d) {
    const CdDoor *door = &content->doors[d];
    if (door->reviewed) {
      CdPoint point = content->nodes[door->node].p;
      printf("door %" PRIu64 " %.9g %.9g\n",
             (uint64_t)content->polygons[door->building].id, point.x, point.y);
    }
  }
  for (size_t h = 0; h < world->home_count; ++h) {
    assert(world->homes[h].members == 4);
    printf("%zu", h + 1);
    for (size_t i = h * 4; i < h * 4 + 4; ++i) {
      const CdCitizen *p = &world->people[i];
      assert(p->civil.household == h + 1);
      printf(" %" PRIu64 " %u", (uint64_t)p->civil.ref.person_id, p->civil.age);
    }
    puts("");
  }
  cd_world_close(world);
}

static void check_routes(CdContent *content) {
  CdWorld *world = NULL;
  CdScenario scenario = {1, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
  OK(cd_world_open(content, &scenario, &world, &diagnostic));
  uint64_t from, to;
  int read;
  while ((read = scanf("%" SCNu64 " %" SCNu64, &from, &to)) == 2) {
    uint32_t start = cd_reviewed_door(world, from), goal = cd_reviewed_door(world, to);
    assert(start != CD_NONE && goal != CD_NONE);
    CdRoute route;
    CdStatus status = cd_route_indices(world, content->doors[start].node,
                                      content->doors[goal].node, &route);
    printf("%d %u %.9g\n", status == CD_OK, (unsigned)route.reason, route.cost * cd_city_scale(world));
  }
  assert(read == EOF);
  cd_world_close(world);
}

typedef struct {
  uint32_t home, destination;
  unsigned leave, start, end, attendance;
  int on_time, departed, returned;
  CdRouteReason deadline_reason;
} DayJourney;

static void scheduled_day(CdContent *content) {
  CdWorld *world = NULL;
  CdScenarioStartup startup;
  OK(cd_scenario_startup_read(content, "build/content/access-27000.cdstart",
                              &startup, &diagnostic));
  OK(cd_world_open_duty_startup(content, &startup,
                                  "build/content/access-duty-startup.txt",
                                  &world, &diagnostic));
  DayJourney *journeys = calloc(world->count, sizeof *journeys);
  assert(journeys);
  unsigned assigned[2] = {0};
  size_t initial_count = world->count;
  for (size_t i = 0; i < initial_count; ++i) {
    CdCitizen *p = &world->people[i];
    CdJob *job = cd_job(world, p->jobs[0]);
    if (!job && !p->school_door)
      continue;
    assert(!job || cd_sector_job(job->view.occupation));
    journeys[i] = (DayJourney){
        .home = cd_reviewed_door(world, p->civil.dwelling),
        .destination = cd_door_index(content, job ? job->view.entrance : p->school_door),
        .leave = job ? job->commute_start : p->school_leave,
        .start = job ? job->view.shift_start : p->school_start,
        .end = job ? job->view.shift_end : p->school_end};
    assert(journeys[i].home != CD_NONE && journeys[i].destination != CD_NONE);
    assigned[!job]++;
  }
  assert(assigned[0] == 12000 && assigned[1] == 4050 && world->job_count == 12000);
  for (unsigned step = 0; step < 1440; ++step) {
    CdClockView clock;
    OK(cd_world_clock(world, &clock, &diagnostic));
    unsigned minute = clock.civil.minute_of_day;
    assert(world->count == initial_count);
    for (size_t i = 0; i < initial_count; ++i) {
      DayJourney *j = &journeys[i];
      if (!j->end || minute < j->leave || j->returned)
        continue;
      CdCitizen *p = &world->people[i];
      int present = p->inside == content->doors[j->destination].building && p->door == CD_NONE;
      int at_home = p->inside == content->doors[j->home].building && p->door == CD_NONE;
      if (!at_home && p->inside != content->doors[j->home].building)
        j->departed = 1;
      if (minute == j->start) {
        j->on_time = present;
        j->deadline_reason = p->reason;
      }
      if (minute >= j->start && minute < j->end)
        j->attendance += present;
      if (minute >= j->end && at_home)
        j->returned = j->departed;
    }
    OK(cd_world_step(world, 1, &diagnostic));
  }
  unsigned on_time[2] = {0}, completed[2] = {0}, returned[2] = {0}, passed[2] = {0};
  unsigned fully_paid = 0;
  unsigned late[CD_ROUTE_ACCESS + 1] = {0}, absent[CD_ROUTE_ACCESS + 1] = {0};
  for (size_t i = 0; i < initial_count; ++i) {
    DayJourney *j = &journeys[i];
    if (!j->end)
      continue;
    unsigned group = world->people[i].civil.age < 18;
    int attended = j->attendance == j->end - j->start;
    fully_paid += !group && world->people[i].wallet == j->end - j->start;
    if (!j->on_time)
      late[j->deadline_reason]++;
    if (!j->returned)
      absent[world->people[i].reason]++;
    on_time[group] += j->on_time;
    completed[group] += attended;
    returned[group] += j->returned;
    passed[group] += j->on_time && attended && j->returned;
  }
  for (unsigned reason = 0; reason <= CD_ROUTE_ACCESS; ++reason)
    if (late[reason] || absent[reason])
      fprintf(stderr, "%s: %u late, %u without return\n", cd_route_reason(reason),
              late[reason], absent[reason]);
  CdEconomyLedger ledger;
  OK(cd_world_economy_ledger(world, &ledger, &diagnostic));
  assert(ledger.money_created == 12000 * 480 && ledger.money_stock == ledger.money_created);
  printf("{\"minutes\":1440,\"fully_paid_workers\":%u,\"workers\":{\"on_time\":%u,\"completed\":%u,\"home\":%u,\"passed\":%u},"
         "\"pupils\":{\"on_time\":%u,\"completed\":%u,\"home\":%u,\"passed\":%u},\"ok\":%s}\n",
         fully_paid, on_time[0], completed[0], returned[0], passed[0],
         on_time[1], completed[1], returned[1], passed[1],
         passed[0] == assigned[0] && passed[1] == assigned[1] && fully_paid == assigned[0] ? "true" : "false");
  free(journeys);
  cd_world_close(world);
}

int main(int argc, char **argv) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  if (argc == 2 && !strcmp(argv[1], "--day")) {
    scheduled_day(content);
    cd_content_close(content);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--routes")) {
    check_routes(content);
    cd_content_close(content);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--households")) {
    household_profiles(content);
    cd_content_close(content);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--population")) {
    population_placement(content);
    cd_content_close(content);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--school")) {
    school_journey(content);
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
  CdHouseholdCareView care;
  OK(cd_world_set_household_care(world, worker, 1, &diagnostic));
  OK(cd_world_household_care(world, worker, &care, &diagnostic));
  assert(care.designated && care.infants + care.preschool + care.school_age + care.teenagers);
  assert(cd_world_job_assign(world, job, worker, (CdPersonRef){0}, 0, &diagnostic)
         == CD_INVALID_STATE);
  OK(cd_world_set_household_care(world, worker, 0, &diagnostic));
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
