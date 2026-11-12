#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include "cd_native.h"
#include <string.h>

#define STOCK_LIMIT UINT64_C(1000000000)

static unsigned resource_number(const char *id) {
  const CdDslResource *resource = cd_dsl_resource(id);
  return resource ? (unsigned)(resource - cd_dsl_resource_at(0)) + 1 : 0;
}

static uint64_t carry_capacity(unsigned resource) {
  const CdDslResource *kind = resource ? cd_dsl_resource_at(resource - 1) : NULL;
  return kind ? (strcmp(kind->unit, "each") == 0 ? 1 : 5000) : 0;
}

static uint32_t building_index(const CdWorld *w, CdEntityId id) {
  uint32_t index = w ? cd_polygon_index(w->content, id) : CD_NONE;
  return index != CD_NONE && w->content->polygons[index].kind == CD_BUILDING
             ? index : CD_NONE;
}

static CdDslStock *slot(CdPremises *site, unsigned resource, int vacant) {
  CdDslStock *empty = NULL;
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    CdDslStock *stock = &site->dsl_stock[i];
    if (stock->resource == resource)
      return stock;
    if (!stock->resource && !empty)
      empty = stock;
  }
  return vacant ? empty : NULL;
}

CdStatus cd_world_dsl_stock(const CdWorld *w, CdEntityId id, const char *name,
                            CdDslStockView *out, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (index == CD_NONE || !resource || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown stock premises or resource");
  const CdDslStock *stock = slot(&w->premises[index], resource, 0);
  *out = stock ? (CdDslStockView){stock->available, stock->reserved}
               : (CdDslStockView){0};
  return cd_diag(d, CD_OK, "Finite premises stock");
}

CdStatus cd_world_dsl_import(CdWorld *w, CdEntityId id, const char *name,
                             uint64_t quantity, const char *basis, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (!cd_model_ready(w) || index == CD_NONE || !resource ||
      resource > CD_DSL_RESOURCE_LIMIT || !quantity || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Import requires premises, resource, quantity and basis");
  CdDslStock *stock = slot(&w->premises[index], resource, 1);
  if (!stock || quantity > STOCK_LIMIT - stock->available - stock->reserved ||
      quantity > UINT64_MAX - w->dsl_imported[resource - 1] - w->dsl_produced[resource - 1])
    return cd_diag(d, CD_CAPACITY, "Premises stock or import ledger limit exceeded");
  stock->resource = resource;
  stock->available += quantity;
  w->dsl_imported[resource - 1] += quantity;
  cd_record_emit(w, CD_EVENT_INVENTORY, (CdPersonRef){0}, (CdPersonRef){0},
                 id, 0, CD_RECORD_DIAGNOSTIC, basis, "Refined resource imported");
  return cd_diag(d, CD_OK, "Refined resource imported");
}

CdStatus cd_world_dsl_reserve(CdWorld *w, CdEntityId id, const char *name,
                              uint64_t key, uint64_t quantity, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (!cd_model_ready(w) || index == CD_NONE || !resource || !key || !quantity)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid resource reservation");
  CdDslStock *stock = slot(&w->premises[index], resource, 0);
  if (stock && !stock->work && stock->key == key && stock->reserved == quantity)
    return cd_diag(d, CD_OK, "Existing reservation");
  if (!stock || stock->available < quantity)
    return cd_diag(d, CD_CAPACITY, "Insufficient available resource");
  if (stock->reserved || key <= stock->key)
    return cd_diag(d, CD_INVALID_STATE, "Outstanding reservation or stale key");
  stock->available -= quantity;
  stock->reserved = quantity;
  stock->key = key;
  stock->settled = 0;
  return cd_diag(d, CD_OK, "Resource reserved");
}

CdStatus cd_world_dsl_settle(CdWorld *w, CdEntityId id, const char *name,
                             uint64_t key, int consume, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (!cd_model_ready(w) || index == CD_NONE || !resource || !key ||
      (consume != 0 && consume != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid reservation settlement");
  CdDslStock *stock = slot(&w->premises[index], resource, 0);
  unsigned settlement = consume ? 2 : 1;
  if (!stock || stock->work || stock->key != key || (stock->settled && stock->settled != settlement))
    return cd_diag(d, CD_INVALID_STATE, "Reservation key or settlement mismatch");
  if (stock->settled)
    return cd_diag(d, CD_OK, "Existing settlement");
  if (consume)
    w->dsl_consumed[resource - 1] += stock->reserved;
  else
    stock->available += stock->reserved;
  stock->reserved = 0;
  stock->settled = settlement;
  return cd_diag(d, CD_OK, consume ? "Reservation consumed" : "Reservation released");
}

int cd_dsl_stock_valid(const CdWorld *w) {
  uint64_t totals[CD_DSL_RESOURCE_LIMIT] = {0};
  for (uint32_t b = 0; b < w->content->np; ++b) {
    unsigned char seen[CD_DSL_RESOURCE_LIMIT] = {0};
    for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
      const CdDslStock *stock = &w->premises[b].dsl_stock[i];
      if (!stock->resource) {
        if (stock->available || stock->reserved || stock->key || stock->settled || stock->work)
          return 0;
        continue;
      }
      if (w->content->polygons[b].kind != CD_BUILDING ||
          stock->resource > cd_dsl_resource_count() || stock->settled > 2 ||
          (stock->work && !stock->reserved) ||
          (stock->reserved && !stock->work && (!stock->key || stock->settled)) ||
          (stock->key && (!stock->reserved || stock->work) && !stock->settled) ||
          (stock->settled && !stock->key) || stock->available > STOCK_LIMIT ||
          stock->reserved > STOCK_LIMIT - stock->available)
        return 0;
      unsigned r = stock->resource - 1;
      if (seen[r]++)
        return 0;
      totals[r] += stock->available + stock->reserved;
    }
  }
  for (size_t i = 0; i < w->count; ++i) {
    const CdDslCargo *cargo = &w->people[i].dsl_cargo;
    if (cargo->resource > cd_dsl_resource_count() ||
        cargo->requested > cd_dsl_resource_count() ||
        !!cargo->resource != !!cargo->carried ||
        cargo->carried > carry_capacity(cargo->resource) ||
        cargo->delivery > 1 || !!cargo->quantity != !!cargo->requested ||
        !!cargo->quantity != !!cargo->premises ||
        (cargo->quantity &&
         (building_index(w, cargo->premises) == CD_NONE ||
          cargo->quantity > carry_capacity(cargo->requested) ||
          (cargo->resource && cargo->resource != cargo->requested) ||
          (cargo->delivery && cargo->quantity > cargo->carried) ||
          (!cargo->delivery && cargo->carried + cargo->quantity >
                                    carry_capacity(cargo->requested)))))
      return 0;
    if (cargo->carried)
      totals[cargo->resource - 1] += cargo->carried;
  }
  for (size_t i = 0; i < CD_DSL_RESOURCE_LIMIT; ++i) {
    uint64_t imported = w->dsl_imported[i], produced = w->dsl_produced[i];
    if ((i >= cd_dsl_resource_count() &&
         (imported || produced || w->dsl_consumed[i] || w->dsl_lost[i])) ||
        produced > UINT64_MAX - imported ||
        w->dsl_consumed[i] > imported + produced ||
        w->dsl_lost[i] > imported + produced - w->dsl_consumed[i] ||
        totals[i] != imported + produced - w->dsl_consumed[i] - w->dsl_lost[i])
      return 0;
  }
  return 1;
}

static const CdDslRecipe *work_recipe(const CdWorkTask *task) {
  return task->spec.project && task->spec.project <= cd_dsl_recipe_count()
             ? cd_dsl_recipe_at((size_t)task->spec.project - 1)
             : NULL;
}

static int work_hold(CdPremises *site, unsigned resource, uint64_t amount,
                     uint64_t work, CdDslWorkAction action) {
  CdDslStock *stock = slot(site, resource, 0);
  if (!stock || !amount)
    return 0;
  if (action == CD_DSL_HOLD) {
    if (stock->reserved || stock->available < amount)
      return 0;
    stock->available -= amount;
    stock->reserved = amount;
    stock->work = work;
  } else if (stock->work != work || stock->reserved != amount) {
    return 0;
  }
  return 1;
}

int cd_dsl_work_stock(CdWorld *w, uint32_t b, const CdWorkTask *task,
                      CdDslWorkAction action) {
  const CdDslRecipe *recipe = work_recipe(task);
  if (!recipe)
    return 0;
  CdPremises candidate = w->premises[b];
  uint64_t consumed[CD_DSL_RESOURCE_LIMIT] = {0};
  uint64_t produced[CD_DSL_RESOURCE_LIMIT] = {0};
  for (size_t i = 0; i < recipe->input_count + recipe->equipment_count; ++i) {
    int input = i < recipe->input_count;
    const char *name = input ? recipe->inputs[i].resource :
        recipe->equipment[i - recipe->input_count];
    unsigned resource = resource_number(name);
    if (!resource || !task->spec.units ||
        (input && recipe->inputs[i].quantity > STOCK_LIMIT / task->spec.units))
      return 0;
    uint64_t amount = input ? recipe->inputs[i].quantity * task->spec.units : 1;
    if (!work_hold(&candidate, resource, amount, task->view.id, action))
      return 0;
    if (action != CD_DSL_HOLD) {
      CdDslStock *stock = slot(&candidate, resource, 0);
      if (input && action != CD_DSL_RETURN)
        consumed[resource - 1] = amount;
      else
        stock->available += amount;
      stock->reserved = stock->work = 0;
    }
  }
  if (action != CD_DSL_RETURN)
    for (size_t i = 0; i < recipe->output_count; ++i) {
      unsigned resource = resource_number(recipe->outputs[i].resource);
      if (!resource || recipe->outputs[i].quantity > STOCK_LIMIT / task->spec.units)
        return 0;
      CdDslStock *stock = slot(&candidate, resource, 1);
      uint64_t amount = recipe->outputs[i].quantity * task->spec.units;
      uint64_t freed = action == CD_DSL_HOLD && stock &&
                       stock->work == task->view.id ? stock->reserved : 0;
      if (!stock || amount > STOCK_LIMIT - stock->available - stock->reserved + freed ||
          amount > UINT64_MAX - w->dsl_imported[resource - 1] -
                                  w->dsl_produced[resource - 1])
        return 0;
      stock->resource = resource;
      if (action != CD_DSL_HOLD) {
        stock->available += amount;
        produced[resource - 1] = amount;
      }
    }
  if (action == CD_DSL_CHECK_OUTPUT)
    return 1;
  w->premises[b] = candidate;
  if (action == CD_DSL_COMPLETE)
    for (size_t i = 0; i < CD_DSL_RESOURCE_LIMIT; ++i) {
      w->dsl_consumed[i] += consumed[i];
      w->dsl_produced[i] += produced[i];
    }
  return 1;
}

int cd_dsl_work_holds(const CdWorld *w, uint32_t b, const CdWorkTask *task,
                      CdPremises *held) {
  const CdDslRecipe *recipe = work_recipe(task);
  if (!recipe || !recipe->worker_minutes || !task->spec.units ||
      task->view.recipe != CD_RECIPE_REFINED || task->spec.job != task->view.job ||
      (uint64_t)recipe->worker_minutes * task->spec.units != task->view.duration)
    return 0;
  for (size_t i = 0; i < recipe->input_count + recipe->equipment_count; ++i) {
    int input = i < recipe->input_count;
    unsigned resource = resource_number(input ? recipe->inputs[i].resource :
        recipe->equipment[i - recipe->input_count]);
    if (!resource || (input && recipe->inputs[i].quantity > STOCK_LIMIT / task->spec.units))
      return 0;
    const CdDslStock *stock = slot(&w->premises[b], resource, 0);
    uint64_t amount = input ? recipe->inputs[i].quantity * task->spec.units : 1;
    if (!stock || stock->work != task->view.id || stock->reserved != amount)
      return 0;
    size_t index = (size_t)(stock - w->premises[b].dsl_stock);
    if (held->dsl_stock[index].work)
      return 0;
    held->dsl_stock[index].work = task->view.id;
  }
  return 1;
}

static int cargo_present(const CdWorld *w, const CdCitizen *person, uint32_t building) {
  return building != CD_NONE && person->civil.position_known &&
         person->inside == building && person->door == CD_NONE &&
         person->civil.age >= 18 && person->health.severity < 600 &&
         !person->health.mortality_pending && cd_usable(w, building);
}

CdStatus cd_world_dsl_cargo_order(CdWorld *w, CdPersonRef ref, CdEntityId premises,
                                  const char *name, uint64_t quantity,
                                  int delivery, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(w, ref);
  unsigned resource = resource_number(name);
  uint32_t building = building_index(w, premises);
  if (!cd_model_ready(w) || !person || !quantity ||
      quantity > carry_capacity(resource) || (delivery != 0 && delivery != 1) ||
      !cargo_present(w, person, building))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Cargo requires an eligible adult inside the premises");
  CdDslCargo *cargo = &person->dsl_cargo;
  if (cargo->quantity || (cargo->resource && cargo->resource != resource) ||
      (delivery ? quantity > cargo->carried :
                  quantity + cargo->carried > carry_capacity(resource)))
    return cd_diag(d, CD_INVALID_STATE, "Outstanding cargo action or incompatible load");
  cargo->premises = premises;
  cargo->quantity = quantity;
  cargo->requested = resource;
  cargo->delivery = (unsigned)delivery;
  return cd_diag(d, CD_OK, "Cargo action awaits a native minute");
}

static void clear_order(CdDslCargo *cargo) {
  cargo->premises = cargo->quantity = 0;
  cargo->requested = cargo->delivery = 0;
}

CdStatus cd_world_dsl_cargo_cancel(CdWorld *w, CdPersonRef ref, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !person)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown cargo carrier");
  clear_order(&person->dsl_cargo);
  return cd_diag(d, CD_OK, "Pending action cancelled");
}

CdStatus cd_world_dsl_cargo(const CdWorld *w, CdPersonRef ref, CdDslCargoView *out,
                            CdDiagnostic *d) {
  const CdCitizen *person = cd_citizen((CdWorld *)w, ref);
  if (!person || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown cargo carrier");
  const CdDslCargo *cargo = &person->dsl_cargo;
  *out = (CdDslCargoView){
      cargo->resource ? cd_dsl_resource_at(cargo->resource - 1)->id : NULL,
      cargo->carried, cargo->quantity};
  return cd_diag(d, CD_OK, "Refined cargo custody");
}

void cd_dsl_cargo_tick(CdWorld *w, CdCitizen *person) {
  CdDslCargo *cargo = &person->dsl_cargo;
  if (!cargo->quantity || person->activity != CD_ACTIVITY_IDLE)
    return;
  uint32_t building = building_index(w, cargo->premises);
  if (!cargo_present(w, person, building))
    return;
  CdDslStock *stock = slot(&w->premises[building], cargo->requested, cargo->delivery);
  if (!stock || (cargo->delivery
                     ? cargo->quantity > STOCK_LIMIT - stock->available - stock->reserved
                     : cargo->quantity > stock->available))
    return;
  if (!cd_native_claim(w->native, person->civil.ref))
    return;
  if (cargo->delivery) {
    stock->resource = cargo->requested;
    stock->available += cargo->quantity;
    cargo->carried -= cargo->quantity;
    if (!cargo->carried)
      cargo->resource = 0;
  } else {
    stock->available -= cargo->quantity;
    cargo->resource = cargo->requested;
    cargo->carried += cargo->quantity;
  }
  cd_record_emit(w, CD_EVENT_INVENTORY, person->civil.ref, (CdPersonRef){0},
                 cargo->premises, 0, CD_RECORD_DIAGNOSTIC, "native hand cargo",
                 cargo->delivery ? "Refined cargo delivered" : "Refined cargo picked up");
  clear_order(cargo);
  person->activity = CD_ACTIVITY_WORK;
}

void cd_dsl_cargo_retire(CdWorld *w, CdCitizen *person) {
  CdDslCargo *cargo = &person->dsl_cargo;
  if (cargo->carried)
    w->dsl_lost[cargo->resource - 1] += cargo->carried;
  *cargo = (CdDslCargo){0};
}
