#include "cd_city_internal.h"
#include "cd_native.h"
#include <assert.h>
#include <stdlib.h>
#define OK(x)                                                                  \
  do {                                                                         \
    CdStatus s = (x);                                                          \
    if (s != CD_OK) {                                                          \
      fprintf(stderr, "line %d status %d: %s\n", __LINE__, s, d.message);      \
      abort();                                                                 \
    }                                                                          \
  } while (0)
static CdDiagnostic d;
static uint64_t digest(CdWorld *w) {
  uint64_t h;
  OK(cd_world_digest(w, &h, &d));
  return h;
}
static void arrive(CdWorld *w, CdPersonRef p) {
  for (int i = 0; i < 200; i++) {
    CdTravelView t;
    OK(cd_world_travel_view(w, p, &t, &d));
    if (t.state == CD_ROUTE_ARRIVED)
      return;
    OK(cd_world_step(w, 1, &d));
  }
  fprintf(stderr, "arrival timeout\n");
  abort();
}
int main(int argc, char **argv) {
  assert(argc == 2);
  int cycle = atoi(argv[1]);
  CdContent *c = NULL;
  OK(cd_content_open("build/fixtures/city.bin", &c, &d));
  assert(cd_content_building_count(c) == 3);
  if (cycle == 9) {
    CdEntityId id = 0;
    OK(cd_content_pick(c, (CdPoint){31, 31}, &id, &d));
    assert(id == 1);
    assert(cd_content_pick(c, (CdPoint){35, 35}, &id, &d) != CD_OK);
    assert(cd_content_segment(c, (CdPoint){29, 31}, (CdPoint){41, 31}, 0, 0,
                              &d) != CD_OK);
    OK(cd_content_segment(c, (CdPoint){34, 34}, (CdPoint){36, 36}, 0, 0, &d));
    assert(cd_content_segment(c, (CdPoint){89, 31}, (CdPoint){101, 31}, 0, 0,
                              &d) != CD_OK);
    OK(cd_content_segment(c, (CdPoint){89, 31}, (CdPoint){101, 31}, 1, 0, &d));
    assert(cd_content_segment(c, (CdPoint){109, 31}, (CdPoint){121, 31}, 0, 0,
                              &d) != CD_OK);
    OK(cd_content_segment(c, (CdPoint){109, 31}, (CdPoint){121, 31}, 0, 1, &d));
    int reviewed;
    OK(cd_content_connector(c, 301, &reviewed, &d));
    assert(reviewed);
    OK(cd_content_connector(c, 302, &reviewed, &d));
    assert(!reviewed);
    CdBuildingView b;
    OK(cd_content_building(c, 0, &b, &d));
    assert(b.confidence == .6);
    CdAccess a = {0, 0, 0, 0, 600};
    OK(cd_content_access(c, 1, &a, &d));
    const char *bad[] = {"duplicate", "bad-door",     "cut-wall",
                         "bad-grade", "unknown-node", "bad-crc"};
    for (unsigned i = 0; i < 6; i++) {
      char path[100];
      snprintf(path, sizeof path, "build/fixtures/%s.bin", bad[i]);
      CdContent *invalid = NULL;
      assert(cd_content_open(path, &invalid, &d) == CD_INVALID_DATA &&
             !invalid);
    }
  }
  if (cycle == 0 || cycle == 30000) {
    cd_content_close(c);
    c = NULL;
    OK(cd_content_open("build/content/CanterburyCity.bin", &c, &d));
    assert(cd_content_building_count(c) == 3341);
  }
  CdScenario scenario = {cycle == 0       ? 27000
                         : cycle == 30000 ? 30000
                                          : 8,
                         1940,
                         {1940, 1, 1, 0},
                         "synthetic-ordinary-v1"};
  if (cycle == 10 || cycle == 11 || cycle == 101)
    scenario.civil_origin.minute_of_day = 480;
  CdWorld *w = NULL;
  OK(cd_world_open(c, &scenario, &w, &d));
  CdResidentView p[8];
  for (int i = 0; i < 8; i++)
    OK(cd_world_resident(w, i, &p[i], &d));
  if (cycle == 10) {
    OK(cd_world_place(w, p[0].ref, 101, &d));
    assert(cd_world_place(w, p[0].ref, 102, &d) == CD_INVALID_STATE);
    CdRequest request = {.kind = CD_OPERATION_MOVE,
                         .actor = p[0].ref,
                         .target = {.kind = CD_TARGET_NODE, .entity_id = 103}};
    CdPreview preview;
    uint64_t h = digest(w);
    OK(cd_world_preview(w, &request, &preview, &d));
    assert(digest(w) == h);
    CdRequestId rid;
    OK(cd_world_submit(w, &request, &rid, &d));
    CdTravelView prev, t;
    OK(cd_world_travel_view(w, p[0].ref, &prev, &d));
    OK(cd_world_step(w, 0, &d));
    OK(cd_world_travel_view(w, p[0].ref, &t, &d));
    assert(prev.position.x == t.position.x);
    for (int i = 0; i < 40; i++) {
      prev = t;
      OK(cd_world_step(w, 1, &d));
      OK(cd_world_travel_view(w, p[0].ref, &t, &d));
      assert(hypot(t.position.x - prev.position.x,
                   t.position.y - prev.position.y) <=
             8 / cd_city_scale(w) + 1e-8);
      uint16_t x, y;
      OK(cd_native_position(w->native, p[0].ref, &x, &y));
      assert(fabs(t.position.x - x / cd_city_scale(w)) < 1e-10);
      if (i == 0) {
        OK(cd_world_cancel(w, rid, &d));
        CdPoint paused = t.position;
        OK(cd_world_step(w, 2, &d));
        OK(cd_world_travel_view(w, p[0].ref, &t, &d));
        assert(t.position.x == paused.x);
        OK(cd_world_travel(w, p[0].ref, 103, &d));
      }
      if (t.state == CD_ROUTE_ARRIVED)
        break;
    }
    assert(t.state == CD_ROUTE_ARRIVED);
    OK(cd_world_place(w, p[1].ref, 107, &d));
    assert(cd_world_enter(w, p[1].ref, 302, 0, 0, &d) == CD_UNSUPPORTED);
    OK(cd_world_lock(w, 1, 1, &d));
    assert(cd_world_enter(w, p[1].ref, 301, 0, 0, &d) != CD_OK);
    OK(cd_world_enter(w, p[1].ref, 301, 1, 0, &d));
    OK(cd_world_step(w, 1, &d));
    OK(cd_world_stop(w, p[1].ref, &d));
    OK(cd_world_step(w, 1, &d));
    OK(cd_world_enter(w, p[1].ref, 301, 1, 0, &d));
    arrive(w, p[1].ref);
    assert(w->people[1].inside == cd_polygon_index(c, 1));
    OK(cd_world_exit(w, p[1].ref, 301, 1, 0, &d));
    arrive(w, p[1].ref);
    assert(w->people[1].inside == CD_NONE);
  }
  if (cycle == 11) {
    CdRoute route;
    OK(cd_world_plan(w, 101, 103, &route, &d));
    assert(route.count == 2 && route.edges[0] == 201);
    OK(cd_world_close_edge(w, 201, 1, &d));
    OK(cd_world_plan(w, 101, 103, &route, &d));
    assert(route.edges[0] == 203);
    OK(cd_world_close_edge(w, 203, 1, &d));
    assert(cd_world_plan(w, 101, 103, &route, &d) != CD_OK &&
           route.reason == CD_ROUTE_CLOSED);
    OK(cd_world_close_edge(w, 201, 0, &d));
    OK(cd_world_close_edge(w, 203, 0, &d));
    OK(cd_world_set_crowd(w, 201, 4, &d));
    OK(cd_world_plan(w, 101, 103, &route, &d));
    assert(route.edges[0] == 203);
    OK(cd_world_set_crowd(w, 203, 4, &d));
    assert(cd_world_plan(w, 101, 103, &route, &d) != CD_OK &&
           route.reason == CD_ROUTE_CROWDED);
    assert(cd_world_plan(w, 101, 105, &route, &d) != CD_OK &&
           route.reason == CD_ROUTE_DISCONNECTED);
    assert(cd_world_plan(w, 101, 109, &route, &d) != CD_OK &&
           route.reason == CD_ROUTE_UNQUALIFIED);
    OK(cd_world_set_crowd(w, 201, 0, &d));
    OK(cd_world_place(w, p[0].ref, 101, &d));
    OK(cd_world_travel(w, p[0].ref, 103, &d));
    OK(cd_world_step(w, 1, &d));
    OK(cd_world_close_edge(w, 201, 1, &d));
    OK(cd_world_step(w, 1, &d));
    CdTravelView t;
    OK(cd_world_travel_view(w, p[0].ref, &t, &d));
    assert(t.state == CD_ROUTE_WAITING && t.reason == CD_ROUTE_CLOSED);
    OK(cd_world_close_edge(w, 201, 0, &d));
    arrive(w, p[0].ref);
  }
  if (cycle == 12) {
    for (int i = 0; i < 8; i++) {
      assert(cd_civil_valid(p[i].birth) && p[i].synthetic &&
             !p[i].affiliation_known && !p[i].position_known && p[i].name[0] &&
             !strstr(p[i].name, "fixture"));
      assert(p[i].age ==
             cd_civil_age(p[i].birth, (CdCivilTime){1940, 1, 1, 1}));
      if (p[i].age < 18) {
        CdResidentView g;
        OK(cd_world_resolve(w, p[i].guardian, &g, &d));
        assert(g.age >= 18 && g.household == p[i].household);
      }
    }
    assert(cd_world_category(w, p[2].ref, CD_TROOP, "fixture troop", &d) ==
           CD_INVALID_ARGUMENT);
    OK(cd_world_category(w, p[0].ref, CD_TROOP, "explicit technical fixture",
                         &d));
    CdPersonRef foreign = p[0].ref;
    foreign.world_token++;
    assert(cd_world_resolve(w, foreign, &p[0], &d) == CD_UNKNOWN_REFERENCE);
    assert(!cd_civil_valid((CdCivilTime){1900, 2, 29, 0}));
    assert(cd_civil_valid((CdCivilTime){1940, 2, 29, 0}));
  }
  if (cycle == 13) {
    CdCitySummary summary;
    OK(cd_world_summary(w, &summary, &d));
    assert(summary.households == 2 && summary.housing_unknown == 8 &&
           !summary.unhoused);
    OK(cd_world_household_move(w, 1, 1, &d));
    uint64_t h = digest(w);
    assert(cd_world_household_move(w, 2, 1, &d) == CD_CAPACITY &&
           digest(w) == h);
    assert(cd_world_household_move(w, 2, 3, &d) == CD_UNSUPPORTED);
    assert(cd_world_visit(w, p[4].ref, 1, 1, &d) == CD_CAPACITY);
    OK(cd_world_household_move(w, 2, 2, &d));
    OK(cd_world_visit(w, p[0].ref, 2, 1, &d));
    OK(cd_world_owner(w, 2, p[0].ref, &d));
    OK(cd_world_summary(w, &summary, &d));
    assert(summary.housed == 8 && summary.visitors == 1 && !summary.located);
    assert(cd_world_transfer(w, p[0].ref, 2, p[4].ref, &d) == CD_INVALID_STATE);
    OK(cd_world_guardian(w, p[2].ref, p[1].ref, &d));
    OK(cd_world_guardian(w, p[3].ref, p[1].ref, &d));
    OK(cd_world_transfer(w, p[0].ref, 2, p[4].ref, &d));
    assert(cd_world_transfer(w, p[2].ref, 2, p[6].ref, &d) ==
           CD_INVALID_ARGUMENT);
    OK(cd_world_transfer(w, p[2].ref, 2, p[4].ref, &d));
    assert(cd_world_visit(w, p[0].ref, 2, 0, &d) == CD_UNKNOWN_REFERENCE);
    OK(cd_world_household_move(w, 1, 0, &d));
    OK(cd_world_summary(w, &summary, &d));
    assert(summary.housed + summary.unhoused + summary.housing_unknown == 8 &&
           summary.unhoused == 2);
  }
  if (cycle == 0 || cycle == 30000) {
    CdCitySummary summary;
    OK(cd_world_summary(w, &summary, &d));
    assert(summary.people == scenario.initial_people &&
           summary.households == scenario.initial_people / 4 &&
           summary.housing_unknown == scenario.initial_people &&
           summary.capacity == 30000 && !summary.located);
    CdPersonId last = 0;
    for (size_t i = 0; i < scenario.initial_people; i++) {
      CdResidentView v;
      OK(cd_world_resident(w, i, &v, &d));
      assert(v.ref.person_id > last && cd_civil_valid(v.birth) && v.synthetic &&
             !v.affiliation_known && !v.position_known);
      last = v.ref.person_id;
      if (v.age < 18) {
        CdResidentView g;
        OK(cd_world_resolve(w, v.guardian, &g, &d));
        assert(g.age >= 18 && g.household == v.household);
      }
      CdHouseholdView h;
      OK(cd_world_household(w, i / 4, &h, &d));
      assert(h.members == 4);
    }
    CdEntityId a, b;
    CdPoint point;
    OK(cd_content_node(c, 0, &a, &point, &d));
    OK(cd_content_node(c, 1, &b, &point, &d));
    CdRoute route;
    assert(cd_world_plan(w, a, b, &route, &d) != CD_OK);
    OK(cd_world_step(w, 1, &d));
  }
  if (cycle == 100) {
    uint64_t h = digest(w);
    CdWorld *old = w;
    CdPersonRef ref = p[0].ref;
    size_t total = cd_memory_stats().total;
    size_t failures = 0;
    for (size_t n = 1; n < 100; n++) {
      cd_memory_fail_at(n);
      CdStatus status = cd_world_open(c, &scenario, &w, &d);
      cd_memory_fail_at(0);
      if (status == CD_OK) {
        assert(cd_world_resolve(w, ref, &p[0], &d) == CD_UNKNOWN_REFERENCE);
        break;
      }
      assert(status == CD_CAPACITY && w == old && digest(w) == h &&
             cd_memory_stats().total == total);
      failures++;
    }
    assert(failures >= 25);
  }
  if (cycle == 101) {
    for (int i = 0; i < 5; i++)
      OK(cd_world_place(w, p[i].ref, 107, &d));
    for (int i = 0; i < 4; i++)
      OK(cd_world_enter(w, p[i].ref, 301, 0, 0, &d));
    assert(cd_world_enter(w, p[4].ref, 301, 0, 0, &d) == CD_CAPACITY);
    for (int i = 0; i < 4; i++)
      arrive(w, p[i].ref);
    OK(cd_world_household_move(w, 1, 1, &d));
    assert(cd_housing_load(w, cd_polygon_index(c, 1)) == 4);
    assert(cd_world_visit(w, p[4].ref, 1, 1, &d) == CD_CAPACITY);
    OK(cd_world_exit(w, p[0].ref, 301, 0, 0, &d));
    arrive(w, p[0].ref);
    assert(cd_world_enter(w, p[4].ref, 301, 0, 0, &d) == CD_CAPACITY);
    OK(cd_world_household_move(w, 1, 0, &d));
    OK(cd_world_enter(w, p[4].ref, 301, 0, 0, &d));
  }
  CdSnapshot *snapshot = NULL;
  OK(cd_snapshot_capture(w, &snapshot, &d));
  cd_world_close(w);
  CdPersonView view;
  OK(cd_snapshot_person(snapshot, 0, &view, &d));
  assert(view.ref.person_id);
  cd_snapshot_close(snapshot);
  cd_content_close(c);
  assert(cd_memory_stats().total == 0);
  printf("{\"ok\":true,\"cycle\":%d,\"closed_bytes\":0}\n", cycle);
  return 0;
}
