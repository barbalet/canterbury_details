#!/usr/bin/env python3
"""Compile audited Canterbury candidates offline into one atomic, deterministic bundle."""
import argparse,hashlib,json,math,os,re,subprocess,tempfile
from pathlib import Path
from audit_sources import ROOT,MANIFEST,inventory

FILES={
 'vectors':'newcanterbury/canterbury.json','annotations':'newcanterbury/annotations.json',
 'buildings':'simulation-map/derived/buildings.json','buildings_modern':'simulation-map/derived/buildings-enriched.json',
 'buildings_1940':'simulation-map/derived/buildings-1940.json','road_ink':'simulation-map/derived/roads.json',
 'modern_roads':'simulation-map/derived/road-network.json','historical_roads':'simulation-map/derived/road-network-historical-traces.json',
 'waterways':'simulation-map/derived/waterways.json','rail_ink':'simulation-map/derived/rail.json',
 'rail_corridors':'simulation-map/derived/rail-corridors.json','institutions':'simulation-map/derived/religious-sites-1940.json',
 'institution_policy':'simulation-map/religious-sites-1940.json','review_candidates':'simulation-map/derived/review-queue.json',
 'assembly_report':'simulation-map/derived/report.json','enrichment_report':'simulation-map/derived/building-enrichment-report.json',
 'connectivity_report':'simulation-map/derived/road-connectivity-report.json','road_traces':'simulation-map/road-traces.json',
 'modern_sources':'simulation-map/modern/sources.json','controls':'simulation-map/modern/control-points.json',
 'osm':'simulation-map/modern/osm-canterbury.json','osm_roads':'simulation-map/modern/osm-roads.json',
 'assembler':'simulation-map/assemble.py','enricher':'simulation-map/enrich_buildings.py',
 'road_builder':'simulation-map/osm_roads.py','tracer':'simulation-map/complete_roads.py','institution_builder':'simulation-map/religion_1940.py'}
LOCAL=['tools/compile_content.py','tools/audit_sources.py','content/content-policy.json','content/review-annotations.json','content/content.schema.json','content/cycle-01-validation.json','content/source-manifest.json',*[f'docs/evidence/cycle01-{i}.log' for i in range(1,4)]]
OUTPUT=Path('build/content/canterbury.json')
class ContentError(ValueError):pass
def require(condition,message):
    if not condition:raise ContentError(message)
def digest(data):return hashlib.sha256(data).hexdigest()
def canonical(value):return (json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False,allow_nan=False)+'\n').encode()
def pairs(items):
    result={}
    for k,v in items:
        require(k not in result,'Duplicate JSON key: '+k);result[k]=v
    return result
def finite(value):
    pending=[value]
    while pending:
        item=pending.pop()
        if type(item) is int:require(abs(item)<=2**63-1,'Integer outside signed 64-bit range')
        elif isinstance(item,float):require(math.isfinite(item),'Non-finite numeric value')
        elif isinstance(item,dict):pending.extend(item.values())
        elif isinstance(item,list):pending.extend(item)
def parse(raw):
    try:value=json.loads(raw,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ContentError('Non-finite JSON: '+x)))
    except (UnicodeError,json.JSONDecodeError,RecursionError) as e:raise ContentError('Invalid JSON: '+str(e)) from e
    finite(value);require(isinstance(value,dict),'JSON root must be an object');return value
def schema_check(value,schema,path='$'):
    """Local schema subset used by the checked-in envelope schema; no URI reads."""
    supported={'$schema','$id','title','description','type','const','enum','pattern','minimum','required','properties','additionalProperties','items'}
    require(not set(schema)-supported,'Unsupported schema keyword at '+path)
    if 'const' in schema:require(canonical(value)==canonical(schema['const']),'Schema const: '+path)
    if 'enum' in schema:require(value in schema['enum'],'Schema enum: '+path)
    kind=schema.get('type')
    types={'object':dict,'array':list,'string':str,'integer':int,'boolean':bool}
    if kind:require(kind in types and type(value) is types[kind],'Schema type: '+path)
    if 'pattern' in schema:require(isinstance(value,str) and re.fullmatch(schema['pattern'],value) is not None,'Schema pattern: '+path)
    if 'minimum' in schema:require(value>=schema['minimum'],'Schema minimum: '+path)
    if isinstance(value,dict):
        require(set(schema.get('required',[]))<=value.keys(),'Schema missing field: '+path)
        properties=schema.get('properties',{});extra=schema.get('additionalProperties',True)
        for key,item in value.items():
            if key in properties:schema_check(item,properties[key],path+'.'+key)
            elif isinstance(extra,dict):schema_check(item,extra,path+'.'+key)
            else:require(extra is True,'Schema extra field: '+path+'.'+key)
    elif isinstance(value,list) and 'items' in schema:
        for item in value:schema_check(item,schema['items'],path+'[]')

def local_file(root,path):
    p=root/path;require(p.resolve().is_relative_to(root.resolve()) and p.is_file(),'Missing/external input: '+str(path));return p

def source_inventory():
    for name in ('canterbury','apesdk'):
        folder=ROOT/name
        require(folder.is_dir() and folder.resolve().is_relative_to(ROOT.resolve()),'Missing/external submodule: '+name)
    sizes=[local_file(ROOT,'canterbury/'+path).stat().st_size for path in FILES.values()]
    require(all(size<=64*1024*1024 for size in sizes) and sum(sizes)<=256*1024*1024,'Input size budget exceeded before auditing')
    for path in [*LOCAL,'LICENSE']:
        local_file(ROOT,path)
    if (ROOT/'content/sdk-working-tree.json').exists():local_file(ROOT,'content/sdk-working-tree.json')
    return inventory()

def read_inputs():
    manifest_raw=local_file(ROOT,MANIFEST).read_bytes();manifest=parse(manifest_raw);require(source_inventory()==manifest,'Stale audited source inventory; review upstream changes first')
    expected={f['path']:f['sha256'] for f in manifest['tracked_files']}
    documents={};inputs={};total=0
    for key,path in FILES.items():
        relative='canterbury/'+path;raw=local_file(ROOT,relative).read_bytes();total+=len(raw)
        require(len(raw)<=64*1024*1024 and total<=256*1024*1024,'Input size budget exceeded')
        require(digest(raw)==expected.get(relative),'Stale input: '+relative)
        inputs[key]={'path':relative,'sha256':digest(raw),'bytes':len(raw)}
        if path.endswith('.json'):documents[key]=parse(raw)
    owned={p:local_file(ROOT,p).read_bytes() for p in LOCAL}
    require(owned['content/source-manifest.json']==manifest_raw,'Source manifest changed while reading inputs')
    evidence=parse(owned['content/cycle-01-validation.json'])
    require(evidence.get('ok') is True and evidence.get('source_manifest_sha256')==digest((ROOT/MANIFEST).read_bytes()),'Stale Cycle 01 source validation')
    commands=[['python3','simulation-map/validate.py'],['python3','simulation-map/validate_buildings.py'],['python3','-m','unittest','discover','-s','simulation-map','-v']]
    require(len(evidence.get('runs',[]))==3,'Missing upstream validation evidence')
    for i,(run,command) in enumerate(zip(evidence['runs'],commands),1):
        path=f'docs/evidence/cycle01-{i}.log'
        require(run.get('command')==command and run.get('exit_code')==0 and run.get('log')==path and run.get('log_sha256')==digest(owned[path]),'Invalid upstream validation evidence')
    return documents,inputs,owned,manifest

def indexed(items,label):
    require(isinstance(items,list),label+': expected array');result={}
    for item in items:
        require(isinstance(item,dict),label+': expected record');key=item.get('id')
        require(type(key) in (int,str) and key!='' and key not in result,label+': duplicate/invalid ID');result[key]=item
    return result

def point(p,width,height):
    require(isinstance(p,list) and len(p)==2,'Expected 2D coordinate')
    require(all(type(v) in (int,float) and math.isfinite(v) for v in p),'Invalid coordinate')
    require(0<=p[0]<=width and 0<=p[1]<=height,'Coordinate outside canvas')
def rings(geometry,width,height):
    require(geometry.get('encoding')=='closed_pixel_boundary_rings' and geometry.get('fill_rule')=='even-odd','Unsupported polygon encoding/fill rule')
    rr=geometry.get('rings');require(isinstance(rr,list) and rr,'Missing polygon rings')
    for ring in rr:
        require(isinstance(ring,list) and len(ring)>=4 and ring[0]==ring[-1],'Open/short polygon ring')
        for p in ring:point(p,width,height)
def network(graph,width,height,objects=None,routes=None):
    nodes=indexed(graph.get('nodes'),'network nodes');edges=indexed(graph.get('edges'),'network edges')
    require(all(isinstance(k,str) for k in [*nodes,*edges]),'Network IDs must be strings')
    for n in nodes.values():
        point(n.get('position'),width,height)
        if objects is not None:require(n.get('object_id') in objects,'Missing node object reference')
    for e in edges.values():
        require(e.get('from') in nodes and e.get('to') in nodes,'Missing edge endpoint')
        require(e.get('traversable') is False,'Unapproved traversable edge')
        require(e.get('status') in ('unreviewed','osm_source_topology','source_guided_trace','reference_corridor_requires_review'),'Unknown edge status')
        geometry=e.get('geometry',{});coords=geometry.get('coordinates')
        require(geometry.get('type')=='LineString' and isinstance(coords,list) and len(coords)>=2,'Invalid edge geometry')
        for p in coords:point(p,width,height)
        require(math.dist(coords[0],nodes[e['from']]['position'])<=1e-5 and math.dist(coords[-1],nodes[e['to']]['position'])<=1e-5,'Edge endpoint geometry differs')
        length=e.get('length_pixels');require(type(length) in (int,float) and math.isfinite(length) and length>=0,'Invalid length')
        require(abs(sum(math.dist(a,b) for a,b in zip(coords,coords[1:]))-length)<=max(.001,len(coords)*.000001),'Stale edge length')
        if objects is not None:require(e.get('object_id') in objects and nodes[e['from']]['object_id']==nodes[e['to']]['object_id']==e['object_id'],'Missing/cross-object edge reference')
        if routes is not None:require(e.get('route_ids') and set(e['route_ids'])<=routes,'Missing route reference')
    return nodes,edges

def validate(documents,inputs,policy,review):
    d=documents;w,h=policy['canvas']['width'],policy['canvas']['height']
    require(type(w) is int and type(h) is int and 0<w<=16384 and 0<h<=16384,'Invalid canvas dimensions')
    require(set(policy)=={'format','version','setting_year','canvas','units','routing_enabled','formats'},'Unknown content policy field')
    require(set(policy['canvas'])=={'width','height','origin','y_axis'} and policy['canvas']['origin']=='top-left' and policy['canvas']['y_axis']=='down','Unsupported canvas axes')
    require(type(policy.get('version')) is int and policy.get('format')=='canterbury-details-content-policy' and policy.get('version')==1 and policy.get('routing_enabled') is False,'Unsupported content policy')
    require(policy.get('units')=='pixels' and policy.get('setting_year')==1940,'Unsupported setting or units')
    for key,spec in policy['formats'].items():
        require(set(spec)=={'format','version'},'Unsupported format policy entry')
        require(key in d and d[key].get('format')==spec['format'] and type(d[key].get('version')) is int and d[key]['version']==spec['version'],'Unsupported input format/version: '+key)
    require(set(policy['formats'])=={k for k in d if 'format' in d[k]},'Input format policy is incomplete')
    for key in ('vectors','annotations','buildings','buildings_modern','buildings_1940','road_ink','modern_roads','historical_roads','waterways','rail_ink','road_traces'):
        require(d[key].get('width')==w and d[key].get('height')==h,'Dimension mismatch: '+key)
        if key not in ('vectors','annotations','road_traces'):require(d[key].get('units')=='pixels','Unexpected units: '+key)
    features=indexed(d['vectors']['features'],'source features')
    for f in features.values():
        require(type(f['id']) is int and f['id']>0 and isinstance(f.get('class'),str),'Invalid source feature')
        if 'confidence' in f:require(type(f['confidence']) in (int,float) and 0<=f['confidence']<=1,'Invalid source confidence')
        rings({'encoding':'closed_pixel_boundary_rings','fill_rule':'even-odd','rings':f.get('rings')},w,h)
    groups={};graphs={};global_ids=set()
    for key,kind in [('buildings','buildings'),('road_ink','roads'),('waterways','waterways'),('rail_ink','rail')]:
        layer=d[key];require(layer.get('layer')==kind,'Wrong layer class');objects=indexed(layer['objects'],key);groups[key]=objects
        for obj in objects.values():
            require(isinstance(obj['id'],str) and obj['id'] not in global_ids,'Colliding candidate ID');global_ids.add(obj['id'])
            require(obj.get('status')=='unreviewed' and type(obj.get('network_included')) is bool,'Invalid candidate input flags')
            require(isinstance(obj.get('review_flags'),list) and all(isinstance(x,str) for x in obj['review_flags']),'Invalid review flags')
            refs=obj.get('source_feature_ids');require(isinstance(refs,list) and refs and len(set(refs))==len(refs),'Missing/duplicate source references')
            require(all(x in features and features[x]['class']==kind for x in refs),'Missing/wrong-class source feature')
            rings(obj['geometry'],w,h);bounds=obj.get('bounds');require(isinstance(bounds,list) and len(bounds)==4,'Invalid bounds')
            point(bounds[:2],w,h);point(bounds[2:],w,h);require(bounds[0]<=bounds[2] and bounds[1]<=bounds[3],'Inverted bounds')
            require(all(bounds[0]<=p[0]<=bounds[2] and bounds[1]<=p[1]<=bounds[3] for r in obj['geometry']['rings'] for p in r),'Geometry outside object bounds')
            require(type(obj.get('area_pixels')) is int and obj['area_pixels']>0,'Invalid candidate area')
        if 'network' in layer:graphs[key]=network(layer['network'],w,h,objects)
    base=groups['buildings'];modern=indexed(d['buildings_modern']['objects'],'modern buildings');period=indexed(d['buildings_1940']['objects'],'institution buildings')
    require(base.keys()==modern.keys()==period.keys(),'Building enrichment ID coverage changed')
    osm=set()
    for o in d['osm']['elements']:
        require(o.get('type') in ('node','way','relation') and type(o.get('id')) is int and o['id']>0,'Invalid OSM identity')
        ref=f"{o['type']}/{o['id']}";require(ref not in osm,'Duplicate OSM identity');osm.add(ref)
        if o['type']=='node':require(-180<=o['lon']<=180 and -90<=o['lat']<=90,'Invalid OSM lon/lat')
    road_nodes=indexed(d['osm_roads']['nodes'],'OSM road nodes');road_ways=indexed(d['osm_roads']['ways'],'OSM road ways')
    for way in road_ways.values():require(len(way['node_ids'])>=2 and set(way['node_ids'])<=road_nodes.keys(),'Missing OSM road node reference')
    for label in d['vectors'].get('labels',[]):point([label['x'],label['y']],w,h)
    for control in d['controls']['points']:require(control['osm_ref'] in osm,'Missing registration OSM reference');point(control['pixel'],w,h)
    for key in base:
        for enriched in (modern[key],period[key]):require(all(enriched.get(k)==v for k,v in base[key].items()),'Enrichment changed original field: '+key)
        require(modern[key].get('modern_match',{}).get('historical_identity_verified') is False and period[key].get('religious_identity_verified') is False,'Unsupported verified building identity')
        match=modern[key]['modern_match'];require(match.get('method')=='unmatched' or match.get('osm_ref') in osm,'Missing modern OSM match')
        for c in modern[key].get('modern_candidates',[]):require(c.get('osm_ref') in osm,'Missing OSM candidate')
    routes=indexed(d['historical_roads']['routes'],'traced routes')
    for r in routes.values():
        require(r.get('traversable') is False and r.get('status')=='source_guided_trace','Unapproved route flags')
        require(r.get('geometry',{}).get('type')=='LineString','Unsupported route geometry')
        for p in r['geometry']['coordinates']:point(p,w,h)
    graphs['historical_roads']=network(d['historical_roads']['network'],w,h,routes=set(routes))
    graphs['modern_roads']=network(d['modern_roads']['network'],w,h)
    for edge in graphs['modern_roads'][1].values():require(edge.get('osm_way_id') in road_ways,'Missing OSM way reference')
    graphs['rail_corridors']=network(d['rail_corridors'],w,h)
    sites=indexed(d['institutions']['sites'],'institutions');annotations=indexed(d['institution_policy']['sites'],'institution annotations')
    require(sites.keys()==annotations.keys() and d['institutions'].get('setting_year')==d['institution_policy'].get('setting_year')==1940,'Institution coverage/year mismatch')
    for site in sites.values():
        require(all(site.get(k)==v for k,v in annotations[site['id']].items()),'Institution source annotation changed')
        require(site.get('historical_identity_verified') is False,'Invalid institution identity flag')
        if site.get('position_pixels') is not None:
            require(site.get('spatial_review_required') is True,'Missing institution spatial review flag');point(site['position_pixels'],w,h)
        else:require(site.get('location_basis')=='unlocated_historical_site' and not site.get('candidate_buildings'),'Unlocated site received spatial identity')
        for candidate in site.get('candidate_buildings',[]):require(candidate.get('id') in base and candidate.get('identity_verified') is False,'Invalid institution building reference')
    for b in period.values():require(set(b.get('religious_site_candidate_ids',[]))<=sites.keys(),'Missing building institution reference')
    candidates=indexed(d['review_candidates']['candidates'],'upstream review')
    for c in candidates.values():
        layer={'roads':'road_ink','rail':'rail_ink','waterways':'waterways'}.get(c.get('layer'))
        require(c.get('status')=='unreviewed' and layer in graphs,'Invalid review candidate flags/layer')
        nodes,edges=graphs[layer]
        if c['kind']=='possible_fragment_gap':
            require(set(c['node_ids'])<=nodes.keys() and set(c['object_ids'])<=groups[layer].keys(),'Dangling gap review reference')
        elif c['kind']=='reference_rail_crossing':require(c['edge_id'] in edges and c['corridor_id'] in graphs['rail_corridors'][1],'Dangling crossing review reference')
        else:raise ContentError('Unknown review candidate kind')
    # Exact upstream lineage, never regenerated from network services.
    def linked(key,field,target):require(d[key].get(field)==inputs[target]['sha256'],'Stale upstream lineage: '+key+'.'+field)
    for key in ('buildings','buildings_modern','buildings_1940','road_ink','modern_roads','historical_roads','waterways','rail_ink'):linked(key,'source_sha256','vectors')
    for field,target in [('historical_buildings_sha256','buildings'),('osm_sha256','osm'),('control_points_sha256','controls'),('generator_sha256','enricher')]:linked('buildings_modern',field,target)
    for field,target in [('osm_roads_sha256','osm_roads'),('osm_sha256','osm'),('control_points_sha256','controls'),('generator_sha256','road_builder')]:linked('modern_roads',field,target)
    linked('osm_roads','sources_sha256','modern_sources')
    require(d['osm_roads']['source_tile_hashes']==[t['sha256'] for t in d['modern_sources']['tiles']],'Stale OSM tile provenance')
    linked('historical_roads','traces_sha256','road_traces');linked('rail_corridors','annotations_sha256','annotations');linked('modern_sources','osm_sha256','osm')
    for field,target in [('annotations','institution_policy'),('osm','osm'),('controls','controls'),('buildings','buildings'),('generator','institution_builder')]:require(d['institutions']['input_sha256'].get(field)==inputs[target]['sha256'],'Stale institution lineage: '+field)
    linked('assembly_report','generator_sha256','assembler');linked('assembly_report','road_builder_sha256','road_builder')
    for key,target in [('vectors','vectors'),('annotations','annotations'),('osm_roads','osm_roads'),('control_points','controls')]:require(d['assembly_report']['inputs'][key]['sha256']==inputs[target]['sha256'],'Stale assembly lineage: '+key)
    for key in ('modern_roads','buildings_modern','institutions','modern_sources','osm_roads'):
        require(d[key].get('license')=='ODbL-1.0' and d[key].get('attribution')=='© OpenStreetMap contributors','Missing OSM license/attribution')
    require(review.get('format')=='canterbury-details-review-annotations' and review.get('version')==1 and review.get('vectors_sha256')==inputs['vectors']['sha256'],'Stale review annotation basis')
    targets={'buildings':base,'institutions':sites,'review_candidates':candidates,**{k:e for k,(_,e) in graphs.items()}}
    seen=set()
    for note in review.get('annotations',[]):
        require(set(note)=={'dataset','record_id','source_sha256','decision','note'},'Unsupported review annotation fields')
        pair=(note['dataset'],note['record_id']);require(pair not in seen,'Duplicate project review');seen.add(pair)
        require(note['dataset'] in targets and note['record_id'] in targets[note['dataset']],'Unknown project review target')
        require(note['source_sha256']==inputs[note['dataset']]['sha256'],'Stale project review target')
        require(note['decision'] in ('needs_review','retain_candidate','exclude_from_simulation') and isinstance(note['note'],str) and note['note'].strip(),'Unsupported/empty project review')
    return groups,graphs,sites

def compile_bytes(expected_owned=None,expected_sources=None):
    d,inputs,owned,manifest=read_inputs()
    if expected_owned is not None:require({p:digest(raw) for p,raw in owned.items()}==expected_owned,'Captured owned inputs differ from initial compilation lock')
    if expected_sources is not None:require(manifest==expected_sources,'Captured source inventory differs from initial compilation lock')
    policy=parse(owned['content/content-policy.json']);review=parse(owned['content/review-annotations.json'])
    groups,graphs,sites=validate(d,inputs,policy,review)
    modern=indexed(d['buildings_modern']['objects'],'modern');period=indexed(d['buildings_1940']['objects'],'period')
    buildings=[]
    for b in d['buildings']['objects']:
        buildings.append({**b,'modern_annotation':{k:v for k,v in modern[b['id']].items() if k not in b},'institution_annotation':{k:v for k,v in period[b['id']].items() if k not in b}})
    data={k:d[k] for k in ('vectors','annotations','road_ink','modern_roads','historical_roads','waterways','rail_ink','rail_corridors','institutions','institution_policy','review_candidates')}
    data['buildings']={'metadata':{k:v for k,v in d['buildings'].items() if k!='objects'},'objects':buildings,'annotation_metadata':{k:{a:v for a,v in d[k].items() if a!='objects'} for k in ('buildings_modern','buildings_1940')}}
    data['context']={k:d[k] for k in ('assembly_report','enrichment_report','connectivity_report','road_traces','modern_sources','controls')}
    result={'format':'canterbury-details-content','version':1,'setting':{'year':1940,'initial_people':27000,'population_basis':'user-required simulation total; not a verified census'},'canvas':policy['canvas'],'units':'pixels','admission':{'candidate_only':True,'routing_enabled':False,'historical_identity_inferred':False},'provenance':{'source_manifest_sha256':digest(owned['content/source-manifest.json']),'revisions':{m['path']:m['revision'] for m in manifest['submodules']},'inputs':inputs,'owned_inputs':{p:digest(raw) for p,raw in owned.items()}},'data':data,'review_annotations':review,'statistics':{'buildings':len(buildings),'institutions':len(sites),'unlocated_institutions':sum(s.get('position_pixels') is None for s in sites.values()),'networks':{k:{'nodes':len(n),'edges':len(e)} for k,(n,e) in graphs.items()}}}
    result['generation_id']=digest(canonical(result))
    schema_check(result,parse(owned['content/content.schema.json']))
    require(source_inventory()==manifest,'Source inventory changed during compilation')
    for p,raw in owned.items():require(local_file(ROOT,p).read_bytes()==raw,'Owned compiler input changed during compilation: '+p)
    return canonical(result)
def output_path(value):
    p=(ROOT/value).resolve();require(p.is_relative_to((ROOT/'build/content').resolve()) and (ROOT/'build/content').resolve()==ROOT.resolve()/'build/content' and p.suffix=='.json','Output must be a JSON file inside this checkout build/content');return p
def atomic_write(path,raw,before_replace=None):
    path.parent.mkdir(parents=True,exist_ok=True);temporary=None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent,prefix='.content-',delete=False) as f:
            temporary=Path(f.name);f.write(raw);f.flush();os.fsync(f.fileno())
        if before_replace:before_replace()
        os.replace(temporary,path);temporary=None
    finally:
        if temporary is not None:temporary.unlink(missing_ok=True)
def compile_to(path,check=False):
    source_lock=source_inventory();owned_lock={p:digest(local_file(ROOT,p).read_bytes()) for p in LOCAL}
    def fresh():
        require(source_inventory()==source_lock,'Source changed before publication')
        require(all(digest(local_file(ROOT,p).read_bytes())==sha for p,sha in owned_lock.items()),'Compiler inputs changed before publication')
    raw=compile_bytes(owned_lock,source_lock)
    if check:
        fresh();require(path.is_file() and path.read_bytes()==raw,'Compiled generation is missing, stale or corrupt; run make content')
    else:atomic_write(path,raw,fresh)
    return {'generation_sha256':digest(raw),'bytes':len(raw)}
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');p.add_argument('--output',type=Path,default=OUTPUT);a=p.parse_args()
    try:print(json.dumps(compile_to(output_path(a.output),a.check),sort_keys=True))
    except (ValueError,OSError,KeyError,TypeError,AttributeError,subprocess.CalledProcessError) as e:p.exit(2,'Content error: '+str(e)+'\n')
if __name__=='__main__':main()
