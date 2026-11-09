#include "cd_city_internal.h"
#include "cd_native.h"
#include <zlib.h>
/* This component checkpoint validates against the full world digest at the same
   native instant. It cannot roll back paid work or replace a native city save.
 */
typedef struct {
  unsigned char *p;
  size_t n, at;
  int bad;
} Stream;
static void put(Stream *s, uint64_t v) {
  if (s->at + 8 > s->n) {
    s->bad = 1;
    return;
  }
  for (unsigned i = 0; i < 8; i++)
    s->p[s->at++] = (unsigned char)(v >> (8 * i));
}
static uint64_t get(Stream *s) {
  if (s->at + 8 > s->n) {
    s->bad = 1;
    return 0;
  }
  uint64_t v = 0;
  for (unsigned i = 0; i < 8; i++)
    v |= (uint64_t)s->p[s->at++] << (8 * i);
  return v;
}
static void bytes(Stream *s, void *b, size_t n, int writing) {
  if (s->at + n > s->n) {
    s->bad = 1;
    return;
  }
  if (writing)
    memcpy(s->p + s->at, b, n);
  else
    memcpy(b, s->p + s->at, n);
  s->at += n;
}
static void row(Stream *s, CdGathering *g, int writing) {
#define X(field)                                                               \
  do {                                                                         \
    if (writing)                                                               \
      put(s, (uint64_t)(field));                                               \
    else                                                                       \
      (field) = get(s);                                                        \
  } while (0)
  CdGatheringView *v = &g->view;
  X(v->id);
  X(v->key);
  X(v->state);
  X(v->office);
  X(v->kind);
  X(v->starts);
  X(v->minutes);
  X(v->capacity);
  X(v->repeat_days);
  X(v->repeats);
  X(v->occurrence);
  X(v->progress);
  X(v->present);
  X(v->completed);
  X(v->members_only);
  X(v->paid);
  X(v->reserved_money);
  bytes(s, v->basis, 96, writing);
  bytes(s, v->reason, 96, writing);
  X(g->count);
  X(g->rate);
  for (unsigned k = 0; k < CD_ATTENDANCE_LIMIT; k++) {
    X(g->attendees[k].person_id);
    X(g->attendees[k].world_token);
    X(g->credited[k]);
  }
#undef X
}
CdStatus cd_world_calendar_save(const CdWorld *w, void **out, size_t *size,
                                CdDiagnostic *d) {
  if (!cd_model_ready(w) || !out || *out || !size)
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Empty calendar checkpoint output and closed world required");
  size_t n = 8 * 5 + 65 +
             w->gathering_count * (8 * 19 + 192 + 24 * CD_ATTENDANCE_LIMIT) + 8;
  unsigned char *p = cd_memory_calloc(n, 1, CD_MEM_CITY);
  if (!p)
    return cd_diag(d, CD_CAPACITY, "Calendar checkpoint allocation failed");
  Stream s = {p, n, 0, 0};
  uint64_t digest;
  cd_world_digest(w, &digest, NULL);
  put(&s, UINT64_C(0x314c41434443));
  put(&s, cd_native_minutes(w->native));
  put(&s, digest);
  put(&s, w->gathering_count);
  put(&s, sizeof(CdGathering));
  bytes(&s, w->content->generation, 65, 1);
  for (size_t i = 0; i < w->gathering_count; i++) {
    CdGathering g = w->gatherings[i];
    row(&s, &g, 1);
  }
  uint64_t crc = crc32(0, p, (uInt)s.at);
  put(&s, crc);
  if (s.bad || s.at != n) {
    cd_memory_free(p);
    return cd_diag(d, CD_INVALID_STATE, "Calendar codec bound mismatch");
  }
  *out = p;
  *size = n;
  return cd_diag(d, CD_OK,
                 "Pointer-free calendar component checkpoint; same-instant "
                 "city identity required");
}
CdStatus cd_world_calendar_restore(CdWorld *w, const void *data, size_t n,
                                   CdDiagnostic *d) {
  if (!cd_model_ready(w) || !data || n < 113 || n > 1024 * 1024)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Invalid calendar checkpoint bounds");
  Stream s = {(unsigned char *)data, n, 0, 0};
  uint64_t magic = get(&s), minute = get(&s), hash = get(&s), count = get(&s),
           abi = get(&s), current;
  char generation[65];
  bytes(&s, generation, 65, 0);
  cd_world_digest(w, &current, NULL);
  if (magic != UINT64_C(0x314c41434443) ||
      minute != cd_native_minutes(w->native) || hash != current ||
      count != w->gathering_count || abi != sizeof(CdGathering) ||
      memcmp(generation, w->content->generation, 65) ||
      n != 8 * 5 + 65 + count * (8 * 19 + 192 + 24 * CD_ATTENDANCE_LIMIT) + 8)
    return cd_diag(
        d, CD_INVALID_STATE,
        "Calendar checkpoint differs from this native instant/content/city; "
        "use coordinated city save in Cycle 31");
  CdGathering *staged =
      cd_memory_calloc(CD_CIVIC_LIMIT, sizeof *staged, CD_MEM_CITY);
  if (!staged)
    return cd_diag(d, CD_CAPACITY, "Calendar restore staging failed");
  int valid = 1;
  for (size_t i = 0; i < count; i++) {
    row(&s, &staged[i], 0);
    CdGathering *g = &staged[i];
    valid &= g->view.id == i + 1 && g->count <= CD_ATTENDANCE_LIMIT &&
             g->view.capacity <= CD_ATTENDANCE_LIMIT && g->view.office &&
             g->view.office <= w->office_count &&
             memchr(g->view.basis, 0, 96) && memchr(g->view.reason, 0, 96);
  }
  uint64_t crc = crc32(0, data, (uInt)s.at), expected = get(&s);
  if (!valid || s.bad || s.at != n || crc != expected) {
    cd_memory_free(staged);
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Corrupt calendar checkpoint; live state retained");
  } /* Compare the canonical encoding to the current component as well: a forged
CRC cannot authorize a rollback while retaining the original city digest header.
*/
  void *canonical = NULL;
  size_t length = 0;
  CdStatus result = cd_world_calendar_save(w, &canonical, &length, d);
  if (result == CD_OK && (length != n || memcmp(canonical, data, n)))
    result =
        cd_diag(d, CD_INVALID_ARGUMENT,
                "Calendar component does not match coordinated city identity");
  cd_memory_free(canonical);
  if (result == CD_OK)
    memcpy(w->gatherings, staged, count * sizeof *staged);
  cd_memory_free(staged);
  return result;
}
void cd_calendar_free(void *p) { cd_memory_free(p); }
