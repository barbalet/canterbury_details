#include "cd_city_internal.h"
#include "cd_native.h"
#include "bronze/actors/bronze_actors.h"
#include "entity/entity.h"
#include <assert.h>
#include <stdlib.h>
static CdDiagnostic d;
#define OK(x)                                                                  \
  do {                                                                         \
    CdStatus s_ = (x);                                                         \
    if (s_ != CD_OK) {                                                         \
      fprintf(stderr, "line %d: %s\n", __LINE__, d.message);                   \
      abort();                                                                 \
    }                                                                          \
  } while (0)
static uint64_t digest(CdWorld *w) {
  uint64_t h;
  OK(cd_world_digest(w, &h, &d));
  return h;
}
static unsigned events(CdWorld *w, CdPersonRef ref, CdEventKind kind) {
  CdRecordSummary s;
  OK(cd_world_record_summary(w, &s, &d));
  unsigned n = 0;
  for (uint64_t id = s.first; id < s.next; id++) {
    CdRecord r;
    if (cd_world_record(w, id, (CdRecordQuery){ref, CD_RECORD_PERSONAL}, &r,
                        NULL) == CD_OK &&
        r.actor.person_id == ref.person_id && r.kind == kind)
      n++;
  }
  return n;
}
static simulated_being *body(CdPersonRef ref) {
  return bronze_actors_resolve(bronze_actors_current(), ref.person_id);
}
static int remembers(CdPersonRef ref, unsigned code, unsigned food) {
  simulated_iepisodic *e = being_episodic(body(ref));
  for (unsigned i = 0; i < EPISODIC_SIZE; i++)
    if (e[i].event == code && (code != EVENT_EAT || e[i].food == food))
      return 1;
  return 0;
}
static void travel(CdWorld *w, CdPersonRef who, CdEntityId node) {
  OK(cd_world_travel(w, who, node, &d));
  for (unsigned i = 0; i < 200; i++) {
    CdTravelView v;
    OK(cd_world_travel_view(w, who, &v, &d));
    if (v.state == CD_ROUTE_ARRIVED)
      return;
    OK(cd_world_step(w, 1, &d));
  }
  abort();
}
int main(void) {
  CdContent *c = NULL;
  CdWorld *w = NULL;
  OK(cd_content_open("build/fixtures/human.bin", &c, &d));
  CdScenario scenario = {8, 1940, {1940, 1, 1, 480}, "synthetic-ordinary-v1"};
  OK(cd_world_open(c, &scenario, &w, &d));
  CdPersonRef a = w->people[0].civil.ref, b = w->people[1].civil.ref;
  /* Unknown positions must not silently become conversations. */
  OK(cd_world_step(w, 60, &d));
  assert(events(w, a, CD_EVENT_CHAT) == 0);
  /* Actual finite consumption emits a native food memory and a personal event.
   */
  unsigned meals = w->people[0].meals;
  cd_native_energy_set(w->native, a, (int)cd_native_hungry() - 10);
  OK(cd_world_step(w, 1, &d));
  assert(w->people[0].meals == meals - 1 && events(w, a, CD_EVENT_EAT) == 1);
  assert(remembers(a, EVENT_EAT, FOOD_PREPARED));
  CdFoodLedger food;
  OK(cd_world_food_ledger(w, &food, &d));
  assert(food.consumed == 1);
  uint64_t before = digest(w);
  unsigned n = events(w, a, CD_EVENT_EAT);
  OK(cd_world_step(w, 0, &d));
  assert(events(w, a, CD_EVENT_EAT) == n && digest(w) == before);
  /* Real source-node co-location, then only the unused native minute may chat.
   */
  OK(cd_world_place(w, a, 107, &d));
  OK(cd_world_place(w, b, 107, &d));
  OK(cd_world_step(w, (unsigned)(60 - cd_native_minutes(w->native) % 60), &d));
  assert(events(w, a, CD_EVENT_CHAT) == 1 && events(w, b, CD_EVENT_CHAT) == 1);
  assert(remembers(a, EVENT_CHAT, 0) && remembers(b, EVENT_CHAT, 0));
  assert(w->people[0].activity == CD_ACTIVITY_CHAT &&
         w->people[1].activity == CD_ACTIVITY_CHAT);
  CdRecordSummary history;
  OK(cd_world_record_summary(w, &history, &d));
  int privacy_checked = 0;
  for (uint64_t id = history.first; id < history.next; id++) {
    CdRecord r;
    if (cd_world_record(w, id, (CdRecordQuery){a, CD_RECORD_PERSONAL}, &r,
                        NULL) == CD_OK &&
        r.actor.person_id == a.person_id && r.kind == CD_EVENT_CHAT) {
      assert(strstr(r.message, w->people[1].civil.name));
      assert(cd_world_record(w, id, (CdRecordQuery){b, CD_RECORD_PERSONAL}, &r,
                             NULL) == CD_UNSUPPORTED);
      privacy_checked = 1;
    }
  }
  assert(privacy_checked);
  before = digest(w);
  OK(cd_world_save(w, "build/activity-events.city", &d));
  OK(cd_world_step(w, 60, &d));
  uint64_t continuation = digest(w);
  OK(cd_world_load(&w, c, "build/activity-events.city", &d));
  assert(digest(w) == before);
  a = w->people[0].civil.ref;
  b = w->people[1].civil.ref;
  assert(remembers(a, EVENT_EAT, FOOD_PREPARED) && remembers(a, EVENT_CHAT, 0));
  for (unsigned i = 0; i < 60; i++)
    OK(cd_world_step(w, 1, &d));
  assert(digest(w) == continuation);
  /* Busy, sleeping and separated people do not create successful chats. */
  unsigned chats = events(w, a, CD_EVENT_CHAT);
  travel(w, b, 110);
  OK(cd_world_step(w, 60, &d));
  assert(events(w, a, CD_EVENT_CHAT) == chats);
  travel(w, b, 107);
  chats = events(w, a, CD_EVENT_CHAT);
  OK(cd_world_rest(w, b, 60, &d));
  OK(cd_world_step(w, 60, &d));
  assert(events(w, a, CD_EVENT_CHAT) == chats);
  CdPersonRef baby = {0};
  OK(cd_world_household_move(w, 1, 2, &d));
  cd_native_energy_set(w->native, a, (int)cd_native_full());
  cd_native_awake(w->native, a, 1);
  OK(cd_world_birth(w, a, b, "technical event birth fixture", &baby, &d));
  assert(remembers(a, EVENT_BIRTH, 0));
  for (unsigned i = 0; i < CD_RECORD_LIMIT + 1; i++)
    cd_record_emit(w, CD_EVENT_WORLD, (CdPersonRef){0}, (CdPersonRef){0}, 0, 0,
                   CD_RECORD_DIAGNOSTIC, "technical retention fixture",
                   "ring overwrite");
  char memories[4096];
  before = digest(w);
  OK(cd_world_activity_memories(w, a, (CdRecordQuery){a, CD_RECORD_PERSONAL},
                                memories, sizeof memories, &d));
  assert(strstr(memories, "prepared meal") && digest(w) == before);
  assert(cd_world_activity_memories(
             w, a, (CdRecordQuery){b, CD_RECORD_PERSONAL}, memories,
             sizeof memories, &d) == CD_UNSUPPORTED);
  assert(events(w, a, CD_EVENT_EAT) ==
         0); /* Native memories survive city-ring eviction. */
  char partner_name[128];
  snprintf(partner_name, sizeof partner_name, "%s",
           cd_citizen(w, b)->civil.name);
  CdPersonRef order[16];
  size_t used = 0;
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].civil.ref.person_id != b.person_id)
      order[used++] = w->people[i].civil.ref;
  order[used++] = b;
  assert(used == w->count);
  OK(cd_smoke_reorder(w->native, order, used, &d));
  simulated_being *old_partner_slot = body(b);
  OK(cd_world_retire(w, a, CD_DEPARTURE,
                     "technical native memory compaction fixture", &d));
  CdResidentView survivor;
  OK(cd_world_resolve(w, b, &survivor, &d));
  assert(body(b) != old_partner_slot && !strcmp(survivor.name, partner_name) &&
         remembers(b, EVENT_CHAT, 0));
  OK(cd_world_activity_memories(w, b, (CdRecordQuery){b, CD_RECORD_PERSONAL},
                                memories, sizeof memories, &d));
  assert(strstr(memories, "conversation"));
  cd_world_close(w);
  cd_content_close(c);
  assert(cd_memory_stats().total == 0);
  puts("PASS native meals, paired co-located chats, private civil-name "
       "history, pause, checkpoints and replay");
  return 0;
}
