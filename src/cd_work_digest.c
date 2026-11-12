#include "cd_city_internal.h"
static uint64_t mix(uint64_t h, uint64_t x) {
  return (h ^ x) * UINT64_C(1099511628211);
}
static uint64_t text(uint64_t h, const char *s) {
  while (*s)
    h = mix(h, (unsigned char)*s++);
  return mix(h, 0);
}
uint64_t cd_work_digest(const CdWorld *w, uint64_t h) {
#define H(x) h = mix(h, (uint64_t)(x))
#define T(x) h = text(h, x)
  H(w->next_work);
  H(w->next_trade);
  H(w->next_project);
  H(w->job_count);
  for (size_t i = 0; i < CD_DSL_RESOURCE_LIMIT; ++i) {
    H(w->dsl_imported[i]);
    H(w->dsl_consumed[i]);
    H(w->dsl_produced[i]);
    H(w->dsl_lost[i]);
  }
  for (size_t i = 0; i < w->job_count; i++) {
    const CdJob *j = &w->jobs[i];
    H(j->view.id);
    H(j->view.workplace);
    H(j->view.entrance);
    H(j->view.occupation);
    H(j->view.institution);
    H(j->view.worker.person_id);
    H(j->view.mentor.person_id);
    H(j->view.shift_start);
    H(j->view.shift_end);
    H(j->view.wage);
    H(j->view.open);
    H(j->view.apprenticeship);
    T(j->view.basis);
    H(j->has_key);
  }
  for (size_t i = 0; i < w->count; i++) {
    const CdCitizen *p = &w->people[i];
    H(p->jobs[0]);
    H(p->jobs[1]);
    for (size_t i = 0; i < CD_DSL_CERTIFICATES; ++i)
      H(p->certificates[i].role);
    H(p->wallet);
    H(p->cargo.building);
    H(p->cargo.household);
    H(p->cargo.good);
    H(p->cargo.remaining);
    H(p->cargo.delivery);
    H(p->dsl_cargo.premises);
    H(p->dsl_cargo.carried);
    H(p->dsl_cargo.quantity);
    H(p->dsl_cargo.resource);
    H(p->dsl_cargo.requested);
    H(p->dsl_cargo.delivery);
    H(p->trade.cost);
    H(p->trade.payer);
    H(p->work.view.id);
    H(p->work.view.key);
    H(p->work.view.state);
    H(p->work.view.job);
    H(p->work.view.project);
    H(p->work.view.recipe);
    H(p->work.view.duration);
    H(p->work.view.progress);
    H(p->work.view.paid);
    H(p->work.view.reserved_money);
    T(p->work.view.reason);
    T(p->work.basis);
    H(p->work.spec.job);
    H(p->work.spec.project);
    H(p->work.spec.recipe);
    H(p->work.spec.units);
    H(p->work.raw);
    H(p->work.rate);
    H(p->work.mentor_rate);
    H(p->work.mentor.person_id);
    H(p->trade.view.id);
    H(p->trade.view.key);
    H(p->trade.view.total);
    H(p->trade.view.state);
    H(p->trade.view.seller);
    H(p->trade.view.good);
    H(p->trade.view.quantity);
    H(p->trade.view.meals);
    T(p->trade.view.reason);
    for (unsigned g = 0; g < CD_GOODS; g++) {
      H(p->goods[g]);
      H(p->work.inputs[g]);
    }
  }
  for (size_t i = 0; i < w->home_count; i++) {
    H(w->home_wallet[i]);
    for (unsigned g = 0; g < CD_GOODS; g++)
      H(w->home_goods[i * CD_GOODS + g]);
  }
  for (uint32_t i = 0; i < w->content->np; i++) {
    const CdPremises *p = &w->premises[i];
    const CdProject *project = &w->projects[i];
    H(p->treasury);
    for (size_t i = 0; i < CD_DSL_PREMISES_STOCKS; ++i) {
      const CdDslStock *stock = &p->dsl_stock[i];
      H(stock->resource);
      H(stock->available);
      H(stock->reserved);
      H(stock->key);
      H(stock->work);
      H(stock->settled);
    }
    H(p->escrow_money);
    H(p->reserved_raw);
    H(p->reserved_meals);
    H(p->meal_price);
    H(p->work_capacity);
    H(p->condition);
    H(p->capacity_override);
    H(p->capacity_changed);
    H(p->employer);
    H(p->closed);
    T(p->employer_name);
    T(p->employer_basis);
    T(p->scenario_use);
    H(project->view.id);
    H(project->view.key);
    H(project->view.building);
    H(project->view.state);
    H(project->view.kind);
    H(project->view.capacity);
    H(project->view.minutes);
    H(project->view.progress);
    T(project->view.use);
    T(project->view.basis);
    T(project->view.reason);
    for (unsigned g = 0; g < CD_GOODS; g++) {
      H(p->goods[g]);
      H(p->reserved[g]);
      H(p->prices[g]);
      H(project->materials[g]);
    }
  }
  for (uint32_t i = 0; i < w->content->ns; i++) {
    const CdInstitutionRuntime *r = &w->institutions[i];
    H(r->count);
    H(r->known);
    H(r->operating);
    H(r->override);
    T(r->office);
    T(r->basis);
    for (size_t k = 0; k < r->count; k++) {
      H(r->premises[k]);
      T(r->link_basis[k]);
    }
  }
  H(w->economy.money_created);
  H(w->economy.wages);
  H(w->economy.trade_value);
  H(w->economy.work_completed);
  H(w->economy.work_cancelled);
  H(w->economy.services);
  for (unsigned g = 0; g < CD_GOODS; g++) {
    H(w->economy.imported[g]);
    H(w->economy.produced[g]);
    H(w->economy.consumed[g]);
    H(w->economy.exported[g]);
    for (size_t i = 0; i < w->life_count; i++)
      H(w->life[i].goods[g]);
  }
#undef H
#undef T
  return h;
}
