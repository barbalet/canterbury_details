#include "cd_city_internal.h"
#include "cd_native.h"
static int endpoint(CdWorld *w, CdFoodEndpoint e, unsigned **meals,
                    unsigned **raw, uint32_t *building) {
  if (e.kind == CD_FOOD_HOME) {
    if (!e.id || e.id > w->home_count)
      return 0;
    *meals = &w->home_meals[e.id - 1];
    *raw = &w->home_raw[e.id - 1];
    *building = w->homes[e.id - 1].dwelling;
    return 1;
  }
  if (e.kind != CD_FOOD_STORE)
    return 0;
  uint32_t b = cd_polygon_index(w->content, e.id);
  if (b == CD_NONE || !w->premises[b].enabled)
    return 0;
  *meals = &w->premises[b].meals;
  *raw = &w->premises[b].raw;
  *building = b;
  return 1;
}
CdStatus cd_world_food_store(CdWorld *w, CdEntityId id, int producer,
                             const char *basis, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      w->content->polygons[b].kind != CD_BUILDING ||
      (producer != 0 && producer != 1) || !cd_basis_valid(basis))
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Store requires source building and explicit technical basis");
  w->premises[b].enabled = 1;
  w->premises[b].producer = producer;
  snprintf(w->premises[b].food_basis, 96, "%s", basis);
  return cd_diag(
      d, CD_OK,
      "Configured finite food store/producer, not historical identity");
}
CdStatus cd_world_food_import(CdWorld *w, CdFoodEndpoint e, unsigned meals,
                              unsigned raw, const char *basis,
                              CdDiagnostic *d) {
  unsigned *m, *r;
  uint32_t b;
  if (!cd_model_ready(w) || !cd_basis_valid(basis) ||
      !endpoint(w, e, &m, &r, &b) || !((uint64_t)meals + raw) || meals > 4096 ||
      raw > 4096 ||
      (uint64_t)*m + meals +
              (e.kind == CD_FOOD_STORE ? w->premises[b].reserved_meals : 0) >
          4096 ||
      (uint64_t)*r + raw +
              (e.kind == CD_FOOD_STORE ? w->premises[b].reserved_raw : 0) >
          4096 ||
      UINT64_MAX - w->food.imported_meals < meals ||
      UINT64_MAX - w->food.imported_raw < raw)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Invalid or over-capacity explicitly imported food");
  *m += meals;
  *r += raw;
  w->food.imported_meals += meals;
  w->food.imported_raw += raw;
  return cd_diag(d, CD_OK,
                 "Scenario import recorded; no automatic replenishment");
}
CdStatus cd_world_food_order(CdWorld *w, CdPersonRef ref, CdFoodAction action,
                             CdFoodEndpoint e, unsigned units,
                             CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  unsigned *m, *r;
  uint32_t b;
  if (!p || !cd_model_ready(w) || (unsigned)action > CD_PRODUCE_MEALS ||
      !units || units > 4096 || !endpoint(w, e, &m, &r, &b))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid bounded food order");
  if (p->civil.age < 18 || p->health.severity >= 600 ||
      p->health.mortality_pending)
    return cd_diag(d, CD_INVALID_STATE,
                   "Food logistics requires a capable adult");
  if (!p->civil.position_known || b == CD_NONE || p->inside != b ||
      p->door != CD_NONE)
    return cd_diag(
        d, CD_INVALID_STATE,
        "Physically reach the food endpoint through a reviewed entrance first");
  if (e.kind == CD_FOOD_HOME && p->civil.household != e.id)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Cannot withdraw another household's food");
  if (action == CD_PRODUCE_MEALS &&
      (e.kind != CD_FOOD_STORE || !w->premises[b].producer ||
       !(p->skills & CD_SKILL_FOOD)))
    return cd_diag(d, CD_INVALID_STATE,
                   "Producer work requires configured premises and explicit "
                   "food qualification");
  if (p->food_remaining)
    return cd_diag(d, CD_INVALID_STATE,
                   "Existing order must complete or be cancelled");
  p->food_action = action;
  p->food_endpoint = e;
  p->food_remaining = units;
  return cd_diag(
      d, CD_OK,
      "Food order queued; one physical, shared-grant unit per minute");
}
CdStatus cd_world_food_cancel(CdWorld *w, CdPersonRef ref, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w))
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown food actor");
  p->food_remaining = 0;
  p->feed_target = (CdPersonRef){0};
  return cd_diag(d, CD_OK,
                 "Unperformed food units cancelled; committed units retained");
}
static int target_food(CdWorld *w, CdCitizen *p) {
  if (!cd_ration_available(w, p))
    return 0;
  if (p->meals)
    return 1;
  if (p->civil.household && p->inside != CD_NONE && p->door == CD_NONE &&
      p->inside == w->homes[p->civil.household - 1].dwelling &&
      w->home_meals[p->civil.household - 1]) {
    w->home_meals[p->civil.household - 1]--;
    p->meals++;
    return 1;
  }
  return 0;
}
CdStatus cd_world_feed(CdWorld *w, CdPersonRef ar, CdPersonRef pr,
                       CdDiagnostic *d) {
  CdCitizen *a = cd_citizen(w, ar), *p = cd_citizen(w, pr);
  if (!a || !p || !cd_model_ready(w) || a == p || a->civil.age < 18 ||
      a->health.severity >= 600 || a->health.mortality_pending ||
      p->health.mortality_pending || !cd_colocated(w, a, p) ||
      (p->civil.age < 18 && p->civil.guardian.person_id != ar.person_id))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Feeding requires a capable, co-located authorized adult");
  if (p->meals >= 12)
    return cd_diag(d, CD_CAPACITY, "Dependent carried-meal capacity full");
  if (!a->meals)
    return cd_diag(d, CD_INVALID_STATE, "Carer has no finite meal to transfer");
  if (a->feed_target.person_id)
    return cd_diag(d, CD_INVALID_STATE, "Feeding intention already pending");
  a->feed_target = pr;
  return cd_diag(d, CD_OK,
                 "Physical meal transfer queued for one shared native minute");
}
void cd_food_tick(CdWorld *w) {
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (p->feed_target.person_id && p->activity == CD_ACTIVITY_IDLE) {
      CdCitizen *target = cd_citizen(w, p->feed_target);
      if (!target || p->civil.age < 18 || p->health.severity >= 600 ||
          p->health.mortality_pending || target->health.mortality_pending ||
          !cd_colocated(w, p, target) ||
          (target->civil.age < 18 &&
           target->civil.guardian.person_id != p->civil.ref.person_id)) {
        p->feed_target = (CdPersonRef){0};
      } else if (p->meals && target->meals < 12 &&
                 cd_native_claim(w->native, p->civil.ref)) {
        p->meals--;
        target->meals++;
        p->feed_target = (CdPersonRef){0};
        p->activity = CD_ACTIVITY_CARE;
      }
    }
    if (!p->food_remaining || p->activity != CD_ACTIVITY_IDLE)
      continue;
    unsigned *m, *r;
    uint32_t b;
    if (p->civil.age < 18 || p->health.severity >= 600 ||
        p->health.mortality_pending ||
        !endpoint(w, p->food_endpoint, &m, &r, &b) ||
        (p->food_endpoint.kind == CD_FOOD_HOME &&
         p->civil.household != p->food_endpoint.id) ||
        !p->civil.position_known || b == CD_NONE || p->inside != b ||
        p->door != CD_NONE || !cd_usable(w, b)) {
      p->activity = CD_ACTIVITY_WAIT;
      continue;
    }
    int available = 0;
    switch (p->food_action) {
    case CD_FETCH_MEALS:
      available = *m && p->meals < 12;
      break;
    case CD_DELIVER_MEALS:
      available = p->meals && *m + (p->food_endpoint.kind == CD_FOOD_STORE
                                        ? w->premises[b].reserved_meals
                                        : 0) <
                                  4096;
      break;
    case CD_FETCH_RAW:
      available = *r && p->raw < 12;
      break;
    case CD_DELIVER_RAW:
      available = p->raw && *r + (p->food_endpoint.kind == CD_FOOD_STORE
                                      ? w->premises[b].reserved_raw
                                      : 0) <
                                4096;
      break;
    case CD_PRODUCE_MEALS:
      available = p->food_endpoint.kind == CD_FOOD_STORE &&
                  w->premises[b].producer && (p->skills & CD_SKILL_FOOD) &&
                  *r &&
                  *m + (p->food_endpoint.kind == CD_FOOD_STORE
                            ? w->premises[b].reserved_meals
                            : 0) <
                      4096 &&
                  w->food.produced < UINT64_MAX;
      break;
    }
    if (!available) {
      p->activity = CD_ACTIVITY_WAIT;
      continue;
    }
    if (!cd_native_claim(w->native, p->civil.ref)) {
      p->activity = CD_ACTIVITY_WAIT;
      continue;
    }
    switch (p->food_action) {
    case CD_FETCH_MEALS:
      (*m)--;
      p->meals++;
      break;
    case CD_DELIVER_MEALS:
      p->meals--;
      (*m)++;
      break;
    case CD_FETCH_RAW:
      (*r)--;
      p->raw++;
      break;
    case CD_DELIVER_RAW:
      p->raw--;
      (*r)++;
      break;
    case CD_PRODUCE_MEALS:
      (*r)--;
      (*m)++;
      w->food.produced++;
      break;
    }
    p->food_remaining--;
    p->activity = CD_ACTIVITY_WORK;
  }
}
CdStatus cd_world_food_ledger(const CdWorld *w, CdFoodLedger *out,
                              CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing food ledger output");
  CdFoodLedger f = w->food;
  f.meal_stock = f.raw_stock = f.cached_meals = f.cached_raw = 0;
  for (size_t i = 0; i < w->life_count; i++) {
    f.cached_meals += w->life[i].meals;
    f.cached_raw += w->life[i].raw;
  }
  f.meal_stock = f.cached_meals;
  f.raw_stock = f.cached_raw;
  for (size_t i = 0; i < w->count; i++) {
    f.meal_stock += w->people[i].meals;
    f.raw_stock += w->people[i].raw;
  }
  for (size_t i = 0; i < w->home_count; i++) {
    f.meal_stock += w->home_meals[i];
    f.raw_stock += w->home_raw[i];
  }
  for (uint32_t i = 0; i < w->content->np; i++) {
    f.meal_stock += w->premises[i].meals + w->premises[i].reserved_meals;
    f.raw_stock += w->premises[i].raw + w->premises[i].reserved_raw;
  }
  for (size_t i = 0; i < w->vehicle_count; i++)
    if (w->vehicles[i].view.meals)
      f.meal_stock += w->vehicles[i].view.loaded;
  if (f.initial_meals + f.imported_meals + f.produced !=
          f.meal_stock + f.consumed + f.exported_meals ||
      f.imported_raw != f.raw_stock + f.produced + f.exported_raw)
    return cd_diag(d, CD_INVALID_STATE, "Food conservation mismatch");
  *out = f;
  return cd_diag(d, CD_OK, "Finite meal/raw ledgers reconcile");
}
/* Household withdrawal for one's own need does not grant work or create food.
 */
int cd_food_personal(CdWorld *w, CdCitizen *p) { return target_food(w, p); }
