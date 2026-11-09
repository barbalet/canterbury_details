#ifndef CD_MEMORY_H
#define CD_MEMORY_H
#include <stddef.h>
#include <stdint.h>
typedef enum {
  CD_MEM_NATIVE,
  CD_MEM_ACTORS,
  CD_MEM_CITY,
  CD_MEM_TEMP,
  CD_MEM_SDK_BUFFER,
  CD_MEM_CATEGORIES
} CdMemoryCategory;
typedef struct {
  size_t bytes[CD_MEM_CATEGORIES], peak[CD_MEM_CATEGORIES], blocks, total,
      peak_total, attempts;
} CdMemoryStats;
#define CD_MEMORY_LIMIT UINT64_C(5368709120)
void *cd_memory_calloc(size_t count, size_t size, CdMemoryCategory);
void cd_memory_free(void *);
CdMemoryStats cd_memory_stats(void);
#ifdef CD_TESTING
/* Fail exactly the Nth subsequent allocation; zero disables injection. */
void cd_memory_fail_at(size_t);
#endif
#endif
