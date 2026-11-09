import copy,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import compile_occupations as c
class OccupationTest(unittest.TestCase):
 def setUp(self):
  self.original=c.ROOT;self.temp=tempfile.TemporaryDirectory();c.ROOT=Path(self.temp.name)
  for p in c.INPUTS:
   target=c.ROOT/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((self.original/p).read_bytes())
  self.policy=json.loads((c.ROOT/'content/occupation-policy.json').read_text())
 def tearDown(self):c.ROOT=self.original;self.temp.cleanup()
 def reject(self,mutate):
  policy=copy.deepcopy(self.policy);mutate(policy);(c.ROOT/'content/occupation-policy.json').write_text(json.dumps(policy))
  with self.assertRaises((ValueError,TypeError)):c.build(verify_sources=False)
 def test_determinism_and_complete_audit(self):
  one=c.build(verify_sources=False);raw=(c.ROOT/'build/content/OccupationCatalogue.inc').read_bytes();two=c.build(verify_sources=False);self.assertEqual(one,two);self.assertEqual(raw,(c.ROOT/'build/content/OccupationCatalogue.inc').read_bytes());audit=json.loads((c.ROOT/'build/content/OccupationCatalogue.json').read_text());self.assertEqual(audit['sdk_vocations_audited'],63);self.assertEqual(sum(r['enabled'] for r in audit['records']),8);self.assertTrue(any('ritual_specialist' in r['source'] and not r['enabled'] for r in audit['records']))
 def test_unknown_source(self):self.reject(lambda p:p['roles'][0].update(sdk_vocation='invented'))
 def test_ritual_role_not_renamed(self):self.reject(lambda p:p['roles'][0].update(sdk_vocation='ritual_specialist_priest_seer'))
 def test_clergy_uses_institution_evidence(self):self.reject(lambda p:p['roles'][6].update(site_ids=[]))
 def test_unknown_site(self):self.reject(lambda p:p['roles'][6].update(site_ids=['invented']))
 def test_underage(self):self.reject(lambda p:p['roles'][0].update(minimum_age=5))
 def test_unqualified(self):self.reject(lambda p:p['roles'][0].update(skills=0))
 def test_missing_basis(self):self.reject(lambda p:p['roles'][0].update(basis='historical fact'))
 def test_duplicate(self):self.reject(lambda p:p['roles'].append(copy.deepcopy(p['roles'][0])))
 def test_stale_output(self):
  c.build(verify_sources=False);(c.ROOT/'build/content/OccupationCatalogue.inc').write_text('corrupt')
  with self.assertRaises(ValueError):c.build(check=True,verify_sources=False)
if __name__=='__main__':unittest.main()
