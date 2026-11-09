#!/usr/bin/env python3
"""Audit all local SDK vocations; compile an explicit ordinary-city role catalogue."""
import argparse,hashlib,json,re,os,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
INPUTS=['apesdk/bronze/content/bronze_age_full.bronze','canterbury/simulation-map/derived/religious-sites-1940.json','content/occupation-policy.json','tools/compile_occupations.py']
def build(check=False,verify_sources=True):
    if verify_sources:
        from audit_sources import inventory,MANIFEST
        if inventory()!=json.loads((ROOT/MANIFEST).read_text()):raise ValueError('Stale source inventory')
    if any(not (ROOT/p).resolve().is_relative_to(ROOT.resolve()) for p in INPUTS):raise ValueError('Occupation input escapes checkout')
    raw={p:(ROOT/p).read_bytes() for p in INPUTS}
    names=re.findall(r'^\s*vocation\s+(\w+)\s*\{',raw[INPUTS[0]].decode(),re.M)
    if not names or len(names)!=len(set(names)):raise ValueError('Missing/duplicate SDK vocations')
    policy=json.loads(raw[INPUTS[2]]);sites={s['id']:s for s in json.loads(raw[INPUTS[1]])['sites']}
    if set(policy)!={'format','version','roles'} or policy['format']!='canterbury-details-occupation-policy' or policy['version']!=1:raise ValueError('Invalid occupation policy')
    enabled=[];used=set();mapped=set()
    for i,role in enumerate(policy['roles'],1):
        if set(role)!={'key','name','sdk_vocation','site_ids','basis','output','minimum_age','skills'}:raise ValueError('Unknown role fields')
        if any(not isinstance(role[k],str) or len(role[k])>512 for k in ('key','name','sdk_vocation','basis','output')) or not role['name'] or not isinstance(role['site_ids'],list) or any(not isinstance(x,str) for x in role['site_ids']):raise ValueError('Invalid role text/source fields')
        if role['sdk_vocation'] and role['sdk_vocation'] not in {'farmer_crop_tender','brewer_ale_maker','carpenter','textile_weaver','pottery_maker'}:raise ValueError('SDK specialization has not been qualified as an ordinary city role')
        if not role['sdk_vocation'] and role['key'] not in {'care_worker','clergy','religious_lay_caretaker'}:raise ValueError('Unreviewed technical city role')
        if role['key'] in used or not role['key'] or role['sdk_vocation'] and role['sdk_vocation'] not in names:raise ValueError('Duplicate/unknown vocation')
        if role['key'] in ('clergy','religious_lay_caretaker') and (role['sdk_vocation'] or not role['site_ids']):raise ValueError('Religious city roles must use institution evidence, not relabeled Bronze ritual roles')
        if any(s not in sites for s in role['site_ids']) or not role['basis'].startswith('technical adaptation: ') or not role['output'] or type(role['minimum_age']) is not int or not 18<=role['minimum_age']<=100 or type(role['skills']) is not int or not 1<=role['skills']<=15:raise ValueError('Unsupported role source, basis, age or skill')
        used.add(role['key']);mapped.add(role['sdk_vocation']);enabled.append(dict(role,id=i,enabled=True,source=('SDK vocation '+role['sdk_vocation']) if role['sdk_vocation'] else ('Canterbury institution IDs: '+', '.join(role['site_ids']) if role['site_ids'] else 'Explicit technical role definition; no historical staffing evidence')))
    records=enabled+[dict(id=len(enabled)+i+1,key=n,name=n,enabled=False,source='SDK vocation '+n,basis='Disabled: Bronze-age specialization not qualified for Canterbury 1940',output='none',minimum_age=0,skills=0) for i,n in enumerate(n for n in names if n not in mapped)]
    identity=hashlib.sha256(b''.join(p.encode()+b'\0'+raw[p] for p in INPUTS)).hexdigest()
    audit={'format':'canterbury-details-occupation-catalogue','version':1,'identity':identity,'inputs':{p:hashlib.sha256(b).hexdigest() for p,b in raw.items()},'sdk_vocations_audited':len(names),'records':records,'scope':'role definitions and eligibility only; vacancies, employers, shifts and assignments are Cycle 19'}
    quote=lambda x:json.dumps(x,ensure_ascii=True)
    header='/* Generated from audited local inputs; do not hand edit. */\nstatic const char occupation_identity[]='+quote(identity)+';\nstatic const CdOccupationView occupations[]={\n'+''.join('{'+','.join([str(x['id']),quote(x['name']),quote(x['source']),quote(x['basis']),quote(x['output']),str(x['minimum_age']),str(x['skills']),'1' if x['enabled'] else '0'])+'},\n' for x in records)+'};\n'
    outputs={'build/content/OccupationCatalogue.inc':header.encode(),'build/content/OccupationCatalogue.json':(json.dumps(audit,indent=2)+'\n').encode()}
    for path,data in outputs.items():
        target=ROOT/path
        if target.resolve()!=ROOT.resolve()/path:raise ValueError('Catalogue output escapes checkout')
        if check:
            if not target.is_file() or target.read_bytes()!=data:raise ValueError('Stale occupation catalogue')
        elif not target.is_file() or target.read_bytes()!=data:
            target.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=target.parent,delete=False) as f:f.write(data);temporary=f.name
            try:
                if any((ROOT/p).read_bytes()!=b for p,b in raw.items()):raise ValueError('Occupation source changed before publication')
                os.replace(temporary,target)
            finally:
                if os.path.exists(temporary):os.unlink(temporary)
    return {'identity':identity,'enabled':len(enabled),'disabled':len(records)-len(enabled),'sdk_vocations_audited':len(names)}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    try:print(json.dumps(build(a.check),sort_keys=True))
    except (ValueError,OSError,KeyError,TypeError) as e:p.exit(2,str(e)+'\n')
