"""Verify drink choices against the supplied BPRJ ROM and execute all script IDs."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', type=Path, default=Path(r'C:\Users\liru55\Documents\PokemonStart解凍\Pocket Monsters - FireRed (Japan).gba'))
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    assert len(rom) == 0x1000000 and rom[0xAC:0xB0] == b'BPRJ' and rom[0xBC] == 0
    source = (ROOT / 'src/hack_thirsty_girl.c').read_text(encoding='utf-8')
    table = re.search(r'static const struct[^;]+;[^=]*=\s*\{[\s\S]*?\n\};', source)[0]
    pairs = [(int(p, 16), int(n)) for p, n in re.findall(r'\{(0x[0-9A-Fa-f]+),\s*(\d+)\}', table)]
    assert len(pairs) == 6 and all(0 < n <= 7 for _, n in pairs)
    descriptors = b''.join(struct.pack('<II', p, n) for p, n in pairs)
    assert rom.count(descriptors) == 1
    offset = rom.index(descriptors)
    native_table = 0x08000000 + offset - 0x20 * 8
    assert rom.count(struct.pack('<I', native_table)) >= 2
    rows = []
    for pointer, count in pairs:
        start = pointer - 0x08000000
        assert 0 <= start <= len(rom) - count * 8
        entries = [struct.unpack_from('<II', rom, start + i * 8) for i in range(count)]
        for name, unused in entries:
            address = name - 0x08000000
            assert 0 <= address < len(rom) and b'\xff' in rom[address:address + 64]
            assert unused == 0
        rows.append([name for name, _ in entries])
    water, soda, milk, cancel = rows[0][0], rows[1][0], rows[3][0], rows[0][-1]
    assert len({water, soda, milk, cancel}) == 4
    assert rows == [[water, cancel], [soda, cancel], [water, soda, cancel],
                    [milk, cancel], [water, milk, cancel], [soda, milk, cancel]]
    production = function(source, 'void HackThirstyGirl_LoadChoices(void)')
    production = production.replace('(const struct ListMenuItem*) sThirstyGirlLists[k].list',
                                    'ResolveList(sThirstyGirlLists[k].list)')
    row_data = ',\n'.join('{' + ','.join('{(const u8*)(unsigned long long)' + hex(n) + ',0}' for n in row) + '}' for row in rows)
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
struct ListMenuItem {const u8 *name;int id;};
static struct ListMenuItem gMultiChoice[7];
static u16 Var8004,Var8006;
TABLE
static const struct ListMenuItem rows[6][3]={ROWS};
static unsigned resolves,errors;
static const struct ListMenuItem *ResolveList(u32 pointer){
    resolves++;for(unsigned i=0;i<6;i++) if(sThirstyGirlLists[i].list==pointer) return rows[i];
    errors++;return rows[0];
}
PRODUCTION
int main(void){
    for(unsigned input=0;input<65536;input++){
        for(unsigned i=0;i<7;i++){gMultiChoice[i].name=(const u8*)123;gMultiChoice[i].id=99;}
        Var8004=input;Var8006=99;resolves=errors=0;HackThirstyGirl_LoadChoices();
        int valid=input>=0x20&&input<=0x25;unsigned row=valid?input-0x20:0;
        unsigned count=valid?sThirstyGirlLists[row].count:0;
        if(resolves!=(unsigned)valid||errors||Var8006!=(valid?0:99)) return 1;
        for(unsigned i=0;i<7;i++){
            if(gMultiChoice[i].name!=(i<count?rows[row][i].name:(const u8*)123)) return 2;
            if(gMultiChoice[i].id!=(i<count?(int)i:99)) return 3;
        }
    }
    return 0;
}
'''.replace('TABLE', table).replace('ROWS', row_data).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('thirsty' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {
        'scope': 'HackThirstyGirl_LoadChoices in src/hack_thirsty_girl.c',
        'source_sha256': hashlib.sha256((ROOT / 'src/hack_thirsty_girl.c').read_bytes()).hexdigest(),
        'rom_sha256': hashlib.sha256(rom).hexdigest(),
        'rom': 'Supplied read-only BPRJ revision 0, 16 MiB',
        'native_descriptor_table': hex(native_table),
        'verification': 'Six native descriptor pointers/counts match one contiguous ROM sequence; text pointers, terminators and drink/cancel combinations validated. All 65536 script inputs preserve untouched menu slots and invalid-input width state; valid entries copy names and assign selection IDs.',
        'change': 'No handler fix indicated by these checks.',
        'limitations': 'Native fixture substitutes ROM address resolution only. Actual multichoice drawing, widths, event script rewards and linked GBA behavior remain unverified.',
    }
    (ROOT / 'audit/thirsty_girl_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Drink choices: native BPRJ tables and all 65536 script inputs pass.')


if __name__ == '__main__':
    main()
