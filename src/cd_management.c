#include "cd_dsl_catalogue.h"
#include "cd_city_internal.h"
#include "cd_management.h"
static const char *names[] = {"Jobs",
                              "Work",
                              "Premises and inventories",
                              "Development",
                              "Service rosters",
                              "Service requests",
                              "Institution offices",
                              "Gatherings",
                              "Transport",
                              "Environment",
                              "Operations"};
static void utf8_boundary(char *s) {
  for (size_t i = 0; s[i];) {
    unsigned c = (unsigned char)s[i];
    size_t n = c < 128                ? 1
               : c >= 194 && c <= 223 ? 2
               : c >= 224 && c <= 239 ? 3
               : c >= 240 && c <= 244 ? 4
                                      : 0;
    if (!n) {
      s[i] = 0;
      return;
    }
    for (size_t k = 1; k < n; k++)
      if (!s[i + k] || ((unsigned char)s[i + k] & 192) != 128) {
        s[i] = 0;
        return;
      }
    i += n;
  }
}
const char *cd_management_name(CdManagementKind k) {
  return (unsigned)k < CD_MANAGE_KINDS ? names[k] : "Unknown";
}
CdStatus cd_world_management(const CdWorld *w, CdManagementKind kind,
                             size_t offset, CdManagementRow *rows,
                             size_t capacity, size_t *total, size_t *written,
                             CdDiagnostic *d) {
  if (!cd_model_ready(w) || (unsigned)kind >= CD_MANAGE_KINDS || !total ||
      !written || capacity > 256 || (capacity && !rows))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Closed city and bounded management page required");
  *total = *written = 0;
  size_t count = 0;
  switch (kind) {
  case CD_MANAGE_JOBS:
    count = w->job_count;
    break;
  case CD_MANAGE_WORK:
    count = w->count;
    break;
  case CD_MANAGE_PREMISES:
  case CD_MANAGE_PROJECTS:
    count = w->content->np;
    break;
  case CD_MANAGE_ROSTERS:
    count = w->roster_count;
    break;
  case CD_MANAGE_SERVICES:
    count = w->service_count;
    break;
  case CD_MANAGE_OFFICES:
    count = w->office_count;
    break;
  case CD_MANAGE_GATHERINGS:
    count = w->gathering_count;
    break;
  case CD_MANAGE_VEHICLES:
    count = w->vehicle_count;
    break;
  case CD_MANAGE_PRESSURES:
    count = w->pressure_count;
    break;
  case CD_MANAGE_OPERATIONS:
    count = w->operation_count;
    break;
  default:
    break;
  }
  for (size_t i = 0; i < count; i++) {
    if (kind == CD_MANAGE_WORK && !w->people[i].work.view.id)
      continue;
    if (kind == CD_MANAGE_PREMISES &&
        w->content->polygons[i].kind != CD_BUILDING)
      continue;
    if (kind == CD_MANAGE_PROJECTS && !w->projects[i].view.id)
      continue;
    size_t ordinal = (*total)++;
    if (ordinal < offset || *written == capacity)
      continue;
    CdManagementRow *r = &rows[(*written)++];
    memset(r, 0, sizeof *r);
    r->known = 1;
    snprintf(r->name, sizeof r->name, "%s", names[kind]);
    switch (kind) {
    case CD_MANAGE_JOBS: {
      const CdJobView *v = &w->jobs[i].view;
      CdOccupationView role = {0};
      const CdDslRole *named = cd_dsl_job_role(v->occupation);
      if (!named)
        cd_occupation(v->occupation - 1, &role, NULL);
      r->id = v->id;
      r->actor = v->worker.person_id;
      r->related = v->workplace;
      r->state = v->open;
      r->quantity = v->wage;
      snprintf(r->name, sizeof r->name, "%s", named ? named->title : role.name);
      snprintf(
          r->detail, sizeof r->detail,
          "Worker %llu; premises %llu; shift %u-%u; wage %u model tokens; %s",
          (unsigned long long)r->actor, (unsigned long long)r->related,
          v->shift_start, v->shift_end, v->wage, v->basis);
      break;
    }
    case CD_MANAGE_WORK: {
      const CdWorkView *v = &w->people[i].work.view;
      r->id = v->id;
      r->actor = w->people[i].civil.ref.person_id;
      r->related = v->job;
      r->state = v->state;
      r->quantity = v->progress;
      r->balance = v->reserved_money;
      if (v->recipe == CD_RECIPE_REFINED) {
        CdEntityId index = w->people[i].work.spec.project;
        const CdDslRecipe *recipe = index && index <= cd_dsl_recipe_count()
                                        ? cd_dsl_recipe_at((size_t)index - 1)
                                        : NULL;
        if (recipe)
          snprintf(r->name, sizeof r->name, "%s", recipe->id);
      }
      snprintf(
          r->detail, sizeof r->detail,
          "Person %llu; job %llu; progress %u/%u; paid %llu; reserved %llu; %s",
          (unsigned long long)r->actor, (unsigned long long)v->job, v->progress,
          v->duration, (unsigned long long)v->paid,
          (unsigned long long)v->reserved_money, v->reason);
      break;
    }
    case CD_MANAGE_PREMISES: {
      const CdPolygon *b = &w->content->polygons[i];
      const CdPremises *p = &w->premises[i];
      r->id = b->id;
      r->known = b->capacity_known;
      r->quantity = cd_capacity(w, (uint32_t)i);
      r->balance = p->treasury;
      r->state = p->condition;
      snprintf(r->name, sizeof r->name, "%s", b->name);
      snprintf(
          r->detail, sizeof r->detail,
          "Capacity %s %u; condition %u; employer %s; available meals/raw "
          "%u/%u; "
          "wood/clay/fiber/tool/pot/textile %llu/%llu/%llu/%llu/%llu/%llu; "
          "water %u; medicine %u; treasury/escrow %llu/%llu; policy %s",
          r->known ? "known" : "unknown", (unsigned)r->quantity, p->condition,
          p->employer_name, p->meals, p->raw, (unsigned long long)p->goods[0],
          (unsigned long long)p->goods[1], (unsigned long long)p->goods[2],
          (unsigned long long)p->goods[3], (unsigned long long)p->goods[4],
          (unsigned long long)p->goods[5], p->water, p->medicine,
          (unsigned long long)p->treasury, (unsigned long long)p->escrow_money,
          b->policy_basis);
      break;
    }
    case CD_MANAGE_PROJECTS: {
      const CdDevelopmentView *v = &w->projects[i].view;
      r->id = v->id;
      r->related = v->building;
      r->state = v->state;
      r->quantity = v->progress;
      snprintf(r->detail, sizeof r->detail,
               "Building %llu; kind %u; progress %u/%u; %s; basis %s",
               (unsigned long long)v->building, v->kind, v->progress,
               v->minutes, v->reason, v->basis);
      break;
    }
    case CD_MANAGE_ROSTERS: {
      const CdRosterView *v = &w->rosters[i].view;
      r->id = v->id;
      r->related = v->job;
      r->quantity = v->capabilities;
      snprintf(r->name, sizeof r->name, "%s", v->title);
      snprintf(r->detail, sizeof r->detail,
               "Job %llu; capabilities %u; outputs %llu; basis %s",
               (unsigned long long)v->job, v->capabilities,
               (unsigned long long)v->outputs, v->basis);
      break;
    }
    case CD_MANAGE_SERVICES: {
      const CdServiceView *v = &w->services[i].view;
      r->id = v->id;
      r->actor = v->worker.person_id;
      r->related = v->building;
      r->state = v->state;
      r->quantity = v->progress;
      r->balance = v->reserved_money;
      snprintf(r->detail, sizeof r->detail,
               "Provider %llu; worker %llu; subject %llu; building %llu; kind "
               "%u; progress %u/%u; %s; basis %s",
               (unsigned long long)v->provider,
               (unsigned long long)v->worker.person_id,
               (unsigned long long)v->subject.person_id,
               (unsigned long long)v->building, v->kind, v->progress,
               v->minutes, v->reason, v->basis);
      break;
    }
    case CD_MANAGE_OFFICES: {
      CdOfficeView v;
      cd_world_office(w, i + 1, &v, NULL);
      r->id = v.id;
      r->actor = v.holder.person_id;
      r->related = v.institution;
      r->state = v.available;
      r->quantity = v.successions;
      snprintf(r->detail, sizeof r->detail,
               "Institution %llu; job %llu; title %u; holder %llu; available "
               "%d; successions %llu; basis %s",
               (unsigned long long)v.institution, (unsigned long long)v.job,
               v.kind, (unsigned long long)v.holder.person_id, v.available,
               (unsigned long long)v.successions, v.basis);
      break;
    }
    case CD_MANAGE_GATHERINGS: {
      const CdGatheringView *v = &w->gatherings[i].view;
      r->id = v->id;
      r->related = v->office;
      r->state = v->state;
      r->quantity = v->present;
      r->balance = v->reserved_money;
      snprintf(r->detail, sizeof r->detail,
               "Office %llu; starts %llu civil minutes; occurrence %u/%u; "
               "present %u/%u; progress %u/%u; %s",
               (unsigned long long)v->office, (unsigned long long)v->starts,
               v->occurrence, v->repeats, v->present, v->capacity, v->progress,
               v->minutes, v->reason);
      break;
    }
    case CD_MANAGE_VEHICLES: {
      const CdVehicleView *v = &w->vehicles[i].view;
      r->id = v->id;
      r->actor = v->crew.person_id;
      r->related = v->passenger.person_id;
      r->state = v->phase;
      r->quantity = v->loaded;
      snprintf(r->detail, sizeof r->detail,
               "Crew %llu; passenger %llu; %llu -> %llu; load %u/%u; delivered "
               "%u; %s",
               (unsigned long long)r->actor, (unsigned long long)r->related,
               (unsigned long long)v->source,
               (unsigned long long)v->destination, v->loaded, v->capacity,
               v->delivered, v->reason);
      break;
    }
    case CD_MANAGE_PRESSURES: {
      const CdPressureView *v = &w->pressures[i].view;
      r->id = v->id;
      r->related = v->target;
      r->state = v->state;
      r->quantity = v->value;
      snprintf(r->detail, sizeof r->detail,
               "Kind %u; target %llu; starts %llu civil minutes; duration %u; "
               "intensity %u; basis %s",
               v->kind, (unsigned long long)v->target,
               (unsigned long long)v->starts, v->minutes, v->value, v->basis);
      break;
    }
    case CD_MANAGE_OPERATIONS: {
      const CdOperationView *v = &w->operations[i].view;
      r->id = v->id;
      r->actor = v->command.actor.person_id;
      r->related = v->result_id;
      r->state = v->state;
      r->quantity = v->command.key;
      snprintf(r->name, sizeof r->name, "%s", cd_command_name(v->command.kind));
      snprintf(r->detail, sizeof r->detail,
               "Key %llu; state %u; status %u; result ID %llu; "
               "earliest/deadline %llu/%llu; %s; basis %s",
               (unsigned long long)v->command.key, v->state, v->result,
               (unsigned long long)v->result_id,
               (unsigned long long)v->command.earliest,
               (unsigned long long)v->command.deadline, v->reason,
               v->command.basis);
      break;
    }
    default:
      break;
    }
    utf8_boundary(r->name);
    utf8_boundary(r->detail);
  }
  return cd_diag(d, CD_OK, "Copied management page; no time or RNG consumed");
}
