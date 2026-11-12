#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include "cd_native.h"
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

static void funded_advancement(CdWorld *world) {
  unsigned *jobs_by_building = calloc(world->content->np, sizeof *jobs_by_building);
  assert(jobs_by_building);
  for (size_t i = 0; i < world->job_count; ++i)
    if (world->jobs[i].view.open)
      ++jobs_by_building[cd_polygon_index(world->content, world->jobs[i].view.workplace)];
  for (size_t i = 0; i < world->job_count; ++i) {
    CdJob *from = &world->jobs[i];
    const CdDslRole *entry = cd_dsl_job_role(from->view.occupation);
    if (!entry || entry->stage != 2)
      continue;
    char role_id[96], transition[96];
    snprintf(role_id, sizeof role_id, "%s.3", entry->career);
    snprintf(transition, sizeof transition, "%s_2_to_3", entry->career);
    const CdDslRole *target = cd_dsl_role(role_id);
    if (!target || target->independent)
      continue;
    CdPersonRef person = from->view.worker;
    CdCitizen *worker = cd_citizen(world, person);
    assert(cd_dsl_role_minutes(worker, entry) == 480);
    uint32_t home_node = cd_source_node(world, worker);
    for (uint32_t door = 0; door < world->content->nd; ++door) {
      const CdDoor *entrance = &world->content->doors[door];
      uint32_t building = entrance->building;
      const CdPolygon *site = &world->content->polygons[building];
      if (!entrance->reviewed || !cd_dsl_role_premises(target, site->use))
        continue;
      unsigned jobs = jobs_by_building[building];
      unsigned capacity = world->premises[building].employer
                              ? world->premises[building].work_capacity
                              : cd_capacity(world, building);
      if (jobs >= capacity)
        continue;
      CdRoute route;
      if (cd_route_indices(world, home_node, entrance->node, &route) != CD_OK)
        continue;
      if (!world->premises[building].employer)
        OK(cd_world_employer(world, site->id, "Career progression employer", capacity,
                            "Reviewed scenario premises", &diagnostic));
      OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = site->id},
                               (jobs + 1) * 480, "Next-day promotion payroll", &diagnostic));
      CdJobSpec spec = {site->id, entrance->id, 0, 0, 540, 1020, 1, 0, 0,
                        "Funded senior vacancy"};
      CdEntityId next;
      OK(cd_world_dsl_job_create(world, &spec, role_id, &next, &diagnostic));
      uint64_t before, after;
      OK(cd_world_digest(world, &before, &diagnostic));
      assert(cd_world_dsl_advance_job(world, person, transition,
                                     from->view.id, next, 1, &diagnostic) != CD_OK);
      OK(cd_world_digest(world, &after, &diagnostic));
      assert(before == after);
      OK(cd_world_dsl_certify(world, person, role_id, "Senior role assessment", &diagnostic));
      assert(cd_world_dsl_advance_job(world, person, transition,
                                     from->view.id, next, 0, &diagnostic) != CD_OK);
      OK(cd_world_dsl_advance_job(world, person, transition,
                                  from->view.id, next, 1, &diagnostic));
      assert(!from->view.open && cd_citizen(world, person)->jobs[0] == next);
      free(jobs_by_building);
      return;
    }
  }
  free(jobs_by_building);
  fputs("No funded worker with available compatible capacity for advancement test\n", stderr);
  abort();
}

static void funded_resource_matrix(CdWorld *world) {
  uint32_t door = 0;
  for (size_t i = 0; i < cd_dsl_resource_count(); ++i) {
    const CdDslResource *resource = cd_dsl_resource_at(i);
    if (i % 12 == 0) {
      while (door < world->content->nd && !world->content->doors[door].reviewed)
        ++door;
      assert(door < world->content->nd);
    }
    CdEntityId site = world->content->polygons[world->content->doors[door].building].id;
    CdDslBatch details = {.quality = 750, .condition = CD_DSL_GRADE_UNKNOWN,
                          .provenance = "Funded scenario resource matrix"};
    uint64_t batch;
    assert(cd_world_dsl_import_batch(world, site, resource->id, "bad unit",
                                     1, &details, &batch, &diagnostic) != CD_OK);
    assert(cd_world_dsl_import_batch(world, site, resource->id, resource->unit,
                                     UINT64_MAX, &details, &batch, &diagnostic) == CD_CAPACITY);
    OK(cd_world_dsl_import_batch(world, site, resource->id, resource->unit,
                                 1, &details, &batch, &diagnostic));
    assert(cd_world_dsl_batch_reserve(world, site, resource->id, batch, 1, 2,
                                      &diagnostic) == CD_CAPACITY);
    OK(cd_world_dsl_batch_reserve(world, site, resource->id, batch, 1, 1, &diagnostic));
    OK(cd_world_dsl_batch_reserve(world, site, resource->id, batch, 1, 1, &diagnostic));
    int consume = strcmp(resource->group, "equipment") != 0;
    OK(cd_world_dsl_batch_settle(world, site, resource->id, batch, 1, consume, &diagnostic));
    OK(cd_world_dsl_batch_settle(world, site, resource->id, batch, 1, consume, &diagnostic));
    CdDslBatchView view;
    OK(cd_world_dsl_batch(world, site, resource->id, batch, &view, &diagnostic));
    assert(view.available == (unsigned)!consume && !view.reserved && view.batch.quality == 750);
    if (i % 12 == 11)
      ++door;
  }
  assert(cd_dsl_stock_valid(world));
}

typedef struct {
  CdJob *job;
  const CdDslRecipe *recipe;
  uint64_t task;
} CityProduction;

static size_t city_production_orders(CdWorld *world, CityProduction *orders, size_t capacity) {
  size_t count = 0;
  for (size_t i = 0; i < world->job_count && count < capacity; ++i) {
    CdJob *job = &world->jobs[i];
    const CdDslRole *role = cd_dsl_job_role(job->view.occupation);
    const CdDslRecipe *recipe = cd_dsl_recipe(cd_dsl_role_recipe(role));
    if (!recipe)
      continue;
    int duplicate = 0;
    for (size_t n = 0; n < count; ++n)
      duplicate |= orders[n].job->view.workplace == job->view.workplace ||
                   !strcmp(cd_dsl_job_role(orders[n].job->view.occupation)->career, role->career);
    if (duplicate)
      continue;
    CdEntityId site = job->view.workplace;
    for (size_t n = 0; n < recipe->input_count; ++n)
      OK(cd_world_dsl_import(world, site, recipe->inputs[n].resource, recipe->inputs[n].quantity,
                              "City workshop input", &diagnostic));
    for (size_t n = 0; n < recipe->equipment_count; ++n)
      OK(cd_world_dsl_import(world, site, recipe->equipment[n], 1, "City workshop tool", &diagnostic));
    orders[count] = (CityProduction){.job = job, .recipe = recipe};
    OK(cd_world_refined_work_submit(world, job->view.worker, 1, job->view.id, recipe->id, 1,
                                    "Funded city workshop order", &orders[count].task, &diagnostic));
    ++count;
  }
  assert(count > 1);
  return count;
}

static size_t city_service_orders(CdWorld *world, uint64_t *requests) {
  static const struct {
    const char *career;
    CdServiceKind kind;
    unsigned skill, authority;
  } services[] = {
      {"office_administration", CD_SERVICE_ADMIN, CD_SKILL_COMMUNITY, CD_AUTH_ADMIN},
      {"social_care", CD_SERVICE_VISIT, CD_SKILL_CARE, 0},
      {"technical_instruction", CD_SERVICE_EDUCATION, CD_SKILL_COMMUNITY, CD_AUTH_TEACH},
  };
  size_t count = 0;
  for (size_t n = 0; n < sizeof services / sizeof services[0]; ++n) {
    for (size_t i = 0; i < world->job_count; ++i) {
      const CdJob *job = &world->jobs[i];
      const CdDslRole *role = cd_dsl_job_role(job->view.occupation);
      if (!role || strcmp(role->career, services[n].career))
        continue;
      CdPersonRef subject = {0};
      CdEntityId building = job->view.workplace;
      if (services[n].kind == CD_SERVICE_VISIT) {
        for (size_t k = 0; k < world->job_count; ++k) {
          const CdJob *peer = &world->jobs[k];
          if (peer != job && peer->view.workplace == building &&
              peer->view.shift_start == job->view.shift_start &&
              peer->view.shift_end == job->view.shift_end) {
            subject = peer->view.worker;
            break;
          }
        }
      } else if (services[n].kind == CD_SERVICE_EDUCATION) {
        for (size_t k = 0; k < world->count; ++k) {
          const CdCitizen *pupil = &world->people[k];
          if (pupil->school_door == job->view.entrance &&
              pupil->school_start + 5 < job->view.shift_end &&
              job->view.shift_start + 5 < pupil->school_end) {
            subject = pupil->civil.ref;
            break;
          }
        }
      }
      if (services[n].kind != CD_SERVICE_ADMIN && !subject.person_id)
        continue;
      CdPersonRef worker = job->view.worker;
      OK(cd_world_training(world, worker, services[n].skill, "Funded shift service skill", &diagnostic));
      if (services[n].authority)
        OK(cd_world_authorize(world, worker, services[n].authority, "Funded shift authority", &diagnostic));
      CdRosterSpec roster = {job->view.id, 1u << services[n].kind,
                             "Scheduled Canterbury service", "Funded shift duty"};
      CdEntityId provider;
      OK(cd_world_dsl_public_roster(world, &roster, &provider, &diagnostic));
      CdServiceSpec request = {provider, 0, services[n].kind, building, subject,
                               cd_now(world) + job->view.shift_end, 3, "Funded shift service"};
      uint64_t key = count + 1;
      OK(cd_world_dispatch(world, key, &request, &requests[count], &diagnostic));
      count++;
      break;
    }
  }
  assert(count == sizeof services / sizeof services[0]);
  return count;
}

static void city_day_ledgers(CdWorld *world, CdEconomyLedger *economy,
                             CdFoodLedger *food, CdCivicSummary *civic) {
  OK(cd_world_economy_ledger(world, economy, &diagnostic));
  OK(cd_world_food_ledger(world, food, &diagnostic));
  OK(cd_world_civic_summary(world, civic, &diagnostic));
  assert(economy->money_stock == economy->money_created);
  assert(civic->water_stock + civic->water_used + civic->water_exported == civic->water_imported);
  assert(cd_dsl_stock_valid(world));
}

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
  for (size_t index = 0; index < cd_dsl_role_count(); ++index) {
    const CdDslRole *role = cd_dsl_role_at(index);
    int eligible;
    uint64_t minutes;
    assert(role && role == cd_dsl_job_role(cd_dsl_role_occupation(role)));
    assert(role->requirement_count && cd_dsl_career(role->career));
    OK(cd_world_dsl_eligibility(world, world->people[0].civil.ref, role->id,
                                world->people[0].civil.dwelling, &eligible, &minutes, &diagnostic));
  }
  unsigned assigned[2] = {0};
  size_t initial_count = world->count;
  for (size_t i = 0; i < initial_count; ++i) {
    CdCitizen *p = &world->people[i];
    CdJob *job = cd_job(world, p->jobs[0]);
    if (!job && !p->school_door)
      continue;
    if (job) {
      const CdDslRole *role = cd_dsl_job_role(job->view.occupation);
      int eligible;
      uint64_t minutes;
      assert(role && job->attendance_sector);
      OK(cd_world_dsl_eligibility(world, p->civil.ref, role->id,
                                  job->view.workplace, &eligible, &minutes, &diagnostic));
      assert(eligible && !minutes);
    }
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
  CityProduction production[26];
  size_t production_count = city_production_orders(world, production, 26);
  uint64_t services[3];
  size_t service_count = city_service_orders(world, services);
  CdEconomyLedger ledger;
  CdFoodLedger food;
  CdCivicSummary civic;
  uint64_t before, after;
  CdClockView start_clock, inspected_clock;
  OK(cd_world_digest(world, &before, &diagnostic));
  OK(cd_world_clock(world, &start_clock, &diagnostic));
  city_day_ledgers(world, &ledger, &food, &civic);
  for (size_t i = 0; i < service_count; ++i) {
    CdServiceView service;
    OK(cd_world_service(world, services[i], &service, &diagnostic));
    assert(service.progress == 0 && service.reserved_money == 0);
  }
  OK(cd_world_clock(world, &inspected_clock, &diagnostic));
  OK(cd_world_digest(world, &after, &diagnostic));
  assert(before == after && start_clock.native_minutes == inspected_clock.native_minutes);
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
    if (!group) {
      CdCitizen *p = &world->people[i];
      CdJob *job = cd_job(world, p->jobs[0]);
      int eligible;
      uint64_t minutes;
      OK(cd_world_dsl_eligibility(world, p->civil.ref,
                                  cd_dsl_job_role(job->view.occupation)->id,
                                  job->view.workplace, &eligible, &minutes, &diagnostic));
      uint64_t family_minutes;
      OK(cd_world_dsl_family_experience(world, p->civil.ref,
          cd_dsl_job_role(job->view.occupation)->career, &family_minutes, &diagnostic));
      assert(family_minutes == minutes);
      fully_paid += eligible && p->wallet == j->end - j->start && minutes == j->end - j->start;
    }
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
  city_day_ledgers(world, &ledger, &food, &civic);
  assert(ledger.money_created == UINT64_C(12000) * 480 && ledger.wages == ledger.money_created);
  for (uint32_t building = 0; building < content->np; ++building)
    assert(!world->premises[building].escrow_money);
  assert(civic.service_outputs == service_count);
  for (size_t i = 0; i < service_count; ++i) {
    CdServiceView service;
    OK(cd_world_service(world, services[i], &service, &diagnostic));
    if (service.state != CD_TASK_DONE)
      fprintf(stderr, "Scheduled service %zu: %s (%u of %u minutes)\n",
              i, service.reason, service.progress, service.minutes);
    assert(service.state == CD_TASK_DONE && service.progress == 3 &&
           !service.paid && !service.reserved_money);
  }
  CdClockView end_clock;
  OK(cd_world_clock(world, &end_clock, &diagnostic));
  assert(end_clock.native_minutes - start_clock.native_minutes == 1440);
  for (size_t i = 0; i < production_count; ++i) {
    CdJob *job = production[i].job;
    const CdDslRecipe *recipe = production[i].recipe;
    CdWorkView work;
    OK(cd_world_work(world, job->view.worker, &work, &diagnostic));
    assert(work.id == production[i].task && work.state == CD_TASK_DONE &&
           work.paid == recipe->worker_minutes);
    for (size_t n = 0; n < recipe->output_count; ++n) {
      CdDslStockView stock;
      OK(cd_world_dsl_stock(world, job->view.workplace, recipe->outputs[n].resource, &stock, &diagnostic));
      assert(stock.available == recipe->outputs[n].quantity && !stock.reserved);
    }
  }
  assert(cd_dsl_stock_valid(world));
  int complete_day = passed[0] == assigned[0] && passed[1] == assigned[1] &&
                     fully_paid == assigned[0];
  if (complete_day) {
    funded_advancement(world);
    funded_resource_matrix(world);
  }
  size_t career_storage = cd_smoke_capacity() * sizeof world->people[0].certificates;
  printf("{\"minutes\":1440,\"scheduled_services\":%zu,\"wages\":%" PRIu64
         ",\"money_created\":%" PRIu64 ",\"money_stock\":%" PRIu64
         ",\"ledgers_balanced\":true,\"inspection_pure\":true,\"city_production_orders\":%zu,\"resource_kinds_verified\":%zu,\"funded_advancement\":%s,\"roles_inspected\":%zu,\"competence_storage_bytes\":%zu,"
         "\"owned_peak_bytes\":%zu,\"fully_paid_workers\":%u,\"workers\":{\"on_time\":%u,\"completed\":%u,\"home\":%u,\"passed\":%u},"
         "\"pupils\":{\"on_time\":%u,\"completed\":%u,\"home\":%u,\"passed\":%u},\"ok\":%s}\n",
         service_count, ledger.wages, ledger.money_created, ledger.money_stock,
         production_count, complete_day ? cd_dsl_resource_count() : 0,
         complete_day ? "true" : "false", cd_dsl_role_count(), career_storage, cd_memory_stats().peak_total,
         fully_paid, on_time[0], completed[0], returned[0], passed[0],
         on_time[1], completed[1], returned[1], passed[1],
         complete_day ? "true" : "false");
  free(journeys);
  cd_world_close(world);
}

static void supervised_milling(CdWorld *world, CdPersonRef mentor, CdEntityId original,
                               CdEntityId premises, CdEntityId entrance) {
  CdPersonRef learner = world->people[1].civil.ref;
  OK(cd_world_job_close(world, original, &diagnostic));
  OK(cd_world_employer(world, premises, "Technical milling school", 3,
                       "Supervised milling curriculum", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = premises},
                           1000, "Funded technical training shifts", &diagnostic));
  OK(cd_world_dsl_certify(world, mentor, "miller.3", "Skilled miller assessment", &diagnostic));
  CdJobSpec spec = {premises, entrance, 0, 0, 920, 1150, 1, 0, 0, "Milling training trial"};
  CdEntityId supervisor, apprentice, graduate;
  OK(cd_world_dsl_job_create(world, &spec, "miller.3", &supervisor, &diagnostic));
  OK(cd_world_dsl_job_create(world, &spec, "miller.1", &apprentice, &diagnostic));
  OK(cd_world_dsl_job_create(world, &spec, "miller.2", &graduate, &diagnostic));
  OK(cd_world_job_assign(world, supervisor, mentor, (CdPersonRef){0}, 0, &diagnostic));
  assert(cd_world_job_assign(world, apprentice, learner, (CdPersonRef){0}, 0, &diagnostic) != CD_OK);
  OK(cd_world_job_assign(world, apprentice, learner, mentor, 0, &diagnostic));
  OK(cd_world_dsl_training_start(world, apprentice, 60, &diagnostic));
  const CdDslRole *role = cd_dsl_role("miller.1");
  CdCitizen *person = cd_citizen(world, learner);
  for (unsigned i = 0; i < 80 && cd_dsl_role_minutes(person, role) < 15; ++i)
    OK(cd_world_step(world, 1, &diagnostic));
  uint64_t before = cd_dsl_role_minutes(person, role);
  assert(before == 15);
  OK(cd_world_job_unassign(world, supervisor, &diagnostic));
  OK(cd_world_step(world, 5, &diagnostic));
  assert(cd_dsl_role_minutes(person, role) == before);
  OK(cd_world_job_assign(world, supervisor, mentor, (CdPersonRef){0}, 0, &diagnostic));
  OK(cd_world_job_assign(world, apprentice, learner, mentor, 0, &diagnostic));
  assert(cd_world_dsl_advance_job(world, learner, "miller_1_to_2",
                                 apprentice, graduate, 1, &diagnostic) != CD_OK);
  OK(cd_world_dsl_import(world, premises, "wheat_grain", 1000,
                        "Competing production input", &diagnostic));
  OK(cd_world_dsl_import(world, premises, "hand_tool", 1,
                        "Competing production equipment", &diagnostic));
  uint64_t competing_order;
  OK(cd_world_refined_work_submit(world, mentor, 2, supervisor, "mill_wheat", 1,
                                    "Production competes with instruction", &competing_order, &diagnostic));
  for (unsigned i = 0; i < 60; ++i) {
    CdWorkView work;
    OK(cd_world_work(world, mentor, &work, &diagnostic));
    if (work.state == CD_TASK_DONE)
      break;
    OK(cd_world_step(world, 1, &diagnostic));
    assert(cd_dsl_role_minutes(person, role) == before);
  }
  CdWorkView production;
  OK(cd_world_work(world, mentor, &production, &diagnostic));
  assert(production.id == competing_order && production.state == CD_TASK_DONE &&
         production.progress == 30 && production.paid == 30);
  for (unsigned i = 0; i < 100 && cd_dsl_role_minutes(person, role) < 60; ++i)
    OK(cd_world_step(world, 1, &diagnostic));
  assert(cd_dsl_role_minutes(person, role) == 60);
  assert(cd_world_dsl_certify(world, learner, "medicine.2", "Technical assessment", &diagnostic) != CD_OK);
  OK(cd_world_dsl_credential(world, learner, "medicine.2", "Trial independent medical credential", &diagnostic));
  assert(cd_world_dsl_credential(world, learner, "anglican_ministry.2", "Missing appointment", &diagnostic) != CD_OK);
  OK(cd_world_dsl_certify(world, learner, "miller.2", "Milling completion assessment", &diagnostic));
  OK(cd_world_dsl_advance_job(world, learner, "miller_1_to_2", apprentice, graduate, 1, &diagnostic));
  assert(person->jobs[0] == graduate && !cd_job(world, apprentice)->view.open);
}

static void batch_mill_delivery(CdWorld **active, CdPersonRef *worker, CdEntityId home,
                                 CdEntityId mill, uint32_t entrance) {
  CdWorld *world = *active;
  CdPersonRef carrier = *worker;
  CdDslBatch input = {.quality = 820, .condition = CD_DSL_GRADE_UNKNOWN,
                      .provenance = "Reviewed mill delivery wheat"};
  uint64_t wheat;
  OK(cd_world_dsl_import_batch(world, home, "wheat_grain", "g", 1000, &input, &wheat, &diagnostic));
  OK(cd_world_dsl_batch_cargo_order(world, carrier, home, "wheat_grain", wheat, 1, 1000, 0, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  CdDslCargoView cargo;
  OK(cd_world_dsl_cargo(world, carrier, &cargo, &diagnostic));
  assert(cargo.carried == 1000 && cargo.batch.id == wheat && cargo.batch.quality == 820);
  uint64_t saved, restored;
  OK(cd_world_digest(world, &saved, &diagnostic));
  OK(cd_world_save(world, "build/access-custody.city", &diagnostic));
  OK(cd_world_load(active, world->content, "build/access-custody.city", &diagnostic));
  world = *active;
  carrier = *worker = world->people[0].civil.ref;
  OK(cd_world_digest(world, &restored, &diagnostic));
  assert(saved == restored);
  OK(cd_world_dsl_cargo(world, carrier, &cargo, &diagnostic));
  assert(cargo.carried == 1000 && cargo.batch.id == wheat);
  CdCitizen *person = cd_citizen(world, carrier);
  uint32_t destination = world->content->doors[entrance].building;
  for (unsigned i = 0; i < 120 && (person->inside != destination || person->door != CD_NONE); ++i) {
    cd_commute_to(world, person, entrance, 0, 0);
    OK(cd_world_step(world, 1, &diagnostic));
  }
  assert(person->inside == destination && person->door == CD_NONE);
  OK(cd_world_dsl_batch_cargo_order(world, carrier, mill, "wheat_grain", wheat, 2, 1000, 1, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  uint64_t before, after;
  OK(cd_world_digest(world, &before, &diagnostic));
  OK(cd_world_dsl_batch_cargo_order(world, carrier, mill, "wheat_grain", wheat, 2, 1000, 1, &diagnostic));
  assert(cd_world_dsl_batch_cargo_order(world, carrier, mill, "wheat_grain", wheat, 2, 999, 1,
                                        &diagnostic) != CD_OK);
  OK(cd_world_digest(world, &after, &diagnostic));
  assert(before == after);
  CdDslBatchView view;
  OK(cd_world_dsl_batch(world, mill, "wheat_grain", wheat, &view, &diagnostic));
  assert(view.available == 1000 && !strcmp(view.batch.provenance, input.provenance));

  uint64_t tool, expired_lot;
  CdDslBatch equipment = {.quality = CD_DSL_GRADE_UNKNOWN, .condition = 900,
                           .provenance = "Mill hand tool inspection"};
  OK(cd_world_dsl_import_batch(world, mill, "hand_tool", "each", 1, &equipment, &tool, &diagnostic));
  OK(cd_world_dsl_batch_wear(world, mill, "hand_tool", tool, 100, &diagnostic));
  OK(cd_world_dsl_batch(world, mill, "hand_tool", tool, &view, &diagnostic));
  assert(view.batch.condition == 800 && view.available == 1);
  input.expires = cd_now(world) + 2;
  OK(cd_world_dsl_import_batch(world, mill, "wheat_grain", "g", 100, &input, &expired_lot, &diagnostic));
  OK(cd_world_dsl_batch_reserve(world, mill, "wheat_grain", expired_lot, 1, 40, &diagnostic));
  OK(cd_world_step(world, 2, &diagnostic));
  assert(cd_world_dsl_batch_settle(world, mill, "wheat_grain", expired_lot, 1, 1, &diagnostic) != CD_OK);
  OK(cd_world_dsl_batch_settle(world, mill, "wheat_grain", expired_lot, 1, 0, &diagnostic));
  OK(cd_world_dsl_batch(world, mill, "wheat_grain", expired_lot, &view, &diagnostic));
  assert(!view.available && !view.reserved && cd_dsl_stock_valid(world));
}

static uint32_t nearby_shop(CdWorld *world, uint32_t from, uint32_t exclude) {
  uint32_t chosen = CD_NONE;
  double best = INFINITY;
  for (uint32_t i = 0; i < world->content->nd; ++i) {
    const CdDoor *door = &world->content->doors[i];
    if (i == from || i == exclude || !door->reviewed ||
        strcmp(world->content->polygons[door->building].use, "commercial"))
      continue;
    CdRoute route;
    if (cd_route_indices(world, world->content->doors[from].node, door->node, &route) == CD_OK &&
        route.cost < best) {
      best = route.cost;
      chosen = i;
    }
  }
  assert(chosen != CD_NONE);
  return chosen;
}

static void supply_arrival(CdWorld *world, CdPersonRef ref, uint32_t door) {
  CdCitizen *person = cd_citizen(world, ref);
  uint32_t building = world->content->doors[door].building;
  for (unsigned i = 0; i < 120; ++i) {
    if (person->inside == building && person->door == CD_NONE)
      return;
    cd_commute_to(world, person, door, 0, 0);
    OK(cd_world_step(world, 1, &diagnostic));
  }
  fputs("Supply route did not reach its destination\n", stderr);
  abort();
}

static CdEntityId supply_job(CdWorld *world, CdPersonRef worker, uint32_t door,
                             const char *role) {
  const CdDoor *entrance = &world->content->doors[door];
  CdEntityId premises = world->content->polygons[entrance->building].id;
  OK(cd_world_employer(world, premises, "Funded supply chain premises", 1,
                       "Technical production chain", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = premises},
                           600, "Supply chain shift payroll", &diagnostic));
  OK(cd_world_dsl_certify(world, worker, role, "Supply chain assessment", &diagnostic));
  CdJobSpec spec = {premises, entrance->id, 0, 0, 900, 1310, 1, 0, 0, "Funded supply shift"};
  CdEntityId job;
  OK(cd_world_dsl_job_create(world, &spec, role, &job, &diagnostic));
  OK(cd_world_job_assign(world, job, worker, (CdPersonRef){0}, 0, &diagnostic));
  return job;
}

static void carry_supply(CdWorld *world, CdPersonRef carrier, CdEntityId from,
                         uint32_t to, const char *resource, uint64_t amount) {
  OK(cd_world_dsl_cargo_order(world, carrier, from, resource, amount, 0, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  CdDslCargoView cargo;
  OK(cd_world_dsl_cargo(world, carrier, &cargo, &diagnostic));
  assert(cargo.carried == amount && !cargo.pending_quantity);
  supply_arrival(world, carrier, to);
  CdEntityId destination = world->content->polygons[world->content->doors[to].building].id;
  OK(cd_world_dsl_cargo_order(world, carrier, destination, resource, amount, 1, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  OK(cd_world_dsl_cargo(world, carrier, &cargo, &diagnostic));
  assert(!cargo.carried && !cargo.pending_quantity);
}

static void finish_supply_work(CdWorld *world, CdPersonRef worker, uint64_t task, unsigned pay) {
  CdWorkView work;
  for (unsigned i = 0; i < 100; ++i) {
    OK(cd_world_work(world, worker, &work, &diagnostic));
    if (work.state == CD_TASK_DONE)
      break;
    OK(cd_world_step(world, 1, &diagnostic));
  }
  assert(work.id == task && work.state == CD_TASK_DONE && work.paid == pay);
}

static void funded_book_chain(CdWorld *world, uint32_t mill_door) {
  CdPersonRef printer = world->people[0].civil.ref;
  CdEntityId printshop = world->content->polygons[world->content->doors[mill_door].building].id;
  CdEntityId job = supply_job(world, printer, mill_door, "printer.2");
  OK(cd_world_dsl_import(world, printshop, "paper", 1000, "Paper import", &diagnostic));
  OK(cd_world_dsl_import(world, printshop, "cardboard", 100, "Book cover import", &diagnostic));
  uint64_t task;
  OK(cd_world_refined_work_submit(world, printer, 3, job, "cut_paper", 1,
                                  "Cut book stock", &task, &diagnostic));
  finish_supply_work(world, printer, task, 5);
  OK(cd_world_refined_work_submit(world, printer, 4, job, "bind_school_book", 1,
                                  "Bind school book", &task, &diagnostic));
  finish_supply_work(world, printer, task, 15);
  OK(cd_world_job_close(world, job, &diagnostic));
  uint32_t school = CD_NONE;
  double best = INFINITY;
  for (uint32_t i = 0; i < world->content->nd; ++i) {
    const CdDoor *door = &world->content->doors[i];
    if (!door->reviewed || strcmp(world->content->polygons[door->building].use, "education"))
      continue;
    CdRoute route;
    if (cd_route_indices(world, world->content->doors[mill_door].node, door->node, &route) == CD_OK &&
        route.cost < best) {
      school = i;
      best = route.cost;
    }
  }
  assert(school != CD_NONE);
  carry_supply(world, printer, printshop, school, "school_book", 1);
  CdEntityId destination = world->content->polygons[world->content->doors[school].building].id;
  CdDslStockView stock;
  OK(cd_world_dsl_stock(world, destination, "school_book", &stock, &diagnostic));
  assert(stock.available == 1 && !stock.reserved);
  OK(cd_world_dsl_stock(world, printshop, "paper_offcuts", &stock, &diagnostic));
  assert(stock.available == 70);
  OK(cd_world_dsl_stock(world, printshop, "paper", &stock, &diagnostic));
  assert(stock.available == 750);
  assert(cd_dsl_stock_valid(world));
}

static void funded_bread_chain(CdWorld *world, uint32_t mill_door) {
  CdPersonRef carrier = world->people[0].civil.ref;
  CdPersonRef baker = world->people[1].civil.ref;
  OK(cd_world_job_close(world, world->people[0].jobs[0], &diagnostic));
  OK(cd_world_job_close(world, world->people[1].jobs[0], &diagnostic));
  CdEntityId mill = world->content->polygons[world->content->doors[mill_door].building].id;
  uint32_t bakery_door = nearby_shop(world, mill_door, CD_NONE);
  uint32_t shop_door = nearby_shop(world, bakery_door, mill_door);
  CdEntityId bakery = world->content->polygons[world->content->doors[bakery_door].building].id;
  CdEntityId shop = world->content->polygons[world->content->doors[shop_door].building].id;
  CdEntityId bake_job = supply_job(world, baker, bakery_door, "baker.2");
  supply_arrival(world, carrier, mill_door);
  carry_supply(world, carrier, mill, bakery_door, "flour", 750);
  OK(cd_world_dsl_import(world, bakery, "potable_water", 300, "Bakery water import", &diagnostic));
  OK(cd_world_dsl_import(world, bakery, "cooking_equipment", 1, "Bakery equipment", &diagnostic));
  OK(cd_world_dsl_import(world, bakery, "firewood", 120, "Accounted bakery fuel", &diagnostic));
  uint64_t task;
  OK(cd_world_refined_work_submit(world, baker, 1, bake_job, "bake_bread", 1,
                                  "Funded bakery order", &task, &diagnostic));
  finish_supply_work(world, baker, task, 25);
  OK(cd_world_job_close(world, bake_job, &diagnostic));
  CdEntityId grocer = supply_job(world, baker, shop_door, "grocer.2");
  carry_supply(world, carrier, bakery, shop_door, "bread", 1000);
  wait_inside(world, baker, world->content->doors[shop_door].building, 120);
  CdAccount till = {.kind = CD_ACCOUNT_EMPLOYER, .id = shop};
  CdAccount wallet = {.kind = CD_ACCOUNT_PERSON, .person = carrier};
  uint64_t before, after, buyer_before, buyer_after;
  OK(cd_world_balance(world, till, &before, &diagnostic));
  OK(cd_world_balance(world, wallet, &buyer_before, &diagnostic));
  OK(cd_world_dsl_purchase_at_post(world, carrier, shop, grocer, "bread", 0, 3, 100, 12, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  OK(cd_world_dsl_purchase_at_post(world, carrier, shop, grocer, "bread", 0, 3, 100, 12, &diagnostic));
  assert(cd_world_dsl_purchase_at_post(world, carrier, shop, grocer, "bread", 0, 3, 100, 13,
                               &diagnostic) != CD_OK);
  OK(cd_world_balance(world, till, &after, &diagnostic));
  OK(cd_world_balance(world, wallet, &buyer_after, &diagnostic));
  assert(after == before + 11 && buyer_before == buyer_after + 12);
  CdNeedsView needs_before, needs_after;
  OK(cd_world_needs(world, carrier, &needs_before, &diagnostic));
  OK(cd_world_dsl_eat_carried(world, carrier, shop, 100, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  OK(cd_world_needs(world, carrier, &needs_after, &diagnostic));
  assert(needs_after.energy > needs_before.energy);
  CdDslStockView stock;
  OK(cd_world_dsl_stock(world, shop, "bread", &stock, &diagnostic));
  assert(stock.available == 900);
  OK(cd_world_dsl_stock(world, bakery, "waste_water", &stock, &diagnostic));
  assert(stock.available == 50);
  OK(cd_world_dsl_stock(world, bakery, "ash", &stock, &diagnostic));
  assert(stock.available == 12);
  OK(cd_world_dsl_stock(world, bakery, "firewood", &stock, &diagnostic));
  assert(!stock.available && !stock.reserved);
  OK(cd_world_dsl_stock(world, bakery, "potable_water", &stock, &diagnostic));
  assert(!stock.available && !stock.reserved);
  CdEconomyLedger ledger;
  OK(cd_world_economy_ledger(world, &ledger, &diagnostic));
  assert(ledger.money_stock == ledger.money_created && cd_dsl_stock_valid(world));
}

static void fixture_credential(CdWorld *world, CdPersonRef person, const CdDslRole *role) {
  if (role->independent)
    OK(cd_world_dsl_credential(world, person, role->id, "Independent fixture authority", &diagnostic));
  else
    OK(cd_world_dsl_certify(world, person, role->id, "Technical fixture assessment", &diagnostic));
}

static void transition_matrix(void) {
  CdContent *content = NULL;
  OK(cd_content_open("build/fixtures/civic.bin", &content, &diagnostic));
  const char *denomination = content->sites[cd_site_index(content, 401)].denomination;
  unsigned passed = 0, rejected = 0;
  for (size_t n = 0; n < cd_dsl_role_count(); ++n) {
    const CdDslRole *first = cd_dsl_role_at(n);
    if (first->stage != 1)
      continue;
    const CdDslCareer *career = cd_dsl_career(first->career);
    assert(career && career->premises_count);
    CdScenario scenario = {4, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
    CdWorld *world = NULL;
    OK(cd_world_open(content, &scenario, &world, &diagnostic));
    uint32_t building = cd_polygon_index(content, 2);
    snprintf(world->premises[building].scenario_use, sizeof world->premises[building].scenario_use,
             "%s", career->premises[0]);
    CdPersonRef worker = world->people[0].civil.ref;
    CdPersonRef mentor = world->people[1].civil.ref;
    OK(cd_world_place(world, worker, 110, &diagnostic));
    OK(cd_world_place(world, mentor, 110, &diagnostic));
    CdEntityId institution = 0;
    if (!strcmp(career->id, "anglican_ministry") || !strcmp(career->id, "roman_catholic_ministry") ||
        !strcmp(career->id, "jewish_ministry")) {
      institution = !strcmp(career->id, "jewish_ministry") ? 404 : 401;
      CdOfficeKind kind = institution == 404 ? CD_OFFICE_RABBI : CD_OFFICE_PRIEST;
      content->sites[cd_site_index(content, 401)].denomination =
          !strcmp(career->id, "roman_catholic_ministry") ? "roman_catholic" : "anglican";
      OK(cd_world_institution_link(world, institution, 2, "Fixture premises", &diagnostic));
      OK(cd_world_institution_operate(world, institution, 1, 1, "Fixture office",
                                      "Explicit fixture", &diagnostic));
      assert(cd_world_religious_qualification(world, worker, institution,
          kind == CD_OFFICE_RABBI ? CD_OFFICE_PRIEST : CD_OFFICE_RABBI,
          "Mismatched tradition", &diagnostic) != CD_OK);
      OK(cd_world_religious_qualification(world, worker, institution, kind,
                                          "Independent fixture appointment", &diagnostic));
      OK(cd_world_religious_qualification(world, mentor, institution, kind,
                                          "Independent fixture appointment", &diagnostic));
    }
    OK(cd_world_employer(world, 2, "Fixture employer", 8, "Reviewed fixture", &diagnostic));
    OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = 2},
                             10000, "Fixture payroll", &diagnostic));
    CdJobSpec spec = {2, 303, 0, institution, 480, 960, 1, 0, 0, "Transition fixture"};
    char name[96];
    snprintf(name, sizeof name, "%s.3", career->id);
    const CdDslRole *teacher = cd_dsl_role(name);
    CdEntityId mentor_job, current;
    fixture_credential(world, mentor, teacher);
    OK(cd_world_dsl_job_create(world, &spec, teacher->id, &mentor_job, &diagnostic));
    OK(cd_world_job_assign(world, mentor_job, mentor, (CdPersonRef){0}, 0, &diagnostic));
    OK(cd_world_dsl_job_create(world, &spec, first->id, &current, &diagnostic));
    OK(cd_world_job_assign(world, current, worker, mentor, 0, &diagnostic));
    const CdDslRole *source = first;
    for (unsigned stage = 1; stage < 5; ++stage) {
      char transition[96];
      snprintf(name, sizeof name, "%s.%u", career->id, stage + 1);
      snprintf(transition, sizeof transition, "%s_%u_to_%u", career->id, stage, stage + 1);
      const CdDslRole *target = cd_dsl_role(name);
      const CdDslAdvance *link = cd_dsl_advance(transition);
      assert(target && link && link->minimum_minutes);
      CdEntityId destination;
      OK(cd_world_dsl_job_create(world, &spec, name, &destination, &diagnostic));
      uint64_t before, after;
      OK(cd_world_digest(world, &before, &diagnostic));
      assert(cd_world_dsl_advance_job(world, worker, transition, current,
                                      destination, 1, &diagnostic) != CD_OK);
      OK(cd_world_digest(world, &after, &diagnostic));
      assert(before == after);
      ++rejected;
      /* Seed completed evidence here to isolate each transition gate. */
      cd_dsl_record_experience(cd_citizen(world, worker), source, link->minimum_minutes);
      assert(cd_world_dsl_advance_job(world, worker, transition, current,
                                      destination, 1, &diagnostic) != CD_OK);
      fixture_credential(world, worker, target);
      if (target->owner) {
        assert(cd_world_dsl_advance_job(world, worker, transition, current,
                                        destination, 1, &diagnostic) != CD_OK);
        OK(cd_world_owner(world, 2, worker, &diagnostic));
      }
      OK(cd_world_dsl_advance_job(world, worker, transition, current,
                                  destination, 1, &diagnostic));
      assert(!cd_job(world, current)->view.open && cd_citizen(world, worker)->jobs[0] == destination);
      source = target;
      current = destination;
      ++passed;
    }
    cd_world_close(world);
  }
  content->sites[cd_site_index(content, 401)].denomination = (char *)denomination;
  cd_content_close(content);
  assert(passed == cd_dsl_advance_count() && rejected == passed);
  printf("{\"transitions_passed\":%u,\"atomic_rejections\":%u,\"ok\":true}\n", passed, rejected);
}

static void recipe_matrix(void) {
  static const char *const roles[][2] = {
    {"brew_small_beer", "brewer.2"},
    {"fit_pipe", "plumber.2"},
    {"form_electrical_fitting", "electrician.2"},
    {"recondition_vehicle_part", "motor_mechanic.2"},
    {"form_pottery", "potter.2"},
    {"prepare_brickwork", "bricklayer.2"},
    {"dress_stone", "stonemason.2"},
    {"cut_roofing", "roofer.2"},
    {"mix_plaster", "plasterer.2"},
    {"prepare_paint", "painter_decorator.2"},
    {"clean_harvest", "crop_farming.2"},
    {"sort_eggs", "livestock_farming.2"},
    {"churn_butter", "dairy.2"},
    {"grade_vegetables", "horticulture.2"},
    {"split_firewood", "woodland.2"},
    {"wash_linen", "laundry.2"},
    {"mill_wheat", "miller.2"}, {"cut_timber", "carpenter.2"},
    {"cut_cloth", "weaver.2"}, {"cut_paper", "printer.2"},
    {"form_metalwork", "metalworker.2"}, {"trial_portion_meat", "butcher.2"},
    {"trial_portion_fish", "fishmonger.2"}, {"trial_trim_leather", "shoemaker.2"},
    {"bake_bread", "baker.2"}, {"cook_meal", "kitchen.2"},
    {"bind_school_book", "printer.2"}, {"sew_garment", "dressmaker.2"},
    {"make_shoes", "shoemaker.2"},
  };
  assert(cd_dsl_recipe_count() == sizeof roles / sizeof roles[0]);
  CdContent *content = NULL;
  OK(cd_content_open("build/fixtures/civic.bin", &content, &diagnostic));
  content->walk_units_per_minute = 769;
  unsigned verified = 0;
  for (size_t i = 0; i < sizeof roles / sizeof roles[0]; ++i) {
    const CdDslRecipe *recipe = cd_dsl_recipe(roles[i][0]);
    const CdDslRole *role = cd_dsl_role(roles[i][1]);
    const CdDslCareer *career = cd_dsl_career(role->career);
    CdScenario scenario = {4, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
    CdWorld *world = NULL;
    OK(cd_world_open(content, &scenario, &world, &diagnostic));
    uint32_t building = cd_polygon_index(content, 2);
    snprintf(world->premises[building].scenario_use, sizeof world->premises[building].scenario_use,
             "%s", career->premises[0]);
    CdPersonRef worker = world->people[0].civil.ref;
    OK(cd_world_place(world, worker, 110, &diagnostic));
    OK(cd_world_enter(world, worker, 303, 0, 0, &diagnostic));
    wait_inside(world, worker, building, 120);
    OK(cd_world_employer(world, 2, "Recipe fixture", 2, "Reviewed trial premises", &diagnostic));
    OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = 2},
                             1000, "Recipe fixture payroll", &diagnostic));
    OK(cd_world_dsl_certify(world, worker, role->id, "Technical recipe assessment", &diagnostic));
    CdJobSpec spec = {2, 303, 0, 0, 480, 1020, 1, 0, 0, "Funded recipe fixture"};
    CdEntityId job;
    OK(cd_world_dsl_job_create(world, &spec, role->id, &job, &diagnostic));
    OK(cd_world_job_assign(world, job, worker, (CdPersonRef){0}, 0, &diagnostic));
    for (size_t n = 0; n < recipe->input_count; ++n)
      OK(cd_world_dsl_import(world, 2, recipe->inputs[n].resource, recipe->inputs[n].quantity,
                              "Recipe fixture import", &diagnostic));
    for (size_t n = 0; n < recipe->equipment_count; ++n)
      OK(cd_world_dsl_import(world, 2, recipe->equipment[n], 1, "Recipe fixture tools", &diagnostic));
    uint64_t task;
    OK(cd_world_refined_work_submit(world, worker, 1, job, recipe->id, 1, "Recipe trial", &task, &diagnostic));
    OK(cd_world_step(world, 1, &diagnostic));
    CdWorkView view;
    OK(cd_world_work(world, worker, &view, &diagnostic));
    assert(view.id == task && view.progress == 1 && view.paid == 1);
    OK(cd_world_employer_open(world, 2, 0, &diagnostic));
    OK(cd_world_step(world, 3, &diagnostic));
    OK(cd_world_work(world, worker, &view, &diagnostic));
    assert(view.id == task && view.progress == 1 && view.paid == 1 &&
           view.state == CD_TASK_WAITING);
    OK(cd_world_employer_open(world, 2, 1, &diagnostic));
    for (unsigned minute = 0; minute < 100 && view.state != CD_TASK_DONE; ++minute) {
      OK(cd_world_step(world, 1, &diagnostic));
      OK(cd_world_work(world, worker, &view, &diagnostic));
    }
    assert(view.state == CD_TASK_DONE && view.paid == recipe->worker_minutes);
    for (size_t n = 0; n < recipe->output_count; ++n) {
      CdDslStockView stock;
      OK(cd_world_dsl_stock(world, 2, recipe->outputs[n].resource, &stock, &diagnostic));
      assert(stock.available == recipe->outputs[n].quantity && !stock.reserved);
    }
    assert(cd_dsl_stock_valid(world));
    cd_world_close(world);
    ++verified;
  }
  cd_content_close(content);
  printf("{\"recipes_passed\":%u,\"interruptions_passed\":%u,\"ok\":true}\n", verified, verified);
}

static void retail_matrix(void) {
  static const char *const cases[][2] = {
    {"grocer", "bread"}, {"draper", "woven_cloth"}, {"ironmonger", "steel_stock"},
    {"bookseller", "paper"}, {"coal_merchant", "coal"},
  };
  CdContent *content = NULL;
  OK(cd_content_open("build/fixtures/dsl-commercial.bin", &content, &diagnostic));
  content->walk_units_per_minute = 769;
  for (size_t n = 0; n < sizeof cases / sizeof cases[0]; ++n) {
    CdScenario scenario = {12, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
    CdWorld *world = NULL;
    OK(cd_world_open(content, &scenario, &world, &diagnostic));
    CdPersonRef staff[] = {world->people[0].civil.ref, world->people[1].civil.ref,
                          world->people[4].civil.ref, world->people[5].civil.ref,
                          world->people[8].civil.ref, world->people[9].civil.ref};
    for (unsigned i = 0; i < 6; ++i) {
      OK(cd_world_place(world, staff[i], 110, &diagnostic));
      OK(cd_world_enter(world, staff[i], 303, 0, 0, &diagnostic));
      wait_inside(world, staff[i], cd_polygon_index(content, 2), 120);
    }
    OK(cd_world_employer(world, 2, "Funded retail fixture", 6, "Commercial premises", &diagnostic));
    CdAccount till = {.kind = CD_ACCOUNT_EMPLOYER, .id = 2};
    CdAccount buyer = {.kind = CD_ACCOUNT_PERSON, .person = staff[5]};
    OK(cd_world_money_import(world, till, 6000, "Retail payroll", &diagnostic));
    OK(cd_world_money_import(world, buyer, 100, "Buyer budget", &diagnostic));
    CdJobSpec spec = {2, 303, 0, 0, 480, 1200, 1, 0, 0, "Funded shop post"};
    CdEntityId jobs[5];
    for (unsigned stage = 1; stage <= 5; ++stage) {
      char id[96];
      snprintf(id, sizeof id, "%s.%u", cases[n][0], stage);
      OK(cd_world_dsl_job_create(world, &spec, id, &jobs[stage - 1], &diagnostic));
      if (stage == 1)
        continue;
      const CdDslRole *role = cd_dsl_role(id);
      fixture_credential(world, staff[stage - 1], role);
      if (role->owner)
        OK(cd_world_owner(world, 2, staff[stage - 1], &diagnostic));
      OK(cd_world_job_assign(world, jobs[stage - 1], staff[stage - 1],
                              (CdPersonRef){0}, 0, &diagnostic));
    }
    OK(cd_world_job_assign(world, jobs[0], staff[0], staff[2], 0, &diagnostic));
    OK(cd_world_dsl_import(world, 2, cases[n][1], 9, "Shop stock", &diagnostic));
    assert(cd_world_dsl_retail_open(world, staff[1], 2, 0, &diagnostic) != CD_OK);
    OK(cd_world_dsl_retail_open(world, staff[3], 2, 0, &diagnostic));
    assert(cd_world_dsl_purchase_at_post(world, staff[5], 2, jobs[1], cases[n][1],
                                         0, 1, 1, 5, &diagnostic) != CD_OK);
    OK(cd_world_dsl_retail_open(world, staff[4], 2, 1, &diagnostic));
    for (unsigned stage = 0; stage < 3; ++stage) {
      OK(cd_world_dsl_purchase_at_post(world, staff[5], 2, jobs[stage], cases[n][1],
                                       0, stage + 1, 1, 5, &diagnostic));
      CdDslCargoView cargo;
      for (unsigned i = 0; i < 10; ++i) {
        OK(cd_world_step(world, 1, &diagnostic));
        OK(cd_world_dsl_cargo(world, staff[5], &cargo, &diagnostic));
        if (!cargo.pending_quantity)
          break;
      }
      assert(!cargo.pending_quantity && cargo.carried == stage + 1);
      CdCitizen *clerk = cd_citizen(world, staff[stage]);
      assert(clerk->wallet == 1 &&
             cd_dsl_role_minutes(clerk, cd_dsl_job_role(cd_job(world, jobs[stage])->view.occupation)) == 1);
    }
    uint64_t balance;
    OK(cd_world_balance(world, buyer, &balance, &diagnostic));
    assert(balance == 85);
    OK(cd_world_balance(world, till, &balance, &diagnostic));
    assert(balance == 6012 && cd_dsl_stock_valid(world));
    cd_world_close(world);
  }
  cd_content_close(content);
  puts("{\"retail_families\":5,\"retail_roles\":25,\"ok\":true}");
}

static void transport_hospitality(CdContent *content) {
  CdWorld *world = NULL;
  CdScenario scenario = {12, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
  OK(cd_world_open(content, &scenario, &world, &diagnostic));
  uint32_t from = CD_NONE, to = CD_NONE;
  for (uint32_t a = 0; a < content->nd && from == CD_NONE; ++a) {
    if (!content->doors[a].reviewed || content->polygons[content->doors[a].building].capacity < 6 || strcmp(content->polygons[content->doors[a].building].use, "commercial"))
      continue;
    for (uint32_t b = a + 1; b < content->nd; ++b) {
      if (!content->doors[b].reviewed || strcmp(content->polygons[content->doors[b].building].use, "commercial"))
        continue;
      CdRoute route;
      if (cd_route_indices_mode(world, content->doors[a].node, content->doors[b].node, 1, &route) == CD_OK &&
          route.count && route.cost < 100) {
        from = a;
        to = b;
        break;
      }
    }
  }
  assert(from != CD_NONE && to != CD_NONE);
  uint32_t source = content->doors[from].building, destination = content->doors[to].building;
  CdEntityId depot = content->polygons[source].id, stop = content->polygons[destination].id;
  snprintf(world->premises[source].scenario_use, sizeof world->premises[source].scenario_use,
           "%s", "transport");
  for (unsigned i = 0; i < 12; ++i) {
    CdPersonRef person = world->people[i].civil.ref;
    OK(cd_world_place(world, person, content->nodes[content->doors[from].node].id, &diagnostic));
    OK(cd_world_enter(world, person, content->doors[from].id, 0, 0, &diagnostic));
    wait_inside(world, person, source, 120);
  }
  OK(cd_world_employer(world, depot, "Technical transport depot", 6,
                       "Actual mapped technical road scenario", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = depot},
                           4000, "Transport payroll", &diagnostic));
  OK(cd_world_goods_import(world, depot, CD_TOOL, 4, "Crew equipment", &diagnostic));
  OK(cd_world_goods_import(world, depot, CD_CLAY, 9, "Finite depot freight", &diagnostic));
  CdEntityId vehicles[4], posts[4];
  const unsigned crew[] = {0, 1, 4, 5};
  const char *const roles[] = {"road_haulage.2", "bus_operations.3", "goods_carrier.2", "postal_service.3"};
  const CdVehicleKind modes[] = {CD_ROAD_FREIGHT, CD_ROAD_PASSENGER, CD_HANDCART, CD_HANDCART};
  CdPersonRef passenger = world->people[2].civil.ref;
  for (unsigned i = 0; i < 4; ++i) {
    CdPersonRef driver = world->people[crew[i]].civil.ref;
    fixture_credential(world, driver, cd_dsl_role(roles[i]));
    OK(cd_world_authorize(world, driver, CD_AUTH_CARRIER, "Reviewed transport crew", &diagnostic));
    OK(cd_world_goods_order(world, driver, depot, CD_TOOL, 1, 0, &diagnostic));
    OK(cd_world_step(world, 1, &diagnostic));
    CdJobSpec spec = {depot, content->doors[from].id, 0, 0, 480, 1050, 1, 0, 0, "Funded transport post"};
    OK(cd_world_dsl_job_create(world, &spec, roles[i], &posts[i], &diagnostic));
    OK(cd_world_job_assign(world, posts[i], driver, (CdPersonRef){0}, 0, &diagnostic));
    CdVehicleSpec vehicle = {posts[i], modes[i], i == 1 ? 0 : 3, "Qualified technical vehicle"};
    OK(cd_world_vehicle(world, &vehicle, &vehicles[i], &diagnostic));
  }
  CdVehicleSpec rail = {posts[0], CD_RAIL_VEHICLE, 0, "Road post cannot operate rail"};
  CdEntityId rejected_rail;
  assert(cd_world_vehicle(world, &rail, &rejected_rail, &diagnostic) == CD_UNSUPPORTED);
  for (unsigned stage = 4; stage <= 5; ++stage) {
    CdPersonRef manager = world->people[stage + 4].civil.ref;
    const char *role = stage == 4 ? "road_haulage.4" : "road_haulage.5";
    fixture_credential(world, manager, cd_dsl_role(role));
    CdJobSpec spec = {depot, content->doors[from].id, 0, 0, 480, 1050, 1, 0, 0,
                      "Funded road dispatch post"};
    CdEntityId job;
    OK(cd_world_dsl_job_create(world, &spec, role, &job, &diagnostic));
    OK(cd_world_job_assign(world, job, manager, (CdPersonRef){0}, 0, &diagnostic));
  }
  assert(cd_world_dsl_transport_dispatch(world, world->people[0].civil.ref,
      vehicles[0], 1, depot, stop, CD_CLAY, 0, 3, (CdPersonRef){0}, &diagnostic) != CD_OK);
  OK(cd_world_dsl_transport_dispatch(world, world->people[8].civil.ref,
      vehicles[0], 1, depot, stop, CD_CLAY, 0, 3, (CdPersonRef){0}, &diagnostic));
  OK(cd_world_dsl_transport_dispatch(world, world->people[9].civil.ref,
      vehicles[0], 1, depot, stop, CD_CLAY, 0, 3, (CdPersonRef){0}, &diagnostic));
  for (unsigned i = 1; i < 4; ++i)
    OK(cd_world_transport(world, vehicles[i], 1, depot, stop, CD_CLAY, 0, i == 1 ? 0 : 3,
                          i == 1 ? passenger : (CdPersonRef){0}, &diagnostic));
  OK(cd_world_step(world, 2, &diagnostic));
  CdVehicleView freight, bus;
  OK(cd_world_vehicle_view(world, vehicles[0], &freight, &diagnostic));
  assert(freight.loaded > 0);
  OK(cd_world_vehicle_stop(world, vehicles[0], 1, &diagnostic));
  unsigned custody = freight.loaded;
  OK(cd_world_step(world, 3, &diagnostic));
  OK(cd_world_vehicle_view(world, vehicles[0], &freight, &diagnostic));
  assert(freight.phase == CD_VEHICLE_STOPPED && freight.loaded == custody);
  OK(cd_world_vehicle_stop(world, vehicles[0], 0, &diagnostic));
  CdVehicleView goods, post;
  for (unsigned minute = 0; minute < 180; ++minute) {
    OK(cd_world_vehicle_view(world, vehicles[0], &freight, &diagnostic));
    OK(cd_world_vehicle_view(world, vehicles[1], &bus, &diagnostic));
    OK(cd_world_vehicle_view(world, vehicles[2], &goods, &diagnostic));
    OK(cd_world_vehicle_view(world, vehicles[3], &post, &diagnostic));
    if (freight.phase == CD_VEHICLE_DONE && bus.phase == CD_VEHICLE_DONE &&
        goods.phase == CD_VEHICLE_DONE && post.phase == CD_VEHICLE_DONE)
      break;
    OK(cd_world_step(world, 1, &diagnostic));
  }
  assert(freight.delivered == 3 && goods.delivered == 3 && post.delivered == 3 &&
         bus.phase == CD_VEHICLE_DONE && world->people[2].inside == destination &&
         !world->people[2].vehicle && world->premises[destination].goods[CD_CLAY] == 9);
  for (unsigned i = 0; i < 4; ++i) {
    assert(world->people[crew[i]].wallet);
    OK(cd_world_job_close(world, posts[i], &diagnostic));
  }
  CdPersonRef host = world->people[0].civil.ref;
  OK(cd_world_employer(world, stop, "Funded hotel", 1, "Technical hotel trial", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = stop},
                           600, "Hotel payroll", &diagnostic));
  OK(cd_world_dsl_certify(world, host, "hotel_service.2", "Hotel service assessment", &diagnostic));
  CdJobSpec host_spec = {stop, content->doors[to].id, 0, 0, 480, 1000, 1, 0, 0, "Hotel shift"};
  CdEntityId hotel;
  OK(cd_world_dsl_job_create(world, &host_spec, "hotel_service.2", &hotel, &diagnostic));
  OK(cd_world_job_assign(world, hotel, host, (CdPersonRef){0}, 0, &diagnostic));
  OK(cd_world_dsl_hotel_visit(world, hotel, passenger, 1, &diagnostic));
  assert(world->people[2].presence == stop && world->visitors[destination] == 1);
  CdRosterSpec roster = {hotel, 1u << CD_SERVICE_MEAL, "Hotel meal service", "Funded hospitality trial"};
  CdEntityId provider;
  OK(cd_world_dsl_hospitality_roster(world, &roster, &provider, &diagnostic));
  OK(cd_world_food_store(world, stop, 0, "Hotel finite meal store", &diagnostic));
  OK(cd_world_food_import(world, (CdFoodEndpoint){CD_FOOD_STORE, stop}, 1, 0,
                          "Hotel meal import", &diagnostic));
  CdServiceSpec meal = {provider, 0, CD_SERVICE_MEAL, stop, passenger, cd_now(world) + 30,
                        1, "Funded hotel meal"};
  uint64_t request;
  OK(cd_world_dispatch(world, 1, &meal, &request, &diagnostic));
  CdServiceView service;
  for (unsigned minute = 0; minute < 50; ++minute) {
    OK(cd_world_service(world, request, &service, &diagnostic));
    if (service.state == CD_TASK_DONE)
      break;
    OK(cd_world_step(world, 1, &diagnostic));
  }
  assert(service.state == CD_TASK_DONE && service.paid == 1);
  OK(cd_world_dsl_hotel_visit(world, hotel, passenger, 0, &diagnostic));
  assert(!world->people[2].presence && !world->visitors[destination]);
  CdEconomyLedger ledger;
  OK(cd_world_economy_ledger(world, &ledger, &diagnostic));
  assert(ledger.money_created == ledger.money_stock);
  printf("{\"road_freight_units\":%u,\"road_passengers\":1,\"goods_cart_units\":3,\"postal_cart_units\":3,\"hotel_meals\":1,\"hotel_visits\":1,\"dispatch_authority\":true,\"rail_refused\":true,\"ok\":true}\n", freight.delivered);
  cd_world_close(world);
}

static void transport_stage_fixture(const char *family, CdVehicleKind kind) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  int rail = kind == CD_RAIL_VEHICLE;
  int passengers = rail || kind == CD_ROAD_PASSENGER;
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  CdScenario scenario = {12, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
  OK(cd_world_open(content, &scenario, &world, &diagnostic));
  uint32_t from = CD_NONE, to = CD_NONE;
  CdRoute route;
  for (uint32_t i = 0; i < content->nd && from == CD_NONE; ++i) {
    const CdDoor *door = &content->doors[i];
    const CdPolygon *building = &content->polygons[door->building];
    if (!door->reviewed || building->capacity < 6 ||
        strcmp(rail ? building->name : building->use, rail ? "Canterbury West" : "commercial"))
      continue;
    for (uint32_t j = 0; j < content->nd; ++j) {
      const CdDoor *target = &content->doors[j];
      const CdPolygon *other = &content->polygons[target->building];
      if (!target->reviewed || door->building == target->building ||
          strcmp(rail ? other->source : other->use,
                 rail ? "buildings-ae21ffc2ad9ed726de63" : "commercial"))
        continue;
      if (cd_route_indices_mode(world, door->node, target->node,
                                cd_vehicle_route_mode(kind), &route) == CD_OK &&
          route.count && (rail || route.cost < 100)) {
        from = i;
        to = j;
        break;
      }
    }
  }
  assert(from != CD_NONE && to != CD_NONE);
  if (rail)
    assert(route.count == 39);
  CdEntityId origin = content->polygons[content->doors[from].building].id;
  CdEntityId destination = content->polygons[content->doors[to].building].id;
  CdEntityId entrance = content->doors[from].id;
  CdEntityId start = content->nodes[content->doors[from].node].id;
  snprintf(world->premises[content->doors[from].building].scenario_use,
           sizeof world->premises[content->doors[from].building].scenario_use, "%s", "transport");

  const unsigned people[] = {0, 1, 4, 5, 8, 9};
  CdPersonRef staff[6];
  CdEntityId posts[5];
  for (size_t i = 0; i < 6; ++i) {
    staff[i] = world->people[people[i]].civil.ref;
    OK(cd_world_place(world, staff[i], start, &diagnostic));
    OK(cd_world_enter(world, staff[i], entrance, 0, 0, &diagnostic));
    wait_inside(world, staff[i], cd_polygon_index(content, origin), 120);
  }
  OK(cd_world_employer(world, origin, "Technical transport depot", 6, "Reviewed funded map transport depot", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = origin},
                           4000, "Railway fixture payroll", &diagnostic));
  CdJobSpec spec = {origin, entrance, 0, 0, 480, 1100, 1, 0, 0, "Funded railway fixture post"};
  for (unsigned stage = 1; stage <= 5; ++stage) {
    char name[96];
    snprintf(name, sizeof name, "%s.%u", family, stage);
    OK(cd_world_dsl_job_create(world, &spec, name, &posts[stage - 1], &diagnostic));
    if (stage > 1) {
      fixture_credential(world, staff[stage - 1], cd_dsl_role(name));
      OK(cd_world_job_assign(world, posts[stage - 1], staff[stage - 1], (CdPersonRef){0}, 0, &diagnostic));
    }
  }
  OK(cd_world_job_assign(world, posts[0], staff[0], staff[2], 0, &diagnostic));
  OK(cd_world_dsl_training_start(world, posts[0], 1, &diagnostic));
  OK(cd_world_step(world, 1, &diagnostic));
  const CdDslRole *trainee = cd_dsl_job_role(cd_job(world, posts[0])->view.occupation);
  assert(cd_dsl_role_minutes(cd_citizen(world, staff[0]), trainee) == 1);
  if (passengers) {
    CdRosterSpec roster = {posts[1], 1u << CD_SERVICE_VISIT, "Station assistance", "Funded station fixture"};
    CdEntityId provider;
    OK(cd_world_dsl_station_roster(world, &roster, &provider, &diagnostic));
    CdServiceSpec help = {provider, 0, CD_SERVICE_VISIT, origin, staff[5], cd_now(world) + 20,
                          1, "Station assistance"};
    uint64_t request;
    OK(cd_world_dispatch(world, 1, &help, &request, &diagnostic));
    OK(cd_world_step(world, 1, &diagnostic));
    CdServiceView service;
    OK(cd_world_service(world, request, &service, &diagnostic));
    assert(service.state == CD_TASK_DONE && service.paid == 1);
  } else {
    OK(cd_world_goods_import(world, origin, CD_CLAY, 2, "Finite fixture parcels", &diagnostic));
  }

  OK(cd_world_goods_import(world, origin, CD_TOOL, 2, "Operating equipment", &diagnostic));
  for (unsigned stage = passengers ? 2 : 1; stage <= 2; ++stage) {
    CdPersonRef rider = passengers ? staff[5] : (CdPersonRef){0};
    unsigned units = passengers ? 0 : 1;
    OK(cd_world_authorize(world, staff[stage], CD_AUTH_CARRIER, "Explicit technical operator", &diagnostic));
    OK(cd_world_goods_order(world, staff[stage], origin, CD_TOOL, 1, 0, &diagnostic));
    OK(cd_world_step(world, 1, &diagnostic));
    CdVehicleSpec spec_vehicle = {posts[stage], kind, units, "Explicit transport fixture vehicle"};
    CdEntityId vehicle;
    OK(cd_world_vehicle(world, &spec_vehicle, &vehicle, &diagnostic));
    CdPersonRef dispatcher = passengers || stage == 1 ? staff[3] : staff[4];
    OK(cd_world_dsl_transport_dispatch(world, dispatcher, vehicle, 1, origin, destination,
                                        CD_CLAY, 0, units, rider, &diagnostic));
    OK(cd_world_step(world, 1, &diagnostic));
    CdVehicleView view;
    OK(cd_world_vehicle_view(world, vehicle, &view, &diagnostic));
    assert(passengers ? cd_citizen(world, rider)->vehicle == vehicle : view.loaded == 1);
    OK(cd_world_vehicle_stop(world, vehicle, 1, &diagnostic));
    OK(cd_world_step(world, 3, &diagnostic));
    OK(cd_world_vehicle_view(world, vehicle, &view, &diagnostic));
    assert(passengers ? cd_citizen(world, rider)->vehicle == vehicle : view.loaded == 1);
    OK(cd_world_vehicle_stop(world, vehicle, 0, &diagnostic));
    for (unsigned i = 0; i < 500; ++i) {
      OK(cd_world_vehicle_view(world, vehicle, &view, &diagnostic));
      if (view.phase == CD_VEHICLE_DONE)
        break;
      OK(cd_world_step(world, 1, &diagnostic));
    }
    assert(view.phase == CD_VEHICLE_DONE && cd_citizen(world, staff[stage])->wallet);
    if (passengers) {
      assert(!cd_citizen(world, rider)->vehicle &&
             cd_citizen(world, rider)->inside == cd_polygon_index(content, destination));
      OK(cd_world_dsl_transport_dispatch(world, staff[4], vehicle, 2, destination, origin,
                                          0, 0, 0, rider, &diagnostic));
      for (unsigned i = 0; i < 500; ++i) {
        OK(cd_world_vehicle_view(world, vehicle, &view, &diagnostic));
        if (view.phase == CD_VEHICLE_DONE)
          break;
        OK(cd_world_step(world, 1, &diagnostic));
      }
      assert(view.phase == CD_VEHICLE_DONE && !cd_citizen(world, rider)->vehicle &&
             cd_citizen(world, rider)->inside == cd_polygon_index(content, origin));
    } else {
      assert(view.delivered == 1);
    }
  }
  CdEconomyLedger ledger;
  OK(cd_world_economy_ledger(world, &ledger, &diagnostic));
  assert(ledger.money_created == ledger.money_stock);
  cd_world_close(world);
  cd_content_close(content);
}

static void transport_stage_matrix(void) {
  transport_stage_fixture("goods_carrier", CD_HANDCART);
  transport_stage_fixture("postal_service", CD_HANDCART);
  transport_stage_fixture("road_haulage", CD_ROAD_FREIGHT);
  transport_stage_fixture("bus_operations", CD_ROAD_PASSENGER);
  transport_stage_fixture("railway_operations", CD_RAIL_VEHICLE);
  puts("{\"transport_roles\":25,\"mapped_rail_roles\":5,\"rail_trips\":2,\"ok\":true}");
}

static void public_service(CdServiceKind kind, const char *name, const char *role,
                           const char *use, unsigned authority, unsigned skill) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  CdScenario scenario = {12, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
  OK(cd_world_open(content, &scenario, &world, &diagnostic));
  uint32_t entrance = CD_NONE;
  for (uint32_t i = 0; i < content->nd; ++i)
    if (content->doors[i].reviewed &&
        !strcmp(content->polygons[content->doors[i].building].use, use)) {
      entrance = i;
      break;
    }
  assert(entrance != CD_NONE);
  const CdDoor *door = &content->doors[entrance];
  CdEntityId building = content->polygons[door->building].id;
  CdPersonRef clerk = world->people[0].civil.ref;
  CdPersonRef recipient = {0};
  if (kind == CD_SERVICE_LIBRARY || kind == CD_SERVICE_WATER || kind == CD_SERVICE_HEALTH ||
      kind == CD_SERVICE_VISIT || kind == CD_SERVICE_CHARITY ||
      kind == CD_SERVICE_EDUCATION) {
    recipient = world->people[1].civil.ref;
    OK(cd_world_place(world, recipient, content->nodes[door->node].id, &diagnostic));
    OK(cd_world_enter(world, recipient, door->id, 0, 0, &diagnostic));
    wait_inside(world, recipient, door->building, 120);
  }
  if (kind == CD_SERVICE_WATER) {
    snprintf(world->premises[door->building].scenario_use,
             sizeof world->premises[door->building].scenario_use, "%s", "utility_support");
    OK(cd_world_water_import(world, building, 1, "Funded water stock", &diagnostic));
  }
  if (kind == CD_SERVICE_CHARITY) {
    snprintf(world->premises[door->building].scenario_use,
             sizeof world->premises[door->building].scenario_use, "%s", "civic_support");
    OK(cd_world_goods_import(world, building, CD_TEXTILE, 1, "Funded textile stock", &diagnostic));
  }
  if (kind == CD_SERVICE_MIDWIFERY) {
    OK(cd_world_households_allocate(world, &diagnostic));
    OK(cd_world_birth(world, world->people[1].civil.ref, (CdPersonRef){0},
                      "Funded neonatal fixture birth", &recipient, &diagnostic));
    cd_native_energy_set(world->native, recipient, cd_native_full());
    OK(cd_world_place(world, recipient, content->nodes[door->node].id, &diagnostic));
    OK(cd_world_enter(world, recipient, door->id, 0, 0, &diagnostic));
    wait_inside(world, recipient, door->building, 120);
  }
  if (kind == CD_SERVICE_CLEANING)
    snprintf(world->premises[door->building].scenario_use,
             sizeof world->premises[door->building].scenario_use, "%s", "residential");
  if (kind == CD_SERVICE_PHARMACY) {
    OK(cd_world_clinic(world, building, 1, 0, "Funded dispensary", &diagnostic));
  }
  if (kind == CD_SERVICE_LIBRARY)
    OK(cd_world_dsl_import(world, building, "school_book", 1, "Reading-room stock", &diagnostic));
  if (kind == CD_SERVICE_HEALTH || kind == CD_SERVICE_MIDWIFERY)
    OK(cd_world_clinic(world, building, 1, 1, "Funded clinic stock", &diagnostic));

  OK(cd_world_place(world, clerk, content->nodes[door->node].id, &diagnostic));
  OK(cd_world_enter(world, clerk, door->id, 0, 0, &diagnostic));
  wait_inside(world, clerk, door->building, 120);
  OK(cd_world_employer(world, building, "Funded public service office", 1,
                       "Technical Canterbury service trial", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = building},
                           620, "Administration payroll", &diagnostic));
  CdJobSpec post = {building, door->id, 0, 0, 480, 1100, 1, 0, 0, "Named administration post"};
  CdEntityId job, roster;
  OK(cd_world_dsl_job_create(world, &post, role, &job, &diagnostic));
  fixture_credential(world, clerk, cd_dsl_role(role));
  if (cd_dsl_role(role)->owner)
    OK(cd_world_owner(world, building, clerk, &diagnostic));
  OK(cd_world_job_assign(world, job, clerk, (CdPersonRef){0}, 0, &diagnostic));
  OK(cd_world_training(world, clerk, skill, "Technical community skills", &diagnostic));
  if (authority)
    OK(cd_world_authorize(world, clerk, authority, "Technical service authority", &diagnostic));
  CdRosterSpec capability = {job, 1u << (kind == CD_SERVICE_POLICE ? CD_SERVICE_ADMIN : CD_SERVICE_POLICE), "Public office", "Funded test"};
  assert(cd_world_dsl_public_roster(world, &capability, &roster, &diagnostic) == CD_UNSUPPORTED);
  capability.capabilities = 1u << kind;
  OK(cd_world_dsl_public_roster(world, &capability, &roster, &diagnostic));
  CdServiceSpec request = {roster, 0, kind, building, recipient, cd_now(world) + 20,
                           1, "Administration request"};
  uint64_t id, duplicate;
  if (kind == CD_SERVICE_MIDWIFERY) {
    request.subject = clerk;
    assert(cd_world_dispatch(world, 1, &request, &id, &diagnostic) == CD_INVALID_ARGUMENT);
    request.subject = recipient;
  }
  request.minutes = 621;
  assert(cd_world_dispatch(world, 1, &request, &id, &diagnostic) == CD_CAPACITY);
  request.minutes = 1;
  if (kind == CD_SERVICE_FIRE) {
    CdPressureSpec ignition = {CD_PRESSURE_FIRE, building, cd_now(world) + 1,
                               20, 2, "Technical funded-map fire"};
    uint64_t pressure;
    OK(cd_world_pressure(world, 1, &ignition, &pressure, &diagnostic));
    OK(cd_world_step(world, 1, &diagnostic));
    assert(world->premises[door->building].fire);
  }
  OK(cd_world_dispatch(world, 1, &request, &id, &diagnostic));
  OK(cd_world_dispatch(world, 1, &request, &duplicate, &diagnostic));
  assert(id == duplicate);
  if (kind == CD_SERVICE_FIRE) {
    OK(cd_world_step(world, 2, &diagnostic));
    assert(!world->services[id - 1].view.progress);
    OK(cd_world_goods_import(world, building, CD_TOOL, 1, "Response tool", &diagnostic));
    OK(cd_world_water_import(world, building, 1, "Response water", &diagnostic));
  }
  if (kind == CD_SERVICE_CLEANING || kind == CD_SERVICE_PHARMACY) {
    OK(cd_world_step(world, 2, &diagnostic));
    assert(!world->services[id - 1].view.progress);
    OK(cd_world_dsl_import(world, building, kind == CD_SERVICE_CLEANING ? "soap_bar" : "medicine_dose",
                           1, "Finite service material", &diagnostic));
  }
  CdServiceView service;
  for (unsigned minute = 0; minute < 20; ++minute) {
    OK(cd_world_step(world, 1, &diagnostic));
    OK(cd_world_service(world, id, &service, &diagnostic));
    if (service.state == CD_TASK_DONE)
      break;
  }
  assert(service.state == CD_TASK_DONE && service.progress == 1 && service.paid == 1);
  assert(cd_dsl_role_minutes(cd_citizen(world, clerk), cd_dsl_role(role)) == 1);
  if (kind == CD_SERVICE_WATER)
    assert(!world->premises[door->building].water && cd_citizen(world, recipient)->water == 1);
  if (kind == CD_SERVICE_HEALTH || kind == CD_SERVICE_MIDWIFERY)
    assert(!world->premises[door->building].medicine && !world->premises[door->building].occupied);
  if (kind == CD_SERVICE_CLEANING || kind == CD_SERVICE_PHARMACY) {
    CdDslStockView stock;
    OK(cd_world_dsl_stock(world, building, kind == CD_SERVICE_CLEANING ? "soap_bar" : "medicine_dose",
                          &stock, &diagnostic));
    assert(!stock.available && cd_dsl_stock_valid(world));
    if (kind == CD_SERVICE_PHARMACY)
      assert(world->premises[door->building].medicine == 1);
  }
  if (kind == CD_SERVICE_CHARITY)
    assert(!world->premises[door->building].goods[CD_TEXTILE] &&
           cd_citizen(world, recipient)->goods[CD_TEXTILE] == 1);
  if (kind == CD_SERVICE_FIRE)
    assert(!world->premises[door->building].fire && world->water_used == 1);
  cd_world_close(world);
  cd_content_close(content);
  printf("{\"%s\":1,\"ok\":true}\n", name);
}

static void repair_and_worship(void) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  CdScenario scenario = {12, 1940, {1940, 1, 1, 900}, "synthetic-ordinary-v1"};
  OK(cd_world_open(content, &scenario, &world, &diagnostic));
  uint32_t workshop = CD_NONE, church = CD_NONE;
  CdEntityId institution = 0;
  for (uint32_t i = 0; i < content->nd; ++i) {
    const CdDoor *door = &content->doors[i];
    const CdPolygon *building = &content->polygons[door->building];
    if (!door->reviewed)
      continue;
    if (workshop == CD_NONE && !strcmp(building->use, "workshop"))
      workshop = i;
    if (!strcmp(building->name, "Canterbury Cathedral") && building->capacity >= 2)
      for (uint32_t site = 0; site < content->ns; ++site)
        if (content->sites[site].located &&
            !strcmp(content->sites[site].name, "Canterbury Cathedral") &&
            !strcmp(content->sites[site].denomination, "anglican")) {
          institution = content->sites[site].id;
          church = i;
          break;
        }
  }
  assert(workshop != CD_NONE && church != CD_NONE);
  CdPersonRef carpenter = world->people[0].civil.ref;
  OK(cd_world_place(world, carpenter, content->nodes[content->doors[workshop].node].id, &diagnostic));
  supply_arrival(world, carpenter, workshop);
  CdEntityId site = content->polygons[content->doors[workshop].building].id;
  CdEntityId job = supply_job(world, carpenter, workshop, "carpenter.2");
  CdDevelopmentSpec repair = {CD_DEV_REPAIR, 0, 4, NULL, "Funded map repair"};
  uint64_t project, task;
  OK(cd_world_wear(world, site, 200, "Technical wear", &diagnostic));
  assert(cd_world_project(world, site, 1, &repair, &project, &diagnostic) == CD_CAPACITY);
  OK(cd_world_goods_import(world, site, CD_WOOD, 1, "Repair wood", &diagnostic));
  OK(cd_world_goods_import(world, site, CD_TOOL, 1, "Repair equipment", &diagnostic));
  OK(cd_world_project(world, site, 1, &repair, &project, &diagnostic));
  CdWorkSpec order = {job, project, CD_RECIPE_DEVELOPMENT, 4, "Named carpenter repair"};
  OK(cd_world_work_submit(world, carpenter, 1, &order, &task, &diagnostic));
  finish_supply_work(world, carpenter, task, 4);
  assert(world->premises[content->doors[workshop].building].condition == 1000);

  CdPersonRef priest = world->people[1].civil.ref, visitor = world->people[2].civil.ref;
  const CdDoor *entrance = &content->doors[church];
  site = content->polygons[entrance->building].id;
  OK(cd_world_institution_link(world, institution, site, "Reviewed Cathedral footprint", &diagnostic));
  OK(cd_world_institution_operate(world, institution, 1, 1, "Anglican priest",
                                   "Explicit funded scenario", &diagnostic));
  OK(cd_world_place(world, priest, content->nodes[entrance->node].id, &diagnostic));
  supply_arrival(world, priest, church);
  OK(cd_world_place(world, visitor, content->nodes[entrance->node].id, &diagnostic));
  supply_arrival(world, visitor, church);
  OK(cd_world_employer(world, site, "Funded Anglican parish", 1, "Technical parish", &diagnostic));
  OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = site},
                           600, "Parish payroll", &diagnostic));
  const char *role = "anglican_ministry.3";
  OK(cd_world_religious_qualification(world, priest, institution, CD_OFFICE_PRIEST,
                                         "Explicit parish appointment", &diagnostic));
  fixture_credential(world, priest, cd_dsl_role(role));
  OK(cd_world_training(world, priest, CD_SKILL_COMMUNITY, "Technical parish skills", &diagnostic));
  CdJobSpec post = {site, entrance->id, 0, institution, 900, 1310, 1, 0, 0, "Funded parish post"};
  OK(cd_world_dsl_job_create(world, &post, role, &job, &diagnostic));
  OK(cd_world_job_assign(world, job, priest, (CdPersonRef){0}, 0, &diagnostic));
  CdOfficeSpec office = {institution, job, CD_OFFICE_PRIEST, "Named Anglican office"};
  CdEntityId office_id;
  OK(cd_world_office_create(world, &office, &office_id, &diagnostic));
  CdGatheringSpec gathering = {office_id, CD_SERVICE_WORSHIP, cd_now(world) + 2,
                               3, 1, 0, 1, 0, "Funded parish gathering"};
  uint64_t event;
  OK(cd_world_gathering(world, 1, &gathering, &event, &diagnostic));
  OK(cd_world_attend(world, event, visitor, 1, &diagnostic));
  OK(cd_world_step(world, 7, &diagnostic));
  CdGatheringView result;
  OK(cd_world_gathering_view(world, event, &result, &diagnostic));
  assert(result.state == CD_TASK_DONE && result.paid && result.completed);
  cd_world_close(world);
  cd_content_close(content);
  puts("{\"building_repair\":1,\"religious_service\":1,\"community_gathering\":1,\"ok\":true}");
}

static void senior_public_roles(void) {
  static const struct {
    const char *career, *use;
    CdServiceKind kind;
    unsigned first, authority, skill;
  } cases[] = {
      {"medicine", "healthcare", CD_SERVICE_HEALTH, 4, 0, CD_SKILL_CARE},
      {"nursing", "healthcare", CD_SERVICE_HEALTH, 4, 0, CD_SKILL_CARE},
      {"midwifery", "healthcare", CD_SERVICE_MIDWIFERY, 2, 0, CD_SKILL_CARE},
      {"pharmacy", "healthcare", CD_SERVICE_PHARMACY, 2, 0, CD_SKILL_CARE},
      {"social_care", "care_support", CD_SERVICE_VISIT, 4, 0, CD_SKILL_CARE},
      {"school_teaching", "education", CD_SERVICE_EDUCATION, 4, CD_AUTH_TEACH, CD_SKILL_COMMUNITY},
      {"library_service", "civic", CD_SERVICE_LIBRARY, 2, 0, CD_SKILL_COMMUNITY},
      {"technical_instruction", "education", CD_SERVICE_EDUCATION, 4, CD_AUTH_TEACH, CD_SKILL_COMMUNITY},
      {"nursery_education", "education", CD_SERVICE_EDUCATION, 4, CD_AUTH_TEACH, CD_SKILL_COMMUNITY},
      {"office_administration", "commercial", CD_SERVICE_ADMIN, 4, CD_AUTH_ADMIN, CD_SKILL_COMMUNITY},
      {"municipal_administration", "civic", CD_SERVICE_ADMIN, 4, CD_AUTH_ADMIN, CD_SKILL_COMMUNITY},
      {"fire_service", "civic", CD_SERVICE_FIRE, 4, CD_AUTH_FIRE, CD_SKILL_CRAFT},
      {"police_service", "civic", CD_SERVICE_POLICE, 4, CD_AUTH_POLICE, CD_SKILL_COMMUNITY},
      {"waterworks", "industrial", CD_SERVICE_WATER, 2, 0, CD_SKILL_CRAFT},
      {"charitable_service", "civic", CD_SERVICE_CHARITY, 4, 0, CD_SKILL_COMMUNITY},
  };
  unsigned verified = 0;
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
    for (unsigned stage = cases[i].first; stage <= 5; ++stage) {
      char role[96];
      snprintf(role, sizeof role, "%s.%u", cases[i].career, stage);
      public_service(cases[i].kind, role, role, cases[i].use, cases[i].authority, cases[i].skill);
      verified++;
    }
  assert(verified == 38);
  printf("{\"public_roles\":%u,\"ok\":true}\n", verified);
}

static void institutional_support_roles(void) {
  static const struct {
    const char *role, *institution, *venue;
    CdOfficeKind office;
    CdServiceKind service;
  } cases[] = {
      {"anglican_ministry.2", "Canterbury Cathedral", "Canterbury Cathedral",
       CD_OFFICE_PRIEST, CD_SERVICE_VISIT},
      {"roman_catholic_ministry.2", "St Thomas of Canterbury Catholic Church", "Canterbury Cathedral Lodge",
       CD_OFFICE_PRIEST, CD_SERVICE_VISIT},
      {"jewish_ministry.2", "Old Synagogue", "Beerling Hall",
       CD_OFFICE_RABBI, CD_SERVICE_EDUCATION},
  };
  CdContent *content = NULL;
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  for (size_t n = 0; n < sizeof cases / sizeof cases[0]; ++n) {
    CdWorld *world = NULL;
    CdScenario scenario = {12, 1940, {1940, 1, 1, 900}, "synthetic-ordinary-v1"};
    OK(cd_world_open(content, &scenario, &world, &diagnostic));
    uint32_t door = CD_NONE;
    for (uint32_t i = 0; i < content->nd; ++i)
      if (content->doors[i].reviewed &&
          !strcmp(content->polygons[content->doors[i].building].name, cases[n].venue) &&
          content->polygons[content->doors[i].building].capacity >= 2) {
        door = i;
        break;
      }
    CdEntityId institution = 0;
    for (uint32_t i = 0; i < content->ns; ++i)
      if (content->sites[i].located && !strcmp(content->sites[i].name, cases[n].institution))
        institution = content->sites[i].id;
    assert(door != CD_NONE && institution);
    const CdDoor *entrance = &content->doors[door];
    CdEntityId premises = content->polygons[entrance->building].id;
    snprintf(world->premises[entrance->building].scenario_use,
             sizeof world->premises[entrance->building].scenario_use, "%s", "religious");
    OK(cd_world_institution_link(world, institution, premises, "Explicit funded service venue", &diagnostic));
    OK(cd_world_institution_operate(world, institution, 1, 1, "Institutional support",
                                     "Technical appointment fixture", &diagnostic));
    CdPersonRef staff = world->people[0].civil.ref, visitor = world->people[1].civil.ref;
    OK(cd_world_place(world, staff, content->nodes[entrance->node].id, &diagnostic));
    OK(cd_world_place(world, visitor, content->nodes[entrance->node].id, &diagnostic));
    supply_arrival(world, staff, door);
    supply_arrival(world, visitor, door);
    OK(cd_world_employer(world, premises, "Institutional employer", 1, "Funded support post", &diagnostic));
    OK(cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = premises},
                             600, "Institutional payroll", &diagnostic));
    OK(cd_world_religious_qualification(world, staff, institution, cases[n].office,
                                           "Explicit tradition qualification", &diagnostic));
    const CdDslRole *role = cd_dsl_role(cases[n].role);
    fixture_credential(world, staff, role);
    OK(cd_world_training(world, staff, CD_SKILL_COMMUNITY, "Community skills", &diagnostic));
    if (cases[n].service == CD_SERVICE_EDUCATION)
      OK(cd_world_authorize(world, staff, CD_AUTH_TEACH, "Teaching permission", &diagnostic));
    CdJobSpec post = {premises, entrance->id, 0, institution, 900, 1310, 1, 0, 0, "Funded support post"};
    CdEntityId job, provider;
    OK(cd_world_dsl_job_create(world, &post, role->id, &job, &diagnostic));
    OK(cd_world_job_assign(world, job, staff, (CdPersonRef){0}, 0, &diagnostic));
    CdOfficeSpec office = {institution, job, cases[n].office, "Ordained office gate"};
    assert(cd_world_office_create(world, &office, &provider, &diagnostic) == CD_UNSUPPORTED);
    CdRosterSpec roster = {job, 1u << cases[n].service, "Institutional support", "Funded service"};
    OK(cd_world_dsl_public_roster(world, &roster, &provider, &diagnostic));
    CdServiceSpec request = {provider, 0, cases[n].service, premises, visitor,
                             cd_now(world) + 20, 1, "Institutional support"};
    uint64_t id;
    OK(cd_world_dispatch(world, 1, &request, &id, &diagnostic));
    OK(cd_world_step(world, 4, &diagnostic));
    CdServiceView service;
    OK(cd_world_service(world, id, &service, &diagnostic));
    assert(service.state == CD_TASK_DONE && service.paid == 1 &&
           cd_dsl_role_minutes(cd_citizen(world, staff), role) == 1);
    cd_world_close(world);
  }
  cd_content_close(content);
  printf("{\"institutional_roles\":%zu,\"roles\":[", sizeof cases / sizeof cases[0]);
  for (size_t n = 0; n < sizeof cases / sizeof cases[0]; ++n)
    printf("%s\"%s\"", n ? "," : "", cases[n].role);
  puts("],\"ok\":true}");
}

int main(int argc, char **argv) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  if (argc == 2 && !strcmp(argv[1], "--institutional-support-roles")) {
    institutional_support_roles();
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--senior-public-roles")) {
    senior_public_roles();
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--maternal-and-domestic")) {
    public_service(CD_SERVICE_CLEANING, "cleaning", "domestic_service.2", "care_support", 0, CD_SKILL_CRAFT);
    public_service(CD_SERVICE_PHARMACY, "pharmacy", "pharmacy.2", "healthcare", 0, CD_SKILL_CARE);
    public_service(CD_SERVICE_MIDWIFERY, "midwifery_care", "midwifery.2", "healthcare", 0, CD_SKILL_CARE);
    assert(!cd_memory_stats().total);
    puts("{\"cleaning\":1,\"pharmacy\":1,\"midwifery_care\":1,\"ok\":true}");
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--repair-and-worship")) {
    repair_and_worship();
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--community-services")) {
    public_service(CD_SERVICE_VISIT, "household_care", "social_care.2", "care_support", 0, CD_SKILL_CARE);
    public_service(CD_SERVICE_CHARITY, "charitable_support", "charitable_service.2", "civic", 0, CD_SKILL_COMMUNITY);
    public_service(CD_SERVICE_FIRE, "fire_response", "fire_service.2", "civic", CD_AUTH_FIRE, CD_SKILL_CRAFT);
    assert(!cd_memory_stats().total);
    puts("{\"household_care\":1,\"charitable_support\":1,\"fire_response\":1,\"ok\":true}");
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--library-access")) {
    public_service(CD_SERVICE_LIBRARY, "library_access", "library_service.2", "civic", 0, CD_SKILL_COMMUNITY);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--water-supply")) {
    public_service(CD_SERVICE_WATER, "water_supply", "waterworks.2", "industrial", 0, CD_SKILL_CRAFT);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--clinical-care")) {
    public_service(CD_SERVICE_HEALTH, "clinical_care", "nursing.2", "healthcare", 0, CD_SKILL_CARE);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--police-response")) {
    public_service(CD_SERVICE_POLICE, "police_response", "police_service.2", "civic", CD_AUTH_POLICE, CD_SKILL_COMMUNITY);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--public-administration")) {
    public_service(CD_SERVICE_ADMIN, "administration", "office_administration.2", "commercial", CD_AUTH_ADMIN, CD_SKILL_COMMUNITY);
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--transport-stages")) {
    transport_stage_matrix();
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--retail")) {
    retail_matrix();
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--recipes")) {
    recipe_matrix();
    assert(!cd_memory_stats().total);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--transitions")) {
    transition_matrix();
    assert(!cd_memory_stats().total);
    return 0;
  }
  OK(cd_content_open("build/content/CanterburyAccessTrial.bin", &content, &diagnostic));
  if (argc == 2 && !strcmp(argv[1], "--transport")) {
    transport_hospitality(content);
    cd_content_close(content);
    assert(!cd_memory_stats().total);
    return 0;
  }
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
  batch_mill_delivery(&world, &worker, dwelling, premises, entrance);
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
  supervised_milling(world, worker, job, premises, content->doors[entrance].id);
  funded_bread_chain(world, entrance);
  funded_book_chain(world, entrance);
  printf("{\"households\":1,\"residents\":4,\"jobs\":1,\"home\":\"%s\","
         "\"workplace\":\"%s\",\"arrival_minute\":%llu,\"return_minute\":%llu,"
         "\"bakery_fuel_g\":120,\"bakery_ash_g\":12,\"bakery_water_ml\":300,\"bakery_wastewater_ml\":50,\"school_books_delivered\":1,\"bread_delivered_g\":1000,\"bread_eaten_g\":100,\"retail_tokens\":12,\"batch_delivery_g\":1000,\"expired_g\":100,\"flour_g\":750,\"bran_g\":250,\"wages\":60,\"supervised_minutes\":60,\"work_training_contention\":true,\"ok\":true}\n",
         content->polygons[home].source, content->polygons[mill].source,
         (unsigned long long)arrival, (unsigned long long)returned);
  cd_world_close(world);
  cd_content_close(content);
  assert(!cd_memory_stats().total);
}
