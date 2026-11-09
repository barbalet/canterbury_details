#!/usr/bin/env python3
"""Build/smoke/admission/clean-room qualification. Native app GUI launch is separate."""
import argparse,hashlib,json,os,platform,re,shutil,subprocess,tempfile
from pathlib import Path
from audit_sources import ROOT,MANIFEST,sha,inventory
REPORT=Path('content/cycle-03-validation.json')
INPUTS=['Makefile', 'tools/build.py', 'tools/native_sources.py', 'tools/native_profile.py', 'tools/verify_cycle03.py', 'src/cd_api.h', 'src/cd_bootstrap.h', 'src/cd_adapter.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_memory.h', 'src/cd_random.c', 'src/cd_random.h', 'src/cd_people.h', 'src/main.c', 'tests/smoke.c', 'ui/CityApp.m', 'ui/CityMap.m', 'ui/CityMap.h', 'tools/build_basemap.py','tools/assembled_map.py','tools/json2png2.c', 'ui/Info.plist']
INPUTS=sorted(set(INPUTS+['src/cd_city.h', 'src/cd_city_internal.h', 'src/cd_native.h', 'ui/CitySession.h', 'ui/CitySession.m', 'tools/compile_runtime.py', 'content/spatial-review.json', 'src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_geometry.c']))
INPUTS=sorted(set(INPUTS+['src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_health.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_needs.c', 'src/cd_geometry.c', 'src/cd_food.c', 'src/cd_lifecycle.c', 'src/cd_human_digest.c', 'src/cd_occupations.c', 'src/cd_city_internal.h', 'src/cd_bootstrap.h', 'src/cd_native.h', 'src/cd_human.h', 'src/cd_random.h', 'src/cd_city.h', 'src/cd_api.h', 'src/cd_people.h', 'src/cd_memory.h', 'tools/compile_occupations.py', 'content/occupation-policy.json']))
INPUTS=sorted(set(INPUTS+['src/cd_jobs.c', 'src/cd_work.c', 'src/cd_economy.c', 'src/cd_development.c', 'src/cd_institutions.c', 'src/cd_work_digest.c', 'src/cd_work.h', 'tools/compile_runtime.py']))
INPUTS=sorted(set(INPUTS+['src/cd_civic.h', 'src/cd_civic_internal.h', 'src/cd_offices.c', 'src/cd_community.c', 'src/cd_services.c', 'src/cd_logistics.c', 'src/cd_environment.c', 'src/cd_civic_digest.c', 'src/cd_calendar.c']))
from native_sources import CORE
INPUTS=sorted(set(INPUTS+CORE+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['tools/checkpoint_abi.py']))

from native_sources import UI
INPUTS=sorted(set(INPUTS+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'ui').glob('*.h')]+['tools/compile_controls.py','content/operation-controls.json']))

def run(command,cwd=ROOT,expected=0,env=None):
    result=subprocess.run(list(map(str,command)),cwd=cwd,text=True,capture_output=True,env=env)
    if result.returncode!=expected:raise RuntimeError('Command failed: '+str(command)+'\n'+result.stdout[-1200:]+result.stderr[-2000:])
    return result.stdout+result.stderr

def qualify():
    if inventory()!=json.loads((ROOT/MANIFEST).read_text()):raise ValueError('Upstream source baseline changed')
    (ROOT/'build').mkdir(exist_ok=True)
    logs={}
    for target in ('cli','smoke')+ (('app',) if platform.system()=='Darwin' else ()):
        output=run(['python3','tools/build.py',target]);path=Path(f'build/cycle03-{target}.log');(ROOT/path).write_text(output);logs[path.as_posix()]=sha(ROOT/path)
    run([ROOT/'build/smoke-test'])
    first=json.loads(run([ROOT/'build/CanterburyDetails','--people','32','--minutes','5']).splitlines()[-1]);repeat=json.loads(run([ROOT/'build/CanterburyDetails','--people','32','--minutes','5']).splitlines()[-1]);assert first==repeat
    assert first['ok'] and first['native_people']==first['people']==first['actor_records']==32 and first['native_minutes']==first['callbacks']==6 and first['bindings_valid']
    full=json.loads(run([ROOT/'build/CanterburyDetails','--minutes','0']).splitlines()[-1]);assert full['people']==full['native_people']==full['actor_records']==27000 and full['bindings_valid']
    paused=json.loads(run([ROOT/'build/CanterburyDetails','--people','32','--minutes','0']).splitlines()[-1]);assert paused['callbacks']==1
    invalid=[['--people','0'],['--people','30001'],['--minutes','-1'],['--seed','4294967296'],['--unknown','1'],['--minutes']]
    for args in invalid:run([ROOT/'build/CanterburyDetails',*args],expected=2)
    for flags in ('-DCANTERBURY_SIM=0','-UBRONZE_SIM','-D BRONZE_SIM=0'):
        env=dict(os.environ);env['CPPFLAGS']=flags;text=run(['python3','tools/build.py','check'],expected=2,env=env);assert 'Inconsistent profile' in text
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc','src/cd_adapter.c','src/cd_people.c','src/cd_memory.c','src/cd_memory.h','src/cd_random.c','src/cd_random.h','src/cd_people.h','src/main.c','tests/smoke.c'])
    # Snapshot current owned work, initialize real local dependency clones and
    # commit ONLY the temporary fixture. This also covers work not committed yet
    # in the user's checkout; no user repository commit is made.
    with tempfile.TemporaryDirectory(prefix='canterbury-details-cycle03-') as folder:
        work=Path(folder)/'checkout';work.mkdir()
        for path in ROOT.iterdir():
            if path.name in ('.git','apesdk','canterbury','build','__pycache__','.DS_Store'):continue
            dest=work/path.name
            if path.is_dir():shutil.copytree(path,dest,ignore=shutil.ignore_patterns('__pycache__'))
            elif path.is_file():shutil.copy2(path,dest)
        manifest=json.loads((ROOT/MANIFEST).read_text())
        for module in manifest['submodules']:
            run(['git','clone','--quiet','--no-hardlinks',ROOT/module['path'],work/module['path']])
            run(['git','remote','set-url','origin',module['origin']],cwd=work/module['path'])
            if module['path']=='apesdk':
                review=json.loads((ROOT/'content/sdk-working-tree.json').read_text())
                for path in review['files']:
                    shutil.copy2(ROOT/'apesdk'/path,work/'apesdk'/path)
        run(['git','init','--quiet'],cwd=work);run(['git','add','.'],cwd=work)
        run(['git','-c','core.hooksPath=/dev/null','-c','commit.gpgsign=false','-c','user.name=Cycle03 Fixture','-c','user.email=fixture@invalid.example','commit','--quiet','-m','Temporary clean-room fixture'],cwd=work)
        clean_output=run(['make','all'],cwd=work);assert (work/'build/CanterburyDetails').is_file()
        clean_run=json.loads(run([work/'build/CanterburyDetails','--people','32','--minutes','5'],cwd=work).splitlines()[-1]);assert clean_run==first
        if platform.system()=='Darwin':run(['make','app'],cwd=work)
        # Missing either required dependency must diagnose, not fall back to a
        # developer sibling path. Renames occur only inside the disposable copy.
        for module in ('canterbury','apesdk'):
            (work/module).rename(work/(module+'.unavailable'))
            text=run(['python3','tools/build.py','check'],cwd=work,expected=2);assert 'Missing '+module+' submodule' in text
            (work/(module+'.unavailable')).rename(work/module)
        (ROOT/'build/cycle03-clean-room.log').write_text(clean_output)
    assert inventory()==json.loads((ROOT/MANIFEST).read_text())
    return {'format':'canterbury-details-cycle03-validation','version':1,'scope':'minimal native bootstrap/build only; no map, human routines, general capacity/profile isolation or CdWorld runtime qualification','platform':platform.platform(),'input_sha256':{p:sha(ROOT/p) for p in INPUTS},'source_manifest_sha256':sha(ROOT/MANIFEST),'cli':first,'default_population_startup':full,'paused':paused,'invalid_cli_cases':len(invalid),'profile_conflicts_rejected':3,'missing_submodule_cases_rejected':2,'clean_room_cli_matches':True,'clean_room_app_built':platform.system()=='Darwin','temporary_fixture_committed_only_in_temporary_directory':True,'native_ui':'current visual interface in content/native-ui-validation.json; earlier records are historical','build_logs':logs,'sdk_reviewed_worktree_changes':'content/sdk-working-tree.json','ok':True}
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    if a.run:(ROOT/REPORT).write_text(json.dumps(qualify(),indent=2)+'\n')
    else:
        j=json.loads((ROOT/REPORT).read_text());assert j['ok'] and j['input_sha256']=={p:sha(ROOT/p) for p in INPUTS} and j['source_manifest_sha256']==sha(ROOT/MANIFEST)
    print('PASS Cycle 03 CLI/app builds, native smoke, deterministic read/step, admission diagnostics and independent clean-room build')
if __name__=='__main__':main()
