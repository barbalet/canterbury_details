#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#define main cd_observer_previous_main
#include "observer.c"
#undef main
#include "cd_core.h"
#include <sys/resource.h>
#include <time.h>
static double wall(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}
static void management(void) {
  CdWorld *w = busy_world();
  uint64_t before = digest(w);
  CdManagementRow rows[2];
  for (unsigned k = 0; k < CD_MANAGE_KINDS; k++) {
    size_t total = 0, n = 0;
    OK(cd_world_management(w, k, 0, rows, 2, &total, &n, &d));
    assert(n <= 2 && n <= total);
    size_t total2, n2;
    OK(cd_world_management(w, k, SIZE_MAX, NULL, 0, &total2, &n2, &d));
    assert(total2 == total && !n2);
    if (total > 2) {
      OK(cd_world_management(w, k, 2, rows, 2, &total2, &n2, &d));
      assert(n2 && total2 == total);
    }
    printf("{\"category\":%u,\"total\":%zu}\n", k, total);
  }
  size_t total, n;
  assert(cd_world_management(w, CD_MANAGE_KINDS, 0, rows, 2, &total, &n, &d) ==
         CD_INVALID_ARGUMENT);
  assert(cd_world_management(w, 0, 0, rows, 257, &total, &n, &d) ==
         CD_INVALID_ARGUMENT);
  assert(digest(w) == before);
  OK(cd_world_save(w, "build/management.city", &d));
  ledger(w);
  cd_world_close(w);
}
static void navigation(void) {
  CdWorld *w = open_world();
  uint64_t before = digest(w);
  size_t counts[] = {1, 199, 200, 201, 27000, 30000};
  for (unsigned n = 0; n < 6; n++)
    for (size_t i = 0; i < counts[n]; i += 17)
      for (unsigned key = 0; key <= CD_ROSTER_LAST; key++) {
        size_t next;
        assert(cd_roster_navigate(counts[n], i, key, &next) == CD_OK &&
               next < counts[n]);
        if (key == CD_ROSTER_FIRST)
          assert(!next);
        if (key == CD_ROSTER_LAST)
          assert(next == counts[n] - 1);
      }
  size_t next;
  assert(cd_roster_navigate(0, 0, CD_ROSTER_LEFT, &next) ==
         CD_INVALID_ARGUMENT);
  assert(cd_roster_navigate(1, 1, CD_ROSTER_LEFT, &next) ==
         CD_INVALID_ARGUMENT);
  for (uint64_t i = 0; i < 10000; i++) {
    uint64_t a = cd_extension_random(1940, 7, i);
    assert(a == cd_extension_random(1940, 7, i));
    assert(a != cd_extension_random(1940, 8, i));
  }
  assert(before == digest(w));
  cd_world_close(w);
}
static void robustness(void) {
  CdWorld *w = busy_world();
  uint64_t before = digest(w);
  OK(cd_world_save(w, "build/robust.city", &d));
  size_t baseline = cd_memory_stats().total;
  cd_memory_fail_at(1);
  assert(cd_world_save(w, "build/robust.city", &d) == CD_CAPACITY);
  cd_memory_fail_at(0);
  assert(digest(w) == before && cd_memory_stats().total == baseline);
  for (size_t n = 1; n < 100; n++) {
    CdSnapshot *s = NULL;
    cd_memory_fail_at(n);
    CdStatus status = cd_snapshot_capture(w, &s, &d);
    cd_memory_fail_at(0);
    if (status == CD_OK) {
      cd_snapshot_close(s);
      break;
    }
    assert(status == CD_CAPACITY && !s && digest(w) == before &&
           cd_memory_stats().total == baseline);
    assert(n < 99);
  }
  for (unsigned i = 0; i < 4; i++) {
    CdWorld *old = w;
    assert(cd_world_load(&w, content, "build/invalid-release.city", &d) !=
               CD_OK &&
           w == old && digest(w) == before);
  }
  cd_world_close(w);
  for (unsigned i = 0; i < 5; i++) {
    w = open_world();
    step(w, 1);
    cd_world_close(w);
  }
}
static void performance(size_t population, unsigned minutes, const char *mode) {
  scenario.initial_people = population;
  double start = wall();
  CdWorld *w;
  int actual = !strcmp(mode, "actual");
  if (actual) {
    scenario.civil_origin = (CdCivilTime){1940, 1, 1, 0};
    w = open_world();
  } else if (!strcmp(mode, "busy")) {
    w = busy_world();
    for (unsigned i = 0; i < CD_DISPATCH_LIMIT - 1; i++) {
      CdServiceSpec s = {1,
                         0,
                         CD_SERVICE_HEALTH,
                         2,
                         ref(w, 2),
                         cd_now(w) + 10080,
                         1,
                         "bounded full-cohort care load"};
      uint64_t id;
      OK(cd_world_dispatch(w, 2000 + i, &s, &id, &d));
    }
    CdPressureSpec p = {CD_PRESSURE_FIRE,     2, cd_now(w) + 4, 30, 40,
                        "technical fire load"};
    uint64_t id;
    OK(cd_world_pressure(w, 2, &p, &id, &d));
  } else {
    if (!strcmp(mode, "churn"))
      content->polygons[cd_polygon_index(content, 1)].capacity =
          1000; /* Explicit technical housing fixture. */
    w = open_world();
    if (!strcmp(mode, "crowded")) {
      for (size_t i = 0; i < w->count; i++) {
        OK(cd_world_place(w, ref(w, i), 107, &d));
        OK(cd_world_travel(w, ref(w, i), 110, &d));
      }
    } else if (!strcmp(mode, "churn")) {
      CdPersonRef parents[100];
      size_t found = 0;
      for (size_t i = 0; i < w->count && found < 100; i++)
        if (w->people[i].civil.age >= 18)
          parents[found++] = ref(w, i);
      assert(found == 100);
      for (unsigned i = 0; i < 100; i++)
        OK(cd_world_household_move(
            w, w->people[parents[i].person_id - 1].civil.household, 1, &d));
      for (unsigned i = 0; i < 100; i++)
        OK(cd_world_retire(w, ref(w, w->count - 1),
                           i % 2 ? CD_DEATH : CD_DEPARTURE,
                           "full-cohort lifecycle churn", &d));
      for (unsigned i = 0; i < 100; i++) {
        CdPersonRef child;
        OK(cd_world_birth(w, parents[i], (CdPersonRef){0},
                          "explicit technical birth load", &child, &d));
      }
    }
  }
  double opened = wall();
  step(w, minutes);
  double stepped = wall();
  ledger(w);
  for (uint32_t e = 0; e < w->content->ne; e++)
    if (w->content->edges[e].qualified)
      assert(w->traffic[e] <= w->content->edges[e].capacity);
  uint64_t expected = digest(w);
  double hashed = wall();
  OK(cd_world_save(w, "build/performance.city", &d));
  double saved = wall();
  OK(cd_world_load(&w, content, "build/performance.city", &d));
  double loaded = wall();
  assert(digest(w) == expected);
  CdCitySummary summary;
  OK(cd_world_summary(w, &summary, &d));
  size_t peak = cd_memory_stats().peak_total;
  cd_world_close(w);
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  uint64_t rss = usage.ru_maxrss;
#ifndef __APPLE__
  rss *= 1024;
#endif
  printf("{\"mode\":\"%s\",\"population\":%zu,\"minutes\":%u,\"initialization_"
         "seconds\":%.6f,\"step_seconds\":%.6f,\"digest_seconds\":%.6f,\"save_"
         "seconds\":%.6f,\"load_seconds\":%.6f,\"wall_seconds\":%.6f,\"cpu_"
         "seconds\":%.6f,\"peak_rss_bytes\":%llu,\"owned_peak_bytes\":%zu,"
         "\"people_after\":%zu,\"digest\":\"%016llx\",\"checkpoint_match\":"
         "true,\"ok\":true}\n",
         mode, population, minutes, opened - start, stepped - opened,
         hashed - stepped, saved - hashed, loaded - saved, wall() - start,
         usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 +
             usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6,
         (unsigned long long)rss, peak, summary.people,
         (unsigned long long)expected);
}
int main(int argc, char **argv) {
  assert(argc >= 2);
  const char *path = "build/fixtures/civic.bin";
  if (!strcmp(argv[1], "perf") && argc == 5 && !strcmp(argv[4], "actual"))
    path = "build/content/CanterburyCity.bin";
  OK(cd_content_open(path, &content, &d));
  if (!strcmp(argv[1], "35"))
    management();
  else if (!strcmp(argv[1], "36"))
    navigation();
  else if (!strcmp(argv[1], "38"))
    robustness();
  else if (!strcmp(argv[1], "perf") && argc == 5)
    performance(strtoul(argv[2], NULL, 10), strtoul(argv[3], NULL, 10),
                argv[4]);
  else
    abort();
  cd_content_close(content);
  assert(!cd_memory_stats().total);
  if (strcmp(argv[1], "perf"))
    printf("{\"cycle\":%d,\"ok\":true,\"closed_bytes\":0}\n", atoi(argv[1]));
}
