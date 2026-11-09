"""Standalone json2png2 input bounds, deterministic strokes and final-map equality."""
import copy,importlib.util,json,subprocess,tempfile,unittest,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('png_reference',ROOT/'canterbury/newcanterbury/test_maps.py');png=importlib.util.module_from_spec(spec);spec.loader.exec_module(png)
class AssembledRenderer(unittest.TestCase):
 def render(self,model,path,ok=True):
  source=path.with_suffix('.json');source.write_text(json.dumps(model));p=subprocess.run([ROOT/'build/map/json2png2',source,path],capture_output=True,text=True)
  self.assertEqual(p.returncode==0,ok,p.stderr)
 def test_final_reference(self):
  sys.path.insert(0,str(ROOT/'tools'))
  from assembled_map import reference_matches
  self.assertTrue(reference_matches(ROOT/'build/map/CanterburyMap.png'))
 def test_native_outline_resource_matches_rendered_geometry(self):
  records=json.loads((ROOT/'build/map/buildings-enriched.json').read_text())['objects']
  render=json.loads((ROOT/'build/map/AssembledMap.json').read_text())
  self.assertEqual([b['geometry']['rings'] for b in records],[f['rings'] for f in render['features'] if 'rings' in f])
  self.assertEqual(sum('placement_correction' in b for b in records),1483)
 def test_placement_source_locks_and_invalid_records(self):
  sys.path.insert(0,str(ROOT/'tools'))
  from building_placements import corrected_objects,load_corrections
  original=json.loads((ROOT/'canterbury/simulation-map/derived/buildings.json').read_text())['objects']
  review=load_corrections()
  for mutate in [lambda r:r.update(version=99),lambda r:r['input_sha256'].update(roads='0'*64),lambda r:r['corrections'].append(copy.deepcopy(r['corrections'][0])),lambda r:r['corrections'][0].update(source_id='missing'),lambda r:r['corrections'][0].update(original_bounds=[0,0,1,1]),lambda r:r['corrections'][0].update(method='unreviewed'),lambda r:r['corrections'][0].update(rings=[[[float('nan'),0],[0,0],[1,1],[float('nan'),0]]])]:
   bad=copy.deepcopy(review);mutate(bad)
   with self.assertRaises(ValueError):corrected_objects(original,bad)
 def test_modern_structure_admission_is_explicit_and_source_locked(self):
  sys.path.insert(0,str(ROOT/'tools'))
  from building_placements import corrected_objects,load_corrections
  original=json.loads((ROOT/'canterbury/simulation-map/derived/buildings.json').read_text())['objects']
  review=load_corrections();accepted=[r for r in review['structure_refinements'] if r['method']=='osm-surviving-structure']
  self.assertEqual({r['osm_ref'] for r in accepted},{'way/123811376'})
  excluded={r['osm_ref'] for r in review['structure_policy']['excluded_examples']}
  self.assertTrue(excluded.isdisjoint({r['osm_ref'] for r in accepted}))
  index=next(i for i,r in enumerate(review['structure_refinements']) if r['method']=='osm-surviving-structure')
  for mutation in [lambda d:d['structure_sources'].update({'canterbury/simulation-map/derived/modern-buildings-projected.json':'0'*64}),lambda d:d['structure_refinements'][index].update(osm_ref='way/28015428'),lambda d:d['structure_refinements'][index]['rings'][0][1].__setitem__(0,1),lambda d:d['structure_refinements'].append(copy.deepcopy(d['structure_refinements'][index]))]:
   bad=copy.deepcopy(review);mutation(bad)
   with self.assertRaises(ValueError):corrected_objects(original,bad)
 def test_building_free_native_background(self):
  sys.path.insert(0,str(ROOT/'tools'))
  from assembled_map import model
  full=model();background=model(False)
  self.assertFalse(any('rings' in f for f in background['features']))
  self.assertEqual(background['features'],[f for f in full['features'] if 'rings' not in f])
 def test_strokes_and_bounds(self):
  model={'format':'canterbury-assembled-render','version':1,'width':8,'height':8,'palette':[[246,236,214],[68,68,68]],'features':[{'coordinates':[[-1,2],[5,2]],'color':1,'radius':1}]}
  with tempfile.TemporaryDirectory(prefix='json2png2-') as directory:
   out=Path(directory)/'first.png';self.render(model,out);w,h,pixels=png.read_png(out);self.assertEqual((w,h),(8,8))
   for y in range(8):
    for x in range(8):self.assertEqual(pixels[3*(y*w+x):3*(y*w+x)+3],bytes(model['palette'][int(y in (1,2,3) and x<=6)]))
   second=Path(directory)/'second.png';self.render(model,second);self.assertEqual(out.read_bytes(),second.read_bytes())
   for change in [lambda d:d.update(format='canterbury-vectors'),lambda d:d.update(width=0),lambda d:d.update(height=4097),lambda d:d['features'][0].update(radius=2),lambda d:d['features'][0].update(color=9),lambda d:d['features'][0].update(coordinates=[[0,1],[9000,1]])]:
    bad=copy.deepcopy(model);change(bad);self.render(bad,Path(directory)/'bad.png',False)
if __name__=='__main__':unittest.main()
