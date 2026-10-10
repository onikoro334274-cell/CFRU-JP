"""Execute BPRJ's actual rare-candy level-effect instructions with current patches."""
import argparse
import json
from pathlib import Path
import re
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R8, UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_LR

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', required=True)
    rom = bytearray(Path(parser.parse_args().rom).read_bytes())
    assert rom[0xAC:0xB0] == b'BPRJ' and rom[0xBC] == 0
    assert rom[0x40F24:0x40F28] == bytes.fromhex('64 28 24 D0')
    patches = (ROOT / 'bytereplacement').read_text(encoding='utf-8')
    for address in (0x08040F24, 0x08040F50):
        match = re.search(r'^%08X\s+([0-9A-Fa-f ]+)$' % address, patches, re.M)
        assert match, hex(address)
        payload = bytes.fromhex(match[1])
        assert len(payload) == (2 if address == 0x08040F24 else 4)
        rom[address - 0x08000000:address - 0x08000000 + len(payload)] = payload
    max_level = int(re.search(r'#define\s+MAX_LEVEL\s+(\d+)', (ROOT / 'src/config.h').read_text(encoding='utf-8'))[1])
    assert rom[0x40F24:0x40F26] == bytes((max_level, 0x28))
    # Controlled expanded tables; run the original arithmetic and patched stride.
    table, stats = 0x02010000, 0x02018000
    struct.pack_into('<I', rom, 0x4108C, table)
    struct.pack_into('<I', rom, 0x41090, stats)
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    uc.mem_map(0x08000000, 0x1000000)
    uc.mem_write(0x08000000, bytes(rom))
    uc.mem_map(0x02000000, 0x40000)
    uc.mem_write(table, b''.join(struct.pack('<I', growth * 1000000 + level * 1000)
                               for growth in range(6) for level in range(256)))
    state = {}

    def hook(machine, address, size, data):
        if address == 0x0803F354:
            field = machine.reg_read(UC_ARM_REG_R1)
            assert machine.reg_read(UC_ARM_REG_R0) == 0x02001000
            assert field in (56, 11)
            machine.reg_write(UC_ARM_REG_R0, state['level'] if field == 56 else 1)
        elif address == 0x0803FA70:
            assert machine.reg_read(UC_ARM_REG_R1) == 25
            value = struct.unpack('<I', machine.mem_read(machine.reg_read(UC_ARM_REG_R2), 4))[0]
            state['writes'].append(value)
        elif address == 0x0803DBE8:
            state['calculations'] += 1
        else:
            return
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, hook)
    for growth in range(6):
        uc.mem_write(stats + 28 + 19, bytes((growth,)))
        for level in range(1, max_level + 1):
            state.update(level=level, writes=[], calculations=0)
            uc.reg_write(UC_ARM_REG_R8, 0x02001000)
            uc.reg_write(UC_ARM_REG_SP, 0x0203F000)
            uc.emu_start(0x08040F1B, 0x08040F72, count=100)
            expected = [] if level == max_level else [growth * 1000000 + (level + 1) * 1000]
            assert state['writes'] == expected and state['calculations'] == bool(expected), (growth, level, state)
    report = {'result': 'passed', 'native_effect_cases': 6 * max_level,
              'scope': 'Unicorn executes supplied BPRJ rare-candy effect instructions with current cap/stride patches for six growth rates and levels 1..200. Checks exp-table addressing, one experience write/stat calculation below cap and no write at cap.',
              'limitations': 'Get/SetMonData and CalculateMonStats intercepted; controlled expanded tables. Whole linked ROM, native friendship/move/evolution chain and emulator gameplay remain unverified.'}
    (ROOT / 'audit/exp_candy_native_cap_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{6 * max_level} native instruction cap/stride cases passed')


if __name__ == '__main__':
    main()
