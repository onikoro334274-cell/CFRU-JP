"""Execute second-ability terrain helper with finite and infinite duration modes."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    production=function((ROOT/'src/second_ability.c').read_text(encoding='utf-8'),'static bool8 TryStartTerrain(u8 bank, u8 terrain, u8 anim, const u8* string)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool8;
#define FALSE 0
#define TRUE 1
#define BATTLE_TYPE_BATTLE_CIRCUS 1
#define BATTLE_CIRCUS_TERRAIN 2
#define ITEM_EFFECT_TERRAIN_EXTENDER 3
static u32 gBattleTypeFlags,gBattleCircusFlags;static u8 gTerrainType,item;
static struct {u8 TerrainTimer;} state;
#define gNewBS (&state)
#define ITEM_EFFECT(bank) item
static const u8* gBattleStringLoader;
static struct {u8 animArg1,bank;} gBattleScripting;
static u32 seed[3];
#define gSeedHelper ((u8*)&seed[1])
static const u8 BattleScript_TerrainFromAbility[1],message[1],oldMessage[1];
static unsigned pushes;static u8 expectedBank,expectedTerrain;
static void BattleScriptPushCursorAndCallback(const u8* script){
    if(script!=BattleScript_TerrainFromAbility||gTerrainType!=expectedTerrain||
       gBattleScripting.bank!=expectedBank||gBattleStringLoader!=message)__builtin_trap();pushes++;
}
PRODUCTION
int main(void){
    for(unsigned bank=0;bank<4;bank++)for(unsigned terrain=1;terrain<=4;terrain++)
    for(unsigned prior=0;prior<=4;prior++)for(unsigned circus=0;circus<2;circus++)
    for(unsigned locked=0;locked<2;locked++)for(unsigned extender=0;extender<2;extender++){
        expectedBank=bank;expectedTerrain=terrain;pushes=0;
        gTerrainType=prior;gBattleTypeFlags=circus;gBattleCircusFlags=locked?2:0;item=extender?3:0;
        state.TerrainTimer=99;gBattleStringLoader=oldMessage;
        gBattleScripting.animArg1=88;gBattleScripting.bank=77;
        seed[0]=seed[2]=0xdeadbeef;seed[1]=0x12345678;
        unsigned changed=!(circus&&locked)&&prior!=terrain;
        if(TryStartTerrain(bank,terrain,terrain+10,message)!=changed||pushes!=changed)return 1;
        if(seed[0]!=0xdeadbeef||seed[2]!=0xdeadbeef)return 2;
        if(!changed){
            if(gTerrainType!=prior||state.TerrainTimer!=99||gBattleStringLoader!=oldMessage||
               gBattleScripting.animArg1!=88||gBattleScripting.bank!=77||seed[1]!=0x12345678)return 3;
        }else{
#ifdef INFINITE_TERRAIN
            unsigned duration=0;
#else
            unsigned duration=extender?8:5;
#endif
            if(state.TerrainTimer!=duration||gBattleScripting.animArg1!=terrain+10||
               seed[1]!=(u32)(unsigned long long)message)return 4;
        }
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'terrain.c';obj=path/'terrain.obj';exe=path/'terrain.exe';c.write_text(fixture,encoding='utf-8')
        for infinite in (False,True):
            command=['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-Wno-pointer-to-int-cast']
            if infinite:command.append('-DINFINITE_TERRAIN')
            subprocess.run(command+['-c',str(c),'-o',str(obj)],check=True)
            subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
            subprocess.run([str(exe)],check=True)
    print('Second ability terrain: 1280 finite/infinite state combinations preserve blocked state and seed canaries.')
if __name__=='__main__':main()
