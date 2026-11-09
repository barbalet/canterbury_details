#include "cd_bootstrap.h"
#include "cd_city.h"
#include "cd_observer.h"
#include "cd_management.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int number(const char *text, uint64_t *value) {
  char *end;
  errno = 0;
  if (!text || !*text || *text == '-' || *text == '+')
    return 0;
  unsigned long long n = strtoull(text, &end, 10);
  if (errno || *end)
    return 0;
  *value = n;
  return 1;
}
static void json_text(const char *s) {
  putchar('"');
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p == '"' || *p == '\\') {
      putchar('\\');
      putchar(*p);
    } else if (*p < 32)
      printf("\\u%04x", *p);
    else
      putchar(*p);
  }
  putchar('"');
}
static CdStatus management_output(const CdWorld *w, int kind, size_t offset,
                                  CdDiagnostic *d) {
  CdManagementRow rows[100];
  size_t total, written;
  CdStatus status =
      cd_world_management(w, kind, offset, rows, 100, &total, &written, d);
  if (status != CD_OK)
    return status;
  printf("{\"management\":%d,\"total\":%zu,\"offset\":%zu,\"rows\":[", kind,
         total, offset);
  for (size_t i = 0; i < written; i++) {
    CdManagementRow *r = &rows[i];
    printf("%s{\"id\":%" PRIu64 ",\"actor\":%" PRIu64
           ",\"state\":%u,\"known\":%s,\"name\":",
           i ? "," : "", r->id, r->actor, r->state,
           r->known ? "true" : "false");
    json_text(r->name);
    fputs(",\"detail\":", stdout);
    json_text(r->detail);
    putchar('}');
  }
  puts("]}");
  return CD_OK;
}
int main(int argc, char **argv) {
  int manage = -1;
  size_t page = 0;
  uint64_t people = cd_smoke_default_people(), minutes = 5, seed = 1940;
  const char *city_path = NULL, *load_path = NULL, *save_path = NULL,
             *program_path = NULL, *record_path = NULL;
  int intervene = 0, diagnostic = 0, explicit_minutes = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--intervene")) {
      intervene = 1;
      continue;
    }
    if (!strcmp(argv[i], "--diagnostic")) {
      diagnostic = 1;
      continue;
    }
    if (!strcmp(argv[i], "--help")) {
      puts("CanterburyDetails --people 1..30000 --minutes 0..10000 --seed "
           "0..4294967295\nAdd --city build/content/CanterburyCity.bin for the "
           "native-backed city with explicit synthetic demographics. Default "
           "mode retains the native bootstrap diagnostic. City options: --load "
           "FILE --save FILE --program FILE --intervene --records FILE "
           "--diagnostic --manage 0..10 --page 0..30000. Programs use the "
           "documented ordinary operation "
           "protocol; load/program defaults to zero extra minutes.");
      return 0;
    }
    if (i + 1 >= argc) {
      fputs("Missing option value\n", stderr);
      return 2;
    }
    const char *option = argv[i];
    if (!strcmp(option, "--load")) {
      load_path = argv[++i];
      continue;
    }
    if (!strcmp(option, "--save")) {
      save_path = argv[++i];
      continue;
    }
    if (!strcmp(option, "--program")) {
      program_path = argv[++i];
      continue;
    }
    if (!strcmp(option, "--records")) {
      record_path = argv[++i];
      continue;
    }
    if (!strcmp(option, "--city")) {
      city_path = argv[++i];
      continue;
    }
    uint64_t value;
    if (!number(argv[++i], &value)) {
      fputs("Invalid numeric option\n", stderr);
      return 2;
    }
    if (!strcmp(option, "--people"))
      people = value;
    else if (!strcmp(option, "--minutes")) {
      minutes = value;
      explicit_minutes = 1;
    } else if (!strcmp(option, "--manage") && value < CD_MANAGE_KINDS)
      manage = (int)value;
    else if (!strcmp(option, "--page") && value <= 30000)
      page = (size_t)value;
    else if (!strcmp(option, "--seed"))
      seed = value;
    else {
      fputs("Unknown option\n", stderr);
      return 2;
    }
  }
  if (!people || people > cd_smoke_capacity() || minutes > 10000 ||
      seed > UINT32_MAX) {
    fputs("Bootstrap parameter outside supported bounds\n", stderr);
    return 2;
  }
  if ((load_path || save_path || program_path || record_path || manage >= 0) &&
      !city_path)
    city_path = "build/content/CanterburyCity.bin";
  if ((program_path || load_path || manage >= 0) && !explicit_minutes)
    minutes = 0;
  if (city_path) {
    CdDiagnostic d;
    CdContent *content = NULL;
    CdWorld *world = NULL;
    CdScenario scenario = {
        (size_t)people, seed, {1940, 1, 1, 0}, "synthetic-ordinary-v1"};
    CdCitySummary summary;
    uint64_t digest;
    int ok =
        cd_content_open(city_path, &content, &d) == CD_OK &&
        (load_path ? cd_world_load(&world, content, load_path, &d)
                   : cd_world_open(content, &scenario, &world, &d)) == CD_OK &&
        (!program_path ||
         cd_world_run_program(world, program_path,
                              intervene ? CD_PERMISSION_INTERVENTION
                                        : CD_PERMISSION_OBSERVER,
                              &d) == CD_OK) &&
        cd_world_step(world, (unsigned)minutes, &d) == CD_OK &&
        cd_world_summary(world, &summary, &d) == CD_OK &&
        cd_world_digest(world, &digest, &d) == CD_OK &&
        (!save_path || cd_world_save(world, save_path, &d) == CD_OK) &&
        (!record_path ||
         cd_world_export_records(
             world,
             (CdRecordQuery){
                 {0}, diagnostic ? CD_RECORD_DIAGNOSTIC : CD_RECORD_PUBLIC},
             record_path, &d) == CD_OK);
    if (ok && manage >= 0)
      ok = management_output(world, manage, page, &d) == CD_OK;
    if (ok)
      printf("{\"scope\":\"ordinary-city\",\"policy\":\"synthetic-ordinary-"
             "v1\",\"people\":%zu,\"households\":%zu,\"housing_unknown\":%zu,"
             "\"located\":%zu,\"native_minutes\":%" PRIu64
             ",\"digest\":\"%016" PRIx64 "\",\"ok\":true}\n",
             summary.people, summary.households, summary.housing_unknown,
             summary.located, summary.native_minutes, digest);
    else
      fprintf(stderr, "%s\n", d.message);
    cd_world_close(world);
    cd_content_close(content);
    return ok ? 0 : 1;
  }
  CdSmokeSession *session = NULL;
  CdDiagnostic error;
  CdSmokeReport report;
  CdCivilTime time;
  if (cd_smoke_open((size_t)people, (uint32_t)seed, &session, &error) !=
      CD_OK) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }
  int ok = cd_smoke_step(session, (unsigned)minutes, &error) == CD_OK &&
           cd_smoke_read(session, &report, &error) == CD_OK &&
           cd_smoke_time(session, &time, &error) == CD_OK;
  if (ok)
    printf("{\"scope\":\"cycle03-native-smoke\",\"profile\":\"CANTERBURY_SIM=1,"
           "BRONZE_SIM=1\",\"people\":%zu,\"native_people\":%zu,\"actor_"
           "records\":%zu,\"native_minutes\":%" PRIu64 ",\"callbacks\":%" PRIu64
           ",\"digest\":\"%016" PRIx64
           "\",\"bindings_valid\":%s,\"scenario_time\":\"%04d-%02u-%02uT%02u:%"
           "02u\",\"ok\":true}\n",
           report.people, report.native_people, report.actor_records,
           report.native_minutes, report.callbacks, report.digest,
           report.bindings_valid ? "true" : "false", time.year, time.month,
           time.day, time.minute_of_day / 60, time.minute_of_day % 60);
  else
    fprintf(stderr, "%s\n", error.message);
  cd_smoke_close(session);
  return ok ? 0 : 1;
}
