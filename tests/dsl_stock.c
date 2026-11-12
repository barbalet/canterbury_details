#include "cd_city_internal.h"
#include "cd_dsl_catalogue.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  CdContent *content = NULL;
  CdWorld *world = NULL;
  CdDiagnostic error = {0};
  assert(cd_content_open("build/content/CanterburyCity.bin", &content, &error) == CD_OK);
  CdScenario scenario = {4, 1940, {1940, 1, 1, 0}, "synthetic-ordinary-v1"};
  assert(cd_world_open(content, &scenario, &world, &error) == CD_OK);
  CdEntityId premises[128] = {0};
  size_t count = 0;
  for (uint32_t i = 0; i < content->np && count < cd_dsl_resource_count(); ++i)
    if (content->polygons[i].kind == CD_BUILDING)
      premises[count++] = content->polygons[i].id;
  assert(count == 72);
  for (size_t i = 0; i < count; ++i) {
    const CdDslResource *resource = cd_dsl_resource_at(i);
    CdEntityId id = premises[i];
    CdDslStockView stock;
    assert(cd_world_dsl_import(world, id, resource->id, 100, "DSL stock test", &error) == CD_OK);
    assert(cd_world_dsl_reserve(world, id, resource->id, 1, 30, &error) == CD_OK);
    assert(cd_world_dsl_reserve(world, id, resource->id, 1, 30, &error) == CD_OK);
    assert(cd_world_dsl_reserve(world, id, resource->id, 2, 10, &error) != CD_OK);
    assert(cd_world_dsl_settle(world, id, resource->id, 1, 0, &error) == CD_OK);
    assert(cd_world_dsl_settle(world, id, resource->id, 1, 0, &error) == CD_OK);
    assert(cd_world_dsl_reserve(world, id, resource->id, 2, 30, &error) == CD_OK);
    assert(cd_world_dsl_settle(world, id, resource->id, 2, 1, &error) == CD_OK);
    assert(cd_world_dsl_settle(world, id, resource->id, 2, 1, &error) == CD_OK);
    assert(cd_world_dsl_stock(world, id, resource->id, &stock, &error) == CD_OK);
    assert(stock.available == 70 && stock.reserved == 0);
  }
  uint64_t digest;
  assert(cd_world_digest(world, &digest, &error) == CD_OK);
  assert(cd_world_dsl_import(world, premises[0], "not-a-resource", 1,
                            "DSL stock test", &error) != CD_OK);
  assert(cd_world_dsl_import(world, premises[0], cd_dsl_resource_at(0)->id,
                            UINT64_MAX, "DSL stock test", &error) != CD_OK);
  uint64_t after;
  assert(cd_world_digest(world, &after, &error) == CD_OK && after == digest);
  assert(cd_world_dsl_reserve(world, premises[0], cd_dsl_resource_at(0)->id,
                             3, 20, &error) == CD_OK);
  assert(cd_world_digest(world, &digest, &error) == CD_OK);
  assert(cd_world_save(world, "build/dsl-stock.city", &error) == CD_OK);
  assert(cd_world_load(&world, content, "build/dsl-stock.city", &error) == CD_OK);
  assert(cd_world_digest(world, &after, &error) == CD_OK && after == digest);
  assert(cd_world_dsl_settle(world, premises[0], cd_dsl_resource_at(0)->id,
                            3, 1, &error) == CD_OK);
  assert(cd_dsl_stock_valid(world));
  cd_world_close(world);
  cd_content_close(content);
  puts("PASS all 72 refined stocks, idempotent reservation settlement and checkpoint continuation");
}
