#ifndef CD_SCENARIO_STARTUP_H
#define CD_SCENARIO_STARTUP_H
#include "cd_api.h"

typedef struct {
  CdScenario scenario;
  unsigned cohorts[5];
  char identity[65];
} CdScenarioStartup;

CdStatus cd_scenario_startup_read(const CdContent *, const char *,
                                  CdScenarioStartup *, CdDiagnostic *);
CdStatus cd_world_open_startup(const CdContent *, const CdScenarioStartup *,
                               CdWorld **, CdDiagnostic *);

#endif
