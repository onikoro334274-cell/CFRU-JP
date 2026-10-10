import tempfile
import contextlib
import io
import unittest
from pathlib import Path
from unittest.mock import patch

import string_converter as converter


class StringConverterTests(unittest.TestCase):
    def test_spaces_and_quotes(self):
        self.assertEqual(converter.ProcessString(' \u3000', 1), '.byte 0x0, 0x0, ')
        byte = converter.PokeByteTableMaker()['"']
        self.assertEqual(converter.ProcessString('""', 1), '.byte ' + (hex(byte) + ', ') * 2)
        self.assertEqual(converter.ProcessString('""', 1, charMap={'\\"': 0xB2}),
                         '.byte 0xb2, 0xb2, ')

    def test_bad_escape_is_failure(self):
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as error:
            converter.ProcessString('\\q', 1)
        self.assertEqual(error.exception.code, 1)

    def test_unclosed_tokens_are_failure(self):
        for text in ('[BUFFER1', '\\'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                converter.ProcessString(text, 1)

    def test_charmap_is_loaded_once_and_false_does_not_pad(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.string'
            path.write_text('MAX_LENGTH=5\nFILL_FF=False\n#org @text\na\nb\n', encoding='utf-8')
            with patch.object(converter, 'PokeByteTableMaker', wraps=converter.PokeByteTableMaker) as load:
                converter.StringFileConverter(str(path))
                self.assertEqual(load.call_count, 1)
            output = path.with_suffix('.s').read_text()
            self.assertEqual(output.count('0xFF'), 2)


if __name__ == '__main__':
    unittest.main()
