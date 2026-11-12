#include "cd_dsl_catalogue.h"
#define _POSIX_C_SOURCE 200809L
#include "cd_city_internal.h"
#include "cd_native.h"
#include <stdlib.h>
#include <errno.h>
static const char *names[] = {"place",
                              "move",
                              "enter",
                              "exit",
                              "close-road",
                              "lock",
                              "train",
                              "authorize",
                              "employer",
                              "assign-job",
                              "work",
                              "cancel-work",
                              "import-goods",
                              "import-food",
                              "cargo",
                              "import-money",
                              "project",
                              "cancel-project",
                              "dispatch",
                              "cancel-service",
                              "gathering",
                              "attend",
                              "pressure",
                              "cancel-pressure",
                              "shelter",
                              "shelter-person",
                              "link-institution",
                              "operate-institution",
                              "create-job",
                              "unassign-job",
                              "close-job",
                              "roster",
                              "office",
                              "qualify",
                              "membership",
                              "food-store",
                              "clinic",
                              "import-water",
                              "wear",
                              "open-employer",
                              "vehicle",
                              "transport",
                              "stop-vehicle",
                              "unload",
                              "condition"};
const char *cd_command_name(CdCommandKind k) {
  return (unsigned)k < CD_CMD_KINDS ? names[k] : "unknown";
}
static int same(const CdCommand *a, const CdCommand *b) {
  return a->key == b->key && a->kind == b->kind &&
         a->actor.person_id == b->actor.person_id &&
         a->actor.world_token == b->actor.world_token &&
         a->subject.person_id == b->subject.person_id &&
         a->subject.world_token == b->subject.world_token &&
         a->target == b->target && a->secondary == b->secondary &&
         a->auxiliary == b->auxiliary && a->units == b->units &&
         a->value == b->value && a->starts == b->starts &&
         a->duration == b->duration && a->earliest == b->earliest &&
         a->deadline == b->deadline && a->flags == b->flags &&
         !strcmp(a->label, b->label) && !strcmp(a->basis, b->basis);
}
CdStatus cd_world_operation_preview(const CdWorld *world, const CdCommand *c,
                                    CdPermission permission, CdPreview *out,
                                    CdDiagnostic *d) {
  CdWorld *w = (CdWorld *)world;
  if (!cd_model_ready(w) || !c || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Closed world, command and preview required");
  *out = (CdPreview){CD_INVALID_ARGUMENT, 0, 0};
#define FAIL(code, msg)                                                        \
  do {                                                                         \
    out->status = (code);                                                      \
    return cd_diag(d, code, msg);                                              \
  } while (0)
  if (permission != CD_PERMISSION_INTERVENTION)
    FAIL(CD_UNSUPPORTED, "Observer mode cannot authorize interventions");
  if (!c->key || (unsigned)c->kind >= CD_CMD_KINDS ||
      !cd_basis_valid(c->basis) || !memchr(c->label, 0, 96) ||
      (c->label[0] && !cd_basis_valid(c->label)) || c->units > UINT32_MAX ||
      c->value > UINT32_MAX || c->duration > 10080 || c->flags > 65535)
    FAIL(CD_INVALID_ARGUMENT, "Invalid command fields or explicit basis");
  uint64_t now = cd_native_minutes(w->native);
  if (c->earliest > now + 10080 ||
      (c->deadline && (c->deadline <= now || c->deadline < c->earliest ||
                       c->deadline - now > 10080)))
    FAIL(CD_INVALID_ARGUMENT,
         "Operation scheduling/deadline outside bounded native window");
  CdCitizen *p = cd_citizen(w, c->actor);
  if (c->actor.person_id && !p)
    FAIL(CD_UNKNOWN_REFERENCE, "Acting person is retired, foreign or absent");
  uint32_t b = cd_polygon_index(w->content, c->target);
  int building = b != CD_NONE && w->content->polygons[b].kind == CD_BUILDING;
  switch (c->kind) {
  case CD_CMD_PLACE:
    if (!p || cd_node_index(w->content, c->target) == CD_NONE)
      FAIL(CD_UNKNOWN_REFERENCE, "Live actor and source node required");
    {
      uint32_t n = cd_node_index(w->content, c->target);
      int qualified = 0;
      for (uint32_t i = w->content->adj_offset[n];
           i < w->content->adj_offset[n + 1]; i++)
        qualified |= w->content->edges[w->content->adj_edges[i]].qualified;
      if (!qualified)
        FAIL(CD_UNSUPPORTED, "Source node has no qualified walking link");
    }
    if (p->civil.position_known)
      FAIL(CD_INVALID_STATE, "Placement cannot teleport a placed person");
    break;
  case CD_CMD_MOVE:
    if (!p || !p->civil.position_known ||
        cd_node_index(w->content, c->target) == CD_NONE)
      FAIL(CD_UNKNOWN_REFERENCE, "Placed actor and source node required");
    {
      CdRequest r = {
          .kind = CD_OPERATION_MOVE,
          .actor = c->actor,
          .target = {.kind = CD_TARGET_NODE, .entity_id = c->target}};
      CdStatus status = cd_world_preview(w, &r, out, d);
      if (status != CD_OK)
        return status;
    }
    break;
  case CD_CMD_ENTER:
  case CD_CMD_EXIT:
    if (!p || cd_door_index(w->content, c->target) == CD_NONE || c->flags > 3)
      FAIL(CD_UNKNOWN_REFERENCE, "Actor and reviewed door reference required");
    if (!w->content->doors[cd_door_index(w->content, c->target)].reviewed)
      FAIL(CD_UNSUPPORTED, "Door remains unqualified");
    break;
  case CD_CMD_CLOSE_EDGE:
    if (cd_edge_index(w->content, c->target) == CD_NONE || c->flags > 1)
      FAIL(CD_UNKNOWN_REFERENCE,
           "Known source edge and boolean closure required");
    break;
  case CD_CMD_LOCK:
    if (!building || c->flags > 1)
      FAIL(CD_UNKNOWN_REFERENCE, "Building and boolean lock required");
    break;
  case CD_CMD_TRAIN:
  case CD_CMD_AUTHORIZE:
    if (!p || p->civil.age < 18 ||
        c->units > (c->kind == CD_CMD_TRAIN ? 15u : 63u))
      FAIL(CD_INVALID_ARGUMENT,
           "Adult and bounded explicit skills/permissions required");
    break;
  case CD_CMD_EMPLOYER:
    if (!building || !w->content->polygons[b].capacity_known || !c->units ||
        c->units > cd_capacity(w, b) || !c->label[0])
      FAIL(CD_CAPACITY, "Known premises capacity and named employer required");
    break;
  case CD_CMD_JOB_ASSIGN:
    if (!p || !cd_job(w, c->target) || c->flags > 1 ||
        (c->subject.person_id && !cd_citizen(w, c->subject)))
      FAIL(CD_UNKNOWN_REFERENCE,
           "Live assignment actor/mentor and vacancy required");
    break;
  case CD_CMD_WORK:
    if (!p || !cd_job(w, c->target) || c->value > CD_RECIPE_REFINED ||
        !c->units || c->units > 1000 ||
        (c->value == CD_RECIPE_REFINED &&
         (!c->secondary || c->secondary > cd_dsl_recipe_count())))
      FAIL(CD_INVALID_ARGUMENT, "Supported recipe, actor and job required");
    break;
  case CD_CMD_WORK_CANCEL:
    if (!p || !c->target || p->work.view.id != c->target)
      FAIL(CD_UNKNOWN_REFERENCE, "Actor's work ID required");
    break;
  case CD_CMD_GOODS_IMPORT:
    if (!building || c->value >= CD_GOODS || !c->units ||
        c->units > 1000000000 ||
        w->premises[b].goods[c->value] + w->premises[b].reserved[c->value] +
                c->units >
            1000000000)
      FAIL(CD_CAPACITY, "Finite goods capacity required");
    break;
  case CD_CMD_FOOD_IMPORT:
    if (!building || c->flags > 1 || (!c->units && !c->value) ||
        c->units > 4096 || c->value > 4096 ||
        (uint64_t)w->premises[b].meals + w->premises[b].reserved_meals +
                c->units >
            4096 ||
        (uint64_t)w->premises[b].raw + w->premises[b].reserved_raw + c->value >
            4096 ||
        UINT64_MAX - w->food.imported_meals < c->units ||
        UINT64_MAX - w->food.imported_raw < c->value)
      FAIL(CD_CAPACITY, "Finite food import capacity required");
    break;
  case CD_CMD_GOODS_ORDER:
    if (!p || !building || c->value >= CD_GOODS || !c->units ||
        c->units > 4096 || c->flags > 1 || p->inside != b || p->door != CD_NONE)
      FAIL(CD_INVALID_STATE,
           "Physically present capable cargo actor and bounded load required");
    break;
  case CD_CMD_MONEY_IMPORT:
    if (c->flags > 2 || !c->units || (c->flags == 0 && !p) ||
        (c->flags == 1 && (!c->target || c->target > w->home_count)) ||
        (c->flags == 2 && (!building || !w->premises[b].employer)) ||
        w->economy.money_created > UINT64_C(1000000000000) - c->units)
      FAIL(CD_CAPACITY,
           "Known account and finite technical token import required");
    break;
  case CD_CMD_PROJECT:
    if (!building || cd_reviewed_door(w, c->target) == CD_NONE ||
        c->flags > CD_DEV_USE || !c->duration ||
        cd_pending(w->projects[b].view.state) ||
        !w->premises[b].goods[CD_TOOL] ||
        w->premises[b].goods[CD_WOOD] <
            (c->flags == CD_DEV_CAPACITY ? 2u : 1u) ||
        (c->flags == CD_DEV_CAPACITY &&
         (w->premises[b].goods[CD_CLAY] < 2 || !c->units || c->units > 128)) ||
        (c->flags == CD_DEV_USE && !c->label[0]))
      FAIL(CD_CAPACITY,
           "Reviewed worksite, materials and bounded project required");
    break;
  case CD_CMD_PROJECT_CANCEL:
    if (!building || !c->secondary || w->projects[b].view.id != c->secondary)
      FAIL(CD_UNKNOWN_REFERENCE, "Existing site project ID required");
    break;
  case CD_CMD_DISPATCH: {
    CdJob *j = NULL;
    CdCitizen *worker;
    unsigned caps;
    if (c->flags > 1 || c->value >= CD_SERVICE_KINDS || !c->duration ||
        !c->units || c->units > 10080)
      FAIL(CD_INVALID_ARGUMENT, "Bounded ordinary response required");
    cd_provider(w, c->flags, c->target, &j, &worker, &caps);
    if (!j || !(caps & (1u << c->value)) ||
        cd_reviewed_door(w, c->secondary) == CD_NONE)
      FAIL(CD_UNSUPPORTED,
           "Known provider capability and reviewed destination required");
    if (w->premises[cd_polygon_index(w->content, j->view.workplace)].treasury <
        (uint64_t)j->view.wage * c->duration)
      FAIL(CD_CAPACITY, "Response wages unavailable");
    break;
  }
  case CD_CMD_SERVICE_CANCEL:
    if (!c->target || c->target > w->service_count)
      FAIL(CD_UNKNOWN_REFERENCE, "Known dispatch required");
    break;
  case CD_CMD_GATHERING:
    if (!c->target || c->target > w->office_count ||
        c->value >= CD_SERVICE_KINDS || !c->duration || !c->units ||
        c->units > CD_ATTENDANCE_LIMIT || c->starts <= cd_now(w) ||
        c->flags > 1)
      FAIL(CD_INVALID_ARGUMENT, "Known office, future civil-minute schedule "
                                "and attendance capacity required");
    break;
  case CD_CMD_ATTEND:
    if (!p || !c->target || c->target > w->gathering_count || c->flags > 1)
      FAIL(CD_UNKNOWN_REFERENCE, "Live attendee and gathering required");
    break;
  case CD_CMD_PRESSURE:
    if (c->flags >= CD_PRESSURE_KINDS || !c->duration ||
        c->starts <= cd_now(w) || !c->value ||
        c->value >
            (c->flags == CD_PRESSURE_RAIN     ? 3u
             : c->flags == CD_PRESSURE_RATION ? 12u
             : c->flags == CD_PRESSURE_FIRE || c->flags == CD_PRESSURE_DAMAGE
                 ? 1000u
                 : 1u))
      FAIL(CD_INVALID_ARGUMENT, "Explicit future bounded pressure required");
    if ((c->flags == CD_PRESSURE_FIRE || c->flags == CD_PRESSURE_DAMAGE) &&
        !building)
      FAIL(CD_UNKNOWN_REFERENCE, "Source building required");
    if (c->flags == CD_PRESSURE_EDGE_CLOSURE &&
        cd_edge_index(w->content, c->target) == CD_NONE)
      FAIL(CD_UNKNOWN_REFERENCE, "Source walk edge required");
    if (c->flags != CD_PRESSURE_FIRE && c->flags != CD_PRESSURE_DAMAGE &&
        c->flags != CD_PRESSURE_EDGE_CLOSURE && c->target)
      FAIL(CD_INVALID_ARGUMENT, "Pressure is city-wide");
    break;
  case CD_CMD_PRESSURE_CANCEL:
    if (!c->target || c->target > w->pressure_count)
      FAIL(CD_UNKNOWN_REFERENCE, "Known pressure required");
    break;
  case CD_CMD_SHELTER:
    if (!building || cd_reviewed_door(w, c->target) == CD_NONE || !c->units ||
        c->units > cd_capacity(w, b))
      FAIL(CD_CAPACITY, "Reviewed shelter premises/capacity required");
    break;
  case CD_CMD_SHELTER_ASSIGN:
    if (!p || (c->target && (!building || !w->premises[b].shelter_capacity)))
      FAIL(CD_UNKNOWN_REFERENCE, "Live person and configured shelter required");
    break;
  case CD_CMD_INSTITUTION_LINK:
    if (cd_site_index(w->content, c->target) == CD_NONE ||
        cd_reviewed_door(w, c->secondary) == CD_NONE)
      FAIL(CD_UNKNOWN_REFERENCE,
           "Known institution and reviewed premises required");
    break;
  case CD_CMD_INSTITUTION_OPERATE:
    if (cd_site_index(w->content, c->target) == CD_NONE || c->flags > 3 ||
        !c->label[0])
      FAIL(CD_UNKNOWN_REFERENCE,
           "Known institution and explicit office/operation required");
    break;
  case CD_CMD_JOB_CREATE: {
    CdOccupationView role;
    if (!building || !w->premises[b].employer ||
        cd_door_index(w->content, c->secondary) == CD_NONE ||
        !w->content->doors[cd_door_index(w->content, c->secondary)].reviewed ||
        w->content->doors[cd_door_index(w->content, c->secondary)].building !=
            b ||
        !c->units || cd_occupation(c->units - 1, &role, NULL) != CD_OK ||
        !role.enabled || c->value > 1000 || c->starts >= 1440 ||
        c->duration >= 1440 || c->flags > 3 ||
        (c->auxiliary && cd_site_index(w->content, c->auxiliary) == CD_NONE))
      FAIL(CD_INVALID_ARGUMENT,
           "Qualified employer/door, role, shifts and wage required");
    break;
  }
  case CD_CMD_JOB_UNASSIGN:
  case CD_CMD_JOB_CLOSE:
    if (!cd_job(w, c->target))
      FAIL(CD_UNKNOWN_REFERENCE, "Known job required");
    break;
  case CD_CMD_ROSTER:
    if (!cd_job(w, c->target) || !c->units ||
        c->units >= (1u << CD_SERVICE_KINDS) || !c->label[0])
      FAIL(CD_INVALID_ARGUMENT,
           "Known job, named roster and capability mask required");
    break;
  case CD_CMD_OFFICE:
    if (cd_site_index(w->content, c->target) == CD_NONE ||
        !cd_job(w, c->secondary) || c->flags >= CD_OFFICE_KINDS)
      FAIL(CD_INVALID_ARGUMENT,
           "Known institution, job and ordinary office title required");
    break;
  case CD_CMD_QUALIFY:
    if (!p || cd_site_index(w->content, c->target) == CD_NONE ||
        c->flags > CD_OFFICE_RABBI)
      FAIL(CD_INVALID_ARGUMENT,
           "Live person and institution/title qualification required");
    break;
  case CD_CMD_MEMBERSHIP:
    if (!p || cd_site_index(w->content, c->target) == CD_NONE || c->flags > 1)
      FAIL(CD_INVALID_ARGUMENT,
           "Live person and explicit membership intention required");
    break;
  case CD_CMD_FOOD_STORE:
    if (!building || c->flags > 1)
      FAIL(CD_INVALID_ARGUMENT, "Source premises and producer flag required");
    break;
  case CD_CMD_CLINIC:
    if (!building || !c->units || c->units > cd_capacity(w, b) ||
        c->value > 4096)
      FAIL(CD_CAPACITY,
           "Known clinic bed capacity and finite medicine required");
    break;
  case CD_CMD_WATER_IMPORT:
    if (!building || !c->units || c->units > 16384 ||
        w->premises[b].water > 16384 - c->units)
      FAIL(CD_CAPACITY, "Finite response-water capacity required");
    break;
  case CD_CMD_WEAR:
    if (!building || !c->units || c->units > 1000)
      FAIL(CD_INVALID_ARGUMENT,
           "Source premises and bounded technical wear required");
    break;
  case CD_CMD_EMPLOYER_OPEN:
    if (!building || !w->premises[b].employer || c->flags > 1)
      FAIL(CD_INVALID_ARGUMENT, "Configured employer and open flag required");
    break;
  case CD_CMD_VEHICLE:
    if (!cd_job(w, c->target) || c->flags >= CD_RAIL_VEHICLE || c->units > 16)
      FAIL(CD_UNSUPPORTED,
           "Qualified crew job and technical handcart/stretcher only");
    break;
  case CD_CMD_TRANSPORT:
    if (!c->target || c->target > w->vehicle_count ||
        cd_reviewed_door(w, c->secondary) == CD_NONE ||
        cd_reviewed_door(w, c->auxiliary) == CD_NONE || c->value >= CD_GOODS ||
        c->units > 16 || c->flags > 1)
      FAIL(CD_INVALID_ARGUMENT,
           "Known vehicle, physical endpoints and bounded load required");
    break;
  case CD_CMD_VEHICLE_STOP:
  case CD_CMD_UNLOAD:
    if (!c->target || c->target > w->vehicle_count || c->flags > 1)
      FAIL(CD_UNKNOWN_REFERENCE, "Known vehicle and stop/resume flag required");
    break;
  case CD_CMD_CONDITION:
    if (!p || c->flags > CD_CRITICAL || !c->units || c->units > 1000 ||
        !c->value || c->value > 10080)
      FAIL(CD_INVALID_ARGUMENT,
           "Live subject and bounded ordinary health experiment required");
    break;
  default:
    FAIL(CD_UNSUPPORTED, "Operation is not enabled");
  }
  out->status = CD_OK;
  out->estimated_minutes = (unsigned)c->duration;
  out->estimated_quantity = c->units;
  return cd_diag(d, CD_OK,
                 "Preview valid; execution revalidates current actor, access, "
                 "stock and capacity");
#undef FAIL
}
CdStatus cd_world_operation_queue(CdWorld *w, const CdCommand *c,
                                  CdPermission permission, uint64_t *out,
                                  CdDiagnostic *d) {
  if (!w || !c || !out || !memchr(c->basis, 0, 96) ||
      !memchr(c->label, 0, 96) || permission != CD_PERMISSION_INTERVENTION)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Intervention permission and bounded command required");
  for (size_t i = 0; i < w->operation_count; i++)
    if (w->operations[i].view.command.key == c->key) {
      if (!same(c, &w->operations[i].view.command))
        return cd_diag(d, CD_INVALID_ARGUMENT,
                       "Operation key reused for a different request");
      *out = i + 1;
      return CD_OK;
    }
  CdPreview preview;
  CdStatus status = cd_world_operation_preview(w, c, permission, &preview, d);
  if (status != CD_OK)
    return status;
  if (w->operation_count == CD_OPERATION_LIMIT || w->record_saturated)
    return cd_diag(d, CD_CAPACITY, "Bounded operation/result queue full");
  CdOperationView *v = &w->operations[w->operation_count++].view;
  v->id = w->operation_count;
  v->command = *c;
  v->state = CD_TASK_PENDING;
  v->result = CD_OK;
  v->queued_record =
      cd_record_emit(w, CD_EVENT_OPERATION, c->actor, c->subject, c->target, 0,
                     CD_RECORD_DIAGNOSTIC, c->basis,
                     "Intervention queued; no native time advanced");
  *out = v->id;
  return cd_diag(d, CD_OK, "Operation queued once for closed-minute execution");
}
CdStatus cd_world_operation(const CdWorld *w, uint64_t id, CdOperationView *out,
                            CdDiagnostic *d) {
  if (!w || !out || !id || id > w->operation_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown operation ID");
  *out = w->operations[id - 1].view;
  return cd_diag(d, CD_OK, out->reason);
}
static CdStatus apply(CdWorld *w, const CdCommand *c, uint64_t *result,
                      CdDiagnostic *d) {
  switch (c->kind) {
  case CD_CMD_PLACE:
    return cd_world_place(w, c->actor, c->target, d);
  case CD_CMD_MOVE:
    return cd_world_travel(w, c->actor, c->target, d);
  case CD_CMD_ENTER:
    return cd_world_enter(w, c->actor, c->target, c->flags & 1,
                          (c->flags >> 1) & 1, d);
  case CD_CMD_EXIT:
    return cd_world_exit(w, c->actor, c->target, c->flags & 1,
                         (c->flags >> 1) & 1, d);
  case CD_CMD_CLOSE_EDGE:
    return cd_world_close_edge(w, c->target, c->flags, d);
  case CD_CMD_LOCK:
    return cd_world_lock(w, c->target, c->flags, d);
  case CD_CMD_TRAIN:
    return cd_world_training(w, c->actor, c->units, c->basis, d);
  case CD_CMD_AUTHORIZE:
    return cd_world_authorize(w, c->actor, c->units, c->basis, d);
  case CD_CMD_EMPLOYER:
    return cd_world_employer(w, c->target, c->label, c->units, c->basis, d);
  case CD_CMD_JOB_ASSIGN:
    return cd_world_job_assign(w, c->target, c->actor, c->subject, c->flags, d);
  case CD_CMD_WORK: {
    CdWorkSpec s = {c->target, c->secondary, (CdRecipe)c->value,
                    (unsigned)c->units, c->basis};
    return cd_world_work_submit(w, c->actor, c->key, &s, result, d);
  }
  case CD_CMD_WORK_CANCEL:
    return cd_world_work_cancel(w, c->actor, c->target, d);
  case CD_CMD_GOODS_IMPORT:
    return cd_world_goods_import(w, c->target, c->value, c->units, c->basis, d);
  case CD_CMD_FOOD_IMPORT: {
    CdStatus s = cd_world_food_store(w, c->target, c->flags, c->basis, d);
    return s == CD_OK ? cd_world_food_import(
                            w, (CdFoodEndpoint){CD_FOOD_STORE, c->target},
                            c->units, c->value, c->basis, d)
                      : s;
  }
  case CD_CMD_GOODS_ORDER:
    return cd_world_goods_order(w, c->actor, c->target, c->value, c->units,
                                c->flags, d);
  case CD_CMD_MONEY_IMPORT:
    return cd_world_money_import(
        w, (CdAccount){(CdAccountKind)c->flags, c->target, c->actor}, c->units,
        c->basis, d);
  case CD_CMD_PROJECT: {
    CdDevelopmentSpec s = {(CdDevelopmentKind)c->flags, (unsigned)c->units,
                           (unsigned)c->duration, c->label, c->basis};
    return cd_world_project(w, c->target, c->key, &s, result, d);
  }
  case CD_CMD_PROJECT_CANCEL:
    return cd_world_project_cancel(w, c->target, c->secondary, d);
  case CD_CMD_DISPATCH: {
    CdServiceSpec s = {
        c->target,  (int)c->flags,        (CdServiceKind)c->value, c->secondary,
        c->subject, cd_now(w) + c->units, (unsigned)c->duration,   c->basis};
    return cd_world_dispatch(w, c->key, &s, result, d);
  }
  case CD_CMD_SERVICE_CANCEL:
    return cd_world_service_cancel(w, c->target, d);
  case CD_CMD_GATHERING: {
    CdGatheringSpec s = {c->target,
                         (CdServiceKind)c->value,
                         c->starts,
                         (unsigned)c->duration,
                         (unsigned)c->units,
                         0,
                         1,
                         (int)c->flags,
                         c->basis};
    return cd_world_gathering(w, c->key, &s, result, d);
  }
  case CD_CMD_ATTEND:
    return cd_world_attend(w, c->target, c->actor, c->flags, d);
  case CD_CMD_PRESSURE: {
    CdPressureSpec s = {(CdPressureKind)c->flags, c->target,          c->starts,
                        (unsigned)c->duration,    (unsigned)c->value, c->basis};
    return cd_world_pressure(w, c->key, &s, result, d);
  }
  case CD_CMD_PRESSURE_CANCEL:
    return cd_world_pressure_cancel(w, c->target, d);
  case CD_CMD_SHELTER:
    return cd_world_shelter(w, c->target, c->units, c->basis, d);
  case CD_CMD_SHELTER_ASSIGN:
    return cd_world_shelter_assign(w, c->actor, c->target, c->basis, d);
  case CD_CMD_INSTITUTION_LINK:
    return cd_world_institution_link(w, c->target, c->secondary, c->basis, d);
  case CD_CMD_INSTITUTION_OPERATE:
    return cd_world_institution_operate(
        w, c->target, c->flags & 1, (c->flags >> 1) & 1, c->label, c->basis, d);
  case CD_CMD_JOB_CREATE: {
    CdJobSpec x = {.workplace = c->target,
                   .entrance = c->secondary,
                   .occupation = c->units,
                   .institution = c->auxiliary,
                   .shift_start = (unsigned)c->starts,
                   .shift_end = (unsigned)c->duration,
                   .wage = (unsigned)c->value,
                   .apprenticeship = c->flags & 1,
                   .has_key = (c->flags >> 1) & 1,
                   .basis = c->basis};
    return cd_world_job_create(w, &x, result, d);
  }
  case CD_CMD_JOB_UNASSIGN:
    return cd_world_job_unassign(w, c->target, d);
  case CD_CMD_JOB_CLOSE:
    return cd_world_job_close(w, c->target, d);
  case CD_CMD_ROSTER: {
    CdRosterSpec x = {c->target, (unsigned)c->units, c->label, c->basis};
    return cd_world_roster(w, &x, result, d);
  }
  case CD_CMD_OFFICE: {
    CdOfficeSpec x = {c->target, c->secondary, (CdOfficeKind)c->flags,
                      c->basis};
    return cd_world_office_create(w, &x, result, d);
  }
  case CD_CMD_QUALIFY:
    return cd_world_religious_qualification(
        w, c->actor, c->target, (CdOfficeKind)c->flags, c->basis, d);
  case CD_CMD_MEMBERSHIP:
    return cd_world_membership(w, c->actor, c->target, c->flags, c->basis, d);
  case CD_CMD_FOOD_STORE:
    return cd_world_food_store(w, c->target, c->flags, c->basis, d);
  case CD_CMD_CLINIC:
    return cd_world_clinic(w, c->target, c->units, c->value, c->basis, d);
  case CD_CMD_WATER_IMPORT:
    return cd_world_water_import(w, c->target, c->units, c->basis, d);
  case CD_CMD_WEAR:
    return cd_world_wear(w, c->target, c->units, c->basis, d);
  case CD_CMD_EMPLOYER_OPEN:
    return cd_world_employer_open(w, c->target, c->flags, d);
  case CD_CMD_VEHICLE: {
    CdVehicleSpec x = {c->target, (CdVehicleKind)c->flags, (unsigned)c->units,
                       c->basis};
    return cd_world_vehicle(w, &x, result, d);
  }
  case CD_CMD_TRANSPORT:
    return cd_world_transport(w, c->target, c->key, c->secondary, c->auxiliary,
                              c->value, c->flags, c->units, c->subject, d);
  case CD_CMD_VEHICLE_STOP:
    return cd_world_vehicle_stop(w, c->target, c->flags, d);
  case CD_CMD_UNLOAD:
    return cd_world_vehicle_unload_here(w, c->target, d);
  case CD_CMD_CONDITION:
    return cd_world_expose(w, c->actor, (CdCondition)c->flags, c->units,
                           c->value, c->basis, d);
  default:
    return CD_UNSUPPORTED;
  }
}
void cd_operations_tick(CdWorld *w) {
  uint64_t now = cd_native_minutes(w->native);
  for (size_t i = 0; i < w->operation_count; i++) {
    CdOperationView *v = &w->operations[i].view;
    if (!cd_pending(v->state))
      continue;
    if (v->command.earliest > now)
      continue;
    uint64_t end = v->command.deadline;
    if (!end) {
      const CdRecord *queued =
          &w->records[(v->queued_record - 1) % CD_RECORD_LIMIT];
      end = queued->id == v->queued_record ? queued->minute + 10080 : 0;
    }
    CdDiagnostic d = {0};
    CdPreview preview;
    CdStatus status =
        end && now < end
            ? cd_world_operation_preview(
                  w, &v->command, CD_PERMISSION_INTERVENTION, &preview, &d)
            : cd_diag(&d, CD_INVALID_STATE,
                      "Operation deadline expired or queue provenance evicted");
    w->current_cause = v->queued_record;
    if (status == CD_OK)
      status = apply(w, &v->command, &v->result_id, &d);
    v->result = status;
    v->state = status == CD_OK ? CD_TASK_DONE : CD_TASK_CANCELLED;
    snprintf(v->reason, sizeof v->reason, "%.159s", d.message);
    v->result_record =
        cd_record_emit(w, CD_EVENT_OPERATION, v->command.actor,
                       v->command.subject, v->command.target, v->queued_record,
                       CD_RECORD_DIAGNOSTIC, v->command.basis, v->reason);
    w->current_cause = 0;
  }
}
CdStatus cd_world_operations_flush(CdWorld *w, CdPermission p,
                                   CdDiagnostic *d) {
  if (!cd_model_ready(w) || p != CD_PERMISSION_INTERVENTION)
    return cd_diag(
        d, CD_UNSUPPORTED,
        "Explicit intervention permission and closed world required");
  unsigned char pending[CD_OPERATION_LIMIT] = {0};
  for (size_t i = 0; i < w->operation_count; i++)
    pending[i] = cd_pending(w->operations[i].view.state);
  cd_operations_tick(w);
  for (size_t i = 0; i < w->operation_count; i++)
    if (pending[i] && !cd_pending(w->operations[i].view.state) &&
        w->operations[i].view.result != CD_OK)
      return cd_diag(d, w->operations[i].view.result,
                     w->operations[i].view.reason);
  return cd_diag(d, CD_OK,
                 "Due operations applied once without advancing native time");
}
CdStatus cd_world_operation_cancel(CdWorld *w, uint64_t id,
                                   CdPermission permission, CdDiagnostic *d) {
  if (!cd_model_ready(w) || permission != CD_PERMISSION_INTERVENTION || !id ||
      id > w->operation_count)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Known operation and intervention permission required");
  CdOperationView *v = &w->operations[id - 1].view;
  if (cd_pending(v->state)) {
    v->state = CD_TASK_CANCELLED;
    v->result_record = cd_record_emit(
        w, CD_EVENT_OPERATION, v->command.actor, v->command.subject,
        v->command.target, v->queued_record, CD_RECORD_DIAGNOSTIC,
        v->command.basis, "Queued intervention cancelled before execution");
    snprintf(v->reason, 160, "cancelled before execution");
  }
  return CD_OK;
}
static int number(const char *s, uint64_t *v) {
  char *end;
  errno = 0;
  if (!s || !*s || *s == '-' || *s == '+')
    return 0;
  for (const char *p = s; *p; p++)
    if (*p < '0' || *p > '9')
      return 0;
  *v = strtoull(s, &end, 10);
  return !errno && !*end;
}
CdStatus cd_command_parse(const CdWorld *w, const char *line, CdCommand *out,
                          CdDiagnostic *d) {
  if (!w || !line || !out || strlen(line) > 1023)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Command line must be at most 1023 bytes");
  char copy[1024];
  snprintf(copy, sizeof copy, "%s", line);
  size_t n = strlen(copy);
  while (n && (copy[n - 1] == '\n' || copy[n - 1] == '\r'))
    copy[--n] = 0;
  char *fields[15], *p = copy;
  for (unsigned i = 0; i < 15; i++) {
    fields[i] = p;
    char *sep = strchr(p, '|');
    if (i == 14) {
      if (sep)
        return cd_diag(d, CD_INVALID_ARGUMENT,
                       "Unexpected extra command fields");
    } else {
      if (!sep)
        return cd_diag(d, CD_INVALID_ARGUMENT,
                       "Expected 15 pipe-delimited command fields");
      *sep = 0;
      p = sep + 1;
    }
  }
  CdCommand c = {0};
  unsigned kind = 0;
  for (; kind < CD_CMD_KINDS; kind++)
    if (!strcmp(fields[1], names[kind]))
      break;
  if (kind == CD_CMD_KINDS)
    return cd_diag(d, CD_UNSUPPORTED, "Unknown ordinary operation name");
  c.kind = (CdCommandKind)kind;
  uint64_t vals[12];
  unsigned positions[] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  for (unsigned i = 0; i < 12; i++)
    if (!number(fields[positions[i]], &vals[i]))
      return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid unsigned command field");
  c.key = vals[0];
  c.target = vals[2];
  c.secondary = vals[3];
  c.units = vals[4];
  c.value = vals[5];
  c.starts = vals[6];
  c.duration = vals[7];
  c.earliest = vals[8];
  c.deadline = vals[9];
  if (vals[10] > 65535)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Flags outside range or reserved field nonzero");
  c.flags = (unsigned)vals[10];
  c.auxiliary = vals[11];
  if (vals[1] && cd_world_reference(w, vals[1], &c.actor, d) != CD_OK) {
    int archived = 0;
    if (c.kind == CD_CMD_DISPATCH && c.value == CD_SERVICE_BURIAL)
      for (size_t i = 0; i < w->life_count; i++)
        if (w->life[i].ref.person_id == vals[1]) {
          c.actor = w->life[i].ref;
          archived = 1;
          break;
        }
    if (!archived)
      return CD_UNKNOWN_REFERENCE;
  }

  /* For assignment, secondary names the mentor. Dispatch/transport use the
     actor column as recipient/passenger, not as the acting provider. */
  if (c.kind == CD_CMD_JOB_ASSIGN && c.secondary) {
    if (cd_world_reference(w, c.secondary, &c.subject, d) != CD_OK)
      return CD_UNKNOWN_REFERENCE;
    c.secondary = 0;
  }
  if ((c.kind == CD_CMD_DISPATCH || c.kind == CD_CMD_TRANSPORT) && vals[1]) {
    c.subject = c.actor;
    c.actor = (CdPersonRef){0};
  }
  if (strlen(fields[13]) >= 96 || strlen(fields[14]) >= 96)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Operation label/basis too long");
  snprintf(c.label, 96, "%s", fields[13]);
  snprintf(c.basis, 96, "%s", fields[14]);
  *out = c;
  return CD_OK;
}
CdStatus cd_world_run_program(CdWorld *w, const char *path,
                              CdPermission permission, CdDiagnostic *d) {
  FILE *f = fopen(path, "r");
  if (!f)
    return cd_diag(d, CD_IO_ERROR, "Cannot open ordinary operation program");
  char line[1026];
  CdStatus status = CD_OK;
  unsigned lines = 0;
  while (fgets(line, sizeof line, f)) {
    lines++;
    if (lines > 10000 || (!strchr(line, '\n') && !feof(f))) {
      status = cd_diag(d, CD_CAPACITY, "Program line/count bound exceeded");
      break;
    }
    if (line[0] == '#' || line[0] == '\n')
      continue;
    uint64_t before = w->record_next;
    if (!strncmp(line, "step ", 5)) {
      uint64_t count;
      line[strcspn(line, "\r\n")] = 0;
      if (!number(line + 5, &count) || count > 10000) {
        status = CD_INVALID_ARGUMENT;
        break;
      }
      status = cd_world_step(w, (unsigned)count, d);
    } else if (!strcmp(line, "flush\n") || !strcmp(line, "flush")) {
      status = cd_world_operations_flush(w, permission, d);
    } else {
      CdCommand c;
      uint64_t id;
      status = cd_command_parse(w, line, &c, d);
      if (status == CD_OK)
        status = cd_world_operation_queue(w, &c, permission, &id, d);
    }
    if (status == CD_OK)
      for (size_t i = 0; i < w->operation_count; i++) {
        CdOperationView *v = &w->operations[i].view;
        if (v->result_record >= before && v->result != CD_OK) {
          status = cd_diag(d, v->result, v->reason);
          break;
        }
      }
    if (status != CD_OK)
      break;
  }
  if (ferror(f))
    status = CD_IO_ERROR;
  fclose(f);
  return status;
}
