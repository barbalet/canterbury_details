#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include "cd_native.h"
#include <string.h>
#include <stdlib.h>

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

static CdDslStock *batch_slot(CdPremises *site, unsigned resource, uint64_t batch, int vacant) {
  CdDslStock *empty = NULL;
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    CdDslStock *stock = &site->dsl_stock[i];
    if (stock->resource == resource && stock->batch.id == batch)
      return stock;
    if (!empty && (!stock->resource ||
                   (stock->batch.id && !stock->available && !stock->reserved)))
      empty = stock;
  }
  return vacant ? empty : NULL;
}

static CdDslStock *slot(CdPremises *site, unsigned resource, int vacant) {
  return batch_slot(site, resource, 0, vacant);
}

static int expired(const CdWorld *world, const CdDslBatch *batch) {
  return batch->expires && batch->expires <= cd_now(world);
}

int cd_dsl_service_material(CdWorld *world, uint32_t building, const char *id, int consume) {
  unsigned resource = resource_number(id);
  if (!resource)
    return 0;
  CdPremises *premises = &world->premises[building];
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    CdDslStock *stock = &premises->dsl_stock[i];
    if (stock->resource != resource || !stock->available || expired(world, &stock->batch))
      continue;
    if (consume) {
      stock->available--;
      world->dsl_consumed[resource - 1]++;
    }
    return 1;
  }
  return 0;
}

static int equipment(unsigned resource) {
  return !strcmp(cd_dsl_resource_at(resource - 1)->group, "equipment");
}

static int batch_valid(const CdWorld *world, unsigned resource, const CdDslBatch *batch) {
  if (!batch->id)
    return !batch->expires && !batch->quality && !batch->condition && !batch->provenance[0];
  return resource && batch->id <= world->dsl_batch_next && batch->quality <= CD_DSL_GRADE_UNKNOWN &&
         batch->condition <= CD_DSL_GRADE_UNKNOWN &&
         (equipment(resource) || batch->condition == CD_DSL_GRADE_UNKNOWN) &&
         cd_basis_valid(batch->provenance);
}

static uint64_t stored(const CdPremises *site, unsigned resource) {
  uint64_t total = 0;
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i)
    if (site->dsl_stock[i].resource == resource)
      total += site->dsl_stock[i].available + site->dsl_stock[i].reserved;
  return total;
}

CdStatus cd_world_dsl_import_batch(CdWorld *world, CdEntityId id, const char *name,
                                   const char *unit, uint64_t quantity,
                                   const CdDslBatch *details, uint64_t *batch, CdDiagnostic *d) {
  uint32_t b = building_index(world, id);
  unsigned resource = resource_number(name);
  if (!cd_model_ready(world) || b == CD_NONE || !resource || !unit || !quantity ||
      !details || !batch || details->id ||
      strcmp(unit, cd_dsl_resource_at(resource - 1)->unit) ||
      details->quality > CD_DSL_GRADE_UNKNOWN || details->condition > CD_DSL_GRADE_UNKNOWN ||
      (details->condition != CD_DSL_GRADE_UNKNOWN &&
       (!equipment(resource) || quantity != 1)) ||
      expired(world, details) || !cd_basis_valid(details->provenance))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid batch unit, condition, expiry or provenance");
  if (world->dsl_batch_next == UINT64_MAX)
    return cd_diag(d, CD_CAPACITY, "Batch identity limit reached");
  uint64_t identity = world->dsl_batch_next + 1;
  CdPremises *site = &world->premises[b];
  CdDslStock *stock = batch_slot(site, resource, identity, 1);
  if (!stock || quantity > STOCK_LIMIT - stored(site, resource) ||
      quantity > UINT64_MAX - world->dsl_imported[resource - 1] - world->dsl_produced[resource - 1])
    return cd_diag(d, CD_CAPACITY, "Batch storage or ledger limit exceeded");
  *stock = (CdDslStock){.resource = resource, .available = quantity,
      .batch = {.id = identity, .expires = details->expires, .quality = details->quality,
                .condition = details->condition}};
  snprintf(stock->batch.provenance, sizeof stock->batch.provenance, "%s", details->provenance);
  world->dsl_batch_next = identity;
  if (details->expires && (!world->dsl_expiry_next || details->expires < world->dsl_expiry_next))
    world->dsl_expiry_next = details->expires;
  world->dsl_imported[resource - 1] += quantity;
  *batch = identity;
  cd_record_emit(world, CD_EVENT_INVENTORY, (CdPersonRef){0}, (CdPersonRef){0},
                 id, 0, CD_RECORD_DIAGNOSTIC, details->provenance, "Refined batch imported");
  return cd_diag(d, CD_OK, "Refined batch admitted");
}

CdStatus cd_world_dsl_batch(const CdWorld *world, CdEntityId id, const char *name,
                            uint64_t batch, CdDslBatchView *out, CdDiagnostic *d) {
  uint32_t b = building_index(world, id);
  unsigned resource = resource_number(name);
  const CdDslStock *stock = b != CD_NONE && resource && batch
                               ? batch_slot(&world->premises[b], resource, batch, 0) : NULL;
  if (!stock || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown batch at this premises");
  *out = (CdDslBatchView){stock->batch, stock->available, stock->reserved};
  return cd_diag(d, CD_OK, "Refined batch custody");
}

CdStatus cd_world_dsl_stock(const CdWorld *w, CdEntityId id, const char *name,
                            CdDslStockView *out, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (index == CD_NONE || !resource || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown stock premises or resource");
  *out = (CdDslStockView){0};
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    const CdDslStock *stock = &w->premises[index].dsl_stock[i];
    if (stock->resource == resource) {
      out->available += stock->available;
      out->reserved += stock->reserved;
    }
  }
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
  if (!stock || quantity > STOCK_LIMIT - stored(&w->premises[index], resource) ||
      quantity > UINT64_MAX - w->dsl_imported[resource - 1] - w->dsl_produced[resource - 1])
    return cd_diag(d, CD_CAPACITY, "Premises stock or import ledger limit exceeded");
  if (stock->resource != resource || stock->batch.id)
    *stock = (CdDslStock){.resource = resource};
  stock->available += quantity;
  w->dsl_imported[resource - 1] += quantity;
  cd_record_emit(w, CD_EVENT_INVENTORY, (CdPersonRef){0}, (CdPersonRef){0},
                 id, 0, CD_RECORD_DIAGNOSTIC, basis, "Refined resource imported");
  return cd_diag(d, CD_OK, "Refined resource imported");
}

CdStatus cd_world_dsl_batch_reserve(CdWorld *w, CdEntityId id, const char *name,
                              uint64_t batch, uint64_t key, uint64_t quantity, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (!cd_model_ready(w) || index == CD_NONE || !resource || !key || !quantity)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid resource reservation");
  CdDslStock *stock = batch_slot(&w->premises[index], resource, batch, 0);
  if (stock && !stock->work && stock->key == key && stock->reserved == quantity)
    return cd_diag(d, CD_OK, "Existing reservation");
  if (!stock || expired(w, &stock->batch) || stock->available < quantity)
    return cd_diag(d, CD_CAPACITY, "Insufficient available resource");
  if (stock->reserved || key <= stock->key)
    return cd_diag(d, CD_INVALID_STATE, "Outstanding reservation or stale key");
  stock->available -= quantity;
  stock->reserved = quantity;
  stock->key = key;
  stock->settled = 0;
  return cd_diag(d, CD_OK, "Resource reserved");
}

CdStatus cd_world_dsl_batch_settle(CdWorld *w, CdEntityId id, const char *name,
                             uint64_t batch, uint64_t key, int consume, CdDiagnostic *d) {
  uint32_t index = building_index(w, id);
  unsigned resource = resource_number(name);
  if (!cd_model_ready(w) || index == CD_NONE || !resource || !key ||
      (consume != 0 && consume != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid reservation settlement");
  CdDslStock *stock = batch_slot(&w->premises[index], resource, batch, 0);
  unsigned settlement = consume ? 2 : 1;
  if (!stock || stock->work || stock->key != key || (stock->settled && stock->settled != settlement))
    return cd_diag(d, CD_INVALID_STATE, "Reservation key or settlement mismatch");
  if (stock->settled)
    return cd_diag(d, CD_OK, "Existing settlement");
  if (expired(w, &stock->batch) && consume)
    return cd_diag(d, CD_INVALID_STATE, "Expired batch cannot be consumed");
  if (expired(w, &stock->batch))
    w->dsl_lost[resource - 1] += stock->reserved;
  else if (consume)
    w->dsl_consumed[resource - 1] += stock->reserved;
  else
    stock->available += stock->reserved;
  stock->reserved = 0;
  stock->settled = settlement;
  return cd_diag(d, CD_OK, consume ? "Reservation consumed" : "Reservation released");
}

CdStatus cd_world_dsl_reserve(CdWorld *w, CdEntityId id, const char *name,
                              uint64_t key, uint64_t quantity, CdDiagnostic *d) {
  return cd_world_dsl_batch_reserve(w, id, name, 0, key, quantity, d);
}

CdStatus cd_world_dsl_settle(CdWorld *w, CdEntityId id, const char *name,
                             uint64_t key, int consume, CdDiagnostic *d) {
  return cd_world_dsl_batch_settle(w, id, name, 0, key, consume, d);
}

CdStatus cd_world_dsl_batch_wear(CdWorld *w, CdEntityId id, const char *name,
                                 uint64_t batch, unsigned amount, CdDiagnostic *d) {
  uint32_t b = building_index(w, id);
  unsigned resource = resource_number(name);
  CdDslStock *stock = b != CD_NONE && resource && batch
                          ? batch_slot(&w->premises[b], resource, batch, 0) : NULL;
  if (!cd_model_ready(w) || !stock || !equipment(resource) || !amount ||
      stock->available != 1 || stock->reserved || stock->batch.condition > 1000 ||
      expired(w, &stock->batch))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Wear requires one available condition-rated item");
  stock->batch.condition = amount < stock->batch.condition ? stock->batch.condition - amount : 0;
  return cd_diag(d, CD_OK, "Equipment condition reduced");
}

int cd_dsl_stock_valid(const CdWorld *w) {
  uint64_t totals[CD_DSL_RESOURCE_LIMIT] = {0};
  for (uint32_t b = 0; b < w->content->np; ++b) {
    uint64_t held[CD_DSL_RESOURCE_LIMIT] = {0};
    for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
      const CdDslStock *stock = &w->premises[b].dsl_stock[i];
      if (!stock->resource) {
        if (stock->available || stock->reserved || stock->key || stock->settled || stock->work || !batch_valid(w, 0, &stock->batch))
          return 0;
        continue;
      }
      if (w->content->polygons[b].kind != CD_BUILDING ||
          stock->resource > cd_dsl_resource_count() || stock->settled > 2 ||
          !batch_valid(w, stock->resource, &stock->batch) ||
          (stock->available && stock->batch.expires &&
           (!w->dsl_expiry_next || w->dsl_expiry_next > stock->batch.expires ||
            expired(w, &stock->batch))) ||
          (stock->work && !stock->reserved) ||
          (stock->reserved && !stock->work && (!stock->key || stock->settled)) ||
          (stock->key && (!stock->reserved || stock->work) && !stock->settled) ||
          (stock->settled && !stock->key) || stock->available > STOCK_LIMIT ||
          stock->reserved > STOCK_LIMIT - stock->available)
        return 0;
      unsigned r = stock->resource - 1;
      for (size_t prior = 0; prior < i; ++prior)
        if (w->premises[b].dsl_stock[prior].resource == stock->resource &&
            w->premises[b].dsl_stock[prior].batch.id == stock->batch.id)
          return 0;
      held[r] += stock->available + stock->reserved;
      if (held[r] > STOCK_LIMIT || stock->available + stock->reserved > UINT64_MAX - totals[r])
        return 0;
      totals[r] += stock->available + stock->reserved;
    }
  }
  for (size_t i = 0; i < w->count; ++i) {
    const CdDslCargo *cargo = &w->people[i].dsl_cargo;
    if (cargo->resource > cd_dsl_resource_count() ||
        cargo->requested > cd_dsl_resource_count() ||
        !!cargo->resource != !!cargo->carried ||
        (cargo->batch.id && !cargo->resource) ||
        !batch_valid(w, cargo->resource, &cargo->batch) ||
        (cargo->carried && cargo->batch.expires &&
         (!w->dsl_expiry_next || w->dsl_expiry_next > cargo->batch.expires ||
          expired(w, &cargo->batch))) ||
        cargo->requested_batch > w->dsl_batch_next ||
        (cargo->requested_batch && !cargo->quantity) ||
        (cargo->order_key &&
         ((!cargo->order_batch && !cargo->order_price) || cargo->order_batch > w->dsl_batch_next ||
          !cargo->order_resource || cargo->order_resource > cd_dsl_resource_count() ||
          !cargo->order_quantity || cargo->order_quantity > carry_capacity(cargo->order_resource) ||
          cargo->order_delivery > 1 || building_index(w, cargo->order_premises) == CD_NONE)) ||
        (!cargo->order_key && (cargo->order_batch || cargo->order_resource ||
                               cargo->order_quantity || cargo->order_delivery || cargo->order_premises ||
                               cargo->order_price)) ||
        cargo->seller_job > w->job_count || cargo->order_seller_job > w->job_count ||
        (cargo->seller_job && (!cargo->price || cargo->seller_job != cargo->order_seller_job)) ||
        (cargo->order_seller_job && !cargo->order_price) ||
        (cargo->price && (!cargo->quantity || cargo->delivery || cargo->price != cargo->order_price)) ||
        cargo->eating > 1 || (cargo->eating && (!cargo->quantity || !cargo->delivery ||
          !cargo->resource || !cd_dsl_resource_at(cargo->resource - 1)->food_energy_per_unit)) ||
        cargo->carried > carry_capacity(cargo->resource) ||
        cargo->delivery > 1 || !!cargo->quantity != !!cargo->requested ||
        !!cargo->quantity != !!cargo->premises ||
        (cargo->quantity &&
         (building_index(w, cargo->premises) == CD_NONE ||
          cargo->quantity > carry_capacity(cargo->requested) ||
          (cargo->resource && (cargo->resource != cargo->requested ||
                               cargo->batch.id != cargo->requested_batch)) ||
          (cargo->delivery && cargo->quantity > cargo->carried) ||
          (!cargo->delivery && cargo->carried + cargo->quantity >
                                    carry_capacity(cargo->requested)))))
      return 0;
    if (cargo->carried) {
      if (cargo->carried > UINT64_MAX - totals[cargo->resource - 1])
        return 0;
      totals[cargo->resource - 1] += cargo->carried;
    }
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

static CdDslStock *work_slot(CdPremises *site, unsigned resource, uint64_t work) {
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    CdDslStock *stock = &site->dsl_stock[i];
    if (stock->resource == resource && stock->work == work)
      return stock;
  }
  return NULL;
}

static CdDslStock *work_hold(CdWorld *world, CdPremises *site, unsigned resource,
                             uint64_t amount, uint64_t work, CdDslWorkAction action) {
  if (action != CD_DSL_HOLD) {
    CdDslStock *stock = work_slot(site, resource, work);
    return stock && stock->reserved == amount ? stock : NULL;
  }
  CdDslStock *selected = NULL;
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    CdDslStock *stock = &site->dsl_stock[i];
    if (stock->resource != resource || stock->reserved || stock->available < amount ||
        expired(world, &stock->batch) || (equipment(resource) && stock->batch.id &&
                                         !stock->batch.condition))
      continue;
    uint64_t expires = stock->batch.expires ? stock->batch.expires : UINT64_MAX;
    uint64_t current = selected && selected->batch.expires ? selected->batch.expires : UINT64_MAX;
    if (!selected || expires < current)
      selected = stock;
  }
  if (selected) {
    selected->available -= amount;
    selected->reserved = amount;
    selected->work = work;
  }
  return selected;
}

int cd_dsl_work_inputs_current(const CdWorld *world, uint32_t building, uint64_t work) {
  for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
    const CdDslStock *stock = &world->premises[building].dsl_stock[i];
    if (stock->work == work && expired(world, &stock->batch))
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
  uint64_t lost[CD_DSL_RESOURCE_LIMIT] = {0};
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
    CdDslStock *stock = work_hold(w, &candidate, resource, amount, task->view.id, action);
    if (!stock)
      return 0;
    if (action != CD_DSL_HOLD) {
      if (expired(w, &stock->batch)) {
        if (action != CD_DSL_RETURN)
          return 0;
        lost[resource - 1] += amount;
      } else if (input && action != CD_DSL_RETURN)
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
      if (!stock || amount > STOCK_LIMIT - stored(&candidate, resource) + freed ||
          amount > UINT64_MAX - w->dsl_imported[resource - 1] -
                                  w->dsl_produced[resource - 1])
        return 0;
      if (stock->resource != resource || stock->batch.id)
        *stock = (CdDslStock){.resource = resource};
      if (action != CD_DSL_HOLD) {
        stock->available += amount;
        produced[resource - 1] = amount;
      }
    }
  if (action == CD_DSL_CHECK_OUTPUT)
    return 1;
  w->premises[b] = candidate;
  if (action == CD_DSL_COMPLETE || action == CD_DSL_RETURN)
    for (size_t i = 0; i < CD_DSL_RESOURCE_LIMIT; ++i) {
      w->dsl_consumed[i] += consumed[i];
      w->dsl_produced[i] += produced[i];
      w->dsl_lost[i] += lost[i];
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
    const CdDslStock *stock = work_slot(&w->premises[b], resource, task->view.id);
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

static int retail_goods(const CdDslRole *role, unsigned resource) {
  const CdDslResource *kind = cd_dsl_resource_at(resource - 1);
  if (!role || !kind || role->stage > 3)
    return 0;
  if (!strcmp(role->career, "grocer"))
    return !strncmp(kind->group, "food_", 5);
  if (!strcmp(role->career, "draper"))
    return !strcmp(kind->group, "textile") || !strcmp(kind->id, "sewn_garment");
  if (!strcmp(role->career, "ironmonger"))
    return !strcmp(kind->group, "metal") || !strcmp(kind->id, "hand_tool");
  if (!strcmp(role->career, "bookseller"))
    return !strcmp(kind->group, "paper") || !strcmp(kind->id, "school_book");
  if (!strcmp(role->career, "coal_merchant"))
    return !strcmp(kind->group, "fuel") && strcmp(kind->id, "ash");
  return 0;
}

static CdCitizen *retail_clerk(CdWorld *w, CdCitizen *buyer, CdJob *job,
                               uint32_t building, unsigned resource, CdCitizen **mentor) {
  if (!job || job->view.workplace != w->content->polygons[building].id ||
      !retail_goods(cd_dsl_job_role(job->view.occupation), resource))
    return NULL;
  CdCitizen *clerk = cd_citizen(w, job->view.worker);
  if (!clerk || clerk == buyer || clerk->inside != building || clerk->door != CD_NONE ||
      clerk->health.severity >= 600 ||
      !cd_job_qualified(w, job, clerk) || clerk->activity != CD_ACTIVITY_IDLE ||
      !cd_native_can_work(w->native, clerk->civil.ref))
    return NULL;
  CdCivilTime clock;
  if (cd_smoke_civil_time(cd_now(w), &clock, NULL) != CD_OK ||
      !cd_shift(job->view.shift_start, job->view.shift_end, clock.minute_of_day))
    return NULL;
  *mentor = NULL;
  if (job->view.apprenticeship) {
    CdCitizen *teacher = cd_citizen(w, job->view.mentor);
    if (!teacher || teacher == buyer || !cargo_present(w, teacher, building) ||
        teacher->activity != CD_ACTIVITY_IDLE || !cd_native_can_work(w->native, teacher->civil.ref))
      return NULL;
    int teaching = 0;
    for (unsigned slot = 0; slot < 2; ++slot) {
      CdJob *post = cd_job(w, teacher->jobs[slot]);
      if (cd_job_mentor_matches(job, post) &&
          cd_shift(post->view.shift_start, post->view.shift_end, clock.minute_of_day))
        teaching = 1;
    }
    if (!teaching)
      return NULL;
    *mentor = teacher;
  }
  return clerk;
}

static CdStatus cargo_order(CdWorld *w, CdPersonRef ref, CdEntityId premises,
                            const char *name, uint64_t batch, uint64_t key,
                            uint64_t quantity, int delivery, uint64_t price, CdEntityId seller_job, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(w, ref);
  unsigned resource = resource_number(name);
  uint32_t building = building_index(w, premises);
  if (!cd_model_ready(w) || !person || !quantity || !resource ||
      quantity > carry_capacity(resource) || (delivery != 0 && delivery != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid cargo request");
  CdDslCargo *cargo = &person->dsl_cargo;
  if (key && key == cargo->order_key && premises == cargo->order_premises &&
      resource == cargo->order_resource && batch == cargo->order_batch &&
      quantity == cargo->order_quantity && (unsigned)delivery == cargo->order_delivery &&
      price == cargo->order_price && seller_job == cargo->order_seller_job)
    return cd_diag(d, CD_OK, "Existing cargo request");
  if (!cargo_present(w, person, building))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Cargo requires an eligible adult inside the premises");
  if ((key && key <= cargo->order_key) || cargo->quantity ||
      (cargo->resource && (cargo->resource != resource || cargo->batch.id != batch)) ||
      (delivery ? quantity > cargo->carried :
                  quantity + cargo->carried > carry_capacity(resource)))
    return cd_diag(d, CD_INVALID_STATE, "Stale cargo key, outstanding action or incompatible load");
  if ((batch || price) && !delivery) {
    CdDslStock *stock = batch_slot(&w->premises[building], resource, batch, 0);
    if (!stock || expired(w, &stock->batch) || stock->available < quantity)
      return cd_diag(d, CD_CAPACITY, "Available batch cargo is insufficient");
  }
  if (price && (!w->premises[building].employer || w->premises[building].closed ||
                person->wallet < price))
    return cd_diag(d, CD_CAPACITY, "Purchase requires an open employer and funded buyer");
  if (key) {
    cargo->order_price = price;
    cargo->order_seller_job = seller_job;
    cargo->order_key = key;
    cargo->order_premises = premises;
    cargo->order_resource = resource;
    cargo->order_batch = batch;
    cargo->order_quantity = quantity;
    cargo->order_delivery = (unsigned)delivery;
  }
  cargo->requested_batch = batch;
  cargo->price = price;
  cargo->seller_job = seller_job;
  cargo->premises = premises;
  cargo->quantity = quantity;
  cargo->requested = resource;
  cargo->delivery = (unsigned)delivery;
  return cd_diag(d, CD_OK, "Cargo action awaits a native minute");
}

CdStatus cd_world_dsl_cargo_order(CdWorld *w, CdPersonRef ref, CdEntityId premises,
                                  const char *name, uint64_t quantity,
                                  int delivery, CdDiagnostic *d) {
  return cargo_order(w, ref, premises, name, 0, 0, quantity, delivery, 0, 0, d);
}

CdStatus cd_world_dsl_batch_cargo_order(CdWorld *w, CdPersonRef ref, CdEntityId premises,
                                        const char *name, uint64_t batch, uint64_t key,
                                        uint64_t quantity, int delivery, CdDiagnostic *d) {
  if (!batch || !key)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Batch cargo requires a batch and transfer key");
  return cargo_order(w, ref, premises, name, batch, key, quantity, delivery, 0, 0, d);
}

CdStatus cd_world_dsl_purchase(CdWorld *world, CdPersonRef buyer, CdEntityId shop,
                                const char *name, uint64_t batch, uint64_t key,
                                uint64_t quantity, uint64_t price, CdDiagnostic *d) {
  if (!key || !price)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Purchase requires a price and transfer key");
  return cargo_order(world, buyer, shop, name, batch, key, quantity, 0, price, 0, d);
}

CdStatus cd_world_dsl_purchase_at_post(CdWorld *w, CdPersonRef buyer, CdEntityId shop,
                                        CdEntityId post, const char *name, uint64_t batch,
                                        uint64_t key, uint64_t quantity, uint64_t price,
                                        CdDiagnostic *d) {
  CdJob *job = cd_job(w, post);
  unsigned resource = resource_number(name);
  if (!key || !price || !resource || !job || job->view.workplace != shop ||
      !retail_goods(cd_dsl_job_role(job->view.occupation), resource))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Purchase requires a matching named retail post");
  return cargo_order(w, buyer, shop, name, batch, key, quantity, 0, price, post, d);
}

CdStatus cd_world_dsl_eat_carried(CdWorld *world, CdPersonRef ref, CdEntityId premises,
                                   uint64_t quantity, CdDiagnostic *d) {
  CdCitizen *person = cd_citizen(world, ref);
  const CdDslResource *kind = person && person->dsl_cargo.resource
      ? cd_dsl_resource_at(person->dsl_cargo.resource - 1) : NULL;
  if (!kind || !kind->food_energy_per_unit)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Carried resource has no configured food energy");
  CdStatus status = cargo_order(world, ref, premises, kind->id, person->dsl_cargo.batch.id,
                                0, quantity, 1, 0, 0, d);
  if (status == CD_OK)
    person->dsl_cargo.eating = 1;
  return status;
}

static void clear_order(CdDslCargo *cargo) {
  cargo->premises = cargo->quantity = cargo->requested_batch = 0;
  cargo->requested = cargo->delivery = cargo->eating = 0;
  cargo->price = 0;
  cargo->seller_job = 0;
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
      cargo->carried, cargo->quantity, cargo->batch};
  return cd_diag(d, CD_OK, "Refined cargo custody");
}

void cd_dsl_cargo_tick(CdWorld *w, CdCitizen *person) {
  CdDslCargo *cargo = &person->dsl_cargo;
  if (!cargo->quantity || person->activity != CD_ACTIVITY_IDLE)
    return;
  uint32_t building = building_index(w, cargo->premises);
  if (!cargo_present(w, person, building))
    return;
  if (cargo->eating) {
    const CdDslResource *kind = cd_dsl_resource_at(cargo->resource - 1);
    int energy = cd_native_energy(w->native, person->civil.ref);
    if (expired(w, &cargo->batch) || energy >= (int)cd_native_full() ||
        !cd_native_claim(w->native, person->civil.ref))
      return;
    uint64_t gain = cargo->quantity * kind->food_energy_per_unit;
    cd_native_energy_set(w->native, person->civil.ref, energy + (int)gain);
    w->dsl_consumed[cargo->resource - 1] += cargo->quantity;
    cargo->carried -= cargo->quantity;
    if (!cargo->carried) {
      cargo->resource = 0;
      cargo->batch = (CdDslBatch){0};
    }
    cd_native_remember_food(w->native, person->civil.ref,
                            cd_native_energy(w->native, person->civil.ref) - energy);
    cd_record_emit(w, CD_EVENT_EAT, person->civil.ref, (CdPersonRef){0},
                   cargo->premises, 0, CD_RECORD_PERSONAL, "configured refined food energy",
                   "Ate accounted carried food");
    clear_order(cargo);
    person->activity = CD_ACTIVITY_MEAL;
    return;
  }
  CdPremises *site = &w->premises[building];
  CdDslStock *stock = batch_slot(site, cargo->requested, cargo->requested_batch, cargo->delivery);
  if (!cargo->delivery && cargo->requested_batch &&
      (!stock || expired(w, &stock->batch))) {
    clear_order(cargo);
    return;
  }
  if (!stock || (cargo->delivery
                     ? expired(w, &cargo->batch) ||
                           cargo->quantity > STOCK_LIMIT - stored(site, cargo->requested)
                     : expired(w, &stock->batch) || cargo->quantity > stock->available))
    return;
  if (cargo->price && (!site->employer || site->closed || person->wallet < cargo->price))
    return;
  CdCitizen *clerk = NULL, *mentor = NULL;
  if (cargo->seller_job) {
    clerk = retail_clerk(w, person, cd_job(w, cargo->seller_job), building,
                         cargo->requested, &mentor);
    if (!clerk)
      return;
  }
  CdJob *sale = cd_job(w, cargo->seller_job);
  unsigned wage = sale && !sale->attendance_sector ? sale->view.wage : 0;
  if (site->treasury + cargo->price < wage)
    return;
  if (!cd_native_claim(w->native, person->civil.ref))
    return;
  if (clerk) {
    if (!cd_native_claim(w->native, clerk->civil.ref) ||
        (mentor && !cd_native_claim(w->native, mentor->civil.ref)))
      abort();
    clerk->activity = CD_ACTIVITY_WORK;
    if (!sale->attendance_sector)
      cd_dsl_record_experience(clerk, cd_dsl_job_role(sale->view.occupation), 1);
    if (mentor)
      mentor->activity = CD_ACTIVITY_WORK;
  }
  if (cargo->delivery) {
    if (stock->resource != cargo->requested || stock->batch.id != cargo->batch.id)
      *stock = (CdDslStock){.resource = cargo->requested};
    stock->batch = cargo->batch;
    stock->available += cargo->quantity;
    cargo->carried -= cargo->quantity;
    if (!cargo->carried) {
      cargo->resource = 0;
      cargo->batch = (CdDslBatch){0};
    }
  } else {
    if (cargo->price) {
      person->wallet -= cargo->price;
      site->treasury += cargo->price;
      if (clerk && wage) {
        site->treasury -= wage;
        clerk->wallet += wage;
        w->economy.wages += wage;
      }
    }
    stock->available -= cargo->quantity;
    cargo->resource = cargo->requested;
    cargo->batch = stock->batch;
    cargo->carried += cargo->quantity;
  }
  cd_record_emit(w, CD_EVENT_INVENTORY, person->civil.ref, (CdPersonRef){0},
                 cargo->premises, 0, CD_RECORD_DIAGNOSTIC, "native hand cargo",
                 cargo->delivery ? "Refined cargo delivered" :
                 cargo->price ? "Refined stock purchased" : "Refined cargo picked up");
  clear_order(cargo);
  person->activity = CD_ACTIVITY_WORK;
}

static void consider_expiry(CdWorld *world, const CdDslBatch *batch) {
  if (batch->expires && (!world->dsl_expiry_next || batch->expires < world->dsl_expiry_next))
    world->dsl_expiry_next = batch->expires;
}

void cd_dsl_expiry_tick(CdWorld *world) {
  if (!world->dsl_expiry_next || world->dsl_expiry_next > cd_now(world))
    return;
  world->dsl_expiry_next = 0;
  for (uint32_t b = 0; b < world->content->np; ++b) {
    for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
      CdDslStock *stock = &world->premises[b].dsl_stock[i];
      if (!stock->available)
        continue;
      if (expired(world, &stock->batch)) {
        world->dsl_lost[stock->resource - 1] += stock->available;
        stock->available = 0;
      } else {
        consider_expiry(world, &stock->batch);
      }
    }
  }
  for (size_t i = 0; i < world->count; ++i) {
    CdDslCargo *cargo = &world->people[i].dsl_cargo;
    if (!cargo->carried)
      continue;
    if (expired(world, &cargo->batch)) {
      world->dsl_lost[cargo->resource - 1] += cargo->carried;
      cargo->carried = cargo->resource = 0;
      cargo->batch = (CdDslBatch){0};
      clear_order(cargo);
    } else {
      consider_expiry(world, &cargo->batch);
    }
  }
}

void cd_dsl_cargo_retire(CdWorld *w, CdCitizen *person) {
  CdDslCargo *cargo = &person->dsl_cargo;
  if (cargo->carried)
    w->dsl_lost[cargo->resource - 1] += cargo->carried;
  *cargo = (CdDslCargo){0};
}
