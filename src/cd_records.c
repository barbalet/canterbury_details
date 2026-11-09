#include "cd_city_internal.h"
#include "cd_native.h"
#include <inttypes.h>
#include <stdlib.h>
size_t cd_record_cursor_count(const CdContent *c) {
  return cd_smoke_capacity() + 2 * c->np + c->ns + CD_CIVIC_LIMIT * 5 +
         CD_DISPATCH_LIMIT + CD_VEHICLE_LIMIT;
}
uint64_t cd_record_emit(CdWorld *w, CdEventKind kind, CdPersonRef actor,
                        CdPersonRef subject, CdEntityId entity, uint64_t cause,
                        CdRecordPrivacy privacy, const char *basis,
                        const char *message) {
  if (!w || !w->records)
    return 0;
  if (w->record_next == UINT64_MAX) {
    w->record_saturated = 1;
    if (w->record_dropped != UINT64_MAX)
      w->record_dropped++;
    return 0;
  }
  uint64_t id = w->record_next++;
  CdRecord *r = &w->records[(id - 1) % CD_RECORD_LIMIT];
  memset(r, 0, sizeof *r);
  *r = (CdRecord){.id = id,
                  .minute = w->native ? cd_native_minutes(w->native) : 0,
                  .cause = cause ? cause : w->current_cause,
                  .kind = kind,
                  .privacy = privacy,
                  .actor = actor,
                  .subject = subject,
                  .entity = entity};
  CdCitizen *p = cd_citizen(w, actor);
  r->household = p ? p->civil.household : 0;
  snprintf(r->basis, 96, "%s", basis ? basis : "technical model transition");
  snprintf(r->message, 160, "%s", message ? message : "State changed");
  if (w->record_count < CD_RECORD_LIMIT)
    w->record_count++;
  else
    w->record_dropped++;
  w->last_cause = id;
  return id;
}
CdStatus cd_world_record_summary(const CdWorld *w, CdRecordSummary *out,
                                 CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing record summary");
  *out = (CdRecordSummary){w->record_next - w->record_count, w->record_next,
                           w->record_dropped, w->record_count,
                           w->record_saturated};
  return cd_diag(d, CD_OK,
                 "Bounded causal record retention, with explicit evictions");
}
int cd_record_visible(const CdRecord *r, CdRecordQuery q,
                      CdEntityId household) {
  if ((unsigned)q.access > CD_RECORD_DIAGNOSTIC)
    return 0;
  if (q.access == CD_RECORD_DIAGNOSTIC)
    return 1;
  if (r->privacy == CD_RECORD_PUBLIC)
    return 1;
  if (r->privacy == CD_RECORD_PERSONAL)
    return q.viewer.person_id && r->actor.person_id == q.viewer.person_id &&
           r->actor.world_token == q.viewer.world_token;
  if (r->privacy == CD_RECORD_HOUSEHOLD)
    return q.access >= CD_RECORD_HOUSEHOLD && household &&
           r->household == household;
  return 0;
}
CdStatus cd_world_record(const CdWorld *w, uint64_t id, CdRecordQuery q,
                         CdRecord *out, CdDiagnostic *d) {
  if (!w || !out || !id || id >= w->record_next ||
      id < w->record_next - w->record_count)
    return cd_diag(d, CD_UNKNOWN_REFERENCE,
                   "Record unavailable or explicitly evicted");
  CdCitizen *p = cd_citizen((CdWorld *)w, q.viewer);
  if (q.viewer.person_id && !p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown record viewer");
  const CdRecord *r = &w->records[(id - 1) % CD_RECORD_LIMIT];
  if (!cd_record_visible(r, q, p ? p->civil.household : 0))
    return cd_diag(d, CD_UNSUPPORTED,
                   "Record is outside this observer's knowledge/access scope");
  *out = *r;
  return cd_diag(d, CD_OK,
                 "Copied causal record; references do not authorize retired "
                 "actors to act");
}
CdStatus cd_snapshot_record(const CdSnapshot *s, uint64_t id, CdRecordQuery q,
                            CdRecord *out, CdDiagnostic *d) {
  if (!s || !out || id < s->record_summary.first ||
      id >= s->record_summary.next)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown snapshot record");
  CdEntityId home = 0;
  int found = !q.viewer.person_id;
  for (size_t i = 0; i < s->count; i++)
    if (s->people[i].ref.person_id == q.viewer.person_id &&
        s->people[i].ref.world_token == q.viewer.world_token) {
      home = s->people[i].household;
      found = 1;
      break;
    }
  if (!found)
    return cd_diag(d, CD_UNKNOWN_REFERENCE,
                   "Snapshot viewer belongs to another generation");
  CdRecord *r = &s->records[(id - 1) % CD_RECORD_LIMIT];
  if (!cd_record_visible(r, q, home))
    return cd_diag(d, CD_UNSUPPORTED, "Snapshot record access denied");
  *out = *r;
  return CD_OK;
}
CdStatus cd_world_observe(CdWorld *w, CdPersonRef who, uint64_t id,
                          const char *basis, uint64_t *out, CdDiagnostic *d) {
  CdRecord source;
  if (!cd_model_ready(w) || !out || !cd_basis_valid(basis) ||
      !cd_citizen(w, who))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Live observer and explicit observation basis required");
  CdStatus status = cd_world_record(
      w, id, (CdRecordQuery){who, CD_RECORD_HOUSEHOLD}, &source, d);
  if (status != CD_OK)
    return status;
  if (w->record_saturated)
    return cd_diag(d, CD_CAPACITY, "Record identity space saturated");
  *out = cd_record_emit(w, CD_EVENT_OBSERVATION, who, source.subject,
                        source.entity, id, CD_RECORD_PERSONAL, basis,
                        source.message);
  return CD_OK;
}
CdStatus cd_world_social(const CdWorld *w, CdPersonRef who, CdSocialView *out,
                         CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, who);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown social record");
  *out = (CdSocialView){p->bereavement_event, p->support_until,
                        p->bereavement_event &&
                            cd_native_minutes(w->native) < p->support_until};
  return cd_diag(d, CD_OK,
                 "Ordinary bereavement support context, not a diagnosis or "
                 "forced behavior");
}
CdStatus cd_world_reference(const CdWorld *w, CdPersonId id, CdPersonRef *out,
                            CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing stable-ID lookup");
  for (size_t i = 0; i < w->count; i++)
    if (w->people[i].civil.ref.person_id == id) {
      *out = w->people[i].civil.ref;
      return CD_OK;
    }
  return cd_diag(d, CD_UNKNOWN_REFERENCE,
                 "Stable resident ID is retired or absent");
}
static uint64_t latest(const CdWorld *w, CdEventKind kind, CdEntityId entity) {
  for (uint64_t id = w->record_next; id > w->record_next - w->record_count;) {
    const CdRecord *r = &w->records[(--id - 1) % CD_RECORD_LIMIT];
    if (r->kind == kind && r->entity == entity)
      return id;
  }
  return 0;
}
static uint64_t mix(uint64_t h, uint64_t v) {
  return (h ^ v) * UINT64_C(1099511628211);
}
static void changed(CdWorld *w, CdRecordCursor *c, uint64_t fingerprint,
                    CdEventKind kind, CdEntityId id, CdPersonRef who,
                    uint64_t cause, const char *basis, const char *message) {
  if (c->fingerprint == fingerprint)
    return;
  c->fingerprint = fingerprint;
  c->event = cd_record_emit(w, kind, who, (CdPersonRef){0}, id,
                            cause ? cause : c->event, CD_RECORD_DIAGNOSTIC,
                            basis, message);
}
void cd_records_tick(CdWorld *w) {
  size_t at = 0;
  for (size_t i = 0; i < w->job_count; i++) {
    CdJob *j = &w->jobs[i];
    uint64_t f = mix(mix(j->view.worker.person_id, j->view.mentor.person_id),
                     j->view.open);
    changed(w, &w->record_cursors[at + i], f, CD_EVENT_JOB, j->view.id,
            j->view.worker,
            f != w->record_cursors[at + i].fingerprint
                ? latest(w, CD_EVENT_JOB, j->view.id)
                : 0,
            j->view.basis, "Observed job assignment/vacancy state");
  }
  at += cd_smoke_capacity();
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    uint64_t f =
        mix(mix(p->work.view.id, p->work.view.state), p->health.condition);
    changed(w, &p->record_cursor, f, CD_EVENT_WORK, p->work.view.id,
            p->civil.ref, p->record_cursor.event, p->work.basis,
            "Observed ordinary task/health phase");
  }
  for (uint32_t i = 0; i < w->content->np; i++) {
    CdProject *p = &w->projects[i];
    changed(w, &w->record_cursors[at + i], mix(p->view.id, p->view.state),
            CD_EVENT_DEVELOPMENT, p->view.building, (CdPersonRef){0}, 0,
            p->view.basis, "Observed premises project consequence");
  }
  at += w->content->np;
  for (uint32_t i = 0; i < w->content->np; i++) {
    CdPremises *p = &w->premises[i];
    uint64_t f =
        p->condition == 1000 && !p->fire ? 0 : mix(p->condition, p->fire);
    changed(w, &w->record_cursors[at + i], f, CD_EVENT_PRESSURE,
            w->content->polygons[i].id, (CdPersonRef){0},
            f != w->record_cursors[at + i].fingerprint
                ? latest(w, CD_EVENT_PRESSURE, w->content->polygons[i].id)
                : 0,
            p->fire_basis,
            "Observed premises fire/condition; technical cause only");
  }
  at += w->content->np;
  for (uint32_t i = 0; i < w->content->ns; i++) {
    CdInstitutionRuntime *p = &w->institutions[i];
    uint64_t f = p->known ? mix(p->operating, p->count) + 1 : 0;
    changed(w, &w->record_cursors[at + i], f, CD_EVENT_INSTITUTION,
            w->content->sites[i].id, (CdPersonRef){0}, 0, p->basis,
            "Observed institution operation/precinct state");
  }
  at += w->content->ns;
  for (size_t i = 0; i < w->office_count; i++) {
    CdOfficeView v;
    cd_world_office(w, i + 1, &v, NULL);
    changed(w, &w->record_cursors[at + i],
            mix(v.last_holder.person_id, v.successions), CD_EVENT_INSTITUTION,
            v.institution, v.holder, latest(w, CD_EVENT_JOB, v.job), v.basis,
            "Observed ordinary office succession");
  }
  at += CD_CIVIC_LIMIT;
  for (size_t i = 0; i < w->service_count; i++) {
    CdServiceView *v = &w->services[i].view;
    CdJob *job = NULL;
    CdCitizen *p;
    unsigned caps;
    cd_provider(w, v->religious, v->provider, &job, &p, &caps);
    uint64_t cause = job && mix(v->state, v->worker.person_id) !=
                                 w->record_cursors[at + i].fingerprint
                         ? latest(w, CD_EVENT_JOB, job->view.id)
                         : 0;
    changed(w, &w->record_cursors[at + i], mix(v->state, v->worker.person_id),
            CD_EVENT_SERVICE, v->id, v->worker, cause, v->basis,
            v->reason[0] ? v->reason : "Observed ordinary service phase");
  }
  at += CD_DISPATCH_LIMIT;
  for (size_t i = 0; i < w->gathering_count; i++) {
    CdGatheringView *v = &w->gatherings[i].view;
    changed(w, &w->record_cursors[at + i], mix(v->state, v->occurrence),
            CD_EVENT_GATHERING, v->id, (CdPersonRef){0}, 0, v->basis,
            "Observed gathering window/service continuity");
  }
  at += CD_CIVIC_LIMIT;
  for (size_t i = 0; i < w->vehicle_count; i++) {
    CdVehicleView *v = &w->vehicles[i].view;
    changed(w, &w->record_cursors[at + i], mix(v->phase, v->crew.person_id),
            CD_EVENT_TRANSPORT, v->id, v->crew, 0, v->basis,
            v->reason[0] ? v->reason
                         : "Observed cargo/passenger custody phase");
  }
  at += CD_VEHICLE_LIMIT;
  for (size_t i = 0; i < w->pressure_count; i++) {
    CdPressureView *v = &w->pressures[i].view;
    changed(w, &w->record_cursors[at + i], mix(v->state, v->applied),
            CD_EVENT_PRESSURE, v->target, (CdPersonRef){0}, 0, v->basis,
            "Observed explicitly scripted pressure phase");
  }
}
static uint64_t string_hash(uint64_t h, const char *s) {
  while (*s)
    h = mix(h, (unsigned char)*s++);
  return mix(h, 0);
}
uint64_t cd_observer_digest(const CdWorld *w, uint64_t h) {
  h = mix(h, w->record_next);
  h = mix(h, w->record_dropped);
  h = mix(h, w->record_saturated);
  for (uint64_t id = w->record_next - w->record_count; id < w->record_next;
       id++) {
    const CdRecord *r = &w->records[(id - 1) % CD_RECORD_LIMIT];
    h = mix(h, r->id);
    h = mix(h, r->minute);
    h = mix(h, r->cause);
    h = mix(h, r->kind);
    h = mix(h, r->privacy);
    h = mix(h, r->actor.person_id);
    h = mix(h, r->subject.person_id);
    h = mix(h, r->entity);
    h = mix(h, r->household);
    h = string_hash(h, r->basis);
    h = string_hash(h, r->message);
  }
  for (size_t i = 0; i < w->count; i++) {
    h = mix(h, w->people[i].bereavement_event);
    h = mix(h, w->people[i].support_until);
    h = mix(h, w->people[i].record_cursor.fingerprint);
    h = mix(h, w->people[i].record_cursor.event);
  }
  for (size_t i = 0; i < cd_record_cursor_count(w->content); i++) {
    h = mix(h, w->record_cursors[i].fingerprint);
    h = mix(h, w->record_cursors[i].event);
  }
  h = mix(h, w->last_cause);
  h = mix(h, w->operation_count);
  for (size_t i = 0; i < w->operation_count; i++) {
    const CdOperationView *v = &w->operations[i].view;
    const CdCommand *c = &v->command;
    h = mix(h, v->id);
    h = mix(h, v->state);
    h = mix(h, v->result);
    h = mix(h, v->result_id);
    h = mix(h, v->queued_record);
    h = mix(h, v->result_record);
#define H(x) h = mix(h, c->x)
    H(key);
    H(kind);
    H(actor.person_id);
    H(subject.person_id);
    H(target);
    H(secondary);
    H(auxiliary);
    H(units);
    H(value);
    H(starts);
    H(duration);
    H(earliest);
    H(deadline);
    H(flags);
#undef H
    h = string_hash(h, c->label);
    h = string_hash(h, c->basis);
    h = string_hash(h, v->reason);
  }
  return h;
}
