#include "cd_scenario_startup.h"
#include "cd_city_internal.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <zlib.h>

static uint32_t read_u32(const unsigned char *bytes) {
  return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
         (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

CdStatus cd_scenario_startup_read(const CdContent *content, const char *path,
                                  CdScenarioStartup *out, CdDiagnostic *d) {
  if (!content || !path || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Startup requires a city and file");
  FILE *file = fopen(path, "rb");
  if (!file)
    return cd_diag(d, CD_IO_ERROR, "Cannot open scenario startup pack");
  unsigned char bytes[128];
  size_t count = fread(bytes, 1, sizeof bytes, file);
  int extra = fgetc(file);
  int failed = ferror(file);
  fclose(file);
  if (failed || count != sizeof bytes || extra != EOF ||
      memcmp(bytes, "CDSTART1", 8) ||
      read_u32(bytes + 8) != (uint32_t)crc32(0, bytes + 12, sizeof bytes - 12))
    return cd_diag(d, CD_INVALID_DATA, "Invalid scenario startup pack");
  const unsigned char *data = bytes + 12;
  uint64_t map_size = read_u32(data) | (uint64_t)read_u32(data + 4) << 32;
  if (map_size != content->file_bytes ||
      read_u32(data + 8) != (uint32_t)crc32(0, content->file, (uInt)map_size))
    return cd_diag(d, CD_INVALID_DATA, "Scenario startup pack belongs to a different map");
  CdScenarioStartup startup = {0};
  startup.scenario = (CdScenario){
      read_u32(data + 12), read_u32(data + 16),
      {1940, read_u32(data + 20), read_u32(data + 24), read_u32(data + 28)},
      "synthetic-ordinary-v1"};
  for (unsigned i = 0; i < 5; ++i)
    startup.cohorts[i] = read_u32(data + 32 + i * 4);
  for (unsigned i = 0; i < 64; ++i) {
    unsigned char c = data[52 + i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return cd_diag(d, CD_INVALID_DATA, "Invalid scenario identity");
    startup.identity[i] = (char)c;
  }
  *out = startup;
  return cd_diag(d, CD_OK, "Scenario startup pack read");
}


CdStatus cd_world_open_duty_startup(const CdContent *content,
                                     const CdScenarioStartup *startup,
                                     const char *path, CdWorld **out,
                                     CdDiagnostic *d) {
  if (!content || !startup || !path || !out || *out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Duty startup requires a fresh world");
  FILE *file = fopen(path, "rb");
  if (!file)
    return cd_diag(d, CD_IO_ERROR, "Cannot open duty startup schedule");
  char format[10], generation[65];
  CdWorld *world = NULL;
  CdStatus status = CD_INVALID_DATA;
  if (fscanf(file, "%9s %64s", format, generation) != 2 ||
      (strcmp(format, "CDSCHOOL1") && strcmp(format, "CDDUTIES1")) ||
      strcmp(generation, content->generation))
    goto done;
  status = cd_world_open_startup(content, startup, &world, d);
  if (status != CD_OK)
    goto done;
  for (size_t h = 1; h <= world->home_count; ++h) {
    uint64_t home;
    if (fscanf(file, "%" SCNu64, &home) != 1) {
      status = CD_INVALID_DATA;
      goto done;
    }
    uint32_t b = cd_polygon_index(content, home);
    if (b == CD_NONE || strcmp(content->polygons[b].use, "residential")) {
      status = CD_INVALID_DATA;
      goto done;
    }
    status = cd_world_household_move(world, h, home, d);
    if (status != CD_OK)
      goto done;
  }
  unsigned enrolled = 0;
  for (; enrolled < startup->cohorts[1]; ++enrolled) {
    uint64_t person, school;
    unsigned leave, start, end;
    if (fscanf(file, "%" SCNu64 " %" SCNu64 " %u %u %u",
               &person, &school, &leave, &start, &end) != 5) {
      status = CD_INVALID_DATA;
      goto done;
    }
    CdPersonRef ref = {world->people[0].civil.ref.world_token, person};
    CdCitizen *p = cd_citizen(world, ref);
    if (!p || p->school_door) {
      status = CD_INVALID_DATA;
      goto done;
    }
    status = cd_world_enroll_school(world, ref, school, leave, start, end, d);
    if (status != CD_OK)
      goto done;
  }
  status = cd_world_place_households(world, d);
  if (status != CD_OK)
    goto done;
  if (!strcmp(format, "CDDUTIES1")) {
    static const unsigned targets[] = {2800, 1800, 1400, 1500, 900, 1000, 1500, 700, 400};
    static const char *const uses[] = {
      "|commercial|mixed_use|storage|", "|industrial|workshop|commercial|",
      "|workshop|utility|utility_support|commercial|",
      "|agricultural|agricultural_support|", "|education|education_support|",
      "|healthcare|care_support|", "|commercial|leisure|hospitality_support|",
      "|civic|civic_support|commercial|", "|religious|community_service|"
    };
    unsigned counts[9] = {0};
    for (unsigned i = 0; i < 12000; ++i) {
      uint64_t person, workplace;
      unsigned leave, start, end, sector;
      if (fscanf(file, "%" SCNu64 " %" SCNu64 " %u %u %u %u",
                 &person, &workplace, &leave, &start, &end, &sector) != 6 ||
          !sector || sector > 9 || ++counts[sector - 1] > targets[sector - 1] ||
          leave >= start || end >= 1320 || end <= start || end - start != 480) {
        status = CD_INVALID_DATA;
        goto done;
      }
      uint32_t b = cd_polygon_index(content, workplace);
      uint32_t door = cd_reviewed_door(world, workplace);
      if (b == CD_NONE || door == CD_NONE) {
        status = CD_INVALID_DATA;
        goto done;
      }
      const CdPolygon *site = &content->polygons[b];
      char use[96];
      int written = snprintf(use, sizeof use, "|%s|", site->use);
      if (written < 0 || (size_t)written >= sizeof use || !strstr(uses[sector - 1], use)) {
        status = CD_INVALID_DATA;
        goto done;
      }
      CdPersonRef ref = {world->people[0].civil.ref.world_token, person};
      CdCitizen *worker = cd_citizen(world, ref);
      if (!worker || worker->jobs[0] || worker->civil.age < 18 || worker->civil.age >= 65) {
        status = CD_INVALID_DATA;
        goto done;
      }
      if (!world->premises[b].employer) {
        unsigned capacity = cd_capacity(world, b);
        if (!strcmp(site->use, "education") || !strcmp(site->use, "education_support"))
          capacity -= capacity * 3 / 4;
        status = cd_world_employer(world, workplace, "Canterbury sector employer",
                                   capacity, "Technical scenario sector workplace", d);
        if (status != CD_OK)
          goto done;
      }
      const CdDslRole *role = cd_dsl_startup_role(sector, site->use, person);
      if (!role) {
        status = cd_diag(d, CD_UNSUPPORTED, "No compatible named role for the funded sector post");
        goto done;
      }
      CdJobSpec spec = {workplace, content->doors[door].id, cd_dsl_role_occupation(role),
                        0, start, end, 1, 0, 0, "Funded Canterbury sector attendance"};
      CdEntityId job;
      status = cd_world_money_import(world, (CdAccount){.kind = CD_ACCOUNT_EMPLOYER, .id = workplace},
                                     end - start, "One day of scenario payroll", d);
      if (status == CD_OK)
        status = cd_world_dsl_certify(world, ref, role->id,
                                     "Explicit synthetic startup entry-role assessment", d);
      if (status == CD_OK)
        status = cd_world_job_create(world, &spec, &job, d);
      if (status == CD_OK)
        status = cd_world_job_assign(world, job, ref, (CdPersonRef){0}, 0, d);
      if (status != CD_OK)
        goto done;
      cd_job(world, job)->commute_start = leave;
      cd_job(world, job)->attendance_sector = sector;
    }
  }
  char extra;
  if (fscanf(file, " %c", &extra) != EOF || ferror(file)) {
    status = CD_INVALID_DATA;
    goto done;
  }
  status = CD_OK;
done:
  fclose(file);
  if (status != CD_OK) {
    cd_world_close(world);
    return status == CD_INVALID_DATA
               ? cd_diag(d, status, "Invalid duty startup or mismatched map generation")
               : status;
  }
  *out = world;
  return cd_diag(d, CD_OK, "Households and scheduled duties activated");
}
