#include "cd_scenario_startup.h"
#include "cd_city_internal.h"
#include <stdio.h>
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
