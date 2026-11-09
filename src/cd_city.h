#ifndef CD_CITY_H
#define CD_CITY_H
#include "cd_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Ordinary geometry flags; none encode religion or game mythology. */
enum { CD_BUILDING = 1, CD_WATER = 2, CD_RAIL = 3 };
typedef struct {
  int resident, employee, has_key, emergency_authorized;
  unsigned minute_of_day;
} CdAccess;
typedef struct {
  CdEntityId id;
  unsigned kind, capacity;
  const char *source_id, *name, *suggested_use, *policy_basis;
  double confidence;
  int capacity_known, opening_known, locked;
  unsigned opens, closes;
} CdBuildingView;
typedef enum {
  CD_ROUTE_IDLE,
  CD_ROUTE_TRAVELLING,
  CD_ROUTE_ARRIVED,
  CD_ROUTE_WAITING,
  CD_ROUTE_CANCELLED
} CdTravelState;
typedef enum {
  CD_ROUTE_OK,
  CD_ROUTE_UNKNOWN_NODE,
  CD_ROUTE_UNQUALIFIED,
  CD_ROUTE_DISCONNECTED,
  CD_ROUTE_CLOSED,
  CD_ROUTE_CROWDED,
  CD_ROUTE_GEOMETRY,
  CD_ROUTE_LIMIT,
  CD_ROUTE_UNPLACED,
  CD_ROUTE_ACCESS
} CdRouteReason;
enum { CD_ROUTE_EDGE_LIMIT = 128 };
typedef struct {
  CdEntityId edges[CD_ROUTE_EDGE_LIMIT];
  size_t count;
  double cost;
  CdRouteReason reason;
} CdRoute;
typedef struct {
  CdTravelState state;
  CdRouteReason reason;
  CdEntityId destination, edge;
  int position_known;
  CdPoint position;
} CdTravelView;
typedef enum {
  CD_RESIDENT,
  CD_VISITOR,
  CD_TROOP,
  CD_EVACUEE
} CdResidentCategory;
typedef struct {
  CdPersonRef ref;
  char name[128];
  CdCivilTime birth;
  unsigned age;
  CdResidentCategory category;
  int synthetic, affiliation_known, position_known, housing_known;
  char admission_basis[96];
  CdPoint position;
  CdEntityId household, dwelling;
  CdPersonRef spouse, guardian;
  CdPersonRef parents[2];
  int energy, sleeping, health_condition;
  unsigned fatigue, carried_meals, health_severity;
  CdHumanActivity activity;
  CdEntityId jobs[2], occupation, workplace;
  unsigned work_state, work_progress, work_duration;
  uint64_t wallet;
  unsigned credentials;
  CdEntityId institution_memberships[4], vehicle, driving, refuge;
  uint64_t service_request, gathering;
} CdResidentView;
typedef struct {
  size_t people, households, housed, unhoused, located, visitors,
      housing_unknown;
  uint64_t native_minutes;
  size_t capacity;
  uint64_t admissions, births, deaths, departures, birthdays;
  size_t asleep, hungry, ill, receiving_care;
} CdCitySummary;
typedef struct {
  CdEntityId id, dwelling;
  size_t members;
  int unhoused, housing_known;
} CdHouseholdView;
size_t cd_content_building_count(const CdContent *);
CdStatus cd_content_building(const CdContent *, size_t, CdBuildingView *,
                             CdDiagnostic *);
CdStatus cd_content_pick(const CdContent *, CdPoint, CdEntityId *,
                         CdDiagnostic *);
CdStatus cd_content_segment(const CdContent *, CdPoint, CdPoint, int bridge,
                            int rail_crossing, CdDiagnostic *);
CdStatus cd_content_access(const CdContent *, CdEntityId, const CdAccess *,
                           CdDiagnostic *);
CdStatus cd_content_connector(const CdContent *, CdEntityId, int *reviewed,
                              CdDiagnostic *);
CdStatus cd_content_node(const CdContent *, size_t, CdEntityId *, CdPoint *,
                         CdDiagnostic *);
size_t cd_content_node_count(const CdContent *);
CdStatus cd_world_summary(const CdWorld *, CdCitySummary *, CdDiagnostic *);
CdStatus cd_world_resident(const CdWorld *, size_t, CdResidentView *,
                           CdDiagnostic *);
CdStatus cd_world_resolve(const CdWorld *, CdPersonRef, CdResidentView *,
                          CdDiagnostic *);
/* Placement is initial admission only, never a move/teleport for a placed body.
 */
CdStatus cd_world_place(CdWorld *, CdPersonRef, CdEntityId node,
                        CdDiagnostic *);
CdStatus cd_world_plan(CdWorld *, CdEntityId from, CdEntityId to, CdRoute *,
                       CdDiagnostic *);
CdStatus cd_world_travel(CdWorld *, CdPersonRef, CdEntityId destination,
                         CdDiagnostic *);
CdStatus cd_world_travel_view(const CdWorld *, CdPersonRef, CdTravelView *,
                              CdDiagnostic *);
CdStatus cd_world_stop(CdWorld *, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_close_edge(CdWorld *, CdEntityId, int closed, CdDiagnostic *);
CdStatus cd_world_set_crowd(CdWorld *, CdEntityId, size_t load, CdDiagnostic *);
/* Door entry/exit follows a reviewed connector and the same native minute
 * budget. */
CdStatus cd_world_enter(CdWorld *, CdPersonRef, CdEntityId door, int has_key,
                        int emergency, CdDiagnostic *);
CdStatus cd_world_exit(CdWorld *, CdPersonRef, CdEntityId door, int has_key,
                       int emergency, CdDiagnostic *);
CdStatus cd_world_lock(CdWorld *, CdEntityId building, int locked,
                       CdDiagnostic *);
CdStatus cd_world_category(CdWorld *, CdPersonRef, CdResidentCategory,
                           const char *explicit_basis, CdDiagnostic *);
CdStatus cd_world_household(const CdWorld *, size_t, CdHouseholdView *,
                            CdDiagnostic *);
CdStatus cd_world_household_move(CdWorld *, CdEntityId household,
                                 CdEntityId dwelling, CdDiagnostic *);
CdStatus cd_world_transfer(CdWorld *, CdPersonRef, CdEntityId household,
                           CdPersonRef guardian, CdDiagnostic *);
CdStatus cd_world_partner(CdWorld *, CdPersonRef, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_guardian(CdWorld *, CdPersonRef child, CdPersonRef adult,
                           CdDiagnostic *);
CdStatus cd_world_visit(CdWorld *, CdPersonRef, CdEntityId dwelling,
                        int present, CdDiagnostic *);
CdStatus cd_world_owner(CdWorld *, CdEntityId dwelling, CdPersonRef,
                        CdDiagnostic *);
const char *cd_route_reason(CdRouteReason);
#ifdef __cplusplus
}
#endif
#endif
