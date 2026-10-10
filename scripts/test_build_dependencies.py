import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build
import string_converter
from build_dependencies import invalidate_command, needs_rebuild, prerequisites, record_command

GNU_AS = os.environ.get('CFRU_TEST_ARM_AS') or shutil.which('arm-none-eabi-as')
GNU_CC = os.environ.get('CFRU_TEST_ARM_GCC') or shutil.which('arm-none-eabi-gcc')


class DependencyTests(unittest.TestCase):
    @unittest.skipUnless(GNU_CC, 'ARM GNU compiler is required for compiler integration')
    def test_real_gnu_compiler_with_spaced_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / 'path with spaces'
            base.mkdir()
            source, header = base / 'file.c', base / 'value with spaces.h'
            source.write_text('#include "value with spaces.h"\nint value(void) {return VALUE;}\n')
            header.write_text('#define VALUE 1\n')
            tool_path = str(Path(GNU_CC).parent) + os.pathsep + os.environ.get('PATH', '')
            with patch.dict(os.environ, {'PATH': tool_path}), \
                    patch.object(build, 'BUILD', str(base)), patch.object(build, 'CC', GNU_CC), \
                    patch.object(build, 'RunCommand', wraps=build.RunCommand) as run:
                output = Path(build.ProcessC(str(source)))
                original = output.read_bytes()
                build.ProcessC(str(source))
                self.assertEqual(run.call_count, 1)
                header.write_text('#define VALUE 2\n')
                os.utime(header, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                build.ProcessC(str(source))
                self.assertEqual(run.call_count, 2)
                self.assertNotEqual(output.read_bytes(), original)

    @unittest.skipUnless(GNU_AS, 'ARM GNU assembler is required for assembly integration')
    def test_real_gnu_assembly_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, header = base / 'code.s', base / 'value.inc'
            source.write_text('.syntax unified\n.thumb\n.include "' + header.as_posix() + '"\n'
                              '.global test\ntest:\nmovs r0, #VALUE\nbx lr\n')
            header.write_text('.equ VALUE, 1\n')
            with patch.object(build, 'BUILD', directory), patch.object(build, 'AS', GNU_AS), \
                    patch.object(build, 'RunCommand', wraps=build.RunCommand) as run:
                output = Path(build.ProcessAssembly(str(source)))
                original = output.read_bytes()
                self.assertIn(header.as_posix(), prerequisites(str(output) + '.d'))
                build.ProcessAssembly(str(source))
                self.assertEqual(run.call_count, 1)
                header.write_text('.equ VALUE, 2\n')
                os.utime(header, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                build.ProcessAssembly(str(source))
                self.assertEqual(run.call_count, 2)
                self.assertNotEqual(output.read_bytes(), original)

    @unittest.skipUnless(shutil.which('clang'), 'Clang is required for assembler integration')
    def test_strings_rebuild_for_header_and_charmap_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, header, charmap = (base / name for name in ('text.string', 'config.h', 'charmap.tbl'))
            source.write_text('#include "' + header.as_posix() + '"\n#ifdef ONE\n#org @text\n\u3042\n'
                              '#else\n#org @text\n\u3044\n#endif\n', encoding='utf-8')
            header.write_text('#define ONE\n')
            charmap.write_text('01=\u3042\n02=\u3044\n', encoding='utf-8')
            with patch.object(build, 'BUILD', directory), patch.object(build, 'AS', shutil.which('clang')), \
                    patch.object(build, 'ASFLAGS', ['--target=arm-none-eabi', '-mcpu=arm7tdmi', '-mthumb']), \
                    patch.object(string_converter, 'CharMap', str(charmap)), \
                    patch.object(build, 'RunCommand', wraps=build.RunCommand) as run:
                output = Path(build.ProcessString(str(source)))
                original = output.read_bytes()
                build.ProcessString(str(source))
                self.assertEqual(run.call_count, 1)
                header.write_text('// ONE is disabled\n')
                os.utime(header, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                build.ProcessString(str(source))
                self.assertEqual(run.call_count, 2)
                self.assertNotEqual(output.read_bytes(), original)
                os.utime(header, ns=(10000000000,) * 2)
                build.ProcessString(str(source))
                self.assertEqual(run.call_count, 2)
                original = output.read_bytes()
                charmap.write_text('01=\u3042\n03=\u3044\n', encoding='utf-8')
                os.utime(charmap, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                build.ProcessString(str(source))
                self.assertEqual(run.call_count, 3)
                self.assertNotEqual(output.read_bytes(), original)

    def test_makefile_escaping(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.d'
            path.write_text('C:/build/file.o: src/file.c dir\\ with\\ spaces/a.h \\\n C:/include/b.h hash\\#name.h dollar$$name.h\n', encoding='utf-8')
            self.assertEqual(prerequisites(path), ['src/file.c', 'dir with spaces/a.h',
                                                  'C:/include/b.h', 'hash#name.h', 'dollar$name.h'])

    def test_invalidation(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, header, output = (base / name for name in ('file.c', 'file.h', 'file.o'))
            source.write_text('source')
            header.write_text('header')
            output.write_bytes(b'object')
            for path in (source, header):
                os.utime(path, ns=(10000000000, 10000000000))
            os.utime(output, ns=(20000000000, 20000000000))
            command = ['cc', '-O2']
            dep = Path(str(output) + '.d')
            dep.write_text(str(output) + ': ' + str(source).replace('\\', '/') + ' '
                           + str(header).replace('\\', '/') + '\n')
            self.assertTrue(needs_rebuild(str(source), str(output), command))
            record_command(output, command)
            self.assertFalse(needs_rebuild(str(source), str(output), command))
            invalidate_command(output)
            self.assertTrue(needs_rebuild(str(source), str(output), command))
            record_command(output, command)
            self.assertTrue(needs_rebuild(str(source), str(output), ['cc', '-O0']))
            os.utime(header, ns=(30000000000, 30000000000))
            self.assertTrue(needs_rebuild(str(source), str(output), command))
            header.unlink()
            self.assertTrue(needs_rebuild(str(source), str(output), command))
            dep.write_text('malformed')
            self.assertTrue(needs_rebuild(str(source), str(output), command))

    @unittest.skipUnless(shutil.which('clang'), 'Clang is required for compiler integration')
    def test_process_c_rebuilds_changed_header_and_flags(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, header = base / 'file.c', base / 'file.h'
            source.write_text('#include "file.h"\nint value(void) {return VALUE;}\n')
            header.write_text('#define VALUE 1\n')
            with patch.object(build, 'BUILD', directory), patch.object(build, 'CC', shutil.which('clang')), \
                    patch.object(build, 'CFLAGS', ['--target=arm-none-eabi', '-mcpu=arm7tdmi', '-mthumb', '-O2']), \
                    patch.object(build, 'RunCommand', wraps=build.RunCommand) as run:
                output = Path(build.ProcessC(str(source)))
                original = output.read_bytes()
                self.assertEqual(run.call_count, 1)
                build.ProcessC(str(source))
                self.assertEqual(run.call_count, 1)
                header.write_text('#define VALUE 2\n')
                os.utime(header, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                build.ProcessC(str(source))
                self.assertEqual(run.call_count, 2)
                self.assertNotEqual(output.read_bytes(), original)
                os.utime(header, ns=(10000000000,) * 2)
                build.CFLAGS = ['--target=arm-none-eabi', '-mcpu=arm7tdmi', '-mthumb', '-O0']
                build.ProcessC(str(source))
                self.assertEqual(run.call_count, 3)


if __name__ == '__main__':
    unittest.main()
