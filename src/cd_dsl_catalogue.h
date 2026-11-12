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
  unsigned stage, training_post, enabled, mentor_minimum_stage, owner;
} CdDslRole;

typedef struct {
  const char *id, *from, *to;
} CdDslAdvance;

typedef struct {
  const char *id, *group, *unit;
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
const char *cd_dsl_role_recipe(const CdDslRole *);
const CdDslAdvance *cd_dsl_advance(const char *id);
const CdDslResource *cd_dsl_resource(const char *id);
const CdDslResource *cd_dsl_resource_at(size_t index);
const CdDslRecipe *cd_dsl_recipe(const char *id);
const CdDslRecipe *cd_dsl_recipe_at(size_t index);

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
} CdDslCargoView;

CdStatus cd_world_dsl_cargo_order(CdWorld *, CdPersonRef, CdEntityId,
                                  const char *resource, uint64_t quantity,
                                  int delivery, CdDiagnostic *);
CdStatus cd_world_dsl_cargo_cancel(CdWorld *, CdPersonRef, CdDiagnostic *);
CdStatus cd_world_dsl_cargo(const CdWorld *, CdPersonRef, CdDslCargoView *,
                            CdDiagnostic *);

#endif
