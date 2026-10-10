"""Compare the original BPRJ battle callback call sequence with linker symbols."""
import argparse
import hashlib
import json
import re
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
    linker = (ROOT / 'BPRJ.ld').read_text(encoding='utf-8')
    source = (ROOT / 'src/hack_battle_speed.c').read_text(encoding='utf-8')
    calls = [(0x080109C4, 'AnimateSprites'), (0x080109C8, 'BuildOamBuffer'),
             (0x080109CC, 'RunTextPrinters'), (0x080109D0, 'UpdatePaletteFade'),
             (0x080109D4, 'RunTasks'), (0x080109FE, 'ResetPaletteFadeControl'),
             (0x08010A0E, 'BeginNormalPaletteFade'), (0x08010A14, 'SetMainCallback2')]
    verified = []
    for address, name in calls:
        high, low = struct.unpack_from('<HH', rom, address - 0x08000000)
        if high & 0xF800 != 0xF000 or low & 0xF800 != 0xF800:
            raise ValueError(('Not a Thumb BL', hex(address)))
        displacement = ((high & 0x7FF) << 12) | ((low & 0x7FF) << 1)
        if displacement & 0x400000:
            displacement -= 0x800000
        target = address + 4 + displacement
        expected = int(re.search(r'^' + name + r'\s*=\s*(0x[0-9A-Fa-f]+)', linker, re.M)[1], 16)
        if target != expected:
            raise ValueError((name, hex(target), hex(expected)))
        verified.append({'address': hex(address), 'symbol': name, 'target': hex(target)})
    body = source[source.index('static void BattleMainCB2_Body'):source.index('void HackBattleSpeed_BattleMainCB2')]
    positions = [body.index(name + '(') for _, name in calls]
    if positions != sorted(positions):
        raise ValueError('Replacement callback call order differs')
    quit_pointer = struct.unpack_from('<I', rom, 0x10A30)[0]
    if quit_pointer != 0x08010A7D or '(0x08010A7C | 1)' not in source:
        raise ValueError('Pokedude quit callback differs')
    if 'HackBattleSpeed_BattleMainCB2 080109C0 0 0' not in (ROOT / 'functionrewrites').read_text(encoding='utf-8'):
        raise ValueError('Missing callback rewrite registration')
    report = {'scope': 'Original BPRJ battle callback routing and replacement body order',
              'rom_sha256': hashlib.sha256(rom).hexdigest(), 'calls': verified,
              'quit_callback': hex(quit_pointer),
              'limitations': 'Original instruction targets and source registration only; emitted rewrite bytes, linked payload, registers and GBA runtime remain unverified.'}
    (ROOT / 'audit/rom_battle_callback.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('BPRJ battle callback: eight BL targets, body order, quit pointer and rewrite registration match.')


if __name__ == '__main__':
    main()
