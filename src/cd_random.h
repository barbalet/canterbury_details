#ifndef CD_RANDOM_H
#define CD_RANDOM_H
#include <stdint.h>
enum { CD_RANDOM_STREAMS = 3 };
typedef struct {
  uint64_t state[CD_RANDOM_STREAMS];
} CdRandom;
void cd_random_seed(CdRandom *, uint64_t seed);
uint64_t cd_random_next(CdRandom *, unsigned stream);
#endif
