import io
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import make
from rom_bounds import MAX_ROM_SIZE, validate_payload

from make import BuildCode, ChangeFileLine, FindOffsetToPut, InsertCode


class FreeSpaceTests(unittest.TestCase):
    def test_occupied_first_byte(self):
        self.assertEqual(FindOffsetToPut(io.BytesIO(b'\x00' + b'\xff' * 31), 16, 0), 16)

    def test_exact_end_of_file(self):
        self.assertEqual(FindOffsetToPut(io.BytesIO(b'\xff' * 16), 16, 0), 0)

    def test_no_space(self):
        with self.assertRaises(ValueError):
            FindOffsetToPut(io.BytesIO(b'\xff' * 15), 16, 0)

    def test_search_stays_within_code_address_range(self):
        with self.assertRaises(ValueError):
            FindOffsetToPut(io.BytesIO(b'\x00' * 16 + b'\xff' * 32), 16, 0, 16)

    def test_chunk_boundary(self):
        data = b'\x00' * 65520 + b'\xff' * 64
        self.assertEqual(FindOffsetToPut(io.BytesIO(data), 64, 0), 65520)

    def test_matches_aligned_slice_search(self):
        rng = random.Random(0)
        for _ in range(1000):
            data = bytes(rng.choice((0, 255, 255, 255)) for _ in range(128))
            start = rng.randrange(140)
            size = rng.randrange(1, 24)
            expected = next((offset for offset in range((start + 15) & ~15, len(data), 16)
                             if data[offset:offset + size] == b'\xff' * size), None)
            if expected is None:
                with self.assertRaises(ValueError):
                    FindOffsetToPut(io.BytesIO(data), size, start)
            else:
                self.assertEqual(FindOffsetToPut(io.BytesIO(data), size, start), expected)


class BuildScriptTests(unittest.TestCase):
    def test_payload_bounds(self):
        validate_payload(MAX_ROM_SIZE - 1, 1)
        for offset, size in ((-1, 1), (0, 0), (MAX_ROM_SIZE, 1), (MAX_ROM_SIZE - 1, 2)):
            with self.subTest(offset=offset, size=size), self.assertRaises(ValueError):
                validate_payload(offset, size)

    def test_build_relocates_when_actual_payload_does_not_fit(self):
        rom = io.BytesIO(b'\xff' * 16 + b'\x00' * 16 + b'\xff' * 64)
        with patch('make.open', return_value=rom), patch.object(make, 'OFFSET_TO_PUT', 0), \
                patch.object(make, 'SEARCH_FREE_SPACE', True), \
                patch.object(make, 'EditLinker') as linker, patch.object(make, 'EditInsert') as config, \
                patch.object(make, 'BuildCode') as build, patch.object(make, 'InsertCode') as insert, \
                patch('make.Path') as path:
            path.return_value.stat.return_value.st_size = 32
            make.main()
            self.assertEqual([call.args[0] for call in linker.call_args_list], [0, 32])
            self.assertEqual([call.args[0] for call in config.call_args_list], [0, 32])
            self.assertEqual(build.call_count, 2)
            insert.assert_called_once_with()

    def test_build_does_not_insert_when_free_space_is_insufficient(self):
        rom = io.BytesIO(b'\xff' * 16)
        with patch('make.open', return_value=rom), patch.object(make, 'OFFSET_TO_PUT', 0), \
                patch.object(make, 'SEARCH_FREE_SPACE', True), \
                patch.object(make, 'EditLinker'), patch.object(make, 'EditInsert'), \
                patch.object(make, 'BuildCode'), patch.object(make, 'InsertCode') as insert, \
                patch('make.Path') as path:
            path.return_value.stat.return_value.st_size = 32
            with self.assertRaises(ValueError):
                make.main()
            insert.assert_not_called()

    def test_replaces_only_requested_line(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'config.txt'
            path.write_text('same\nsame\nlast\n')
            ChangeFileLine(str(path), 2, 'changed\n')
            self.assertEqual(path.read_text(), 'same\nchanged\nlast\n')
            with self.assertRaises(ValueError):
                ChangeFileLine(str(path), 4, 'bad\n')
            self.assertEqual(path.read_text(), 'same\nchanged\nlast\n')

    def test_child_failures_propagate(self):
        for function, script in ((BuildCode, 'build.py'), (InsertCode, 'insert.py')):
            with self.subTest(script=script):
                calls = []
                original = subprocess.run
                def fail(args, **kwargs):
                    calls.append((args, kwargs))
                    raise subprocess.CalledProcessError(1, script)
                subprocess.run = fail
                try:
                    with self.assertRaises(subprocess.CalledProcessError):
                        function()
                    self.assertEqual(calls, [([sys.executable, 'scripts/' + script], {'check': True})])
                finally:
                    subprocess.run = original


if __name__ == '__main__':
    unittest.main()
