#!/usr/bin/env python3
"""Qualify the reviewed city-only SDK patches in isolated copies."""
import argparse,hashlib,json,re,shutil,subprocess,tempfile
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory,sha
REPORT=Path('content/cycle-03-sdk-validation.json')
def run(args,cwd):
    p=subprocess.run(list(map(str,args)),cwd=cwd,capture_output=True,text=True)
    if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-1000:]+p.stderr[-1500:])
    return p.stdout+p.stderr

def main():
    parser=argparse.ArgumentParser(description=__doc__);g=parser.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');a=parser.parse_args()
    baseline=inventory();assert baseline==json.loads((ROOT/MANIFEST).read_text())
    if a.check:
        j=json.loads((ROOT/REPORT).read_text());assert j['ok'] and j['source_manifest_sha256']==sha(ROOT/MANIFEST) and j['tool_sha256']==sha(Path(__file__)) and j.get('baseline_file_sha256')==sha(ROOT/'content/sdk-profile-baseline.json');print('PASS reviewed SDK population/profile evidence');return
    results=[];probes=[]
    with tempfile.TemporaryDirectory(prefix='canterbury-details-sdk-') as folder:
        work=Path(folder);current=work/'current';original=work/'original'
        shutil.copytree(ROOT/'apesdk',current,ignore=shutil.ignore_patterns('.git','__pycache__','*.o','build'))
        shutil.copytree(current,original)
        review=json.loads((ROOT/'content/sdk-working-tree.json').read_text())
        anchor=json.loads((ROOT/'content/sdk-profile-baseline.json').read_text())['revision'] if (ROOT/'content/sdk-profile-baseline.json').exists() else review['base_revision']
        changed=subprocess.check_output(['git','-C',str(ROOT/'apesdk'),'diff',anchor,'--name-only'],text=True).splitlines()
        for path in changed:(original/path).write_bytes(subprocess.check_output(['git','-C',str(ROOT/'apesdk'),'show',anchor+':'+path]))
        for name,flags in [('native',[]),('bronze',['-DBRONZE_SIM=1'])]:
            for source_path in ('universe/sim.c','entity/being.c','bronze/actors/bronze_actors.c'):
                outputs=[]
                for sdk in (current,original):
                    text=run(['cc','-E','-P','-Wno-builtin-macro-redefined','-D__DATE__="stable"','-D__TIME__="stable"','-D__TIMESTAMP__="stable"','-DHIDE_THUNDER=1','-DCOMMAND_LINE_EXPLICIT',*flags,source_path],sdk)
                    # Added guarded source lines shift diagnostic __LINE__ metadata only.
                    text=re.sub(r'((?:draw_error|apescript_error)\([^\n]*, "[^"\n]+", )\d+(\))',r'\1<LINE>\2',text.replace(str(sdk),'<SDK>'))
                    # Bronze intentionally appends food vocabulary for native activity
                    # memories; declaration-only additions are not a layout or
                    # a change to these clock/body/registry implementations.
                    if name=='bronze':
                        text=re.sub(r'\bFOOD_(?:GRAIN|FISH|PREPARED|OTHER)\s*,','',text)
                        text=re.sub(r'\b(?:int|n_uint)\s+episodic_simulation_(?:food|chat|summary)\s*\([^;]*\)\s*;','',text)
                    text='\n'.join(line.strip() for line in text.splitlines() if line.strip())
                    outputs.append(hashlib.sha256(text.encode()).hexdigest())
                assert outputs[0]==outputs[1],(name,source_path)
                probes.append({'profile':name,'source':source_path,'preprocessed_simulation_equal':True,'appended_activity_declarations_normalized':name=='bronze','sha256':outputs[0]})
        code='#include "universe/universe.h"\n#include <stdio.h>\nint main(void){printf("%u %zu %zu\\n",(unsigned)LARGE_SIM,sizeof(simulated_being),sizeof(simulated_group));return 0;}\n'
        source=work/'layout.c';source.write_text(code)
        for name,flags in [('native',[]),('bronze',['-DBRONZE_SIM=1']),('city',['-DBRONZE_SIM=1','-DCANTERBURY_SIM=1'])]:
            values=[]
            for i,sdk in enumerate((current,original)):
                exe=work/(name+str(i));run(['cc','-std=c11','-I'+str(sdk),*flags,source,'-o',exe],work);values.append(run([exe],work).strip())
            if name!='city':assert values[0]==values[1],name
            else:assert values[0].split()[1]==values[1].split()[1] and int(values[1].split()[0]) in (27000,30000)
            assert int(values[0].split()[0])==(30000 if name=='city' else 256)
            probes.append({'profile':name,'unchanged_layout':values[0]==values[1],'unchanged_body_layout':True,'capacity_body_group_bytes':values[0],'base_capacity_body_group_bytes':values[1]})
        for name,command in [('native',['bash','test.sh']),('contract',['python3','bronze/contract/check.py']),('actors',['bash','bronze/actors/test.sh']),('hybrid',['bash','bronze/hybrid/test.sh'])]:
            output=run(command,current);log=ROOT/'build'/('cycle03-sdk-'+name+'.log');log.parent.mkdir(exist_ok=True);log.write_text(output);results.append({'suite':name,'passed':True,'log':str(log.relative_to(ROOT)),'log_sha256':sha(log)});print('PASS isolated SDK '+name,flush=True)
    assert inventory()==baseline
    j={'format':'canterbury-details-sdk-population-qualification','version':1,'source_manifest_sha256':sha(ROOT/MANIFEST),'tool_sha256':sha(Path(__file__)),'population':27000,'capacity':30000,'storage_scope':'Cycle 07 guarded host-owned storage','change_scope':'BRONZE_SIM/CANTERBURY_SIM native activity memories plus guarded city capacity/storage and checkpoint staging; unflagged native profiles unchanged','probes':probes,'suites':results,'original_checkout_untouched_by_tests':True,'sdk_changes_uncommitted':bool(review['files']),'comparison_revision':anchor,'baseline_file_sha256':sha(ROOT/'content/sdk-profile-baseline.json'),'ok':True}
    (ROOT/REPORT).write_text(json.dumps(j,indent=2)+'\n');print('PASS guarded 30000-capacity SDK change, 27000 default; ordinary profiles/layouts unchanged')
if __name__=='__main__':main()
