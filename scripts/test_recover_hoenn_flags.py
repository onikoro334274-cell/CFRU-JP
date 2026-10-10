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
    def run_recovery(self, flags, variables, reject):
        constants = {
            'C_FLAGS': {}, 'C_VARS': {}, 'RESERVED_FLAGS': flags,
            'RESERVED_VARS': variables, 'HOENN_FLAG_END': 100,
            'VISIT_FLAG_FIRST': 200, 'HOENN_VAR_END': 300,
            'VAR_RESERVED_LAST': 400, 'VAR_RESERVED_HIGH_FIRST': 500,
            'VAR_RESERVED_HIGH_END': 600, 'FERRY_SEVII_FLAG': '0x5C',
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'hoenn_scripts.json').write_text(json.dumps({
                'flags': {'existing': 150}, 'vars': {'existing': 350}, 'trainers': {}}), encoding='utf-8')
            output = root / 'output.h'
            output.write_text('keep prior output', encoding='utf-8')
            with mock.patch('sys.argv', ['recover', '--source-tools', str(root), '--output', str(output)]), \
                 mock.patch.object(recover_hoenn_flags, 'literals', side_effect=[constants, {'LEAGUE_REMATCH': {}}]), \
                 contextlib.redirect_stdout(io.StringIO()):
                if reject:
                    with self.assertRaises(ValueError):
                        recover_hoenn_flags.main()
                    self.assertEqual(output.read_text(encoding='utf-8'), 'keep prior output')
                else:
                    recover_hoenn_flags.main()
                    self.assertIn('#pragma once', output.read_text(encoding='utf-8'))

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
