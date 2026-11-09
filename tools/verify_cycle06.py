#!/usr/bin/env python3
"""Real native identity, body permutation, retirement and world-reference checks."""
import argparse,json,subprocess
from audit_sources import ROOT,MANIFEST,inventory,sha
REPORT=ROOT/'content/cycle-06-validation.json'
FILES=['Makefile', 'tools/build.py', 'tools/native_sources.py', 'tools/native_profile.py', 'tools/verify_cycle06.py', 'src/cd_people.h', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_memory.h', 'src/cd_random.c', 'src/cd_random.h', 'src/cd_bootstrap.h', 'src/cd_adapter.c', 'tests/identity.c', 'docs/IDENTITY.md']
FILES=sorted(set(FILES+['src/cd_city.h', 'src/cd_city_internal.h', 'src/cd_native.h', 'ui/CitySession.h', 'ui/CitySession.m', 'tools/compile_runtime.py', 'content/spatial-review.json', 'src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_geometry.c']))
FILES=sorted(set(FILES+['src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_health.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_needs.c', 'src/cd_geometry.c', 'src/cd_food.c', 'src/cd_lifecycle.c', 'src/cd_human_digest.c', 'src/cd_occupations.c', 'src/cd_city_internal.h', 'src/cd_bootstrap.h', 'src/cd_native.h', 'src/cd_human.h', 'src/cd_random.h', 'src/cd_city.h', 'src/cd_api.h', 'src/cd_people.h', 'src/cd_memory.h', 'tools/compile_occupations.py', 'content/occupation-policy.json']))
FILES=sorted(set(FILES+['src/cd_jobs.c', 'src/cd_work.c', 'src/cd_economy.c', 'src/cd_development.c', 'src/cd_institutions.c', 'src/cd_work_digest.c', 'src/cd_work.h', 'tools/compile_runtime.py']))
FILES=sorted(set(FILES+['src/cd_civic.h', 'src/cd_civic_internal.h', 'src/cd_offices.c', 'src/cd_community.c', 'src/cd_services.c', 'src/cd_logistics.c', 'src/cd_environment.c', 'src/cd_civic_digest.c', 'src/cd_calendar.c']))
def run(args):
    p=subprocess.run(list(map(str,args)),cwd=ROOT,text=True,capture_output=True)
    if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-1500:]+p.stderr[-2000:])
    return p.stdout+p.stderr

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    expected={'inputs':{f:sha(ROOT/f) for f in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
    if a.check:
        j=json.loads(REPORT.read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items());print('PASS Cycle 06 current identity evidence');return
    run(['python3','tools/build.py','cli'])
    cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command'];cmd=['tests/identity.c' if x=='src/main.c' else x for x in cmd]+['-UNDEBUG','-o','build/identity-test']
    (ROOT/'build/cycle06-build.log').write_text(run(cmd))
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc','src/cd_people.c','src/cd_memory.c','src/cd_memory.h','src/cd_random.c','src/cd_random.h','src/cd_adapter.c','tests/identity.c'])
    results=[]
    for count in (32,27000,30000):
        j=json.loads(run([ROOT/'build/identity-test',count]).splitlines()[-1]);assert j['ok'];results.append(j);print('PASS identity fixture '+str(count),flush=True)
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    REPORT.write_text(json.dumps({'format':'canterbury-details-cycle06-validation','version':1,**expected,'fixtures':results,'scope':'Live project membership and native identity lifecycle; demographics and historical life records remain later cycles','sdk_changes_in_cycle06':False,'ok':True},indent=2)+'\n')
    print('PASS Cycle 06 stable identities, reorder, compaction, rebind and atomic reference rejection')
if __name__=='__main__':main()
