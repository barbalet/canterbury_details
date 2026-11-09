#define _POSIX_C_SOURCE 200809L
#include "cd_city_internal.h"
#include "cd_native.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <zlib.h>

#include "../build/content/CheckpointABI.inc"

typedef struct {
  uint64_t magic, version, native_abi, world_bytes, digest, content_hash;
  char abi[65], generation[65];
  uint32_t endian, np, ne, ns;
} Header;
typedef struct {
  gzFile file;
  uint64_t hash;
  int bad;
} Stream;
static uint64_t bytes_hash(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  while (n--)
    h = (h ^ *p++) * UINT64_C(1099511628211);
  return h;
}
static int write_bytes(void *ctx, void *data, size_t n) {
  Stream *s = ctx;
  unsigned char *p = data;
  while (n) {
    unsigned part = n > 1048576 ? 1048576 : (unsigned)n;
    if (gzwrite(s->file, p, part) != (int)part) {
      s->bad = 1;
      return 0;
    }
    s->hash = bytes_hash(s->hash, p, part);
    n -= part;
    p += part;
  }
  return 1;
}
static int read_bytes(void *ctx, void *data, size_t n) {
  Stream *s = ctx;
  unsigned char *p = data;
  while (n) {
    unsigned part = n > 1048576 ? 1048576 : (unsigned)n;
    if (gzread(s->file, p, part) != (int)part) {
      s->bad = 1;
      return 0;
    }
    s->hash = bytes_hash(s->hash, p, part);
    n -= part;
    p += part;
  }
  return 1;
}
static int rebase(CdPersonRef *p, uint64_t token, int reading) {
  if (reading && p->world_token)
    return 0;
  p->world_token = p->person_id ? token : 0;
  return 1;
}
static int refs_CdCitizen(CdCitizen *p, uint64_t token, int reading) {
  if (!rebase(&p->civil.ref, token, reading))
    return 0;
  if (!rebase(&p->civil.spouse, token, reading))
    return 0;
  if (!rebase(&p->civil.guardian, token, reading))
    return 0;
  if (!rebase(&p->civil.parents[0], token, reading))
    return 0;
  if (!rebase(&p->civil.parents[1], token, reading))
    return 0;
  if (!rebase(&p->health.carer, token, reading))
    return 0;
  if (!rebase(&p->care_target, token, reading))
    return 0;
  if (!rebase(&p->feed_target, token, reading))
    return 0;
  if (!rebase(&p->work.mentor, token, reading))
    return 0;
  if (p->work.spec.basis)
    return 0;
  return 1;
}
static int refs_CdLifeRecord(CdLifeRecord *p, uint64_t token, int reading) {
  if (!rebase(&p->ref, token, reading))
    return 0;
  if (!rebase(&p->parents[0], token, reading))
    return 0;
  if (!rebase(&p->parents[1], token, reading))
    return 0;
  return 1;
}
static int refs_CdJob(CdJob *p, uint64_t token, int reading) {
  if (!rebase(&p->view.worker, token, reading))
    return 0;
  if (!rebase(&p->view.mentor, token, reading))
    return 0;
  return 1;
}
static int refs_CdPersonRef(CdPersonRef *p, uint64_t token, int reading) {
  if (!rebase(p, token, reading))
    return 0;
  return 1;
}
static int refs_CdOffice(CdOffice *p, uint64_t token, int reading) {
  if (!rebase(&p->view.holder, token, reading))
    return 0;
  if (!rebase(&p->view.last_holder, token, reading))
    return 0;
  return 1;
}
static int refs_CdService(CdService *p, uint64_t token, int reading) {
  if (!rebase(&p->view.worker, token, reading))
    return 0;
  if (!rebase(&p->view.subject, token, reading))
    return 0;
  return 1;
}
static int refs_CdVehicle(CdVehicle *p, uint64_t token, int reading) {
  if (!rebase(&p->view.crew, token, reading))
    return 0;
  if (!rebase(&p->view.passenger, token, reading))
    return 0;
  if (!rebase(&p->requested_passenger, token, reading))
    return 0;
  return 1;
}
static int refs_CdRecord(CdRecord *p, uint64_t token, int reading) {
  if (!rebase(&p->actor, token, reading))
    return 0;
  if (!rebase(&p->subject, token, reading))
    return 0;
  return 1;
}
static int refs_CdOperation(CdOperation *p, uint64_t token, int reading) {
  if (!rebase(&p->view.command.actor, token, reading))
    return 0;
  if (!rebase(&p->view.command.subject, token, reading))
    return 0;
  return 1;
}
static int refs_CdGathering(CdGathering *p, uint64_t token, int reading) {
  if (p->count > CD_ATTENDANCE_LIMIT)
    return 0;
  for (size_t i = 0; i < CD_ATTENDANCE_LIMIT; i++)
    if (!rebase(&p->attendees[i], token, reading))
      return 0;
  return 1;
}
static int city_io(CdWorld *w, Stream *s, int reading, uint64_t token) {
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdCitizen(&copy, token, 1))
        return 0;
      w->people[i] = copy;
    } else {
      copy = w->people[i];
      if (!refs_CdCitizen(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading ? read_bytes(s, w->homes, (w->home_count) * sizeof *w->homes)
                : write_bytes(s, w->homes, (w->home_count) * sizeof *w->homes)))
    return 0;
  if (!(reading ? read_bytes(s, w->home_meals,
                             (w->home_count) * sizeof *w->home_meals)
                : write_bytes(s, w->home_meals,
                              (w->home_count) * sizeof *w->home_meals)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->home_raw, (w->home_count) * sizeof *w->home_raw)
            : write_bytes(s, w->home_raw,
                          (w->home_count) * sizeof *w->home_raw)))
    return 0;
  for (size_t i = 0; i < w->life_count; i++) {
    CdLifeRecord copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdLifeRecord(&copy, token, 1))
        return 0;
      w->life[i] = copy;
    } else {
      copy = w->life[i];
      if (!refs_CdLifeRecord(&copy, 0, 0) ||
          !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading
            ? read_bytes(s, w->premises, (w->content->np) * sizeof *w->premises)
            : write_bytes(s, w->premises,
                          (w->content->np) * sizeof *w->premises)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->housing, (w->content->np) * sizeof *w->housing)
            : write_bytes(s, w->housing,
                          (w->content->np) * sizeof *w->housing)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->visitors, (w->content->np) * sizeof *w->visitors)
            : write_bytes(s, w->visitors,
                          (w->content->np) * sizeof *w->visitors)))
    return 0;
  for (size_t i = 0; i < w->content->np; i++) {
    CdPersonRef copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdPersonRef(&copy, token, 1))
        return 0;
      w->owners[i] = copy;
    } else {
      copy = w->owners[i];
      if (!refs_CdPersonRef(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading
            ? read_bytes(s, w->locks, (w->content->np) * sizeof *w->locks)
            : write_bytes(s, w->locks, (w->content->np) * sizeof *w->locks)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->closed, (w->content->ne) * sizeof *w->closed)
            : write_bytes(s, w->closed, (w->content->ne) * sizeof *w->closed)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->crowd, (w->content->ne) * sizeof *w->crowd)
            : write_bytes(s, w->crowd, (w->content->ne) * sizeof *w->crowd)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->traffic, (w->content->ne) * sizeof *w->traffic)
            : write_bytes(s, w->traffic,
                          (w->content->ne) * sizeof *w->traffic)))
    return 0;
  for (size_t i = 0; i < w->job_count; i++) {
    CdJob copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) || !refs_CdJob(&copy, token, 1))
        return 0;
      w->jobs[i] = copy;
    } else {
      copy = w->jobs[i];
      if (!refs_CdJob(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading
            ? read_bytes(s, w->projects, (w->content->np) * sizeof *w->projects)
            : write_bytes(s, w->projects,
                          (w->content->np) * sizeof *w->projects)))
    return 0;
  if (!(reading ? read_bytes(s, w->institutions,
                             (w->content->ns) * sizeof *w->institutions)
                : write_bytes(s, w->institutions,
                              (w->content->ns) * sizeof *w->institutions)))
    return 0;
  if (!(reading ? read_bytes(s, w->home_wallet,
                             (w->home_count) * sizeof *w->home_wallet)
                : write_bytes(s, w->home_wallet,
                              (w->home_count) * sizeof *w->home_wallet)))
    return 0;
  if (!(reading
            ? read_bytes(s, w->home_goods,
                         (w->home_count * CD_GOODS) * sizeof *w->home_goods)
            : write_bytes(s, w->home_goods,
                          (w->home_count * CD_GOODS) * sizeof *w->home_goods)))
    return 0;
  for (size_t i = 0; i < w->office_count; i++) {
    CdOffice copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) || !refs_CdOffice(&copy, token, 1))
        return 0;
      w->offices[i] = copy;
    } else {
      copy = w->offices[i];
      if (!refs_CdOffice(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading
            ? read_bytes(s, w->rosters, (w->roster_count) * sizeof *w->rosters)
            : write_bytes(s, w->rosters,
                          (w->roster_count) * sizeof *w->rosters)))
    return 0;
  for (size_t i = 0; i < w->service_count; i++) {
    CdService copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdService(&copy, token, 1))
        return 0;
      w->services[i] = copy;
    } else {
      copy = w->services[i];
      if (!refs_CdService(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  for (size_t i = 0; i < w->gathering_count; i++) {
    CdGathering copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdGathering(&copy, token, 1))
        return 0;
      w->gatherings[i] = copy;
    } else {
      copy = w->gatherings[i];
      if (!refs_CdGathering(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  for (size_t i = 0; i < w->vehicle_count; i++) {
    CdVehicle copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdVehicle(&copy, token, 1))
        return 0;
      w->vehicles[i] = copy;
    } else {
      copy = w->vehicles[i];
      if (!refs_CdVehicle(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading ? read_bytes(s, w->pressures,
                             (w->pressure_count) * sizeof *w->pressures)
                : write_bytes(s, w->pressures,
                              (w->pressure_count) * sizeof *w->pressures)))
    return 0;
  for (size_t i = 0; i < CD_RECORD_LIMIT; i++) {
    CdRecord copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) || !refs_CdRecord(&copy, token, 1))
        return 0;
      w->records[i] = copy;
    } else {
      copy = w->records[i];
      if (!refs_CdRecord(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  if (!(reading ? read_bytes(s, w->record_cursors,
                             (cd_record_cursor_count(w->content)) *
                                 sizeof *w->record_cursors)
                : write_bytes(s, w->record_cursors,
                              (cd_record_cursor_count(w->content)) *
                                  sizeof *w->record_cursors)))
    return 0;
  for (size_t i = 0; i < w->operation_count; i++) {
    CdOperation copy;
    if (reading) {
      if (!read_bytes(s, &copy, sizeof copy) ||
          !refs_CdOperation(&copy, token, 1))
        return 0;
      w->operations[i] = copy;
    } else {
      copy = w->operations[i];
      if (!refs_CdOperation(&copy, 0, 0) || !write_bytes(s, &copy, sizeof copy))
        return 0;
    }
  }
  return 1;
}
#ifdef CD_TESTING
static int fail_publication;
void cd_checkpoint_fail_publish(int value) { fail_publication = value; }
#endif
CdStatus cd_world_save(const CdWorld *world, const char *path,
                       CdDiagnostic *d) {
  CdWorld *w = (CdWorld *)world;
  if (!cd_model_ready(w) || !path || !*path || strlen(path) > 4000 ||
      w->current_cause)
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Closed world and bounded checkpoint path required");
  char temporary[4096];
  snprintf(temporary, sizeof temporary, "%s.tmp.XXXXXX", path);
  int fd = mkstemp(temporary);
  if (fd < 0)
    return cd_diag(d, CD_IO_ERROR, "Cannot create checkpoint staging file");
  int writer = dup(fd);
  gzFile file = writer < 0 ? NULL : gzdopen(writer, "wb1");
  if (!file) {
    if (writer >= 0)
      close(writer);
    close(fd);
    unlink(temporary);
    return cd_diag(d, CD_IO_ERROR, "Cannot open checkpoint compressor");
  }
  Stream stream = {file, UINT64_C(1469598103934665603), 0};
  Header h = {0};
  h.magic = UINT64_C(0x315643445043);
  h.version = 1;
  h.native_abi = cd_native_checkpoint_abi();
  h.world_bytes = sizeof(CdWorld);
  h.endian = 0x01020304;
  h.np = w->content->np;
  h.ne = w->content->ne;
  h.ns = w->content->ns;
  h.content_hash = bytes_hash(UINT64_C(1469598103934665603), w->content->file,
                              w->content->file_bytes);
  snprintf(h.abi, 65, "%s", cd_checkpoint_abi);
  memcpy(h.generation, w->content->generation, 65);
  cd_world_digest(w, &h.digest, NULL);
  CdWorld image = *w;
#define ZERO_POINTER(field) image.field = NULL;
  CD_WORLD_POINTERS(ZERO_POINTER)
#undef ZERO_POINTER
  image.heap_count = 0;
  CdStatus status = CD_OK;
  int ok = write_bytes(&stream, &h, sizeof h) &&
           write_bytes(&stream, &image, sizeof image) &&
           city_io(w, &stream, 0, 0);
  if (ok) {
    status = cd_native_checkpoint(w->native, write_bytes, &stream, d);
    ok = status == CD_OK;
  }
  uint64_t checksum = stream.hash;
  if (ok)
    ok = gzwrite(file, &checksum, sizeof checksum) == sizeof checksum;
  int closed = gzclose(file);
  if (closed != Z_OK || fsync(fd))
    ok = 0;
  close(fd);
#ifdef CD_TESTING
  if (fail_publication)
    ok = 0;
#endif
  if (ok && rename(temporary, path) == 0) {
    char directory[4096];
    snprintf(directory, sizeof directory, "%s", path);
    char *slash = strrchr(directory, '/');
    if (slash) {
      if (slash == directory)
        slash[1] = 0;
      else
        *slash = 0;
    } else
      snprintf(directory, sizeof directory, ".");
    int parent = open(directory, O_RDONLY);
    if (parent >= 0) {
      (void)fsync(parent);
      close(parent);
    }
    return cd_diag(d, CD_OK, "Complete atomic city checkpoint published");
  }
  unlink(temporary);
  return cd_diag(
      d, status == CD_CAPACITY ? status : CD_IO_ERROR,
      "Checkpoint staging/publication failed; preceding checkpoint retained");
}
static int scalar_valid(const CdWorld *w) {
  return w->count <= cd_smoke_capacity() &&
         w->home_count <= cd_smoke_capacity() &&
         w->initial_count <= cd_smoke_capacity() &&
         w->life_capacity == cd_smoke_capacity() * 2 &&
         w->life_count <= w->life_capacity &&
         w->job_count <= cd_smoke_capacity() &&
         w->office_count <= CD_CIVIC_LIMIT &&
         w->roster_count <= CD_CIVIC_LIMIT &&
         w->service_count <= CD_DISPATCH_LIMIT &&
         w->gathering_count <= CD_CIVIC_LIMIT &&
         w->vehicle_count <= CD_VEHICLE_LIMIT &&
         w->pressure_count <= CD_CIVIC_LIMIT &&
         w->record_count <= CD_RECORD_LIMIT &&
         w->record_next > w->record_count &&
         w->operation_count <= CD_OPERATION_LIMIT && !w->current_cause &&
         !w->heap_count && w->next_request && w->next_work && w->next_trade &&
         w->next_project;
}
CdStatus cd_checkpoint_validate(CdWorld *, const CdSmokeStage *,
                                CdDiagnostic *);
CdStatus cd_world_load(CdWorld **out, const CdContent *content,
                       const char *path, CdDiagnostic *d) {
  if (!out || !cd_world_is_owner(*out) ||
      !cd_native_checkpoint_owner(*out ? (*out)->native : NULL) || !content ||
      !path || !*path || (*out && !cd_model_ready(*out)))
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Owning world handle, content and checkpoint path required");
  gzFile file = gzopen(path, "rb");
  if (!file)
    return cd_diag(d, CD_IO_ERROR, "Cannot open checkpoint");
  Stream stream = {file, UINT64_C(1469598103934665603), 0};
  Header h;
  CdWorld image;
  CdWorld *w = NULL;
  CdSmokeStage *stage = NULL;
  CdStatus status = CD_INVALID_DATA;
  if (!read_bytes(&stream, &h, sizeof h) ||
      h.magic != UINT64_C(0x315643445043) || h.version != 1 ||
      h.native_abi != cd_native_checkpoint_abi() ||
      h.world_bytes != sizeof(CdWorld) || h.endian != 0x01020304 ||
      memcmp(h.abi, cd_checkpoint_abi, 65) ||
      memcmp(h.generation, content->generation, 65) || h.np != content->np ||
      h.ne != content->ne || h.ns != content->ns ||
      h.content_hash != bytes_hash(UINT64_C(1469598103934665603), content->file,
                                   content->file_bytes) ||
      !read_bytes(&stream, &image, sizeof image) || !scalar_valid(&image))
    goto done;
#define CHECK_POINTER(field)                                                   \
  if (image.field)                                                             \
    goto done;
  CD_WORLD_POINTERS(CHECK_POINTER)
#undef CHECK_POINTER
  CdScenario scenario = {
      image.count, image.seed, {1940, 1, 1, 0}, "synthetic-ordinary-v1"};
  status = cd_world_allocate(content, &scenario, &w, d);
  if (status != CD_OK)
    goto done;
  CdWorld allocated = *w;
  *w = image;
#define KEEP_POINTER(field) w->field = allocated.field;
  CD_WORLD_POINTERS(KEEP_POINTER)
#undef KEEP_POINTER
  if (!city_io(w, &stream, 1, 0)) {
    status = CD_INVALID_DATA;
    goto done;
  }
  status = cd_native_checkpoint_stage(read_bytes, &stream, &stage, d);
  if (status != CD_OK)
    goto done;
  uint64_t checksum = stream.hash, expected;
  unsigned char extra;
  if (gzread(file, &expected, sizeof expected) != sizeof expected ||
      expected != checksum || gzread(file, &extra, 1) != 0 || !gzeof(file)) {
    status = CD_INVALID_DATA;
    goto done;
  }
  if (gzclose(file) != Z_OK) {
    file = NULL;
    status = CD_INVALID_DATA;
    goto done;
  }
  file = NULL;
  status = cd_checkpoint_validate(w, stage, d);
  if (status != CD_OK)
    goto done;
  if (cd_world_state_digest(w, cd_native_stage_digest(stage)) != h.digest) {
    status = CD_INVALID_DATA;
    goto done;
  }
  uint64_t token = cd_native_stage_token(stage);
  for (size_t i = 0; i < w->count; i++)
    refs_CdCitizen(&w->people[i], token, 0);
  for (size_t i = 0; i < w->life_count; i++)
    refs_CdLifeRecord(&w->life[i], token, 0);
  for (size_t i = 0; i < w->content->np; i++)
    refs_CdPersonRef(&w->owners[i], token, 0);
  for (size_t i = 0; i < w->job_count; i++)
    refs_CdJob(&w->jobs[i], token, 0);
  for (size_t i = 0; i < w->office_count; i++)
    refs_CdOffice(&w->offices[i], token, 0);
  for (size_t i = 0; i < w->service_count; i++)
    refs_CdService(&w->services[i], token, 0);
  for (size_t i = 0; i < w->gathering_count; i++)
    refs_CdGathering(&w->gatherings[i], token, 0);
  for (size_t i = 0; i < w->vehicle_count; i++)
    refs_CdVehicle(&w->vehicles[i], token, 0);
  for (size_t i = 0; i < CD_RECORD_LIMIT; i++)
    refs_CdRecord(&w->records[i], token, 0);
  for (size_t i = 0; i < w->operation_count; i++)
    refs_CdOperation(&w->operations[i], token, 0);
  cd_world_publish_checkpoint(out, w, &stage);
  w = NULL;
  status = CD_OK;
done:
  if (file)
    gzclose(file);
  cd_smoke_discard(stage);
  cd_world_destroy_staged(w);
  return cd_diag(
      d, status,
      status == CD_OK
          ? "Complete city restored at the same closed minute; runtime "
            "references refreshed"
          : "Checkpoint rejected; live world and prior file preserved");
}
