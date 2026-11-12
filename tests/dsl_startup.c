#include "cd_city.h"
#include "cd_scenario_startup.h"
#include "cd_observer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
  assert(argc == 4);
  CdDiagnostic d = {0};
  CdContent *content = NULL;
  CdWorld *world = NULL;
  CdScenarioStartup startup;
  assert(cd_content_open(argv[1], &content, &d) == CD_OK);
  assert(cd_scenario_startup_read(content, argv[2], &startup, &d) == CD_OK);
  assert(cd_world_open_startup(content, &startup, &world, &d) == CD_OK);
  unsigned counts[5] = {0};
  for (size_t i = 0; i < startup.scenario.initial_people; ++i) {
    CdResidentView person;
    assert(cd_world_resident(world, i, &person, &d) == CD_OK);
    unsigned age = person.age;
    ++counts[age < 5 ? 0 : age < 15 ? 1 : age < 18 ? 2 : age < 65 ? 3 : 4];
    if (age < 18) {
      CdResidentView guardian;
      assert(cd_world_resolve(world, person.guardian, &guardian, &d) == CD_OK);
      assert(guardian.age >= 18 && guardian.household == person.household);
    }
  }
  for (unsigned i = 0; i < 5; ++i)
    assert(counts[i] == startup.cohorts[i]);
  uint64_t before, after;
  assert(cd_world_digest(world, &before, &d) == CD_OK);
  CdScenarioStartup rejected;
  assert(cd_scenario_startup_read(content, argv[3], &rejected, &d) != CD_OK);
  rejected = startup;
  ++rejected.cohorts[0];
  assert(cd_world_open_startup(content, &rejected, &world, &d) != CD_OK);
  assert(cd_world_digest(world, &after, &d) == CD_OK && before == after);
  assert(cd_world_step(world, 4, &d) == CD_OK);
  CdCitySummary summary;
  assert(cd_world_summary(world, &summary, &d) == CD_OK);
  assert(summary.native_minutes == 5);
  printf("PASS %zu residents, cohorts, guardians, rejected replacement and native step\n",
         summary.people);
  cd_world_close(world);
  cd_content_close(content);
}
