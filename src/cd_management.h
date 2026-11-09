#ifndef CD_MANAGEMENT_H
#define CD_MANAGEMENT_H
#include "cd_observer.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded copied diagnostic pages; no pointers into model/native storage. */
typedef enum {
  CD_MANAGE_JOBS,
  CD_MANAGE_WORK,
  CD_MANAGE_PREMISES,
  CD_MANAGE_PROJECTS,
  CD_MANAGE_ROSTERS,
  CD_MANAGE_SERVICES,
  CD_MANAGE_OFFICES,
  CD_MANAGE_GATHERINGS,
  CD_MANAGE_VEHICLES,
  CD_MANAGE_PRESSURES,
  CD_MANAGE_OPERATIONS,
  CD_MANAGE_KINDS
} CdManagementKind;
typedef struct {
  CdEntityId id, actor, related;
  unsigned state;
  int known;
  uint64_t quantity, balance;
  char name[128], detail[512];
} CdManagementRow;
typedef enum {
  CD_ROSTER_LEFT,
  CD_ROSTER_RIGHT,
  CD_ROSTER_UP,
  CD_ROSTER_DOWN,
  CD_ROSTER_FIRST,
  CD_ROSTER_LAST
} CdRosterDirection;
static inline CdStatus cd_roster_navigate(size_t count, size_t current,
                                          CdRosterDirection direction,
                                          size_t *out) {
  if (!out || !count || current >= count ||
      (unsigned)direction > CD_ROSTER_LAST)
    return CD_INVALID_ARGUMENT;
  switch (direction) {
  case CD_ROSTER_LEFT:
    *out = current ? current - 1 : 0;
    break;
  case CD_ROSTER_RIGHT:
    *out = current + 1 < count ? current + 1 : count - 1;
    break;
  case CD_ROSTER_UP:
    *out = current >= 200 ? current - 200 : 0;
    break;
  case CD_ROSTER_DOWN:
    *out = current + 200 < count ? current + 200 : count - 1;
    break;
  case CD_ROSTER_FIRST:
    *out = 0;
    break;
  case CD_ROSTER_LAST:
    *out = count - 1;
    break;
  }
  return CD_OK;
}
const char *cd_management_name(CdManagementKind);
CdStatus cd_world_management(const CdWorld *, CdManagementKind, size_t offset,
                             CdManagementRow *, size_t capacity, size_t *total,
                             size_t *written, CdDiagnostic *);
#ifdef __cplusplus
}
#endif
#endif
