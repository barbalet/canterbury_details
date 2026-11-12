"""Generate immutable native definitions from the Canterbury DSL."""
import json

from compile_canterbury_dsl import (
    CATALOGUE, CONTROLS, ROOT, SCHEMA, SOURCE, compile_records, digest, parse,
)
from compile_content import atomic_write, canonical

OUTPUT = ROOT / 'build/content/CanterburyDefinitions.inc'


def build():
    paths = (SOURCE, SCHEMA, CATALOGUE, CONTROLS)
    inputs = {path: path.read_bytes() for path in paths}
    schema = json.loads(inputs[SCHEMA])
    records = parse(inputs[SOURCE].decode('utf-8'), str(SOURCE))
    body = compile_records(records, schema, json.loads(inputs[CATALOGUE]),
                           digest(inputs[CATALOGUE]), json.loads(inputs[CONTROLS]), None)
    declarations = {(record['kind'], record['id']): record['fields']
                    for record in body['records']}

    def entries(kind):
        return [(name, fields) for (group, name), fields in declarations.items()
                if group == kind]

    def string(value):
        return json.dumps(value, ensure_ascii=False)

    def array(name, type_name, rows):
        return f'static const {type_name} {name}[] = {{\n' + ''.join(
            '    {' + ', '.join(row) + '},\n' for row in rows
        ) + '};\n'

    identity = digest(canonical(body['records']))
    output = '/* Generated from the checked Canterbury DSL. */\n'
    output += f'static const char definition_identity[] = "{identity}";\n'
    career_rows = []
    for index, (name, career) in enumerate(entries('career')):
        premises = career['candidate_premises_types']
        symbol = 'NULL'
        if premises:
            symbol = f'career_{index}_premises'
            output += f'static const char *const {symbol}[] = {{'
            output += ', '.join(map(string, premises)) + '};\n'
        career_rows.append([string(name), string(career['sector']), str(len(premises)), symbol])
    output += array('career_families', 'CdDslCareer', career_rows)
    output += array('career_roles', 'CdDslRole', [
        [string(name), string(role['title']), string(role['career']),
         string(role['qualification']), str(role['stage']),
         str(int(role['training_post'])), str(int(role['enabled'])),
         str(role['mentor_minimum_stage'] or 0),
         str(int('capital or approved finance' in role['advancement_requirements']))]
        for name, role in entries('occupation')
    ])
    output += array('career_transitions', 'CdDslAdvance', [
        [string(name), string(link['from']), string(link['to'])]
        for name, link in entries('advance')
    ])
    output += array('resource_kinds', 'CdDslResource', [
        [string(name), string(resource['group']), string(resource['unit'])]
        for name, resource in entries('resource')
    ])
    recipe_rows = []
    for index, (name, recipe) in enumerate(entries('recipe')):
        symbols = {}
        for side in ('input', 'output'):
            symbols[side] = f'recipe_{index}_{side}'
            output += array(symbols[side], 'CdDslQuantity', [
                [string(resource), f'UINT64_C({quantity})']
                for resource, quantity in zip(recipe[side + '_resources'],
                                              recipe[side + '_quantities'])
            ])
        equipment = recipe['required_equipment']
        equipment_symbol = 'NULL'
        if equipment:
            equipment_symbol = f'recipe_{index}_equipment'
            output += f'static const char *const {equipment_symbol}[] = {{'
            output += ', '.join(map(string, equipment)) + '};\n'
        recipe_rows.append([
            string(name), string(recipe['unit']), string(recipe['activation']),
            str(recipe['worker_minutes'] or 0), str(len(recipe['input_resources'])),
            str(len(recipe['output_resources'])), str(len(equipment)),
            symbols['input'], symbols['output'], equipment_symbol,
        ])
    output += array('material_recipes', 'CdDslRecipe', recipe_rows)
    raw = output.encode('utf-8')
    if not OUTPUT.is_file() or OUTPUT.read_bytes() != raw:
        def fresh():
            if any(path.read_bytes() != data for path, data in inputs.items()):
                raise ValueError('DSL catalogue inputs changed before publication')
        atomic_write(OUTPUT, raw, fresh)
    return OUTPUT


if __name__ == '__main__':
    print(build().relative_to(ROOT))
