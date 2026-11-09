#!/usr/bin/env python3
"""Verify the native clock, civil mapping and serialized session lifecycle."""
import argparse,json,subprocess
from audit_sources import ROOT,MANIFEST,inventory,sha
REPORT=ROOT/'content/cycle-05-validation.json'
INPUTS=['Makefile', 'src/cd_bootstrap.h', 'src/cd_adapter.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_memory.h', 'src/cd_random.c', 'src/cd_random.h', 'src/cd_people.h', 'src/main.c', 'ui/CityApp.m', 'tests/lifecycle.c', 'tools/verify_cycle05.py', 'docs/TIME-LIFECYCLE.md', 'tools/native_sources.py', 'tools/native_profile.py']
INPUTS=sorted(set(INPUTS+['src/cd_city.h', 'src/cd_city_internal.h', 'src/cd_native.h', 'ui/CitySession.h', 'ui/CitySession.m', 'tools/compile_runtime.py', 'content/spatial-review.json', 'src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_geometry.c']))
INPUTS=sorted(set(INPUTS+['src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_health.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_needs.c', 'src/cd_geometry.c', 'src/cd_food.c', 'src/cd_lifecycle.c', 'src/cd_human_digest.c', 'src/cd_occupations.c', 'src/cd_city_internal.h', 'src/cd_bootstrap.h', 'src/cd_native.h', 'src/cd_human.h', 'src/cd_random.h', 'src/cd_city.h', 'src/cd_api.h', 'src/cd_people.h', 'src/cd_memory.h', 'tools/compile_occupations.py', 'content/occupation-policy.json']))
INPUTS=sorted(set(INPUTS+['src/cd_jobs.c', 'src/cd_work.c', 'src/cd_economy.c', 'src/cd_development.c', 'src/cd_institutions.c', 'src/cd_work_digest.c', 'src/cd_work.h', 'tools/compile_runtime.py']))
INPUTS=sorted(set(INPUTS+['src/cd_civic.h', 'src/cd_civic_internal.h', 'src/cd_offices.c', 'src/cd_community.c', 'src/cd_services.c', 'src/cd_logistics.c', 'src/cd_environment.c', 'src/cd_civic_digest.c', 'src/cd_calendar.c']))
from native_sources import CORE
INPUTS=sorted(set(INPUTS+CORE+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['tools/checkpoint_abi.py']))

from native_sources import UI
INPUTS=sorted(set(INPUTS+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'ui').glob('*.h')]+['tools/compile_controls.py','content/operation-controls.json']))

def run(args):
    p=subprocess.run(list(map(str,args)),cwd=ROOT,text=True,capture_output=True)
    if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-1200:]+p.stderr[-2000:])
    return p.stdout+p.stderr

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    expected={'inputs':{f:sha(ROOT/f) for f in INPUTS},'source_manifest_sha256':sha(ROOT/MANIFEST)}
    if a.check:
        j=json.loads(REPORT.read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items());print('PASS Cycle 05 current clock/lifecycle evidence');return
    run(['python3','tools/build.py','cli'])
    command=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command']
    command=['tests/lifecycle.c' if x=='src/main.c' else x for x in command]+['-UNDEBUG','-o','build/lifecycle-test']
    output=run(command);(ROOT/'build/cycle05-build.log').write_text(output)
    # Keep normal JSON stdout separate from the expected unconfigured-init errors.
    p=subprocess.run([str(ROOT/'build/lifecycle-test')],cwd=ROOT,text=True,capture_output=True)
    (ROOT/'build/cycle05-lifecycle.log').write_text(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError(p.stdout[-1200:]+p.stderr[-2000:])
    result=json.loads(p.stdout.splitlines()[-1]);assert result['ok']
    cli=json.loads(run([ROOT/'build/CanterburyDetails','--people','1','--minutes','1439']).splitlines()[-1]);assert cli['scenario_time']=='1940-01-02T00:00' and cli['native_minutes']==cli['callbacks']==1440
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    REPORT.write_text(json.dumps({'format':'canterbury-details-cycle05-validation','version':1,**expected,'lifecycle':result,'cli_midnight':cli,'scenario_origin':'1940-01-01T00:00; explicit scenario convention, not sourced event date','native_ui':'separate observation in content/cycle-05-native-ui.json','ok':True},indent=2)+'\n')
    print('PASS Cycle 05 native minute, pause, Gregorian mapping, restart isolation and callback retirement')
if __name__=='__main__':main()
