#!/usr/bin/env python3
"""Pack qualified runtime geometry from the audited Cycle 08 bundle; no network."""
import argparse,hashlib,json,struct,zlib,sys
from pathlib import Path
import compile_content as cc
ROOT=cc.ROOT
OUTPUT=ROOT/'build/content/CanterburyCity.bin'
REVIEW=ROOT/'content/spatial-review.json'
def uid(namespace,value):
    n=1469598103934665603
    for b in (namespace+':'+str(value)).encode():n=((n^b)*1099511628211)&((1<<64)-1)
    if not n:raise ValueError('Zero generated entity ID')
    return n
class Writer:
    def __init__(self):self.data=bytearray()
    def u(self,n):self.data+=struct.pack('<I',n)
    def q(self,n):self.data+=struct.pack('<Q',n)
    def i(self,n):self.data+=struct.pack('<i',n)
    def f(self,n):self.data+=struct.pack('<d',n)
    def s(self,s):
        raw=s.encode('utf8');assert len(raw)<=4096 and b'\0' not in raw;self.u(len(raw));self.data+=raw+b'\0'
    def p(self,p):self.f(p[0]);self.f(p[1])
def encode(model):
    w=Writer();polys=model['polygons'];nodes=model['nodes'];edges=model['edges'];doors=model.get('doors',[]);sites=model.get('sites',[])
    ring_count=sum(len(p['rings']) for p in polys);point_count=sum(sum(len(r) for r in p['rings']) for p in polys)+sum(len(e['points']) for e in edges)+sum(len(d['points']) for d in doors)
    w.u(model['width']);w.u(model['height']);w.s(model['generation']);w.s(model['basis'])
    for n in (len(polys),ring_count,point_count,len(nodes),len(edges),len(doors),len(sites)):w.u(n)
    for p in polys:
        w.q(p['id'])
        for n in (p['kind'],int(p.get('capacity_known',False)),p.get('capacity',0),int(p.get('opening_known',False)),p.get('opens',0),p.get('closes',0),int(p.get('locked',False)),len(p['rings'])):w.u(n)
        for key in ('source','name','use','policy_basis'):w.s(p.get(key,''))
        w.f(p.get('confidence',0))
        for r in p['rings']:
            w.u(len(r))
            for point in r:w.p(point)
    for n in nodes:w.q(n['id']);w.s(n['source']);w.p(n['point'])
    for e in edges:
        w.q(e['id']);w.q(e['from']);w.q(e['to'])
        for key in ('qualified','bridge','rail_crossing','capacity'):w.u(int(e.get(key,0)))
        w.i(e.get('grade',0));w.u(int(e.get('junction',False)));w.s(e['source']);w.s(e['basis']);w.u(len(e['points']))
        for point in e['points']:w.p(point)
    for d in doors:
        for key in ('id','building','node'):w.q(d[key])
        w.u(int(d['reviewed']));w.s(d['basis']);w.u(len(d['points']))
        for p in d['points']:w.p(p)
    for s in sites:
        w.q(s['id']);w.q(s.get('building',0));w.u(int(s['point'] is not None));w.p(s['point'] or [0,0])
        for key in ('source','name','religion','denomination','status','notes','site_type','source_record','location_basis'):w.s(s.get(key,''))
        for key in ('setting_year','identity_verified','review_required','confidence_known'):w.u(int(s.get(key,0)))
        w.f(s.get('confidence',0))
    payload=bytes(w.data);return b'CDCT0002'+struct.pack('<QI',len(payload),zlib.crc32(payload))+payload

def compile_model(bundle,review):
    cc.require(set(review)=={'format','version','generation_id','edges','dwellings','doors'},'Unknown spatial-review fields')
    cc.require(review['format']=='canterbury-details-spatial-review' and review['version']==1 and review['generation_id']==bundle['generation_id'],'Stale spatial review generation')
    data=bundle['data'];used=set()
    def stable(ns,key):
        value=uid(ns,key);cc.require(value not in used,'Entity ID collision');used.add(value);return value
    def reviews(items,keys):
        records={}
        for item in items:
            cc.require(set(item)==set(keys),'Unsupported review fields')
            key=(item.get('network'),item['source_id']);cc.require(key not in records,'Duplicate spatial review')
            cc.require(item['basis'] in ('reviewed-source','technical-fixture') and isinstance(item['note'],str) and item['note'].strip(),'Missing explicit review basis/note')
            records[key]=item
        return records
    er=reviews(review['edges'],['network','source_id','input_sha256','basis','note','bridge','rail_crossing','capacity','grade','junction'])
    dr=reviews(review['dwellings'],['source_id','input_sha256','basis','note','capacity','opening_known','opens','closes','locked'])
    polys=[];buildings={};seen_d=set()
    features={f['id']:f for f in data['vectors']['features']}
    for key,kind in [('buildings',1),('waterways',2),('rail_ink',3)]:
        for p in data[key]['objects']:
            ident=stable(key,p['id']);policy=dr.get((None,p['id'])) if kind==1 else None
            record={'id':ident,'kind':kind,'source':p['id'],'name':p.get('modern_annotation',{}).get('name','unknown'),'use':p.get('modern_annotation',{}).get('use','unknown'),'rings':p['geometry']['rings'],'policy_basis':'unreviewed','confidence':min(features[x].get('confidence',0) for x in p['source_feature_ids'])}
            if kind==1:buildings[p['id']]=ident
            if policy:
                cc.require(policy['input_sha256']==bundle['provenance']['inputs']['buildings']['sha256'],'Stale dwelling review')
                cc.require(type(policy['capacity']) is int and 0<=policy['capacity']<=128,'Invalid model dwelling capacity')
                for flag in ('opening_known','locked'):cc.require(type(policy[flag]) is bool,'Non-boolean dwelling review flag')
                cc.require(all(type(policy[x]) is int and 0<=policy[x]<1440 for x in ('opens','closes')),'Invalid opening minutes')
                record.update(capacity_known=True,capacity=policy['capacity'],opening_known=policy['opening_known'],opens=policy['opens'],closes=policy['closes'],locked=policy['locked'],policy_basis=policy['basis']+': '+policy['note']);seen_d.add((None,p['id']))
            polys.append(record)
    cc.require(seen_d==dr.keys(),'Unknown dwelling review target')
    nodes=[];edges=[];node_ids={};seen_e=set()
    for key in ('road_ink','modern_roads','historical_roads'):
        graph=data[key]['network']
        for n in graph['nodes']:
            ident=stable(key+'-node',n['id']);node_ids[(key,n['id'])]=ident;nodes.append({'id':ident,'source':key+':'+n['id'],'point':n['position']})
        for e in graph['edges']:
            policy=er.get((key,e['id']));record={'id':stable(key+'-edge',e['id']),'source':key+':'+e['id'],'from':node_ids[(key,e['from'])],'to':node_ids[(key,e['to'])],'points':e['geometry']['coordinates'],'basis':'unreviewed source candidate','qualified':False,'capacity':0}
            if policy:
                cc.require(policy['input_sha256']==bundle['provenance']['inputs'][key]['sha256'],'Stale edge review')
                cc.require(all(type(policy[x]) is bool for x in ('bridge','rail_crossing','junction')),'Non-boolean edge review flag')
                cc.require(type(policy['capacity']) is int and 1<=policy['capacity']<=30000 and type(policy['grade']) is int and -16<=policy['grade']<=16,'Invalid route capacity/grade')
                record.update({x:policy[x] for x in ('bridge','rail_crossing','capacity','grade','junction')});record.update(qualified=True,basis=policy['basis']+': '+policy['note']);seen_e.add((key,e['id']))
            edges.append(record)
    cc.require(seen_e==er.keys(),'Unknown network/edge review target')
    doors=[]
    for d in review['doors']:
        cc.require(set(d)=={'source_id','building_id','network','node_id','input_sha256','reviewed','basis','note','points'},'Unsupported door fields')
        cc.require(d['building_id'] in buildings and (d['network'],d['node_id']) in node_ids,'Unknown door building/node')
        cc.require(d['input_sha256']==bundle['provenance']['inputs']['buildings']['sha256'] and type(d['reviewed']) is bool and d['basis'] in ('reviewed-source','technical-fixture') and d['note'].strip(),'Invalid/stale door review')
        doors.append({'id':stable('door',d['source_id']),'building':buildings[d['building_id']],'node':node_ids[(d['network'],d['node_id'])],'reviewed':d['reviewed'],'basis':d['basis']+': '+d['note'],'points':d['points']})
    sites=[{'id':stable('site',s['id']),'source':s['id'],'point':s.get('position_pixels'),'name':s['name'],'religion':s['religion'],'denomination':s['denomination'],'status':s['worship_status_1940'],'notes':s.get('notes',''),'site_type':s.get('site_type','unknown'),'setting_year':s.get('setting_year',0),'identity_verified':s.get('historical_identity_verified',False),'review_required':s.get('spatial_review_required',True),'confidence_known':'spatial_confidence' in s,'confidence':s.get('spatial_confidence',0),'location_basis':s.get('location_basis','unknown'),'source_record':json.dumps(s,ensure_ascii=False,sort_keys=True,separators=(',',':'))} for s in data['institutions']['sites']]

    return {'width':bundle['canvas']['width'],'height':bundle['canvas']['height'],'generation':bundle['generation_id'],'basis':'audited source candidates; per-feature review required','polygons':polys,'nodes':nodes,'edges':edges,'doors':doors,'sites':sites}

def build(check=False):
    cc.require(OUTPUT.resolve()==ROOT.resolve()/'build/content/CanterburyCity.bin','Runtime output escapes checkout');cc.source_inventory();bundle_path=ROOT/cc.OUTPUT
    if not bundle_path.is_file():cc.compile_to(bundle_path)
    raw=cc.local_file(ROOT,cc.OUTPUT).read_bytes();bundle=cc.parse(raw);evidence=cc.parse((ROOT/'content/cycle-08-validation.json').read_bytes())
    cc.require(evidence['artifact_sha256']==cc.digest(raw),'Runtime source bundle differs from qualified Cycle 08 generation')
    cc.require(bundle['provenance']['source_manifest_sha256']==cc.digest((ROOT/'content/source-manifest.json').read_bytes()),'Stale runtime source manifest')
    cc.require(all(cc.digest(cc.local_file(ROOT,p).read_bytes())==sha for p,sha in bundle['provenance']['owned_inputs'].items()),'Stale runtime compiler inputs')
    policy_raw=cc.local_file(ROOT,'content/spatial-review.json').read_bytes();review=cc.parse(policy_raw);tool=(ROOT/'tools/compile_runtime.py').read_bytes()
    model=compile_model(bundle,review);packed=encode(model)
    cc.require(len(packed)<=256*1024*1024,'Runtime pack exceeds byte limit')
    if check:cc.require(OUTPUT.is_file() and OUTPUT.read_bytes()==packed,'Runtime content missing/stale: run make runtime-content')
    else:
        def fresh():
            cc.require(bundle_path.read_bytes()==raw and REVIEW.read_bytes()==policy_raw and (ROOT/'tools/compile_runtime.py').read_bytes()==tool,'Runtime inputs changed before publication');cc.source_inventory()
        cc.atomic_write(OUTPUT,packed,fresh)
    return {'bytes':len(packed),'sha256':cc.digest(packed),'generation_id':bundle['generation_id'],'buildings':sum(p['kind']==1 for p in model['polygons']),'nodes':len(model['nodes']),'edges':len(model['edges']),'qualified_edges':sum(e['qualified'] for e in model['edges']),'reviewed_doors':sum(d['reviewed'] for d in model['doors'])}
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    try:print(json.dumps(build(a.check),sort_keys=True))
    except (ValueError,KeyError,TypeError,OSError) as e:p.exit(2,'Runtime content error: '+str(e)+'\n')
if __name__=='__main__':main()
