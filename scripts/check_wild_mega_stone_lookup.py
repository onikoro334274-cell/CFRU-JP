"""Execute wild held-stone lookup for every u16 species identifier."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/wild_mega_stone.c').read_text(encoding='utf-8'),
                          'u16 WildMegaStoneFor(u16 species)')
    fixture = r'''
typedef unsigned short u16;typedef unsigned int u32;
#include "include/constants/items.h"
#include "include/constants/species.h"
#include "src/Tables/wild_mega_stones.h"
static u16 expected[65536];
PRODUCTION
int main(void) {
    for(u32 i=0;i<WILD_MEGA_STONE_COUNT;i++) {
        u16 species=sWildMegaStones[i][0],item=sWildMegaStones[i][1];
        if(!species || species>=NUM_SPECIES || !item) return 1;
        if(i && sWildMegaStones[i-1][0]>=species) return 2;
        expected[species]=item;
    }
    for(u32 species=0;species<65536;species++)
        if(WildMegaStoneFor(species)!=expected[species]) return 3;
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('wild_stones' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Wild stone lookup: all 65536 species identifiers match the recovered table.')


if __name__ == '__main__':
    main()
