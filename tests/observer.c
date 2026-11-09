#include "cd_city_internal.h"
#include "cd_native.h"
#include <assert.h>
#include <stdlib.h>
static CdDiagnostic d;
#define OK(x)                                                                  \
  do {                                                                         \
    CdStatus status_ = (x);                                                    \
    if (status_ != CD_OK) {                                                    \
      fprintf(stderr, "line %d status %d: %s\n", __LINE__, status_,            \
              d.message);                                                      \
      abort();                                                                 \
    }                                                                          \
  } while (0)
static CdContent *content;
static CdScenario scenario = {
    20, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
static CdWorld *open_world(void) {
  CdWorld *w = NULL;
  OK(cd_world_open(content, &scenario, &w, &d));
  return w;
}
static CdPersonRef ref(CdWorld *w, size_t i) { return w->people[i].civil.ref; }
static uint64_t digest(CdWorld *w) {
  uint64_t h;
  OK(cd_world_digest(w, &h, &d));
  return h;
}
static void step(CdWorld *w, unsigned n) { OK(cd_world_step(w, n, &d)); }
static void inside(CdWorld *w, CdPersonRef p, unsigned b) {
  OK(cd_world_place(w, p, b == 1 ? 107 : 110, &d));
  OK(cd_world_enter(w, p, b == 1 ? 301 : 303, 0, 0, &d));
  for (unsigned i = 0; i < 100; i++) {
    CdTravelView v;
    OK(cd_world_travel_view(w, p, &v, &d));
    if (v.state == CD_ROUTE_ARRIVED)
      return;
    step(w, 1);
  }
  abort();
}
static CdEntityId job(CdWorld *w, CdPersonRef p, unsigned b, unsigned role,
                      CdEntityId site) {
  OK(cd_world_training(w, p, 15, "technical skills", &d));
  OK(cd_world_authorize(w, p, 63, "explicit technical permissions", &d));
  OK(cd_world_employer(w, b, "Technical fixture employer", b == 1 ? 4 : 8,
                       "technical premises", &d));
  OK(cd_world_money_import(w, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = b},
                           100000, "technical tokens", &d));
  CdJobSpec s = {.workplace = b,
                 .entrance = b == 1 ? 301 : 303,
                 .occupation = role,
                 .institution = site,
                 .shift_start = 0,
                 .shift_end = 0,
                 .wage = 2,
                 .basis = "technical schedule"};
  CdEntityId id;
  OK(cd_world_job_create(w, &s, &id, &d));
  OK(cd_world_job_assign(w, id, p, (CdPersonRef){0}, 0, &d));
  return id;
}
static void ledger(CdWorld *w) {
  CdFoodLedger f;
  CdEconomyLedger e;
  CdCivicSummary c;
  OK(cd_world_food_ledger(w, &f, &d));
  OK(cd_world_economy_ledger(w, &e, &d));
  OK(cd_world_civic_summary(w, &c, &d));
}
static void cycle29(void) {
  CdWorld *w = open_world();
  CdPersonRef a = ref(w, 0), b = ref(w, 1), outsider = ref(w, 4);
  inside(w, a, 2);
  CdEntityId j = job(w, a, 2, 8, 0);
  CdRosterSpec spec = {j, 1u << CD_SERVICE_ADMIN, "Technical administration",
                       "explicit roster"};
  CdEntityId provider;
  OK(cd_world_roster(w, &spec, &provider, &d));
  CdServiceSpec demand = {provider,        0,  CD_SERVICE_ADMIN, 2, {0},
                          cd_now(w) + 100, 10, "ordinary demand"};
  uint64_t request;
  OK(cd_world_dispatch(w, 1, &demand, &request, &d));
  step(w, 2);
  OK(cd_world_retire(w, a, CD_DEATH, "explicit ordinary staff death fixture",
                     &d));
  step(w, 1);
  CdSocialView social;
  OK(cd_world_social(w, b, &social, &d));
  assert(social.support_context_active && social.bereavement_event);
  CdRecord r;
  assert(cd_world_record(w, social.bereavement_event,
                         (CdRecordQuery){outsider, CD_RECORD_HOUSEHOLD}, &r,
                         &d) == CD_UNSUPPORTED);
  OK(cd_world_record(w, social.bereavement_event,
                     (CdRecordQuery){b, CD_RECORD_HOUSEHOLD}, &r, &d));
  assert(r.kind == CD_EVENT_BEREAVEMENT && r.subject.person_id == a.person_id);
  CdSnapshot *snapshot = NULL;
  OK(cd_snapshot_capture(w, &snapshot, &d));
  uint64_t observed;
  OK(cd_world_observe(w, b, r.id, "explicit household observation", &observed,
                      &d));
  uint64_t h = digest(w);
  OK(cd_world_export_records(w, (CdRecordQuery){{0}, CD_RECORD_DIAGNOSTIC},
                             "build/records-29.json", &d));
  assert(digest(w) == h);
  int trace = 0;
  for (uint64_t id = w->record_next - 1; id;) {
    OK(cd_world_record(w, id, (CdRecordQuery){{0}, CD_RECORD_DIAGNOSTIC}, &r,
                       &d));
    if (r.kind == CD_EVENT_RETIREMENT)
      trace = 1;
    id = r.cause;
    if (!id)
      break;
  }
  assert(trace);
  assert(cd_world_training(w, a, 15, "retired actor", &d) != CD_OK);
  CdRecord copied;
  OK(cd_snapshot_record(snapshot, social.bereavement_event,
                        (CdRecordQuery){b, CD_RECORD_HOUSEHOLD}, &copied, &d));
  for (unsigned i = 0; i < CD_RECORD_LIMIT + 3; i++)
    OK(cd_world_observe(w, b, observed, "explicit retained knowledge",
                        &observed, &d));
  CdRecordSummary summary;
  OK(cd_world_record_summary(w, &summary, &d));
  assert(summary.retained == CD_RECORD_LIMIT && summary.dropped > 0);
  assert(cd_world_record(w, social.bereavement_event,
                         (CdRecordQuery){{0}, CD_RECORD_DIAGNOSTIC}, &r,
                         &d) == CD_UNKNOWN_REFERENCE);
  assert(copied.subject.person_id == a.person_id);
  OK(cd_snapshot_record(snapshot, social.bereavement_event,
                        (CdRecordQuery){b, CD_RECORD_HOUSEHOLD}, &r, &d));
  cd_snapshot_close(snapshot);
  ledger(w);
  cd_world_close(w);
}
static CdCommand command(CdWorld *w, uint64_t key, CdCommandKind kind,
                         CdEntityId target) {
  CdCommand c = {0};
  c.key = key;
  c.kind = kind;
  c.actor = ref(w, 0);
  c.target = target;
  c.deadline = cd_native_minutes(w->native) + 1000;
  snprintf(c.basis, 96, "explicit technical operation fixture");
  return c;
}
static void cycle30(void) {
  CdWorld *w = open_world();
  CdCommand c = command(w, 1, CD_CMD_PLACE, 107);
  c.earliest = cd_native_minutes(w->native) + 2;
  CdPreview preview;
  uint64_t h = digest(w), id, again;
  assert(cd_world_operation_preview(w, &c, CD_PERMISSION_OBSERVER, &preview,
                                    &d) == CD_UNSUPPORTED);
  OK(cd_world_operation_preview(w, &c, CD_PERMISSION_INTERVENTION, &preview,
                                &d));
  assert(digest(w) == h);
  OK(cd_world_operation_queue(w, &c, CD_PERMISSION_INTERVENTION, &id, &d));
  h = digest(w);
  OK(cd_world_operation_queue(w, &c, CD_PERMISSION_INTERVENTION, &again, &d));
  assert(id == again && digest(w) == h);
  c.target = 110;
  assert(cd_world_operation_queue(w, &c, CD_PERMISSION_INTERVENTION, &again,
                                  &d) == CD_INVALID_ARGUMENT &&
         digest(w) == h);
  step(w, 0);
  assert(digest(w) == h);
  step(w, 3);
  assert(w->people[0].civil.position_known);
  CdOperationView v;
  OK(cd_world_operation(w, id, &v, &d));
  assert(v.state == CD_TASK_DONE);
  c = command(w, 2, CD_CMD_CLOSE_EDGE, 209);
  c.earliest = cd_native_minutes(w->native) + 10;
  c.flags = 1;
  OK(cd_world_operation_queue(w, &c, CD_PERMISSION_INTERVENTION, &id, &d));
  OK(cd_world_operation_cancel(w, id, CD_PERMISSION_INTERVENTION, &d));
  h = digest(w);
  OK(cd_world_operation_cancel(w, id, CD_PERMISSION_INTERVENTION, &d));
  assert(digest(w) == h);
  CdCommand parsed;
  assert(cd_command_parse(w, "bad|command", &parsed, &d) != CD_OK);
  OK(cd_command_parse(
      w, "3|train|1|0|0|15|0|0|0|0|1000|0|0||explicit qualification", &parsed,
      &d));
  assert(parsed.kind == CD_CMD_TRAIN && parsed.actor.person_id == 1);
  size_t attempts = cd_memory_stats().attempts;
  cd_memory_fail_at(1);
  for (unsigned i = 3; i <= CD_OPERATION_LIMIT; i++) {
    c = command(w, i, CD_CMD_TRAIN, 0);
    c.units = 15;
    OK(cd_world_operation_queue(w, &c, CD_PERMISSION_INTERVENTION, &id, &d));
  }
  cd_memory_fail_at(0);
  assert(cd_memory_stats().attempts == attempts);
  h = digest(w);
  c.key = CD_OPERATION_LIMIT + 1;
  assert(cd_world_operation_queue(w, &c, CD_PERMISSION_INTERVENTION, &id, &d) ==
             CD_CAPACITY &&
         digest(w) == h);
  ledger(w);
  cd_world_close(w);
}
static CdWorld *busy_world(void) {
  CdWorld *w = open_world();
  CdPersonRef worker = ref(w, 1), builder = ref(w, 4), clinician = ref(w, 5),
              patient = ref(w, 2), minister = ref(w, 8), crew = ref(w, 12),
              carried = ref(w, 13);
  inside(w, worker, 2);
  inside(w, builder, 2);
  inside(w, clinician, 2);
  inside(w, patient, 2);
  inside(w, minister, 2);
  inside(w, crew, 1);
  inside(w, carried, 1);
  OK(cd_world_place(w, ref(w, 0), 107, &d));
  CdEntityId jw = job(w, worker, 2, 3, 0), jb = job(w, builder, 2, 3, 0),
             jc = job(w, clinician, 2, 6, 0);
  OK(cd_world_institution_link(w, 401, 2, "technical reviewed premises", &d));
  OK(cd_world_institution_operate(w, 401, 1, 1, "minister", "explicit scenario",
                                  &d));
  CdEntityId jm = job(w, minister, 2, 7, 401);
  OK(cd_world_religious_qualification(w, minister, 401, CD_OFFICE_MINISTER,
                                      "explicit title qualification", &d));
  CdOfficeSpec os = {401, jm, CD_OFFICE_MINISTER, "technical office"};
  CdEntityId office;
  OK(cd_world_office_create(w, &os, &office, &d));
  CdEntityId jcrew = job(w, crew, 1, 6, 0);
  OK(cd_world_goods_import(w, 1, CD_TOOL, 1, "technical equipment", &d));
  OK(cd_world_goods_order(w, crew, 1, CD_TOOL, 1, 0, &d));
  step(w, 1);
  for (unsigned g = 0; g < CD_GOODS; g++)
    OK(cd_world_goods_import(w, 2, g, 40, "technical work materials", &d));
  OK(cd_world_clinic(w, 2, 1, 20, "technical clinic", &d));
  OK(cd_world_expose(w, patient, CD_ILL, 400, 200, "technical ordinary illness",
                     &d));
  CdRosterSpec rs = {jc, 1u << CD_SERVICE_HEALTH, "Technical care",
                     "explicit care"};
  CdEntityId roster;
  OK(cd_world_roster(w, &rs, &roster, &d));
  CdServiceSpec ds = {roster,           0, CD_SERVICE_HEALTH,   2, patient,
                      cd_now(w) + 1000, 8, "actual finite care"};
  uint64_t request;
  OK(cd_world_dispatch(w, 1, &ds, &request, &d));
  CdDevelopmentSpec ps = {CD_DEV_CAPACITY, 12, 6, NULL,
                          "technical capacity project"};
  uint64_t project;
  OK(cd_world_project(w, 2, 1, &ps, &project, &d));
  CdWorkSpec work = {jw, 0, CD_RECIPE_TOOL, 3, "finite work"};
  uint64_t task;
  OK(cd_world_work_submit(w, worker, 1, &work, &task, &d));
  work = (CdWorkSpec){jb, project, CD_RECIPE_DEVELOPMENT, 6,
                      "actual project labor"};
  OK(cd_world_work_submit(w, builder, 1, &work, &task, &d));
  CdGatheringSpec gs = {
      office, CD_SERVICE_WORSHIP,           cd_now(w) + 1, 6, 2, 0, 1,
      0,      "explicit ordinary gathering"};
  uint64_t gathering;
  OK(cd_world_gathering(w, 1, &gs, &gathering, &d));
  OK(cd_world_attend(w, gathering, worker, 1, &d));
  CdVehicleSpec vs = {jcrew, CD_STRETCHER, 0, "technical assisted carriage"};
  CdEntityId vehicle;
  OK(cd_world_vehicle(w, &vs, &vehicle, &d));
  OK(cd_world_transport(w, vehicle, 1, 1, 2, 0, 0, 0, carried, &d));
  CdPressureSpec rain = {
      CD_PRESSURE_RAIN, 0, cd_now(w) + 1, 10, 1, "explicit weather experiment"};
  uint64_t pressure;
  OK(cd_world_pressure(w, 1, &rain, &pressure, &d));
  CdCommand close = command(w, 1, CD_CMD_CLOSE_EDGE, 209);
  close.earliest = cd_native_minutes(w->native) + 4;
  close.flags = 1;
  uint64_t operation;
  OK(cd_world_operation_queue(w, &close, CD_PERMISSION_INTERVENTION, &operation,
                              &d));
  close.key = 2;
  close.flags = 0;
  close.earliest += 4;
  OK(cd_world_operation_queue(w, &close, CD_PERMISSION_INTERVENTION, &operation,
                              &d));
  OK(cd_world_travel(w, ref(w, 0), 110, &d));
  step(w, 2);
  ledger(w);
  return w;
}
static void cycle31(void) {
  CdWorld *w = busy_world();
  CdPersonRef old = ref(w, 0);
  uint64_t h = digest(w);
  OK(cd_world_save(w, "build/busy.city", &d));
  CdWorld *foreign = NULL;
  assert(cd_world_load(&foreign, content, "build/busy.city", &d) != CD_OK &&
         !foreign && digest(w) == h);
  assert(digest(w) == h);
  CdSnapshot *snapshot = NULL;
  OK(cd_snapshot_capture(w, &snapshot, &d));
  step(w, 30);
  uint64_t expected = digest(w);
  OK(cd_world_load(&w, content, "build/busy.city", &d));
  assert(digest(w) == h &&
         w->people[0].civil.ref.world_token != old.world_token);
  CdResidentView resident;
  assert(cd_world_resolve(w, old, &resident, &d) == CD_UNKNOWN_REFERENCE);
  step(w, 30);
  assert(digest(w) == expected);
  CdPersonView before;
  OK(cd_snapshot_person(snapshot, 0, &before, &d));
  assert(before.ref.world_token == old.world_token);
  cd_snapshot_close(snapshot);
  ledger(w);
  uint64_t live = digest(w);
  CdWorld *owner = w;
  for (size_t n = 1; n < 100; n++) {
    cd_memory_fail_at(n);
    CdStatus status = cd_world_load(&w, content, "build/busy.city", &d);
    cd_memory_fail_at(0);
    if (status == CD_OK)
      break;
    assert(status == CD_CAPACITY && w == owner && digest(w) == live);
    assert(n < 99);
  }
  assert(digest(w) == h);
  step(w, 1);
  live = digest(w);
  cd_checkpoint_fail_publish(1);
  assert(cd_world_save(w, "build/busy.city", &d) == CD_IO_ERROR);
  cd_checkpoint_fail_publish(0);
  assert(digest(w) == live);
  OK(cd_world_load(&w, content, "build/busy.city", &d));
  assert(digest(w) == h);
  cd_world_close(w);
  // A checkpoint of an empty live population restores and keeps admission IDs.
  CdScenario previous = scenario;
  scenario.initial_people = 1;
  w = open_world();
  OK(cd_world_retire(w, ref(w, 0), CD_DEPARTURE, "explicit empty-world fixture",
                     &d));
  h = digest(w);
  OK(cd_world_save(w, "build/empty.city", &d));
  OK(cd_world_load(&w, content, "build/empty.city", &d));
  assert(!w->count && digest(w) == h);
  cd_world_close(w);
  scenario = previous;
  CdSmokeSession *native_owner = NULL;
  OK(cd_smoke_open(1, 1940, &native_owner, &d));
  CdSmokeReport native_before, native_after;
  OK(cd_smoke_read(native_owner, &native_before, &d));
  w = NULL;
  assert(cd_world_load(&w, content, "build/busy.city", &d) ==
             CD_INVALID_ARGUMENT &&
         !w);
  OK(cd_smoke_read(native_owner, &native_after, &d));
  assert(native_before.digest == native_after.digest);
  cd_smoke_close(native_owner);
}
static void cycle32(void) {
  CdWorld *w = busy_world();
  OK(cd_world_save(w, "build/replay.city", &d));
  step(w, 60);
  uint64_t expected = digest(w);
  OK(cd_world_export_records(w, (CdRecordQuery){{0}, CD_RECORD_DIAGNOSTIC},
                             "build/replay-uninterrupted.json", &d));
  OK(cd_world_load(&w, content, "build/replay.city", &d));
  for (unsigned i = 0; i < 60; i++) {
    uint64_t h = digest(w);
    step(w, 0);
    CdSnapshot *s = NULL;
    OK(cd_snapshot_capture(w, &s, &d));
    cd_snapshot_close(s);
    assert(digest(w) == h);
    step(w, 1);
  }
  assert(digest(w) == expected);
  OK(cd_world_export_records(w, (CdRecordQuery){{0}, CD_RECORD_DIAGNOSTIC},
                             "build/replay-resumed.json", &d));
  printf("{\"continuation\":\"%016llx\"}\n", (unsigned long long)expected);
  cd_world_close(w);
}
static void cycle33(void) {
  CdWorld *w = open_world();
  uint64_t h = digest(w);
  for (unsigned i = 0; i < 1000; i++) {
    CdPoint p = {i * .21, i * .1}, screen, back;
    double z = .05 + (i % 100) * .1;
    CdPoint pan = {i - 400., 200. - i};
    OK(cd_camera_transform(p, z, pan, 0, &screen, &d));
    OK(cd_camera_transform(screen, z, pan, 1, &back, &d));
    assert(hypot(p.x - back.x, p.y - back.y) < 1e-9);
  }
  CdPersonRef picked;
  assert(cd_world_pick_person(w, (CdPoint){0, 0}, 10, &picked, &d) ==
         CD_UNKNOWN_REFERENCE);
  assert(digest(w) == h);
  cd_world_close(w);
}
static void cycle34(void) {
  CdAgentGlyph glyphs[] = {{{999, 4}, {45, 78}, 1}, {{999, 1}, {9, 9}, 0}};
  CdPoint location = {0};
  OK(cd_glyph_location(glyphs, 2, 4, &location, &d));
  assert(location.x == 45 && location.y == 78);
  CdAgentGlyph swapped = glyphs[0];
  glyphs[0] = glyphs[1];
  glyphs[1] = swapped;
  glyphs[1].ref.world_token = 777;
  OK(cd_glyph_location(glyphs, 2, 4, &location, &d));
  assert(location.x == 45 && location.y == 78);
  assert(cd_glyph_location(glyphs, 2, 1, &location, &d) ==
         CD_UNKNOWN_REFERENCE);
  assert(cd_glyph_location(glyphs, 2, 3, &location, &d) ==
         CD_UNKNOWN_REFERENCE);

  CdWorld *w = open_world();
  CdPersonRef selected = ref(w, 4), removed = ref(w, 0);
  inside(w, selected, 2);
  CdPoint point = cd_city_position(w, &w->people[4]);
  CdPersonRef picked;
  OK(cd_world_pick_person(w, point, 1, &picked, &d));
  assert(picked.person_id == selected.person_id);
  uint64_t h = digest(w);
  CdResidentView view;
  OK(cd_world_resolve(w, selected, &view, &d));
  assert(digest(w) == h);
  OK(cd_world_save(w, "build/selection.city", &d));
  OK(cd_world_retire(w, removed, CD_DEPARTURE, "compaction fixture", &d));
  OK(cd_world_reference(w, selected.person_id, &picked, &d));
  assert(picked.person_id == selected.person_id);
  OK(cd_world_load(&w, content, "build/selection.city", &d));
  assert(cd_world_resolve(w, selected, &view, &d) == CD_UNKNOWN_REFERENCE);
  OK(cd_world_reference(w, selected.person_id, &picked, &d));
  OK(cd_world_resolve(w, picked, &view, &d));
  assert(view.position_known && view.ref.person_id == selected.person_id);
  cd_world_close(w);
}
static void malformed(void) {
  CdWorld *w = open_world();
  uint64_t h = digest(w);
  CdWorld *owner = w;
  for (unsigned i = 0; i < 8; i++) {
    char path[96];
    snprintf(path, sizeof path, "build/rejected-%u.city", i);
    assert(cd_world_load(&w, content, path, &d) != CD_OK);
    assert(w == owner && digest(w) == h);
  }
  cd_world_close(w);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  unsigned cycle = (unsigned)atoi(argv[1]);
  OK(cd_content_open("build/fixtures/civic.bin", &content, &d));
  switch (cycle) {
  case 29:
    cycle29();
    break;
  case 30:
    cycle30();
    break;
  case 31:
    cycle31();
    break;
  case 32:
    cycle32();
    break;
  case 33:
    cycle33();
    break;
  case 34:
    cycle34();
    break;
  case 101:
    malformed();
    break;
  case 100: {
    CdWorld *w = NULL;
    OK(cd_world_load(&w, content, "build/replay.city", &d));
    step(w, 60);
    printf("{\"continuation\":\"%016llx\"}\n", (unsigned long long)digest(w));
    cd_world_close(w);
    break;
  }
  default:
    abort();
  }
  cd_content_close(content);
  assert(!cd_memory_stats().total);
  printf("{\"cycle\":%u,\"ok\":true,\"closed_bytes\":0}\n", cycle);
  return 0;
}
