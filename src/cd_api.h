#ifndef CANTERBURY_DETAILS_API_H
#define CANTERBURY_DETAILS_API_H
/* Neutral public contract. Content/world/snapshot and MOVE/node are implemented
 * through Cycle 13; broader operations and persistence remain later cycles.
 * No native layout, persistent pointer or serialized ABI is exposed here. */
#include <stddef.h>
#include <stdint.h>
#if !defined(CANTERBURY_SIM) || CANTERBURY_SIM != 1
#error Canterbury Details requires CANTERBURY_SIM=1
#endif
#if !defined(BRONZE_SIM) || BRONZE_SIM != 1
#error The current Canterbury Details SDK bridge requires BRONZE_SIM=1
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct CdWorld CdWorld;
typedef struct CdContent CdContent;
typedef struct CdSnapshot CdSnapshot;
typedef uint64_t CdPersonId;
typedef uint64_t CdEntityId;
typedef uint64_t CdRequestId;
/* Runtime namespace token changes on replacement/load; never a native pointer.
 */
typedef struct {
  uint64_t world_token;
  CdPersonId person_id;
} CdPersonRef;
typedef struct {
  double x, y;
} CdPoint;
typedef struct {
  int year;
  unsigned month, day, minute_of_day;
} CdCivilTime;
typedef struct {
  uint64_t native_minutes;
  CdCivilTime civil;
} CdClockView;
typedef enum {
  CD_OK,
  CD_INVALID_ARGUMENT,
  CD_INVALID_DATA,
  CD_UNKNOWN_REFERENCE,
  CD_CAPACITY,
  CD_INVALID_STATE,
  CD_IO_ERROR,
  CD_UNSUPPORTED
} CdStatus;
typedef struct {
  CdStatus status;
  char message[256];
} CdDiagnostic;
typedef struct {
  size_t initial_people;
  uint64_t seed;
  CdCivilTime civil_origin;
  const char *policy_id;
} CdScenario;
typedef enum {
  CD_ACTIVITY_IDLE,
  CD_ACTIVITY_TRAVEL,
  CD_ACTIVITY_WORK,
  CD_ACTIVITY_MEAL,
  CD_ACTIVITY_REST,
  CD_ACTIVITY_CARE,
  CD_ACTIVITY_WAIT,
  CD_ACTIVITY_RETIRED
} CdHumanActivity;
/* View strings are borrowed from a snapshot, never pointers into a native body.
 */
typedef struct {
  CdPersonRef ref;
  const char *display_name;
  CdEntityId household_id, occupation_id, workplace_id;
  CdPoint position;
  unsigned age;
  CdHumanActivity activity;
  int living;
  int position_known; /* Unplaced residents have no asserted city-map location.
                       */
} CdPersonView;
typedef struct {
  CdEntityId institution_id, building_id;
  const char *name, *religion, *denomination, *source_status;
  int location_known, operation_known;
} CdInstitutionView;
typedef enum {
  CD_OPERATION_MOVE,
  CD_OPERATION_ASSIGN_JOB,
  CD_OPERATION_DELIVER,
  CD_OPERATION_REPAIR,
  CD_OPERATION_DISPATCH_SERVICE,
  CD_OPERATION_SET_CLOSURE
} CdOperationKind;
typedef enum {
  CD_TARGET_NONE,
  CD_TARGET_PERSON,
  CD_TARGET_BUILDING,
  CD_TARGET_INSTITUTION,
  CD_TARGET_ROAD,
  CD_TARGET_JOB,
  CD_TARGET_STORE,
  CD_TARGET_NODE,
  CD_TARGET_DOOR
} CdTargetKind;
typedef struct {
  CdTargetKind kind;
  CdEntityId entity_id;
  /* Required for person targets; must match the owning world token. */
  CdPersonRef person;
} CdTarget;
typedef struct {
  CdOperationKind kind;
  CdPersonRef actor;
  CdTarget target;
  CdEntityId resource_id;
  uint64_t quantity, earliest_native_minute, deadline_native_minute;
} CdRequest;
typedef struct {
  CdStatus status;
  unsigned estimated_minutes;
  uint64_t estimated_quantity;
} CdPreview;

/* Content is immutable after admission and must outlive its worlds/snapshots.
 */
CdStatus cd_content_open(const char *manifest_path, CdContent **out,
                         CdDiagnostic *);
void cd_content_close(CdContent *);
/* Open publishes only a complete world. Capacity comes from the SDK profile. */
CdStatus cd_world_open(const CdContent *, const CdScenario *, CdWorld **out,
                       CdDiagnostic *);
void cd_world_close(CdWorld *);
/* Each requested minute delegates to sim_cycle; no independent ticking clock.
 */
CdStatus cd_world_step(CdWorld *, unsigned minutes, CdDiagnostic *);
CdStatus cd_world_clock(const CdWorld *, CdClockView *, CdDiagnostic *);
CdStatus cd_world_preview(const CdWorld *, const CdRequest *, CdPreview *,
                          CdDiagnostic *);
CdStatus cd_world_submit(CdWorld *, const CdRequest *, CdRequestId *,
                         CdDiagnostic *);
CdStatus cd_world_cancel(CdWorld *, CdRequestId, CdDiagnostic *);
/* Snapshot/inspection/digest are read-only and consume no simulation RNG. */
CdStatus cd_snapshot_capture(const CdWorld *, CdSnapshot **out, CdDiagnostic *);
void cd_snapshot_close(CdSnapshot *);
size_t cd_snapshot_person_count(const CdSnapshot *);
CdStatus cd_snapshot_person(const CdSnapshot *, size_t index, CdPersonView *,
                            CdDiagnostic *);
CdStatus cd_snapshot_institution(const CdSnapshot *, CdEntityId,
                                 CdInstitutionView *, CdDiagnostic *);
CdStatus cd_world_digest(const CdWorld *, uint64_t *out, CdDiagnostic *);
CdStatus cd_world_save(const CdWorld *, const char *path, CdDiagnostic *);
/* Replaces only on successful validation; failed load preserves *world. */
CdStatus cd_world_load(CdWorld **world, const CdContent *, const char *path,
                       CdDiagnostic *);

#ifdef __cplusplus
}
#endif
#endif
