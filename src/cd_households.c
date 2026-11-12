#include "cd_city_internal.h"
static CdHousehold *home(CdWorld *w, CdEntityId id) {
  return w && id && id <= w->home_count ? &w->homes[id - 1] : NULL;
}
int cd_housing_transient(const CdWorld *w, const CdCitizen *p, uint32_t b) {
  CdEntityId id = w->content->polygons[b].id;
  return p->civil.dwelling != id && p->presence != id &&
         (p->inside == b ||
          (p->door != CD_NONE && w->content->doors[p->door].building == b &&
           p->entering));
}
size_t cd_housing_load(const CdWorld *w, uint32_t b) {
  size_t load = (size_t)w->housing[b] + w->visitors[b];
  for (size_t i = 0; i < w->count; i++)
    load += cd_housing_transient(w, &w->people[i], b);
  return load;
}
static int adult(CdWorld *w, CdCitizen *p) {
  CdResidentView view;
  cd_population_view(w, p, &view);
  return view.age >= 18;
}
CdStatus cd_housing_init(CdWorld *w) {
  w->home_count = (w->count + 3) / 4;
  for (size_t i = 0; i < w->home_count; i++) {
    w->homes[i] = (CdHousehold){i + 1, CD_NONE, 0, 0};
  }
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    CdHousehold *h = &w->homes[i / 4];
    p->civil.household = h->id;
    h->members++;
    if (p->civil.age < 18)
      p->civil.guardian = w->people[i - i % 4].civil.ref;
  }
  for (size_t i = 0; i + 1 < w->count; i += 4) {
    w->people[i].civil.spouse = w->people[i + 1].civil.ref;
    w->people[i + 1].civil.spouse = w->people[i].civil.ref;
  }
  return CD_OK;
}
CdStatus cd_world_households_allocate(CdWorld *w, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !w->home_count)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Housing allocation requires a live population");
  for (size_t h = 0; h < w->home_count; ++h)
    if (w->homes[h].housing_known || w->homes[h].dwelling != CD_NONE)
      return cd_diag(d, CD_INVALID_STATE, "Housing allocation requires fresh households");
  for (size_t i = 0; i < w->count; ++i)
    if (w->people[i].civil.position_known || w->people[i].presence)
      return cd_diag(d, CD_INVALID_STATE, "Housing allocation precedes initial physical placement");

  uint32_t *selected = cd_memory_calloc(w->home_count, sizeof *selected, CD_MEM_TEMP);
  uint32_t *capacity = cd_memory_calloc(w->content->np, sizeof *capacity, CD_MEM_TEMP);
  CdStatus status = CD_CAPACITY;
  const char *message = "Cannot stage housing allocation";
  if (!selected || !capacity)
    goto done;

  for (uint32_t b = 0; b < w->content->np; ++b) {
    const CdPolygon *building = &w->content->polygons[b];
    const char *use = w->premises[b].scenario_use[0] ?
        w->premises[b].scenario_use : building->use;
    /* Mixed-use premises require a separately declared residential capacity. */
    if (building->kind != CD_BUILDING || !building->capacity_known ||
        strcmp(use, "residential") || !cd_usable(w, b))
      continue;
    uint32_t door = cd_reviewed_door(w, building->id);
    if (door == CD_NONE)
      continue;
    uint32_t node = w->content->doors[door].node;
    for (uint32_t i = w->content->adj_offset[node];
         i < w->content->adj_offset[node + 1]; ++i) {
      uint32_t edge = w->content->adj_edges[i];
      if (w->content->edges[edge].qualified && !w->closed[edge]) {
        capacity[b] = cd_capacity(w, b);
        break;
      }
    }
  }
  for (size_t h = 0; h < w->home_count; ++h) {
    uint32_t b = 0;
    unsigned members = w->homes[h].members;
    if (!members) {
      selected[h] = CD_NONE;
      continue;
    }
    while (b < w->content->np && capacity[b] < members)
      ++b;
    if (b == w->content->np) {
      message = "Insufficient reviewed residential capacity connected to the walking network";
      goto done;
    }
    selected[h] = b;
    capacity[b] -= members;
  }

  for (size_t h = 0; h < w->home_count; ++h) {
    CdHousehold *household = &w->homes[h];
    if (selected[h] == CD_NONE)
      continue;
    household->dwelling = selected[h];
    household->housing_known = 1;
    w->housing[selected[h]] += household->members;
  }
  for (size_t i = 0; i < w->count; ++i) {
    CdCitizen *person = &w->people[i];
    person->civil.dwelling =
        w->content->polygons[selected[person->civil.household - 1]].id;
  }
  cd_record_emit(w, CD_EVENT_HOUSEHOLD, (CdPersonRef){0}, (CdPersonRef){0},
                 w->home_count, 0, CD_RECORD_PUBLIC, "reviewed residential capacity",
                 "Initial household allocation committed");
  status = CD_OK;
  message = "Households assigned to reviewed residential premises";
done:
  cd_memory_free(selected);
  cd_memory_free(capacity);
  return cd_diag(d, status, message);
}

CdStatus cd_world_household(const CdWorld *w, size_t i, CdHouseholdView *out,
                            CdDiagnostic *d) {
  if (!w || !out || i >= w->home_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown household");
  const CdHousehold *h = &w->homes[i];
  *out = (CdHouseholdView){
      h->id, h->dwelling == CD_NONE ? 0 : w->content->polygons[h->dwelling].id,
      h->members, h->housing_known && h->dwelling == CD_NONE, h->housing_known};
  return cd_diag(d, CD_OK,
                 "Synthetic household structure; housing unknown unless "
                 "explicitly admitted");
}
CdStatus cd_world_household_move(CdWorld *w, CdEntityId id, CdEntityId dwelling,
                                 CdDiagnostic *d) {
  CdHousehold *h = home(w, id);
  if (!h)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown household");
  uint32_t next = dwelling ? cd_polygon_index(w->content, dwelling) : CD_NONE;
  if (dwelling &&
      (next == CD_NONE || w->content->polygons[next].kind != CD_BUILDING))
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown dwelling");
  if (next == h->dwelling) {
    h->housing_known = 1;
    return cd_diag(d, CD_OK, "Household residence explicitly confirmed");
  }
  size_t reservations = 0;
  if (next != CD_NONE)
    for (size_t i = 0; i < w->count; i++)
      if (w->people[i].civil.household == id &&
          (w->people[i].presence == dwelling ||
           cd_housing_transient(w, &w->people[i], next)))
        reservations++;
  if (next != CD_NONE) {
    CdPolygon *p = &w->content->polygons[next];
    if (!cd_usable(w, next))
      return cd_diag(d, CD_INVALID_STATE, "Dwelling requires repair");
    if (!p->capacity_known)
      return cd_diag(
          d, CD_UNSUPPORTED,
          "Dwelling capacity unknown; source footprint is not a census");
    if (cd_housing_load(w, next) + h->members - reservations >
        cd_capacity(w, next))
      return cd_diag(d, CD_CAPACITY, "Insufficient dwelling capacity");
  }
  if (h->dwelling != CD_NONE)
    w->housing[h->dwelling] -= (uint32_t)h->members;
  if (next != CD_NONE)
    w->housing[next] += (uint32_t)h->members;
  cd_record_emit(w,CD_EVENT_HOUSEHOLD,(CdPersonRef){0},(CdPersonRef){0},id,0,CD_RECORD_DIAGNOSTIC,"explicit household operation","Household housing changed without spatial teleport");
  h->dwelling = next;
  h->housing_known = 1;
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].civil.household == id) {
      w->people[i].civil.dwelling = dwelling;
      if (dwelling && w->people[i].presence == dwelling) {
        w->people[i].presence = 0;
        w->visitors[next]--;
      }
    }
  return cd_diag(d, CD_OK,
                 "Residence changed atomically; physical presence and "
                 "ownership are unchanged");
}
CdStatus cd_world_transfer(CdWorld *w, CdPersonRef ref, CdEntityId id,
                           CdPersonRef guardian, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  CdHousehold *to = home(w, id);
  if (!p || !to)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown person/household");
  CdHousehold *from = home(w, p->civil.household);
  if (!from)
    return cd_diag(d, CD_INVALID_STATE, "Resident has no valid household");
  CdCitizen *g = NULL;
  if (!adult(w, p)) {
    g = cd_citizen(w, guardian);
    if (!g || !adult(w, g) || g == p || g->civil.household != id)
      return cd_diag(
          d, CD_INVALID_ARGUMENT,
          "Minor requires a live adult guardian in the destination household");
  }
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].civil.guardian.person_id == ref.person_id && from != to)
      return cd_diag(d, CD_INVALID_STATE,
                     "Reassign dependents before moving their guardian");
  if (to->dwelling != CD_NONE && !cd_usable(w, to->dwelling))
    return cd_diag(d, CD_INVALID_STATE, "Destination requires repair");
  if (from != to && from->dwelling != to->dwelling && to->dwelling != CD_NONE &&
      (cd_housing_load(w, to->dwelling) + 1 -
           (p->presence == w->content->polygons[to->dwelling].id ||
            cd_housing_transient(w, p, to->dwelling)) >
       cd_capacity(w, to->dwelling)))
    return cd_diag(d, CD_CAPACITY, "Destination dwelling full");
  if (from != to) {
    from->members--;
    to->members++;
    if (from->dwelling != CD_NONE)
      w->housing[from->dwelling]--;
    if (to->dwelling != CD_NONE)
      w->housing[to->dwelling]++;
    p->civil.household = id;
    p->civil.dwelling =
        to->dwelling == CD_NONE ? 0 : w->content->polygons[to->dwelling].id;
    if (p->civil.dwelling && p->presence == p->civil.dwelling) {
      w->visitors[to->dwelling]--;
      p->presence = 0;
    }
  }
  if (g)
    p->civil.guardian = g->civil.ref;
  return cd_diag(
      d, CD_OK,
      "Household membership changed without moving physical presence");
}
CdStatus cd_world_partner(CdWorld *w, CdPersonRef ar, CdPersonRef br,
                          CdDiagnostic *d) {
  CdCitizen *a = cd_citizen(w, ar), *b = cd_citizen(w, br);
  if (!a || !b)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown/foreign partner");
  if (a == b || !adult(w, a) || !adult(w, b) ||
      (a->civil.spouse.person_id &&
       a->civil.spouse.person_id != br.person_id) ||
      (b->civil.spouse.person_id && b->civil.spouse.person_id != ar.person_id))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit adult partnership requires two unpartnered or "
                   "matching residents");
  a->civil.spouse = br;
  b->civil.spouse = ar;
  return cd_diag(d, CD_OK,
                 "Symmetric technical-fixture partnership; no historical "
                 "marriage assertion");
}
CdStatus cd_world_guardian(CdWorld *w, CdPersonRef child, CdPersonRef adult_ref,
                           CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, child), *g = cd_citizen(w, adult_ref);
  if (!p || !g)
    return cd_diag(d, CD_UNKNOWN_REFERENCE,
                   "Unknown/foreign guardian reference");
  if (p == g || adult(w, p) || !adult(w, g) ||
      p->civil.household != g->civil.household)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Guardian must be a live adult in the child's household");
  p->civil.guardian = adult_ref;
  return cd_diag(d, CD_OK, "Valid guardian assigned");
}
CdStatus cd_world_visit(CdWorld *w, CdPersonRef ref, CdEntityId dwelling,
                        int present, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  uint32_t b = w ? cd_polygon_index(w->content, dwelling) : CD_NONE;
  if (!p || b == CD_NONE || w->content->polygons[b].kind != CD_BUILDING ||
      (present != 0 && present != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown visitor/dwelling");
  if (!present) {
    if (p->presence != dwelling)
      return cd_diag(d, CD_UNKNOWN_REFERENCE, "No matching visit");
    w->visitors[b]--;
    p->presence = 0;
    return cd_diag(d, CD_OK, "Visit ended; household and ownership retained");
  }
  if (p->presence)
    return cd_diag(d, CD_INVALID_STATE,
                   "Resident already registered on a visit");
  CdPolygon *poly = &w->content->polygons[b];
  if (!poly->capacity_known)
    return cd_diag(d, CD_UNSUPPORTED, "Visitor capacity unknown");
  if (cd_housing_load(w, b) - cd_housing_transient(w, p, b) >=
      cd_capacity(w, b))
    return cd_diag(d, CD_CAPACITY, "No visitor capacity");
  if (p->civil.dwelling == dwelling)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Household members are not counted again as visitors");
  w->visitors[b]++;
  p->presence = dwelling;
  return cd_diag(d, CD_OK,
                 "Explicit visitor reservation; no teleport or household "
                 "departure inferred");
}
CdStatus cd_world_owner(CdWorld *w, CdEntityId dwelling, CdPersonRef ref,
                        CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, dwelling) : CD_NONE;
  CdCitizen *p = cd_citizen(w, ref);
  if (b == CD_NONE || w->content->polygons[b].kind != CD_BUILDING || !p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown property/person");
  if (!adult(w, p))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Technical model requires an adult property owner");
  w->owners[b] = ref;
  return cd_diag(
      d, CD_OK,
      "Property ownership recorded independently of residence and presence");
}
