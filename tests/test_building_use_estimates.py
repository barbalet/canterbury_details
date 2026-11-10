import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from estimate_building_uses import estimate,ROOT

def row(key,x,use='unknown'):
 return {'id':key,'bounds':[x,0,x+1,1],'use':use}
class BuildingUseEstimates(unittest.TestCase):
 def setUp(self):self.policy=json.loads((ROOT/'content/building-use-estimate-policy.json').read_text())
 def test_preserves_source_and_only_estimates_unknown(self):
  rows=[row('a',0,'industrial'),row('b',2)];before=copy.deepcopy(rows);out=estimate(rows,self.policy)
  self.assertEqual(rows,before);self.assertEqual([x['building_id'] for x in out],['b']);self.assertEqual(out[0]['estimated_use'],'workshop');self.assertFalse(out[0]['operational_assignment'])
 def test_guesses_never_seed_other_guesses(self):
  out=estimate([row('anchor',0,'commercial'),row('near',100),row('distant',200)],self.policy)
  distant=next(x for x in out if x['building_id']=='distant');self.assertEqual(distant['anchors'],[]);self.assertEqual(distant['confidence'],'very_low')
 def test_input_order_does_not_change_ties(self):
  rows=[row('a',0,'industrial'),row('b',2,'residential'),row('c',1),row('d',3)]
  self.assertEqual(estimate(rows,self.policy),estimate(list(reversed(rows)),self.policy))
 def test_sparse_and_conflicting_evidence_is_low_confidence(self):
  out=estimate([row('a',0,'commercial'),row('b',0,'residential'),row('c',0)],self.policy)[0]
  self.assertEqual(out['confidence'],'low');self.assertGreater(len(out['alternatives']),1)
 def test_no_anchor_is_explicit_fallback_not_occupation(self):
  out=estimate([row('a',0)],self.policy)[0];self.assertEqual(out['estimated_use'],'residential');self.assertEqual(out['suggested_workplace_roles'],[]);self.assertEqual(out['support_share'],0)
 def test_bad_geometry_and_duplicate_identity_rejected(self):
  for rows in [[row('a',0),row('a',1)],[row('a',float('nan'))]]:
   with self.assertRaises(ValueError):estimate(rows,self.policy)
 def test_real_sources_preserve_all_unknown_ids_and_bound_evidence(self):
  rows=json.loads((ROOT/'canterbury/simulation-map/derived/buildings-enriched.json').read_text())['objects'];out=estimate(rows,self.policy)
  self.assertEqual({x['building_id'] for x in out},{x['id'] for x in rows if x['use']=='unknown'});self.assertEqual(len(out),1790)
  known={x['id'] for x in rows if x['use']!='unknown'}
  for x in out:
   self.assertTrue(x['not_historical_fact'] and x['requires_review']);self.assertFalse(x['operational_assignment']);self.assertLessEqual(len(x['anchors']),8)
   self.assertTrue(all(a['building_id'] in known and a['distance_pixels']<=120 for a in x['anchors']))
class AppliedBuildingUses(unittest.TestCase):
 def test_runtime_applies_only_unknowns_without_creating_access(self):
  from compile_runtime import compile_model, encode
  bundle=json.loads((ROOT/'build/content/canterbury.json').read_text());review=json.loads((ROOT/'content/spatial-review.json').read_text());guesses=json.loads((ROOT/'content/building-use-estimates.json').read_text())
  baseline=compile_model(bundle,review);applied=compile_model(bundle,review,guesses)
  old={p['source']:p for p in baseline['polygons']};changed=0
  for p in applied['polygons']:
   before=old[p['source']]
   for key in set(before)-{'use','policy_basis'}:self.assertEqual(p[key],before[key])
   if before['kind']==1 and before['use']=='unknown':
    self.assertNotEqual(p['use'],'unknown');self.assertIn('provisional',p['policy_basis']);changed+=1
   else:self.assertEqual(p,before)
  self.assertEqual(changed,1790);self.assertNotEqual(baseline['generation'],applied['generation']);self.assertEqual(baseline['edges'],applied['edges']);self.assertEqual(baseline['doors'],applied['doors']);self.assertEqual(encode(applied),encode(compile_model(bundle,review,guesses)))
 def test_age_and_workforce_totals_reconcile(self):
  p=json.loads((ROOT/'content/population-allocation-proposal.json').read_text())
  self.assertEqual(sum(r['people'] for r in p['age_cohorts']),27000)
  self.assertEqual(sum(r['people'] for r in p['working_age_employment']),12000)
  self.assertEqual(12000+4470,16470);self.assertEqual(p['age_cohorts'][-1]['people'],2700)
if __name__=='__main__':unittest.main()
