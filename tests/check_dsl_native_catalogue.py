"""Check each compiled native definition against the authored DSL."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compile_canterbury_dsl import SOURCE, parse


def inspect(query):
    return json.loads(subprocess.check_output(
        [ROOT / 'build/CanterburyDetails', '--dsl-definition', query], text=True
    ))


def main():
    records = parse(SOURCE.read_text())
    counts = dict(families=60, roles=300, advancements=240, resources=72, recipes=29)
    summary = inspect('summary')
    assert all(summary[key] == count for key, count in counts.items())
    comparisons = {
        'career': ('sector', 'candidate_premises_types'),
        'occupation': ('title', 'career', 'qualification', 'stage', 'training_post', 'enabled'),
        'advance': ('from', 'to'),
        'resource': ('unit', 'group'),
        'recipe': ('unit', 'worker_minutes'),
    }
    for (kind, name), record in records.items():
        if kind not in comparisons:
            continue
        value = inspect(name)
        assert all(value[field] == record[field] for field in comparisons[kind]), (kind, name)
        if kind == 'recipe':
            for side in ('input', 'output'):
                expected = [dict(resource=resource, quantity=quantity) for resource, quantity in
                            zip(record[side + '_resources'], record[side + '_quantities'])]
                assert value[side + 's'] == expected, (name, side)
            assert value['equipment'] == record['required_equipment']
    print(f'PASS all {sum(counts.values())} native family, role, advancement, resource and recipe definitions')


if __name__ == '__main__':
    main()
