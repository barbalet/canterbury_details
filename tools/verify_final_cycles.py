#!/usr/bin/env python3
"""Qualify the final ordinary-city interfaces, robustness and independent core (35–39)."""
import argparse,hashlib,json,os,platform,re,shutil,subprocess,tempfile
from pathlib import Path
from audit_sources import ROOT,MANIFEST,sha,inventory
from native_sources import CORE,UI
SCOPES={35:'Structured native controls for all 45 operations and bounded read-only management pages matching CLI views',36:'Pure bounded keyboard roster navigation, color-independent marks, readable text controls and declared native input coverage',37:'Measured full-day actual/default/full cohorts, crowded movement, loaded civic/environment, lifecycle churn, checkpoint staging and presentation',38:'Owned allocation/publication failures, malformed content/checkpoints, saturation, repeated owner-isolated sessions and original SDK regressions',39:'Standalone ordinary C core package, self-contained headers/resources, independent mock frontend and extension-RNG purity'}
DOCS=['USER-GUIDE','ACCESSIBILITY','PERFORMANCE','ROBUSTNESS','DEVELOPER-GUIDE','REUSE-FOUNDATION']
def files():return sorted(set(CORE+UI+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+['ui/CityMap.h','ui/CitySession.h','ui/CityManagement.h','ui/Info.plist','tools/verify_final_cycles.py','tools/build_core.py','tools/compile_controls.py','tools/build_presentation.py','tests/release.c','tests/observer.c','tests/presentation.m','examples/ordinary_frontend.c','content/operation-controls.json','content/performance-budgets.json','content/sdk-profile-baseline.json','tools/build.py','tools/native_sources.py','tools/generate_xcode.py']+['docs/'+n+'.md' for n in DOCS]))
def run(cmd,expected=0):
 p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
 if p.returncode!=expected:raise RuntimeError(str(cmd)+'\n'+p.stdout[-2000:]+p.stderr[-4000:])
 return p.stdout+p.stderr
def expected():return {'inputs':{p:sha(ROOT/p) for p in files()},'source_manifest_sha256':sha(ROOT/MANIFEST)}
def write(n,result):
 (ROOT/f'content/cycle-{n}-validation.json').write_text(json.dumps({'format':f'canterbury-details-cycle{n}-validation','version':1,**expected(),'scope':SCOPES[n],**result,'ok':True},indent=2)+'\n')
def build_test():
 run(['python3','tools/city_fixtures.py']);run(['python3','tools/build.py','cli']);cmd=json.loads((ROOT/'build/cli-build.json').read_text())['inputs']['command'];cmd=['tests/release.c' if x=='src/main.c' else x for x in cmd]+['-DCD_TESTING=1','-UNDEBUG','-o','build/release-test'];run(cmd)
 run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only','-DCD_TESTING=1','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Iapesdk','-Isrc',*CORE,'tests/release.c'])
def interface_tests():
 build_test();output=run([ROOT/'build/release-test',35]);(ROOT/'build/cycle35.log').write_text(output);counts=[json.loads(line) for line in output.splitlines() if 'category' in line]
 for row in counts:
  cli=run([ROOT/'build/CanterburyDetails','--city','build/fixtures/civic.bin','--load','build/management.city','--manage',row['category']]);report=json.loads(cli.splitlines()[0]);assert report['total']==row['total'] and report['management']==row['category']
 from compile_controls import build
 catalogue=build(True);assert len(catalogue['commands'])==45
 write(35,{'copied_categories':len(counts),'commands':45,'cli_native_readmodel_match':True,'fixture':json.loads(output.splitlines()[-1])})
 out=run([ROOT/'build/release-test',36]);(ROOT/'build/cycle36.log').write_text(out);write(36,{'fixture':json.loads(out.splitlines()[-1]),'keyboard_counts':[1,199,200,201,27000,30000],'input_scope':'Native keyboard/accessibility-tree checks recorded separately; no controller or physical assistive-device qualification claimed','navigation_rng_and_clock_pure':True})
 (ROOT/'build/invalid-release.city').write_bytes(b'not a city checkpoint\x00')
 out=run([ROOT/'build/release-test',38]);(ROOT/'build/cycle38.log').write_text(out)
 from build_core import build as core_build
 core=core_build()
 with tempfile.TemporaryDirectory(prefix='canterbury-independent-core-') as directory:
  package=Path(directory)/'core';shutil.copytree(ROOT/'build/core',package,ignore=shutil.ignore_patterns('objects'))
  command=['cc','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-std=c11','-O2','-I'+str(package/'include'),str(package/'ordinary_frontend.c'),str(package/'libCanterburyCore.a'),'-lz','-lm','-lpthread','-o',str(package/'Independent')];run(command)
  first=json.loads(run([package/'Independent',package/'resources/CanterburyCity.bin',package/'session.city',32]).splitlines()[-1]);assert first['ok'] and first['checkpoint_match'] and first['pause_rng_pure']
  second=json.loads(run([package/'Independent',package/'resources/CanterburyCity.bin',package/'session2.city',32]).splitlines()[-1]);assert first==second
  link=run(['otool','-L',package/'Independent']);assert all(name not in link for name in ('Cocoa','AppKit','/opt/homebrew/','/usr/local/'))
 symbols=run(['nm',ROOT/'build/core/libCanterburyCore.a']);assert not re.search(r'\bT _?main$',symbols,re.M)
 write(39,{'core_archive_sha256':core['archive_sha256'],'public_headers':list(core['headers']),'independent_relocated_frontend':first,'no_cocoa_or_homebrew_linkage':True,'no_application_entry_point':True})
 write(38,{'fixture':json.loads(out.splitlines()[-1]),'additional_gates':['Cycle 07 every allocation/publication failure','Cycle 08 malformed/offline content','Cycles 09–28 saturation, invalid references, lifecycle and finite-resource fixtures','Cycle 31 all load allocations, failed save publication, corrupt/ABI/content files','SDK native/contract/actors/hybrid isolation against independent pinned baseline'],'sdk_commit':json.loads((ROOT/MANIFEST).read_text())['submodules'][1]['revision']})
def performance():
 build_test();budget=json.loads((ROOT/'content/performance-budgets.json').read_text());results=[]
 for population,mode in [(27000,'actual'),(30000,'actual'),(30000,'crowded'),(30000,'busy'),(30000,'churn')]:
  output=run([ROOT/'build/release-test','perf',population,1440,mode]);path=ROOT/f'build/performance-{mode}-{population}.log';path.write_text(output);r=json.loads(output.splitlines()[-1]);assert r['ok'] and r['checkpoint_match'];assert r['step_seconds']<=budget['step_seconds'][f'{mode}-{population}'];assert r['save_seconds']<=budget['save_seconds'] and r['load_seconds']<=budget['load_seconds'];assert r['peak_rss_bytes']<=budget['peak_rss_bytes'] and r['owned_peak_bytes']<=budget['owned_peak_bytes'];results.append(r);print(f"PASS performance {mode} {population}: {r['step_seconds']:.2f}s/day",flush=True)
 metrics=ROOT/'build/presentation-metrics.json';presentation=json.loads(metrics.read_text()) if metrics.exists() else None
 if presentation:assert presentation['source_sha256']==json.loads((ROOT/'build/presentation-build.json').read_text())['source_sha256']
 if presentation:assert presentation['ok'] and presentation['glyphs']==30000 and presentation['p95_ms']<=budget['presentation_p95_ms']
 machine={'platform':platform.platform(),'machine':platform.machine(),'compiler':subprocess.check_output(['cc','--version'],text=True).splitlines()[0]}
 for key in ('hw.memsize','hw.model','machdep.cpu.brand_string'):
  p=subprocess.run(['sysctl','-n',key],capture_output=True,text=True);machine[key]=p.stdout.strip() if p.returncode==0 else 'unavailable'
 result={'machine':machine,'budgets':budget,'runtime_sha256':sha(ROOT/'build/content/CanterburyCity.bin'),'fixture_sha256':sha(ROOT/'build/fixtures/civic.bin'),'workloads':results,'presentation':presentation,'presentation_pending':not bool(presentation),'optimization':'No compact-storage change: measured day costs fit the predeclared budgets; source/model/replay semantics preserved.'}
 (ROOT/'build/performance-results.json').write_text(json.dumps(result,indent=2)+'\n')
 if presentation:write(37,result)
def finalize_performance():
 r=json.loads((ROOT/'build/performance-results.json').read_text());p=json.loads((ROOT/'build/presentation-metrics.json').read_text());b=r['budgets'];assert p['ok'] and p['glyphs']==30000 and p['p95_ms']<=b['presentation_p95_ms'];assert p['source_sha256']==json.loads((ROOT/'build/presentation-build.json').read_text())['source_sha256'];r['presentation']=p;r['presentation_pending']=False;write(37,r)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',action='store_true');p.add_argument('--performance',action='store_true');p.add_argument('--finalize-performance',action='store_true');p.add_argument('--check',action='store_true');a=p.parse_args();assert inventory()==json.loads((ROOT/MANIFEST).read_text())
 if a.check:
  e=expected()
  for n in SCOPES:
   r=json.loads((ROOT/f'content/cycle-{n}-validation.json').read_text());assert r['ok'] and all(r[k]==v for k,v in e.items());assert not r.get('presentation_pending')
  print('PASS current Cycles 35–39 evidence');return
 if a.run:interface_tests()
 if a.performance:performance()
 if a.finalize_performance:finalize_performance()
if __name__=='__main__':main()
