#!/usr/bin/env python3
"""Qualify causal history, scenario operations, whole-city checkpoints and inspection (29–34)."""
import argparse,gzip,json,subprocess,shutil,struct
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
from native_sources import CORE,UI
SCOPES={29:'Causal staffing-loss chain, retirement, household privacy/observations/support and bounded retention',30:'Permission-gated preview/queue/cancel, idempotence, deadline execution and allocation-free queue limits',31:'Complete active travel/work/development/care/gathering/transport checkpoint; staged failure/publication preservation',32:'Batch/pause/snapshot/export replay, optimized/unoptimized and separate-process continuation',33:'Exact json2png source rendering, separate glyphs and read-only camera transforms',34:'Known-position picks, stable-ID selection across compaction/load and immutable inspection'}
DOCS=['docs/'+x+'.md' for x in ['RECORDS','OPERATIONS','PERSISTENCE','REPLAY','MAP','INSPECTION']]
FILES=sorted(set(CORE+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+DOCS+['tests/observer.c','tests/test_assembled_map.py','tools/verify_observer_cycles.py','tools/checkpoint_abi.py','tools/native_sources.py','tools/build.py','tools/build_basemap.py','tools/assembled_map.py','tools/json2png2.c','tools/generate_xcode.py','ui/CityMap.h','ui/CitySession.h','content/spatial-review.json']))
from native_sources import UI
FILES=sorted(set(FILES+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'ui').glob('*.h')]+['tools/compile_controls.py','content/operation-controls.json']))

def run(cmd,expected=0):
 p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
 if p.returncode!=expected:raise RuntimeError(str(cmd)+'\n'+p.stdout[-1500:]+p.stderr[-4000:])
 return p.stdout+p.stderr

def reject_files():
 source=ROOT/'build/replay.city'
 # Stream mutations so the native compatibility landscape need never fit in Python RAM.
 for i in range(8):
  target=ROOT/f'build/rejected-{i}.city'
  if i==0:target.write_bytes(b'');continue
  if i==1:raw=source.read_bytes();target.write_bytes(raw[:len(raw)//2]);continue
  if i==7:
   raw=bytearray(source.read_bytes());raw[-5]^=64;target.write_bytes(raw);continue
  offset={2:8,3:48,4:113,5:struct.calcsize('@6Q260s4I0Q'),6:None}[i]
  with gzip.open(source,'rb') as src,gzip.open(target,'wb',compresslevel=1) as dst:
   pos=0; previous=b''
   while block:=src.read(1024*1024):
    if offset is not None and pos<=offset<pos+len(block):
     block=bytearray(block);block[offset-pos]^=64;block=bytes(block)
    if i==6:
     if previous:dst.write(previous)
     previous=block
    else:dst.write(block)
    pos+=len(block)
   if i==6:
    block=bytearray(previous);block[-1]^=64;dst.write(block)

def main():
 p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
 assert inventory()==json.loads((ROOT/MANIFEST).read_text())
 expected={'inputs':{f:sha(ROOT/f) for f in FILES},'source_manifest_sha256':sha(ROOT/MANIFEST)}
 if a.check:
  for n in SCOPES:
   report=json.loads((ROOT/f'content/cycle-{n:02}-validation.json').read_text());assert report['ok'] and all(report[k]==v for k,v in expected.items())
  print('PASS current Cycles 29–34 evidence');return
 run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli'])
 base=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command']
 command=['tests/observer.c' if x=='src/main.c' else x for x in base]+['-DCD_TESTING=1','-UNDEBUG','-o','build/observer-test'];run(command)
 run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCD_TESTING=1','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc',*CORE,'tests/observer.c'])
 for compiler,standard in [('cc','c11'),('c++','c++17')]:
  check=subprocess.run([compiler,'-std='+standard,'-Wall','-Wextra','-Werror','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Isrc','-fsyntax-only','-x','c++' if compiler=='c++' else 'c','-'],cwd=ROOT,input='#include "cd_observer.h"\n',capture_output=True,text=True);assert not check.returncode,check.stderr
 results={}
 for n in SCOPES:
  output=run([ROOT/'build/observer-test',n]);(ROOT/f'build/observer-{n}.log').write_text(output);results[n]=json.loads(output.splitlines()[-1]);assert results[n]['ok'] and results[n]['closed_bytes']==0;print('PASS observer cycle '+str(n),flush=True)
 continuation=lambda output:json.loads(output.splitlines()[0])['continuation']
 expected_digest=continuation((ROOT/'build/observer-32.log').read_text());separate=run([ROOT/'build/observer-test',100]);assert continuation(separate)==expected_digest
 assert (ROOT/'build/replay-uninterrupted.json').read_bytes()==(ROOT/'build/replay-resumed.json').read_bytes()
 reject_files();run([ROOT/'build/observer-test',101])
 # Compare another supported optimization setting and its event stream.
 other=['-O0' if x=='-O2' else x for x in command];other[-1]='build/observer-debug-test';run(other)
 assert continuation(run([ROOT/'build/observer-debug-test',32]))==expected_digest
 assert continuation(run([ROOT/'build/observer-test',100]))==expected_digest
 program=ROOT/'build/ordinary-example.txt';program.write_text('1|train|1|0|0|15|0|0|0|0|1000|0|0||explicit ordinary training experiment\nflush\nstep 1\n')
 cli=[ROOT/'build/CanterburyDetails','--city','build/fixtures/civic.bin','--people','20','--program',program]
 run(cli,1);first=json.loads(run(cli+['--intervene','--save','build/cli.city']).splitlines()[-1]);restored=json.loads(run([ROOT/'build/CanterburyDetails','--city','build/fixtures/civic.bin','--load','build/cli.city','--minutes','0']).splitlines()[-1]);assert first==restored
 baseline=json.loads(run(['python3','tools/build_basemap.py','--check']).splitlines()[-1]);assert baseline['reference_pixel_match'] and baseline['png_sha256']=='c6c3b07bd101ce01eb6952413cacc50215daf2fbc387eb6fb2226e1340c53e81'
 run(['python3','tests/test_assembled_map.py'])
 for n,scope in SCOPES.items():
  report={'format':f'canterbury-details-cycle{n:02}-validation','version':1,**expected,'scope':scope,'fixture':results[n],'continuation_digest':expected_digest,'separate_process':True,'optimized_unoptimized_match':True,'malformed_files_rejected_preserving_live':8,'cli_checkpoint_match':True,'source_png_sha256':baseline['png_sha256'],'historical_limits':'Technical operational fixtures; actual source retains zero qualified edges/doors and no inferred staffing or positions','ok':True}
  (ROOT/f'content/cycle-{n:02}-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 assert inventory()==json.loads((ROOT/MANIFEST).read_text());print('PASS Cycles 29–34, full checkpoint failures, CLI and separate-process replay')
if __name__=='__main__':main()
