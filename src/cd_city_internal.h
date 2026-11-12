#ifndef CD_CITY_INTERNAL_H
#define CD_CITY_INTERNAL_H
#include "cd_bootstrap.h"
#include "cd_city.h"
#include "cd_dsl_catalogue.h"
#include "cd_human.h"
#include "cd_work.h"
#include "cd_civic_internal.h"
#include "cd_observer_internal.h"
#include "cd_memory.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CD_NONE UINT32_MAX
#define CD_EPS 1e-8
#define CD_SECTOR_JOB_BASE UINT64_C(65536)
typedef struct {
  uint32_t offset, count;
} CdRing;
typedef struct {
  CdEntityId id;
  uint32_t kind, ring, count, capacity, capacity_known, opening_known, opens,
      closes, locked;
  char *source, *name, *use, *policy_basis;
  double confidence;
  double minx, miny, maxx, maxy;
} CdPolygon;
typedef struct {
  CdEntityId id;
  CdPoint p;
  char *source;
} CdNode;
typedef struct {
  CdEntityId id;
  uint32_t from, to, point, count, qualified, bridge, rail_crossing, capacity;
  int grade;
  uint32_t junction;
  char *source, *basis;
  double length;
} CdEdge;
typedef struct {
  CdEntityId id;
  uint32_t building, node, point, count, reviewed;
  char *basis;
} CdDoor;
typedef struct {
  CdEntityId id, building;
  CdPoint p;
  uint32_t located;
  char *source, *name, *religion, *denomination, *status, *notes;
  char *site_type, *source_record, *location_basis;
  unsigned setting_year, identity_verified, review_required, confidence_known;
  double confidence;
} CdSite;
struct CdContent {
  uint32_t refs, width, height, np, nr, nv, nn, ne, nd, ns;
  char generation[65];
  unsigned char *file;
  size_t file_bytes;
  CdPolygon *polygons;
  CdRing *rings;
  CdPoint *points;
  CdNode *nodes;
  CdEdge *edges;
  CdDoor *doors;
  CdSite *sites;
  uint32_t *adj_offset, *adj_edges, *components;
  char *basis;
  unsigned walk_units_per_minute;
};
typedef struct {
  CdWorkView view;
  CdWorkSpec spec;
  char basis[96];
  uint64_t inputs[CD_GOODS], raw, rate, mentor_rate;
  CdPersonRef mentor;
} CdWorkTask;
typedef struct {
  CdTradeView view;
  uint64_t cost;
  CdEntityId payer;
} CdTradeTask;
typedef struct {
  CdEntityId building, household;
  unsigned good, remaining;
  int delivery;
} CdCargo;
typedef struct {
  CdDevelopmentView view;
  uint64_t materials[CD_GOODS];
} CdProject;
typedef struct {
  CdEntityId premises[8];
  size_t count;
  int known, operating, override;
  char office[96], basis[96], link_basis[8][96];
} CdInstitutionRuntime;
typedef struct {
  CdJobView view;
  int has_key;
  unsigned commute_start;
} CdJob;
typedef struct {
  CdEntityId premises;
  uint64_t carried, quantity;
  unsigned resource, requested, delivery;
} CdDslCargo;
enum { CD_DSL_CERTIFICATES = 8 };
typedef struct {
  unsigned role;
} CdDslCertificate;
typedef struct {
  CdResidentView civil;
  uint32_t anchor;
  CdRoute route;
  uint32_t route_index, vertex;
  CdTravelState travel;
  CdRouteReason reason;
  CdEntityId destination;
  uint64_t request;
  uint32_t door, door_vertex, held_edge, inside;
  int entering, door_key, door_emergency;
  CdEntityId presence;
  unsigned fatigue, rest_left, meals, raw, skills;
  int sleeping;
  unsigned household_care;
  CdEntityId school_door;
  unsigned school_leave, school_start, school_end;
  CdHumanActivity activity;
  uint64_t needs_minutes, eaten, unmet_meals;
  CdHealthView health;
  uint64_t condition_since;
  CdPersonRef care_target, feed_target;
  unsigned preventive_minutes;
  CdFoodAction food_action;
  CdFoodEndpoint food_endpoint;
  unsigned food_remaining;
  char training_basis[96];
  CdEntityId jobs[2];
  CdDslCertificate certificates[CD_DSL_CERTIFICATES];
  CdWorkTask work;
  CdTradeTask trade;
  CdCargo cargo;
  CdDslCargo dsl_cargo;
  uint64_t wallet, goods[CD_GOODS];
  unsigned credentials, ordination_kind, water, ration_used;
  CdEntityId ordination_institution, memberships[4], vehicle, driving, refuge;
  uint64_t mission, gathering, ration_day, last_care_grant;
  CdPersonId last_care_subject;
  uint64_t bereavement_event, support_until;
  CdRecordCursor record_cursor;
  char credential_basis[96], ordination_basis[96];
} CdCitizen;
typedef struct {
  CdEntityId id;
  uint32_t dwelling;
  size_t members;
  int housing_known;
} CdHousehold;
enum { CD_DSL_PREMISES_STOCKS = 16, CD_DSL_RESOURCE_LIMIT = 128 };
typedef struct {
  uint64_t available, reserved, key, work;
  unsigned resource, settled;
} CdDslStock;
typedef struct {
  unsigned meals, raw, enabled, producer, beds, occupied, medicine;
  char food_basis[96], clinic_basis[96];
  uint64_t goods[CD_GOODS], reserved[CD_GOODS], treasury, escrow_money;
  CdDslStock dsl_stock[CD_DSL_PREMISES_STOCKS];
  unsigned reserved_raw, reserved_meals, prices[CD_GOODS], meal_price;
  unsigned water, shelter_capacity, fire;
  char water_basis[96], shelter_basis[96], fire_basis[96];
  unsigned work_capacity, condition, capacity_override;
  int capacity_changed, employer, closed;
  char employer_name[96], employer_basis[96], scenario_use[96];
} CdPremises;
struct CdWorld {
  CdContent *content;
  CdSmokeSession *native;
  CdCitizen *people;
  CdHousehold *homes;
  size_t count, home_count, initial_count;
  uint32_t *housing, *visitors, *crowd, *traffic;
  CdPersonRef *owners;
  unsigned char *closed, *locks;
  double *distance;
  uint32_t *previous, *heap, *heap_pos;
  size_t heap_count;
  uint64_t next_request, seed;
  unsigned origin_offset;
  CdPremises *premises;
  unsigned *home_meals, *home_raw;
  CdLifeRecord *life;
  size_t life_count, life_capacity;
  uint64_t admissions, births, deaths, departures, birthdays, last_human_minute;
  CdFoodLedger food;
  uint64_t medicine_imported, medicine_used;
  int mortality_blocked;
  CdJob *jobs;
  size_t job_count;
  CdProject *projects;
  CdInstitutionRuntime *institutions;
  uint64_t *home_wallet, *home_goods;
  uint64_t next_work, next_trade, next_project;
  CdEconomyLedger economy;
  uint64_t dsl_imported[CD_DSL_RESOURCE_LIMIT], dsl_consumed[CD_DSL_RESOURCE_LIMIT],
           dsl_produced[CD_DSL_RESOURCE_LIMIT], dsl_lost[CD_DSL_RESOURCE_LIMIT];
  CdOffice *offices;
  CdRoster *rosters;
  CdService *services;
  CdGathering *gatherings;
  CdVehicle *vehicles;
  CdPressure *pressures;
  size_t office_count, roster_count, service_count, gathering_count,
      vehicle_count, pressure_count;
  unsigned rain, alert, blackout, ration_limit;
  uint64_t water_imported, water_used, water_exported, fire_minutes, damage,
      service_outputs, attendance_minutes;
  CdRecord *records;
  CdRecordCursor *record_cursors;
  CdOperation *operations;
  uint64_t record_next, record_dropped, last_cause, current_cause;
  size_t record_count, operation_count;
  int record_saturated;
};
struct CdSnapshot {
  CdContent *content;
  size_t count;
  CdResidentView *people;
  CdTravelState *travel;
  CdInstitutionRuntime *institutions;
  CdCivicSummary civic;
  CdRecord *records;
  CdRecordSummary record_summary;
};
void cd_content_retain(CdContent *);
CdStatus cd_diag(CdDiagnostic *, CdStatus, const char *);
int cd_polygon_contains(const CdContent *, uint32_t, CdPoint, int boundary);
int cd_ring_contains(const CdContent *, const CdRing *, CdPoint, int *boundary);
int cd_segment_blocked(const CdContent *, CdPoint, CdPoint, int bridge,
                       int rail, uint32_t ignore);
CdStatus cd_access_polygon(const CdPolygon *, int, const CdAccess *);
unsigned cd_polygon_crossings(const CdContent *, uint32_t, CdPoint, CdPoint);
int cd_door_valid(const CdContent *, const CdDoor *);
uint32_t cd_polygon_index(const CdContent *, CdEntityId);
uint32_t cd_node_index(const CdContent *, CdEntityId);
uint32_t cd_edge_index(const CdContent *, CdEntityId);
uint32_t cd_door_index(const CdContent *, CdEntityId);
CdStatus cd_route_indices(CdWorld *, uint32_t, uint32_t, CdRoute *);
CdCitizen *cd_citizen(CdWorld *, CdPersonRef);
void cd_population_name(const CdWorld *, CdPersonId, CdEntityId, char *);
CdStatus cd_population_init(CdWorld *, const CdScenario *, const unsigned *);
void cd_population_view(const CdWorld *, const CdCitizen *, CdResidentView *);
void cd_city_minute(CdSmokeSession *, void *);
void cd_movement_tick(CdWorld *);
CdStatus cd_housing_init(CdWorld *);
size_t cd_housing_load(const CdWorld *, uint32_t);
int cd_housing_transient(const CdWorld *, const CdCitizen *, uint32_t);
unsigned cd_civil_age(CdCivilTime, CdCivilTime);
int cd_civil_valid(CdCivilTime);
CdPoint cd_city_position(const CdWorld *, const CdCitizen *);
double cd_city_scale(const CdWorld *);
unsigned cd_city_walk_units(const CdWorld *);
unsigned cd_days_in_month(int, unsigned);
int cd_model_ready(const CdWorld *);
int cd_food_personal(CdWorld *, CdCitizen *);
int cd_basis_valid(const char *);
int cd_colocated(const CdWorld *, const CdCitizen *, const CdCitizen *);
void cd_human_init(CdWorld *);
void cd_human_tick(CdWorld *);
void cd_needs_tick(CdWorld *);
void cd_food_tick(CdWorld *);
void cd_health_tick(CdWorld *);
void cd_care_release(CdWorld *, CdCitizen *);
CdStatus cd_mortality_commit(CdWorld *, CdDiagnostic *);
uint64_t cd_human_digest(const CdWorld *, uint64_t);
unsigned cd_capacity(const CdWorld *, uint32_t);
int cd_usable(const CdWorld *, uint32_t);
int cd_employee(const CdWorld *, const CdCitizen *, uint32_t);
int cd_job_sleep(const CdWorld *, const CdCitizen *, unsigned);
int cd_job_qualified(const CdWorld *, const CdJob *, const CdCitizen *);
int cd_shift(unsigned, unsigned, unsigned);
CdJob *cd_job(CdWorld *, CdEntityId);
void cd_jobs_plan(CdWorld *);
void cd_jobs_tick(CdWorld *);
void cd_economy_tick(CdWorld *);
void cd_development_tick(CdWorld *);
void cd_work_release(CdWorld *, CdCitizen *, int);
void cd_trade_release(CdWorld *, CdCitizen *);
void cd_employment_retire(CdWorld *, CdCitizen *, CdRetirement);
uint64_t cd_work_digest(const CdWorld *, uint64_t);
const CdDslRole *cd_dsl_job_role(CdEntityId);
int cd_dsl_certified(const CdCitizen *, const CdDslRole *);
int cd_dsl_certificate_valid(const CdCitizen *);
int cd_job_mentor_matches(const CdJob *, const CdJob *);
int cd_job_role_enabled(CdEntityId);
int cd_sector_job(CdEntityId);

int cd_dsl_stock_valid(const CdWorld *);
void cd_dsl_cargo_tick(CdWorld *, CdCitizen *);
void cd_dsl_cargo_retire(CdWorld *, CdCitizen *);
typedef enum { CD_DSL_HOLD, CD_DSL_CHECK_OUTPUT, CD_DSL_COMPLETE, CD_DSL_RETURN } CdDslWorkAction;
int cd_dsl_work_stock(CdWorld *, uint32_t, const CdWorkTask *, CdDslWorkAction);
int cd_dsl_work_holds(const CdWorld *, uint32_t, const CdWorkTask *, CdPremises *);

uint32_t cd_site_index(const CdContent *, CdEntityId);
int cd_pending(CdTaskState);
uint64_t cd_now(const CdWorld *);
uint32_t cd_reviewed_door(const CdWorld *, CdEntityId);
uint32_t cd_source_node(CdWorld *, CdCitizen *);
void cd_automatic_travel(CdWorld *, CdPersonRef, CdEntityId);
void cd_commute_to(CdWorld *, CdCitizen *, uint32_t, int, int);
int cd_civic_busy(const CdCitizen *);
int cd_office_job_valid(CdWorld *, CdEntityId);
int cd_provider(CdWorld *, int, CdEntityId, CdJob **, CdCitizen **, unsigned *);
void cd_offices_tick(CdWorld *);
void cd_community_plan(CdWorld *);
void cd_community_tick(CdWorld *);
void cd_services_plan(CdWorld *);
void cd_services_tick(CdWorld *);
void cd_logistics_plan(CdWorld *);
void cd_logistics_tick(CdWorld *);
unsigned cd_vehicle_extra(const CdWorld *, const CdCitizen *, uint32_t);
void cd_environment_tick(CdWorld *);
void cd_shelter_plan(CdWorld *);
int cd_edge_closed(const CdWorld *, uint32_t);
int cd_ration_available(const CdWorld *, const CdCitizen *);
void cd_civic_retire(CdWorld *, CdCitizen *, CdRetirement);
uint64_t cd_civic_digest(const CdWorld *, uint64_t);
CdStatus cd_care_begin(CdWorld *, CdPersonRef, CdPersonRef, CdEntityId,
                       unsigned, CdDiagnostic *);
CdStatus cd_world_allocate(const CdContent *, const CdScenario *, CdWorld **,
                           CdDiagnostic *);
void cd_world_destroy_staged(CdWorld *);
int cd_world_is_owner(const CdWorld *);
void cd_world_publish_checkpoint(CdWorld **, CdWorld *, CdSmokeStage **);
uint64_t cd_world_state_digest(const CdWorld *, uint64_t);
uint64_t cd_record_emit(CdWorld *, CdEventKind, CdPersonRef, CdPersonRef,
                        CdEntityId, uint64_t, CdRecordPrivacy, const char *,
                        const char *);
void cd_social_tick(CdWorld *);
void cd_records_tick(CdWorld *);
uint64_t cd_observer_digest(const CdWorld *, uint64_t);
void cd_operations_tick(CdWorld *);
size_t cd_record_cursor_count(const CdContent *);
int cd_record_visible(const CdRecord *, CdRecordQuery, CdEntityId);
#endif
