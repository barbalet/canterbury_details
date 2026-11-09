#!/usr/bin/env python3
"""Offline Cycle 01 inventory. --write records a reviewed baseline; --check verifies it."""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MANIFEST=Path('content/source-manifest.json')
INPUTS=[
 ('canterbury/newcanterbury/canterbury.json','source_vectors','objects[]; preserve classes, coordinates and provenance'),
 ('canterbury/simulation-map/road-traces.json','source_guided_traces','routes[]; not survey-qualified'),
 ('canterbury/simulation-map/derived/buildings.json','building_candidates','objects[].id'),
 ('canterbury/simulation-map/derived/buildings-enriched.json','modern_enrichment','objects[].id; suggested use/labels are not 1940 facts'),
 ('canterbury/simulation-map/derived/buildings-1940.json','institution_building_candidates','objects[].id; site association is a candidate'),
 ('canterbury/simulation-map/derived/road-network.json','modern_roads','network.nodes[].id, network.edges[].id; preserve source topology and traversable flags'),
 ('canterbury/simulation-map/derived/road-network-historical-traces.json','source_guided_roads','network.nodes[].id, network.edges[].id; not era-qualified'),
 ('canterbury/simulation-map/derived/waterways.json','water_candidates','objects[].id, network nodes/edges'),
 ('canterbury/simulation-map/derived/rail.json','rail_ink_candidates','objects[].id, network nodes/edges'),
 ('canterbury/simulation-map/derived/rail-corridors.json','rail_reference','nodes[].id, edges[].id; not an operating service'),
 ('canterbury/simulation-map/religious-sites-1940.json','curated_institutions','sites[].id; setting assignments, references and worship status'),
 ('canterbury/simulation-map/derived/religious-sites-1940.json','derived_institutions','sites[].id; preserve override, confidence and unresolved locations'),
 ('canterbury/simulation-map/derived/review-queue.json','unresolved_geometry','candidates[]; unresolved, not approved links'),
 ('canterbury/simulation-map/derived/report.json','assembly_provenance','inputs, parameters, layers and limitations'),
 ('canterbury/simulation-map/derived/building-enrichment-report.json','enrichment_provenance','reported matches and limitations'),
 ('canterbury/simulation-map/derived/road-connectivity-report.json','topology_provenance','reported components and source connectivity'),
 ('canterbury/simulation-map/modern/sources.json','modern_snapshot_provenance','attribution, license, retrieved_utc, tiles[].sha256, osm_sha256'),
 ('canterbury/simulation-map/modern/control-points.json','approximate_registration','control points and rejected controls'),
 ('apesdk/bronze/content/bronze_age_full.bronze','engine_vocations','vocation identifiers; capabilities, not a 1940 occupation census'),
 ('apesdk/universe/universe.h','native_profile_contract','CANTERBURY_POPULATION, LARGE_SIM, canterbury_cycle_handler, sim_canterbury_configure'),
 ('apesdk/universe/sim.c','native_city_clock','guarded configure/init/cycle/close branches'),
 ('apesdk/entity/being.c','native_body','native position/energy and city-specific awake behavior'),
 ('apesdk/bronze/actors/bronze_actors.h','actor_contract','BronzeActorId, registry/reset/rebind and guarded slot APIs'),
 ('apesdk/bronze/work/bronze_work.h','work_contract','guarded city work claims/close; one native budget'),
 ('apesdk/bronze/tasks/bronze_tasks.h','task_contract','bounded resumable native/Bronze tasks'),
 ('apesdk/bronze/hybrid/bronze_hybrid.h','render_contract','guarded city overlay registration and draw context'),
 ('apesdk/sim/sim.h','landscape_checkpoint_contract','guarded land snapshot/install and Bronze weather continuation'),
]
TESTS=[
 ('canterbury',['python3','simulation-map/validate.py'],'candidate geometry/schema validation'),
 ('canterbury',['python3','simulation-map/validate_buildings.py'],'building enrichment admission'),
 ('canterbury',['python3','-m','unittest','discover','-s','simulation-map','-v'],'assembly, enrichment, OSM topology, road completion and religious-site fixtures'),
 ('apesdk',['bash','test.sh'],'native and included Bronze regression entry point'),
 ('apesdk',['python3','bronze/contract/check.py'],'native/Bronze build and observable contract'),
 ('apesdk',['bash','bronze/actors/test.sh'],'identity, lifecycle, rebind and sidecar tests'),
 ('apesdk',['bash','bronze/hybrid/test.sh'],'hybrid terrain, routing and bounded economy'),
]
def git(folder,*args):return subprocess.check_output(['git','-C',str(folder),*args],text=True).strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inventory(root=ROOT):
    modules=[];files=[]
    review_path=root/'content/sdk-working-tree.json'
    review=json.loads(review_path.read_text()) if review_path.exists() else None
    for name in ('canterbury','apesdk'):
        folder=root/name;revision=git(folder,'rev-parse','HEAD')
        pin=git(root,'ls-tree','HEAD',name).split()
        if len(pin)<3 or pin[0]!='160000' or pin[2]!=revision:raise ValueError(f'{name}: checkout differs from committed parent pin')
        modified=[p for p in git(folder,'diff','HEAD','--name-only','-z').split('\0') if p]
        if modified:
            if name!='apesdk' or not review or review['base_revision']!=revision:raise ValueError(f'{name}: tracked modifications require review')
            approved=review['files']
            if set(modified)!=set(approved):raise ValueError('SDK changes differ from reviewed working-tree inventory')
            for path,expected in approved.items():
                relative=Path(path)
                if relative.is_absolute() or '..' in relative.parts or sha(folder/relative)!=expected:raise ValueError('Unreviewed SDK file content: '+path)
        paths=git(folder,'ls-files','-z').split('\0')
        count=0
        for relative in sorted(p for p in paths if p):
            path=folder/relative
            if path.is_symlink():
                target=os.readlink(path)
                if not path.resolve().is_relative_to(folder.resolve()) or not path.resolve().exists():raise ValueError('External or broken upstream symlink: '+str(path))
                files.append({'path':f'{name}/{relative}','kind':'internal_symlink','target':target,'sha256':hashlib.sha256(target.encode()).hexdigest()});count+=1;continue
            if not path.is_file():raise ValueError('Missing tracked source: '+str(path))
            files.append({'path':f'{name}/{relative}','bytes':path.stat().st_size,'sha256':sha(path) });count+=1
        modules.append({'path':name,'revision':revision,'origin':git(folder,'remote','get-url','origin'),'tracked_files':count,'reviewed_worktree_files':modified})
    facts=[]
    for path,role,selector in INPUTS:
        full=root/path
        if not full.is_file():raise ValueError('Missing factual input: '+path)
        entry={'path':path,'role':role,'selector':selector,'sha256':sha(full)}
        if full.suffix=='.json':
            data=json.loads(full.read_text());entry['format']=data.get('format','upstream JSON');entry['keys']=sorted(data)
            for key in ('objects','sites','nodes','edges','candidates'):
                records=data.get(key)
                if isinstance(records,list):
                    entry[key+'_count']=len(records)
                    ids=[x['id'] for x in records if isinstance(x,dict) and 'id' in x]
                    if len(ids)!=len(set(ids)):raise ValueError(f'{path}: duplicate {key} IDs')
                    if ids:entry[key+'_sample_ids']=ids[:3]
            network=data.get('network',{})
            if isinstance(network,dict):
                entry['network_counts']={k:len(network[k]) for k in ('nodes','edges') if isinstance(network.get(k),list)}
                edges=network.get('edges',[])
                if edges:entry['traversable_true']=sum(e.get('traversable') is True for e in edges)
        facts.append(entry)
    return {'format':'canterbury-details-source-inventory','version':1,'audit_date':'2026-11-08','setting':{'place':'Canterbury, Kent, England','year':1940,'population':'not historically established by this audit; SDK default is a capacity fixture','model':'ordinary human city; religion as occupations/institutions/community services'},'submodules':modules,'sdk_worktree_review':({'path':'content/sdk-working-tree.json','sha256':sha(review_path)} if review else None),'root_license':{'path':'LICENSE','sha256':sha(root/'LICENSE')},'factual_inputs':facts,'regression_entry_points':[{'submodule':m,'command':cmd,'purpose':purpose} for m,cmd,purpose in TESTS],'tracked_files':files}
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    result=inventory();text=json.dumps(result,indent=2,ensure_ascii=False)+'\n';target=ROOT/MANIFEST
    if a.write:target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text)
    elif target.read_text()!=text:raise SystemExit('Source inventory differs: review pins/files/inputs before updating the baseline')
    print(f"PASS Cycle 01: {len(result['submodules'])} pinned submodules, {len(result['tracked_files'])} hashed files, {len(result['factual_inputs'])} mapped input contracts")
if __name__=='__main__':main()
