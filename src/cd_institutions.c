#include "cd_city_internal.h"
uint32_t cd_site_index(const CdContent *c, CdEntityId id) {
  if (c)
    for (uint32_t i = 0; i < c->ns; i++)
      if (c->sites[i].id == id)
        return i;
  return CD_NONE;
}
size_t cd_world_institution_count(const CdWorld *w) {
  return w ? w->content->ns : 0;
}
static void detail(const CdContent *c, const CdInstitutionRuntime *r,
                   uint32_t i, CdInstitutionDetail *out) {
  const CdSite *s = &c->sites[i];
  memset(out, 0, sizeof *out);
  out->id = s->id;
  out->source_id = s->source;
  out->name = s->name;
  out->religion = s->religion;
  out->denomination = s->denomination;
  out->status = s->status;
  out->site_type = s->site_type;
  out->source_record = s->source_record;
  out->location_basis = s->location_basis;
  out->setting_year = s->setting_year;
  out->located = s->located;
  out->identity_verified = s->identity_verified;
  out->review_required = s->review_required;
  out->confidence_known = s->confidence_known;
  out->confidence = s->confidence;
  out->source_position = s->p;
  out->scenario_override = r->override;
  memcpy(out->link_basis, r->link_basis, sizeof out->link_basis);
  out->operation_known = r->known;
  out->operating = r->operating;
  out->premises_count = r->count;
  memcpy(out->premises, r->premises, sizeof out->premises);
  snprintf(out->office, 96, "%s", r->office);
  snprintf(out->operation_basis, 96, "%s", r->basis);
}
CdStatus cd_world_institution(const CdWorld *w, size_t i,
                              CdInstitutionDetail *out, CdDiagnostic *d) {
  if (!w || !out || i >= w->content->ns)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown source institution");
  detail(w->content, &w->institutions[i], (uint32_t)i, out);
  return cd_diag(d, CD_OK,
                 "Source tradition/status/provenance retained; offices, "
                 "premises and operations are separate");
}
CdStatus cd_world_institution_link(CdWorld *w, CdEntityId id,
                                   CdEntityId building, const char *basis,
                                   CdDiagnostic *d) {
  uint32_t si = w ? cd_site_index(w->content, id) : CD_NONE,
           b = w ? cd_polygon_index(w->content, building) : CD_NONE;
  if (!cd_model_ready(w) || si == CD_NONE || b == CD_NONE ||
      !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Unknown institution/premises or missing review basis");
  CdSite *source = &w->content->sites[si];
  if (!source->located || w->content->polygons[b].kind != CD_BUILDING ||
      !w->content->polygons[b].capacity_known)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Unlocated institution or unqualified premises cannot be "
                   "made operational by guessing");
  int door = 0;
  for (uint32_t i = 0; i < w->content->nd; i++)
    door |= w->content->doors[i].reviewed && w->content->doors[i].building == b;
  if (!door)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Premises need a reviewed physical entrance");
  CdInstitutionRuntime *r = &w->institutions[si];
  for (size_t i = 0; i < r->count; i++)
    if (r->premises[i] == building)
      return cd_diag(d, CD_OK, "Premises link already recorded");
  if (r->count == 8)
    return cd_diag(d, CD_CAPACITY,
                   "Bounded institution precinct link list full");
  r->premises[r->count] = building;
  snprintf(r->link_basis[r->count++], 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Explicit scenario precinct membership recorded; source "
                 "identity/confidence not rewritten");
}
CdStatus cd_world_institution_operate(CdWorld *w, CdEntityId id, int active,
                                      int override, const char *office,
                                      const char *basis, CdDiagnostic *d) {
  uint32_t si = w ? cd_site_index(w->content, id) : CD_NONE;
  if (!cd_model_ready(w) || si == CD_NONE || (active != 0 && active != 1) ||
      (override != 0 && override != 1) || !cd_basis_valid(office) ||
      !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Institution operation needs explicit bounded office and "
                   "scenario basis");
  CdSite *s = &w->content->sites[si];
  CdInstitutionRuntime *r = &w->institutions[si];
  if (active) {
    if (!s->located || !r->count)
      return cd_diag(
          d, CD_UNSUPPORTED,
          "Unlocated/unlinked institution cannot claim operating premises");
    if (strcmp(s->status, "active") && strcmp(s->status, "scenario_active") &&
        !override)
      return cd_diag(d, CD_INVALID_STATE,
                     "Source closure/review status requires an explicit "
                     "scenario override");
    for (size_t i = 0; i < r->count; i++)
      if (!cd_usable(w, cd_polygon_index(w->content, r->premises[i])))
        return cd_diag(d, CD_INVALID_STATE, "Linked premises require repair");
  }
  r->known = 1;
  r->operating = active;
  r->override = override;
  snprintf(r->office, 96, "%s", office);
  snprintf(r->basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Current office/operation configured without inventing "
                 "congregation, ownership or source dates");
}
CdStatus cd_snapshot_institution_detail(const CdSnapshot *s, CdEntityId id,
                                        CdInstitutionDetail *out,
                                        CdDiagnostic *d) {
  uint32_t i = s ? cd_site_index(s->content, id) : CD_NONE;
  if (!s || !out || i == CD_NONE)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown snapshot institution");
  detail(s->content, &s->institutions[i], i, out);
  return cd_diag(d, CD_OK,
                 "Immutable institution source and operation snapshot");
}
