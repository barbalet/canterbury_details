#include "cd_city_internal.h"
#include "cd_native.h"
int cd_pending(CdTaskState s) {
  return s == CD_TASK_PENDING || s == CD_TASK_WAITING;
}
uint64_t cd_now(const CdWorld *w) {
  return cd_native_minutes(w->native) + w->origin_offset;
}
uint32_t cd_reviewed_door(const CdWorld *w, CdEntityId building) {
  uint32_t b = cd_polygon_index(w->content, building);
  if (b == CD_NONE || !w->content->polygons[b].capacity_known)
    return CD_NONE;
  for (uint32_t i = 0; i < w->content->nd; i++)
    if (w->content->doors[i].building == b && w->content->doors[i].reviewed)
      return i;
  return CD_NONE;
}
int cd_civic_busy(const CdCitizen *p) {
  return p->mission || p->gathering || p->vehicle || p->driving || p->refuge;
}
static int title_supported(const CdSite *s, CdOfficeKind k) {
  if ((unsigned)k >= CD_OFFICE_KINDS)
    return 0;
  if (k == CD_OFFICE_RABBI)
    return !strcmp(s->religion, "judaism") || !strcmp(s->religion, "Jewish");
  if (k == CD_OFFICE_PRIEST)
    return (!strcmp(s->religion, "christianity") ||
            !strcmp(s->religion, "Christian")) &&
           (!strcmp(s->denomination, "anglican") ||
            !strcmp(s->denomination, "roman_catholic") ||
            !strcmp(s->denomination, "catholic"));
  if (k == CD_OFFICE_MINISTER)
    return !strcmp(s->religion, "christianity") ||
           !strcmp(s->religion, "Christian");
  return 1;
}
CdStatus cd_world_authorize(CdWorld *w, CdPersonRef ref, unsigned mask,
                            const char *basis, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !p || p->civil.age < 18 || mask > 63 ||
      !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit adult service credentials required");
  p->credentials = mask;
  snprintf(p->credential_basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Technical service permission recorded; no faith or "
                 "personality inferred");
}
CdStatus cd_world_religious_qualification(CdWorld *w, CdPersonRef ref,
                                          CdEntityId site, CdOfficeKind kind,
                                          const char *basis, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  uint32_t si = w ? cd_site_index(w->content, site) : CD_NONE;
  if (!cd_model_ready(w) || !p || p->civil.age < 18 || si == CD_NONE ||
      (unsigned)kind > CD_OFFICE_RABBI ||
      !title_supported(&w->content->sites[si], kind) || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Source-compatible institution-specific clerical "
                   "qualification required");
  p->ordination_kind = kind;
  p->ordination_institution = site;
  snprintf(p->ordination_basis, 96, "%s", basis);
  return cd_diag(d, CD_OK,
                 "Explicit institution/title qualification; no interchangeable "
                 "authority or inferred membership");
}
static unsigned office_caps(CdOfficeKind k) {
  switch (k) {
  case CD_OFFICE_PRIEST:
  case CD_OFFICE_MINISTER:
  case CD_OFFICE_RABBI:
    return (1u << CD_SERVICE_WORSHIP) | (1u << CD_SERVICE_VISIT) |
           (1u << CD_SERVICE_BURIAL);
  case CD_OFFICE_SEXTON:
    return 1u << CD_SERVICE_BURIAL;
  case CD_OFFICE_STAFF:
  case CD_OFFICE_ADMINISTRATOR:
    return (1u << CD_SERVICE_ADMIN) | (1u << CD_SERVICE_BURIAL);
  case CD_OFFICE_TEACHER:
    return 1u << CD_SERVICE_EDUCATION;
  case CD_OFFICE_CHARITY:
    return (1u << CD_SERVICE_CHARITY) | (1u << CD_SERVICE_MEAL) |
           (1u << CD_SERVICE_VISIT);
  default:
    return 0;
  }
}
static int office_qualified(const CdOffice *o, const CdCitizen *p) {
  if (!p)
    return 0;
  CdOfficeKind k = o->view.kind;
  if (k <= CD_OFFICE_RABBI)
    return p->ordination_institution == o->view.institution &&
           p->ordination_kind == (unsigned)k &&
           (p->skills & CD_SKILL_COMMUNITY);
  if (k == CD_OFFICE_CUSTODIAN)
    return p->skills & CD_SKILL_CRAFT;
  if (k == CD_OFFICE_SEXTON)
    return (p->skills & CD_SKILL_CRAFT) && (p->credentials & CD_AUTH_BURIAL);
  if (!(p->skills & CD_SKILL_COMMUNITY))
    return 0;
  if (k == CD_OFFICE_TEACHER)
    return p->credentials & CD_AUTH_TEACH;
  if (k == CD_OFFICE_ADMINISTRATOR || k == CD_OFFICE_STAFF)
    return p->credentials & CD_AUTH_ADMIN;
  return 1;
}
CdStatus cd_world_office_create(CdWorld *w, const CdOfficeSpec *s,
                                CdEntityId *out, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !s || !out || !cd_basis_valid(s->basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit office definition required");
  uint32_t si = cd_site_index(w->content, s->institution);
  CdJob *j = cd_job(w, s->job);
  if (si == CD_NONE || !j || j->view.institution != s->institution ||
      j->view.apprenticeship ||
      !title_supported(&w->content->sites[si], s->kind) ||
      j->view.occupation != (s->kind <= CD_OFFICE_RABBI ? 7u : 8u))
    return cd_diag(d, CD_UNSUPPORTED,
                   "Office needs compatible source tradition, linked employer "
                   "and ordinary clergy/lay job");
  for (size_t i = 0; i < w->office_count; i++)
    if (w->offices[i].view.job == s->job)
      return cd_diag(d, CD_INVALID_STATE,
                     "Job already has a distinct religious office");
  for (size_t i = 0; i < w->roster_count; i++)
    if (w->rosters[i].view.job == s->job)
      return cd_diag(
          d, CD_INVALID_STATE,
          "Use a distinct job for a religious office and civic authority");
  if (w->office_count == CD_CIVIC_LIMIT)
    return cd_diag(d, CD_CAPACITY, "Office catalogue full");
  CdOffice *o = &w->offices[w->office_count++];
  o->view = (CdOfficeView){.id = w->office_count,
                           .institution = s->institution,
                           .job = s->job,
                           .kind = s->kind,
                           .capabilities = office_caps(s->kind)};
  snprintf(o->view.basis, 96, "%s", s->basis);
  *out = o->view.id;
  return cd_diag(d, CD_OK,
                 "Office defined independently of its current holder");
}
int cd_provider(CdWorld *w, int religious, CdEntityId id, CdJob **job,
                CdCitizen **person, unsigned *caps) {
  *job = NULL;
  *person = NULL;
  *caps = 0;
  if (religious) {
    if (!id || id > w->office_count)
      return 0;
    CdOffice *o = &w->offices[id - 1];
    *job = cd_job(w, o->view.job);
    *caps = o->view.capabilities;
  } else {
    if (!id || id > w->roster_count)
      return 0;
    CdRoster *r = &w->rosters[id - 1];
    *job = cd_job(w, r->view.job);
    *caps = r->view.capabilities;
  }
  CdJob *j = *job;
  if (!j || !j->view.open)
    return 0;
  CdCitizen *p = cd_citizen(w, j->view.worker);
  if (!p || !cd_job_qualified(w, j, p) || p->vehicle)
    return 0;
  uint32_t b = cd_polygon_index(w->content, j->view.workplace);
  if (w->premises[b].closed)
    return 0;
  if (j->view.institution) {
    uint32_t si = cd_site_index(w->content, j->view.institution);
    if (si == CD_NONE || !w->institutions[si].known ||
        !w->institutions[si].operating)
      return 0;
  }
  if (religious && !office_qualified(&w->offices[id - 1], p))
    return 0;
  unsigned required = 0;
  if (*caps & (1u << CD_SERVICE_EDUCATION))
    required |= CD_AUTH_TEACH;
  if (*caps & (1u << CD_SERVICE_ADMIN))
    required |= CD_AUTH_ADMIN;
  if (*caps & (1u << CD_SERVICE_POLICE))
    required |= CD_AUTH_POLICE;
  if (*caps & (1u << CD_SERVICE_FIRE))
    required |= CD_AUTH_FIRE;
  if (!religious && (*caps & (1u << CD_SERVICE_BURIAL)))
    required |= CD_AUTH_BURIAL;
  if ((p->credentials & required) != required)
    return 0;
  if ((*caps & (1u << CD_SERVICE_HEALTH)) && !(p->skills & CD_SKILL_CARE))
    return 0;
  if ((*caps & (1u << CD_SERVICE_FIRE)) && !(p->skills & CD_SKILL_CRAFT))
    return 0;
  if ((*caps & ((1u << CD_SERVICE_EDUCATION) | (1u << CD_SERVICE_ADMIN) |
                (1u << CD_SERVICE_POLICE))) &&
      !(p->skills & CD_SKILL_COMMUNITY))
    return 0;
  *person = p;
  return 1;
}
CdStatus cd_world_office(const CdWorld *world, CdEntityId id, CdOfficeView *out,
                         CdDiagnostic *d) {
  CdWorld *w = (CdWorld *)world;
  if (!w || !out || !id || id > w->office_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown religious office");
  *out = w->offices[id - 1].view;
  CdJob *j;
  CdCitizen *p;
  unsigned caps;
  out->available = cd_provider(w, 1, id, &j, &p, &caps);
  out->holder = out->available ? p->civil.ref : (CdPersonRef){0};
  return cd_diag(d, CD_OK,
                 out->available ? "Qualified ordinary office holder"
                                : "Vacant or unavailable qualified office");
}
void cd_offices_tick(CdWorld *w) {
  for (size_t i = 0; i < w->office_count; i++) {
    CdOffice *o = &w->offices[i];
    CdOfficeView v;
    cd_world_office(w, o->view.id, &v, NULL);
    if (v.holder.person_id &&
        v.holder.person_id != o->view.last_holder.person_id) {
      if (o->view.last_holder.person_id)
        o->view.successions++;
      o->view.last_holder = v.holder;
    }
  }
}
CdStatus cd_world_member(const CdWorld *w, CdPersonRef ref, CdEntityId id,
                         int *out, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !out || cd_site_index(w->content, id) == CD_NONE)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown member/institution");
  *out = 0;
  for (unsigned i = 0; i < 4; i++)
    *out |= p->memberships[i] == id;
  return cd_diag(
      d, CD_OK,
      "Explicit community membership, separate from attendance/office/faith");
}
CdStatus cd_world_membership(CdWorld *w, CdPersonRef ref, CdEntityId id,
                             int join, const char *basis, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!cd_model_ready(w) || !p || cd_site_index(w->content, id) == CD_NONE ||
      (join != 0 && join != 1) || !cd_basis_valid(basis))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Explicit membership operation required");
  for (unsigned i = 0; i < 4; i++)
    if (p->memberships[i] == id) {
      if (!join)
        p->memberships[i] = 0;
      return cd_diag(d, CD_OK, "Membership result already represented");
    }
  if (!join)
    return cd_diag(d, CD_OK, "Membership already absent");
  for (unsigned i = 0; i < 4; i++)
    if (!p->memberships[i]) {
      p->memberships[i] = id;
      return cd_diag(d, CD_OK, "Explicit community membership recorded");
    }
  return cd_diag(d, CD_CAPACITY,
                 "Four explicit community memberships per resident");
}

int cd_office_job_valid(CdWorld *w, CdEntityId job) {
  for (size_t i = 0; i < w->office_count; i++)
    if (w->offices[i].view.job == job) {
      CdJob *j = cd_job(w, job);
      return j &&
             office_qualified(&w->offices[i], cd_citizen(w, j->view.worker));
    }
  return 1;
}
