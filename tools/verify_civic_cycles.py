#!/usr/bin/env python3
"""Native acceptance, source qualification and state conservation for Cycles 24–28."""
import argparse,json,subprocess
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
from native_sources import CORE,UI
FILES=sorted(set(CORE+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CitySession.h','ui/CityMap.h','tools/verify_civic_cycles.py','tools/compile_runtime.py','tools/city_fixtures.py','tools/native_sources.py','tools/build.py','tests/civic.c','docs/RELIGIOUS-EMPLOYMENT.md','docs/COMMUNITY.md','docs/SERVICES.md','docs/LOGISTICS.md','docs/ENVIRONMENT.md','content/architecture.json','content/civic-source-register.json','content/religious-worksite-matrix.json']))
SCOPES={24:'distinct source-compatible offices, explicit permissions/qualifications, succession, vacancies and staff retirement',25:'explicit schedules, membership, physical attendance, finite meal transfers, Gregorian boundaries, batch equivalence and same-instant calendar component checkpoint',26:'ordinary rosters, actual dispatch travel, capability/permission gates, care/beds/medicine, deadline/cancel/staff-loss and finite wages',27:'qualified crew, real native movement, exclusive handcart/stretcher custody, loading/unloading, interruption/replacement/local cancellation and rail rejection',28:'explicit weather/fire/damage/closure/alert/blackout/ration experiments, finite water, physical shelter travel and deterministic recovery/persistence'}
def run(cmd):
 p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(str(cmd)+'\n'+p.stdout[-1500:]+p.stderr[-4000:])
 return p.stdout+p.stderr
def sources(check):
 paths=['canterbury/simulation-map/derived/religious-sites-1940.json','canterbury/simulation-map/derived/buildings-enriched.json','apesdk/bronze/content/bronze_age_full.bronze','apesdk/bronze/work/bronze_work.c','apesdk/bronze/actors/bronze_actors.c','src/cd_civic.h','tools/verify_civic_cycles.py']
 sites=json.loads((ROOT/paths[0]).read_text())['sites'];assert len(sites)==33
 rows=[]
 for s in sites:
  kinds=['custodian','sexton','community staff','teacher','administrator','charitable worker']
  if s['religion']=='christianity':
   kinds+=['minister']
   if s['denomination'] in ('anglican','roman_catholic','catholic'):kinds+=['priest']
  if s['religion']=='judaism':kinds+=['rabbi']
  rows.append({'source_id':s['id'],'name':s['name'],'religion':s['religion'],'denomination':s['denomination'],'source_status':s['worship_status_1940'],'located':s.get('position_pixels') is not None,'permitted_technical_office_kinds':kinds,'source_candidate_premises':s.get('candidate_buildings',[]),'staffing':'unknown; not auto-created','qualification':'Explicit title/institution or ordinary skill/permission inputs; eligibility is not historical staffing evidence'})
 outputs={'content/religious-worksite-matrix.json':{'format':'canterbury-details-religious-worksite-matrix','version':1,'source_sha256':sha(ROOT/paths[0]),'sites':rows},'content/civic-source-register.json':{'format':'canterbury-details-civic-source-register','version':1,'inputs':{p:sha(ROOT/p) for p in paths},'sources':{'institutions':'All exact Jewish/Christian records retained, including closures and scenario assignments','buildings':'Modern suggested uses/candidate footprints; not historical staffed services','sdk':'Shared native work, ordinary movement and resource concepts; ancient ritual professions remain disabled'},'technical_adaptations':['institution-specific ordinary offices','explicit recurring civil-minute schedules','permission-gated civic service experiments','human-carried handcart/stretcher with finite equipment','bounded rain/fire/damage/water/shelter/alert/blackout/ration experiments'],'unknown_or_disabled':['historical staffing and employment schedules','historical policing/fire/medical organizations and licenses','holidays, worship calendars and time-zone conversions','historical weather/ration/raid/evacuation dates or rates','operational rail, motor/animal fleets and unsupported routes','fire spread and predictive casualty rates'],'default_scenario':'No new offices, memberships, gatherings, rosters, dispatches, vehicles or pressures are invented for the shipped map.','persistence':'Calendar component canonical checkpoint only; coordinated full-city native/body/route/account save remains Cycle 31.'}}
 for name,data in outputs.items():
  path=ROOT/name
  if check:assert json.loads(path.read_text())==data
  else:path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
 return {'sites':len(rows),'christian':sum(s['religion']=='christianity' for s in sites),'jewish':sum(s['religion']=='judaism' for s in sites),'unlocated':sum(s.get('position_pixels') is None for s in sites)}
def main():
 p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
 assert inventory()==json.loads((ROOT/MANIFEST).read_text());source=sources(a.check)
 expected={'inputs':{f:sha(ROOT/f) for f in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
 if a.check:
  for n in SCOPES:
   j=json.loads((ROOT/f'content/cycle-{n:02}-validation.json').read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items())
  print('PASS current cycles 24–28 evidence');return
 run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli'])
 cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command'];cmd=['tests/civic.c' if x=='src/main.c' else x for x in cmd]+['-DCD_TESTING=1','-UNDEBUG','-o','build/civic-test'];run(cmd)
 run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCD_TESTING=1','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc',*CORE,'tests/civic.c'])
 for compiler,language,standard in [('cc','c','c11'),('c++','c++','c++17')]:
  p=subprocess.run([compiler,'-std='+standard,'-Wall','-Wextra','-Werror','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Isrc','-fsyntax-only','-x',language,'-'],input='#include "cd_civic.h"\n',cwd=ROOT,capture_output=True,text=True);assert not p.returncode,p.stderr
 from compile_runtime import build
 build()
 default=json.loads(run([ROOT/'build/civic-test',0]).splitlines()[-1]);assert default['ok'] and default['closed_bytes']==0
 for n,scope in SCOPES.items():
  output=run([ROOT/'build/civic-test',n]);(ROOT/f'build/civic-{n}.log').write_text(output);result=json.loads(output.splitlines()[-1]);assert result['ok'] and result['closed_bytes']==0
  report={'format':f'canterbury-details-cycle{n:02}-validation','version':1,**expected,'scope':scope,'native_fixture':result,'sources':source,'default_actual_map':{'people':27000,'new_civic_records':0,'no_inferred_permissions_memberships_events':True,'native_fixture':default},'parameters':'Explicit technical fixtures; no inferred historical staffing, event, equipment fleet, faith or holiday','persistence_scope':'Canonical same-instant calendar component round-trip; full atomic city saves remain Cycle 31','ok':True}
  (ROOT/f'content/cycle-{n:02}-validation.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS civic cycle '+str(n),flush=True)
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
if __name__=='__main__':main()
