#ifndef CD_PEOPLE_H
#define CD_PEOPLE_H
#include "cd_api.h"
typedef struct CdPeople CdPeople;
/* Live project membership; native traits and actor IDs stay SDK-owned. */
typedef struct {
  CdPersonRef ref;
  size_t native_slot;
} CdPersonRecord;
CdPeople *cd_people_create(size_t capacity, uint64_t token);
void cd_people_close(CdPeople *);
size_t cd_people_count(const CdPeople *);
uint64_t cd_people_token(const CdPeople *);
const CdPersonRecord *cd_people_find(const CdPeople *, CdPersonRef);
const CdPersonRecord *cd_people_at(const CdPeople *, size_t record);
CdStatus cd_people_add(CdPeople *, CdPersonId, size_t slot);
CdStatus cd_people_remove(CdPeople *, CdPersonRef);
CdStatus cd_people_validate_order(CdPeople *, const CdPersonId *, size_t);
void cd_people_commit_order(CdPeople *, const CdPersonId *, size_t);
#endif
