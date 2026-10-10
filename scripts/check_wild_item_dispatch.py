"""Verify battle-mode gating, target slots and existing held-item preservation."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    production=function((ROOT/'src/build_pokemon.c').read_text(encoding='utf-8'),'void SetWildMonHeldItem(void)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define NULL ((void*)0)
#define ITEM_NONE 0
#define MON_DATA_SPECIES 0
#define MON_DATA_HELD_ITEM 1
#define BATTLE_TYPE_DOUBLE 0x0001
#define BATTLE_TYPE_TRAINER 0x0008
#define BATTLE_TYPE_SCRIPTED_WILD_1 0x2000
#define BATTLE_TYPE_POKE_DUDE 0x10000
#define IS_DOUBLE_BATTLE (gBattleTypeFlags&BATTLE_TYPE_DOUBLE)
static u32 gBattleTypeFlags;static u8 gDexNavStartedBattle;
static struct {u16 species,item;} gEnemyParty[6];
static u16 chosen[2];static unsigned reads,generated,writes;
static u32 GetMonData(void* mon,unsigned field,void* unused){
    if(field!=MON_DATA_SPECIES)__builtin_trap();
    for(unsigned i=0;i<2;i++)if(mon==&gEnemyParty[i]){reads++;return gEnemyParty[i].species;}
    __builtin_trap();
}
static u16 GenerateWildMonHeldItem(u16 species,u8 bonus){
    if(bonus||species<100||species>101)__builtin_trap();generated++;return chosen[species-100];
}
static void SetMonData(void* mon,unsigned field,const u16* value){
    if(field!=MON_DATA_HELD_ITEM||!*value)__builtin_trap();
    for(unsigned i=0;i<2;i++)if(mon==&gEnemyParty[i]){gEnemyParty[i].item=*value;writes++;return;}
    __builtin_trap();
}
PRODUCTION
int main(void){
    for(unsigned flags=0;flags<32;flags++)for(unsigned nav=0;nav<2;nav++)
    for(unsigned outcomes=0;outcomes<4;outcomes++){
        gBattleTypeFlags=(flags&1?1:0)|(flags&2?8:0)|(flags&4?0x2000:0)|
            (flags&8?0x10000:0)|(flags&16?0x80000:0);
        gDexNavStartedBattle=nav;reads=generated=writes=0;
        for(unsigned i=0;i<6;i++){gEnemyParty[i].species=100+i;gEnemyParty[i].item=500+i;}
        for(unsigned i=0;i<2;i++)chosen[i]=outcomes&(1u<<i)?700+i:0;
        SetWildMonHeldItem();
        unsigned count=(flags&14)||nav?0:flags&1?2:1,expectedWrites=0;
        for(unsigned i=0;i<6;i++){
            u16 expected=500+i;
            if(i<count&&chosen[i]){expected=chosen[i];expectedWrites++;}
            if(gEnemyParty[i].item!=expected||gEnemyParty[i].species!=100+i)return 1;
        }
        if(reads!=count||generated!=count||writes!=expectedWrites)return 2;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'dispatch.c';obj=path/'dispatch.obj';exe=path/'dispatch.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        subprocess.run([str(exe)],check=True)
    print('Wild item dispatch: 256 battle-mode/DexNav/item-outcome cases preserve gated and untargeted slots.')
if __name__=='__main__':main()
