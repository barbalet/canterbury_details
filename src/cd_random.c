#include "cd_random.h"
static uint64_t mix(uint64_t z) {
  z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
  return z ^ (z >> 31);
}
void cd_random_seed(CdRandom *r, uint64_t seed) {
  for (unsigned i = 0; i < CD_RANDOM_STREAMS; i++)
    r->state[i] = mix(seed + UINT64_C(0x9e3779b97f4a7c15) * (i + 1));
}
uint64_t cd_random_next(CdRandom *r, unsigned stream) {
  r->state[stream] += UINT64_C(0x9e3779b97f4a7c15);
  return mix(r->state[stream]);
}
