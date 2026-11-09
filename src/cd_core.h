#ifndef CD_CORE_H
#define CD_CORE_H
#include "cd_management.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CD_CORE_INTERFACE_VERSION 1
const char *cd_core_version(void);
/* Pure extension-domain RNG. Consumers own counters/checkpoint coordination.
 * Nonzero domain keys never consume the three authoritative city RNG streams.
 */
uint64_t cd_extension_random(uint64_t seed, uint64_t domain, uint64_t counter);
#ifdef __cplusplus
}
#endif
#endif
