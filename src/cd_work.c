#include "cd_city_internal.h"
#include "cd_native.h"
#include "cd_dsl_catalogue.h"
#include <stdlib.h>
static int pending(CdTaskState s) {
  return s == CD_TASK_PENDING || s == CD_TASK_WAITING;
}
static void reason(CdWorkTask *t, const char *s) {
  snprintf(t->view.reason, 96, "%s", s);
  t->view.state = CD_TASK_WAITING;
}
static int definition(const CdJob *j, CdRecipe recipe, unsigned *duration,
                      uint64_t inputs[CD_GOODS], uint64_t *raw) {
  *raw = 0;
  memset(inputs, 0, CD_GOODS * sizeof *inputs);
  switch (recipe) {
  case CD_RECIPE_MEAL:
    if (j->view.occupation != 2)
      return 0;
    *duration = 2;
    *raw = 1;
    return 1;
  case CD_RECIPE_POT:
    if (j->view.occupation != 5)
      return 0;
    *duration = 3;
    inputs[CD_WOOD] = 1;
    inputs[CD_CLAY] = 2;
    return 1;
  case CD_RECIPE_TEXTILE:
    if (j->view.occupation != 4)
      return 0;
    *duration = 4;
    inputs[CD_FIBER] = 2;
    return 1;
  case CD_RECIPE_TOOL:
    if (j->view.occupation != 3)
      return 0;
    *duration = 3;
    inputs[CD_WOOD] = 2;
    return 1;
  case CD_RECIPE_SERVICE:
    if (j->view.occupation != 7 && j->view.occupation != 8)
      return 0;
    *duration = 2;
    return 1;
  case CD_RECIPE_DEVELOPMENT:
    if (j->view.occupation != 3)
      return 0;
    *duration = 1;
    return 1;
  case CD_RECIPE_REFINED:
    return 0;
  }
  return 0;
}
void cd_work_release(CdWorld *w, CdCitizen *p, int cancelled) {
  CdWorkTask *t = &p->work;
  if (!pending(t->view.state))
    return;
  CdJob *j = cd_job(w, t->view.job);
  if (!j)
    return;
  uint32_t b = cd_polygon_index(w->content, j->view.workplace);
  CdPremises *site = &w->premises[b];
  if (cancelled && t->spec.recipe == CD_RECIPE_REFINED &&
      !cd_dsl_work_stock(w, b, t, CD_DSL_RETURN))
    abort();
  for (unsigned g = 0; g < CD_GOODS; g++) {
    site->reserved[g] -= t->inputs[g];
    site->goods[g] += t->inputs[g];
    t->inputs[g] = 0;
  }
  site->reserved_raw -= (unsigned)t->raw;
  site->raw += (unsigned)t->raw;
  t->raw = 0;
  site->escrow_money -= t->view.reserved_money;
  site->treasury += t->view.reserved_money;
  t->view.reserved_money = 0;
  t->view.state = cancelled ? CD_TASK_CANCELLED : CD_TASK_DONE;
  if (cancelled)
    w->economy.work_cancelled++;
  snprintf(t->view.reason, 96, "%s",
           cancelled ? "cancelled; unconsumed reserves returned" : "completed");
}
CdStatus cd_world_work_submit(CdWorld *w, CdPersonRef ref, uint64_t key,
                              const CdWorkSpec *s, uint64_t *out,
                              CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w) || !s || !out || !key ||
      !cd_basis_valid(s->basis) || !s->units || s->units > 1000)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid bounded work request");
  CdWorkTask *old = &p->work;
  if (key == old->view.key) {
    if (old->spec.job != s->job || old->spec.project != s->project ||
        old->spec.recipe != s->recipe || old->spec.units != s->units ||
        strcmp(old->basis, s->basis))
      return cd_diag(d, CD_INVALID_ARGUMENT,
                     "Client key reused for different work");
    *out = old->view.id;
    return cd_diag(d, CD_OK,
                   "Existing work result returned without new reservation");
  }
  if (key < old->view.key || pending(old->view.state))
    return cd_diag(d, CD_INVALID_STATE, "Stale key or outstanding work");
  CdJob *j = cd_job(w, s->job);
  if (!j || j->view.worker.person_id != ref.person_id ||
      j->view.worker.world_token != ref.world_token ||
      (!cd_job_qualified(w, j, p) || !cd_office_job_valid(w, j->view.id)))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Work requires this actor's qualified job");
  uint32_t b = cd_polygon_index(w->content, j->view.workplace);
  CdPremises *site = &w->premises[b];
  unsigned duration = 0;
  uint64_t inputs[CD_GOODS] = {0}, raw = 0;
  if (s->recipe == CD_RECIPE_REFINED) {
    const CdDslRecipe *recipe = s->project && s->project <= cd_dsl_recipe_count()
                                   ? cd_dsl_recipe_at((size_t)s->project - 1)
                                   : NULL;
    const CdDslRole *role = cd_dsl_job_role(j->view.occupation);
    const char *bound = cd_dsl_role_recipe(role);
    if (!recipe || !recipe->worker_minutes ||
        strcmp(recipe->power, "manual_power_technical_trial") ||
        !(role ? (role->stage <= 3 && bound && !strcmp(bound, recipe->id)) :
                 ((j->view.occupation == 3 && !strcmp(recipe->id, "cut_timber")) ||
                  (j->view.occupation == 4 && !strcmp(recipe->id, "cut_cloth")))))
      return cd_diag(d, CD_UNSUPPORTED, "Refined recipe needs a compatible enabled occupation");
    duration = recipe->worker_minutes;
  } else if (!definition(j, s->recipe, &duration, inputs, &raw))
    return cd_diag(d, CD_UNSUPPORTED,
                   "Recipe is not supported by this occupation");
  CdProject *project = &w->projects[b];
  if (s->recipe == CD_RECIPE_DEVELOPMENT) {
    if (!s->project || project->view.id != s->project ||
        !pending(project->view.state))
      return cd_diag(d, CD_UNKNOWN_REFERENCE,
                     "Unknown active worksite project");
  } else if (s->project && s->recipe != CD_RECIPE_REFINED)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Non-development work cannot name a project");
  duration *= s->units;
  if (duration > 10080)
    return cd_diag(d, CD_CAPACITY, "Work duration exceeds bound");
  uint64_t mentor_rate = 0;
  if (j->view.apprenticeship) {
    CdCitizen *m = cd_citizen(w, j->view.mentor);
    for (unsigned slot = 0; slot < 2; slot++) {
      CdJob *mj = cd_job(w, m->jobs[slot]);
      if (cd_job_mentor_matches(j, mj)) {
        mentor_rate = mj->view.wage;
        break;
      }
    }
  }
  uint64_t cost = (j->view.wage + mentor_rate) * duration;
  if (cost > site->treasury || w->next_work == UINT64_MAX)
    return cd_diag(d, CD_CAPACITY, "Employer cannot reserve wages or work IDs");
  for (unsigned g = 0; g < CD_GOODS; g++) {
    inputs[g] *= s->units;
    if (inputs[g] > site->goods[g])
      return cd_diag(d, CD_CAPACITY, "Finite material input unavailable");
  }
  raw *= s->units;
  if (raw > site->raw ||
      (s->recipe == CD_RECIPE_MEAL && (!site->producer || !site->enabled)))
    return cd_diag(d, CD_CAPACITY,
                   "Configured finite food producer/raw inputs unavailable");
  CdWorkTask task = {0};
  task.spec = *s;
  task.spec.basis = NULL;
  snprintf(task.basis, 96, "%s", s->basis);
  task.view = (CdWorkView){.id = w->next_work,
                           .key = key,
                           .state = CD_TASK_PENDING,
                           .job = s->job,
                           .project = s->project,
                           .recipe = s->recipe,
                           .duration = duration};
  task.view.reserved_money = cost;
  task.rate = j->view.wage;
  task.mentor_rate = mentor_rate;
  task.mentor = j->view.mentor;
  task.raw = raw;
  memcpy(task.inputs, inputs, sizeof inputs);
  if (s->recipe == CD_RECIPE_REFINED && !cd_dsl_work_stock(w, b, &task, CD_DSL_HOLD))
    return cd_diag(d, CD_CAPACITY, "Refined inputs, equipment or output capacity unavailable");
  w->next_work++;
  for (unsigned g = 0; g < CD_GOODS; g++) {
    site->goods[g] -= inputs[g];
    site->reserved[g] += inputs[g];
  }
  site->raw -= (unsigned)raw;
  site->reserved_raw += (unsigned)raw;
  site->treasury -= cost;
  site->escrow_money += cost;
  p->work = task;
  *out = task.view.id;
  return cd_diag(d, CD_OK,
                 "Inputs and wages reserved once; work begins only at its "
                 "actual workplace");
}
CdStatus cd_world_refined_work_submit(CdWorld *w, CdPersonRef worker, uint64_t key,
                                       CdEntityId job, const char *name,
                                       unsigned batches, const char *basis,
                                       uint64_t *out, CdDiagnostic *d) {
  const CdDslRecipe *recipe = cd_dsl_recipe(name);
  if (!recipe)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown refined recipe");
  CdEntityId index = (CdEntityId)(recipe - cd_dsl_recipe_at(0)) + 1;
  CdWorkSpec spec = {job, index, CD_RECIPE_REFINED, batches, basis};
  return cd_world_work_submit(w, worker, key, &spec, out, d);
}

CdStatus cd_world_work(const CdWorld *w, CdPersonRef ref, CdWorkView *out,
                       CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown worker");
  *out = p->work.view;
  return cd_diag(d, CD_OK, p->work.view.reason);
}
CdStatus cd_world_work_cancel(CdWorld *w, CdPersonRef ref, uint64_t id,
                              CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w) || p->work.view.id != id || !id)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown work ID");
  cd_work_release(w, p, 1);
  return cd_diag(d, CD_OK,
                 "Cancellation is idempotent; performed wages are retained");
}
static int workplace_open(const CdWorld *w, const CdJob *j) {
  uint32_t b = cd_polygon_index(w->content, j->view.workplace);
  if (w->premises[b].closed)
    return 0;
  if (j->view.institution) {
    uint32_t si = cd_site_index(w->content, j->view.institution);
    if (si == CD_NONE || !w->institutions[si].known ||
        !w->institutions[si].operating)
      return 0;
  }
  return 1;
}
void cd_jobs_tick(CdWorld *w) {
  CdCivilTime clock;
  cd_smoke_civil_time(cd_native_minutes(w->native) + w->origin_offset, &clock,
                      NULL);
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    CdJob *attendance = cd_job(w, p->jobs[0]);
    if (attendance && cd_sector_job(attendance->view.occupation) &&
        cd_job_qualified(w, attendance, p) && workplace_open(w, attendance) &&
        cd_shift(attendance->view.shift_start, attendance->view.shift_end,
                 clock.minute_of_day) &&
        p->inside != CD_NONE && p->door == CD_NONE &&
        w->content->polygons[p->inside].id == attendance->view.workplace) {
      CdPremises *employer = &w->premises[p->inside];
      if (employer->treasury >= attendance->view.wage) {
        employer->treasury -= attendance->view.wage;
        p->wallet += attendance->view.wage;
      }
    }
    CdWorkTask *t = &p->work;
    if (!pending(t->view.state))
      continue;
    CdJob *j = cd_job(w, t->view.job);
    if (!j || !cd_job_qualified(w, j, p) ||
        !cd_office_job_valid(w, j->view.id) ||
        j->view.worker.person_id != p->civil.ref.person_id) {
      cd_work_release(w, p, 1);
      continue;
    }
    uint32_t b = cd_polygon_index(w->content, j->view.workplace);
    CdPremises *site = &w->premises[b];
    CdProject *project = &w->projects[b];
    if (t->spec.recipe == CD_RECIPE_DEVELOPMENT &&
        (project->view.id != t->spec.project ||
         !pending(project->view.state))) {
      cd_work_release(w, p, 1);
      continue;
    }
    if (t->spec.recipe == CD_RECIPE_DEVELOPMENT &&
        project->view.progress >= project->view.minutes) {
      reason(t, "project awaits validated publication");
      continue;
    }
    if (!workplace_open(w, j)) {
      reason(t, "employer or institution closed");
      continue;
    }
    if (!cd_shift(j->view.shift_start, j->view.shift_end,
                  clock.minute_of_day)) {
      reason(t, "outside assigned shift");
      continue;
    }
    int repair = t->spec.recipe == CD_RECIPE_DEVELOPMENT &&
                 project->view.kind == CD_DEV_REPAIR;
    if (site->fire || (!cd_usable(w, b) && !repair)) {
      reason(t, "premises require repair");
      continue;
    }
    if (!p->civil.position_known || p->inside != b || p->door != CD_NONE) {
      reason(t, p->reason != CD_ROUTE_OK ? cd_route_reason(p->reason) :
                                            "physical street/entrance arrival required");
      continue;
    }
    if (p->activity != CD_ACTIVITY_IDLE || p->health.severity >= 600) {
      reason(t, "survival, care or travel owns this minute");
      continue;
    }
    unsigned output = CD_GOODS;
    if (t->spec.recipe == CD_RECIPE_POT)
      output = CD_POT;
    if (t->spec.recipe == CD_RECIPE_TOOL)
      output = CD_TOOL;
    if (t->spec.recipe == CD_RECIPE_TEXTILE)
      output = CD_TEXTILE;
    int completes = t->view.progress + 1 == t->view.duration;
    if (completes && t->spec.recipe == CD_RECIPE_REFINED &&
        !cd_dsl_work_stock(w, b, t, CD_DSL_CHECK_OUTPUT)) {
      reason(t, "refined output inventory full");
      continue;
    }
    if (completes && output < CD_GOODS &&
        site->goods[output] + site->reserved[output] + t->spec.units >
            UINT64_C(1000000000)) {
      reason(t, "output inventory full");
      continue;
    }
    if (completes && t->spec.recipe == CD_RECIPE_MEAL &&
        (uint64_t)site->meals + site->reserved_meals + t->spec.units > 4096) {
      reason(t, "meal output capacity full");
      continue;
    }
    CdCitizen *mentor = NULL;
    if (j->view.apprenticeship) {
      mentor = cd_citizen(w, j->view.mentor);
      if (!mentor || mentor->inside != b || mentor->door != CD_NONE ||
          !mentor->civil.position_known ||
          mentor->activity != CD_ACTIVITY_IDLE ||
          mentor->health.severity >= 600) {
        reason(t, "qualified mentor must be present and available");
        continue;
      }
      int on_shift = 0;
      for (unsigned s = 0; s < 2; s++) {
        CdJob *mj = cd_job(w, mentor->jobs[s]);
        if (cd_job_mentor_matches(j, mj) &&
            cd_shift(mj->view.shift_start, mj->view.shift_end,
                     clock.minute_of_day))
          on_shift = 1;
      }
      if (!on_shift) {
        reason(t, "mentor is off shift");
        continue;
      }
    }
    if (!cd_native_can_work(w->native, p->civil.ref) ||
        (mentor && !cd_native_can_work(w->native, mentor->civil.ref))) {
      reason(t, "native minute grant unavailable");
      continue;
    }
    if (UINT64_MAX - p->wallet < t->rate ||
        (mentor && UINT64_MAX - mentor->wallet < t->mentor_rate)) {
      reason(t, "wage account overflow");
      continue;
    }
    if (!cd_native_claim(w->native, p->civil.ref))
      continue;
    if (mentor && !cd_native_claim(w->native, mentor->civil.ref))
      abort();
    p->activity = CD_ACTIVITY_WORK;
    if (mentor)
      mentor->activity = CD_ACTIVITY_WORK;
    t->view.state = CD_TASK_PENDING;
    t->view.reason[0] = 0;
    t->view.progress++;
    uint64_t wages = t->rate + t->mentor_rate;
    t->view.reserved_money -= wages;
    site->escrow_money -= wages;
    p->wallet += t->rate;
    if (mentor)
      mentor->wallet += t->mentor_rate;
    w->economy.wages += wages;
    t->view.paid += wages;
    if (t->spec.recipe == CD_RECIPE_DEVELOPMENT) {
      if (project->view.progress < project->view.minutes)
        project->view.progress++;
    }
    if (t->view.progress == t->view.duration) {
      if (t->spec.recipe == CD_RECIPE_REFINED &&
          !cd_dsl_work_stock(w, b, t, CD_DSL_COMPLETE))
        abort();
      for (unsigned g = 0; g < CD_GOODS; g++) {
        site->reserved[g] -= t->inputs[g];
        w->economy.consumed[g] += t->inputs[g];
        t->inputs[g] = 0;
      }
      if (t->raw) {
        site->reserved_raw -= (unsigned)t->raw;
        t->raw = 0;
        site->meals += t->spec.units;
        w->food.produced += t->spec.units;
      }
      if (output < CD_GOODS) {
        site->goods[output] += t->spec.units;
        w->economy.produced[output] += t->spec.units;
      }
      if (t->spec.recipe == CD_RECIPE_SERVICE) {
        w->economy.services += t->spec.units;
        for (size_t oi = 0; oi < w->office_count; oi++)
          if (w->offices[oi].view.job == j->view.id)
            w->offices[oi].view.outputs += t->spec.units;
      }
      cd_native_remember_work(w->native, p->civil.ref);
      if (mentor)
        cd_native_remember_training(w->native, mentor->civil.ref, p->civil.ref);
      w->economy.work_completed++;
      cd_work_release(w, p, 0);
    }
  }
}
void cd_employment_retire(CdWorld *w, CdCitizen *p, CdRetirement why) {
  cd_work_release(w, p, 1);
  cd_trade_release(w, p);
  p->cargo.remaining = 0;
  cd_dsl_cargo_retire(w, p);
  for (unsigned s = 0; s < 2; s++)
    if (p->jobs[s])
      cd_world_job_unassign(w, p->jobs[s], NULL);
  w->home_wallet[p->civil.household - 1] += p->wallet;
  p->wallet = 0;
  for (unsigned g = 0; g < CD_GOODS; g++) {
    if (why == CD_DEPARTURE)
      w->economy.exported[g] += p->goods[g];
    p->goods[g] = 0;
  }
}
