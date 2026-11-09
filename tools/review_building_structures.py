"""Review-only conservative structure refiner; requires Shapely and writes an unadmitted candidate."""
import sys,json,math,hashlib
from pathlib import Path
from shapely import Polygon,LineString,union_all,make_valid,STRtree
from shapely.affinity import translate
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'tools'));sys.path.insert(0,str(R/'canterbury/simulation-map'))
from building_placements import corrected_objects
from osm_roads import style
D=R/'canterbury/simulation-map/derived';load=lambda n:json.loads((D/n).read_text())
def shape(rs):
 p=Polygon()
 for r in rs:p=p.symmetric_difference(make_valid(Polygon(r)))
 return p
def rings(p):
 out=[]
 for g in ([p] if p.geom_type=='Polygon' else p.geoms):
  if g.geom_type!='Polygon':continue
  out.append([list(x) for x in g.exterior.coords]);out.extend([[list(x) for x in r.coords] for r in g.interiors])
 return out
path=R/'content/building-placement-corrections.json';doc=json.loads(path.read_text());doc.pop('structure_refinements',None);doc.pop('structure_sources',None)
bs=corrected_objects(load('buildings.json')['objects'],doc);ps=[shape(x['geometry']['rings']) for x in bs];tree=STRtree(ps)
road=union_all([LineString(e['geometry']['coordinates']).buffer((1 if style(e)[1]>=1.7 else 0)+math.sqrt(2)+.5,quad_segs=2) for e in load('road-network.json')['network']['edges']]);rail=union_all([LineString(e['geometry']['coordinates']).buffer(math.sqrt(2)+.5,quad_segs=2) for e in load('rail.json')['network']['edges']+load('rail-corridors.json')['edges']]);water=union_all([shape(x['geometry']['rings']) for x in load('waterways.json')['objects']])
def safe(q,i):
 p=ps[i]
 if q.is_empty or not q.is_valid or not (0<=q.bounds[0]<=q.bounds[2]<=1600 and 0<=q.bounds[1]<=q.bounds[3]<=1600):return False
 if q.intersection(road).area>1e-9 or q.intersection(rail).difference(p).area>1e-9 or q.intersection(water).difference(p).area>1e-9:return False
 return all(int(j)==i or q.intersection(ps[int(j)]).area<1e-9 for j in tree.query(q,predicate='intersects'))
modern={x['osm_ref']:x for x in load('modern-buildings-projected.json')['objects']};refinements=[]
reviewed=[('buildings-905b3d026b5c5a4cb40c','way/123811376','Hothe Court','https://historicengland.org.uk/listing/the-list/list-entry/1336585')]
for bid,ref,name,url in reviewed:
 i=next(i for i,x in enumerate(bs) if x['id']==bid);q=shape(modern[ref]['rings']);found=None
 for dx,dy in sorted(((x,y) for x in range(-12,13) for y in range(-12,13)),key=lambda x:(x[0]**2+x[1]**2,abs(x[0])+abs(x[1]),x[1],x[0])):
  moved=translate(q,dx,dy)
  if safe(moved,i):found=(moved,dx,dy);break
 if found:
  p,dx,dy=found;refinements.append({'source_id':bid,'method':'osm-surviving-structure','osm_ref':ref,'name':name,'evidence_url':url,'basis':'Named Hothe Court footprint overlaps the historical source with the same modern identity; Historic England describes its surviving C16-or-earlier T-shaped house and older wings. Use that principal house outline alone; import no surrounding modern structures. Registration and local street-clearance offset remain schematic, not surveyed historical coordinates.','offset_pixels':[dx,dy],'rings':[[[x+dx,y+dy] for x,y in ring] for ring in modern[ref]['rings']],'bounds':list(p.bounds)})
  ps[i]=p;tree=STRtree(ps)
 else:print('NO SAFE PLACEMENT',name)
for i,p in enumerate(ps):
 if any(r['source_id']==bs[i]['id'] for r in refinements):continue
 for tolerance in (.7,.55):
  q=p.simplify(tolerance,preserve_topology=True)
  if q.equals_exact(p,1e-9) or abs(q.area-p.area)>max(.6,p.area*.12):continue
  if safe(q,i):
   refinements.append({'source_id':bs[i]['id'],'method':'historical-contour-cleanup','tolerance_pixels':tolerance,'rings':rings(q),'bounds':list(q.bounds)});ps[i]=q;tree=STRtree(ps);break
files=['modern-buildings-projected.json','religious-sites-1940.json'];doc['structure_sources']={str((D/n).relative_to(R)):hashlib.sha256((D/n).read_bytes()).hexdigest() for n in files};f=R/'canterbury/simulation-map/modern/osm-canterbury.json';doc['structure_sources'][str(f.relative_to(R))]=hashlib.sha256(f.read_bytes()).hexdigest()
doc['structure_policy']={'modern_geometry':'Explicitly reviewed surviving structure with historical footprint overlap and documentary support only; no proximity-only or automatic nearest-building substitution. Import no modern surroundings.','excluded_examples':[{'osm_ref':'way/150281463','reason':'Old Sessions House has a large 1990s extension; retain the historical outline. Council planning agenda 2 April 2024, paragraph 22.','evidence_url':'https://www.canterbury.gov.uk/sites/default/files/2024-04/Planning%20Committee%20agenda%202%20April%202024.pdf'},{'osm_ref':'way/191475362','reason':'Nearest historical source candidate is labelled St Martin’s Priory; church-to-footprint identity is not established.'},{'osm_ref':'way/152634959','reason':'Nearest historical source candidate is labelled Church Hall; church-to-footprint identity is not established.'},{'osm_ref':'way/28015428','reason':'Castle Street multi-storey car park: modern replacement is not the historical structure despite strong footprint overlap.'},{'osm_ref':'way/150515691','reason':'start_date 2015-09'},{'osm_ref':'way/766424699','reason':'start_date 2015-09'},{'osm_ref':'way/150466030','reason':'St George tower cannot substitute for the complete church present in 1940 and destroyed in 1942.'}],'unknown_identity':'Retain historical structure; geometric overlap alone is insufficient.','historical_cleanup':'Topology-preserving simplification at at most 0.7 source pixels; area change at most max(0.6 pixel squared,12 percent). Reject any new street, building, water or rail obstruction.','modern_alignment':'Local translation only, no modern-footprint resize, rotation or street clipping; retain explicit schematic offset.'}
doc['structure_refinements']=sorted(refinements,key=lambda r:r['source_id']);(R/'build/map/candidate-building-structures.json').write_text(json.dumps(doc,indent=2)+'\n');print('refinements',len(refinements),'OSM',[{k:v for k,v in x.items() if k not in ('rings','bounds')} for x in refinements if x['method'].startswith('osm')],flush=True)
