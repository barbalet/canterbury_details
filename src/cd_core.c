#include "cd_core.h"
const char *cd_core_version(void) { return "CanterburyCore-1"; }
uint64_t cd_extension_random(uint64_t seed, uint64_t domain, uint64_t counter) {
  uint64_t x = seed ^ (domain * UINT64_C(0x9e3779b97f4a7c15)) ^
               (counter * UINT64_C(0xd1b54a32d192ed03));
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}
