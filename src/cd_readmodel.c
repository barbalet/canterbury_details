#define _POSIX_C_SOURCE 200809L
#include "cd_city_internal.h"
#include "cd_native.h"
#include <stdlib.h>
#include <unistd.h>
CdStatus cd_camera_transform(CdPoint point, double zoom, CdPoint pan,
                             int inverse, CdPoint *out, CdDiagnostic *d) {
  if (!out || !isfinite(point.x) || !isfinite(point.y) || !isfinite(pan.x) ||
      !isfinite(pan.y) || !isfinite(zoom) || zoom < .05 || zoom > 64 ||
      (inverse != 0 && inverse != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Finite camera coordinates and zoom 0.05..64 required");
  CdPoint p =
      inverse ? (CdPoint){(point.x - pan.x) / zoom, (point.y - pan.y) / zoom}
              : (CdPoint){point.x * zoom + pan.x, point.y * zoom + pan.y};
  if (!isfinite(p.x) || !isfinite(p.y))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Camera transform overflow");
  *out = p;
  return CD_OK;
}
CdStatus cd_world_pick_person(const CdWorld *w, CdPoint point, double radius,
                              CdPersonRef *out, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !out || !isfinite(point.x) || !isfinite(point.y) ||
      !isfinite(radius) || radius < 0)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Closed city and finite source-map pick required");
  *out = (CdPersonRef){0};
  double best = radius;
  for (size_t i = 0; i < w->count; i++) {
    const CdCitizen *p = &w->people[i];
    if (!p->civil.position_known)
      continue;
    CdPoint pos = cd_city_position(w, p);
    double distance = hypot(point.x - pos.x, point.y - pos.y);
    if (distance < best ||
        (distance == best &&
         (!out->person_id || p->civil.ref.person_id < out->person_id))) {
      *out = p->civil.ref;
      best = distance;
    }
  }
  return out->person_id ? CD_OK
                        : cd_diag(d, CD_UNKNOWN_REFERENCE,
                                  "No located person at this map position");
}
CdStatus cd_glyph_location(const CdAgentGlyph *glyphs, size_t count,
                           CdPersonId id, CdPoint *out, CdDiagnostic *d) {
  if ((count && !glyphs) || !id || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Glyph data, stable person ID and output required");
  for (size_t i = 0; i < count; i++)
    if (glyphs[i].ref.person_id == id) {
      if (!glyphs[i].located)
        return cd_diag(d, CD_UNKNOWN_REFERENCE,
                       "Agent has no assigned map position");
      if (!isfinite(glyphs[i].position.x) || !isfinite(glyphs[i].position.y))
        return cd_diag(d, CD_INVALID_DATA, "Nonfinite copied agent position");
      *out = glyphs[i].position;
      return CD_OK;
    }
  return cd_diag(d, CD_UNKNOWN_REFERENCE,
                 "Selected agent is not active in this city");
}
static void quoted(FILE *f, const char *s) {
  fputc('"', f);
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p == '"' || *p == '\\') {
      fputc('\\', f);
      fputc(*p, f);
    } else if (*p < 32)
      fprintf(f, "\\u%04x", *p);
    else
      fputc(*p, f);
  }
  fputc('"', f);
}
CdStatus cd_world_export_records(const CdWorld *w, CdRecordQuery query,
                                 const char *path, CdDiagnostic *d) {
  if (!cd_model_ready(w) || !path || !*path || strlen(path) > 4000 ||
      (unsigned)query.access > CD_RECORD_DIAGNOSTIC)
    return cd_diag(
        d, CD_INVALID_ARGUMENT,
        "Closed city, explicit record scope and bounded export path required");
  if (query.viewer.person_id && !cd_citizen((CdWorld *)w, query.viewer))
    return cd_diag(d, CD_UNKNOWN_REFERENCE,
                   "Record viewer is retired or foreign");
  char temp[4096];
  snprintf(temp, sizeof temp, "%s.tmp.XXXXXX", path);
  int fd = mkstemp(temp);
  if (fd < 0)
    return cd_diag(d, CD_IO_ERROR, "Cannot stage record export");
  FILE *f = fdopen(fd, "w");
  if (!f) {
    close(fd);
    unlink(temp);
    return CD_IO_ERROR;
  }
  fprintf(f,
          "{\"format\":\"canterbury-causal-records\",\"scope\":%u,\"dropped\":%"
          "llu,\"records\":[",
          (unsigned)query.access, (unsigned long long)w->record_dropped);
  int comma = 0;
  for (uint64_t id = w->record_next - w->record_count; id < w->record_next;
       id++) {
    CdRecord r;
    if (cd_world_record(w, id, query, &r, NULL) != CD_OK)
      continue;
    fprintf(f,
            "%s{\"id\":%llu,\"minute\":%llu,\"cause\":%llu,\"kind\":%u,"
            "\"actor\":%llu,\"subject\":%llu,\"entity\":%llu,\"household\":%"
            "llu,\"privacy\":%u,\"basis\":",
            comma ? "," : "", (unsigned long long)r.id,
            (unsigned long long)r.minute, (unsigned long long)r.cause,
            (unsigned)r.kind, (unsigned long long)r.actor.person_id,
            (unsigned long long)r.subject.person_id,
            (unsigned long long)r.entity, (unsigned long long)r.household,
            (unsigned)r.privacy);
    quoted(f, r.basis);
    fputs(",\"message\":", f);
    quoted(f, r.message);
    fputc('}', f);
    comma = 1;
  }
  fputs("]}\n", f);
  int bad = ferror(f) || fflush(f) || fsync(fd);
  if (fclose(f))
    bad = 1;
  if (!bad && rename(temp, path) == 0)
    return cd_diag(d, CD_OK,
                   "Record export published without advancing native state");
  unlink(temp);
  return cd_diag(d, CD_IO_ERROR, "Export failed; preceding file retained");
}
