"""Execute production level unlock and gifted Pikachu provenance conditions."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def extract(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    source = (ROOT / 'src/second_ability.c').read_text(encoding='utf-8')
    functions = '\n'.join(extract(source, name) for name in
                          ('static bool8 IsMomPikachu(', 'bool8 SecondAbility_IsUnlocked('))
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#define FALSE 0
#define NULL ((void*)0)
#define MON_DATA_MET_LOCATION 1
#define MON_DATA_MET_LEVEL 2
#define MON_DATA_OT_ID 3
#include "include/constants/species.h"
#include "include/constants/abilities.h"
#include "include/constants/region_map_sections.h"
#include "src/Tables/second_abilities.h"
struct Pokemon {u32 metLocation, metLevel, otId;};
static struct {u8 playerTrainerId[4];} save;
#define gSaveBlock2 (&save)
static u32 GetMonData(struct Pokemon* mon, int field, void* ignored) {
    if(field == MON_DATA_MET_LOCATION) return mon->metLocation;
    if(field == MON_DATA_MET_LEVEL) return mon->metLevel;
    return mon->otId;
}
FUNCTIONS
int main(void) {
    for(int level=0; level<256; level++)
        if(SecondAbility_IsUnlocked(level) != (level >= SECOND_ABILITY_UNLOCK_LEVEL)) return 1;
    int forms[3]={SPECIES_PIKACHU,SPECIES_PIKACHU_POP_STAR,SPECIES_RAICHU};
    struct Pokemon mon;
    for(int value=0; value<256; value++) {
        save.playerTrainerId[0]=value;
        save.playerTrainerId[1]=255-value;
        save.playerTrainerId[2]=value ^ 0x55;
        save.playerTrainerId[3]=value ^ 0xAA;
        u32 owner=0;
        for(int byte=3; byte>=0; byte--) owner=owner*256u+save.playerTrainerId[byte];
        for(int form=0; form<3; form++) for(int location=0; location<256; location++)
        for(int level=0; level<256; level++) for(int own=0; own<2; own++) {
            mon.metLocation=location;mon.metLevel=level;mon.otId=owner ^ (own ? 0u : 1u);
            int expected=form<2 && location==MAPSEC_PALLET_TOWN && level!=0 && own;
            if(IsMomPikachu(forms[form],&mon)!=expected) return 2;
        }
        for(int form=0; form<3; form++) if(IsMomPikachu(forms[form],NULL)) return 3;
    }
    return 0;
}
'''.replace('FUNCTIONS', functions)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('conditions' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Second ability conditions: 256 unlock levels, 100663296 provenance combinations and 768 null cases pass.')


if __name__ == '__main__':
    main()
