#!/usr/bin/env python3
"""Offline review-only placement solver. Requires Shapely; writes a candidate under build/map, never edits admitted sources."""
import sys,json,math,time,hashlib,collections
from pathlib import Path
from shapely import Polygon,LineString,STRtree,make_valid,union_all,box,prepare
from shapely.affinity import translate
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'canterbury/simulation-map'));from osm_roads import style
D=ROOT/'canterbury/simulation-map/derived'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def polygon(b):
 g=Polygon()
 for ring in b['geometry']['rings']:g=g.symmetric_difference(make_valid(Polygon(ring)))
 return g
b=json.loads((D/'buildings.json').read_text());roads=json.loads((D/'road-network.json').read_text());edges=roads['network']['edges'];lines=[LineString(e['geometry']['coordinates']) for e in edges];tree=STRtree(lines)
# Cover the actual floor-sampled square raster stroke as well as its centreline.
radii=[(1 if style(e)[1]>=1.7 else 0)+math.sqrt(2)+.5 for e in edges]
corridors=union_all([line.buffer(radius,quad_segs=2) for line,radius in zip(lines,radii)]);prepare(corridors)
water=json.loads((D/'waterways.json').read_text());waters=union_all([polygon(x) for x in water['objects']]);prepare(waters)
polys=[polygon(x) for x in b['objects']];affected=[i for i,p in enumerate(polys) if p.intersects(corridors)];print('Road/stroke clearance conflicts',len(affected),flush=True)
# Dynamic grid avoids overlaps with fixed buildings and previously relocated ones.
grid=collections.defaultdict(set);placed={};cell=20
def cells(p):
 x0,y0,x1,y1=p.bounds
 for x in range(math.floor(x0/cell),math.floor(x1/cell)+1):
  for y in range(math.floor(y0/cell),math.floor(y1/cell)+1):yield x,y
def add(i,p):
 placed[i]=p
 for c in cells(p):grid[c].add(i)
def overlaps(p,own):
 ids=set()
 for c in cells(p):ids.update(grid.get(c,()))
 return any(p.intersects(placed[j]) and p.intersection(placed[j]).area>1e-8 for j in ids if j!=own)
for i,p in enumerate(polys):add(i,p)
# Deterministic minimum integer translation by squared displacement.
offsets=sorted(((x,y) for x in range(-24,25) for y in range(-24,25) if x or y),key=lambda p:(p[0]*p[0]+p[1]*p[1],abs(p[0])+abs(p[1]),p[1],p[0]))
rows=[];start=time.time()
for n,i in enumerate(sorted(affected,key=lambda i:(-polys[i].area,b['objects'][i]['id']))):
 p=polys[i];found=None
 for dx,dy in offsets:
  moved=translate(p,dx,dy);x0,y0,x1,y1=moved.bounds
  if x0<0 or y0<0 or x1>1600 or y1>1600 or moved.intersects(corridors):continue
  if moved.intersects(waters) or overlaps(moved,i):continue
  found=(dx,dy,moved);break
 if found is None:
  moved=p.difference(corridors)
  if moved.is_empty:raise RuntimeError('Empty street-sided footprint '+b['objects'][i]['id'])
  found=(0,0,moved)
 dx,dy,moved=found
 for c in cells(placed[i]):grid[c].discard(i)
 add(i,moved)
 hits=[int(j) for j in tree.query(p,predicate='intersects') if p.intersection(lines[int(j)]).length>1e-7]
 rings=[]
 def collect(g):
  if g.geom_type=='Polygon':
   rings.append([[round(x,6),round(y,6)] for x,y in g.exterior.coords]);rings.extend([[[round(x,6),round(y,6)] for x,y in r.coords] for r in g.interiors])
  elif hasattr(g,'geoms'):
   for part in g.geoms:collect(part)
 collect(moved)
 rows.append({'method':'translate' if dx or dy else 'separate-merged-footprint','rings':rings,'source_id':b['objects'][i]['id'],'offset_pixels':[dx,dy],'original_bounds':b['objects'][i]['bounds'],'corrected_bounds':list(moved.bounds),'intersected_street_ids':[edges[j]['id'] for j in sorted(hits)],'street_names':sorted(set(edges[j].get('name','unknown') for j in hits))})
 if n%50==0:print(n+1,'/',len(affected),'max',round(max(math.hypot(*r['offset_pixels']) for r in rows),2),'seconds',round(time.time()-start),flush=True)
# Prevent a street correction from introducing a reference-rail obstruction.
rail=json.loads((D/'rail.json').read_text());rail_links=json.loads((D/'rail-corridors.json').read_text())
rail_center=union_all([LineString(e['geometry']['coordinates']) for e in rail['network']['edges']+rail_links['edges']])
rail_space=rail_center.buffer(math.sqrt(2)+.5,quad_segs=2);prepare(rail_space)
refined=0
rail_offsets=sorted(((x,y) for x in range(-64,65) for y in range(-64,65) if x or y),key=lambda p:(p[0]*p[0]+p[1]*p[1],abs(p[0])+abs(p[1]),p[1],p[0]))
by_id={record['id']:i for i,record in enumerate(b['objects'])}
for row in sorted(rows,key=lambda r:r['source_id']):
 i=by_id[row['source_id']];current=placed[i];original=polys[i]
 if row['method']!='translate' or not current.intersects(rail_space):continue
 found=None
 for dx,dy in rail_offsets:
  moved=translate(original,dx,dy);x0,y0,x1,y1=moved.bounds
  if x0<0 or y0<0 or x1>1600 or y1>1600:continue
  if moved.intersects(corridors) or moved.intersects(rail_space) or moved.intersects(waters) or overlaps(moved,i):continue
  found=(dx,dy,moved);break
 if found is None:raise RuntimeError('No safe rail-clear placement '+row['source_id'])
 dx,dy,moved=found
 for c in cells(current):grid[c].discard(i)
 add(i,moved);row['offset_pixels']=[dx,dy];row['corrected_bounds']=list(moved.bounds)
 row['rings']=[[[x+dx,y+dy] for x,y in ring] for ring in b['objects'][i]['geometry']['rings']]
 row['rail_clearance_refined']=True;refined+=1
print('Rail-safe refinements',refined,flush=True)
report={'format':'canterbury-building-placement-corrections','version':1,'basis':'User-requested schematic street-clearance correction: translate individual source footprints locally; separate merged block footprints along street corridors. Not surveyed historical locations.','input_sha256':{'buildings':sha(D/'buildings.json'),'roads':sha(D/'road-network.json'),'waterways':sha(D/'waterways.json'),'rail':sha(D/'rail.json'),'rail_corridors':sha(D/'rail-corridors.json')},'clearance_policy':{'stroke_radius':'upstream radius 0/1 plus sqrt(2) floor-sampling allowance and 0.5 pixel margin','new_building_overlap_allowed':False,'new_water_overlap_allowed':False,'increased_rail_overlap_allowed':False,'intact_footprints_resize_or_rotate':False,'merged_footprints':'Separate only oversized merged source footprints that have no nearby intact placement; retain original source IDs and provenance.'},'buildings_total':len(polys),'centerline_conflicts_before':sum(bool(tree.query(p,predicate='intersects').size) for p in polys),'corrections':sorted(rows,key=lambda r:r['source_id'])}
(ROOT/'build/map/candidate-building-placement-corrections.json').write_text(json.dumps(report,indent=2)+'\n');print('SOLVED',len(rows),'maxshift',max(math.hypot(*r['offset_pixels']) for r in rows),flush=True)
