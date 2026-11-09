#!/usr/bin/env python3
"""Prepare an offline clean-clone release gate; finalize after normal native GUI checks."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
EXCLUDED={'.git','build','apesdk','canterbury','__pycache__','.DS_Store'}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def call(args,cwd=ROOT,log=None):
 env={**os.environ,'GIT_TERMINAL_PROMPT':'0','HOMEBREW_NO_AUTO_UPDATE':'1','http_proxy':'http://127.0.0.1:9','https_proxy':'http://127.0.0.1:9','ALL_PROXY':'http://127.0.0.1:9'}
 p=subprocess.run(list(map(str,args)),cwd=cwd,env=env,capture_output=True,text=True)
 if log:Path(log).write_text(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-1500:]+p.stderr[-3500:])
 return p.stdout.strip()
def owned_files(root):
 raw=call(['git','ls-files','--cached','--others','--exclude-standard','-z'],root)
 return sorted(set(p for p in raw.split('\0') if p and Path(p).parts[0] not in EXCLUDED and (root/p).is_file()))
def code_inputs(root):
 files=[]
 for folder in ('src','ui','tools','tests','examples'):
  files += [p for p in (root/folder).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.m','.py','.plist') and '__pycache__' not in p.parts]
 files += [root/'Makefile',root/'content/operation-controls.json',root/'content/performance-budgets.json',root/'content/sdk-profile-baseline.json']
 return {str(p.relative_to(root)):sha(p) for p in sorted(files)}
def clone_modules(destination,source,manifest):
 for module in manifest['submodules']:
  name=module['path'];call(['git','clone','--quiet','--no-hardlinks',source/name,destination/name]);call(['git','checkout','--quiet',module['revision']],destination/name);call(['git','remote','set-url','origin',module['origin']],destination/name)
  assert not (destination/name/'.git/objects/info/alternates').exists()
def prepare():
 work=ROOT/'build/release-checkout';logs=ROOT/'build/release-logs';logs.mkdir(parents=True,exist_ok=True)
 if work.exists():raise ValueError('Release checkout already exists; keep it for inspection or pass --replace to replace this generated checkout')
 manifest=json.loads((ROOT/'content/source-manifest.json').read_text());files=owned_files(ROOT);inputs=code_inputs(ROOT)
 assert not call(['git','diff','HEAD','--name-only'],ROOT/'apesdk')
 with tempfile.TemporaryDirectory(prefix='canterbury-release-source-') as directory:
  snapshot=Path(directory)
  for name in files:
   source=ROOT/name
   if source.is_symlink():raise ValueError('Owned release source symlink requires explicit admission: '+name)
   target=snapshot/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
  call(['git','init','--quiet'],snapshot);clone_modules(snapshot,ROOT,manifest);call(['git','add','.'],snapshot);call(['git','-c','core.hooksPath=/dev/null','-c','commit.gpgsign=false','-c','user.name=Release Fixture','-c','user.email=release@invalid.example','commit','--quiet','-m','Qualified-source release snapshot'],snapshot)
  commit=call(['git','rev-parse','HEAD'],snapshot);call(['git','clone','--quiet','--no-hardlinks',snapshot,work]);clone_modules(work,ROOT,manifest)
 assert code_inputs(work)==inputs
 plan=[('source',['python3','tools/verify_cycle01.py','--run']),('architecture',['python3','tools/verify_cycle02.py','--run']),('sdk',['python3','tools/verify_sdk_profile.py','--run']),('content',['python3','tools/verify_cycle08.py','--run'])]
 for name,cmd in plan:call(cmd,work,logs/(name+'.log'));print('PASS clean clone '+name,flush=True)
 review_path=work/'content/spatial-review.json';review=json.loads(review_path.read_text());assert not any(review[k] for k in ('edges','doors','dwellings'));review['generation_id']=json.loads((work/'build/content/canterbury.json').read_text())['generation_id'];review_path.write_text(json.dumps(review,indent=2)+'\n');call(['python3','tools/compile_runtime.py','--write'],work,logs/'runtime.log')
 for name in ['verify_cycle03','verify_cycle04','verify_cycle05','verify_cycle06','verify_cycle07','verify_city_cycles','verify_human_cycles','verify_work_cycles','verify_civic_cycles','verify_observer_cycles','verify_final_cycles','verify_xcode']:
  call(['python3','tools/'+name+'.py','--run'],work,logs/(name+'.log'));print('PASS clean clone '+name,flush=True)
 for name in ('presentation-metrics.json','presentation-build.json'):
  source=ROOT/'build'/name
  if not source.exists():raise ValueError('Normally launch PresentationBenchmark.app and finish measurements before release prepare')
  shutil.copy2(source,work/'build'/name)
 presentation=json.loads((work/'build/presentation-build.json').read_text());assert all(sha(work/p)==h for p,h in presentation['inputs'].items());metrics=json.loads((work/'build/presentation-metrics.json').read_text());assert metrics['source_sha256']==presentation['source_sha256']
 call(['python3','tools/verify_final_cycles.py','--performance','--finalize-performance'],work,logs/'performance.log');print('PASS clean clone full-population budgets',flush=True)
 result={'format':'canterbury-clean-clone-candidate','version':1,'snapshot_commit':commit,'code_inputs':inputs,'submodules':manifest['submodules'],'logs':{p.name:sha(p) for p in sorted(logs.glob('*.log'))},'checkout':str(work),'offline':True,'status':'Awaiting native release GUI verification','ok':True};(ROOT/'build/release-candidate.json').write_text(json.dumps(result,indent=2)+'\n');print('READY native release app: '+str(work/'build/CanterburyDetails.app'),flush=True)
def finalize():
 candidate=json.loads((ROOT/'build/release-candidate.json').read_text());work=Path(candidate['checkout']);assert candidate['ok'] and code_inputs(ROOT)==candidate['code_inputs']==code_inputs(work)
 native=json.loads((ROOT/'content/cycle-35-40-native-ui.json').read_text());assert native['ok'] and native.get('release_clone_verified') and native['code_inputs']==candidate['code_inputs']
 for name in ['verify_cycle01','verify_cycle02','verify_cycle03','verify_cycle04','verify_cycle05','verify_cycle06','verify_cycle07','verify_cycle08','verify_city_cycles','verify_human_cycles','verify_work_cycles','verify_civic_cycles','verify_observer_cycles','verify_final_cycles','verify_sdk_profile','verify_xcode']:call(['python3','tools/'+name+'.py','--check'],work)
 # Copy executed qualification evidence and its exact content generation back;
 # never manufacture reports by changing hashes without rerunning their checks.
 for p in (work/'content').iterdir():
  if p.is_file() and (p.name.endswith('-validation.json') or p.name in ('source-manifest.json','spatial-review.json','operation-controls.json','institution-source-matrix.json','civic-source-register.json','religious-worksite-matrix.json')):shutil.copy2(p,ROOT/'content'/p.name)
 for p in (work/'docs/evidence').glob('cycle01-*.log'):shutil.copy2(p,ROOT/'docs/evidence'/p.name)
 for folder in ('content','map','core'):
  shutil.copytree(work/'build'/folder,ROOT/'build'/folder,dirs_exist_ok=True)
 shutil.copytree(work/'build/CanterburyDetails.app',ROOT/'build/CanterburyDetails.app',dirs_exist_ok=True)
 for name in ('app-build.json','cli-build.json','presentation-build.json','presentation-metrics.json'):shutil.copy2(work/'build'/name,ROOT/'build'/name)
 shutil.copy2(work/'build/CanterburyDetails',ROOT/'build/CanterburyDetails')
 evidence={p.name:sha(p) for p in sorted((ROOT/'content').glob('*validation.json')) if p.name!='cycle-40-validation.json'};evidence['cycle-35-40-native-ui.json']=sha(ROOT/'content/cycle-35-40-native-ui.json')
 index={'format':'canterbury-release-evidence-index','version':1,'reports':evidence};(ROOT/'content/evidence-index.json').write_text(json.dumps(index,indent=2)+'\n')
 report={'format':'canterbury-details-cycle40-validation','version':1,'code_inputs':candidate['code_inputs'],'qualified_source_snapshot_commit':candidate['snapshot_commit'],'submodules':candidate['submodules'],'clean_clone_offline':True,'clean_clone_logs':candidate['logs'],'native_ui_sha256':sha(ROOT/'content/cycle-35-40-native-ui.json'),'evidence_index_sha256':sha(ROOT/'content/evidence-index.json'),'checkpoint_compatibility':'Strict v1 ABI/profile/content; no implicit migration','scope':'Ordinary Canterbury 1940, explicit technical operational fixtures; actual source unknowns retained','remote_publication':False,'ok':True};(ROOT/'content/cycle-40-validation.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS complete independent Canterbury release gate')
def main():
 p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--prepare',action='store_true');g.add_argument('--finalize',action='store_true');g.add_argument('--check',action='store_true');p.add_argument('--replace',action='store_true');a=p.parse_args()
 if a.prepare:
  if a.replace:shutil.rmtree(ROOT/'build/release-checkout',ignore_errors=True)
  prepare()
 elif a.finalize:finalize()
 else:
  r=json.loads((ROOT/'content/cycle-40-validation.json').read_text());assert r['ok'] and r['code_inputs']==code_inputs(ROOT);assert r['evidence_index_sha256']==sha(ROOT/'content/evidence-index.json');index=json.loads((ROOT/'content/evidence-index.json').read_text());assert all(sha(ROOT/'content'/p)==h for p,h in index['reports'].items());print('PASS current release-gate evidence')
if __name__=='__main__':main()
