#ifndef CD_CIVIC_H
#define CD_CIVIC_H
#include "cd_work.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  CD_CIVIC_LIMIT = 256,
  CD_DISPATCH_LIMIT = 512,
  CD_ATTENDANCE_LIMIT = 64,
  CD_VEHICLE_LIMIT = 64
};
typedef enum {
  CD_OFFICE_PRIEST,
  CD_OFFICE_MINISTER,
  CD_OFFICE_RABBI,
  CD_OFFICE_CUSTODIAN,
  CD_OFFICE_SEXTON,
  CD_OFFICE_STAFF,
  CD_OFFICE_TEACHER,
  CD_OFFICE_ADMINISTRATOR,
  CD_OFFICE_CHARITY,
  CD_OFFICE_KINDS
} CdOfficeKind;
enum {
  CD_AUTH_TEACH = 1,
  CD_AUTH_ADMIN = 2,
  CD_AUTH_POLICE = 4,
  CD_AUTH_FIRE = 8,
  CD_AUTH_CARRIER = 16,
  CD_AUTH_BURIAL = 32
};
typedef enum {
  CD_SERVICE_WORSHIP,
  CD_SERVICE_EDUCATION,
  CD_SERVICE_CHARITY,
  CD_SERVICE_MEAL,
  CD_SERVICE_VISIT,
  CD_SERVICE_BURIAL,
  CD_SERVICE_HEALTH,
  CD_SERVICE_ADMIN,
  CD_SERVICE_POLICE,
  CD_SERVICE_FIRE,
  CD_SERVICE_LIBRARY,
  CD_SERVICE_WATER,
  CD_SERVICE_CLEANING,
  CD_SERVICE_PHARMACY,
  CD_SERVICE_MIDWIFERY,
  CD_SERVICE_KINDS
} CdServiceKind;
typedef struct {
  CdEntityId institution, job;
  CdOfficeKind kind;
  const char *basis;
} CdOfficeSpec;
typedef struct {
  CdEntityId id, institution, job;
  CdOfficeKind kind;
  CdPersonRef holder, last_holder;
  uint64_t successions, outputs;
  unsigned capabilities;
  int available;
  char basis[96];
} CdOfficeView;
typedef struct {
  CdEntityId job;
  unsigned capabilities;
  const char *title, *basis;
} CdRosterSpec;
typedef struct {
  CdEntityId id, job;
  unsigned capabilities;
  uint64_t outputs;
  char title[96], basis[96];
} CdRosterView;
typedef struct {
  CdEntityId provider;
  int religious;
  CdServiceKind kind;
  CdEntityId building;
  CdPersonRef subject;
  uint64_t deadline;
  unsigned minutes;
  const char *basis;
} CdServiceSpec;
typedef struct {
  uint64_t id, key;
  CdTaskState state;
  CdEntityId provider, building;
  CdPersonRef worker, subject;
  CdServiceKind kind;
  int religious;
  unsigned minutes, progress;
  uint64_t paid, reserved_money;
  char basis[96], reason[96];
} CdServiceView;
typedef struct {
  CdEntityId office;
  CdServiceKind kind;
  uint64_t starts;
  unsigned minutes, capacity, repeat_days, repeats;
  int members_only;
  const char *basis;
} CdGatheringSpec;
typedef struct {
  uint64_t id, key;
  CdTaskState state;
  CdEntityId office;
  CdServiceKind kind;
  uint64_t starts;
  unsigned minutes, capacity, repeat_days, repeats, occurrence, progress,
      present, completed;
  int members_only;
  uint64_t paid, reserved_money;
  char basis[96], reason[96];
} CdGatheringView;
typedef enum {
  CD_HANDCART, CD_STRETCHER, CD_RAIL_VEHICLE, CD_ROAD_FREIGHT, CD_ROAD_PASSENGER
} CdVehicleKind;
typedef enum {
  CD_VEHICLE_IDLE,
  CD_VEHICLE_LOADING,
  CD_VEHICLE_MOVING,
  CD_VEHICLE_UNLOADING,
  CD_VEHICLE_DONE,
  CD_VEHICLE_STOPPED
} CdVehiclePhase;
typedef struct {
  CdEntityId job;
  CdVehicleKind kind;
  unsigned cargo_capacity;
  const char *basis;
} CdVehicleSpec;
typedef struct {
  CdEntityId id, job, source, destination;
  CdPersonRef crew, passenger;
  CdVehicleKind kind;
  CdVehiclePhase phase;
  unsigned capacity, good, requested, loaded, delivered;
  int meals, stopped, position_known;
  CdPoint position;
  uint64_t key;
  char basis[96], reason[96];
} CdVehicleView;
typedef enum {
  CD_PRESSURE_RAIN,
  CD_PRESSURE_FIRE,
  CD_PRESSURE_DAMAGE,
  CD_PRESSURE_EDGE_CLOSURE,
  CD_PRESSURE_ALERT,
  CD_PRESSURE_BLACKOUT,
  CD_PRESSURE_RATION,
  CD_PRESSURE_KINDS
} CdPressureKind;
typedef struct {
  CdPressureKind kind;
  CdEntityId target;
  uint64_t starts;
  unsigned minutes, value;
  const char *basis;
} CdPressureSpec;
typedef struct {
  uint64_t id, key, starts;
  CdPressureKind kind;
  CdEntityId target;
  CdTaskState state;
  unsigned minutes, value;
  int applied;
  char basis[96];
} CdPressureView;
typedef struct {
  unsigned rain, alert, blackout, ration_limit;
  uint64_t water_imported, water_stock, water_used, water_exported,
      fire_minutes, damage;
  size_t offices, gatherings, rosters, requests, vehicles, pressures;
  uint64_t service_outputs, attendance_minutes;
} CdCivicSummary;
CdStatus cd_world_authorize(CdWorld *, CdPersonRef, unsigned, const char *,
                            CdDiagnostic *);
CdStatus cd_world_religious_qualification(CdWorld *, CdPersonRef, CdEntityId,
                                          CdOfficeKind, const char *,
                                          CdDiagnostic *);
CdStatus cd_world_office_create(CdWorld *, const CdOfficeSpec *, CdEntityId *,
                                CdDiagnostic *);
CdStatus cd_world_office(const CdWorld *, CdEntityId, CdOfficeView *,
                         CdDiagnostic *);
CdStatus cd_world_membership(CdWorld *, CdPersonRef, CdEntityId, int,
                             const char *, CdDiagnostic *);
CdStatus cd_world_member(const CdWorld *, CdPersonRef, CdEntityId, int *,
                         CdDiagnostic *);
CdStatus cd_world_gathering(CdWorld *, uint64_t, const CdGatheringSpec *,
                            uint64_t *, CdDiagnostic *);
CdStatus cd_world_gathering_view(const CdWorld *, uint64_t, CdGatheringView *,
                                 CdDiagnostic *);
CdStatus cd_world_attend(CdWorld *, uint64_t, CdPersonRef, int, CdDiagnostic *);
CdStatus cd_world_gathering_cancel(CdWorld *, uint64_t, CdDiagnostic *);
CdStatus cd_world_roster(CdWorld *, const CdRosterSpec *, CdEntityId *,
                         CdDiagnostic *);
CdStatus cd_world_roster_view(const CdWorld *, CdEntityId, CdRosterView *,
                              CdDiagnostic *);
CdStatus cd_world_dispatch(CdWorld *, uint64_t, const CdServiceSpec *,
                           uint64_t *, CdDiagnostic *);
CdStatus cd_world_service(const CdWorld *, uint64_t, CdServiceView *,
                          CdDiagnostic *);
CdStatus cd_world_service_cancel(CdWorld *, uint64_t, CdDiagnostic *);
CdStatus cd_world_vehicle(CdWorld *, const CdVehicleSpec *, CdEntityId *,
                          CdDiagnostic *);
CdStatus cd_world_transport(CdWorld *, CdEntityId, uint64_t, CdEntityId,
                            CdEntityId, unsigned, int, unsigned, CdPersonRef,
                            CdDiagnostic *);
CdStatus cd_world_dsl_transport_dispatch(CdWorld *, CdPersonRef, CdEntityId, uint64_t,
                                           CdEntityId, CdEntityId, unsigned, int,
                                           unsigned, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_dsl_public_roster(CdWorld *, const CdRosterSpec *, CdEntityId *, CdDiagnostic *);
CdStatus cd_world_dsl_station_roster(CdWorld *, const CdRosterSpec *, CdEntityId *, CdDiagnostic *);
CdStatus cd_world_dsl_hotel_visit(CdWorld *, CdEntityId, CdPersonRef, int, CdDiagnostic *);
CdStatus cd_world_dsl_hospitality_roster(CdWorld *, const CdRosterSpec *,
                                          CdEntityId *, CdDiagnostic *);
CdStatus cd_world_vehicle_view(const CdWorld *, CdEntityId, CdVehicleView *,
                               CdDiagnostic *);
CdStatus cd_world_vehicle_crew(CdWorld *, CdEntityId, CdPersonRef,
                               CdDiagnostic *);
CdStatus cd_world_vehicle_stop(CdWorld *, CdEntityId, int, CdDiagnostic *);
CdStatus cd_world_vehicle_unload_here(CdWorld *, CdEntityId, CdDiagnostic *);
CdStatus cd_world_pressure(CdWorld *, uint64_t, const CdPressureSpec *,
                           uint64_t *, CdDiagnostic *);
CdStatus cd_world_pressure_view(const CdWorld *, uint64_t, CdPressureView *,
                                CdDiagnostic *);
CdStatus cd_world_pressure_cancel(CdWorld *, uint64_t, CdDiagnostic *);
CdStatus cd_world_water_import(CdWorld *, CdEntityId, unsigned, const char *,
                               CdDiagnostic *);
CdStatus cd_world_shelter(CdWorld *, CdEntityId, unsigned, const char *,
                          CdDiagnostic *);
CdStatus cd_world_shelter_assign(CdWorld *, CdPersonRef, CdEntityId,
                                 const char *, CdDiagnostic *);
CdStatus cd_snapshot_civic_summary(const CdSnapshot *, CdCivicSummary *,
                                   CdDiagnostic *);
CdStatus cd_world_civic_summary(const CdWorld *, CdCivicSummary *,
                                CdDiagnostic *);
/* Calendar-only component checkpoint, bound to the same native instant/content.
   Use cd_world_save/load for complete atomic city checkpoints. Caller owns
   returned memory. */
CdStatus cd_world_calendar_save(const CdWorld *, void **, size_t *,
                                CdDiagnostic *);
CdStatus cd_world_calendar_restore(CdWorld *, const void *, size_t,
                                   CdDiagnostic *);
void cd_calendar_free(void *);
#ifdef __cplusplus
}
#endif
#endif
