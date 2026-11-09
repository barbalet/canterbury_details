#!/usr/bin/env python3
"""Native episodic activity acceptance; explicit SDK working-tree review required."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
from audit_sources import ROOT,inventory,MANIFEST,sha
from native_sources import CORE,UI
REPORT=ROOT/'content/activity-events-validation.json'
FILES=sorted(set(CORE+UI+['tests/events.c','tools/verify_activity_events.py','EVENTS_PLAN.md','docs/RECORDS.md','content/sdk-working-tree.json','content/source-manifest.json']+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CitySession.h']))
def run(cmd):
 p=subprocess.run(list(map(str,cmd)),cwd=ROOT,text=True,capture_output=True)
 if p.returncode:raise RuntimeError(str(cmd)+'\n'+p.stdout[-2000:]+p.stderr[-4000:])
 return p.stdout+p.stderr

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args()
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
 expected={f:sha(ROOT/f) for f in FILES}
 if a.check:
  r=json.loads(REPORT.read_text());assert r['ok'] and r['inputs']==expected
  for f,h in r['logs'].items():assert sha(ROOT/f)==h
  print('PASS current native activity event evidence');return
 run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli'])
 cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command']
 cmd=['tests/events.c' if x=='src/main.c' else x for x in cmd]+['-DCD_TESTING=1','-UNDEBUG','-o','build/activity-events-test']
 run(cmd)
 text=run([ROOT/'build/activity-events-test']);(ROOT/'build/activity-events-test.log').write_text(text)
 # The existing human/work/civic/observer suites additionally cover the ordinary
 # lifecycle, resource, service, batching and checkpoint failure paths.
 report={'format':'canterbury-native-activity-events-validation','version':1,'inputs':expected,'sdk_revision':subprocess.check_output(['git','-C',str(ROOT/'apesdk'),'rev-parse','HEAD'],text=True).strip(),'sdk_worktree_review':sha(ROOT/'content/sdk-working-tree.json'),'scope':'Native episodic activity, actual meals, co-located chats, privacy, birth and save/replay; not a scheduled world-event system','native_eat_and_chat_verified':True,'food_conservation':True,'unplaced_separated_and_resting_no_chat':True,'civil_name_history_and_personal_privacy':True,'read_and_zero_step_purity':True,'save_load_and_batch_replay':True,'native_birth_memory':True,'native_memory_survives_city_history_eviction':True,'native_memory_and_civil_identity_survive_compaction':True,'bounded_local_pairing':'At most 8 following stable-ID neighbours per resident per native hour; no all-pairs scan','logs':{'build/activity-events-test.log':sha(ROOT/'build/activity-events-test.log')},'ok':True}
 REPORT.write_text(json.dumps(report,indent=2)+'\n');print(text.strip())
if __name__=='__main__':main()
