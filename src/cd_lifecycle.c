#include "cd_city_internal.h"
#include "cd_native.h"
int cd_model_ready(const CdWorld *w) {
  return w && cd_native_closed(w->native);
}
static int valid_text(const char *s, size_t limit) {
  if (!s || !*s)
    return 0;
  size_t n = 0;
  while (n < limit && s[n])
    n++;
  if (n == limit)
    return 0;
  for (size_t i = 0; i < n;) {
    unsigned x = (unsigned char)s[i++], extra = 0, min = 0;
    if (x < 128) {
      if (x < 32 || x == 127)
        return 0;
      continue;
    }
    if (x >= 0xc2 && x <= 0xdf) {
      extra = 1;
      x &= 31;
      min = 128;
    } else if (x >= 0xe0 && x <= 0xef) {
      extra = 2;
      x &= 15;
      min = 2048;
    } else if (x >= 0xf0 && x <= 0xf4) {
      extra = 3;
      x &= 7;
      min = 65536;
    } else
      return 0;
    if (extra > n - i)
      return 0;
    while (extra--) {
      unsigned y = (unsigned char)s[i++];
      if ((y & 192) != 128)
        return 0;
      x = (x << 6) | (y & 63);
    }
    if (x < min || x > 0x10ffff || (x >= 0xd800 && x <= 0xdfff))
      return 0;
  }
  return 1;
}
int cd_basis_valid(const char *s) { return valid_text(s, 96); }
int cd_colocated(const CdWorld *w, const CdCitizen *a, const CdCitizen *b) {
  return a && b && a->civil.position_known && b->civil.position_known &&
         a->inside == b->inside && a->door == CD_NONE && b->door == CD_NONE &&
         hypot(cd_city_position(w, a).x - cd_city_position(w, b).x,
               cd_city_position(w, a).y - cd_city_position(w, b).y) <=
             1.0 / cd_city_scale(w);
}
static CdCivilTime now(const CdWorld *w) {
  CdCivilTime t = {0};
  cd_smoke_civil_time(cd_native_minutes(w->native) + w->origin_offset, &t,
                      NULL);
  return t;
}
static CdStatus admission_check(CdWorld *w, const CdAdmission *a,
                                CdPersonRef *out, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !a || !out || !cd_basis_valid(a->basis) ||
      !cd_civil_valid(a->birth) || a->birth.minute_of_day || !a->household ||
      a->household > w->home_count || (unsigned)a->category > CD_EVACUEE ||
      !valid_text(a->name, 128))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Admission needs a closed world, valid civil fields and "
                   "explicit basis");
  CdCivilTime t = now(w);
  if (a->birth.year > t.year ||
      (a->birth.year == t.year &&
       (a->birth.month > t.month ||
        (a->birth.month == t.month && a->birth.day > t.day))))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Birth date is in the future");
  if (w->count >= cd_smoke_capacity() || w->life_count == w->life_capacity)
    return cd_diag(d, CD_CAPACITY,
                   "Population or bounded life archive is full");
  CdHousehold *h = &w->homes[a->household - 1];
  if (!h->housing_known || h->dwelling == CD_NONE)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Admission requires explicitly established housing");
  CdPolygon *b = &w->content->polygons[h->dwelling];
  if (!b->capacity_known ||
      cd_housing_load(w, h->dwelling) >= cd_capacity(w, h->dwelling) ||
      !cd_usable(w, h->dwelling))
    return cd_diag(d, CD_CAPACITY, "No reviewed dwelling capacity");
  unsigned age = cd_civil_age(a->birth, t);
  if (a->category == CD_TROOP && age < 18)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Troop admission requires an adult");
  if (age < 18) {
    CdCitizen *g = cd_citizen(w, a->guardian);
    if (!g || g->civil.household != a->household ||
        cd_civil_age(g->civil.birth, t) < 18 || g->health.mortality_pending)
      return cd_diag(d, CD_INVALID_ARGUMENT,
                     "Minor requires a live adult guardian in the household");
  }
  return CD_OK;
}
CdStatus cd_world_admit(CdWorld *w, const CdAdmission *a, CdPersonRef *out,
                        CdDiagnostic *d) {
  CdStatus status = admission_check(w, a, out, d);
  if (status != CD_OK)
    return status;
  CdPersonRef ref;
  status = cd_smoke_admit_next(w->native, &ref, d);
  if (status != CD_OK)
    return status;
  CdCitizen *p = &w->people[w->count];
  memset(p, 0, sizeof *p);
  p->civil.ref = ref;
  snprintf(p->civil.name, sizeof p->civil.name, "%s", a->name);
  snprintf(p->civil.admission_basis, sizeof p->civil.admission_basis, "%s",
           a->basis);
  p->civil.birth = a->birth;
  p->civil.age = cd_civil_age(a->birth, now(w));
  p->civil.category = a->category;
  p->civil.synthetic = 1;
  p->civil.household = a->household;
  p->civil.guardian = p->civil.age < 18 ? a->guardian : (CdPersonRef){0};
  CdHousehold *h = &w->homes[a->household - 1];
  p->civil.dwelling = w->content->polygons[h->dwelling].id;
  p->civil.housing_known = 1;
  p->anchor = p->held_edge = p->door = p->inside = CD_NONE;
  w->count++;
  h->members++;
  w->housing[h->dwelling]++;
  w->admissions++;
  cd_native_city_body(w->native, ref);
  cd_native_energy_set(w->native, ref, cd_native_full());
  cd_record_emit(w,CD_EVENT_ADMISSION,ref,(CdPersonRef){0},p->civil.household,0,CD_RECORD_HOUSEHOLD,a->basis,"Explicit ordinary admission");
  *out = ref;
  return cd_diag(d, CD_OK,
                 "Explicit ordinary admission committed without invented "
                 "spatial placement");
}
CdStatus cd_world_birth(CdWorld *w, CdPersonRef guardian, CdPersonRef other,
                        const char *basis, CdPersonRef *out, CdDiagnostic *d) {
  CdCitizen *g = cd_citizen(w, guardian),
            *second = other.person_id ? cd_citizen(w, other) : NULL;
  if (!g || !cd_model_ready(w) || !out || !cd_basis_valid(basis) ||
      g->civil.age < 18 || g->health.severity >= 600 ||
      g->health.mortality_pending ||
      (other.person_id && (!second || second == g || second->civil.age < 18 ||
                           second->civil.household != g->civil.household ||
                           second->health.mortality_pending)))
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Birth requires configured live adult parent/guardian references");
  int energy = cd_native_energy(w->native, guardian);
  if (energy < (int)(2 * cd_native_hungry()))
    return cd_diag(d, CD_INVALID_STATE,
                   "Insufficient technical parent energy reserve");
  CdCivilTime t = now(w);
  t.minute_of_day = 0;
  CdAdmission a = {"Newborn [configured fixture]",
                   basis,
                   t,
                   g->civil.household,
                   CD_RESIDENT,
                   guardian};
  CdStatus status = cd_world_admit(w, &a, out, d);
  if (status != CD_OK)
    return status;
  cd_native_energy_set(w->native, guardian, energy - (int)cd_native_hungry());
  cd_native_energy_set(w->native, *out, cd_native_hungry());
  CdCitizen *child = cd_citizen(w, *out);
  child->civil.parents[0] = guardian;
  child->civil.parents[1] = other;
  cd_population_name(w, out->person_id, child->civil.household,
                     child->civil.name);
  const char *surname = strchr(g->civil.name, ' ');
  char *space = strchr(child->civil.name, ' ');
  if (surname && surname[1] && space &&
      (size_t)(space - child->civil.name) + strlen(surname) <
          sizeof child->civil.name) {
    char combined[128];
    snprintf(combined, sizeof combined, "%.*s%s",
             (int)(space - child->civil.name), child->civil.name, surname);
    snprintf(child->civil.name, sizeof child->civil.name, "%s", combined);
  }
  w->births++;
  cd_record_emit(w,CD_EVENT_BIRTH,*out,guardian,child->civil.household,0,CD_RECORD_HOUSEHOLD,basis,"Configured birth and household parentage");
  return cd_diag(d, CD_OK,
                 "Configured birth; no demographic rate or location inferred");
}
CdStatus cd_world_retire(CdWorld *w, CdPersonRef ref, CdRetirement reason,
                         const char *basis, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown/retired/foreign person");
  if (!cd_model_ready(w) || (unsigned)reason > CD_DEATH ||
      !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Retirement requires closed interval and explicit reason");
  if (w->life_count == w->life_capacity)
    return cd_diag(d, CD_CAPACITY,
                   "Life archive full; retirement not discarded");
  CdCitizen *replacement = NULL;
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *candidate = &w->people[i];
    if (candidate != p && candidate->civil.household == p->civil.household &&
        candidate->civil.age >= 18 && !candidate->health.mortality_pending) {
      replacement = candidate;
      break;
    }
  }
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].civil.guardian.person_id == ref.person_id && !replacement)
      return cd_diag(
          d, CD_INVALID_STATE,
          "Reassign dependents before removing the last available guardian");
  CdHousehold *h = &w->homes[p->civil.household - 1];
  CdLifeRecord record = {0};
  record.ref = ref;
  record.birth = p->civil.birth;
  record.parents[0] = p->civil.parents[0];
  record.parents[1] = p->civil.parents[1];
  record.minute = cd_native_minutes(w->native);
  record.household = h->id;
  record.age = cd_civil_age(p->civil.birth, now(w));
  record.reason = reason;
  record.position_known = p->civil.position_known;
  if (record.position_known)
    record.position = cd_city_position(w, p);
  if (reason == CD_DEATH) {
    record.meals = p->meals;
    record.raw = p->raw;
    record.water = p->water;
    memcpy(record.goods, p->goods, sizeof record.goods);
  }
  snprintf(record.name, sizeof record.name, "%s", p->civil.name);
  snprintf(record.basis, sizeof record.basis, "%s", basis);
  CdStatus status = cd_smoke_retire(w->native, ref, d);
  if (status != CD_OK)
    return status;
  uint64_t retirement=cd_record_emit(w,CD_EVENT_RETIREMENT,ref,(CdPersonRef){0},p->civil.household,0,CD_RECORD_HOUSEHOLD,basis,reason==CD_DEATH?"Ordinary death archived":"Ordinary departure archived");
  uint64_t previous_cause=w->current_cause;w->current_cause=retirement;
  if(reason==CD_DEATH)for(size_t i=0;i<w->count;i++){CdCitizen *q=&w->people[i];if(q!=p&&q->civil.household==p->civil.household){q->support_until=cd_native_minutes(w->native)+1440;q->bereavement_event=cd_record_emit(w,CD_EVENT_BEREAVEMENT,q->civil.ref,ref,q->civil.household,retirement,CD_RECORD_HOUSEHOLD,basis,"One-day technical bereavement support context; ordinary visitation available");}}
  if (p->held_edge != CD_NONE && w->traffic[p->held_edge])
    w->traffic[p->held_edge]--;
  if (p->presence) {
    uint32_t b = cd_polygon_index(w->content, p->presence);
    if (b != CD_NONE)
      w->visitors[b]--;
  }
  cd_care_release(w, p);
  cd_civic_retire(w, p, reason);
  cd_employment_retire(w, p, reason);
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *q = &w->people[i];
    if (q->civil.spouse.person_id == ref.person_id)
      q->civil.spouse = (CdPersonRef){0};
    if (q->civil.guardian.person_id == ref.person_id)
      q->civil.guardian = replacement->civil.ref;
    if (q->feed_target.person_id == ref.person_id)
      q->feed_target = (CdPersonRef){0};
    if (q->care_target.person_id == ref.person_id)
      cd_care_release(w, q);
  }
  for (uint32_t b = 0; b < w->content->np; b++)
    if (w->owners[b].person_id == ref.person_id)
      w->owners[b] = (CdPersonRef){0};
  if (reason == CD_DEPARTURE) {
    w->food.exported_meals += p->meals;
    w->food.exported_raw += p->raw;
  }
  w->food.retired_transfers += p->meals + p->raw;
  if (h->dwelling != CD_NONE)
    w->housing[h->dwelling]--;
  h->members--;
  size_t index = (size_t)(p - w->people);
  memmove(p, p + 1, (w->count - index - 1) * sizeof *p);
  w->count--;
  memset(&w->people[w->count], 0, sizeof *p);
  w->life[w->life_count++] = record;
  w->current_cause=previous_cause;
  if (reason == CD_DEATH)
    w->deaths++;
  else
    w->departures++;
  return cd_diag(d, CD_OK,
                 "Retirement archived; references, work, occupancy and "
                 "property cleaned once");
}
size_t cd_world_life_count(const CdWorld *w) { return w ? w->life_count : 0; }
CdStatus cd_world_life(const CdWorld *w, size_t i, CdLifeRecord *out,
                       CdDiagnostic *d) {
  if (!w || !out || i >= w->life_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown life record");
  *out = w->life[i];
  return cd_diag(d, CD_OK, "Archived ordinary life record");
}
CdStatus cd_mortality_commit(CdWorld *w, CdDiagnostic *d) {
  w->mortality_blocked = 0;
  for (size_t i = 0; i < w->count;) {
    CdCitizen *p = &w->people[i];
    if (!p->health.mortality_pending) {
      i++;
      continue;
    }
    CdStatus s = cd_world_retire(w, p->civil.ref, CD_DEATH,
                                 "technical health scenario mortality", d);
    if (s != CD_OK) {
      p->health.mortality_blocked = 1;
      w->mortality_blocked = 1;
      return s;
    }
  }
  return CD_OK;
}
