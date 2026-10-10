"""Check legacy tutor mapping and hook registration against the supplied BPRJ ROM."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    assert rom[0xAC:0xB0] == b'BPRJ' and rom[0xBC] == 0
    assert rom[0x121310:0x121318].hex() == 'f0b5474680b4051c'
    assert rom[0x121398:0x12139E].hex() == '00b50006010e'
    # Original GetTutorMove's table load and three exceptional branches.
    assert rom[0x1213C6:0x1213CE].hex() == '0348490009180888'
    assert rom[0x1213B2:0x1213B6].hex() == 'a9204000'
    assert rom[0x1213B8:0x1213BA].hex() == '0048'
    assert rom[0x1213C0:0x1213C4].hex() == '9a204000'
    pointer = struct.unpack_from('<I', rom, 0x1213D4)[0]
    offset = pointer - 0x08000000
    assert 0 <= offset <= len(rom) - 30
    original = list(struct.unpack_from('<15H', rom, offset))
    original += [0xA9 * 2, struct.unpack_from('<I', rom, 0x1213BC)[0], 0x9A * 2]
    source = (ROOT / 'src/hack_vanilla_tutor.c').read_text(encoding='utf-8')
    table = re.search(r'sVanillaTutorMoves\[[^]]+\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    constants = (ROOT / 'include/constants/moves.h').read_text(encoding='utf-8')
    ids = {name: int(value, 0) for name, value in re.findall(r'^#define\s+(MOVE_\w+)\s+(0x[0-9A-Fa-f]+|\d+)\b', constants, re.M)}
    names = re.findall(r'MOVE_\w+', table)
    assert [ids[name] for name in names] == original
    hooks = (ROOT / 'hooks').read_text(encoding='utf-8')
    for name, address, scratch in [('HackVanillaTutor_GetMove', 0x08121398, 1),
                                   ('HackVanillaTutor_CanMonLearnTMTutor', 0x08121310, 3)]:
        rows = re.findall(r'^' + name + r'\s+([0-9A-Fa-f]+)\s+(\d+)\s*$', hooks, re.M)
        assert len(rows) == 1 and (int(rows[0][0], 16), int(rows[0][1])) == (address, scratch)
    report = {'result': 'passed', 'rom_sha256': hashlib.sha256(rom).hexdigest(),
              'original_table_address': hex(pointer), 'vanilla_moves': dict(zip(names, original)),
              'scope': 'BPRJ rev0 original entry instructions, 15 table moves and 3 exceptional moves match current C table; unique hook registrations use r1 for one argument and r3 for three arguments.',
              'limitations': 'Checks source registrations and original ROM only; does not prove inserted wrapper bytes, compiled target addresses, compatibility tables or GBA execution.'}
    (ROOT / 'audit/rom_tutor_mapping_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('All 18 vanilla tutor moves match BPRJ ROM; both hook registrations passed')


if __name__ == '__main__':
    main()
