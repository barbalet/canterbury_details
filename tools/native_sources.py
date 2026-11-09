"""Shared source/resource definition for Make and Xcode; all paths checkout-local."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CORE=['src/cd_adapter.c','src/cd_people.c','src/cd_memory.c','src/cd_random.c','src/cd_content.c','src/cd_geometry.c','src/cd_world.c','src/cd_routing.c','src/cd_movement.c','src/cd_population.c','src/cd_households.c','src/cd_lifecycle.c','src/cd_needs.c','src/cd_food.c','src/cd_health.c','src/cd_human_digest.c','src/cd_occupations.c','src/cd_jobs.c','src/cd_work.c','src/cd_economy.c','src/cd_development.c','src/cd_institutions.c','src/cd_work_digest.c','src/cd_offices.c','src/cd_community.c','src/cd_services.c','src/cd_logistics.c','src/cd_environment.c','src/cd_civic_digest.c','src/cd_calendar.c','src/cd_records.c','src/cd_operations.c','src/cd_persistence.c','src/cd_checkpoint_validation.c','src/cd_readmodel.c','src/cd_management.c','src/cd_core.c']
UI=['ui/CityApp.m','ui/CityMap.m','ui/CitySession.m','ui/CityManagement.m']
RESOURCES=['content/operation-controls.json','build/content/CanterburyCity.bin','build/map/CanterburyMap.png','build/map/CanterburyMapBackground.png','canterbury/simulation-map/derived/religious-sites-1940.json','build/map/buildings-enriched.json']
def sdk_sources():
    sources=[]
    for folder in ('toolkit','script','render','sim','entity','universe'):
        sources.extend(str(p.relative_to(ROOT)) for p in sorted((ROOT/'apesdk'/folder).glob('*.c')))
    return sources+['apesdk/'+p for p in ['gui/draw.c','gui/buffer.c','gui/tutorial.c','bronze/hybrid/bronze_hybrid.c','bronze/actors/bronze_actors.c','bronze/work/bronze_work.c','bronze/tasks/bronze_tasks.c']]
def sources(target):return CORE+(UI if target=='app' else [{'cli':'src/main.c','smoke':'tests/smoke.c'}[target]])+sdk_sources()
