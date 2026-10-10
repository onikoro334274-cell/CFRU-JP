import os
import contextlib
import io
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build
from build_dependencies import needs_rebuild

GNU_AS = os.environ.get('CFRU_TEST_ARM_AS') or shutil.which('arm-none-eabi-as')


class AssetBuildTests(unittest.TestCase):
    def test_audio_and_music_names_are_unique(self):
        for function in (build.MakeOutputAudioFile, build.MakeOutputMusicFile):
            first = function('audio/a/same.s')[0]
            second = function('audio/b/same.s')[0]
            self.assertNotEqual(first, second)
        self.assertTrue(Path(build.MakeOutputAudioFile('audio/plain.s')[0]).name.startswith('SND_'))

    @unittest.skipUnless(GNU_AS, 'ARM GNU assembler is required for asset pipeline integration')
    def test_cache_tracks_flags_commands_and_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original, assembly, flags = (root / name for name in ('asset.txt', 'asset.s', 'flags.txt'))
            converter = root / 'convert.py'
            original.write_text('1')
            flags.write_text('option')
            converter.write_text('import sys\nfrom pathlib import Path\n'
                                 'Path(sys.argv[2]).write_text(".syntax unified\\n.thumb\\nmovs r0, #" '
                                 '+ Path(sys.argv[1]).read_text() + "\\nbx lr\\n")\n')
            command = [sys.executable, str(converter), str(original), str(assembly)]
            def process(flag_file=str(flags), cmd=command):
                return build.DoMiddleManAssembly(str(original), str(assembly), flag_file, [], cmd,
                                                 build.MakeOutputImageFile, lambda: None, False)
            with patch.object(build, 'BUILD', directory), patch.object(build, 'AS', GNU_AS), \
                    patch.object(build, 'RunCommand', wraps=build.RunCommand) as run:
                output = Path(process())
                self.assertFalse(assembly.exists())
                process()
                self.assertEqual(run.call_count, 2)
                os.utime(flags, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                process()
                self.assertEqual(run.call_count, 4)
                flags.unlink()
                process('')
                self.assertEqual(run.call_count, 6)
                process('')
                self.assertEqual(run.call_count, 6)
                changed = command + ['new-option']
                process('', changed)
                self.assertEqual(run.call_count, 8)
                original.write_text('not-an-immediate')
                os.utime(original, ns=(output.stat().st_mtime_ns + 1000000000,) * 2)
                check_output = build.subprocess.check_output
                def capture_output(cmd):
                    return check_output(cmd, stderr=build.subprocess.STDOUT)
                with patch.object(build.subprocess, 'check_output', side_effect=capture_output), \
                        contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    process('', changed)
                self.assertFalse(Path(str(output) + '.cmd').exists())
                self.assertTrue(needs_rebuild(str(original), str(output), []))


if __name__ == '__main__':
    unittest.main()
