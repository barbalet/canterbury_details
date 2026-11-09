"""Publish operation help from the actual C catalogue and documented field contract."""
import argparse,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'content/operation-controls.json'
def build(check=False):
 text=(ROOT/'src/cd_operations.c').read_text().split('static const char *names[] = {',1)[1].split('};',1)[0];names=re.findall(r'"([a-z-]+)"',text)
 help={}
 for line in (ROOT/'docs/OPERATIONS.md').read_text().splitlines():
  parts=line.split('|')
  if len(parts)==4:
   for name in parts[1].strip().split(' / '):help[name]=parts[2].strip()
 assert len(names)==45 and len(set(names))==len(names) and all(n in help for n in names)
 data={'format':'canterbury-operation-controls','version':1,'scenario':'Canterbury 1940 — synthetic ordinary population; actual assembled map','permission_default':'observer','fields':['key','actorID','target','secondary','units','value','starts','duration','earliest','deadline','flags','auxiliary','label','basis'],'commands':[{'name':n,'help':help[n]} for n in names]}
 raw=json.dumps(data,indent=2)+'\n'
 if check:assert OUTPUT.read_text()==raw
 else:OUTPUT.write_text(raw)
 return data
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args();print(json.dumps({'commands':len(build(a.check)['commands'])}))
