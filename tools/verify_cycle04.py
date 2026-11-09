#!/usr/bin/env python3
"""Qualify all-human capacity boundaries and retain exact SDK isolation evidence."""
import argparse,json,platform,subprocess,tempfile
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
REPORT=ROOT/'content/cycle-04-validation.json'
INPUTS=['Makefile', 'tests/capacity.c', 'tools/verify_cycle04.py', 'docs/SDK-INTEGRATION.md', 'src/cd_adapter.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_memory.h', 'src/cd_random.c', 'src/cd_random.h', 'src/cd_people.h', 'src/cd_bootstrap.h', 'src/cd_api.h', 'tools/build.py', 'tools/native_sources.py', 'tools/native_profile.py']
INPUTS=sorted(set(INPUTS+['src/cd_city.h', 'src/cd_city_internal.h', 'src/cd_native.h', 'ui/CitySession.h', 'ui/CitySession.m', 'tools/compile_runtime.py', 'content/spatial-review.json', 'src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_geometry.c']))
INPUTS=sorted(set(INPUTS+['src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_health.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_needs.c', 'src/cd_geometry.c', 'src/cd_food.c', 'src/cd_lifecycle.c', 'src/cd_human_digest.c', 'src/cd_occupations.c', 'src/cd_city_internal.h', 'src/cd_bootstrap.h', 'src/cd_native.h', 'src/cd_human.h', 'src/cd_random.h', 'src/cd_city.h', 'src/cd_api.h', 'src/cd_people.h', 'src/cd_memory.h', 'tools/compile_occupations.py', 'content/occupation-policy.json']))
INPUTS=sorted(set(INPUTS+['src/cd_jobs.c', 'src/cd_work.c', 'src/cd_economy.c', 'src/cd_development.c', 'src/cd_institutions.c', 'src/cd_work_digest.c', 'src/cd_work.h', 'tools/compile_runtime.py']))
INPUTS=sorted(set(INPUTS+['src/cd_civic.h', 'src/cd_civic_internal.h', 'src/cd_offices.c', 'src/cd_community.c', 'src/cd_services.c', 'src/cd_logistics.c', 'src/cd_environment.c', 'src/cd_civic_digest.c', 'src/cd_calendar.c']))
from native_sources import CORE
INPUTS=sorted(set(INPUTS+CORE+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['tools/checkpoint_abi.py']))

from native_sources import UI
INPUTS=sorted(set(INPUTS+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'ui').glob('*.h')]+['tools/compile_controls.py','content/operation-controls.json']))

def run(args,cwd=ROOT):
    p=subprocess.run(list(map(str,args)),cwd=cwd,text=True,capture_output=True)
    if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-1500:]+p.stderr[-2000:])
    return p.stdout+p.stderr

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    run(['python3','tools/verify_sdk_profile.py','--check'])
    evidence=ROOT/'content/cycle-03-sdk-validation.json'
    expected={'inputs':{s:sha(ROOT/s) for s in INPUTS},'source_manifest_sha256':sha(ROOT/MANIFEST),'sdk_profile_evidence_sha256':sha(evidence)}
    if a.check:
        j=json.loads(REPORT.read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items());print('PASS Cycle 04 current capacity/ABI evidence');return
    run(['python3','tools/build.py','cli'])
    command=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command']
    assert command.count('src/main.c')==1
    command=['tests/capacity.c' if s=='src/main.c' else s for s in command]
    command+=['-UNDEBUG','-o','build/capacity-test']
    log=run(command);(ROOT/'build/cycle04-build.log').write_text(log)
    results=[]
    for people in (1,27000,29999,30000):
        j=json.loads(run([ROOT/'build/capacity-test',people]).splitlines()[-1]);assert j['ok'] and j['people']==people;results.append(j)
        print('PASS capacity fixture '+str(people),flush=True)
    default=json.loads(run([ROOT/'build/CanterburyDetails','--minutes','0']).splitlines()[-1]);assert default['people']==default['native_people']==default['actor_records']==27000
    # Independent executables for ABI probes; never mix objects from profiles.
    code='''#include "universe/universe.h"
#include "bronze/actors/bronze_actors.h"
#include <stddef.h>
#include <stdio.h>
int main(void){printf("%u %zu %zu %zu %zu %zu\\n",(unsigned)LARGE_SIM,sizeof(simulated_being),sizeof(simulated_group),offsetof(simulated_group,num),offsetof(simulated_group,max),sizeof(BronzeActor));return 0;}
'''
    layouts=[]
    with tempfile.TemporaryDirectory(prefix='canterbury-details-abi-') as d:
        source=Path(d)/'probe.c';source.write_text(code)
        for name,flags in [('native',[]),('bronze',['-DBRONZE_SIM=1']),('city',['-DBRONZE_SIM=1','-DCANTERBURY_SIM=1'])]:
            exe=Path(d)/name;run(['cc','-std=c11','-Iapesdk',*flags,source,'-o',exe]);values=list(map(int,run([exe]).split()))
            assert values[0]==(30000 if name=='city' else 256)
            layouts.append(dict(zip(['capacity','body_bytes','group_bytes','num_offset','max_offset','actor_bytes'],values),profile=name))
    assert layouts[0]['body_bytes']==layouts[1]['body_bytes']==layouts[2]['body_bytes']
    assert layouts[0]['group_bytes']==layouts[1]['group_bytes']
    sdk=json.loads(evidence.read_text());assert sdk['ok'] and all(x['passed'] for x in sdk['suites'])
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    report={'format':'canterbury-details-cycle04-validation','version':1,**expected,'platform':platform.platform(),'capacity_cases':results,'default_startup':default,'layouts':layouts,'ordinary_profile_baseline':'Current reviewed 30000-capacity city overlay; exact-source ordinary-profile preprocessing/layout comparison and isolated native/contract/actors/hybrid suites remain current','admission_scope':'Native identity capacity only; demographic births and visitor policies remain later cycles','ok':True}
    REPORT.write_text(json.dumps(report,indent=2)+'\n');print('PASS Cycle 04 capacity, overflow preservation, bounded admission, identity replacement and independent ABI probes')
if __name__=='__main__':main()
