#ifndef CD_OBSERVER_INTERNAL_H
#define CD_OBSERVER_INTERNAL_H
#include "cd_observer.h"
typedef struct {
  uint64_t fingerprint, event;
} CdRecordCursor;
typedef struct {
  CdOperationView view;
} CdOperation;
#endif
