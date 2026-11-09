#!/usr/bin/env python3
"""Exhaustive street/building clearance audit; normal validation uses stdlib only."""
import argparse,collections,copy,hashlib,importlib.util,json,math,subprocess,sys,html
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from building_placements import corrected_objects,load_corrections,CORRECTIONS
from assembled_map import model,reference_matches
REPORT=ROOT/'content/building-placement-validation.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def load(n):return json.loads((ROOT/'canterbury/simulation-map/derived'/n).read_text())
def pixels(edges,style):
 result=set()
 for e in edges:
  radius=int(style(e)[1]>=1.7)
  for a,b in zip(e['geometry']['coordinates'],e['geometry']['coordinates'][1:]):
   steps=max(1,math.ceil(math.dist(a,b)*2))
   for i in range(steps+1):
    x=math.floor(a[0]+(b[0]-a[0])*i/steps);y=math.floor(a[1]+(b[1]-a[1])*i/steps)
    for dx in range(-radius,radius+1):
     for dy in range(-radius,radius+1):
      if 0<=x+dx<1600 and 0<=y+dy<1600:result.add((y+dy)*1600+x+dx)
 return result
def inputs():
 files=['tools/verify_building_placements.py','tools/building_placements.py','tools/review_building_placements.py','tools/review_building_structures.py','ui/CityMap.m','tests/presentation.m','tools/assembled_map.py','tools/compile_content.py','tools/compile_runtime.py','tools/build_basemap.py','tests/test_assembled_map.py','content/content.schema.json',CORRECTIONS,'content/spatial-review.json','docs/MAP.md','docs/CONTENT.md','build/map/CanterburyMap.png','build/map/CanterburyMapBackground.png','build/map/AssembledMap.json','build/map/buildings-enriched.json','build/content/canterbury.json','build/content/CanterburyCity.bin']
 return {p:sha(ROOT/p) for p in files}
def geometry_check(original,corrected,roads):
 from shapely import Polygon,LineString,STRtree,make_valid,union_all
 def polygon(b):
  result=Polygon()
  for ring in b['geometry']['rings']:result=result.symmetric_difference(make_valid(Polygon(ring)))
  return result
 shapes=[polygon(b) for b in corrected];old=[polygon(b) for b in original];lines=[LineString(e['geometry']['coordinates']) for e in roads];tree=STRtree(lines)
 crossings=[corrected[i]['id'] for i,p in enumerate(shapes) if any(p.intersection(lines[int(j)]).length>1e-7 for j in tree.query(p,predicate='intersects'))]
 assert not crossings,crossings[:10]
 tree=STRtree(shapes);overlaps=[]
 for i,p in enumerate(shapes):
  for j in tree.query(p,predicate='intersects'):
   if int(j)>i and p.intersection(shapes[int(j)]).area>1e-6:overlaps.append([corrected[i]['id'],corrected[int(j)]['id']])
 assert not overlaps,overlaps[:10]
 rail=union_all([LineString(e['geometry']['coordinates']) for e in load('rail.json')['network']['edges']+load('rail-corridors.json')['edges']]);assert all(new.intersection(rail).length<=before.intersection(rail).length+1e-6 for new,before in zip(shapes,old))
 water=union_all([polygon(b) for b in load('waterways.json')['objects']]);assert all(new.intersection(water).area<=before.intersection(water).area+1e-6 for new,before in zip(shapes,old))
 return {'centerline_building_intersections_after':0,'building_overlap_pairs_after':0,'increased_water_overlaps':0,'increased_rail_intersections':0,'all_corrected_shapes_valid':all(p.is_valid for p in shapes)}
def gallery(rows):
 source=(ROOT/'canterbury/simulation-map/derived/overview.png').read_bytes();(ROOT/'build/map/CanterburyMap-before.png').write_bytes(source)
 # Two source-locked views and one selectable entry for every corrected source ID.
 data=json.dumps(rows).replace('</','<\\/')
 text='''<!doctype html><meta charset="utf-8"><title>Canterbury street clearance audit</title><style>body{font:16px system-ui;background:#f6ecd6;color:#222;margin:24px}canvas{width:48%;height:auto;border:1px solid #555}input{width:110px}pre{white-space:pre-wrap}</style><h1>Canterbury street clearance audit</h1><p>Before / after. All corrected cases are listed below. Schematic geometry corrections, not surveyed historical positions.</p><label>Case <input id="index" type="number" min="1" value="1"></label><button onclick="go(-1)">Previous</button><button onclick="go(1)">Next</button><pre id="detail"></pre><canvas id="before" width="800" height="800"></canvas> <canvas id="after" width="800" height="800"></canvas><script>const rows=DATA;const a=new Image(),b=new Image();a.src='CanterburyMap-before.png';b.src='CanterburyMap.png';index.max=rows.length;function go(n){index.value=Math.max(1,Math.min(rows.length,+index.value+n));show()}function show(){let r=rows[+index.value-1];detail.textContent=`${index.value} / ${rows.length} | ${r.source_id}\n${r.method} | offset ${r.offset_pixels.join(', ')} px\n${r.street_names.join('; ')||'Road stroke clearance'}\nOriginal bounds ${r.original_bounds.join(', ')}\nCorrected bounds ${r.corrected_bounds.join(', ')}`;let z=[r.original_bounds,r.corrected_bounds],lo=[Math.min(...z.map(x=>x[0]))-12,Math.min(...z.map(x=>x[1]))-12],hi=[Math.max(...z.map(x=>x[2]))+12,Math.max(...z.map(x=>x[3]))+12],size=Math.max(hi[0]-lo[0],hi[1]-lo[1],50);for(let [id,img,k,col]of[['before',a,0,'#df452a'],['after',b,1,'#087743']]){let c=document.getElementById(id).getContext('2d'),q=z[k];c.imageSmoothingEnabled=false;c.clearRect(0,0,800,800);c.drawImage(img,lo[0],lo[1],size,size,0,0,800,800);c.strokeStyle=col;c.lineWidth=3;c.strokeRect((q[0]-lo[0])*800/size,(q[1]-lo[1])*800/size,(q[2]-q[0])*800/size,(q[3]-q[1])*800/size)}}index.oninput=show;a.onload=show;b.onload=show;</script><h2>All corrections</h2><table><tr><th>Source ID</th><th>Method</th><th>Offset</th><th>Streets</th></tr>TABLE</table>'''
 table=''.join('<tr>'+''.join('<td>'+html.escape(str(v))+'</td>' for v in [r['source_id'],r['method'],r['offset_pixels'],'; '.join(r['street_names'])])+'</tr>' for r in rows)
 (ROOT/'build/map/building-conflicts.html').write_text(text.replace('DATA',data).replace('TABLE',table))
def run(geometry=False):
 sys.path.insert(0,str(ROOT/'canterbury/simulation-map'))
 from assemble import rasterize
 from osm_roads import style
 rail_ink=pixels(load('rail.json')['network']['edges']+load('rail-corridors.json')['edges'],lambda e:('',1))
 source=load('buildings.json')['objects'];corrected=corrected_objects(source);road=load('road-network.json')['network']['edges'];ink=pixels(road,style)
 conflicts_before=[];conflicts_after=[];occupied=set();overlap_pixels=0
 for old,new in zip(source,corrected):
  if rasterize(old['geometry']['rings'],1600,1600)&ink:conflicts_before.append(old['id'])
  filled=rasterize(new['geometry']['rings'],1600,1600)
  assert not ((filled&rail_ink)-(rasterize(old['geometry']['rings'],1600,1600)&rail_ink)), 'New reference-rail obstruction: '+new['id']
  if filled&ink:conflicts_after.append(new['id'])
  overlap_pixels+=len(filled&occupied);occupied.update(filled)
 assert not conflicts_after,conflicts_after[:10];assert overlap_pixels==0,overlap_pixels
 assert [b['id'] for b in source]==[b['id'] for b in corrected]
 ui=json.loads((ROOT/'build/map/buildings-enriched.json').read_text())['objects'];assert [b['geometry']['rings'] for b in corrected]==[b['geometry']['rings'] for b in ui];assert [b['bounds'] for b in corrected]==[b['bounds'] for b in ui]
 rendering=model();assert [b['geometry']['rings'] for b in corrected]==[f['rings'] for f in rendering['features'] if 'rings' in f]
 bundle=json.loads((ROOT/'build/content/canterbury.json').read_text())
 assert [b['geometry']['rings'] for b in corrected]==[b['geometry']['rings'] for b in bundle['data']['buildings']['objects']]
 from compile_runtime import compile_model
 runtime=compile_model(bundle,json.loads((ROOT/'content/spatial-review.json').read_text()));assert [b['geometry']['rings'] for b in corrected]==[p['rings'] for p in runtime['polygons'] if p['kind']==1]
 assert reference_matches(ROOT/'build/map/CanterburyMap.png')
 rows=load_corrections()['corrections'];counts=collections.Counter(r['method'] for r in rows)
 final_bounds={b['id']:b['bounds'] for b in corrected};gallery([{**r,'corrected_bounds':final_bounds[r['source_id']]} for r in rows])
 report={'format':'canterbury-building-placement-validation','version':1,'inputs':inputs(),'source_buildings':len(source),'corrected_buildings':len(rows),'methods':dict(counts),'structure_refinements':dict(collections.Counter(r['method'] for r in load_corrections().get('structure_refinements',[]))),'buildings_with_road_pixels_before':len(conflicts_before),'buildings_with_road_pixels_after':0,'building_overlap_pixels_after':0,'new_rail_obstruction_pixels':0,'renderer_bundle_runtime_geometry_identical':True,'native_selection_bounds_and_outlines_identical':True,'independent_python_png_reference_matches':True,'source_ids_preserved':True,'historical_verification_inferred':False,'all_cases':'build/map/building-conflicts.html','geometry':geometry_check(source,corrected,road) if geometry else None,'ok':True}
 REPORT.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='inputs'}),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--geometry',action='store_true');a=p.parse_args()
 if a.check:
  r=json.loads(REPORT.read_text());assert r['ok'] and r['inputs']==inputs();print('PASS current exhaustive building/street clearance evidence')
 else:run(a.geometry)
