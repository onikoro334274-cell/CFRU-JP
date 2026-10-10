"""Ensure conflicting reserved save IDs cannot be emitted as a header."""
import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock
import recover_hoenn_flags


class ReservedAllocationTests(unittest.TestCase):
    def run_recovery(self, flags, variables, reject, *, mapped=None, trainer_ids=None,
                     constant_overrides=None, expected_lines=()):
        constants = {
            'C_FLAGS': {}, 'C_VARS': {}, 'RESERVED_FLAGS': flags,
            'RESERVED_VARS': variables, 'HOENN_FLAG_END': 100,
            'VISIT_FLAG_FIRST': 200, 'HOENN_VAR_END': 300,
            'VAR_RESERVED_LAST': 400, 'VAR_RESERVED_HIGH_FIRST': 500,
            'VAR_RESERVED_HIGH_END': 600, 'FERRY_SEVII_FLAG': '0x5C',
        }
        allocations = {'flags': {'existing': 150}, 'vars': {'existing': 350}, 'trainers': {}}
        trainers = {'LEAGUE_REMATCH': {}}
        if mapped is not None:
            namespace, value = mapped
            constants['C_FLAGS' if namespace == 'flags' else 'C_VARS'] = {'MAPPED': 'mapped'}
            allocations[namespace]['mapped'] = value
        if trainer_ids is not None:
            trainers['LEAGUE_REMATCH'] = {'REMATCH': ('BASE', 'unused')}
            allocations['trainers'] = {'BASE': trainer_ids[0], 'REMATCH': trainer_ids[1]}
        constants.update(constant_overrides or {})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'hoenn_scripts.json').write_text(json.dumps(allocations), encoding='utf-8')
            output = root / 'output.h'
            output.write_text('keep prior output', encoding='utf-8')
            with mock.patch('sys.argv', ['recover', '--source-tools', str(root), '--output', str(output)]), \
                 mock.patch.object(recover_hoenn_flags, 'literals', side_effect=[constants, trainers]), \
                 contextlib.redirect_stdout(io.StringIO()):
                if reject:
                    with self.assertRaises(ValueError):
                        recover_hoenn_flags.main()
                    self.assertEqual(output.read_text(encoding='utf-8'), 'keep prior output')
                else:
                    recover_hoenn_flags.main()
                    text = output.read_text(encoding='utf-8')
                    self.assertIn('#pragma once', text)
                    for line in expected_lines:
                        self.assertIn(line, text)

    def test_duplicate_reserved_flags_rejected(self):
        self.run_recovery({'FIRST': 100, 'SECOND': 100}, {}, True)

    def test_duplicate_reserved_vars_rejected(self):
        self.run_recovery({}, {'FIRST': 500, 'SECOND': 500}, True)

    def test_distinct_reserved_ids_accepted(self):
        self.run_recovery({'FIRST': 100, 'SECOND': 199}, {'LOW': 300, 'HIGH': 599}, False)

    def test_allocated_flag_overlap_rejected(self):
        self.run_recovery({'FIRST': 150}, {}, True)

    def test_upper_var_boundary_rejected(self):
        self.run_recovery({}, {'FIRST': 600}, True)

    def test_noninteger_reserved_ids_rejected(self):
        for value in (True, False, 100.0, '100', None, -1, 65536):
            with self.subTest(value=value):
                self.run_recovery({'FIRST': value}, {}, True)

    def test_definition_name_collision_rejected(self):
        self.run_recovery({'SAME': 100}, {'SAME': 500}, True)

    def test_ferry_name_collision_rejected(self):
        self.run_recovery({'HOENN_FLAG_CAN_GO_HOENN': 100}, {}, True)

    def test_invalid_mapped_halfwords_rejected(self):
        for namespace in ('flags', 'vars'):
            for value in (True, False, 100.0, '100', None, -1, 65536):
                with self.subTest(namespace=namespace, value=value):
                    self.run_recovery({}, {}, True, mapped=(namespace, value))

    def test_mapped_halfword_boundaries_preserved(self):
        for namespace in ('flags', 'vars'):
            for value in (0, 65535):
                with self.subTest(namespace=namespace, value=value):
                    self.run_recovery({}, {}, False, mapped=(namespace, value),
                                      expected_lines=(f'#define MAPPED 0x{value:04X}',))

    def test_invalid_rematch_halfwords_rejected(self):
        for position in (0, 1):
            for value in (True, False, 100.0, '100', None, -1, 65536):
                pair = [12, 34]
                pair[position] = value
                with self.subTest(position=position, value=value):
                    self.run_recovery({}, {}, True, trainer_ids=pair)

    def test_rematch_halfword_boundaries_preserved(self):
        self.run_recovery({}, {}, False, trainer_ids=(0, 65535),
                          expected_lines=('#define HOENN_LEAGUE_REMATCH_COUNT 1',
                                          '#define HOENN_LEAGUE_REMATCH_TABLE {{0, 65535}}'))

    def test_mapped_reserved_name_collision_rejected(self):
        self.run_recovery({'MAPPED': 100}, {}, True, mapped=('flags', 1))

    def test_invalid_ferry_halfword_rejected(self):
        for value in ('-1', '0x10000'):
            with self.subTest(value=value):
                self.run_recovery({}, {}, True, constant_overrides={'FERRY_SEVII_FLAG': value})


class HeaderWriteTests(unittest.TestCase):
    def test_success_replaces_complete_utf8_header(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'header.h'
            output.write_bytes(b'old')
            recover_hoenn_flags.write_header(output, '// 日本語\n#define FLAG 100\n')
            self.assertEqual(output.read_bytes(), '// 日本語\n#define FLAG 100\n'.encode('utf-8'))
            self.assertEqual(list(Path(directory).iterdir()), [output])

    def test_replace_failure_preserves_prior_header(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'header.h'
            output.write_bytes(b'old')
            with mock.patch.object(recover_hoenn_flags.os, 'replace', side_effect=PermissionError('locked')):
                with self.assertRaises(PermissionError):
                    recover_hoenn_flags.write_header(output, 'new\n')
            self.assertEqual(output.read_bytes(), b'old')
            self.assertEqual(list(Path(directory).iterdir()), [output])

    def test_encoding_failure_preserves_prior_header(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'header.h'
            output.write_bytes(b'old')
            with self.assertRaises(UnicodeEncodeError):
                recover_hoenn_flags.write_header(output, 'bad\ud800')
            self.assertEqual(output.read_bytes(), b'old')
            self.assertEqual(list(Path(directory).iterdir()), [output])


if __name__ == '__main__':
    unittest.main()
