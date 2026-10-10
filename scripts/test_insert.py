"""Exercise insertion using a synthetic ROM and Windows legacy text defaults."""

import builtins
import contextlib
import io
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import insert


class InsertEncodingTests(unittest.TestCase):
    def test_utf8_tables_apply_under_cp1252_default(self):
        original_open = builtins.open

        def legacy_open(file, mode='r', *args, **kwargs):
            if 'b' not in mode and not args and 'encoding' not in kwargs:
                kwargs['encoding'] = 'cp1252'
            return original_open(file, mode, *args, **kwargs)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'build').mkdir()
            source = bytes(range(256))
            (root / 'source.gba').write_bytes(source)
            (root / 'build/output.bin').write_bytes(b'payload')
            (root / 'defs.h').write_text('// 日本語\n#define ENABLED\n', encoding='utf-8')
            comment = '## 日本語コメントあ\n'
            tables = {
                'bytereplacement': '#include "defs.h"\n#ifdef ENABLED\n08000000 FE FD\n#endif\n',
                'hooks': 'dummy 08000010 0\n',
                'repoints': 'dummy 08000024\n',
                'repointall': '',
                'routinepointers': 'dummy 08000028\n',
                'functionrewrites': '',
                'eventscripts': '',
                'songs': '',
            }
            for name, lines in tables.items():
                (root / name).write_text(comment + lines, encoding='utf-8-sig')
            # Confirm that the fixture actually fails under the legacy decoder.
            with self.assertRaises(UnicodeDecodeError):
                (root / 'hooks').read_bytes().decode('cp1252')
            previous = Path.cwd()
            try:
                os.chdir(root)
                with mock.patch.multiple(insert, SOURCE_ROM='source.gba', ROM_NAME='result.gba',
                                         OFFSET_TO_PUT=0x40), \
                     mock.patch.object(insert, 'GetSymbols', return_value={'dummy': 0}), \
                     mock.patch.object(insert, 'GetTextSection', return_value=0), \
                     mock.patch('builtins.open', side_effect=legacy_open), \
                     contextlib.redirect_stdout(io.StringIO()):
                    insert.main()
            finally:
                os.chdir(previous)
            actual = (root / 'result.gba').read_bytes()
            self.assertEqual((root / 'source.gba').read_bytes(), source)
            self.assertEqual(actual[:2], b'\xfe\xfd')
            self.assertEqual(actual[0x40:0x47], b'payload')
            self.assertEqual(actual[0x24:0x28], (0x08000040).to_bytes(4, 'little'))
            self.assertEqual(actual[0x28:0x2c], (0x08000041).to_bytes(4, 'little'))
            self.assertIn('dummy:', (root / 'offsets.ini').read_text(encoding='utf-8'))


class InsertFailureTests(unittest.TestCase):
    def prepare(self, root):
        (root / 'build').mkdir()
        (root / 'build/output.bin').write_bytes(b'payload')
        source = bytearray(0x1c10dc)
        # Minimal map-bank, map-header and song-table pointers for event/song tests.
        for offset, value in ((0x54b0c, 0x100), (0x100, 0x110),
                              (0x110, 0x120), (0x1c10d8, 0x130),
                              (16, 0x1a0), (96, 0x1a0), (20, 0x1b0), (100, 0x1b0)):
            source[offset:offset + 4] = (0x08000000 + value).to_bytes(4, 'little')
        (root / 'source.gba').write_bytes(source)
        previous = {'result.gba': b'previous ROM', 'generatedrepoints': b'## previous cache\n',
                    'offsets.ini': b'previous offsets\n'}
        for name, data in previous.items():
            (root / name).write_bytes(data)
        return previous

    def invoke(self, root, symbols=None):
        previous = Path.cwd()
        try:
            os.chdir(root)
            with mock.patch.multiple(insert, SOURCE_ROM='source.gba', ROM_NAME='result.gba',
                                     OFFSET_TO_PUT=0x40), \
                 mock.patch.object(insert, 'GetSymbols', return_value=dict(symbols or {'dummy': 0})), \
                 mock.patch.object(insert, 'GetTextSection', return_value=0), \
                 contextlib.redirect_stdout(io.StringIO()):
                insert.main()
        finally:
            os.chdir(previous)

    def test_missing_symbols_abort_and_preserve_outputs(self):
        cases = {
            'hooks': 'dummy 08000010 0\nmissing 08000020 0\n',
            'repoints': 'dummy 08000010\nmissing 08000020\n',
            'repoints_slide': 'dummy 08000010 1\nmissing 08000020 1\n',
            'generatedrepoints': 'dummy 16\nmissing 32\n',
            'repointall': 'dummy 08000010\nmissing 08000020\n',
            'routinepointers': 'dummy 08000010\nmissing 08000020\n',
            'functionrewrites': 'dummy 08000010 0 1\nmissing 08000030 0 1\n',
            'eventscripts': 'map 0 0 dummy\nmap 0 0 missing\n',
            'songs': '0 dummy\n1 missing\n',
        }
        for name, lines in cases.items():
            with self.subTest(table=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                previous = self.prepare(root)
                name = 'repoints' if name == 'repoints_slide' else name
                (root / name).write_text(lines, encoding='utf-8')
                if name == 'generatedrepoints':
                    (root / 'repointall').write_text('missing 08000020\n', encoding='utf-8')
                if name in previous:
                    previous[name] = (root / name).read_bytes()
                source = (root / 'source.gba').read_bytes()
                with self.assertRaisesRegex(ValueError, 'missing'):
                    self.invoke(root)
                for filename, data in previous.items():
                    self.assertEqual((root / filename).read_bytes(), data)
                self.assertEqual((root / 'source.gba').read_bytes(), source)
                self.assertFalse(list(root.glob('.cfru-insert-*')))

    def test_publication_failure_restores_rom_and_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            previous = self.prepare(root)
            replace = os.replace

            def fail_rom_publish(source, destination):
                if Path(source).name == 'rom' and Path(destination) == root / 'result.gba':
                    raise PermissionError('simulated ROM publication failure')
                return replace(source, destination)

            with mock.patch.object(insert.os, 'replace', side_effect=fail_rom_publish):
                with self.assertRaisesRegex(PermissionError, 'publication failure'):
                    self.invoke(root)
            for filename, data in previous.items():
                self.assertEqual((root / filename).read_bytes(), data)

    def test_publication_failure_removes_new_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            previous = self.prepare(root)
            for name in ('generatedrepoints', 'offsets.ini'):
                (root / name).unlink()
            replace = os.replace

            def fail_rom_publish(source, destination):
                if Path(source).name == 'rom' and Path(destination) == root / 'result.gba':
                    raise PermissionError('simulated ROM publication failure')
                return replace(source, destination)

            with mock.patch.object(insert.os, 'replace', side_effect=fail_rom_publish):
                with self.assertRaises(PermissionError):
                    self.invoke(root)
            self.assertEqual((root / 'result.gba').read_bytes(), previous['result.gba'])
            self.assertFalse((root / 'generatedrepoints').exists())
            self.assertFalse((root / 'offsets.ini').exists())

    def test_failed_rollback_retains_old_rom_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            previous = self.prepare(root)
            replace = os.replace

            def fail_rom_publish_and_restore(source, destination):
                if Path(destination) == root / 'result.gba':
                    raise PermissionError('simulated publication and restoration failure')
                return replace(source, destination)

            with mock.patch.object(insert.os, 'replace', side_effect=fail_rom_publish_and_restore):
                with self.assertRaisesRegex(RuntimeError, 'previous outputs are retained'):
                    self.invoke(root)
            retained = list(root.glob('.cfru-insert-*'))
            self.assertEqual(len(retained), 1)
            self.assertEqual((retained[0] / 'rom.previous').read_bytes(), previous['result.gba'])
            self.assertEqual((root / 'generatedrepoints').read_bytes(), previous['generatedrepoints'])
            self.assertEqual((root / 'offsets.ini').read_bytes(), previous['offsets.ini'])

    def test_source_cannot_be_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'source.gba').write_bytes(b'original')
            with mock.patch.multiple(insert, SOURCE_ROM=str(root / 'source.gba'),
                                     ROM_NAME=str(root / 'source.gba')):
                with self.assertRaisesRegex(ValueError, 'different paths'):
                    insert.main()
            self.assertEqual((root / 'source.gba').read_bytes(), b'original')
            with mock.patch.multiple(insert, SOURCE_ROM=str(root / 'source.gba'),
                                     GENERATED_REPOINTS=str(root / 'source.gba'),
                                     ROM_NAME=str(root / 'result.gba')):
                with self.assertRaisesRegex(ValueError, 'metadata output'):
                    insert.main()
            self.assertEqual((root / 'source.gba').read_bytes(), b'original')

    def test_cached_repoints_do_not_enable_disabled_features(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root)
            (root / 'generatedrepoints').write_text('dummy 96\nremoved_symbol 100\n', encoding='utf-8')
            (root / 'repointall').write_text('#include "config.h"\n#ifdef FEATURE\ndummy 08000010\n#endif\n',
                                           encoding='utf-8')
            (root / 'config.h').write_text('#define FEATURE\n', encoding='utf-8')
            self.invoke(root)
            self.assertEqual((root / 'result.gba').read_bytes()[96:100],
                             (0x08000040).to_bytes(4, 'little'))
            self.assertNotIn('removed_symbol', (root / 'generatedrepoints').read_text())
            (root / 'config.h').write_text('// disabled\n', encoding='utf-8')
            self.invoke(root)
            self.assertEqual((root / 'result.gba').read_bytes()[96:100],
                             (root / 'source.gba').read_bytes()[96:100])
            self.assertNotIn('dummy', (root / 'generatedrepoints').read_text())

    def test_invalid_rows_preserve_outputs(self):
        cases = [('bytereplacement', '08200000 FE\n'),
                 ('bytereplacement', '08000000 FE invalid\n'),
                 ('repoints', 'dummy 08000010 extra invalid\n'),
                 ('eventscripts', 'invalid-row\n'),
                 ('eventscripts', 'unknown 0 0 dummy\n'),
                 ('songs', 'invalid-number dummy\n'), ('songs', '0\n')]
        for table, row in cases:
            with self.subTest(table=table, row=row), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                previous = self.prepare(root)
                (root / table).write_text(row, encoding='utf-8')
                with self.assertRaises(ValueError):
                    self.invoke(root)
                for filename, data in previous.items():
                    self.assertEqual((root / filename).read_bytes(), data)

    def test_cache_reuse_and_source_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root)
            (root / 'repointall').write_text('dummy 08000010\n', encoding='utf-8')
            self.invoke(root)
            with mock.patch.object(insert, 'RealRepoint', side_effect=AssertionError('cache not reused')):
                self.invoke(root)
            source = bytearray((root / 'source.gba').read_bytes())
            source[96:100] = (0x080001c0).to_bytes(4, 'little')
            source[104:108] = source[16:20]
            (root / 'source.gba').write_bytes(source)
            with mock.patch.object(insert, 'RealRepoint', wraps=insert.RealRepoint) as scan:
                self.invoke(root)
                self.assertEqual(scan.call_count, 1)
            result = (root / 'result.gba').read_bytes()
            self.assertEqual(result[96:100], source[96:100])
            self.assertEqual(result[104:108], (0x08000040).to_bytes(4, 'little'))

    def test_changed_request_and_corrupt_cache_rescan(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root)
            (root / 'repointall').write_text('dummy 08000010\n', encoding='utf-8')
            self.invoke(root)
            (root / 'repointall').write_text('dummy 08000014\n', encoding='utf-8')
            self.invoke(root)
            result = (root / 'result.gba').read_bytes()
            self.assertEqual(result[96:100], (root / 'source.gba').read_bytes()[96:100])
            self.assertEqual(result[100:104], (0x08000040).to_bytes(4, 'little'))
            cache = root / 'generatedrepoints'
            cache.write_text(cache.read_text(encoding='utf-8') + 'dummy 108\n', encoding='utf-8')
            with mock.patch.object(insert, 'RealRepoint', wraps=insert.RealRepoint) as scan:
                self.invoke(root)
                self.assertEqual(scan.call_count, 1)
            self.assertEqual((root / 'result.gba').read_bytes(), result)

    def test_event_kinds_patch_correct_script_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root)
            source = bytearray((root / 'source.gba').read_bytes())
            for offset, target in ((0x124, 0x180), (0x184, 0x300), (0x18c, 0x400), (0x190, 0x500)):
                source[offset:offset + 4] = (0x08000000 + target).to_bytes(4, 'little')
            source[0x180:0x184] = bytes((3, 0, 2, 2))
            (root / 'source.gba').write_bytes(source)
            (root / 'eventscripts').write_text(
                'map 0 0 dummy\nnpc 0 0 0 dummy\ntrainer 0 0 1 Battle\n'
                'item 0 0 2 Ball\ntile 0 0 1 dummy\nsign 0 0 1 dummy\n', encoding='utf-8')
            self.invoke(root, {'dummy': 0, 'EventScript_Battle': 4, 'ItemFindScript_Ball': 8})
            result = (root / 'result.gba').read_bytes()
            for offset, target in ((0x128, 0x40), (0x310, 0x40), (0x328, 0x44),
                                   (0x340, 0x48), (0x41c, 0x40), (0x514, 0x40)):
                self.assertEqual(result[offset:offset + 4], (0x08000000 + target).to_bytes(4, 'little'))
            self.assertEqual((root / 'source.gba').read_bytes(), source)
            for kind, index in (('npc', 3), ('tile', 2), ('sign', 2)):
                previous = (root / 'result.gba').read_bytes()
                (root / 'eventscripts').write_text('{} 0 0 {} dummy\n'.format(kind, index), encoding='utf-8')
                with self.assertRaises(ValueError):
                    self.invoke(root)
                self.assertEqual((root / 'result.gba').read_bytes(), previous)


if __name__ == '__main__':
    unittest.main()
