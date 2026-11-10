#!/usr/bin/env python3
"""Offline advisory building/occupation hypotheses; never historical or runtime assignments."""
import argparse, collections, hashlib, json, math
from pathlib import Path
from compile_content import ROOT, canonical, atomic_write
from audit_sources import inventory, MANIFEST
SOURCE='canterbury/simulation-map/derived/buildings-enriched.json'
POLICY='content/building-use-estimate-policy.json'
OUTPUT='content/building-use-estimates.json'

def sha(raw): return hashlib.sha256(raw).hexdigest()
def estimate(rows, policy):
    ids=[r['id'] for r in rows]
    if len(set(ids))!=len(ids): raise ValueError('Duplicate source building ID')
    for r in rows:
        b=r['bounds']
        if len(b)!=4 or not all(type(x) in (int,float) and math.isfinite(x) for x in b) or b[0]>b[2] or b[1]>b[3]: raise ValueError('Invalid building bounds')
    center=lambda r:((r['bounds'][0]+r['bounds'][2])/2,(r['bounds'][1]+r['bounds'][3])/2)
    anchors=[r for r in rows if r.get('use','unknown') in policy['affinities']]
    results=[]
    for r in sorted(rows,key=lambda x:x['id']):
        if r.get('use','unknown')!='unknown': continue
        x,y=center(r);near=[]
        for a in anchors:
            ax,ay=center(a);d=math.hypot(ax-x,ay-y)
            if d<=policy['radius_pixels']: near.append((d,a['id'],a))
        near=sorted(near,key=lambda x:(x[0],x[1]))[:policy['maximum_anchors']]
        scores=collections.Counter()
        for d,_,a in near:
            weight=1/(1+d/policy['distance_scale_pixels'])
            for use,affinity in policy['affinities'][a['use']].items():scores[use]+=weight*affinity
        ranked=sorted(scores.items(),key=lambda x:(-x[1],x[0]))
        if ranked:
            use,top=ranked[0];share=top/sum(scores.values());margin=(top-ranked[1][1])/top if len(ranked)>1 else 1
            confidence='moderate' if len(near)>=3 and share>=.55 and margin>=.3 else 'low'
            reason='Distance-weighted source-use neighbourhood plus disclosed complementary-service affinities'
        else:
            use=policy['fallback_use'];share=0;confidence='very_low';reason='No classified anchor in radius; explicit residential scenario fallback'
        results.append({'building_id':r['id'],'source_use':'unknown','estimated_use':use,'confidence':confidence,'not_historical_fact':True,'requires_review':True,'basis':reason,'bounds':r['bounds'],'area_pixels':r.get('area_pixels'),'support_share':round(share,4),'alternatives':[{'use':k,'score':round(v,4)} for k,v in ranked[:3]],'anchors':[{'building_id':ident,'source_use':a['use'],'distance_pixels':round(d,3),'historical_identity_verified':bool(a.get('modern_match',{}).get('historical_identity_verified',False))} for d,ident,a in near],'suggested_workplace_roles':policy['roles'][use],'operational_assignment':False})
    return results

def build(check=False):
    if inventory()!=json.loads((ROOT/MANIFEST).read_text()):raise ValueError('Source audit is stale')
    paths=[SOURCE,POLICY,'tools/estimate_building_uses.py'];raw={p:(ROOT/p).read_bytes() for p in paths}
    source=json.loads(raw[SOURCE]);policy=json.loads(raw[POLICY]);rows=source['objects'];guesses=estimate(rows,policy)
    report={'format':'canterbury-details-advisory-building-use-estimates','version':1,'scope':'User-authorized provisional simulation guesses, not 1940 facts, verified modern identity, safe access, capacity, employment or routes. Euclidean neighbours can be separated by water, rail or walls. No guessed building is used as evidence for another guess.','inputs':{p:sha(b) for p,b in raw.items()},'source_manifest_sha256':sha((ROOT/MANIFEST).read_bytes()),'source_use_counts':dict(sorted(collections.Counter(r.get('use','unknown') for r in rows).items())),'estimated_use_counts':dict(sorted(collections.Counter(r['estimated_use'] for r in guesses).items())),'confidence_counts':dict(sorted(collections.Counter(r['confidence'] for r in guesses).items())),'estimates':guesses}
    output=canonical(report)
    if check:
        if (ROOT/OUTPUT).read_bytes()!=output:raise ValueError('Stale building-use estimates')
    else:
        def fresh():
            if any((ROOT/p).read_bytes()!=b for p,b in raw.items()):raise ValueError('Inputs changed during estimation')
        atomic_write(ROOT/OUTPUT,output,fresh)
    return {k:report[k] for k in ['estimated_use_counts','confidence_counts']}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args();print(json.dumps(build(a.check),sort_keys=True))
