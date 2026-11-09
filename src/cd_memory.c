#include "cd_memory.h"
#include <stdlib.h>
#include <stddef.h>
typedef union {
  max_align_t alignment;
  struct {
    size_t bytes;
    CdMemoryCategory category;
  } info;
} Header;
static CdMemoryStats stats;
#ifdef CD_TESTING
static size_t fail_at, attempt;
void cd_memory_fail_at(size_t n) {
  fail_at = n;
  attempt = 0;
}
#endif
void *cd_memory_calloc(size_t count, size_t size, CdMemoryCategory category) {
  stats.attempts++;
#ifdef CD_TESTING
  if (fail_at && ++attempt == fail_at)
    return NULL;
#endif
  if ((unsigned)category >= CD_MEM_CATEGORIES || !size || !count ||
      count > SIZE_MAX / size)
    return NULL;
  size_t bytes = count * size;
  if (bytes > SIZE_MAX - sizeof(Header) || bytes > CD_MEMORY_LIMIT ||
      stats.total > CD_MEMORY_LIMIT - bytes)
    return NULL;
  Header *h = calloc(1, sizeof(Header) + bytes);
  if (!h)
    return NULL;
  h->info.bytes = bytes;
  h->info.category = category;
  stats.bytes[category] += bytes;
  stats.total += bytes;
  stats.blocks++;
  if (stats.bytes[category] > stats.peak[category])
    stats.peak[category] = stats.bytes[category];
  if (stats.total > stats.peak_total)
    stats.peak_total = stats.total;
  return h + 1;
}
void cd_memory_free(void *p) {
  if (p) {
    Header *h = (Header *)p - 1;
    stats.bytes[h->info.category] -= h->info.bytes;
    stats.total -= h->info.bytes;
    stats.blocks--;
    free(h);
  }
}
CdMemoryStats cd_memory_stats(void) { return stats; }
