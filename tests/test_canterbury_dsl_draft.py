import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from check_canterbury_dsl_draft import ROOT,DEFAULT,parse,validate,DraftError
class CanterburyDraft(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.records=parse(DEFAULT.read_text());cls.catalogue=json.loads((ROOT/'content/occupation-careers-draft.json').read_text())
 def test_complete_catalogue(self):
  out=validate(self.records,self.catalogue);self.assertEqual(out['roles'],300);self.assertEqual(out['advancement_links'],240);self.assertEqual(out['resources'],72);self.assertFalse(out['runtime_executable'])
 def test_syntax_rejects_duplicate_fields_and_records(self):
  for text in ['policy x { a 1; a 2; }','policy x {} policy x {}','policy x { a "missing terminator" }','policy x { a NaN; }']:
   with self.assertRaises(DraftError):parse(text)
 def test_comments_and_quoted_punctuation(self):
  self.assertEqual(parse('# comment\npolicy x { text "a;#b{}"; list [1, "two", false]; }')['policy','x']['text'],'a;#b{}')
 def test_broken_links_and_automatic_promotions_rejected(self):
  for field,value in [('to','absent.2'),('automatic',True)]:
   r=copy.deepcopy(self.records);r['advance','grocer_1_to_2'][field]=value
   with self.assertRaises(DraftError):validate(r)
 def test_no_silent_activation(self):
  r=copy.deepcopy(self.records);r['occupation','grocer.1']['enabled']=True
  with self.assertRaises(DraftError):validate(r)
 def test_resource_imbalance_and_wrong_units_rejected(self):
  r=copy.deepcopy(self.records);r['recipe','mill_wheat']['output_quantities'][0]+=1
  with self.assertRaises(DraftError):validate(r)
  r=copy.deepcopy(self.records);r['resource','wheat_grain']['unit']='ml'
  with self.assertRaises(DraftError):validate(r)
 def test_population_and_catalogue_drift_rejected(self):
  r=copy.deepcopy(self.records);r['cohort','age_group_1']['people']+=1
  with self.assertRaises(DraftError):validate(r)
  r=copy.deepcopy(self.records);r['occupation','grocer.1']['title']='Renamed without review'
  with self.assertRaises(DraftError):validate(r,self.catalogue)
if __name__=='__main__':unittest.main()
