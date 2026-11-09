#!/usr/bin/env python3
"""Qualify offline candidate compilation, reproducibility and rejection contracts."""
import argparse,json,re,subprocess
from audit_sources import ROOT,MANIFEST,inventory,sha
from compile_content import OUTPUT,LOCAL,parse,canonical,digest
REPORT=ROOT/'content/cycle-08-validation.json'
FILES=['Makefile','tools/verify_cycle08.py','tools/compile_content.py','tests/test_content.py','content/content-policy.json','content/review-annotations.json','content/content.schema.json','docs/CONTENT.md']
def run(args):
    p=subprocess.run(list(map(str,args)),cwd=ROOT,capture_output=True,text=True)
    if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-1200:]+p.stderr[-2200:])
    return p.stdout+p.stderr

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    baseline=inventory();assert baseline==json.loads((ROOT/MANIFEST).read_text())
    expected={'inputs':{p:sha(ROOT/p) for p in FILES},'compiler_owned_inputs':{p:sha(ROOT/p) for p in LOCAL},'source_manifest_sha256':sha(ROOT/MANIFEST)}
    target=ROOT/OUTPUT
    if a.check:
        j=json.loads(REPORT.read_text());assert j['ok'] and all(j[k]==v for k,v in expected.items())
        assert target.is_file() and sha(target)==j['artifact_sha256'];print('PASS Cycle 08 current compiler/source/artifact evidence');return
    output=run(['python3','-m','unittest','discover','-s','tests','-p','test_content.py','-v'])
    (ROOT/'build').mkdir(exist_ok=True);(ROOT/'build/cycle08-contracts.log').write_text(output)
    match=re.search(r'Ran (\d+) tests',output);assert match and 'OK' in output
    first=json.loads(run(['python3','tools/compile_content.py','--write']).splitlines()[-1]);artifact=sha(target)
    repeat=json.loads(run(['python3','tools/compile_content.py','--write']).splitlines()[-1]);assert first==repeat and sha(target)==artifact
    run(['python3','tools/compile_content.py','--check'])
    bundle=parse(target.read_bytes());generation=bundle.pop('generation_id');assert digest(canonical(bundle))==generation
    assert bundle['admission']=={'candidate_only':True,'routing_enabled':False,'historical_identity_inferred':False}
    assert bundle['statistics']['buildings']==3341 and bundle['statistics']['institutions']==33 and bundle['statistics']['unlocated_institutions']==5
    synagogue=next(s for s in bundle['data']['institutions']['sites'] if s['id']=='old-synagogue');assert synagogue['worship_status_1940']=='scenario_active' and 'closure' in synagogue['notes'].lower()
    assert inventory()==baseline
    report={'format':'canterbury-details-cycle08-validation','version':1,**expected,'contract_tests':int(match.group(1)),'artifact_path':OUTPUT.as_posix(),'artifact_bytes':first['bytes'],'artifact_sha256':artifact,'generation_id':generation,'repeat_byte_identical':True,'cli_freshness_check_passed':True,'statistics':bundle['statistics'],'historical_uncertainty_preserved':True,'network_access_required':False,'sibling_project_required':False,'submodules_modified_by_compilation':False,'runtime_content_api_implemented':False,'ok':True}
    REPORT.write_text(json.dumps(report,indent=2)+'\n');print('PASS Cycle 08 offline compilation, byte determinism, schema/provenance/review contracts and atomic failure preservation')
if __name__=='__main__':main()
