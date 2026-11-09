/* Independent ordinary policy consumer; links only the packaged public headers
 * and libCanterburyCore.a. No private state or Cocoa application entry point.
 */
#include "cd_core.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(call)                                                            \
  do {                                                                         \
    if ((call) != CD_OK) {                                                     \
      fprintf(stderr, "%s\n", d.message);                                      \
      goto fail;                                                               \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    fprintf(stderr,
            "Usage: OrdinaryFrontend content.bin checkpoint.city [people]\n");
    return 2;
  }
  size_t people = 32;
  if (argc == 4) {
    char *end;
    unsigned long n = strtoul(argv[3], &end, 10);
    if (!*argv[3] || *end || !n || n > 30000)
      return 2;
    people = n;
  }
  CdDiagnostic d = {0};
  CdContent *content = NULL;
  CdWorld *world = NULL, *foreign = NULL;
  CdSnapshot *snapshot = NULL;
  uint64_t before, expected, actual;
  CdScenario scenario = {
      people, 1940, {1940, 1, 1, 0}, "synthetic-ordinary-v1"};
  CHECK(cd_content_open(argv[1], &content, &d));
  CHECK(cd_world_open(content, &scenario, &world, &d));
  if (cd_world_open(content, &scenario, &foreign, &d) == CD_OK || foreign)
    goto fail;
  CHECK(cd_world_digest(world, &before, &d));
  CHECK(cd_world_step(world, 0, &d));
  CHECK(cd_snapshot_capture(world, &snapshot, &d));
  if (cd_snapshot_person_count(snapshot) != people)
    goto fail;
  cd_snapshot_close(snapshot);
  snapshot = NULL;
  for (uint64_t i = 0; i < 100; i++)
    (void)cd_extension_random(1940, UINT64_C(0x4f5244494e415259), i);
  CHECK(cd_world_digest(world, &actual, &d));
  if (before != actual)
    goto fail;
  CHECK(cd_world_step(world, 1, &d));
  CHECK(cd_world_save(world, argv[2], &d));
  CHECK(cd_world_step(world, 10, &d));
  CHECK(cd_world_digest(world, &expected, &d));
  CHECK(cd_world_load(&world, content, argv[2], &d));
  CHECK(cd_world_step(world, 10, &d));
  CHECK(cd_world_digest(world, &actual, &d));
  if (expected != actual)
    goto fail;
  CdClockView clock;
  CHECK(cd_world_clock(world, &clock, &d));
  printf("{\"core\":\"%s\",\"people\":%zu,\"native_minutes\":%" PRIu64
         ",\"digest\":\"%016" PRIx64
         "\",\"pause_rng_pure\":true,\"checkpoint_match\":true,\"ok\":true}\n",
         cd_core_version(), people, clock.native_minutes, actual);
  cd_world_close(world);
  cd_content_close(content);
  return 0;
fail:
  cd_snapshot_close(snapshot);
  cd_world_close(world);
  cd_content_close(content);
  return 1;
}
