"""Verify starter-only shiny rolls across maps, party state and RNG outcomes."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/hack_gift_shiny.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/hack_gift_shiny.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    maps = (ROOT / 'include/constants/maps.h').read_text(encoding='utf-8')
    definitions = '\n'.join(re.search(r'^#define ' + re.escape(name) + r'[^\n]*', maps, re.M)[0]
                            for name in ('MAP_GROUP(', 'MAP_NUM(', 'MAP_IS(', 'MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB '))
    definitions += '\n' + re.search(r'^#define GIFT_SHINY_ODDS[^\n]*', source, re.M)[0]
    for path, name in (('include/constants/flags.h', 'FLAG_SYS_POKEMON_GET'),
                       ('include/constants/flags.h', 'SYS_FLAGS'),
                       ('include/pokemon.h', 'MON_DATA_SPECIES'),
                       ('include/constants/species.h', 'SPECIES_NONE')):
        text = (ROOT / path).read_text(encoding='utf-8')
        definitions += '\n' + re.search(r'^#define ' + name + r'\s+[^\n]*', text, re.M)[0]
    production = function(source, 'bool8 HackGiftShiny_Roll(void)') + '\n' + function(source, 'bool8 HackGiftShiny_IsStarterRoll(void)')
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
DEFINITIONS
static struct {struct {signed char mapGroup,mapNum;} location;} save,*gSaveBlock1=&save;
static struct Pokemon {u16 species;} gPlayerParty[6];
static u16 randomValue;static int flag,randomCalls,monCalls,flagCalls,errors;
static u16 Random(void){randomCalls++;return randomValue;}
static bool8 FlagGet(u16 id){flagCalls++;if(id!=FLAG_SYS_POKEMON_GET) errors++;return flag;}
static u32 GetMonData(struct Pokemon *mon,int field,void *out){
    monCalls++;if(mon!=gPlayerParty||field!=MON_DATA_SPECIES||out!=NULL) errors++;
    return mon->species;
}
PRODUCTION
int main(void){
    for(unsigned map=0;map<65536;map++) for(flag=0;flag<2;flag++)
    for(int occupied=0;occupied<2;occupied++) for(int draw=0;draw<2;draw++){
        save.location.mapGroup=map>>8;save.location.mapNum=map&255;
        gPlayerParty[0].species=occupied?1:0;randomValue=draw;
        randomCalls=monCalls=flagCalls=errors=0;
        int location=map==0x0403,eligible=location&&flag&&!occupied;
        if(HackGiftShiny_IsStarterRoll()!=(eligible&&!draw)) return 1;
        if(randomCalls!=eligible||monCalls!=(location&&flag)||errors) return 2;
        if(gPlayerParty[0].species!=(occupied?1:0)) return 3;
    }
    unsigned successes=0;randomCalls=0;
    for(unsigned value=0;value<65536;value++){
        randomValue=value;int result=HackGiftShiny_Roll();
        if(result!=(value%16==0)) return 4;successes+=result;
    }
    if(successes!=4096||randomCalls!=65536) return 5;
    gSaveBlock1=NULL;randomCalls=monCalls=flagCalls=0;
    if(HackGiftShiny_IsStarterRoll()||randomCalls||monCalls||flagCalls) return 6;
    return 0;
}
'''.replace('DEFINITIONS', definitions).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('gift_scope' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Gift shiny scope: 524288 map/state/RNG cases and all 65536 random values pass; direct gift odds remain 1/16.')


if __name__ == '__main__':
    main()
