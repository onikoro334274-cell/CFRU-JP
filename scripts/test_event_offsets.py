"""Resolve event script pointers with map/table count and ROM-range checks."""

import io
import unittest

import insert


def fixture():
    data = bytearray(0x54b10)
    for offset, target in ((0x54b0c, 0x100), (0x100, 0x120), (0x120, 0x140),
                           (0x144, 0x180), (0x184, 0x200), (0x18c, 0x240), (0x190, 0x280)):
        data[offset:offset + 4] = (0x08000000 + target).to_bytes(4, 'little')
    data[0x180:0x184] = bytes((2, 0, 2, 2))
    return data


class EventOffsetTests(unittest.TestCase):
    def test_each_event_layout_and_last_valid_index(self):
        rom = io.BytesIO(fixture())
        for kind, expected in (('map', 0x148), ('npc', 0x228), ('trainer', 0x228),
                               ('item', 0x228), ('tile', 0x25c), ('sign', 0x294)):
            self.assertEqual(insert._event_script_offset(rom, 0, 0, kind, 1, {}), expected)

    def test_negative_and_excess_indices(self):
        for kind in ('npc', 'trainer', 'item', 'tile', 'sign'):
            for index in (-1, 2, 999):
                with self.subTest(kind=kind, index=index), self.assertRaises(ValueError):
                    insert._event_script_offset(io.BytesIO(fixture()), 0, 0, kind, index, {})

    def test_map_ids_and_unknown_kind(self):
        for bank, number, kind in ((-1, 0, 'map'), (256, 0, 'map'),
                                   (0, -1, 'map'), (0, 256, 'map'), (0, 0, 'unknown')):
            with self.assertRaises(ValueError):
                insert._event_script_offset(io.BytesIO(fixture()), bank, number, kind, 0, {})

    def test_bad_header_pointers_and_truncated_tables(self):
        for pointer in (0, 0xf7f7f7f7, 0x08054b0e):
            data = fixture()
            data[0x54b0c:0x54b10] = pointer.to_bytes(4, 'little')
            with self.assertRaises(ValueError):
                insert._event_script_offset(io.BytesIO(data), 0, 0, 'map', 0, {})
        for kind, pointer_offset in (('npc', 0x184), ('tile', 0x18c), ('sign', 0x190)):
            data = fixture()
            data[pointer_offset:pointer_offset + 4] = (0x08000000 + len(data) - 8).to_bytes(4, 'little')
            with self.assertRaises(ValueError):
                insert._event_script_offset(io.BytesIO(data), 0, 0, kind, 0, {})

    def test_empty_event_tables(self):
        data = fixture()
        data[0x180:0x184] = bytes(4)
        for kind in ('npc', 'tile', 'sign'):
            with self.assertRaises(ValueError):
                insert._event_script_offset(io.BytesIO(data), 0, 0, kind, 0, {})


if __name__ == '__main__':
    unittest.main()
