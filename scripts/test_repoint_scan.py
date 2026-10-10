"""Compare block-based ROM repointing against a sequential reference."""

import io
import random
import struct
import unittest
from unittest import mock

import insert


def reference(data, requests, ignored):
    data = bytearray(data)
    pointers = {}
    for offset, target, symbol in requests:
        pointers[int.from_bytes(data[offset:offset + 4], 'little')] = (target + 0x08000000, symbol)
    results = []
    for offset in range(0, min(len(data), 0x1000000) - 3, 4):
        if offset in ignored:
            continue
        word = int.from_bytes(data[offset:offset + 4], 'little')
        if word in pointers:
            target, symbol = pointers[word]
            data[offset:offset + 4] = target.to_bytes(4, 'little')
            results.append((offset, symbol))
    return bytes(data), results


class RepointScanTests(unittest.TestCase):
    def test_randomized_reference_and_duplicate_targets(self):
        rng = random.Random(314159)
        for trial in range(100):
            words = [0x08000000 + rng.randrange(1, 24) * 4 for _ in range(128)]
            data = struct.pack('<128I', *words)
            requests = [(rng.randrange(128) * 4, rng.randrange(32) * 4, str(i)) for i in range(6)]
            ignored = {0, 32, 100}
            expected_data, expected_results = reference(data, requests, ignored)
            rom = io.BytesIO(data)
            with mock.patch.object(insert, 'IGNORED_OFFSETS', list(ignored)):
                actual = insert.RealRepoint(rom, requests)
            self.assertEqual(actual, expected_results, trial)
            self.assertEqual(rom.getvalue(), expected_data, trial)

    def test_chunk_boundary_alignment_and_partial_eof(self):
        data = bytearray(65540 + 3)
        pointer = (0x08012340).to_bytes(4, 'little')
        for offset in (0, 65532, 65536):
            data[offset:offset + 4] = pointer
        data[17:21] = pointer
        rom = io.BytesIO(data)
        self.assertEqual(insert.RealRepoint(rom, [(0, 0x100, 'target')]),
                         [(0, 'target'), (65532, 'target'), (65536, 'target')])
        self.assertEqual(rom.getvalue()[17:21], pointer)
        self.assertEqual(len(rom.getvalue()), len(data))

    def test_scan_stops_at_original_16_mib_boundary(self):
        data = bytearray(0x1000008)
        pointer = (0x08012340).to_bytes(4, 'little')
        for offset in (0, 0xfffffc, 0x1000000):
            data[offset:offset + 4] = pointer
        rom = io.BytesIO(data)
        self.assertEqual(insert.RealRepoint(rom, [(0, 0x100, 'target')]),
                         [(0, 'target'), (0xfffffc, 'target')])
        self.assertEqual(rom.getvalue()[0x1000000:0x1000004], pointer)

    def test_invalid_source_and_null_pointer_fail(self):
        for offset in (1, 4):
            with self.assertRaises(ValueError):
                insert.RealRepoint(io.BytesIO(b'\x01\x00\x00\x08'), [(offset, 1, 'x')])
        with self.assertRaisesRegex(ValueError, 'null pointer'):
            insert.RealRepoint(io.BytesIO(bytes(16)), [(0, 1, 'x')])


if __name__ == '__main__':
    unittest.main()
