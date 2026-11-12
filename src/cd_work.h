#ifndef CD_WORK_H
#define CD_WORK_H
#include "cd_human.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { CD_WOOD, CD_CLAY, CD_FIBER, CD_TOOL, CD_POT, CD_TEXTILE, CD_GOODS };
typedef enum {
  CD_TASK_NONE,
  CD_TASK_PENDING,
  CD_TASK_WAITING,
  CD_TASK_DONE,
  CD_TASK_CANCELLED
} CdTaskState;
typedef enum {
  CD_RECIPE_MEAL,
  CD_RECIPE_POT,
  CD_RECIPE_TEXTILE,
  CD_RECIPE_TOOL,
  CD_RECIPE_SERVICE,
  CD_RECIPE_DEVELOPMENT,
  CD_RECIPE_REFINED
} CdRecipe;
typedef struct {
  CdEntityId workplace, entrance, occupation, institution;
  unsigned shift_start, shift_end, wage;
  int apprenticeship, has_key;
  const char *basis;
} CdJobSpec;
typedef struct {
  CdEntityId id, workplace, entrance, occupation, institution;
  CdPersonRef worker, mentor;
  unsigned shift_start, shift_end, wage;
  int open, apprenticeship;
  char basis[96];
} CdJobView;
typedef struct {
  CdEntityId job, project;
  CdRecipe recipe;
  unsigned units;
  const char *basis;
} CdWorkSpec;
typedef struct {
  uint64_t id, key;
  CdTaskState state;
  CdEntityId job, project;
  CdRecipe recipe;
  unsigned duration, progress;
  uint64_t paid, reserved_money;
  char reason[96];
} CdWorkView;
typedef struct {
  uint64_t imported[CD_GOODS], produced[CD_GOODS], consumed[CD_GOODS],
      exported[CD_GOODS], stock[CD_GOODS];
  uint64_t money_created, money_stock, wages, trade_value, work_completed,
      work_cancelled, services;
} CdEconomyLedger;
typedef enum {
  CD_ACCOUNT_PERSON,
  CD_ACCOUNT_HOME,
  CD_ACCOUNT_EMPLOYER
} CdAccountKind;
typedef struct {
  CdAccountKind kind;
  CdEntityId id;
  CdPersonRef person;
} CdAccount;
typedef struct {
  uint64_t id, key, total;
  CdTaskState state;
  CdEntityId seller;
  unsigned good, quantity;
  int meals;
  char reason[96];
} CdTradeView;
typedef enum {
  CD_DEV_REPAIR,
  CD_DEV_CAPACITY,
  CD_DEV_USE,
  CD_DEV_GEOMETRY
} CdDevelopmentKind;
typedef struct {
  CdDevelopmentKind kind;
  unsigned capacity, minutes;
  const char *use, *basis;
} CdDevelopmentSpec;
typedef struct {
  uint64_t id, key;
  CdEntityId building;
  CdTaskState state;
  CdDevelopmentKind kind;
  unsigned capacity, minutes, progress;
  char use[96], basis[96], reason[96];
} CdDevelopmentView;
typedef struct {
  CdEntityId building;
  unsigned capacity, condition, work_capacity;
  int capacity_known, scenario_override, employer_open;
  char use[96], employer[96];
} CdPremisesView;
typedef struct {
  CdEntityId id;
  const char *source_id, *name, *religion, *denomination, *status, *site_type,
      *source_record, *location_basis;
  unsigned setting_year;
  int located, identity_verified, review_required, confidence_known,
      operation_known, operating, precinct_boundary_known, congregation_known;
  double confidence;
  CdPoint source_position;
  CdEntityId premises[8];
  size_t premises_count;
  char office[96], operation_basis[96], link_basis[8][96];
  int scenario_override;
} CdInstitutionDetail;
CdStatus cd_world_employer(CdWorld *, CdEntityId, const char *name,
                           unsigned capacity, const char *basis,
                           CdDiagnostic *);
CdStatus cd_world_employer_open(CdWorld *, CdEntityId, int, CdDiagnostic *);
CdStatus cd_world_dsl_certify(CdWorld *, CdPersonRef, const char *role,
                              const char *basis, CdDiagnostic *);
CdStatus cd_world_dsl_job_create(CdWorld *, const CdJobSpec *, const char *role,
                                  CdEntityId *, CdDiagnostic *);
CdStatus cd_world_dsl_supervise(CdWorld *, CdPersonRef supervisor,
                                 CdPersonRef worker, uint64_t key, CdEntityId job,
                                 const char *recipe, unsigned batches,
                                 const char *basis, uint64_t *, CdDiagnostic *);
CdStatus cd_world_dsl_fund(CdWorld *, CdPersonRef owner, CdEntityId premises,
                            uint64_t amount, CdDiagnostic *);
CdStatus cd_world_job_create(CdWorld *, const CdJobSpec *, CdEntityId *,
                             CdDiagnostic *);
size_t cd_world_job_count(const CdWorld *);
CdStatus cd_world_job(const CdWorld *, CdEntityId, CdJobView *, CdDiagnostic *);
CdStatus cd_world_job_assign(CdWorld *, CdEntityId, CdPersonRef,
                             CdPersonRef mentor, int secondary, CdDiagnostic *);
/* Fill a batch of regular funded vacancies from an ordered applicant list. */
CdStatus cd_world_staff_jobs(CdWorld *, const CdEntityId *, size_t job_count,
                              const CdPersonRef *, size_t applicant_count,
                              CdDiagnostic *);
CdStatus cd_world_job_unassign(CdWorld *, CdEntityId, CdDiagnostic *);
CdStatus cd_world_job_close(CdWorld *, CdEntityId, CdDiagnostic *);
CdStatus cd_world_work_submit(CdWorld *, CdPersonRef, uint64_t key,
                              const CdWorkSpec *, uint64_t *, CdDiagnostic *);
CdStatus cd_world_refined_work_submit(CdWorld *, CdPersonRef, uint64_t key,
                                       CdEntityId job, const char *recipe,
                                       unsigned batches, const char *basis,
                                       uint64_t *, CdDiagnostic *);
CdStatus cd_world_work(const CdWorld *, CdPersonRef, CdWorkView *,
                       CdDiagnostic *);
CdStatus cd_world_work_cancel(CdWorld *, CdPersonRef, uint64_t, CdDiagnostic *);
CdStatus cd_world_money_import(CdWorld *, CdAccount, uint64_t,
                               const char *basis, CdDiagnostic *);
CdStatus cd_world_money_transfer(CdWorld *, CdAccount, CdAccount, uint64_t,
                                 CdDiagnostic *);
CdStatus cd_world_balance(const CdWorld *, CdAccount, uint64_t *,
                          CdDiagnostic *);
CdStatus cd_world_goods_import(CdWorld *, CdEntityId, unsigned good, uint64_t,
                               const char *basis, CdDiagnostic *);
CdStatus cd_world_goods_order(CdWorld *, CdPersonRef, CdEntityId, unsigned good,
                              unsigned units, int delivery, CdDiagnostic *);
CdStatus cd_world_price(CdWorld *, CdEntityId, unsigned good, int meals,
                        unsigned price, const char *basis, CdDiagnostic *);
CdStatus cd_world_purchase(CdWorld *, CdPersonRef, uint64_t key, CdEntityId,
                           unsigned good, int meals, unsigned units,
                           CdDiagnostic *);
CdStatus cd_world_trade(const CdWorld *, CdPersonRef, CdTradeView *,
                        CdDiagnostic *);
CdStatus cd_world_trade_cancel(CdWorld *, CdPersonRef, uint64_t,
                               CdDiagnostic *);
CdStatus cd_world_economy_ledger(const CdWorld *, CdEconomyLedger *,
                                 CdDiagnostic *);
CdStatus cd_world_wear(CdWorld *, CdEntityId, unsigned, const char *basis,
                       CdDiagnostic *);
CdStatus cd_world_project(CdWorld *, CdEntityId, uint64_t key,
                          const CdDevelopmentSpec *, uint64_t *,
                          CdDiagnostic *);
CdStatus cd_world_project_view(const CdWorld *, CdEntityId, CdDevelopmentView *,
                               CdDiagnostic *);
CdStatus cd_world_project_cancel(CdWorld *, CdEntityId, uint64_t,
                                 CdDiagnostic *);
CdStatus cd_world_premises(const CdWorld *, CdEntityId, CdPremisesView *,
                           CdDiagnostic *);
CdStatus cd_snapshot_institution_detail(const CdSnapshot *, CdEntityId,
                                        CdInstitutionDetail *, CdDiagnostic *);
CdStatus cd_world_goods_cancel(CdWorld *, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_home_goods_order(CdWorld *, CdPersonRef, unsigned, unsigned,
                                   int, CdDiagnostic *);
size_t cd_world_institution_count(const CdWorld *);
CdStatus cd_world_institution(const CdWorld *, size_t, CdInstitutionDetail *,
                              CdDiagnostic *);
CdStatus cd_world_institution_link(CdWorld *, CdEntityId, CdEntityId,
                                   const char *basis, CdDiagnostic *);
CdStatus cd_world_institution_operate(CdWorld *, CdEntityId, int active,
                                      int scenario_override, const char *office,
                                      const char *basis, CdDiagnostic *);
#ifdef __cplusplus
}
#endif
#endif
