#include "cd_city_internal.h"
#include "cd_native.h"
#define STOCK_MAX UINT64_C(1000000000)
#define MONEY_MAX UINT64_C(1000000000000)
static int active(CdTaskState s) {
  return s == CD_TASK_PENDING || s == CD_TASK_WAITING;
}
static uint64_t *account(CdWorld *w, CdAccount a) {
  if (!w)
    return NULL;
  switch (a.kind) {
  case CD_ACCOUNT_PERSON: {
    CdCitizen *p = cd_citizen(w, a.person);
    return p ? &p->wallet : NULL;
  }
  case CD_ACCOUNT_HOME:
    return a.id && a.id <= w->home_count ? &w->home_wallet[a.id - 1] : NULL;
  case CD_ACCOUNT_EMPLOYER: {
    uint32_t b = cd_polygon_index(w->content, a.id);
    return b != CD_NONE && w->premises[b].employer ? &w->premises[b].treasury
                                                   : NULL;
  }
  }
  return NULL;
}
CdStatus cd_world_money_import(CdWorld *w, CdAccount a, uint64_t amount,
                               const char *basis, CdDiagnostic *d) {
  uint64_t *balance = account(w, a);
  if (!cd_model_ready(w) || !balance || !amount ||
      amount > MONEY_MAX - w->economy.money_created || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Invalid explicit technical capital import");
  *balance += amount;
  w->economy.money_created += amount;
  return cd_diag(
      d, CD_OK, "Model tokens imported; no historical currency value inferred");
}
CdStatus cd_world_money_transfer(CdWorld *w, CdAccount from, CdAccount to,
                                 uint64_t amount, CdDiagnostic *d) {
  uint64_t *a = account(w, from), *b = account(w, to);
  if (!cd_model_ready(w) || !a || !b || a == b || !amount)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid account transfer");
  if (*a < amount)
    return cd_diag(d, CD_CAPACITY, "Insufficient finite funds");
  *a -= amount;
  *b += amount;
  return cd_diag(
      d, CD_OK,
      "Financial rights transferred without creating money or moving bodies");
}
CdStatus cd_world_balance(const CdWorld *w, CdAccount a, uint64_t *out,
                          CdDiagnostic *d) {
  uint64_t *balance = account((CdWorld *)w, a);
  if (!balance || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown account");
  *out = *balance;
  return cd_diag(d, CD_OK, "Read-only model-token account");
}
CdStatus cd_world_goods_import(CdWorld *w, CdEntityId id, unsigned good,
                               uint64_t amount, const char *basis,
                               CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE ||
      w->content->polygons[b].kind != CD_BUILDING || good >= CD_GOODS ||
      !amount || amount > STOCK_MAX || !cd_basis_valid(basis) ||
      w->premises[b].goods[good] + w->premises[b].reserved[good] + amount >
          STOCK_MAX ||
      UINT64_MAX - w->economy.imported[good] < amount)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Invalid explicit finite material import");
  w->premises[b].goods[good] += amount;
  w->economy.imported[good] += amount;
  return cd_diag(d, CD_OK, "Finite goods imported at named premises");
}
CdStatus cd_world_goods_order(CdWorld *w, CdPersonRef ref, CdEntityId id,
                              unsigned good, unsigned units, int delivery,
                              CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!p || !cd_model_ready(w) || b == CD_NONE || good >= CD_GOODS || !units ||
      units > 4096 || (delivery != 0 && delivery != 1) || p->civil.age < 18 ||
      p->health.severity >= 600 || !p->civil.position_known || p->inside != b ||
      p->door != CD_NONE || !cd_usable(w, b))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Cargo requires capable adult physically inside usable "
                   "source premises");
  if (p->cargo.remaining)
    return cd_diag(d, CD_INVALID_STATE,
                   "Existing cargo intention must complete or cancel");
  p->cargo = (CdCargo){
      .building = id, .good = good, .remaining = units, .delivery = delivery};
  return cd_diag(d, CD_OK, "One carried material unit per shared native grant");
}
CdStatus cd_world_goods_cancel(CdWorld *w, CdPersonRef ref, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown cargo actor");
  p->cargo.remaining = 0;
  return cd_diag(d, CD_OK, "Unperformed cargo units cancelled");
}
CdStatus cd_world_home_goods_order(CdWorld *w, CdPersonRef ref, unsigned good,
                                   unsigned units, int delivery,
                                   CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown household actor");
  CdStatus status =
      cd_world_goods_order(w, ref, p->civil.dwelling, good, units, delivery, d);
  if (status == CD_OK)
    p->cargo.household = p->civil.household;
  return status;
}
CdStatus cd_world_price(CdWorld *w, CdEntityId id, unsigned good, int meals,
                        unsigned price, const char *basis, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || b == CD_NONE || !w->premises[b].employer ||
      good >= CD_GOODS || (meals != 0 && meals != 1) || !price ||
      price > 1000 || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Price requires configured employer and explicit bounded "
                   "technical model tokens");
  if (meals)
    w->premises[b].meal_price = price;
  else
    w->premises[b].prices[good] = price;
  return cd_diag(d, CD_OK,
                 "Technical quote configured; no inferred historical price");
}
CdStatus cd_world_purchase(CdWorld *w, CdPersonRef ref, uint64_t key,
                           CdEntityId id, unsigned good, int meals,
                           unsigned units, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (!p || !cd_model_ready(w) || !key || b == CD_NONE || good >= CD_GOODS ||
      (meals != 0 && meals != 1) || !units || units > 12)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid household purchase");
  CdTradeTask *t = &p->trade;
  if (key == t->view.key) {
    if (t->view.seller != id || t->view.good != good ||
        t->view.meals != meals || t->view.quantity != units)
      return cd_diag(d, CD_INVALID_ARGUMENT,
                     "Client key reused for another purchase");
    return cd_diag(d, CD_OK, "Existing trade returned without a second charge");
  }
  if (key < t->view.key || active(t->view.state))
    return cd_diag(d, CD_INVALID_STATE, "Stale key or pending purchase");
  CdPremises *seller = &w->premises[b];
  if (!seller->employer || seller->closed || !cd_usable(w, b) ||
      p->civil.age < 18 || p->health.severity >= 600 ||
      p->health.mortality_pending || !p->civil.position_known ||
      p->inside != b || p->door != CD_NONE)
    return cd_diag(d, CD_INVALID_STATE,
                   "A capable adult must physically reach the open seller");
  unsigned price = meals ? seller->meal_price : seller->prices[good];
  uint64_t total = (uint64_t)price * units;
  if (!price || w->next_trade == UINT64_MAX ||
      w->home_wallet[p->civil.household - 1] < total ||
      (meals ? (seller->meals < units || p->meals + units > 12)
             : (seller->goods[good] < units || p->goods[good] + units > 16)))
    return cd_diag(d, CD_CAPACITY,
                   "Finite funds, quote, stock, carrying capacity or trade IDs "
                   "unavailable");
  memset(t, 0, sizeof *t);
  t->view = (CdTradeView){.id = w->next_trade++,
                          .key = key,
                          .total = total,
                          .state = CD_TASK_PENDING,
                          .seller = id,
                          .good = good,
                          .quantity = units,
                          .meals = meals};
  t->cost = total;
  t->payer = p->civil.household;
  w->home_wallet[t->payer - 1] -= total;
  seller->escrow_money += total;
  if (meals) {
    seller->meals -= units;
    seller->reserved_meals += units;
  } else {
    seller->goods[good] -= units;
    seller->reserved[good] += units;
  }
  return cd_diag(d, CD_OK, "Quoted goods and household payment reserved once");
}
void cd_trade_release(CdWorld *w, CdCitizen *p) {
  CdTradeTask *t = &p->trade;
  if (!active(t->view.state))
    return;
  uint32_t b = cd_polygon_index(w->content, t->view.seller);
  CdPremises *seller = &w->premises[b];
  seller->escrow_money -= t->cost;
  w->home_wallet[t->payer - 1] += t->cost;
  if (t->view.meals) {
    seller->reserved_meals -= t->view.quantity;
    seller->meals += t->view.quantity;
  } else {
    seller->reserved[t->view.good] -= t->view.quantity;
    seller->goods[t->view.good] += t->view.quantity;
  }
  t->cost = 0;
  t->view.state = CD_TASK_CANCELLED;
  snprintf(t->view.reason, 96, "cancelled; payment and goods returned once");
}
CdStatus cd_world_trade_cancel(CdWorld *w, CdPersonRef ref, uint64_t id,
                               CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p || !cd_model_ready(w) || !id || p->trade.view.id != id)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown purchase");
  cd_trade_release(w, p);
  return cd_diag(d, CD_OK, "Idempotent purchase cancellation");
}
CdStatus cd_world_trade(const CdWorld *w, CdPersonRef ref, CdTradeView *out,
                        CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown purchaser");
  *out = p->trade.view;
  return cd_diag(d, CD_OK, p->trade.view.reason);
}
void cd_economy_tick(CdWorld *w) {
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    CdTradeTask *t = &p->trade;
    if (active(t->view.state)) {
      uint32_t b = cd_polygon_index(w->content, t->view.seller);
      CdPremises *seller = &w->premises[b];
      if (p->civil.household != t->payer || p->civil.age < 18 ||
          p->health.mortality_pending) {
        cd_trade_release(w, p);
      } else if (p->activity == CD_ACTIVITY_IDLE && p->health.severity < 600 &&
                 p->civil.position_known && p->inside == b &&
                 p->door == CD_NONE && cd_usable(w, b) && !seller->closed &&
                 (t->view.meals
                      ? p->meals + t->view.quantity <= 12
                      : p->goods[t->view.good] + t->view.quantity <= 16) &&
                 cd_native_claim(w->native, p->civil.ref)) {
        seller->escrow_money -= t->cost;
        seller->treasury += t->cost;
        w->economy.trade_value += t->cost;
        t->cost = 0;
        if (t->view.meals) {
          seller->reserved_meals -= t->view.quantity;
          p->meals += t->view.quantity;
        } else {
          seller->reserved[t->view.good] -= t->view.quantity;
          p->goods[t->view.good] += t->view.quantity;
        }
        t->view.state = CD_TASK_DONE;
        t->view.reason[0] = 0;
        p->activity = CD_ACTIVITY_WORK;
      } else {
        t->view.state = CD_TASK_WAITING;
        snprintf(t->view.reason, 96,
                 "awaiting physical seller, capacity and native minute "
                 "availability");
      }
    }
    if (!p->cargo.remaining || p->activity != CD_ACTIVITY_IDLE)
      continue;
    uint32_t b = cd_polygon_index(w->content, p->cargo.building);
    unsigned g = p->cargo.good;
    CdPremises *site = &w->premises[b];
    if (p->civil.age < 18 || p->health.severity >= 600 ||
        p->health.mortality_pending || p->inside != b || p->door != CD_NONE ||
        !p->civil.position_known || !cd_usable(w, b))
      continue;
    if (p->cargo.household && p->cargo.household != p->civil.household) {
      p->cargo.remaining = 0;
      continue;
    }
    uint64_t *stock =
        p->cargo.household
            ? &w->home_goods[(p->cargo.household - 1) * CD_GOODS + g]
            : &site->goods[g];
    uint64_t reserved = p->cargo.household ? 0 : site->reserved[g];
    if (p->cargo.delivery ? (!p->goods[g] || *stock + reserved >= STOCK_MAX)
                          : (!*stock || p->goods[g] >= 16))
      continue;
    if (!cd_native_claim(w->native, p->civil.ref))
      continue;
    if (p->cargo.delivery) {
      p->goods[g]--;
      (*stock)++;
    } else {
      (*stock)--;
      p->goods[g]++;
    }
    p->cargo.remaining--;
    p->activity = CD_ACTIVITY_WORK;
  }
}
CdStatus cd_world_economy_ledger(const CdWorld *w, CdEconomyLedger *out,
                                 CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing economy ledger");
  CdEconomyLedger e = w->economy;
  memset(e.stock, 0, sizeof e.stock);
  e.money_stock = 0;
  for (size_t i = 0; i < w->count; i++) {
    e.money_stock += w->people[i].wallet;
    for (unsigned g = 0; g < CD_GOODS; g++)
      e.stock[g] += w->people[i].goods[g];
  }
  for (size_t i = 0; i < w->home_count; i++) {
    e.money_stock += w->home_wallet[i];
    for (unsigned g = 0; g < CD_GOODS; g++)
      e.stock[g] += w->home_goods[i * CD_GOODS + g];
  }
  for (size_t i = 0; i < w->life_count; i++)
    for (unsigned g = 0; g < CD_GOODS; g++)
      e.stock[g] += w->life[i].goods[g];
  for (uint32_t i = 0; i < w->content->np; i++) {
    e.money_stock += w->premises[i].treasury + w->premises[i].escrow_money;
    for (unsigned g = 0; g < CD_GOODS; g++)
      e.stock[g] += w->premises[i].goods[g] + w->premises[i].reserved[g];
  }
  for (size_t i = 0; i < w->vehicle_count; i++)
    if (!w->vehicles[i].view.meals)
      e.stock[w->vehicles[i].view.good] += w->vehicles[i].view.loaded;
  for (unsigned g = 0; g < CD_GOODS; g++)
    if (e.imported[g] + e.produced[g] !=
        e.stock[g] + e.consumed[g] + e.exported[g])
      return cd_diag(d, CD_INVALID_STATE, "Material conservation mismatch");
  if (e.money_created != e.money_stock)
    return cd_diag(d, CD_INVALID_STATE, "Money conservation mismatch");
  *out = e;
  return cd_diag(
      d, CD_OK,
      "Goods, wages, purchases, exports and all escrow balances reconcile");
}
