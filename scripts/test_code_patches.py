"""Check emitted Thumb branches and reject invalid patch parameters."""

import io
import unittest

import insert


class CodePatchTests(unittest.TestCase):
    def test_hook_literal_and_register_for_both_alignments(self):
        for offset in (0, 2, 3):
            for register in range(8):
                for target in (0x40, 0x41):
                    rom = io.BytesIO(bytes(64))
                    insert.Hook(rom, target, offset, register)
                    aligned = offset & ~1
                    raw = rom.getvalue()[aligned:]
                    # Decode LDR literal address and BX register, independently of generator.
                    ldr = int.from_bytes(raw[:2], 'little')
                    literal = ((aligned + 4) & ~3) + (ldr & 255) * 4
                    self.assertEqual((ldr >> 8) & 7, register)
                    self.assertEqual(int.from_bytes(rom.getvalue()[literal:literal + 4], 'little'),
                                     0x08000041)
                    self.assertEqual(int.from_bytes(raw[2:4], 'little'), 0x4700 | register << 3)

    def test_invalid_hook_does_not_write(self):
        for target, offset, register in ((0, 0, -1), (0, 0, 8), (-1, 0, 0),
                                         (0x2000000, 0, 0), (0, -2, 0), (0, 60, 0)):
            rom = io.BytesIO(bytes(64))
            with self.assertRaises(ValueError):
                insert.Hook(rom, target, offset, register)
            self.assertEqual(rom.getvalue(), bytes(64))

    def test_wrapper_thumb_target(self):
        for parameters in range(126):
            for mode in (0, 1):
                for target in (0x40, 0x41):
                    rom = io.BytesIO(bytes(2048))
                    insert.FunctionWrap(rom, target, 0, parameters, mode)
                    end = rom.tell()
                    self.assertEqual(rom.getvalue()[end - 4:end], b'\x41\x00\x00\x08')
                    literal_loads = []
                    for offset in range(0, end - 4, 2):
                        instruction = int.from_bytes(rom.getvalue()[offset:offset + 2], 'little')
                        if instruction & 0xf800 == 0x4800:
                            literal_loads.append(((offset + 4) & ~3) + (instruction & 255) * 4)
                    self.assertEqual(literal_loads, [end - 4])
                    branches = []
                    for offset in range(0, end - 6, 2):
                        high = int.from_bytes(rom.getvalue()[offset:offset + 2], 'little')
                        low = int.from_bytes(rom.getvalue()[offset + 2:offset + 4], 'little')
                        if high & 0xf800 == 0xf000 and low & 0xf800 == 0xf800:
                            displacement = ((high & 0x7ff) << 12) | ((low & 0x7ff) << 1)
                            if displacement & 0x400000:
                                displacement -= 0x800000
                            branches.append(offset + 4 + displacement)
                    self.assertEqual(branches, [end - 6])
                    self.assertEqual(rom.getvalue()[end - 6:end - 4], b'\x20\x47')

    def test_invalid_wrapper_does_not_write(self):
        for target, offset, parameters, mode in ((0, 2, 0, 1), (0, 0, -1, 1),
                                                (0, 0, 126, 1), (0, 0, 1, 2),
                                                (-1, 0, 0, 1), (0, 60, 0, 1)):
            rom = io.BytesIO(bytes(64))
            with self.assertRaises(ValueError):
                insert.FunctionWrap(rom, target, offset, parameters, mode)
            self.assertEqual(rom.getvalue(), bytes(64))


if __name__ == '__main__':
    unittest.main()
