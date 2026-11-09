"""Standalone json2png2 input bounds, deterministic strokes and final-map equality."""
import copy,importlib.util,json,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('png_reference',ROOT/'canterbury/newcanterbury/test_maps.py');png=importlib.util.module_from_spec(spec);spec.loader.exec_module(png)
class AssembledRenderer(unittest.TestCase):
 def render(self,model,path,ok=True):
  source=path.with_suffix('.json');source.write_text(json.dumps(model));p=subprocess.run([ROOT/'build/map/json2png2',source,path],capture_output=True,text=True)
  self.assertEqual(p.returncode==0,ok,p.stderr)
 def test_final_reference(self):
  self.assertEqual(png.read_png(ROOT/'build/map/CanterburyMap.png'),png.read_png(ROOT/'canterbury/simulation-map/derived/overview.png'))
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
