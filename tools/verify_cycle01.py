#!/usr/bin/env python3
"""Run/check Cycle 01 source qualification without modifying either submodule."""
import argparse
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from audit_sources import ROOT, MANIFEST, TESTS, inventory, sha

REPORT=Path('content/cycle-01-validation.json')
def check():
    expected=json.loads((ROOT/MANIFEST).read_text())
    if inventory()!=expected:raise ValueError('Source inventory no longer matches checkout')
    evidence=json.loads((ROOT/REPORT).read_text())
    if not evidence['ok'] or evidence['source_manifest_sha256']!=sha(ROOT/MANIFEST):raise ValueError('Stale Cycle 01 evidence')
    required=[cmd for module,cmd,_ in TESTS if module=='canterbury']
    if [r['command'] for r in evidence['runs']]!=required:raise ValueError('Incomplete upstream source suite')
    for run in evidence['runs']:
        if run['exit_code'] or sha(ROOT/run['log'])!=run['log_sha256']:raise ValueError('Invalid Cycle 01 test evidence')
    print('PASS Cycle 01 evidence: offline upstream validators/tests, exact input inventory, unchanged submodules')
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    if a.check:check();return
    baseline=inventory()
    if baseline!=json.loads((ROOT/MANIFEST).read_text()):raise ValueError('Review/write the source baseline first')
    runs=[]
    with tempfile.TemporaryDirectory(prefix='canterbury-details-cycle01-') as temporary:
        copy=Path(temporary)/'canterbury';shutil.copytree(ROOT/'canterbury',copy,ignore=shutil.ignore_patterns('.git','__pycache__'))
        for module,command,purpose in TESTS:
            if module!='canterbury':continue
            process=subprocess.run(command,cwd=copy,capture_output=True,text=True)
            output=process.stdout+process.stderr
            log=Path('docs/evidence')/f'cycle01-{len(runs)+1}.log';(ROOT/log).parent.mkdir(parents=True,exist_ok=True);(ROOT/log).write_text(output)
            result={'command':command,'purpose':purpose,'exit_code':process.returncode,'log':log.as_posix(),'log_sha256':sha(ROOT/log)}
            match=re.search(r'Ran (\d+) tests?',output)
            if match:result['tests_run']=int(match.group(1))
            runs.append(result)
            if process.returncode:raise RuntimeError('Upstream source validation failed: '+str(log))
    if inventory()!=baseline:raise ValueError('Original submodule changed during isolated validation')
    result={'format':'canterbury-details-cycle01-validation','version':1,'date':'2026-11-08','scope':'source inventory and upstream map data validation, not standalone simulation/runtime qualification','source_manifest_sha256':sha(ROOT/MANIFEST),'runs':runs,'sdk_suites':'entry points inventoried; execution belongs to SDK/build/profile implementation cycles','original_submodules_unchanged':True,'network_required':False,'sibling_project_required':False,'ok':True}
    (ROOT/REPORT).write_text(json.dumps(result,indent=2)+'\n');check()
if __name__=='__main__':main()
