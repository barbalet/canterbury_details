#ifndef CANTERBURY_DETAILS_BOOTSTRAP_H
#define CANTERBURY_DETAILS_BOOTSTRAP_H
#include "cd_api.h"
/* Native host lifecycle; CdWorld/content modules remain separate future APIs.
 */
typedef struct CdSmokeSession CdSmokeSession;
typedef struct CdSmokeStage CdSmokeStage;
/* Prepare requires *out == NULL and owns only unpublished resources.
   Failed prepare/open never overwrite an existing active owner or stage. */
CdStatus cd_smoke_prepare(size_t people, uint32_t seed, CdSmokeStage **,
                          CdDiagnostic *);
void cd_smoke_discard(CdSmokeStage *);
CdStatus cd_smoke_publish(CdSmokeStage **, CdSmokeSession **current,
                          CdDiagnostic *);
typedef enum {
  CD_RNG_ADMISSION,
  CD_RNG_ROUTINES,
  CD_RNG_SERVICES
} CdRandomStream;
CdStatus cd_smoke_random(CdSmokeSession *, CdRandomStream, uint64_t *,
                         CdDiagnostic *);
CdStatus cd_smoke_admit_next(CdSmokeSession *, CdPersonRef *, CdDiagnostic *);
typedef struct {
  size_t people, native_people, actor_records;
  uint64_t native_minutes, callbacks, digest;
  int bindings_valid;
} CdSmokeReport;
/* Derived Gregorian scenario time: native elapsed zero = 1940-01-01 00:00.
   January 1 is an explicit scenario convention, not a sourced event date. */
CdStatus cd_smoke_civil_time(uint64_t elapsed_minutes, CdCivilTime *,
                             CdDiagnostic *);
CdStatus cd_smoke_time(const CdSmokeSession *, CdCivilTime *, CdDiagnostic *);
size_t cd_smoke_default_people(void);
size_t cd_smoke_capacity(void);
CdStatus cd_smoke_open(size_t people, uint32_t seed, CdSmokeSession **out,
                       CdDiagnostic *);
CdStatus cd_smoke_step(CdSmokeSession *, unsigned minutes, CdDiagnostic *);
CdStatus cd_smoke_read(const CdSmokeSession *, CdSmokeReport *, CdDiagnostic *);
/* Value references remain safe to retain; resolve only within their world. */
CdStatus cd_smoke_person(const CdSmokeSession *, size_t native_slot,
                         CdPersonRef *, CdDiagnostic *);
CdStatus cd_smoke_resolve(const CdSmokeSession *, CdPersonRef,
                          size_t *native_slot, CdDiagnostic *);
CdStatus cd_smoke_admit(CdSmokeSession *, uint32_t fixture_seed, CdPersonRef *,
                        CdDiagnostic *);
CdStatus cd_smoke_retire(CdSmokeSession *, CdPersonRef, CdDiagnostic *);
CdStatus cd_smoke_reorder(CdSmokeSession *, const CdPersonRef *, size_t count,
                          CdDiagnostic *);
CdStatus cd_smoke_rebind(CdSmokeSession *, CdDiagnostic *);
void cd_smoke_close(CdSmokeSession *);
#endif
