#ifndef CD_OBSERVER_H
#define CD_OBSERVER_H
#include "cd_civic.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { CD_RECORD_LIMIT = 8192, CD_OPERATION_LIMIT = 128 };
typedef enum {
  CD_RECORD_PUBLIC,
  CD_RECORD_HOUSEHOLD,
  CD_RECORD_PERSONAL,
  CD_RECORD_DIAGNOSTIC
} CdRecordPrivacy;
typedef enum {
  CD_EVENT_WORLD,
  CD_EVENT_ADMISSION,
  CD_EVENT_BIRTH,
  CD_EVENT_RETIREMENT,
  CD_EVENT_HOUSEHOLD,
  CD_EVENT_JOB,
  CD_EVENT_WORK,
  CD_EVENT_SERVICE,
  CD_EVENT_DEVELOPMENT,
  CD_EVENT_INSTITUTION,
  CD_EVENT_GATHERING,
  CD_EVENT_TRANSPORT,
  CD_EVENT_PRESSURE,
  CD_EVENT_OBSERVATION,
  CD_EVENT_BEREAVEMENT,
  CD_EVENT_OPERATION,
  CD_EVENT_INVENTORY,
  CD_EVENT_KINDS
} CdEventKind;
typedef struct {
  uint64_t id, minute, cause;
  CdEventKind kind;
  CdRecordPrivacy privacy;
  CdPersonRef actor, subject;
  CdEntityId entity, household;
  char basis[96], message[160];
} CdRecord;
typedef struct {
  uint64_t first, next, dropped;
  size_t retained;
  int saturated;
} CdRecordSummary;
typedef struct {
  CdPersonRef viewer;
  CdRecordPrivacy access;
} CdRecordQuery;
typedef struct {
  uint64_t bereavement_event, support_until;
  int support_context_active;
} CdSocialView;
CdStatus cd_world_record_summary(const CdWorld *, CdRecordSummary *,
                                 CdDiagnostic *);
CdStatus cd_world_record(const CdWorld *, uint64_t, CdRecordQuery, CdRecord *,
                         CdDiagnostic *);
CdStatus cd_snapshot_record(const CdSnapshot *, uint64_t, CdRecordQuery,
                            CdRecord *, CdDiagnostic *);
CdStatus cd_world_observe(CdWorld *, CdPersonRef, uint64_t, const char *,
                          uint64_t *, CdDiagnostic *);
CdStatus cd_world_social(const CdWorld *, CdPersonRef, CdSocialView *,
                         CdDiagnostic *);
CdStatus cd_world_reference(const CdWorld *, CdPersonId, CdPersonRef *,
                            CdDiagnostic *);
CdStatus cd_world_export_records(const CdWorld *, CdRecordQuery, const char *,
                                 CdDiagnostic *);
typedef enum {
  CD_CMD_PLACE,
  CD_CMD_MOVE,
  CD_CMD_ENTER,
  CD_CMD_EXIT,
  CD_CMD_CLOSE_EDGE,
  CD_CMD_LOCK,
  CD_CMD_TRAIN,
  CD_CMD_AUTHORIZE,
  CD_CMD_EMPLOYER,
  CD_CMD_JOB_ASSIGN,
  CD_CMD_WORK,
  CD_CMD_WORK_CANCEL,
  CD_CMD_GOODS_IMPORT,
  CD_CMD_FOOD_IMPORT,
  CD_CMD_GOODS_ORDER,
  CD_CMD_MONEY_IMPORT,
  CD_CMD_PROJECT,
  CD_CMD_PROJECT_CANCEL,
  CD_CMD_DISPATCH,
  CD_CMD_SERVICE_CANCEL,
  CD_CMD_GATHERING,
  CD_CMD_ATTEND,
  CD_CMD_PRESSURE,
  CD_CMD_PRESSURE_CANCEL,
  CD_CMD_SHELTER,
  CD_CMD_SHELTER_ASSIGN,
  CD_CMD_INSTITUTION_LINK,
  CD_CMD_INSTITUTION_OPERATE,
  CD_CMD_JOB_CREATE,
  CD_CMD_JOB_UNASSIGN,
  CD_CMD_JOB_CLOSE,
  CD_CMD_ROSTER,
  CD_CMD_OFFICE,
  CD_CMD_QUALIFY,
  CD_CMD_MEMBERSHIP,
  CD_CMD_FOOD_STORE,
  CD_CMD_CLINIC,
  CD_CMD_WATER_IMPORT,
  CD_CMD_WEAR,
  CD_CMD_EMPLOYER_OPEN,
  CD_CMD_VEHICLE,
  CD_CMD_TRANSPORT,
  CD_CMD_VEHICLE_STOP,
  CD_CMD_UNLOAD,
  CD_CMD_CONDITION,
  CD_CMD_KINDS
} CdCommandKind;
typedef struct {
  uint64_t key;
  CdCommandKind kind;
  CdPersonRef actor, subject;
  CdEntityId target, secondary, auxiliary;
  uint64_t units, value, starts, duration, earliest, deadline;
  unsigned flags;
  char label[96], basis[96];
} CdCommand;
typedef enum {
  CD_PERMISSION_OBSERVER,
  CD_PERMISSION_INTERVENTION
} CdPermission;
typedef struct {
  uint64_t id, queued_record, result_record;
  CdCommand command;
  CdTaskState state;
  CdStatus result;
  uint64_t result_id;
  char reason[160];
} CdOperationView;
CdStatus cd_world_operation_preview(const CdWorld *, const CdCommand *,
                                    CdPermission, CdPreview *, CdDiagnostic *);
CdStatus cd_world_operation_queue(CdWorld *, const CdCommand *, CdPermission,
                                  uint64_t *, CdDiagnostic *);
CdStatus cd_world_operation(const CdWorld *, uint64_t, CdOperationView *,
                            CdDiagnostic *);
CdStatus cd_world_operation_cancel(CdWorld *, uint64_t, CdPermission,
                                   CdDiagnostic *);
CdStatus cd_world_operations_flush(CdWorld *, CdPermission, CdDiagnostic *);
CdStatus cd_command_parse(const CdWorld *, const char *, CdCommand *,
                          CdDiagnostic *);
#ifdef CD_TESTING
void cd_checkpoint_fail_publish(int);
#endif
const char *cd_command_name(CdCommandKind);
CdStatus cd_world_run_program(CdWorld *, const char *, CdPermission,
                              CdDiagnostic *);
/* Immutable worker-published map glyphs; logical ID is independent of
 * slot/token. */
typedef struct {
  CdPersonRef ref;
  CdPoint position;
  int located;
} CdAgentGlyph;
CdStatus cd_glyph_location(const CdAgentGlyph *, size_t, CdPersonId, CdPoint *,
                           CdDiagnostic *);
typedef struct {
  CdPoint source, viewport;
  double zoom;
  CdPoint pan;
} CdCameraPoint;
CdStatus cd_camera_transform(CdPoint, double, CdPoint, int, CdPoint *,
                             CdDiagnostic *);
CdStatus cd_world_pick_person(const CdWorld *, CdPoint, double, CdPersonRef *,
                              CdDiagnostic *);
#ifdef __cplusplus
}
#endif
#endif
