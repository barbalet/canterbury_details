"""Acceptance coverage for the scenario compiler and publication boundary."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from canterbury_dsl_parser import DraftError, MAX_BYTES, parse
from compile_canterbury_dsl import (
    CATALOGUE, CITY, CONTROLS, SCHEMA, SOURCE, build, check_activation,
    compile_records, digest, map_premises,
)


class CompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = SOURCE.read_text()
        cls.schema = json.loads(SCHEMA.read_text())
        cls.catalogue = json.loads(CATALOGUE.read_text())
        cls.controls = json.loads(CONTROLS.read_text())
        cls.map_hash = digest(CITY.read_bytes())

    def test_parser_reports_locations_and_enforces_boundaries(self):
        for text, diagnostic in [
            ('\nscenario test { population 9223372036854775808; }', 'signed 64-bit'),
            ('\nscenario test { population 1e400; }', 'non-finite'),
            ('\nscenario test { x ' + '[' * 66 + '0' + ']' * 66 + '; }', 'nesting'),
        ]:
            with self.assertRaisesRegex(DraftError, r'fixture:2:\d+: .*' + diagnostic):
                parse(text, 'fixture')
        with self.assertRaisesRegex(DraftError, '8 MiB'):
            parse(' ' * (MAX_BYTES + 1))

    def test_canonical_pack_and_transactional_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'scenario.canterbury'
            output = Path(directory) / 'pack.json'
            source.write_text(self.raw)
            pack = build(source, output)
            original = output.read_bytes()
            source.write_text('# formatting changes\n\n' + self.raw)
            self.assertEqual(build(source, output, check=True)['identity'], pack['identity'])
            self.assertEqual(len(pack['records']), 841)
            self.assertEqual(pack['authored_version'], '0.1')
            self.assertEqual(next(r for r in pack['records'] if r['kind'] == 'scenario')
                             ['fields']['language_version'], '1.0')
            source.write_text(self.raw.replace('population 27000;', 'population true;', 1))
            with self.assertRaisesRegex(DraftError, 'population requires integer'):
                build(source, output)
            self.assertEqual(output.read_bytes(), original)
            source.write_text(self.raw)
            with self.assertRaisesRegex(DraftError, 'prerequisites missing'):
                build(source, output, activation=True)
            self.assertEqual(output.read_bytes(), original)
            with self.assertRaisesRegex(DraftError, 'different map'):
                check_activation(pack, '0' * 64)
            pack['activation_blockers'] = []
            with self.assertRaisesRegex(DraftError, 'identity mismatch'):
                check_activation(pack, self.map_hash)

    def test_version_one_native_binding_and_reference_validation(self):
        records = parse(self.raw)
        scenario = records['scenario', 'canterbury_1940']
        scenario['language_version'] = '1.0'
        records.update(parse('''
            task breakfast { handler "work"; recipe "meal"; units 1; }
            program cook { tasks ["breakfast"]; }
        '''))
        role = records['occupation', 'grocer.2']
        role['task_program'] = 'cook'

        def compile_fixture():
            return compile_records(records, self.schema, self.catalogue,
                                   digest(CATALOGUE.read_bytes()), self.controls,
                                   self.map_hash)

        binding = compile_fixture()['task_bindings']['breakfast']
        self.assertEqual(binding['operation'], 'work')
        self.assertEqual(binding['recipe'], 0)
        self.assertEqual(binding['target'], 'assigned_job')
        records['task', 'breakfast']['handler'] = 'train'
        with self.assertRaisesRegex(DraftError, 'unsupported task handler'):
            compile_fixture()
        records['task', 'breakfast']['handler'] = 'work'
        role['task_program'] = 'missing'
        with self.assertRaisesRegex(DraftError, 'unknown program reference'):
            compile_fixture()

    def test_refined_binding_retains_stable_recipe_id(self):
        records = parse(self.raw)
        records['scenario', 'canterbury_1940']['language_version'] = '1.0'
        records.update(parse('task trim { handler "refined-work"; recipe "cut_timber"; units 1; }'))
        def compile_fixture():
            return compile_records(records, self.schema, self.catalogue,
                                   digest(CATALOGUE.read_bytes()), self.controls,
                                   self.map_hash)

        binding = compile_fixture()['task_bindings']['trim']
        self.assertEqual(binding['operation'], 'work')
        self.assertEqual(binding['recipe_id'], 'cut_timber')
        self.assertNotIn('secondary', binding)
        records['recipe', 'unbound_trial'] = dict(records['recipe', 'mill_wheat'])
        records['task', 'trim']['recipe'] = 'unbound_trial'
        with self.assertRaisesRegex(DraftError, 'no native occupation binding'):
            compile_fixture()

    def test_activation_checks_each_role_and_service(self):
        records = parse(self.raw)
        records['scenario', 'canterbury_1940']['language_version'] = '1.0'
        records['occupation', 'grocer.2']['enabled'] = True
        records['occupation', 'grocer.3']['enabled'] = True
        records['service', 'retail_sale']['duration_and_output_rate'] = 1
        records.update(parse("""
            employer shop {
              premises "fixture"; name "Fixture shop";
              work_capacity 1; funding_pence 10;
            }
            shift morning { start_minute 480; duration_minutes 60; }
            post counter {
              employer "shop"; occupation "grocer.2"; shift "morning";
              vacancies 1; wage_pence 10;
            }
        """))
        blockers = compile_records(records, self.schema, self.catalogue,
                                   digest(CATALOGUE.read_bytes()), self.controls,
                                   self.map_hash)['activation_blockers']
        self.assertNotIn('occupation grocer.2: no funded post', blockers)
        self.assertIn('occupation grocer.3: no funded post', blockers)
        self.assertIn('service retail_sale: native DSL service binding pending', blockers)

    def test_map_checksum_and_premises(self):
        raw = CITY.read_bytes()
        self.assertEqual(len(map_premises(raw)), 3341)
        damaged = raw[:-1] + bytes([raw[-1] ^ 1])
        with self.assertRaisesRegex(DraftError, 'checksum'):
            map_premises(damaged)


if __name__ == '__main__':
    unittest.main()
