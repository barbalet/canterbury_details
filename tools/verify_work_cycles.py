#!/usr/bin/env python3
"""Qualify cycles 19–23 against native work grants and exact local source records."""
import argparse,json,subprocess
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
from native_sources import CORE,UI
from compile_runtime import build
FILES=sorted(set(CORE+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CitySession.h','ui/CityMap.h','tools/verify_work_cycles.py','tools/city_fixtures.py','tools/compile_runtime.py','tools/native_sources.py','tools/build.py','tests/work.c','docs/EMPLOYMENT.md','docs/WORK.md','docs/ECONOMY.md','docs/DEVELOPMENT.md','docs/INSTITUTIONS.md','content/architecture.json','content/institution-source-matrix.json']))
SCOPES={19:'bounded employers, reachable qualified jobs, secondary and overnight shifts, apprenticeship and deterministic mentor loss',20:'single native-minute arbitration, real commute, interruption/resume, batch equivalence, exactly-once wages/materials/output and cancellation',21:'finite production chains, shortages, household purchases, escrow, retry/cancel safety, goods/food/money conservation and death possessions',22:'material/labor gated repair and capacity/use projects, publication validation, cancellation and immutable source geometry',23:'all 33 exact Jewish/Christian source records, closure/unlocated distinction, explicit operations, retained review evidence and immutable snapshots'}
from native_sources import UI
FILES=sorted(set(FILES+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'ui').glob('*.h')]+['tools/compile_controls.py','content/operation-controls.json']))

def run(cmd):
 p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(str(cmd)+'\n'+p.stdout[-1800:]+p.stderr[-3500:])
 return p.stdout+p.stderr
def matrix():
 bundle=json.loads((ROOT/'build/content/canterbury.json').read_text())
 # Use the canonical bundle's institution records, preserving every supplied field.
 from compile_runtime import compile_model
 model=compile_model(bundle,json.loads((ROOT/'content/spatial-review.json').read_text()))
 records=[json.loads(s['source_record']) for s in model['sites']]
 return {'format':'canterbury-details-institution-source-matrix','generation_id':bundle['generation_id'],'source_manifest_sha256':sha(ROOT/MANIFEST),'sites':records,'scope':'exact supplied records; no inferred congregation, ownership, precinct boundary or current staffing'}
def main():
 p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
 source=matrix();path=ROOT/'content/institution-source-matrix.json'
 if a.check:assert json.loads(path.read_text())==source
 else:path.write_text(json.dumps(source,ensure_ascii=False,indent=2)+'\n')
 expected={'inputs':{f:sha(ROOT/f) for f in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
 if a.check:
  build(True)
  for n in SCOPES:
   j=json.loads((ROOT/f'content/cycle-{n:02}-validation.json').read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items())
  print('PASS current cycles 19–23 evidence');return
 packed=build();run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli'])
 cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command'];cmd=['tests/work.c' if x=='src/main.c' else x for x in cmd]+['-DCD_TESTING=1','-UNDEBUG','-o','build/work-test'];run(cmd)
 run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCD_TESTING=1','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc',*CORE,'tests/work.c'])
 output=run([ROOT/'build/work-test',0]);actual=json.loads(output.splitlines()[-2]);assert actual['sites']==source['sites']
 assert len(actual['sites'])==33 and sum(s['religion']=='judaism' for s in actual['sites'])==1 and sum(not s.get('position_pixels') for s in actual['sites'])==5
 for n,scope in SCOPES.items():
  output=run([ROOT/'build/work-test',n]);(ROOT/f'build/work-{n}.log').write_text(output);result=json.loads(output.splitlines()[-1]);assert result['ok'] and result['closed_bytes']==0
  report={'format':f'canterbury-details-cycle{n:02}-validation','version':1,**expected,'scope':scope,'native_fixture':result,'runtime_pack':packed,'source_institutions':len(actual['sites']),'parameters':'Explicit technical scenario fixtures; no historical staffing, prices, yields or congregations inferred','ok':True}
  (ROOT/f'content/cycle-{n:02}-validation.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS work cycle '+str(n),flush=True)
 output=run([ROOT/'build/work-test',24])
 (ROOT/'build/work-refined.log').write_text(output)
 result=json.loads(output.splitlines()[-1]);assert result['ok'] and result['closed_bytes']==0
 print('PASS refined production, physical cargo, cancellation and checkpoint',flush=True)
 output=run([ROOT/'build/work-test',25]);(ROOT/'build/work-dsl-trades.log').write_text(output)
 result=json.loads(output.splitlines()[-1]);assert result['ok'] and result['closed_bytes']==0
 print('PASS 155 named production and hospitality roles, authority, finite production and checkpoint',flush=True)
 output=run([ROOT/'build/work-test',26]);(ROOT/'build/work-staffed-day.log').write_text(output)
 result=json.loads(output.splitlines()[-1]);assert result['ok'] and result['closed_bytes']==0
 print('PASS staffed full day, commutes, production, service, conservation and replay',flush=True)
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
if __name__=='__main__':main()
