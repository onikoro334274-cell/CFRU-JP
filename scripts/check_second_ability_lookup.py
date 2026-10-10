"""Execute the production binary search across all u16 species identifiers."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/second_ability.c').read_text(encoding='utf-8')
    start = source.index('u16 SecondAbility_OfSpecies(')
    function = source[start:source.index('/*', start)]
    fixture = '''
typedef unsigned short u16;
typedef unsigned int u32;
#include "include/constants/species.h"
#include "include/constants/abilities.h"
#include "src/Tables/second_abilities.h"
static u16 expected[65536];
FUNCTION
int main(void) {
    for (u32 i = 0; i < SECOND_ABILITY_COUNT; i++) {
        u16 species = sSecondAbilities[i][0], ability = sSecondAbilities[i][1];
        if (!species || species >= NUM_SPECIES || !ability) return 1;
        if (i && sSecondAbilities[i-1][0] >= species) return 2;
        expected[species] = ability;
    }
    for (u32 species = 0; species < 65536; species++)
        if (SecondAbility_OfSpecies(species) != expected[species]) return 3;
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('lookup' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Second ability lookup: all 65536 species identifiers match the recovered table.')


if __name__ == '__main__':
    main()
