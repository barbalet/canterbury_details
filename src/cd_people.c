#include "cd_people.h"
#include "cd_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
struct CdPeople {
  CdPersonRecord *records;
  unsigned char *seen;
  size_t count, capacity;
  uint64_t token, last_id;
};
CdPeople *cd_people_create(size_t capacity, uint64_t token) {
  if (!token || !capacity || capacity > SIZE_MAX / sizeof(CdPersonRecord))
    return NULL;
  CdPeople *p = cd_memory_calloc(1, sizeof *p, CD_MEM_CITY);
  if (!p)
    return NULL;
  p->records = cd_memory_calloc(capacity, sizeof *p->records, CD_MEM_CITY);
  p->seen = cd_memory_calloc(capacity, 1, CD_MEM_TEMP);
  if (!p->records || !p->seen) {
    cd_people_close(p);
    return NULL;
  }
  p->capacity = capacity;
  p->token = token;
  return p;
}
void cd_people_close(CdPeople *p) {
  if (p) {
    cd_memory_free(p->records);
    cd_memory_free(p->seen);
    cd_memory_free(p);
  }
}
size_t cd_people_count(const CdPeople *p) { return p ? p->count : 0; }
uint64_t cd_people_token(const CdPeople *p) { return p ? p->token : 0; }
const CdPersonRecord *cd_people_find(const CdPeople *p, CdPersonRef ref) {
  if (!p || !ref.person_id || ref.world_token != p->token)
    return NULL;
  size_t lo = 0, hi = p->count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (p->records[mid].ref.person_id < ref.person_id)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo < p->count && p->records[lo].ref.person_id == ref.person_id
             ? &p->records[lo]
             : NULL;
}
const CdPersonRecord *cd_people_at(const CdPeople *p, size_t i) {
  return p && i < p->count ? &p->records[i] : NULL;
}
CdStatus cd_people_add(CdPeople *p, CdPersonId id, size_t slot) {
  if (!p || !id || slot >= p->capacity)
    return CD_INVALID_ARGUMENT;
  if (p->count == p->capacity)
    return CD_CAPACITY;
  /* SDK IDs are monotonic. A retired ID can never be inserted again. */
  if (id <= p->last_id)
    return CD_INVALID_DATA;
  p->last_id = id;
  p->records[p->count++] = (CdPersonRecord){{p->token, id}, slot};
  return CD_OK;
}
CdStatus cd_people_remove(CdPeople *p, CdPersonRef ref) {
  const CdPersonRecord *record = cd_people_find(p, ref);
  if (!record)
    return CD_UNKNOWN_REFERENCE;
  size_t i = (size_t)(record - p->records);
  memmove(&p->records[i], &p->records[i + 1],
          (p->count - i - 1) * sizeof *p->records);
  p->count--;
  return CD_OK;
}
CdStatus cd_people_validate_order(CdPeople *p, const CdPersonId *ids,
                                  size_t count) {
  if (!p || (!ids && count) || count != p->count)
    return CD_INVALID_ARGUMENT;
  memset(p->seen, 0, p->capacity);
  for (size_t i = 0; i < count; i++) {
    const CdPersonRecord *r =
        cd_people_find(p, (CdPersonRef){p->token, ids[i]});
    if (!r)
      return CD_UNKNOWN_REFERENCE;
    size_t index = (size_t)(r - p->records);
    if (p->seen[index])
      return CD_INVALID_DATA;
    p->seen[index] = 1;
  }
  return CD_OK;
}
void cd_people_commit_order(CdPeople *p, const CdPersonId *ids, size_t count) {
  /* Serialized commit, only after validate_order succeeds for the same IDs. */
  for (size_t i = 0; i < count; i++) {
    CdPersonRecord *r =
        (CdPersonRecord *)cd_people_find(p, (CdPersonRef){p->token, ids[i]});
    r->native_slot = i;
  }
}
