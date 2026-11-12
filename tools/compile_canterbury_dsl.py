#!/usr/bin/env python3
"""Compile Canterbury authoring records into a deterministic scenario pack."""
import argparse
from datetime import datetime
import hashlib
import json
import struct
import zlib
from pathlib import Path

from canterbury_dsl_parser import DraftError, MAX_BYTES, parse
from check_canterbury_dsl_draft import validate as validate_draft
from compile_content import atomic_write, canonical

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'content/scenarios/canterbury1940-draft.canterbury'
SCHEMA = ROOT / 'content/canterbury-dsl-schema.json'
CATALOGUE = ROOT / 'content/occupation-careers-draft.json'
CONTROLS = ROOT / 'content/operation-controls.json'
CITY = ROOT / 'build/content/CanterburyCity.bin'
PACK = ROOT / 'build/content/CanterburyScenario.json'
BUDGETS = {'career': 128, 'occupation': 512, 'resource': 128, 'service': 128,
           'recipe': 512}
# Keep this initial binding limited to the existing typed work operation.
# Actor, job and premises are resolved from the live assignment at dispatch.
REFINED_RECIPES = {'cut_timber', 'cut_cloth', 'mill_wheat', 'cut_paper',
                   'form_metalwork', 'trial_trim_leather', 'trial_portion_meat',
                   'trial_portion_fish'}
WORK_RECIPES = ('meal', 'pot', 'textile', 'tool', 'service', 'development')
TYPES = {'string': (str,), 'integer': (int,), 'boolean': (bool,),
         'number': (int, float)}


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def map_premises(raw):
    """Read building source IDs from the existing CDCT0002 polygon prefix."""
    if len(raw) < 20 or raw[:8] != b'CDCT0002':
        raise DraftError('invalid Canterbury runtime map header')
    size, checksum = struct.unpack_from('<QI', raw, 8)
    payload = memoryview(raw)[20:]
    if size != len(payload) or zlib.crc32(payload) != checksum:
        raise DraftError('runtime map length or checksum mismatch')
    offset = 0

    def take(fmt):
        nonlocal offset
        values = struct.unpack_from(fmt, payload, offset)
        offset += struct.calcsize(fmt)
        return values

    def skip(size):
        nonlocal offset
        offset += size
        if offset > len(payload):
            raise DraftError('truncated runtime map polygon section')

    def string():
        length, = take('<I')
        start = offset
        skip(length + 1)
        if length > 4096 or payload[offset - 1] != 0:
            raise DraftError('invalid runtime map string')
        return bytes(payload[start:offset - 1]).decode('utf-8')

    try:
        take('<II')
        string()
        string()
        polygons, *_ = take('<7I')
        buildings = set()
        for _ in range(polygons):
            _, kind, *fields = take('<Q8I')
            source = string()
            for _ in range(3):
                string()
            skip(8)
            for _ in range(fields[-1]):
                count, = take('<I')
                skip(count * 16)
            if kind == 1:
                buildings.add(source)
    except struct.error as error:
        raise DraftError('truncated runtime map polygon section') from error
    return buildings


def matches(value, spec):
    if spec.endswith('?'):
        return value is None or matches(value, spec[:-1])
    if spec.endswith('[]'):
        return isinstance(value, list) and all(matches(item, spec[:-2]) for item in value)
    return type(value) in TYPES[spec]


def validate_fields(records, schema):
    declarations = schema['declarations'] | schema['executable_declarations']
    for key, fields in records.items():
        spec = declarations.get(key[0])
        if spec is None:
            raise records.error(key, f'unknown declaration {key[0]}')
        if key[0] == 'policy':
            # Policy blocks have distinct field sets. The versioned schema
            # assigns them by policy name.
            spec = schema['policies'].get(key[1])
            if spec is None:
                raise records.error(key, 'unknown policy')
        for field in sorted(fields.keys() - spec.keys()):
            raise records.error(key, f'unknown field {field}', field)
        for field in sorted(spec.keys() - fields.keys()):
            raise records.error(key, f'missing field {field}; use unset for an unconfigured nullable field')
        for field, value in fields.items():
            if not matches(value, spec[field]):
                raise records.error(key, f'{field} requires {spec[field]}', field)


def compile_records(records, schema, catalogue, catalogue_hash, controls, map_hash, premises=None):
    validate_fields(records, schema)
    groups = {}
    for (kind, name), fields in records.items():
        groups.setdefault(kind, {})[name] = fields

    def group(kind):
        return groups.get(kind, {})

    def ensure(ok, key, message, field=None):
        if not ok:
            raise records.error(key, message, field)

    def reference(owner, field, kind, name):
        ensure(name in group(kind), owner, f'unknown {kind} reference {name!r}', field)
        return group(kind)[name]

    ensure(len(group('scenario')) == 1, ('scenario', ''), 'expected exactly one scenario')
    scenario_name, scenario = next(iter(group('scenario').items()))
    root = ('scenario', scenario_name)
    version = scenario['language_version']
    ensure(version in (schema['migration_from'], schema['version']), root,
           'unsupported language version', 'language_version')
    ensure(scenario['profile'] == 'CANTERBURY_SIM', root, 'wrong profile', 'profile')
    ensure(scenario['clock'] == 'native_minute', root, 'native minute clock required', 'clock')
    ensure(0 <= scenario['population'] <= scenario['population_capacity'] <= 30000,
           root, 'invalid population or capacity', 'population')
    ensure(0 <= scenario['seed'] < 2**32, root, 'seed must fit uint32', 'seed')
    try:
        origin = datetime.strptime(scenario['civil_origin'], '%Y-%m-%dT%H:%M')
    except ValueError as error:
        raise records.error(root, 'invalid civil origin', 'civil_origin') from error
    ensure(origin.year == 1940, root, 'scenario must begin in 1940', 'civil_origin')
    ensure(scenario['source_map'] == 'CanterburyCity.bin', root,
           'unsupported source map', 'source_map')
    for kind, maximum in BUDGETS.items():
        ensure(len(group(kind)) <= maximum, root, f'{kind} limit {maximum} exceeded')
    if group('limits'):
        ensure(set(group('limits')) == {'authoring'}, root, 'expected limits authoring')
        authored = group('limits')['authoring']
        budgets = {'families': 128, 'roles': 512, 'resource_kinds': 128,
                   'services': 128, 'recipes': 512, 'file_bytes': MAX_BYTES,
                   'tasks_per_role': 256, 'rules_per_role': 256,
                   'expanded_task_statements': 4096, 'nesting_depth': 64}
        for field, ceiling in budgets.items():
            ensure(1 <= authored[field] <= ceiling, ('limits', 'authoring'),
                   f'{field} must be between 1 and {ceiling}', field)
        for kind, field in [('career', 'families'), ('occupation', 'roles'),
                            ('resource', 'resource_kinds'), ('service', 'services'),
                            ('recipe', 'recipes')]:
            ensure(len(group(kind)) <= authored[field], root, f'{kind} exceeds authored budget')


    if version == schema['migration_from']:
        ensure(scenario['source_career_catalogue_sha256'] == catalogue_hash,
               root, 'draft career catalogue fingerprint changed')
        try:
            validate_draft(records, catalogue)
        except DraftError as error:
            raise records.error(root, str(error)) from error

    blockers = []
    if not scenario['runtime_activation']:
        blockers.append('scenario.runtime_activation is false')
    if not map_hash:
        blockers.append('compile against a CanterburyCity.bin runtime map')

    for kind, fields in [('cohort', ['people']), ('workforce', ['target_people']),
                         ('occupation', ['funded_vacancies']), ('stock', ['quantity']),
                         ('employer', ['work_capacity', 'funding_pence']),
                         ('post', ['vacancies', 'wage_pence'])]:
        for name, record in group(kind).items():
            for field in fields:
                ensure(record[field] >= 0, (kind, name), 'quantity must be nonnegative', field)
    if group('cohort'):
        ensure(sum(r['people'] for r in group('cohort').values()) == scenario['population'],
               root, 'cohort population does not reconcile')

    for name, career in group('career').items():
        key = ('career', name)
        for kind in ('competence', 'qualification'):
            reference(key, kind, kind, career[kind])
        ensure(len(set(career['role_ids'])) == len(career['role_ids']), key,
               'duplicate career role', 'role_ids')
        for role in career['role_ids']:
            occupation = reference(key, 'role_ids', 'occupation', role)
            ensure(occupation['career'] == name, key, 'role belongs to another career', 'role_ids')

    for name, role in group('occupation').items():
        key = ('occupation', name)
        career = reference(key, 'career', 'career', role['career'])
        ensure(name in career['role_ids'], key, 'career does not list role', 'career')
        ensure(role['stage'] > 0, key, 'stage must be positive', 'stage')
        for kind in ('competence', 'qualification'):
            reference(key, kind, kind, role[kind])
        for field, maximum in [('wage_pence_per_shift', 2**63 - 1),
                               ('shift_start_minute', 1439),
                               ('shift_duration_minutes', 1440)]:
            value = role[field]
            ensure(value is None or (int(field == 'shift_duration_minutes') <= value <= maximum),
                   key, f'{field} out of range', field)
        if role['task_program'] is not None:
            reference(key, 'task_program', 'program', role['task_program'])
        stage = role['mentor_minimum_stage']
        ensure(not role['training_post'] or (role['mentor_required'] and stage is not None and stage > role['stage']),
               key, 'training post requires a mentor above the trainee stage', 'mentor_minimum_stage')
        if role['training_post']:
            ensure(any(group('occupation')[other]['stage'] >= stage
                       for other in career['role_ids']), key,
                   'career has no role at the required mentor stage')
        if role['enabled']:
            for field in ('wage_pence_per_shift', 'shift_start_minute',
                          'shift_duration_minutes', 'task_program'):
                if role[field] is None:
                    blockers.append(f'occupation {name}: configure {field}')
            qualification = group('qualification')[role['qualification']]
            if qualification['review_required'] and not any(
                record['qualification'] == role['qualification']
                for record in group('credential').values()
            ):
                blockers.append(f'occupation {name}: define credential assessment')
            if role['training_post'] and not any(
                record['career'] == role['career'] for record in group('curriculum').values()
            ):
                blockers.append(f'occupation {name}: define curriculum and mentor capacity')

    stock_locations = set()
    for name, stock in group('stock').items():
        location = stock['premises'], stock['resource']
        ensure(location not in stock_locations, ('stock', name),
               'duplicate initial stock for premises and resource')
        stock_locations.add(location)

    edges = set()
    for name, advance in group('advance').items():
        key = ('advance', name)
        left = reference(key, 'from', 'occupation', advance['from'])
        right = reference(key, 'to', 'occupation', advance['to'])
        edge = advance['from'], advance['to']
        ensure(edge not in edges, key, 'duplicate transition')
        edges.add(edge)
        ensure(left['career'] == right['career'] and left['stage'] < right['stage'],
               key, 'advancement must increase stage within the same career')
        ensure(not advance['automatic'] and
               {'consent', 'funded_vacancy', 'destination_qualification'} <= set(advance['requires']),
               key, 'advancement requires consent, funded vacancy and qualification')
        minutes = advance['minimum_supervised_minutes']
        ensure(minutes is None or minutes > 0, key, 'supervised minutes must be positive')
        if left['enabled'] and right['enabled'] and minutes is None:
            blockers.append(f'advance {name}: configure supervised minutes')

    for name, resource in group('resource').items():
        key = ('resource', name)
        ensure(resource['unit'] in {'g', 'ml', 'each'} and resource['initial_stock'] >= 0,
               key, 'invalid stock units or quantity')
        ensure(not resource['automatic_renewal'], key, 'stocks require accounted replenishment')
    for name, recipe in group('recipe').items():
        key = ('recipe', name)
        for side in ('input', 'output'):
            amounts = recipe[side + '_quantities']
            resources = recipe[side + '_resources']
            ensure(len(amounts) == len(resources) > 0 and all(n > 0 for n in amounts),
                   key, f'invalid {side} quantities')
            ensure(len(set(resources)) == len(resources), key, 'duplicate recipe resource', side + '_resources')
            for resource in resources:
                definition = reference(key, side + '_resources', 'resource', resource)
                ensure(definition['unit'] == recipe['unit'], key, 'incompatible recipe units')
        if recipe['accounting'] == 'mass_conserved_including_byproducts':
            ensure(recipe['unit'] == 'g' and
                   sum(recipe['input_quantities']) == sum(recipe['output_quantities']),
                   key, 'mass including byproducts must balance')
        if not recipe['worker_minutes'] or not recipe['required_equipment']:
            blockers.append(f'recipe {name}: configure positive worker minutes and equipment')
        ensure(recipe['worker_minutes'] is None or recipe['worker_minutes'] > 0,
               key, 'invalid worker minutes')
        for equipment in recipe['required_equipment']:
            definition = reference(key, 'required_equipment', 'resource', equipment)
            ensure(definition['group'] == 'equipment' and definition['unit'] == 'each',
                   key, 'recipe equipment must be a counted equipment kind', 'required_equipment')
        ensure(not set(recipe['required_equipment']).intersection(recipe['input_resources']),
               key, 'equipment cannot also be consumed as material', 'required_equipment')
        ensure(len(set(recipe['required_equipment'])) == len(recipe['required_equipment']),
               key, 'duplicate equipment requirement', 'required_equipment')
        if recipe['activation'] != 'manual_power_technical_trial':
            blockers.append(f'recipe {name}: configure supported process power')
        elif name not in REFINED_RECIPES:
            blockers.append(f'recipe {name}: native production binding pending')
        if recipe['worker_minutes'] is not None:
            ensure(recipe['worker_minutes'] <= 10080, key, 'recipe duration exceeds 10080 minutes')

    for name, service in group('service').items():
        key = ('service', name)
        ensure(not service['stockpilable'] and service['requires_native_action_grant'],
               key, 'services require native work and cannot be stockpiled')
        if not service['duration_and_output_rate']:
            blockers.append(f'service {name}: configure delivery rate')
        else:
            blockers.append(f'service {name}: native DSL service binding pending')
        ensure(service['duration_and_output_rate'] is None or service['duration_and_output_rate'] > 0,
               key, 'invalid delivery rate')

    for name, shift in group('shift').items():
        ensure(0 <= shift['start_minute'] < 1440 and 1 <= shift['duration_minutes'] <= 1440,
               ('shift', name), 'shift outside civil minute bounds')
    for kind, bindings in [('curriculum', {'career': 'career', 'qualification': 'qualification'}),
                           ('credential', {'qualification': 'qualification'}),
                           ('stock', {'resource': 'resource'}),
                           ('post', {'employer': 'employer', 'occupation': 'occupation',
                                     'shift': 'shift'})]:
        for name, record in group(kind).items():
            for field, target in bindings.items():
                reference((kind, name), field, target, record[field])
    for name, record in group('curriculum').items():
        ensure(record['supervised_minutes'] > 0 and record['mentor_stage'] > 0,
               ('curriculum', name), 'curriculum requires positive training time and mentor stage')
    if premises is not None:
        for kind, field in [('household', 'dwelling'), ('employer', 'premises'),
                            ('stock', 'premises')]:
            for name, record in group(kind).items():
                ensure(record[field] in premises, (kind, name),
                       f'unknown map building {record[field]!r}', field)

    residents = set()
    for name, household in group('household').items():
        members = household['members']
        ensure(bool(members) and len(set(members)) == len(members) and not residents.intersection(members),
               ('household', name), 'household members must be nonempty and unique')
        ensure(household['guardian'] is None or household['guardian'] in members,
               ('household', name), 'guardian must belong to household')
        residents.update(members)
    ensure(len(residents) <= scenario['population'], root, 'households exceed population')
    if len(residents) != scenario['population'] and scenario['status'] != 'population_fixture':
        blockers.append('declare households and premises for the scenario population')

    vacancies = {}
    funded_roles = set()
    funding = {}
    for name, post in group('post').items():
        key = ('post', name)
        role = group('occupation')[post['occupation']]
        ensure(post['vacancies'] == 0 or role['enabled'], key, 'vacancies require an enabled role')
        if post['vacancies']:
            funded_roles.add(post['occupation'])
        employer = post['employer']
        vacancies[employer] = vacancies.get(employer, 0) + post['vacancies']
        funding[employer] = funding.get(employer, 0) + post['vacancies'] * post['wage_pence']
    for name, employer in group('employer').items():
        ensure(vacancies.get(name, 0) <= employer['work_capacity'] and
               funding.get(name, 0) <= employer['funding_pence'], ('employer', name),
               'posts exceed employer capacity or one-shift payroll funding')

    commands = [entry['name'] for entry in controls['commands']]
    task_bindings = {}
    for name, task in group('task').items():
        key = ('task', name)
        handler = task['handler']
        ensure(handler in {'work', 'refined-work'} and 'work' in commands,
               key, 'unsupported task handler', 'handler')
        ensure(1 <= task['units'] <= 1000, key, 'work units must be in 1..1000', 'units')
        binding = {'operation': 'work', 'opcode': commands.index('work'),
                   'units': task['units'], 'target': 'assigned_job'}
        if handler == 'refined-work':
            reference(key, 'recipe', 'recipe', task['recipe'])
            ensure(task['recipe'] in REFINED_RECIPES, key,
                   'refined recipe has no native occupation binding', 'recipe')
            binding.update(recipe=len(WORK_RECIPES), recipe_id=task['recipe'])
        else:
            ensure(task['recipe'] in WORK_RECIPES, key, 'unknown native work recipe', 'recipe')
            binding['recipe'] = WORK_RECIPES.index(task['recipe'])
        task_bindings[name] = binding
    for name, program in group('program').items():
        ensure(0 < len(program['tasks']) <= 256, ('program', name), 'program task budget exceeded')
        for task in program['tasks']:
            reference(('program', name), 'tasks', 'task', task)
    if group('occupation') and not any(role['enabled'] for role in group('occupation').values()):
        blockers.append('enable and bind eligible occupations')
    for name, role in group('occupation').items():
        if role['enabled'] and name not in funded_roles:
            blockers.append(f'occupation {name}: no funded post')
    normalized = [{'kind': kind, 'id': name, 'fields': fields | (
        {'language_version': schema['version']} if kind == 'scenario' else {})}
        for (kind, name), fields in sorted(records.items())]
    return {'authored_version': version, 'map_sha256': map_hash,
            'activation_blockers': sorted(set(blockers)), 'records': normalized,
            'task_bindings': task_bindings}


def check_activation(pack, map_hash):
    body = {key: value for key, value in pack.items() if key != 'identity'}
    if pack.get('format') != 'canterbury-scenario-pack' or pack.get('version') != 1:
        raise DraftError('unsupported scenario pack format')
    if pack.get('identity') != digest(canonical(body)):
        raise DraftError('scenario pack identity mismatch')
    if not map_hash or body['map_sha256'] != map_hash:
        raise DraftError('scenario pack was compiled against different map content')
    if body['activation_blockers']:
        raise DraftError('scenario prerequisites missing:\n  ' + '\n  '.join(body['activation_blockers']))


def build(source=SOURCE, output=PACK, city=CITY, check=False, activation=False):
    with source.open('rb') as stream:
        raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise DraftError(f'{source}: file exceeds 8 MiB')
    inputs = {path: path.read_bytes() for path in (SCHEMA, CATALOGUE, CONTROLS, city)}
    records = parse(raw.decode('utf-8'), str(source))
    map_hash = digest(inputs[city])
    premises = map_premises(inputs[city])
    limits = records.get(('limits', 'authoring'), {})
    if type(limits.get('file_bytes')) is int and len(raw) > limits['file_bytes']:
        raise records.error(('limits', 'authoring'), 'source exceeds authored file budget')
    body = compile_records(records, json.loads(inputs[SCHEMA]), json.loads(inputs[CATALOGUE]),
                           digest(inputs[CATALOGUE]), json.loads(inputs[CONTROLS]), map_hash, premises)
    body.update(format='canterbury-scenario-pack', version=1,
                schema_sha256=digest(inputs[SCHEMA]),
                occupation_sha256=digest(canonical([r for r in body['records'] if r['kind'] == 'occupation'])),
                resource_sha256=digest(canonical([r for r in body['records'] if r['kind'] == 'resource'])),
                operation_catalogue_sha256=digest(inputs[CONTROLS]))
    pack = body | {'identity': digest(canonical(body))}
    packed = canonical(pack)
    if activation:
        check_activation(pack, map_hash)
    if check:
        if output.read_bytes() != packed:
            raise DraftError(f'{output}: scenario pack is stale')
    else:
        def fresh():
            if source.read_bytes() != raw or any(path.read_bytes() != data for path, data in inputs.items()):
                raise DraftError('scenario inputs changed before publication')
        if not output.is_file() or output.read_bytes() != packed:
            atomic_write(output, packed, fresh)
    return pack


def build_startup(source, output, city):
    """Lower the population-only profile to a fixed-width native startup record."""
    with source.open('rb') as stream:
        raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise DraftError('source exceeds 8 MiB')
    records = parse(raw.decode('utf-8'), str(source))
    if any(kind not in {'scenario', 'cohort'} for kind, _ in records):
        raise DraftError('population startup supports only scenario and cohort declarations')
    map_bytes = city.read_bytes()
    premises = map_premises(map_bytes)
    schema_bytes = SCHEMA.read_bytes()
    body = compile_records(records, json.loads(schema_bytes), {}, '', {'commands': []},
                           digest(map_bytes), premises)
    if body['activation_blockers']:
        raise DraftError('startup prerequisites missing: ' + ', '.join(body['activation_blockers']))
    scenario = next(value for (kind, _), value in records.items() if kind == 'scenario')
    if (scenario['language_version'] != '1.0' or not scenario['runtime_activation'] or
            scenario['status'] != 'population_fixture' or scenario['population_capacity'] != 30000):
        raise DraftError('startup requires an active 1.0 population_fixture with capacity 30000')
    bands = ('0-4', '5-14', '15-17', '18-64', '65+')
    counts = {record['age_band']: record['people'] for (kind, _), record in records.items()
              if kind == 'cohort'}
    population = scenario['population']
    child_slots = 2 * (population // 4) + max(0, population % 4 - 2)
    if (not population or len(records) != 6 or set(counts) != set(bands) or
            sum(counts[band] for band in bands[:3]) > child_slots):
        raise DraftError('startup requires five distinct age bands and two adult slots per household')
    origin = datetime.strptime(scenario['civil_origin'], '%Y-%m-%dT%H:%M')
    identity = digest(canonical(body) + schema_bytes).encode('ascii')
    payload = struct.pack('<Q11I', len(map_bytes), zlib.crc32(map_bytes), population,
                          scenario['seed'], origin.month, origin.day,
                          origin.hour * 60 + origin.minute, *(counts[band] for band in bands))
    payload += identity
    def fresh():
        if source.read_bytes() != raw or city.read_bytes() != map_bytes or SCHEMA.read_bytes() != schema_bytes:
            raise DraftError('startup inputs changed before publication')
    atomic_write(output, b'CDSTART1' + struct.pack('<I', zlib.crc32(payload)) + payload, fresh)
    return identity.decode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', nargs='?', type=Path, default=SOURCE)
    parser.add_argument('--output', type=Path, default=PACK)
    parser.add_argument('--city', type=Path, default=CITY)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--startup', type=Path, help='write a native population startup pack')
    parser.add_argument('--require-activation', action='store_true')
    args = parser.parse_args()
    try:
        if args.startup:
            if args.check or args.require_activation:
                raise DraftError('--startup cannot be combined with pack checking')
            print(build_startup(args.source, args.startup, args.city))
            return
        pack = build(args.source, args.output, args.city, args.check, args.require_activation)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(2, f'{error}\n')
    print(json.dumps({'identity': pack['identity'], 'records': len(pack['records']),
                      'activation_blockers': len(pack['activation_blockers'])}, sort_keys=True))


if __name__ == '__main__':
    main()
