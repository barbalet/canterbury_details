#!/usr/bin/env python3
"""Native-backed lifecycle/needs/food/health/occupation acceptance for cycles 14–18."""
import argparse,json,re,subprocess
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
from native_sources import CORE,UI
from compile_occupations import build as occupations
FILES=sorted(set(CORE+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CityMap.h','ui/CitySession.h','tools/verify_human_cycles.py','tools/compile_occupations.py','tools/build.py','tools/native_sources.py','tools/city_fixtures.py','content/occupation-policy.json','tests/human.c','tests/test_occupations.py','docs/LIFECYCLE.md','docs/NEEDS.md','docs/FOOD.md','docs/HEALTH.md','docs/OCCUPATIONS.md']))
SCOPES={14:'30,000 capacity, allocation-free configured births, parentage, archived retirement, guardian cleanup and birthday transitions',15:'sole native energy, finite meals, shared-minute survival/rest, pause and day-length batch equivalence',16:'finite raw/meal stocks, native-grant production, real street/door delivery, dependent feeding and ledger conservation',17:'configured illness/injury, physical qualified care, clinic capacity, medicine conservation, prevention, recovery and mortality',18:'all 63 upstream vocations audited; 8 explicit ordinary roles, 58 disabled specializations and no inferred affiliation'}
def run(cmd):
 p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(str(cmd)+'\n'+p.stdout[-1500:]+p.stderr[-3500:])
 return p.stdout+p.stderr
def main():
 p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
 expected={'inputs':{f:sha(ROOT/f) for f in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
 if a.check:
  occupations(True)
  for n in SCOPES:
   j=json.loads((ROOT/f'content/cycle-{n:02}-validation.json').read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items())
  print('PASS current cycles 14–18 evidence');return
 catalogue=occupations();run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli'])
 cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command'];cmd=['tests/human.c' if x=='src/main.c' else x for x in cmd]+['-DCD_TESTING=1','-UNDEBUG','-o','build/human-test'];run(cmd)
 run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCD_TESTING=1','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc',*CORE,'tests/human.c'])
 text=run(['python3','-m','unittest','discover','-s','tests','-p','test_occupations.py','-v']);match=re.search(r'Ran (\d+) tests',text);assert match and 'OK' in text;(ROOT/'build/cycle18-catalogue.log').write_text(text)
 results={}
 for n in [0,*SCOPES]:
  output=run([ROOT/'build/human-test',n]);(ROOT/f'build/human-{n}.log').write_text(output);result=json.loads(output.splitlines()[-1]);assert result['ok'] and result['closed_bytes']==0;
  if n==0:result.update(json.loads(output.splitlines()[-2]))
  results[n]=result;print('PASS human cycle '+str(n),flush=True)
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
 for n,scope in SCOPES.items():
  report={'name_variation':results[0],'format':f'canterbury-details-cycle{n:02}-validation','version':1,**expected,'scope':scope,'native_fixture':results[n],'catalogue':catalogue,'negative_catalogue_contract_tests':int(match.group(1)),'parameters':'Explicit technical fixtures; no historical birth/disease/ration/staffing rates inferred','movement':'CANTERBURY_SIM native position plus reviewed source polylines/entrances; no physical inventory/care teleport','ok':True}
  (ROOT/f'content/cycle-{n:02}-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS cycles 14–18 native behavior, finite resources, source qualification and zero retained allocations')
if __name__=='__main__':main()
