#ifndef CD_HUMAN_H
#define CD_HUMAN_H
#include "cd_city.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  CD_SKILL_FOOD = 1,
  CD_SKILL_CRAFT = 2,
  CD_SKILL_CARE = 4,
  CD_SKILL_COMMUNITY = 8
};
typedef enum { CD_DEPARTURE, CD_DEATH } CdRetirement;
typedef struct {
  const char *name, *basis;
  CdCivilTime birth;
  CdEntityId household;
  CdResidentCategory category;
  CdPersonRef guardian;
} CdAdmission;
typedef struct {
  CdPersonRef ref;
  uint64_t minute;
  CdEntityId household;
  unsigned age;
  CdRetirement reason;
  CdCivilTime birth;
  CdPersonRef parents[2];
  unsigned meals, raw;
  uint64_t goods[6];
  unsigned water;
  int position_known;
  CdPoint position;
  char name[128], basis[96];
} CdLifeRecord;
typedef struct {
  unsigned fatigue;
  int energy, sleeping;
  CdHumanActivity activity;
  uint64_t minutes, meals, unmet_meals;
  unsigned carried_meals, carried_raw, skills;
} CdNeedsView;
typedef enum { CD_FOOD_STORE, CD_FOOD_HOME } CdFoodEndpointKind;
typedef struct {
  CdFoodEndpointKind kind;
  CdEntityId id;
} CdFoodEndpoint;
typedef enum {
  CD_FETCH_MEALS,
  CD_DELIVER_MEALS,
  CD_FETCH_RAW,
  CD_DELIVER_RAW,
  CD_PRODUCE_MEALS
} CdFoodAction;
typedef struct {
  uint64_t meal_stock, raw_stock, initial_meals, imported_meals, imported_raw,
      produced, consumed, retired_transfers, unmet;
  uint64_t exported_meals, exported_raw, cached_meals, cached_raw;
} CdFoodLedger;
typedef enum {
  CD_HEALTHY,
  CD_ILL,
  CD_INJURED,
  CD_RECOVERING,
  CD_CRITICAL
} CdCondition;
typedef struct {
  CdCondition condition;
  unsigned severity, remaining, starvation;
  uint64_t care_minutes, prevention_until;
  CdEntityId clinic;
  CdPersonRef carer;
  int mortality_pending, mortality_blocked;
  char basis[96];
} CdHealthView;
typedef struct {
  unsigned beds, occupied, medicine;
  uint64_t imported, used;
} CdClinicView;
CdStatus cd_world_clinic_view(const CdWorld *, CdEntityId, CdClinicView *,
                              CdDiagnostic *);
typedef struct {
  CdEntityId id;
  const char *name, *source, *basis, *output;
  unsigned minimum_age, skills;
  int enabled;
} CdOccupationView;
CdStatus cd_world_admit(CdWorld *, const CdAdmission *, CdPersonRef *,
                        CdDiagnostic *);
CdStatus cd_world_birth(CdWorld *, CdPersonRef guardian,
                        CdPersonRef other_parent, const char *basis,
                        CdPersonRef *, CdDiagnostic *);
CdStatus cd_world_retire(CdWorld *, CdPersonRef, CdRetirement,
                         const char *basis, CdDiagnostic *);
size_t cd_world_life_count(const CdWorld *);
CdStatus cd_world_life(const CdWorld *, size_t, CdLifeRecord *, CdDiagnostic *);
CdStatus cd_world_needs(const CdWorld *, CdPersonRef, CdNeedsView *,
                        CdDiagnostic *);
CdStatus cd_world_rest(CdWorld *, CdPersonRef, unsigned minutes,
                       CdDiagnostic *);
CdStatus cd_world_training(CdWorld *, CdPersonRef, unsigned skills,
                           const char *basis, CdDiagnostic *);
CdStatus cd_world_food_store(CdWorld *, CdEntityId, int producer,
                             const char *basis, CdDiagnostic *);
CdStatus cd_world_food_import(CdWorld *, CdFoodEndpoint, unsigned meals,
                              unsigned raw, const char *basis, CdDiagnostic *);
CdStatus cd_world_food_order(CdWorld *, CdPersonRef, CdFoodAction,
                             CdFoodEndpoint, unsigned units, CdDiagnostic *);
CdStatus cd_world_food_cancel(CdWorld *, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_feed(CdWorld *, CdPersonRef carer, CdPersonRef dependent,
                       CdDiagnostic *);
CdStatus cd_world_food_ledger(const CdWorld *, CdFoodLedger *, CdDiagnostic *);
CdStatus cd_world_health(const CdWorld *, CdPersonRef, CdHealthView *,
                         CdDiagnostic *);
CdStatus cd_world_expose(CdWorld *, CdPersonRef, CdCondition, unsigned severity,
                         unsigned minutes, const char *basis, CdDiagnostic *);
CdStatus cd_world_clinic(CdWorld *, CdEntityId, unsigned beds,
                         unsigned medicine, const char *basis, CdDiagnostic *);
CdStatus cd_world_care(CdWorld *, CdPersonRef carer, CdPersonRef patient,
                       CdEntityId clinic, unsigned preventive_minutes,
                       CdDiagnostic *);
CdStatus cd_world_care_cancel(CdWorld *, CdPersonRef carer, CdDiagnostic *);
size_t cd_occupation_count(void);
CdStatus cd_occupation(size_t, CdOccupationView *, CdDiagnostic *);
CdStatus cd_world_occupation_eligible(const CdWorld *, CdPersonRef, CdEntityId,
                                      int *, CdDiagnostic *);
const char *cd_occupation_identity(void);
/* Daily school enrollment, with a civil-minute departure and session. */
CdStatus cd_world_enroll_school(CdWorld *, CdPersonRef, CdEntityId,
                                unsigned, unsigned, unsigned, CdDiagnostic *);
typedef struct {
  unsigned designated;
  unsigned infants, preschool, school_age, teenagers;
} CdHouseholdCareView;
CdStatus cd_world_household_care(const CdWorld *, CdPersonRef, CdHouseholdCareView *,
                                 CdDiagnostic *);
CdStatus cd_world_set_household_care(CdWorld *, CdPersonRef, int, CdDiagnostic *);
#ifdef __cplusplus
}
#endif
#endif
