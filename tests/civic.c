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
static CdPersonRef ref(CdWorld *w, unsigned i) {
  return w->people[i].civil.ref;
}
static void step(CdWorld *w, unsigned n) { OK(cd_world_step(w, n, &d)); }
static uint64_t hash(CdWorld *w) {
  uint64_t h;
  OK(cd_world_digest(w, &h, &d));
  return h;
}
static void arrive(CdWorld *w, CdPersonRef p) {
  for (unsigned i = 0; i < 500; i++) {
    CdTravelView t;
    OK(cd_world_travel_view(w, p, &t, &d));
    if (t.state == CD_ROUTE_ARRIVED)
      return;
    step(w, 1);
  }
  fputs("arrival timeout\n", stderr);
  abort();
}
static void inside(CdWorld *w, CdPersonRef p, unsigned b) {
  OK(cd_world_place(w, p, b == 1 ? 107 : 110, &d));
  OK(cd_world_enter(w, p, b == 1 ? 301 : 303, 0, 0, &d));
  arrive(w, p);
}
static void ledger(CdWorld *w) {
  CdEconomyLedger e;
  CdFoodLedger f;
  CdCivicSummary c;
  OK(cd_world_economy_ledger(w, &e, &d));
  OK(cd_world_food_ledger(w, &f, &d));
  OK(cd_world_civic_summary(w, &c, &d));
}
static CdEntityId job(CdWorld *w, CdPersonRef p, unsigned building,
                      unsigned occupation, unsigned skills,
                      CdEntityId institution) {
  OK(cd_world_training(w, p, skills, "technical qualification", &d));
  OK(cd_world_employer(w, building, "Explicit technical employer",
                       building == 1 ? 4 : 8, "technical capacity", &d));
  OK(cd_world_money_import(
      w, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = building}, 100000,
      "technical capital", &d));
  CdJobSpec s = {.workplace = building,
                 .entrance = building == 1 ? 301 : 303,
                 .occupation = occupation,
                 .institution = institution,
                 .shift_start = 0,
                 .shift_end = 0,
                 .wage = 2,
                 .basis =
                     "technical full-day availability; needs still preempt"};
  CdEntityId id;
  OK(cd_world_job_create(w, &s, &id, &d));
  OK(cd_world_job_assign(w, id, p, (CdPersonRef){0}, 0, &d));
  return id;
}
static CdEntityId office(CdWorld *w, CdPersonRef p, unsigned site,
                         CdOfficeKind kind) {
  OK(cd_world_institution_link(w, site, 2,
                               "technical reviewed fixture premises", &d));
  OK(cd_world_institution_operate(w, site, 1, 1, "explicit office",
                                  "technical scenario", &d));
  CdEntityId j = job(w, p, 2, kind <= CD_OFFICE_RABBI ? 7 : 8,
                     CD_SKILL_COMMUNITY | CD_SKILL_CRAFT, site);
  if (kind <= CD_OFFICE_RABBI)
    OK(cd_world_religious_qualification(
        w, p, site, kind, "explicit institution/title qualification", &d));
  CdOfficeSpec s = {site, j, kind, "technical office, not historical staffing"};
  CdEntityId out;
  OK(cd_world_office_create(w, &s, &out, &d));
  return out;
}
static CdEntityId roster(CdWorld *w, CdPersonRef p, unsigned base,
                         unsigned kind) {
  CdEntityId j = job(w, p, base, kind == CD_SERVICE_HEALTH ? 6 : 8, 15, 0);
  OK(cd_world_authorize(w, p, 63, "explicit civic permissions", &d));
  CdRosterSpec s = {j, 1u << kind, "Technical response roster",
                    "technical experiment, not an inferred organization"};
  CdEntityId out;
  OK(cd_world_roster(w, &s, &out, &d));
  return out;
}
static uint64_t dispatch(CdWorld *w, uint64_t key, CdEntityId provider,
                         int religious, CdServiceKind kind, unsigned target,
                         CdPersonRef subject, unsigned minutes) {
  CdServiceSpec s = {
      provider, religious,        kind,    target,
      subject,  cd_now(w) + 1000, minutes, "technical service demand"};
  uint64_t id;
  OK(cd_world_dispatch(w, key, &s, &id, &d));
  return id;
}
static void finish(CdWorld *w, uint64_t id) {
  for (unsigned i = 0; i < 600; i++) {
    CdServiceView v;
    OK(cd_world_service(w, id, &v, &d));
    if (!cd_pending(v.state)) {
      assert(v.state == CD_TASK_DONE);
      return;
    }
    step(w, 1);
  }
  fputs("service timeout\n", stderr);
  abort();
}
static void cycle24(void) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0), b = ref(w, 1);
  inside(w, a, 2);
  inside(w, b, 2);
  assert(cd_world_religious_qualification(w, a, 401, (CdOfficeKind)-1,
                                          "invalid title",
                                          &d) == CD_INVALID_ARGUMENT);
  CdEntityId o = office(w, a, 401, CD_OFFICE_PRIEST);
  CdOfficeView v;
  step(w, 1);
  OK(cd_world_office(w, o, &v, &d));
  assert(v.available && v.holder.person_id == a.person_id && !v.successions);
  assert(cd_world_religious_qualification(w, b, 401, CD_OFFICE_RABBI, "fixture",
                                          &d) == CD_INVALID_ARGUMENT);
  uint64_t id = dispatch(w, 1, o, 1, CD_SERVICE_VISIT, 2, b, 3);
  step(w, 1);
  assert(w->services[id - 1].view.progress == 1);
  OK(cd_world_job_unassign(w, v.job, &d));
  step(w, 2);
  assert(w->services[id - 1].view.progress == 1);
  OK(cd_world_office(w, o, &v, &d));
  assert(!v.available);
  OK(cd_world_training(w, b, CD_SKILL_COMMUNITY, "fixture", &d));
  OK(cd_world_job_assign(w, v.job, b, (CdPersonRef){0}, 0, &d));
  step(w, 1);
  assert(w->services[id - 1].view.progress == 1);
  OK(cd_world_religious_qualification(w, b, 401, CD_OFFICE_PRIEST,
                                      "explicit successor qualification", &d));
  finish(w, id);
  OK(cd_world_office(w, o, &v, &d));
  assert(v.successions == 1 && v.outputs == 1 &&
         v.holder.person_id == b.person_id);
  int member = 1;
  OK(cd_world_member(w, b, 401, &member, &d));
  assert(!member);
  OK(cd_world_job_unassign(w, v.job, &d));
  CdEntityId rabbi = office(w, a, 404, CD_OFFICE_RABBI);
  OK(cd_world_office(w, rabbi, &v, &d));
  assert(v.kind == CD_OFFICE_RABBI && v.available);
  assert(cd_world_religious_qualification(w, b, 404, CD_OFFICE_MINISTER,
                                          "fixture",
                                          &d) == CD_INVALID_ARGUMENT);
  ledger(w);
  cd_world_close(w);
}
static uint64_t calendar_run(int batch) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0), b = ref(w, 1);
  inside(w, a, 2);
  inside(w, b, 2);
  CdEntityId o = office(w, a, 401, CD_OFFICE_CHARITY);
  OK(cd_world_membership(w, b, 401, 1, "explicit membership", &d));
  OK(cd_world_food_store(w, 2, 0, "technical finite meals", &d));
  OK(cd_world_food_import(w, (CdFoodEndpoint){CD_FOOD_STORE, 2}, 4, 0,
                          "technical meals", &d));
  CdGatheringSpec s = {o,
                       CD_SERVICE_MEAL,
                       cd_now(w) + 2,
                       3,
                       2,
                       0,
                       1,
                       1,
                       "explicit dated local meal experiment"};
  uint64_t id, retry;
  OK(cd_world_gathering(w, 1, &s, &id, &d));
  OK(cd_world_attend(w, id, b, 1, &d));
  uint64_t h = hash(w);
  OK(cd_world_gathering(w, 1, &s, &retry, &d));
  assert(id == retry && hash(w) == h);
  void *data = NULL;
  size_t size = 0;
  OK(cd_world_calendar_save(w, &data, &size, &d));
  OK(cd_world_calendar_restore(w, data, size, &d));
  assert(hash(w) == h);
  ((unsigned char *)data)[size - 1] ^= 1;
  assert(cd_world_calendar_restore(w, data, size, &d) == CD_INVALID_ARGUMENT &&
         hash(w) == h);
  ((unsigned char *)data)[size - 1] ^= 1;
  cd_memory_fail_at(1);
  assert(cd_world_calendar_restore(w, data, size, &d) == CD_CAPACITY &&
         hash(w) == h);
  cd_memory_fail_at(0);
  if (batch)
    step(w, 6);
  else
    for (unsigned i = 0; i < 6; i++)
      step(w, 1);
  CdGatheringView v;
  OK(cd_world_gathering_view(w, id, &v, &d));
  assert(v.state == CD_TASK_DONE && v.completed == 1 && v.paid == 6 &&
         w->attendance_minutes == 3 && w->people[1].meals == 4);
  assert(cd_world_calendar_restore(w, data, size, &d) == CD_INVALID_STATE);
  cd_calendar_free(data);
  h = hash(w);
  OK(cd_world_gathering_cancel(w, id, &d));
  assert(hash(w) == h);
  ledger(w);
  h = hash(w);
  cd_world_close(w);
  return h;
}
static void cycle25(void) {
  assert(calendar_run(0) == calendar_run(1));
  scenario.civil_origin = (CdCivilTime){1940, 12, 31, 1438};
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0);
  OK(cd_world_place(w, a, 110, &d));
  CdEntityId o = office(w, a, 401, CD_OFFICE_MINISTER);
  OK(cd_world_enter(w, a, 303, 0, 0, &d));
  arrive(w, a);
  CdGatheringSpec s = {o,
                       CD_SERVICE_WORSHIP,
                       cd_now(w) + 1,
                       2,
                       1,
                       1,
                       2,
                       0,
                       "explicit dates; no holiday inference"};
  uint64_t id;
  OK(cd_world_gathering(w, 7, &s, &id, &d));
  step(w, 4);
  assert(w->gatherings[id - 1].view.occurrence == 1);
  CdClockView clock;
  OK(cd_world_clock(w, &clock, &d));
  assert(clock.civil.year == 1941);
  OK(cd_world_gathering_cancel(w, id, &d));
  ledger(w);
  cd_world_close(w);
  scenario.civil_origin = (CdCivilTime){1940, 1, 1, 480};
}
static void cycle26(void) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0), b = ref(w, 1);
  inside(w, a, 1);
  inside(w, b, 2);
  CdEntityId r = roster(w, a, 1, CD_SERVICE_ADMIN);
  uint64_t id = dispatch(w, 1, r, 0, CD_SERVICE_ADMIN, 2, (CdPersonRef){0}, 3);
  step(w, 2);
  assert(!w->services[id - 1].view.progress);
  finish(w, id);
  assert(w->people[0].inside == 1 && w->services[id - 1].view.paid == 6);
  id = dispatch(w, 2, r, 0, CD_SERVICE_ADMIN, 2, (CdPersonRef){0}, 5);
  step(w, 1);
  OK(cd_world_service_cancel(w, id, &d));
  uint64_t h = hash(w);
  OK(cd_world_service_cancel(w, id, &d));
  assert(hash(w) == h);
  CdEntityId j = w->rosters[r - 1].view.job;
  OK(cd_world_job_unassign(w, j, &d));
  r = roster(w, a, 2, CD_SERVICE_HEALTH);
  OK(cd_world_clinic(w, 2, 1, 3, "technical clinic", &d));
  OK(cd_world_expose(w, b, CD_ILL, 300, 100, "technical condition", &d));
  id = dispatch(w, 3, r, 0, CD_SERVICE_HEALTH, 2, b, 2);
  step(w, 1);
  assert(w->services[id - 1].view.progress == 1);
  OK(cd_world_employer_open(w, 2, 0, &d));
  step(w, 2);
  assert(w->services[id - 1].view.progress == 1 &&
         w->premises[1].occupied == 0);
  OK(cd_world_employer_open(w, 2, 1, &d));
  finish(w, id);
  assert(w->medicine_used == 2);
  ledger(w);
  cd_world_close(w);
}
static void equip(CdWorld *w, CdPersonRef p, unsigned b) {
  OK(cd_world_goods_import(w, b, CD_TOOL, 2, "technical transport equipment",
                           &d));
  OK(cd_world_goods_order(w, p, b, CD_TOOL, 1, 0, &d));
  step(w, 1);
  assert(cd_citizen(w, p)->goods[CD_TOOL] == 1);
}
static void delivery(CdWorld *w, CdEntityId id) {
  for (unsigned i = 0; i < 500; i++) {
    if (w->vehicles[id - 1].view.phase == CD_VEHICLE_DONE)
      return;
    step(w, 1);
    ledger(w);
  }
  fputs("delivery timeout\n", stderr);
  abort();
}
static void cycle27(void) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0);
  inside(w, a, 1);
  CdEntityId j = job(w, a, 1, 8, 15, 0);
  OK(cd_world_authorize(w, a, CD_AUTH_CARRIER, "explicit technical crew", &d));
  equip(w, a, 1);
  OK(cd_world_goods_import(w, 1, CD_CLAY, 4, "technical cargo", &d));
  CdVehicleSpec s = {j, CD_HANDCART, 3, "technical handcart fixture"};
  CdEntityId v;
  OK(cd_world_vehicle(w, &s, &v, &d));
  assert(cd_world_transport(w, v, 1, 1, 2, CD_CLAY, 0, 4, (CdPersonRef){0},
                            &d) == CD_INVALID_ARGUMENT);
  OK(cd_world_transport(w, v, 1, 1, 2, CD_CLAY, 0, 3, (CdPersonRef){0}, &d));
  uint64_t h = hash(w);
  OK(cd_world_transport(w, v, 1, 1, 2, CD_CLAY, 0, 3, (CdPersonRef){0}, &d));
  assert(hash(w) == h);
  step(w, 3);
  assert(w->vehicles[v - 1].view.loaded == 3);
  step(w, 12);
  OK(cd_world_vehicle_stop(w, v, 1, &d));
  CdPoint before = w->vehicles[v - 1].view.position;
  step(w, 3);
  assert(w->vehicles[v - 1].view.position.x == before.x &&
         w->vehicles[v - 1].view.position.y == before.y);
  OK(cd_world_vehicle_stop(w, v, 0, &d));
  delivery(w, v);
  assert(w->premises[1].goods[CD_CLAY] == 3 &&
         w->vehicles[v - 1].view.delivered == 3);
  h = hash(w);
  OK(cd_world_transport(w, v, 1, 1, 2, CD_CLAY, 0, 3, (CdPersonRef){0}, &d));
  assert(hash(w) == h);
  s.kind = CD_RAIL_VEHICLE;
  assert(cd_world_vehicle(w, &s, &v, &d) == CD_UNSUPPORTED);
  ledger(w);
  cd_world_close(w);
  w = world();
  a = ref(w, 0);
  CdPersonRef patient = ref(w, 1);
  inside(w, a, 1);
  inside(w, patient, 1);
  j = job(w, a, 1, 6, 15, 0);
  OK(cd_world_authorize(w, a, CD_AUTH_CARRIER, "explicit care crew", &d));
  equip(w, a, 1);
  s = (CdVehicleSpec){j, CD_STRETCHER, 0, "technical stretcher fixture"};
  OK(cd_world_vehicle(w, &s, &v, &d));
  OK(cd_world_transport(w, v, 1, 1, 2, 0, 0, 0, patient, &d));
  step(w, 1);
  assert(cd_citizen(w, patient)->vehicle == v);
  assert(cd_world_travel(w, patient, 110, &d) == CD_INVALID_STATE);
  delivery(w, v);
  assert(cd_citizen(w, patient)->inside == 1 &&
         !cd_citizen(w, patient)->vehicle);
  ledger(w);
  cd_world_close(w);
}
static void passenger_capacity(void) {
  scenario.initial_people = 12;
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0), patient = ref(w, 8);
  inside(w, a, 1);
  inside(w, patient, 1);
  OK(cd_world_household_move(w, 1, 2, &d));
  OK(cd_world_household_move(w, 2, 2, &d));
  CdEntityId j = job(w, a, 1, 6, 15, 0);
  OK(cd_world_authorize(w, a, CD_AUTH_CARRIER, "explicit crew", &d));
  equip(w, a, 1);
  CdVehicleSpec s = {j, CD_STRETCHER, 0,
                     "technical full-building arrival fixture"};
  CdEntityId v;
  OK(cd_world_vehicle(w, &s, &v, &d));
  OK(cd_world_transport(w, v, 1, 1, 2, 0, 0, 0, patient, &d));
  step(w, 200);
  assert(w->people[0].inside != 1 && cd_citizen(w, patient)->inside != 1 &&
         cd_citizen(w, patient)->vehicle == v && cd_housing_load(w, 1) == 8);
  OK(cd_world_household_move(w, 2, 0, &d));
  delivery(w, v);
  assert(w->people[0].inside == 1 && cd_citizen(w, patient)->inside == 1);
  ledger(w);
  cd_world_close(w);
  scenario.initial_people = 8;
}
static uint64_t pressure(CdWorld *w, uint64_t key, CdPressureKind kind,
                         CdEntityId target, unsigned value, unsigned minutes) {
  CdPressureSpec s = {
      kind,          target,
      cd_now(w) + 1, minutes,
      value,         "explicit technical scenario; no historic event inferred"};
  uint64_t id;
  OK(cd_world_pressure(w, key, &s, &id, &d));
  return id;
}
static void cycle28(void) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0);
  inside(w, a, 2);
  CdEntityId r = roster(w, a, 2, CD_SERVICE_FIRE);
  uint64_t p = pressure(w, 1, CD_PRESSURE_FIRE, 2, 20, 2);
  step(w, 1);
  assert(w->premises[1].fire == 20 && !cd_usable(w, 1));
  uint64_t id = dispatch(w, 1, r, 0, CD_SERVICE_FIRE, 2, (CdPersonRef){0}, 1);
  step(w, 2);
  assert(!w->services[id - 1].view.progress);
  OK(cd_world_goods_import(w, 2, CD_TOOL, 1, "technical response equipment",
                           &d));
  OK(cd_world_water_import(w, 2, 1, "technical water", &d));
  finish(w, id);
  assert(!w->premises[1].fire && w->water_used == 1);
  CdPressureView view;
  OK(cd_world_pressure_view(w, p, &view, &d));
  assert(view.applied);
  unsigned condition = w->premises[1].condition;
  p = pressure(w, 2, CD_PRESSURE_DAMAGE, 2, 50, 1);
  step(w, 1);
  assert(w->premises[1].condition == condition - 50);
  step(w, 2);
  assert(w->premises[1].condition == condition - 50);
  p = pressure(w, 3, CD_PRESSURE_EDGE_CLOSURE, 209, 1, 3);
  step(w, 1);
  assert(cd_edge_closed(w, cd_edge_index(content, 209)));
  OK(cd_world_close_edge(w, 209, 1, &d));
  step(w, 3);
  assert(cd_edge_closed(w, cd_edge_index(content, 209)));
  OK(cd_world_close_edge(w, 209, 0, &d));
  assert(!cd_edge_closed(w, cd_edge_index(content, 209)));
  p = pressure(w, 4, CD_PRESSURE_RAIN, 0, 3, 2);
  step(w, 1);
  assert(w->rain == 3);
  step(w, 2);
  assert(!w->rain);
  p = pressure(w, 5, CD_PRESSURE_RATION, 0, 1, 5);
  cd_native_energy_set(w->native, a, cd_native_hungry());
  step(w, 1);
  uint64_t eaten = w->people[0].eaten;
  assert(eaten == 1);
  cd_native_energy_set(w->native, a, cd_native_hungry());
  step(w, 1);
  assert(w->people[0].eaten == eaten);
  step(w, 4);
  assert(w->people[0].eaten == eaten + 1);
  OK(cd_world_shelter(w, 1, 1, "technical shelter", &d));
  OK(cd_world_shelter_assign(w, a, 1, "explicit evacuation request", &d));
  for (unsigned i = 0; i < 400 && w->people[0].inside != 0; i++)
    step(w, 1);
  assert(w->people[0].inside == 0);
  OK(cd_world_shelter_assign(w, a, 0, "release shelter", &d));
  ledger(w);
  cd_world_close(w);
}
static void failure_fixtures(void) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0), b = ref(w, 1);
  inside(w, a, 1);
  inside(w, b, 1);
  CdEntityId r = roster(w, a, 1, CD_SERVICE_POLICE);
  uint64_t id = dispatch(w, 1, r, 0, CD_SERVICE_POLICE, 1, (CdPersonRef){0}, 4);
  OK(cd_world_authorize(w, a, 0, "revoked explicit permission", &d));
  step(w, 2);
  assert(!w->services[id - 1].view.progress);
  OK(cd_world_authorize(w, a, CD_AUTH_POLICE, "renewed explicit permission",
                        &d));
  step(w, 1);
  assert(w->services[id - 1].view.progress == 1);
  OK(cd_world_retire(w, a, CD_DEATH, "technical staff loss", &d));
  step(w, 1);
  assert(w->services[id - 1].view.progress == 1);
  CdEntityId j = w->rosters[r - 1].view.job;
  OK(cd_world_training(w, b, 15, "replacement skills", &d));
  OK(cd_world_authorize(w, b, CD_AUTH_POLICE, "replacement permission", &d));
  OK(cd_world_job_assign(w, j, b, (CdPersonRef){0}, 0, &d));
  finish(w, id);
  CdServiceSpec bad = {r,
                       0,
                       CD_SERVICE_HEALTH,
                       1,
                       b,
                       cd_now(w) + 100,
                       1,
                       "unsupported roster request"};
  uint64_t out;
  uint64_t h = hash(w);
  assert(cd_world_dispatch(w, 9, &bad, &out, &d) == CD_UNSUPPORTED &&
         hash(w) == h);
  CdServiceSpec deadline = {r,
                            0,
                            CD_SERVICE_POLICE,
                            1,
                            (CdPersonRef){0},
                            cd_now(w) + 1,
                            4,
                            "explicit short deadline"};
  OK(cd_world_dispatch(w, 2, &deadline, &out, &d));
  step(w, 1);
  assert(w->services[out - 1].view.state == CD_TASK_CANCELLED);
  ledger(w);
  cd_world_close(w);
  // Crew loss preserves a partially loaded cart and finite goods at that
  // position.
  w = world();
  a = ref(w, 0);
  b = ref(w, 1);
  inside(w, a, 1);
  inside(w, b, 1);
  j = job(w, a, 1, 8, 15, 0);
  OK(cd_world_authorize(w, a, CD_AUTH_CARRIER, "crew", &d));
  equip(w, a, 1);
  equip(w, b, 1);
  OK(cd_world_goods_import(w, 1, CD_CLAY, 4, "cargo", &d));
  CdVehicleSpec vs = {j, CD_HANDCART, 4, "technical cart"};
  CdEntityId v;
  OK(cd_world_vehicle(w, &vs, &v, &d));
  OK(cd_world_transport(w, v, 1, 1, 2, CD_CLAY, 0, 4, (CdPersonRef){0}, &d));
  step(w, 1);
  assert(w->vehicles[v - 1].view.loaded == 1);
  OK(cd_world_retire(w, a, CD_DEATH, "crew loss fixture", &d));
  step(w, 2);
  assert(w->vehicles[v - 1].view.loaded == 1 &&
         w->vehicles[v - 1].view.phase == CD_VEHICLE_STOPPED);
  ledger(w);
  OK(cd_world_training(w, b, 15, "replacement", &d));
  OK(cd_world_authorize(w, b, CD_AUTH_CARRIER, "replacement crew", &d));
  OK(cd_world_job_assign(w, j, b, (CdPersonRef){0}, 0, &d));
  OK(cd_world_vehicle_crew(w, v, b, &d));
  OK(cd_world_vehicle_stop(w, v, 0, &d));
  step(w, 1);
  assert(w->vehicles[v - 1].view.loaded == 2);
  OK(cd_world_vehicle_stop(w, v, 1, &d));
  OK(cd_world_vehicle_unload_here(w, v, &d));
  delivery(w, v);
  assert(w->premises[0].goods[CD_CLAY] == 4 && !w->vehicles[v - 1].view.loaded);
  h = hash(w);
  OK(cd_world_vehicle_unload_here(w, v, &d));
  assert(hash(w) == h);
  ledger(w);
  cd_world_close(w);
}
static void negative_queues(void) {
  CdWorld *w = world();
  CdPersonRef a = ref(w, 0);
  inside(w, a, 2);
  CdEntityId o = office(w, a, 401, CD_OFFICE_CHARITY);
  CdGatheringSpec s = {o, CD_SERVICE_MEAL,       cd_now(w) + 10, 2, 1, 0, 1,
                       0, "explicit future meal"};
  uint64_t id;
  size_t attempts = cd_memory_stats().attempts;
  cd_memory_fail_at(1);
  OK(cd_world_gathering(w, 1, &s, &id, &d));
  cd_memory_fail_at(0);
  assert(cd_memory_stats().attempts == attempts);
  s.minutes = 3;
  uint64_t h = hash(w);
  assert(cd_world_gathering(w, 1, &s, &id, &d) == CD_INVALID_ARGUMENT &&
         hash(w) == h);
  s.starts = cd_now(w);
  assert(cd_world_gathering(w, 2, &s, &id, &d) != CD_OK);
  s.starts = cd_now(w) + 1;
  s.minutes = 1441;
  s.repeat_days = 1;
  s.repeats = 2;
  assert(cd_world_gathering(w, 2, &s, &id, &d) != CD_OK);
  CdSnapshot *snap = NULL;
  OK(cd_snapshot_capture(w, &snap, &d));
  pressure(w, 1, CD_PRESSURE_ALERT, 0, 1, 2);
  step(w, 1);
  CdCivicSummary before, after;
  OK(cd_snapshot_civic_summary(snap, &before, &d));
  OK(cd_world_civic_summary(w, &after, &d));
  assert(!before.alert && after.alert);
  cd_snapshot_close(snap);
  CdPressureSpec p = {CD_PRESSURE_RAIN,   0, cd_now(w) + 1, 2, 4,
                      "invalid intensity"};
  h = hash(w);
  assert(cd_world_pressure(w, 3, &p, &id, &d) == CD_INVALID_ARGUMENT &&
         hash(w) == h);
  s = (CdGatheringSpec){o, CD_SERVICE_MEAL,        cd_now(w) + 20, 1, 1, 0, 1,
                        0, "bounded queue fixture"};
  for (uint64_t key = 2; key <= CD_CIVIC_LIMIT; key++)
    OK(cd_world_gathering(w, key, &s, &id, &d));
  h = hash(w);
  assert(cd_world_gathering(w, CD_CIVIC_LIMIT + 1, &s, &id, &d) ==
             CD_CAPACITY &&
         hash(w) == h);
  CdServiceSpec demand = {o,
                          1,
                          CD_SERVICE_MEAL,
                          2,
                          ref(w, 1),
                          cd_now(w) + 1000,
                          1,
                          "bounded demand queue"};
  for (uint64_t key = 1; key <= CD_DISPATCH_LIMIT; key++)
    OK(cd_world_dispatch(w, key, &demand, &id, &d));
  h = hash(w);
  assert(cd_world_dispatch(w, CD_DISPATCH_LIMIT + 1, &demand, &id, &d) ==
             CD_CAPACITY &&
         hash(w) == h);
  p = (CdPressureSpec){CD_PRESSURE_RAIN,        0, cd_now(w) + 20, 1, 1,
                       "bounded pressure queue"};
  for (uint64_t key = 2; key <= CD_CIVIC_LIMIT; key++)
    OK(cd_world_pressure(w, key, &p, &id, &d));
  h = hash(w);
  assert(cd_world_pressure(w, CD_CIVIC_LIMIT + 1, &p, &id, &d) == CD_CAPACITY &&
         hash(w) == h);
  ledger(w);
  cd_world_close(w);
}
static void actual_default(void) {
  cd_content_close(content);
  content = NULL;
  OK(cd_content_open("build/content/CanterburyCity.bin", &content, &d));
  scenario.initial_people = 27000;
  CdWorld *w = world();
  CdCivicSummary c;
  OK(cd_world_civic_summary(w, &c, &d));
  assert(!c.offices && !c.gatherings && !c.rosters && !c.requests &&
         !c.vehicles && !c.pressures && !c.rain && !c.alert && !c.blackout &&
         !c.ration_limit);
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    assert(!p->credentials && !p->ordination_institution && !p->mission &&
           !p->gathering && !p->vehicle && !p->driving && !p->refuge);
    for (unsigned j = 0; j < 4; j++)
      assert(!p->memberships[j]);
  }
  ledger(w);
  cd_world_close(w);
  scenario.initial_people = 8;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  unsigned cycle = atoi(argv[1]);
  OK(cd_content_open("build/fixtures/civic.bin", &content, &d));
  switch (cycle) {
  case 0:
    actual_default();
    break;
  case 24:
    cycle24();
    failure_fixtures();
    break;
  case 25:
    cycle25();
    negative_queues();
    break;
  case 26:
    cycle26();
    break;
  case 27:
    cycle27();
    passenger_capacity();
    break;
  case 28:
    cycle28();
    break;
  default:
    abort();
  }
  cd_content_close(content);
  assert(!cd_memory_stats().total);
  printf("{\"cycle\":%u,\"ok\":true,\"closed_bytes\":0}\n", cycle);
}
