#!/usr/bin/env python3
"""Check the draft Canterbury language; not an executable scenario compiler."""
import argparse,hashlib,json,math,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'content/scenarios/canterbury1940-draft.canterbury'
KINDS=set('scenario limits policy cohort workforce activity resource service recipe competence qualification career occupation advance'.split())
from canterbury_dsl_parser import DraftError, parse as parse_records

def require(ok,message):
 if not ok:raise DraftError(message)

def parse(text):
 records=parse_records(text)
 for key in records:
  require(key[0] in KINDS,'Unknown declaration '+key[0])
 return records

def validate(records,catalogue=None):
 def group(kind):return {key:v for (k,key),v in records.items() if k==kind}
 def ref(kind,key):require((kind,key) in records,'Unknown '+kind+' reference '+str(key));return records[kind,key]
 scenario=ref('scenario','canterbury_1940');require(scenario['language_version']=='0.1' and scenario['profile']=='CANTERBURY_SIM' and scenario['runtime_activation'] is False,'Not the supported inactive draft profile')
 limits=ref('limits','authoring');roles=group('occupation');families=group('career');resources=group('resource');advances=group('advance')
 for kind,maximum in [('occupation','roles'),('career','families'),('resource','resource_kinds'),('service','services'),('recipe','recipes')]:require(len(group(kind))<=limits[maximum],kind+' budget exceeded')
 require(len(roles)==300 and len(families)==60,'Draft must contain 300 roles in 60 families');require(len({r['title'] for r in roles.values()})==300,'Duplicate role title')
 listed=[]
 for key,f in families.items():
  ref('competence',f['competence']);ref('qualification',f['qualification']);require(len(f['role_ids'])==5,'Expected five candidate stages');listed+=f['role_ids']
  require([ref('occupation',r)['stage'] for r in f['role_ids']]==[1,2,3,4,5],'Career stage ordering mismatch')
  for r in f['role_ids']:require(roles[r]['career']==key,'Career membership mismatch')
 require(len(listed)==len(set(listed))==300 and set(listed)==set(roles),'Career role coverage mismatch')
 for key,r in roles.items():
  ref('career',r['career']);ref('competence',r['competence']);ref('qualification',r['qualification']);require(r['enabled'] is False and r['funded_vacancies']==0,'Draft cannot activate employment')
  require(r['mentor_required']==r['training_post'],'Training must require mentorship')
 links=set()
 for a in advances.values():
  src=ref('occupation',a['from']);dst=ref('occupation',a['to']);require(src['career']==dst['career'] and dst['stage']==src['stage']+1,'Invalid advancement edge');require(a['automatic'] is False,'Automatic promotion forbidden');require({'funded_vacancy','destination_qualification','consent'}<=set(a['requires']),'Missing promotion gates');links.add((a['from'],a['to']))
 require(len(links)==len(advances)==240,'Expected 240 unique advancement links')
 for f in families.values():require(all(pair in links for pair in zip(f['role_ids'],f['role_ids'][1:])),'Missing career transition')
 cohorts=group('cohort');require(sum(c['people'] for c in cohorts.values())==scenario['population']==27000,'Population totals do not reconcile');require(scenario['population_capacity']==30000,'Wrong capacity')
 require(sum(w['target_people'] for w in group('workforce').values())+ref('activity','non_employment')['working_age_not_employed']==next(c['people'] for c in cohorts.values() if c['age_band']=='18-64'),'Workforce total mismatch')
 for key,r in resources.items():require(r['storage']=='finite_accounted_stock' and r['automatic_renewal'] is False and r['unit'] in ['g','ml','each'],'Invalid finite resource '+key)
 for s in group('service').values():require(s['stockpilable'] is False and s['requires_native_action_grant'] is True,'Service is not real work')
 for key,r in group('recipe').items():
  for side in ['input','output']:
   require(len(r[side+'_resources'])==len(r[side+'_quantities'])>0,'Recipe quantity mismatch')
   require(all(type(q) is int and q>0 for q in r[side+'_quantities']),'Recipe quantities must be positive integers')
   for resource in r[side+'_resources']:
    unit=ref('resource',resource)['unit']
    if r['accounting'] in {'water_mass_conserved','water_mixed_material'}:
     require(r['unit']=='g' and (unit=='g' or resource in {'potable_water','process_water','waste_water','beer'}),'Incompatible water recipe units')
    elif r['accounting']=='counted_bill_of_materials':
     require(r['unit']=='each' and (unit=='g' or (side=='output' and unit=='each')),'Incompatible counted recipe units')
    else:
     require(unit==r['unit'],'Incompatible recipe units')
  if r['accounting'] not in {'counted_bill_of_materials','water_mass_conserved'}:
   require(sum(r['input_quantities'])==sum(r['output_quantities']),'Recipe mass not conserved including waste: '+key)
 if catalogue:
  require(set(roles)=={r['id'] for r in catalogue['roles']},'Catalogue ID mismatch')
  for r in catalogue['roles']:require(roles[r['id']]['title']==r['name'],'Catalogue title mismatch')
 return {'families':len(families),'roles':len(roles),'advancement_links':len(links),'resources':len(resources),'services':len(group('service')),'recipes':len(group('recipe')),'runtime_executable':False}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('path',nargs='?',type=Path,default=DEFAULT);a=p.parse_args();raw=a.path.read_bytes();require(len(raw)<=8*1024*1024,'File exceeds draft 8 MiB budget');records=parse(raw.decode('utf8'));source=ROOT/'content/occupation-careers-draft.json';require(records['scenario','canterbury_1940']['source_career_catalogue_sha256']==hashlib.sha256(source.read_bytes()).hexdigest(),'Career source fingerprint changed');print(json.dumps(validate(records,json.loads(source.read_text())),sort_keys=True))
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,TypeError,OSError) as e:raise SystemExit('Draft validation error: '+str(e))
