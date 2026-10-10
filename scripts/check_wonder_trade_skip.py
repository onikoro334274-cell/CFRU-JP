"""Execute scene-skip swap, rejection and Pokedex behavior with controlled mons."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/hack_wonder_trade.c').read_text(encoding='utf-8'),
                          'void WonderTrade_TrySkipScene(void)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#include "include/constants/items.h"
#include "src/Tables/hoenn_script_flags.h"
#define FALSE 0
#define NULL ((void*)0)
#define TRUE 1
#define PARTY_SIZE 6
#define SPECIES_NONE 0
#define EVO_MODE_TRADE 1
#define MON_DATA_HELD_ITEM 1
#define MON_DATA_IS_EGG 2
#define MON_DATA_SPECIES 3
#define MON_DATA_FRIENDSHIP 4
#define MON_DATA_NICKNAME 5
#define FLAG_SET_SEEN 1
#define FLAG_SET_CAUGHT 2
struct Pokemon {u32 personality;u16 species,item;u8 egg,friendship,nickname[8],extra[80];};
static struct Pokemon gPlayerParty[6],gEnemyParty[6];
static u16 Var8005,gSpecialVar_LastResult;static u8 gStringVar1[32];
static int enabled,evolves,dexCalls,errors;
static int FlagGet(u16 flag) {if(flag!=HACK_FLAG_WONDER_SKIP) errors++;return enabled;}
static u32 GetMonData(struct Pokemon* p,int field,void* dest) {
    if(field==1) return p->item;if(field==2) return p->egg;if(field==3) return p->species;
    if(field==5) {for(int i=0;i<8;i++) ((u8*)dest)[i]=p->nickname[i];return 0;}
    errors++;return 0;
}
static void SetMonData(struct Pokemon* p,int field,const u8* value) {if(field!=4) errors++;p->friendship=*value;}
static u16 GetEvolutionTargetSpecies(struct Pokemon* p,int mode,u16 item) {
    if(p!=gEnemyParty || mode!=1 || item!=ITEM_NONE) errors++;return evolves;
}
static u16 SpeciesToNationalPokedexNum(u16 species) {return species+100;}
static void HandleSetPokedexFlag(u16 dex,int flag,u32 personality) {
    if(dex!=302 || flag!=dexCalls+1 || personality!=222) errors++;dexCalls++;
}
PRODUCTION
static int same(const void* a,const void* b,u32 count) {
    for(u32 i=0;i<count;i++) if(((const u8*)a)[i]!=((const u8*)b)[i]) return 0;return 1;
}
int main(void) {
    for(u32 slot=0;slot<65536;slot++) for(int mode=0;mode<8;mode++) {
        enabled=!(mode&1);evolves=(mode&2)!=0;int egg=(mode&4)!=0;
        for(int mail=0;mail<3;mail++) {
            struct Pokemon mine={111,101,mail==0 ? ITEM_NONE : mail==1 ? ITEM_ORANGE_MAIL : ITEM_RETRO_MAIL,0,99,{1,2,0xFF},{17}};
            struct Pokemon theirs={222,202,ITEM_NONE,egg,33,{3,4,0xFF},{29}};
            for(int i=0;i<6;i++) {gPlayerParty[i]=mine;gEnemyParty[i]=theirs;}
            Var8005=slot;gSpecialVar_LastResult=77;dexCalls=errors=0;
            for(int i=0;i<32;i++) gStringVar1[i]=0xCC;
            WonderTrade_TrySkipScene();
            int accept=enabled && slot<6 && !mail && !evolves;
            if(gSpecialVar_LastResult!=accept || errors) return 1;
            if(accept) {
                if(!egg) theirs.friendship=70;
                if(!same(&gPlayerParty[slot],&theirs,sizeof(theirs)) || !same(gEnemyParty,&mine,sizeof(mine))) return 2;
                if(dexCalls!=(egg ? 0 : 2) || !same(gStringVar1,theirs.nickname,8)) return 3;
            } else if(!same(gEnemyParty,&theirs,sizeof(theirs)) || dexCalls || gStringVar1[0]!=0xCC) return 4;
            for(int i=0;i<6;i++) if((!accept || i!=(int)slot) && !same(&gPlayerParty[i],&mine,sizeof(mine))) return 5;
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('wonder_skip' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Wonder skip: 1572864 slot/flag/evolution/egg/mail cases passed with complete mon payload preservation.')


if __name__ == '__main__':
    main()
