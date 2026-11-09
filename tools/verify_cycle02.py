#!/usr/bin/env python3
"""Cycle 02 design/header qualification; no city functions are implemented or linked."""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from audit_sources import ROOT, MANIFEST, inventory, sha

REPORT=Path('content/cycle-02-validation.json')
FILES=['content/architecture.json','content/reuse-ledger.json','src/cd_api.h','docs/ARCHITECTURE.md','docs/REUSE.md','tools/verify_cycle02.py']
PUBLIC={'cd_content_open','cd_content_close','cd_world_open','cd_world_close','cd_world_step','cd_world_clock','cd_world_preview','cd_world_submit','cd_world_cancel','cd_snapshot_capture','cd_snapshot_close','cd_snapshot_person_count','cd_snapshot_person','cd_snapshot_institution','cd_world_digest','cd_world_save','cd_world_load'}
def local(path):
    p=Path(path)
    if p.is_absolute() or '..' in p.parts:raise ValueError('Nonlocal dependency path: '+str(path))
    return p

def validate(contract,ledger):
    if contract['project']['runtime_dependencies']!=['canterbury','apesdk'] or contract['project']['population_model']!='ordinary_humans_only':raise ValueError('Invalid source or human scope')
    nodes={}
    for node in contract['nodes']:
        if node['id'] in nodes:raise ValueError('Duplicate dependency node')
        local(node['path']);nodes[node['id']]=node
    upstream={n['path'] for n in nodes.values() if n['scope']=='submodule'}
    if upstream!={'canterbury','apesdk'}:raise ValueError('Unexpected upstream dependency')
    if any(n['scope'] not in ('project','submodule') for n in nodes.values()):raise ValueError('Unknown dependency scope')
    visiting=set();complete=set()
    def visit(key):
        if key not in nodes:raise ValueError('Unknown dependency: '+key)
        if key in visiting:raise ValueError('Compile dependency cycle: '+key)
        if key in complete:return
        visiting.add(key)
        for dependency in nodes[key]['depends_on']:visit(dependency)
        visiting.remove(key);complete.add(key)
    for key in nodes:visit(key)
    fields=set();owners=set()
    for owner in contract['owners']:
        if owner['module'] not in nodes or owner['module'] in owners:raise ValueError('Unknown or duplicate module owner')
        owners.add(owner['module'])
        for group in owner['authoritative_field_groups']:
            if group in fields:raise ValueError('Duplicate authoritative field owner: '+group)
            fields.add(group)
    for cache in contract['derived_caches']:
        if cache['source_owner'] not in owners or cache['field_group'] in fields:raise ValueError('Derived cache treated as authoritative')
    expected={key for key in nodes if key.startswith('project.') and key!='project.api'}
    if owners!=expected:raise ValueError('Module missing ownership declaration')
    if ledger['implementation_copied_in_cycle02'] or not ledger['entries']:raise ValueError('Incorrect reuse stage')
    seen=set()
    for entry in ledger['entries']:
        if entry['runtime_dependency'] or entry['reference_path'] in seen:raise ValueError('Reference became a runtime dependency')
        seen.add(entry['reference_path'])
        if entry['decision'] not in ('adapt_selected_algorithms','reimplement_around_selected_algorithms','reimplement','exclude') or not re.fullmatch('[0-9a-f]{64}',entry['reviewed_sha256']) or not entry['required_changes_or_exclusion']:raise ValueError('Unreviewed reuse entry')
    header=(ROOT/local(contract['public_header'])).read_text()
    actual=set(re.findall(r'^(?:CdStatus|void|size_t)\s+(cd_\w+)\(',header,re.M))
    if actual!=PUBLIC:raise ValueError('Public declarations differ from the Cycle 02 contract')
    for folder in ('src','ui'):
        for path in (ROOT/folder).rglob('*'):
            if not path.is_file() or path.suffix not in ('.h','.c','.m','.mm'):continue
            text=path.read_text()
            for symbol in contract['forbidden_owned_symbols']:
                if re.search(r'\b'+re.escape(symbol)+r'\w*\b',text):raise ValueError('Forbidden owned runtime symbol: '+symbol)
            for include in re.findall(r'^\s*#\s*include\s*"([^"]+)"',text,re.M):
                resolved=(path.parent/include).resolve()
                if not resolved.is_relative_to(ROOT.resolve()):raise ValueError('Sibling include dependency')
    return {'dependency_nodes':len(nodes),'authoritative_field_groups':len(fields),'owner_modules':len(owners),'reviewed_reference_files':len(seen),'public_functions':len(actual)}

def load():return json.loads((ROOT/'content/architecture.json').read_text()),json.loads((ROOT/'content/reuse-ledger.json').read_text())
def qualification():
    contract,ledger=load();counts=validate(contract,ledger);rejections=0
    mutations=[lambda c:c['nodes'][0].update(path='../other'),lambda c:c['nodes'][0]['depends_on'].append('project.world'),lambda c:c['owners'][1]['authoritative_field_groups'].append(c['owners'][0]['authoritative_field_groups'][0]),lambda c:c['nodes'].append({'id':'third','scope':'submodule','path':'third','depends_on':[]})]
    for mutate in mutations:
        changed=deepcopy(contract);mutate(changed)
        try:validate(changed,ledger)
        except ValueError:rejections+=1
        else:raise ValueError('Invalid architecture admitted')
    probes=[]
    for language,compiler,standard in [('c','cc','c11'),('c++','c++','c++17')]:
        executable=shutil.which(compiler)
        if not executable:raise RuntimeError('Compiler unavailable: '+compiler)
        assertion='_Static_assert' if language=='c' else 'static_assert'
        source='#include "cd_api.h"\n'+assertion+'(sizeof(CdPersonId)==8, "stable IDs are 64 bits");\nCdStatus (*contract_step)(CdWorld *,unsigned,CdDiagnostic *)=cd_world_step;\n'
        options=[('city',['-DCANTERBURY_SIM=1','-DBRONZE_SIM=1'],True),('missing_profile',[],False),('disabled_city',['-DCANTERBURY_SIM=0','-DBRONZE_SIM=1'],False),('missing_bridge',['-DCANTERBURY_SIM=1'],False)]
        for name,flags,success in options:
            command=[executable,'-std='+standard,'-Wall','-Wextra','-Werror','-fsyntax-only','-x',language,'-Isrc',*flags,'-']
            result=subprocess.run(command,input=source,text=True,cwd=ROOT,capture_output=True)
            if (result.returncode==0)!=success:raise RuntimeError('Header/profile probe failed: '+name+'\n'+result.stderr)
            if not success and 'requires' not in result.stderr:raise RuntimeError('Negative probe failed for an unrelated reason')
            probes.append({'language':language,'profile':name,'expected_compile_success':success,'observed_compile_success':not result.returncode})
    return {'format':'canterbury-details-cycle02-validation','version':1,'date':'2026-11-08','scope':'architecture admission and declaration syntax only; no runtime, linkage or SDK behavior qualification','source_manifest_sha256':sha(ROOT/MANIFEST),'design_sha256':{p:sha(ROOT/p) for p in FILES},'checks':counts,'negative_contract_cases_rejected':rejections,'header_probes':probes,'reference_checkout_required':False,'submodules_changed':False,'ok':True}

def check_sources():
    if inventory()!=json.loads((ROOT/MANIFEST).read_text()):raise ValueError('Cycle 01 source baseline changed')
def main():
    parser=argparse.ArgumentParser(description=__doc__);g=parser.add_mutually_exclusive_group(required=True);g.add_argument('--run',action='store_true');g.add_argument('--check',action='store_true');args=parser.parse_args();check_sources()
    if args.run:
        result=qualification();(ROOT/REPORT).write_text(json.dumps(result,indent=2)+'\n')
    else:
        contract,ledger=load();validate(contract,ledger);result=json.loads((ROOT/REPORT).read_text())
        if not result['ok'] or result['design_sha256']!={p:sha(ROOT/p) for p in FILES} or result['source_manifest_sha256']!=sha(ROOT/MANIFEST):raise ValueError('Cycle 02 evidence is stale')
    print('PASS Cycle 02: local dependency graph, single field owners, reuse review, neutral API and C/C++ profile contracts')
if __name__=='__main__':main()
