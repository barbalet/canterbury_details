#include "cd_city_internal.h"
#include "cd_native.h"
#include <stdlib.h>
static int compare(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}
static int ref_known(CdPersonRef r, const uint64_t *ids, size_t n) {
  return !r.world_token &&
         (!r.person_id || bsearch(&r.person_id, ids, n, sizeof *ids, compare));
}
static int index_ok(uint32_t i, uint32_t count) {
  return i == CD_NONE || i < count;
}
static int task(CdTaskState state) {
  return (unsigned)state <= CD_TASK_CANCELLED;
}
static int finite_money(uint64_t *sum, uint64_t amount) {
  if (amount > UINT64_C(1000000000000) - *sum)
    return 0;
  *sum += amount;
  return 1;
}
CdStatus cd_checkpoint_validate(CdWorld *w, const CdSmokeStage *stage,
                                CdDiagnostic *d) {
  CdStatus status = CD_INVALID_DATA;
  uint64_t *ids =
      cd_memory_calloc(w->count + w->life_count + 1, sizeof *ids, CD_MEM_TEMP);
  CdPremises *held = cd_memory_calloc(w->content->np ? w->content->np : 1,
                                      sizeof *held, CD_MEM_TEMP);
  uint32_t *homes = cd_memory_calloc(w->home_count ? w->home_count : 1,
                                     sizeof *homes, CD_MEM_TEMP),
           *counts = cd_memory_calloc(w->content->np * 3 + w->content->ne + 1,
                                      sizeof *counts, CD_MEM_TEMP);
  if (!ids || !held || !homes || !counts) {
    status = CD_CAPACITY;
    goto done;
  }
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
#define TEXT(x) REQUIRE(memchr((x), 0, sizeof(x)))
#define REF(x) REQUIRE(ref_known((x), ids, w->count + w->life_count))
  uint64_t native_minute = cd_native_stage_minutes(stage);
  CdCivilTime civil;
  REQUIRE(w->origin_offset < 366 * 1440 &&
          w->last_human_minute == native_minute &&
          cd_smoke_civil_time(native_minute + w->origin_offset, &civil, NULL) ==
              CD_OK);
  REQUIRE(w->record_count == (w->record_next - 1 < CD_RECORD_LIMIT
                                  ? w->record_next - 1
                                  : CD_RECORD_LIMIT));
  REQUIRE(cd_native_stage_count(stage) == w->count &&
          w->count + w->deaths + w->departures ==
              w->initial_count + w->admissions &&
          w->births <= w->admissions &&
          w->deaths + w->departures == w->life_count &&
          w->record_next > w->record_count && w->last_cause < w->record_next &&
          w->current_cause == 0);
  for (size_t i = 0; i < w->count; i++) {
    ids[i] = w->people[i].civil.ref.person_id;
    REQUIRE(ids[i] && cd_native_stage_has(stage, ids[i]) &&
            (!i || ids[i] > ids[i - 1]));
  }
  for (size_t i = 0; i < w->life_count; i++) {
    CdLifeRecord *r = &w->life[i];
    ids[w->count + i] = r->ref.person_id;
    REQUIRE(r->ref.person_id && (unsigned)r->reason <= CD_DEATH &&
            r->household && r->household <= w->home_count &&
            cd_civil_valid(r->birth));
    TEXT(r->name);
    TEXT(r->basis);
    if (r->position_known)
      REQUIRE(isfinite(r->position.x) && isfinite(r->position.y));
  }
  qsort(ids, w->count + w->life_count, sizeof *ids, compare);
  for (size_t i = 0; i < w->life_count; i++) {
    REF(w->life[i].ref);
    REF(w->life[i].parents[0]);
    REF(w->life[i].parents[1]);
  }
  for (size_t i = 1; i < w->count + w->life_count; i++)
    REQUIRE(ids[i] != ids[i - 1]);
  uint64_t money = 0;
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    REF(p->civil.ref);
    REF(p->civil.spouse);
    REF(p->civil.guardian);
    REF(p->civil.parents[0]);
    REF(p->civil.parents[1]);
    REF(p->health.carer);
    REF(p->care_target);
    REF(p->feed_target);
    REF(p->work.mentor);
    TEXT(p->civil.name);
    TEXT(p->civil.admission_basis);
    TEXT(p->work.basis);
    TEXT(p->work.view.reason);
    TEXT(p->trade.view.reason);
    TEXT(p->training_basis);
    TEXT(p->health.basis);
    TEXT(p->credential_basis);
    TEXT(p->ordination_basis);
    REQUIRE(
        p->civil.household && p->civil.household <= w->home_count &&
        cd_civil_valid(p->civil.birth) && p->civil.birth.minute_of_day == 0 &&
        p->skills <= 15 && p->credentials <= 63 && p->meals <= 12 &&
        p->raw <= 12 && p->water <= 12 && p->fatigue <= 1440 &&
        (unsigned)p->activity <= CD_ACTIVITY_RETIRED &&
        (unsigned)p->travel <= CD_ROUTE_CANCELLED &&
        (unsigned)p->reason <= CD_ROUTE_ACCESS &&
        (unsigned)p->health.condition <= CD_CRITICAL &&
        p->health.severity <= 1000 && p->health.remaining <= 10080 &&
        p->rest_left <= 1440 && p->route.count <= CD_ROUTE_EDGE_LIMIT &&
        p->route_index <= p->route.count &&
        index_ok(p->anchor, w->content->nn) &&
        index_ok(p->inside, w->content->np) &&
        index_ok(p->door, w->content->nd) &&
        index_ok(p->held_edge, w->content->ne) && !p->work.spec.basis &&
        task(p->work.view.state) && task(p->trade.view.state) &&
        p->jobs[0] <= w->job_count && p->jobs[1] <= w->job_count &&
        p->mission <= w->service_count && p->gathering <= w->gathering_count &&
        p->vehicle <= w->vehicle_count && p->driving <= w->vehicle_count &&
        p->bereavement_event < w->record_next &&
        p->record_cursor.event < w->record_next && p->food_remaining <= 4096 &&
        (unsigned)p->food_action <= CD_PRODUCE_MEALS &&
        finite_money(&money, p->wallet));
    if (p->civil.spouse.person_id)
      REQUIRE(cd_citizen(w, p->civil.spouse));
    if (p->civil.guardian.person_id) {
      CdCitizen *g = cd_citizen(w, p->civil.guardian);
      REQUIRE(g && g->civil.age >= 18 &&
              g->civil.household == p->civil.household);
    }
    if (p->health.carer.person_id)
      REQUIRE(cd_citizen(w, p->health.carer));
    for (size_t k = 0; k < p->route.count; k++)
      REQUIRE(cd_edge_index(w->content, p->route.edges[k]) != CD_NONE);
    if (p->door != CD_NONE)
      REQUIRE(w->content->doors[p->door].reviewed &&
              p->door_vertex < w->content->doors[p->door].count);
    if (p->held_edge != CD_NONE)
      counts[w->content->np * 3 + p->held_edge]++;
    CdHousehold *h = &w->homes[p->civil.household - 1];
    REQUIRE(index_ok(h->dwelling, w->content->np));
    homes[p->civil.household - 1]++;
    REQUIRE(
        p->civil.dwelling ==
        (h->dwelling == CD_NONE ? 0 : w->content->polygons[h->dwelling].id));
    if (p->presence) {
      uint32_t b = cd_polygon_index(w->content, p->presence);
      REQUIRE(b != CD_NONE);
      counts[w->content->np + b]++;
    }
    if (p->health.clinic) {
      uint32_t b = cd_polygon_index(w->content, p->health.clinic);
      REQUIRE(b != CD_NONE);
      counts[w->content->np * 2 + b]++;
    }
    for (unsigned k = 0; k < 4; k++)
      if (p->memberships[k])
        REQUIRE(cd_site_index(w->content, p->memberships[k]) != CD_NONE);
    if (p->refuge)
      REQUIRE(cd_polygon_index(w->content, p->refuge) != CD_NONE);
    if (p->ordination_institution)
      REQUIRE(cd_site_index(w->content, p->ordination_institution) != CD_NONE &&
              p->ordination_kind <= CD_OFFICE_RABBI);
    for (unsigned g = 0; g < CD_GOODS; g++)
      REQUIRE(p->goods[g] <= 16);
    if (p->work.view.id) {
      REQUIRE(p->work.view.id < w->next_work && p->work.view.job &&
              p->work.view.job <= w->job_count &&
              p->work.view.progress <= p->work.view.duration &&
              (unsigned)p->work.spec.recipe <= CD_RECIPE_DEVELOPMENT &&
              p->work.spec.units && p->work.spec.units <= 1000 &&
              p->work.view.duration && p->work.view.duration <= 10080);
      uint32_t b = cd_polygon_index(
          w->content, w->jobs[p->work.view.job - 1].view.workplace);
      REQUIRE(b != CD_NONE);
      if (cd_pending(p->work.view.state)) {
        for (unsigned g = 0; g < CD_GOODS; g++) {
          REQUIRE(p->work.inputs[g] <= 2000);
          held[b].reserved[g] += p->work.inputs[g];
        }
        REQUIRE(p->work.raw <= 4096);
        held[b].reserved_raw += (unsigned)p->work.raw;
        held[b].escrow_money += p->work.view.reserved_money;
      }
    }
    if (p->trade.view.id) {
      REQUIRE(p->trade.view.id < w->next_trade &&
              p->trade.view.good < CD_GOODS && p->trade.view.quantity &&
              p->trade.view.quantity <= 12 &&
              (p->trade.view.meals == 0 || p->trade.view.meals == 1) &&
              p->trade.payer && p->trade.payer <= w->home_count);
      uint32_t b = cd_polygon_index(w->content, p->trade.view.seller);
      REQUIRE(b != CD_NONE);
      if (cd_pending(p->trade.view.state)) {
        REQUIRE(p->trade.cost == p->trade.view.total);
        held[b].escrow_money += p->trade.cost;
        if (p->trade.view.meals)
          held[b].reserved_meals += p->trade.view.quantity;
        else
          held[b].reserved[p->trade.view.good] += p->trade.view.quantity;
      }
    }
    if (p->cargo.remaining)
      REQUIRE(p->cargo.remaining <= 4096 && p->cargo.good < CD_GOODS &&
              cd_polygon_index(w->content, p->cargo.building) != CD_NONE &&
              p->cargo.household <= w->home_count);
    if (p->food_remaining)
      REQUIRE((p->food_endpoint.kind == CD_FOOD_HOME && p->food_endpoint.id &&
               p->food_endpoint.id <= w->home_count) ||
              (p->food_endpoint.kind == CD_FOOD_STORE &&
               cd_polygon_index(w->content, p->food_endpoint.id) != CD_NONE));
  }
  for (size_t i = 0; i < w->home_count; i++) {
    CdHousehold *h = &w->homes[i];
    REQUIRE(h->id == i + 1 && index_ok(h->dwelling, w->content->np) &&
            h->members == homes[i] && w->home_meals[i] <= 4096 &&
            w->home_raw[i] <= 4096 && finite_money(&money, w->home_wallet[i]));
    if (h->dwelling != CD_NONE) {
      REQUIRE(w->content->polygons[h->dwelling].kind == CD_BUILDING &&
              w->content->polygons[h->dwelling].capacity_known);
      counts[h->dwelling] += (uint32_t)h->members;
    }
    for (unsigned g = 0; g < CD_GOODS; g++)
      REQUIRE(w->home_goods[i * CD_GOODS + g] <= 1000000000);
  }
  for (size_t i = 0; i < w->job_count; i++) {
    CdJob *j = &w->jobs[i];
    REF(j->view.worker);
    REF(j->view.mentor);
    TEXT(j->view.basis);
    REQUIRE(j->view.id == i + 1 && j->view.occupation &&
            j->view.occupation <= cd_occupation_count() &&
            j->view.shift_start < 1440 && j->view.shift_end < 1440 &&
            j->view.wage <= 1000 &&
            cd_polygon_index(w->content, j->view.workplace) != CD_NONE &&
            cd_door_index(w->content, j->view.entrance) != CD_NONE);
    if (j->view.worker.person_id) {
      CdCitizen *p = cd_citizen(w, j->view.worker);
      REQUIRE(p && (p->jobs[0] == j->view.id || p->jobs[1] == j->view.id));
    }
  }
  for (uint32_t b = 0; b < w->content->np; b++) {
    CdProject *p = &w->projects[b];
    TEXT(p->view.basis);
    TEXT(p->view.reason);
    TEXT(p->view.use);
    REQUIRE(task(p->view.state) && p->view.progress <= p->view.minutes &&
            p->view.minutes <= 10080 &&
            (unsigned)p->view.kind <= CD_DEV_GEOMETRY);
    if (p->view.id)
      REQUIRE(p->view.id < w->next_project &&
              p->view.building == w->content->polygons[b].id);
    if (cd_pending(p->view.state))
      for (unsigned g = 0; g < CD_GOODS; g++) {
        REQUIRE(p->materials[g] <= 2);
        held[b].reserved[g] += p->materials[g];
      }
  }
  for (size_t i = 0; i < w->office_count; i++) {
    CdOfficeView *v = &w->offices[i].view;
    REF(v->holder);
    REF(v->last_holder);
    TEXT(v->basis);
    REQUIRE(v->id == i + 1 && v->job && v->job <= w->job_count &&
            cd_site_index(w->content, v->institution) != CD_NONE &&
            (unsigned)v->kind < CD_OFFICE_KINDS);
  }
  for (size_t i = 0; i < w->roster_count; i++) {
    CdRosterView *v = &w->rosters[i].view;
    TEXT(v->title);
    TEXT(v->basis);
    REQUIRE(v->id == i + 1 && v->job && v->job <= w->job_count &&
            v->capabilities < (1u << CD_SERVICE_KINDS));
  }
  for (size_t i = 0; i < w->service_count; i++) {
    CdService *r = &w->services[i];
    CdServiceView *v = &r->view;
    REF(v->worker);
    REF(v->subject);
    TEXT(v->basis);
    TEXT(v->reason);
    REQUIRE(v->id == i + 1 && task(v->state) &&
            (unsigned)v->kind < CD_SERVICE_KINDS && v->minutes &&
            v->minutes <= 10080 && v->progress <= v->minutes &&
            cd_polygon_index(w->content, v->building) != CD_NONE &&
            v->provider &&
            v->provider <= (v->religious ? w->office_count : w->roster_count));
    CdEntityId job = v->religious ? w->offices[v->provider - 1].view.job
                                  : w->rosters[v->provider - 1].view.job;
    uint32_t b = cd_polygon_index(w->content, w->jobs[job - 1].view.workplace);
    REQUIRE(b != CD_NONE);
    if (cd_pending(v->state))
      held[b].escrow_money += v->reserved_money;
    else
      REQUIRE(!v->reserved_money);
  }
  for (size_t i = 0; i < w->gathering_count; i++) {
    CdGathering *r = &w->gatherings[i];
    CdGatheringView *v = &r->view;
    TEXT(v->basis);
    TEXT(v->reason);
    REQUIRE(v->id == i + 1 && task(v->state) && v->office &&
            v->office <= w->office_count &&
            (unsigned)v->kind < CD_SERVICE_KINDS && v->capacity &&
            v->capacity <= CD_ATTENDANCE_LIMIT && r->count <= v->capacity &&
            v->minutes && v->minutes <= 10080 && v->repeats &&
            v->repeats <= 366 && v->occurrence <= v->repeats &&
            v->progress <= v->minutes &&
            (!cd_pending(v->state) || v->occurrence < v->repeats));
    for (size_t k = 0; k < CD_ATTENDANCE_LIMIT; k++)
      REF(r->attendees[k]);
    uint32_t b = cd_polygon_index(
        w->content,
        w->jobs[w->offices[v->office - 1].view.job - 1].view.workplace);
    REQUIRE(b != CD_NONE);
    if (cd_pending(v->state))
      held[b].escrow_money += v->reserved_money;
    else
      REQUIRE(!v->reserved_money);
  }
  for (size_t i = 0; i < w->vehicle_count; i++) {
    CdVehicle *r = &w->vehicles[i];
    CdVehicleView *v = &r->view;
    REF(v->crew);
    REF(v->passenger);
    REF(r->requested_passenger);
    TEXT(v->basis);
    TEXT(v->reason);
    REQUIRE(v->id == i + 1 && v->job && v->job <= w->job_count &&
            (unsigned)v->kind < CD_RAIL_VEHICLE &&
            (unsigned)v->phase <= CD_VEHICLE_STOPPED && v->capacity <= 16 &&
            v->loaded <= v->capacity && v->good < CD_GOODS &&
            v->requested <= 16 && v->delivered <= v->requested);
    if (v->position_known)
      REQUIRE(isfinite(v->position.x) && isfinite(v->position.y));
    if (v->phase != CD_VEHICLE_IDLE)
      REQUIRE(cd_polygon_index(w->content, v->source) != CD_NONE &&
              cd_polygon_index(w->content, v->destination) != CD_NONE &&
              r->destination_door < w->content->nd);
    if (v->passenger.person_id) {
      CdCitizen *p = cd_citizen(w, v->passenger);
      REQUIRE(p && p->vehicle == v->id);
    }
  }
  for (size_t i = 0; i < w->pressure_count; i++) {
    CdPressureView *v = &w->pressures[i].view;
    TEXT(v->basis);
    REQUIRE(v->id == i + 1 && task(v->state) &&
            (unsigned)v->kind < CD_PRESSURE_KINDS && v->minutes &&
            v->minutes <= 10080 && v->value && v->value <= 1000);
    if (v->kind == CD_PRESSURE_FIRE || v->kind == CD_PRESSURE_DAMAGE)
      REQUIRE(cd_polygon_index(w->content, v->target) != CD_NONE);
    if (v->kind == CD_PRESSURE_EDGE_CLOSURE)
      REQUIRE(cd_edge_index(w->content, v->target) != CD_NONE);
  }
  for (uint32_t b = 0; b < w->content->np; b++) {
    CdPremises *p = &w->premises[b];
    TEXT(p->food_basis);
    TEXT(p->clinic_basis);
    TEXT(p->water_basis);
    TEXT(p->shelter_basis);
    TEXT(p->fire_basis);
    TEXT(p->employer_name);
    TEXT(p->employer_basis);
    TEXT(p->scenario_use);
    REF(w->owners[b]);
    REQUIRE(
        w->housing[b] == counts[b] &&
        w->visitors[b] == counts[w->content->np + b] &&
        p->occupied == counts[w->content->np * 2 + b] && p->condition <= 1000 &&
        p->fire <= 1000 && p->water <= 16384 && p->capacity_override <= 128 &&
        p->meals <= 4096 && p->reserved_meals <= 4096 &&
        p->meals + p->reserved_meals <= 4096 && p->raw <= 4096 &&
        p->reserved_raw <= 4096 && p->raw + p->reserved_raw <= 4096 &&
        p->reserved_raw == held[b].reserved_raw &&
        p->reserved_meals == held[b].reserved_meals &&
        p->escrow_money == held[b].escrow_money && p->occupied <= p->beds &&
        p->beds <= cd_capacity(w, b) && p->work_capacity <= cd_capacity(w, b) &&
        p->shelter_capacity <= cd_capacity(w, b) &&
        finite_money(&money, p->treasury) &&
        finite_money(&money, p->escrow_money));
    for (unsigned g = 0; g < CD_GOODS; g++)
      REQUIRE(p->goods[g] <= 1000000000 && p->reserved[g] <= 1000000000 &&
              p->goods[g] + p->reserved[g] <= 1000000000 &&
              p->reserved[g] == held[b].reserved[g]);
  }
  REQUIRE(money == w->economy.money_created &&
          money <= UINT64_C(1000000000000));
  for (uint32_t e = 0; e < w->content->ne; e++)
    REQUIRE(w->traffic[e] == counts[w->content->np * 3 + e] &&
            w->closed[e] <= 1);
  for (uint32_t i = 0; i < w->content->ns; i++) {
    CdInstitutionRuntime *r = &w->institutions[i];
    REQUIRE(r->count <= 8);
    TEXT(r->office);
    TEXT(r->basis);
    for (size_t k = 0; k < 8; k++)
      TEXT(r->link_basis[k]);
    for (size_t k = 0; k < r->count; k++)
      REQUIRE(cd_polygon_index(w->content, r->premises[k]) != CD_NONE);
  }
  for (uint64_t id = w->record_next - w->record_count; id < w->record_next;
       id++) {
    CdRecord *r = &w->records[(id - 1) % CD_RECORD_LIMIT];
    REF(r->actor);
    REF(r->subject);
    TEXT(r->basis);
    TEXT(r->message);
    REQUIRE(r->id == id && r->cause < id &&
            (unsigned)r->kind < CD_EVENT_KINDS &&
            (unsigned)r->privacy <= CD_RECORD_DIAGNOSTIC &&
            r->household <= w->home_count);
  }
  for (size_t i = 0; i < cd_record_cursor_count(w->content); i++)
    REQUIRE(w->record_cursors[i].event < w->record_next);
  for (size_t i = 0; i < w->operation_count; i++) {
    CdOperationView *v = &w->operations[i].view;
    CdCommand *c = &v->command;
    REF(c->actor);
    REF(c->subject);
    TEXT(c->label);
    TEXT(c->basis);
    TEXT(v->reason);
    REQUIRE(v->id == i + 1 && c->key && (unsigned)c->kind < CD_CMD_KINDS &&
            task(v->state) && (unsigned)v->result <= CD_UNSUPPORTED &&
            v->queued_record < w->record_next &&
            v->result_record < w->record_next);
  }
  {
    CdFoodLedger food;
    CdEconomyLedger economy;
    CdCivicSummary civic;
    REQUIRE(cd_world_food_ledger(w, &food, NULL) == CD_OK &&
            cd_world_economy_ledger(w, &economy, NULL) == CD_OK &&
            cd_world_civic_summary(w, &civic, NULL) == CD_OK);
  }
  (void)native_minute;
  status = CD_OK;
done:
  cd_memory_free(ids);
  cd_memory_free(held);
  cd_memory_free(homes);
  cd_memory_free(counts);
  return cd_diag(d, status,
                 status == CD_OK
                     ? "Complete staged city references, reservations and "
                       "conservation valid"
                     : "Staged city invariant/reference/allocation rejected");
#undef REQUIRE
#undef TEXT
#undef REF
}
