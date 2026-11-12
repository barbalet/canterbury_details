"""Inventory compiled roles and mark the evidence available for each handler."""
import json
import subprocess

from compile_canterbury_dsl import ROOT, SOURCE, parse


PUBLIC = {
    'medicine': 'clinical_care', 'nursing': 'clinical_care',
    'social_care': 'household_care', 'school_teaching': 'education',
    'technical_instruction': 'education', 'nursery_education': 'education',
    'office_administration': 'administration', 'municipal_administration': 'administration',
    'fire_service': 'fire_response', 'police_service': 'police_response',
    'charitable_service': 'charitable_support',
    'midwifery': 'midwifery_care', 'pharmacy': 'clinical_care',
    'library_service': 'library_access', 'waterworks': 'water_supply',
    'domestic_service': 'cleaning',
}
TRANSPORT = {
    'railway_operations': 'passenger_transport', 'bus_operations': 'passenger_transport',
    'goods_carrier': 'goods_delivery', 'road_haulage': 'goods_delivery',
    'postal_service': 'postal_delivery',
}
MINISTRY = {
    'anglican_ministry', 'roman_catholic_ministry', 'jewish_ministry', 'religious_lay_service',
}
RETAIL = {'grocer', 'draper', 'ironmonger', 'bookseller', 'coal_merchant'}


# These checks exercise service-specific outcomes on the funded map.
FUNDED_SERVICE_TESTS = {
    'retail_sale': 'retail',
    'food_preparation': 'recipes',
    'goods_delivery': 'transport_stages',
    'education': 'scheduled_native_day',
    'apprentice_instruction': 'district',
    'administration': 'public_administration',
    'police_response': 'police_response',
    'laundry': 'recipes',
    'postal_delivery': 'transport_stages',
    'passenger_transport': 'transport_stages',
    'building_repair': 'repair_and_worship',
    'clinical_care': 'clinical_care',
    'midwifery_care': 'maternal_and_domestic',
    'household_care': 'community_services',
    'fire_response': 'community_services',
    'water_supply': 'water_supply',
    'cleaning': 'maternal_and_domestic',
    'library_access': 'library_access',
    'community_gathering': 'repair_and_worship',
    'charitable_support': 'community_services',
    'religious_service': 'repair_and_worship',
}


FUNDED_PUBLIC_ROLES = {
    'office_administration.2': 'public_administration',
    'police_service.2': 'police_response',
    'nursing.2': 'clinical_care',
    'midwifery.2': 'maternal_and_domestic',
    'pharmacy.2': 'maternal_and_domestic',
    'domestic_service.2': 'maternal_and_domestic',
    'social_care.2': 'community_services',
    'charitable_service.2': 'community_services',
    'fire_service.2': 'community_services',
    'library_service.2': 'library_access',
    'waterworks.2': 'water_supply',
    'carpenter.2': 'repair_and_worship',
    'anglican_ministry.3': 'repair_and_worship',
}


EXERCISED_SERVICE_ROLES = {
    'building_repair': {'carpenter.2'},
    'apprentice_instruction': {'miller.1', 'miller.3'},
    'community_gathering': {'anglican_ministry.3'},
}


def trace(records, binary):
    roles = []
    for (kind, name), draft in records.items():
        if kind != 'occupation':
            continue
        role = json.loads(subprocess.check_output([binary, '--dsl-definition', name], text=True))
        family, stage = role['career'], role['stage']
        assert (family, stage) == (draft['career'], draft['stage'])
        handler, service, funded_test = None, None, None
        if family in TRANSPORT:
            handler, service, funded_test = 'transport', TRANSPORT[family], 'transport_stages'
        elif family in RETAIL:
            handler, service, funded_test = 'retail', 'retail_sale', 'retail'
        elif family in PUBLIC and stage >= 2:
            handler, service = 'public_roster', PUBLIC[family]
        elif family in MINISTRY - {'religious_lay_service'} and stage == 2:
            handler = 'institutional_education' if family == 'jewish_ministry' else 'pastoral_visit'
            service = 'education' if family == 'jewish_ministry' else 'religious_service'
        elif family in MINISTRY and stage >= (2 if family == 'religious_lay_service' else 3):
            handler = 'religious_office'
            service = 'religious_service' if family != 'religious_lay_service' else None
        elif role['technical_recipe'] and stage in (2, 3):
            handler = role['technical_recipe']
            service = {'cook_meal': 'food_preparation', 'wash_linen': 'laundry'}.get(handler)
        if handler is None and role['training_post']:
            handler = 'supervised_training'
        if handler is None and stage >= 4 and role['technical_recipe']:
            handler = 'production_authority'
        funded_test = FUNDED_PUBLIC_ROLES.get(name, funded_test)
        roles.append(dict(role=name, occupation_id=role['occupation_id'], career=family,
                          stage=stage, qualification=role['qualification'],
                          handler=handler, service=service, funded_test=funded_test,
                          technical_recipe=role['technical_recipe']))
    assert len(roles) == len({r['occupation_id'] for r in roles}) == 300
    return roles


if __name__ == '__main__':
    records = parse(SOURCE.read_text(), str(SOURCE))
    roles = trace(records, ROOT / 'build/CanterburyDetails')
    report = json.loads((ROOT / 'build/access-trial-validation.json').read_text())
    for check in ('senior_public_roles', 'institutional_support_roles'):
        evidence = report[check]
        assert evidence['ok']
        verified = set(evidence['roles'])
        assert verified <= {role['role'] for role in roles}
        for role in roles:
            if role['role'] in verified:
                role['funded_test'] = check
    for role in roles:
        role['funded_passed'] = report.get(role['funded_test'], {}).get('ok', False)
    declared = {name for kind, name in records if kind == 'service'}
    assert declared == FUNDED_SERVICE_TESTS.keys() and len(declared) == 21
    assert all(role['handler'] for role in roles)
    services = {
        name: dict(roles=[role['role'] for role in roles if role['service'] == name or
                          role['role'] in EXERCISED_SERVICE_ROLES.get(name, set())],
                   funded_test=FUNDED_SERVICE_TESTS[name],
                   funded_passed=report['dsl_service_coverage'][name]['passed'])
        for name in sorted(declared)
    }
    print(json.dumps(dict(roles=roles, services=services), indent=2))
