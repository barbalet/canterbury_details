#!/usr/bin/env python3
"""Build and qualify native Xcode schemes and json2png-bundled resources."""
import argparse,json,subprocess
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
from native_sources import RESOURCES,CORE,UI
REPORT=ROOT/'content/xcode-validation.json'
PROJECT='CanterburyDetails.xcodeproj'
FILES=['tools/generate_xcode.py','tools/native_sources.py','tools/native_profile.py','tools/build.py','tools/build_basemap.py','tools/verify_xcode.py','ui/CityApp.m','ui/CityMap.m','ui/CityMap.h','ui/Info.plist','docs/XCODE.md',PROJECT+'/project.pbxproj']+[PROJECT+'/xcshareddata/xcschemes/'+name+'.xcscheme' for name in ('Canterbury Details','Canterbury Details CLI','Canterbury Details Smoke')]
FILES=sorted(set(FILES+['src/cd_city.h', 'src/cd_city_internal.h', 'src/cd_native.h', 'ui/CitySession.h', 'ui/CitySession.m', 'tools/compile_runtime.py', 'content/spatial-review.json', 'src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_geometry.c']))
FILES=sorted(set(FILES+['src/cd_adapter.c', 'src/cd_population.c', 'src/cd_random.c', 'src/cd_health.c', 'src/cd_movement.c', 'src/cd_content.c', 'src/cd_people.c', 'src/cd_memory.c', 'src/cd_households.c', 'src/cd_routing.c', 'src/cd_world.c', 'src/cd_needs.c', 'src/cd_geometry.c', 'src/cd_food.c', 'src/cd_lifecycle.c', 'src/cd_human_digest.c', 'src/cd_occupations.c', 'src/cd_city_internal.h', 'src/cd_bootstrap.h', 'src/cd_native.h', 'src/cd_human.h', 'src/cd_random.h', 'src/cd_city.h', 'src/cd_api.h', 'src/cd_people.h', 'src/cd_memory.h', 'tools/compile_occupations.py', 'content/occupation-policy.json']))
FILES=sorted(set(FILES+['src/cd_jobs.c', 'src/cd_work.c', 'src/cd_economy.c', 'src/cd_development.c', 'src/cd_institutions.c', 'src/cd_work_digest.c', 'src/cd_work.h', 'tools/compile_runtime.py']))
FILES=sorted(set(FILES+['src/cd_civic.h', 'src/cd_civic_internal.h', 'src/cd_offices.c', 'src/cd_community.c', 'src/cd_services.c', 'src/cd_logistics.c', 'src/cd_environment.c', 'src/cd_civic_digest.c', 'src/cd_calendar.c']))
def run(command,expected=0):
    p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True)
    if p.returncode!=expected:raise RuntimeError(str(command)+'\n'+p.stdout[-1500:]+p.stderr[-2500:])
    return p.stdout+p.stderr

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    assert inventory()==json.loads((ROOT/MANIFEST).read_text());run(['python3','tools/generate_xcode.py','--check'])
    expected={'inputs':{p:sha(ROOT/p) for p in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
    if a.check:
        j=json.loads(REPORT.read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items());print('PASS current native Xcode build evidence');return
    run(['python3','tools/build_basemap.py','--write']);builds=[]
    for scheme,configuration in [('Canterbury Details','Debug'),('Canterbury Details CLI','Debug'),('Canterbury Details Smoke','Debug'),('Canterbury Details','Release')]:
        output=run(['xcodebuild','-project',PROJECT,'-scheme',scheme,'-configuration',configuration,'-derivedDataPath','build/Xcode','CODE_SIGNING_ALLOWED=NO','build']);assert '** BUILD SUCCEEDED **' in output
        log=ROOT/'build'/('xcode-'+scheme.replace(' ','-')+'-'+configuration+'.log');log.write_text(output);builds.append({'scheme':scheme,'configuration':configuration,'passed':True,'log_sha256':sha(log)});print('PASS Xcode '+scheme+' '+configuration,flush=True)
    base=ROOT/'build/Xcode/Build/Products'
    cli=json.loads(run([base/'Debug/CanterburyDetailsCLI','--people','32','--minutes','5']).splitlines()[-1]);assert cli['ok'] and cli['people']==cli['native_people']==32 and cli['callbacks']==cli['native_minutes']==6
    smoke=run([base/'Debug/CanterburyDetailsSmoke']);assert 'PASS native bootstrap' in smoke
    resources={}
    for configuration in ('Debug','Release'):
        contents=base/configuration/'CanterburyDetails.app/Contents'
        for path in RESOURCES:assert sha(contents/'Resources'/Path(path).name)==sha(ROOT/path)
        links=run(['otool','-L',contents/'MacOS/CanterburyDetails']);assert '/opt/homebrew/' not in links and '/usr/local/' not in links
        resources[configuration]={Path(path).name:sha(contents/'Resources'/Path(path).name) for path in RESOURCES}
    bad=subprocess.run(['xcodebuild','-project',PROJECT,'-scheme','Canterbury Details CLI','-configuration','Debug','-derivedDataPath','build/XcodeInvalid','GCC_PREPROCESSOR_DEFINITIONS=CANTERBURY_SIM=0 BRONZE_SIM=1','CODE_SIGNING_ALLOWED=NO','build'],cwd=ROOT,capture_output=True,text=True)
    badlog=bad.stdout+bad.stderr;(ROOT/'build/xcode-invalid-profile.log').write_text(badlog);assert bad.returncode and 'requires CANTERBURY_SIM=1' in badlog
    report={'format':'canterbury-details-native-xcode-validation','version':1,**expected,'xcode':run(['xcodebuild','-version']),'builds':builds,'cli':cli,'native_smoke_passed':True,'conflicting_profile_rejected':True,'resources':resources,'app_has_no_homebrew_runtime_linkage':True,'renderer':'exact canterbury/newcanterbury/json2png.c; labels enabled','ok':True}
    REPORT.write_text(json.dumps(report,indent=2)+'\n');print('PASS native Xcode schemes, both app configurations, CLI/smoke, profile guard and json2png resources')
if __name__=='__main__':main()
