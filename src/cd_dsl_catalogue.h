#ifndef CD_DSL_CATALOGUE_H
#define CD_DSL_CATALOGUE_H
#include "cd_api.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char *id, *sector;
  size_t premises_count;
  const char *const *premises;
} CdDslCareer;

typedef struct {
  const char *id, *title, *career, *qualification;
  unsigned stage, training_post, enabled, mentor_minimum_stage, owner, independent;
  size_t requirement_count;
  const char *const *requirements;
} CdDslRole;

typedef struct {
  const char *id, *from, *to;
  unsigned minimum_minutes;
} CdDslAdvance;

typedef struct {
  const char *id, *group, *unit;
  unsigned food_energy_per_unit;
} CdDslResource;

typedef struct {
  const char *resource;
  uint64_t quantity;
} CdDslQuantity;

typedef struct {
  const char *id, *unit, *power;
  unsigned worker_minutes;
  size_t input_count, output_count, equipment_count;
  const CdDslQuantity *inputs, *outputs;
  const char *const *equipment;
} CdDslRecipe;

const char *cd_dsl_catalogue_identity(void);
size_t cd_dsl_career_count(void);
size_t cd_dsl_role_count(void);
size_t cd_dsl_advance_count(void);
size_t cd_dsl_resource_count(void);
size_t cd_dsl_recipe_count(void);
const CdDslCareer *cd_dsl_career(const char *id);
const CdDslRole *cd_dsl_role(const char *id);
const CdDslRole *cd_dsl_role_at(size_t index);
CdEntityId cd_dsl_role_occupation(const CdDslRole *);
CdStatus cd_world_dsl_eligibility(const CdWorld *, CdPersonRef, const char *,
                                  CdEntityId, int *, uint64_t *, CdDiagnostic *);
CdStatus cd_world_dsl_family_experience(const CdWorld *, CdPersonRef,
                                        const char *, uint64_t *, CdDiagnostic *);
const char *cd_dsl_role_recipe(const CdDslRole *);
const CdDslAdvance *cd_dsl_advance(const char *id);
const CdDslResource *cd_dsl_resource(const char *id);
const CdDslResource *cd_dsl_resource_at(size_t index);
const CdDslRecipe *cd_dsl_recipe(const char *id);
const CdDslRecipe *cd_dsl_recipe_at(size_t index);

enum { CD_DSL_GRADE_UNKNOWN = 1001 };
typedef struct {
  uint64_t id, expires;
  unsigned quality, condition;
  char provenance[96];
} CdDslBatch;

typedef struct {
  CdDslBatch batch;
  uint64_t available, reserved;
} CdDslBatchView;

CdStatus cd_world_dsl_import_batch(CdWorld *, CdEntityId, const char *, const char *,
                                   uint64_t, const CdDslBatch *, uint64_t *, CdDiagnostic *);
CdStatus cd_world_dsl_batch(const CdWorld *, CdEntityId, const char *, uint64_t,
                            CdDslBatchView *, CdDiagnostic *);
CdStatus cd_world_dsl_batch_reserve(CdWorld *, CdEntityId, const char *, uint64_t,
                                    uint64_t, uint64_t, CdDiagnostic *);
CdStatus cd_world_dsl_batch_settle(CdWorld *, CdEntityId, const char *, uint64_t,
                                   uint64_t, int, CdDiagnostic *);
CdStatus cd_world_dsl_batch_wear(CdWorld *, CdEntityId, const char *, uint64_t,
                                 unsigned, CdDiagnostic *);
CdStatus cd_world_dsl_purchase(CdWorld *, CdPersonRef, CdEntityId, const char *,
                                uint64_t, uint64_t, uint64_t, uint64_t, CdDiagnostic *);
CdStatus cd_world_dsl_purchase_at_post(CdWorld *, CdPersonRef, CdEntityId, CdEntityId,
                                        const char *, uint64_t, uint64_t, uint64_t,
                                        uint64_t, CdDiagnostic *);
CdStatus cd_world_dsl_retail_open(CdWorld *, CdPersonRef, CdEntityId, int, CdDiagnostic *);
CdStatus cd_world_dsl_eat_carried(CdWorld *, CdPersonRef, CdEntityId, uint64_t, CdDiagnostic *);
CdStatus cd_world_dsl_batch_cargo_order(CdWorld *, CdPersonRef, CdEntityId,
                                        const char *, uint64_t, uint64_t, uint64_t,
                                        int, CdDiagnostic *);

typedef struct {
  uint64_t available, reserved;
} CdDslStockView;

CdStatus cd_world_dsl_stock(const CdWorld *, CdEntityId, const char *,
                            CdDslStockView *, CdDiagnostic *);
CdStatus cd_world_dsl_import(CdWorld *, CdEntityId, const char *, uint64_t,
                             const char *, CdDiagnostic *);
CdStatus cd_world_dsl_reserve(CdWorld *, CdEntityId, const char *, uint64_t key,
                              uint64_t quantity, CdDiagnostic *);
CdStatus cd_world_dsl_settle(CdWorld *, CdEntityId, const char *, uint64_t key,
                             int consume, CdDiagnostic *);

typedef struct {
  const char *resource;
  uint64_t carried, pending_quantity;
  CdDslBatch batch;
} CdDslCargoView;

CdStatus cd_world_dsl_cargo_order(CdWorld *, CdPersonRef, CdEntityId,
                                  const char *resource, uint64_t quantity,
                                  int delivery, CdDiagnostic *);
CdStatus cd_world_dsl_cargo_cancel(CdWorld *, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_dsl_cargo(const CdWorld *, CdPersonRef, CdDslCargoView *,
                            CdDiagnostic *);

#endif
