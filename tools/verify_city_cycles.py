#!/usr/bin/env python3
"""Native-backed acceptance and current evidence for cycles 09 through 13."""
import argparse,json,subprocess
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
from native_sources import CORE,UI
from compile_runtime import build
FILES=sorted(set(CORE+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CityMap.h','ui/CitySession.h','tools/verify_city_cycles.py','tools/city_fixtures.py','tools/compile_runtime.py','tools/native_sources.py','tools/build.py','content/spatial-review.json','tests/city.c','docs/GEOMETRY.md','docs/MOVEMENT.md','docs/ROUTING.md','docs/POPULATION.md','docs/HOUSEHOLDS.md','docs/UI-THREADING.md']))
FILES=sorted(set(FILES+['src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_health.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_needs.c', 'src/cd_geometry.c', 'src/cd_food.c', 'src/cd_lifecycle.c', 'src/cd_human_digest.c', 'src/cd_occupations.c', 'src/cd_city_internal.h', 'src/cd_bootstrap.h', 'src/cd_native.h', 'src/cd_human.h', 'src/cd_random.h', 'src/cd_city.h', 'src/cd_api.h', 'src/cd_people.h', 'src/cd_memory.h', 'tools/compile_occupations.py', 'content/occupation-policy.json']))
SCOPES={9:'rings, holes, water, rail, entrance qualification, confidence and ordinary access',10:'native position, shared minute claim, bounded polyline/door travel and interruption',11:'source components, per-link qualification, grade rejection, detour, closure and crowd',12:'27,000 unique native-backed IDs; explicit synthetic names/dates/categories; unknown faith',13:'households, guardians, spouse consistency, capacity, ownership, visitor reservations and housing uncertainty'}
def run(cmd):
    p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
    if p.returncode:raise RuntimeError(str(cmd)+'\n'+p.stdout[-1500:]+p.stderr[-3000:])
    return p.stdout+p.stderr
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    expected={'inputs':{f:sha(ROOT/f) for f in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
    if a.check:
        for n in SCOPES:
            j=json.loads((ROOT/f'content/cycle-{n:02}-validation.json').read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items())
        print('PASS current cycles 09–13 evidence');return
    runtime=build();run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli'])
    cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command'];cmd=['tests/city.c' if x=='src/main.c' else x for x in cmd]+['-DCD_TESTING=1','-UNDEBUG','-o','build/city-test'];run(cmd)
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCD_TESTING=1','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc',*CORE,'tests/city.c'])
    results={}
    for n in [9,10,11,12,13,0,100,101,30000]:
        output=run([ROOT/'build/city-test',n]);(ROOT/f'build/city-{n}.log').write_text(output);results[n]=json.loads(output.splitlines()[-1]);assert results[n]['ok'] and results[n]['closed_bytes']==0;print('PASS city acceptance '+str(n),flush=True)
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    for n,scope in SCOPES.items():
        report={'format':f'canterbury-details-cycle{n:02}-validation','version':1,**expected,'scope':scope,'fixture':results[n],'full_source_population':results[0],'allocation_failure_replacement':results[100],'physical_capacity':results[101],'maximum_population':results[30000],'runtime_pack':runtime,'historical_limits':'No source roads/doors/capacities qualified by default. Demographics/households explicitly synthetic. Housing unknown is not unhoused.','ok':True}
        (ROOT/f'content/cycle-{n:02}-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS cycles 09–13 and full-map/native population, transactional failure cleanup')
if __name__=='__main__':main()
