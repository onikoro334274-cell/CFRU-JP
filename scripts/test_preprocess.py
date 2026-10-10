import tempfile
import unittest
from pathlib import Path

from preprocess import TryProcessConditionalCompilation as conditional
from preprocess import TryProcessFileInclusion as inclusion
from preprocess import validate_definition_file


class ConditionalTests(unittest.TestCase):
    def test_nested_inactive_parent_stays_inactive(self):
        stack = []
        defines = {'B': True}
        for line in ('#ifdef A', '#ifdef B', '#else'):
            self.assertTrue(conditional(line, defines, stack))
        self.assertTrue(conditional('data', defines, stack))
        conditional('#endif', defines, stack)
        conditional('#else // comment', defines, stack)
        self.assertFalse(conditional('data', defines, stack))
        conditional('#endif', defines, stack)
        self.assertFalse(stack)

    def test_condition_is_evaluated_when_entered(self):
        stack, defines = [], {}
        conditional('#ifndef A', defines, stack)
        defines['A'] = True
        self.assertFalse(conditional('data', defines, stack))

    def test_malformed_directives_fail(self):
        for line in ('#else', '#endif', '#ifdef', '#if 0', '#elif A'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                conditional(line, {}, [])
        stack = []
        conditional('#ifdef A', {}, stack)
        conditional('#else', {}, stack)
        with self.assertRaises(ValueError):
            conditional('#else', {}, stack)

    def test_inactive_include_is_not_opened(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'table'
            path.write_text('#ifdef DISABLED\n#include "no-such-input.h"\n#endif\n')
            validate_definition_file(path)
            path.write_text('#ifdef DISABLED\n')
            with self.assertRaises(ValueError):
                validate_definition_file(path)
            path.write_text('#include "no-such-input.h"\n')
            with self.assertRaises(FileNotFoundError):
                validate_definition_file(path)

    def test_indented_utf8_definitions(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'header.h'
            header.write_text('// \u5b9a\u7fa9\n  #define FLAG // comment\n#define VALUE 0x42\n', encoding='utf-8')
            definitions = {}
            self.assertTrue(inclusion('  #include "' + header.as_posix() + '"', definitions))
            self.assertEqual(definitions, {'FLAG': True, 'VALUE': '0x42'})


if __name__ == '__main__':
    unittest.main()
