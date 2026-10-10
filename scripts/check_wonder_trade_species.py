"""Check accepted random distribution and bounded invalid-species retries."""
import re
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_wonder_trade.c').read_text(encoding='utf-8')
    production = function(source, 'static u16 GetRandomWonderTradeSpecies(void)')
    config = (ROOT / 'src/config.h').read_text(encoding='utf-8')
    dex_count = int(re.search(r'^#define NATIONAL_DEX_COUNT (\d+)', config, re.M)[1])
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#include "include/constants/species.h"
#define NATIONAL_DEX_COUNT @DEX_COUNT@
static struct {u8 baseHP;} gBaseStats[NUM_SPECIES];
static u32 draws,mappings,firstDex,mode;static u16 firstRoll;
static u32 histogram[NATIONAL_DEX_COUNT+1];
static u16 Random(void) {return draws++==0 || mode==5 ? firstRoll : 0;}
static u16 NationalPokedexNumToSpecies(u16 dex) {
    if(!mappings++) firstDex=dex;
    if(mode==1) return SPECIES_NONE;
    if(mode==2) return SPECIES_EGG;
    if(mode==3) return NUM_SPECIES;
    return SPECIES_PIKACHU;
}
PRODUCTION
int main(void) {
    gBaseStats[SPECIES_PIKACHU].baseHP=35;
    u32 complete=65536/NATIONAL_DEX_COUNT*NATIONAL_DEX_COUNT;
    for(u32 roll=0;roll<65536;roll++) {
        firstRoll=roll;draws=mappings=firstDex=mode=0;
        if(GetRandomWonderTradeSpecies()!=SPECIES_PIKACHU) return 1;
        if(roll<complete) {
            if(draws!=1 || mappings!=1 || firstDex!=1+roll%NATIONAL_DEX_COUNT) return 2;
            histogram[firstDex]++;
        } else if(draws!=2 || mappings!=1 || firstDex!=1) return 3;
    }
    for(u32 dex=1;dex<=NATIONAL_DEX_COUNT;dex++)
        if(histogram[dex]!=65536/NATIONAL_DEX_COUNT) return 4;
    firstRoll=0;
    for(mode=1;mode<=4;mode++) {
        draws=mappings=0;gBaseStats[SPECIES_PIKACHU].baseHP=mode==4 ? 0 : 35;
        if(GetRandomWonderTradeSpecies()!=SPECIES_PIKACHU || draws!=20 || mappings!=20) return 5;
    }
    mode=5;firstRoll=65535;draws=mappings=0;
    if(GetRandomWonderTradeSpecies()!=SPECIES_PIKACHU || draws!=20 || mappings) return 6;
    return 0;
}
'''.replace('PRODUCTION', production).replace('@DEX_COUNT@', str(dex_count))
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('wonder_species' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print(f'Wonder species: all 65536 initial draws, equal accepted coverage for {dex_count} dex entries and five retry-limit cases passed.')


if __name__ == '__main__':
    main()
