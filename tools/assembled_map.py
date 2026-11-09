"""Convert final assembled geometry to the json2png core's explicit draw order."""
import importlib.util,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
INPUTS=['canterbury/simulation-map/derived/'+name+'.json' for name in ('buildings','waterways','rail','rail-corridors','road-network')]+['canterbury/simulation-map/osm_roads.py','canterbury/simulation-map/complete_roads.py','canterbury/simulation-map/derived/overview.png','canterbury/newcanterbury/test_maps.py']
def model():
 load=lambda name:json.loads((ROOT/'canterbury/simulation-map/derived'/name).read_text())
 buildings=load('buildings.json');water=load('waterways.json');rail=load('rail.json');corridors=load('rail-corridors.json');roads=load('road-network.json')
 spec=importlib.util.spec_from_file_location('cd_osm_style',ROOT/'canterbury/simulation-map/osm_roads.py');module=importlib.util.module_from_spec(spec)
 # Upstream styling imports its sibling geometry helpers, all from the pinned module.
 import sys
 directory=str(ROOT/'canterbury/simulation-map');sys.path.insert(0,directory)
 try:spec.loader.exec_module(module)
 finally:sys.path.remove(directory)
 colors=['f6ecd6','444444','2879b0','8e55a3','101010','a79278','aa7661','ad6250','b85542','bd382c']
 features=[{'rings':b['geometry']['rings'],'color':1} for b in buildings['objects']]
 for layer,index in ((water,2),(rail,3)):
  features.extend({'coordinates':e['geometry']['coordinates'],'color':index,'radius':0} for e in layer['network']['edges'])
 features.extend({'coordinates':e['geometry']['coordinates'],'color':4,'radius':0} for e in corridors['edges'])
 for e in sorted(roads['network']['edges'],key=module.draw_order):
  color,width=module.style(e);features.append({'coordinates':e['geometry']['coordinates'],'color':colors.index(color[1:]),'radius':int(width>=1.7)})
 return {'format':'canterbury-assembled-render','version':1,'width':buildings['width'],'height':buildings['height'],'palette':[[int(c[i:i+2],16) for i in (0,2,4)] for c in colors],'features':features,'source':'Final assembled overview; cropped OSM road topology; no simulation qualification inferred'}

def reference_matches(path):
 spec=importlib.util.spec_from_file_location('cd_png_reference',ROOT/'canterbury/newcanterbury/test_maps.py')
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 return module.read_png(Path(path))==module.read_png(ROOT/'canterbury/simulation-map/derived/overview.png')

if __name__=='__main__':
 import argparse
 from native_profile import preflight
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',required=True,type=Path);args=parser.parse_args();preflight()
 args.write.parent.mkdir(parents=True,exist_ok=True);args.write.write_text(json.dumps(model(),separators=(',',':'))+'\n')
 print(json.dumps({'assembled_json':str(args.write)}))
