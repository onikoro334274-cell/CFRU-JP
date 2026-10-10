"""ROM data patches must validate complete input before changing bytes."""

import io
import unittest

import insert


class RomWriteTests(unittest.TestCase):
    def test_byte_patch_is_atomic_on_bad_token(self):
        for data in ('12 invalid', '12 100', '12 -1'):
            rom = io.BytesIO(b'original')
            with self.assertRaises(ValueError):
                insert.ReplaceBytes(rom, 0, data)
            self.assertEqual(rom.getvalue(), b'original')

    def test_byte_patch_bounds_and_exact_eof(self):
        for offset, data in ((-1, '12'), (7, '12 34'), (9, '')):
            rom = io.BytesIO(b'original')
            with self.assertRaises(ValueError):
                insert.ReplaceBytes(rom, offset, data)
            self.assertEqual(rom.getvalue(), b'original')
        rom = io.BytesIO(b'original')
        insert.ReplaceBytes(rom, 6, '12 34')
        self.assertEqual(rom.getvalue(), b'origin\x12\x34')
        insert.ReplaceBytes(rom, 8, '')
        self.assertEqual(len(rom.getvalue()), 8)

    def test_pointer_patch_bounds(self):
        for offset in (-1, 5, 8):
            rom = io.BytesIO(b'original')
            with self.assertRaises(ValueError):
                insert.Repoint(rom, 0x40, offset)
            self.assertEqual(rom.getvalue(), b'original')
        rom = io.BytesIO(b'original')
        insert.Repoint(rom, 0x40, 4, 1)
        self.assertEqual(rom.getvalue(), b'orig\x41\x00\x00\x08')


if __name__ == '__main__':
    unittest.main()
