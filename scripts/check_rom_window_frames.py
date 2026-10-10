"""Validate original BPRJ frame clamp instructions and referenced asset extents."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', required=True)
    args = parser.parse_args()
    rom = Path(args.rom).read_bytes()
    if rom[0xAC:0xB0] != b'BPRJ' or rom[0xBC] != 0:
        raise ValueError('Expected original BPRJ revision zero')
    half = lambda address: struct.unpack_from('<H', rom, address - 0x08000000)[0]
    word = lambda address: struct.unpack_from('<I', rom, address - 0x08000000)[0]
    expected = {0x0806904A: 0x0600, 0x0806904C: 0x0E00,
                0x0806904E: 0x2813, 0x08069050: 0xD806,
                0x08069052: 0x00C0, 0x08069054: 0x4901,
                0x08069056: 0x1840, 0x08069058: 0xE003,
                0x08069060: 0x4801}
    for address, instruction in expected.items():
        if half(address) != instruction:
            raise ValueError(f'Unexpected frame routing instruction at {address:#x}')
    table = word(0x0806905C)
    if table != word(0x08069068):
        raise ValueError('Invalid-index fallback differs from first frame')
    assets = []
    for index in range(10):
        tiles = word(table + index * 8)
        palette = word(table + index * 8 + 4)
        for pointer, length in ((tiles, 0x120), (palette, 0x20)):
            offset = pointer - 0x08000000
            if offset < 0 or offset + length > len(rom) or pointer & 1:
                raise ValueError(('Invalid frame asset extent', index, pointer, length))
        assets.append({'index': index, 'tiles': hex(tiles), 'palette': hex(palette)})
    if not (word(table + 80) & 1 and word(table + 84) & 1):
        raise ValueError('Expected following Thumb function table not found')
    report = {
        'scope': 'Original BPRJ GetWindowFrameTilesPal at 0x08069048 and its referenced table',
        'rom_sha256': hashlib.sha256(rom).hexdigest(), 'table': hex(table), 'assets': assets,
        'verified': 'Thumb byte sequence masks the parameter to u8, accepts 0..19, indexes eight-byte entries and returns the first entry for larger IDs. Only ten frame entries are valid: all ten raw 288-byte tile and 32-byte palette extents lie within ROM.',
        'defect': 'Entry ten contains odd Thumb code pointers rather than frame assets. The option menu now clamps frame IDs to its verified ten-entry table before calling the original function.',
        'limitations': 'Instruction inspection and pointer extents only; actual frame drawing, changed linked-ROM routine and GBA runtime unverified.'}
    (ROOT / 'audit/rom_window_frames.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Original BPRJ: ten frame assets in bounds; vanilla upper bound incorrectly permits following data.')


if __name__ == '__main__':
    main()
