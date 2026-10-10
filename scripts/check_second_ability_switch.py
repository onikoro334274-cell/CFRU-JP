"""Exercise once-per-species dispatch and temporary primary-ability done flags."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    production=function((ROOT/'src/second_ability.c').read_text(encoding='utf-8'),'bool8 SecondAbility_TrySwitchIn(u8 bank)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#include "include/constants/abilities.h"
#define FALSE 0
#define TRUE 1
#define SPECIES_NONE 0
#define STATUS3_SWITCH_IN_ABILITY_DONE 0x100
#define ABILITYEFFECT_ON_SWITCHIN 0
#define ELECTRIC_TERRAIN 1
#define GRASSY_TERRAIN 2
#define MISTY_TERRAIN 3
#define PSYCHIC_TERRAIN 4
#define B_ANIM_ELECTRIC_SURGE 1
#define B_ANIM_GRASSY_SURGE 2
#define B_ANIM_MISTY_SURGE 3
#define B_ANIM_PSYCHIC_SURGE 4
static const u8 ElectricTerrainSetString[1],GrassyTerrainSetString[1],MistyTerrainSetString[1],PsychicTerrainSetString[1];
static struct {u16 secondAbilitySpecies[4],abilityPopupOverwrite;} state;
#define gNewBS (&state)
static u32 gStatuses3[4];static u16 currentSpecies,ability;static u8 alive,effect;
static unsigned calls,route;static u8 expectedBank;
#define SPECIES(bank) currentSpecies
#define BATTLER_ALIVE(bank) alive
static u16 SecondAbility_OfBank(u8 bank){return ability;}
static bool8 Hack_StartIntimidateLike(u8 bank,bool8 atk){if(bank!=expectedBank)__builtin_trap();calls++;route=atk?1:2;return effect;}
static bool8 TryStartTerrain(u8 bank,u8 terrain,u8 anim,const u8* string){if(bank!=expectedBank||terrain!=anim)__builtin_trap();calls++;route=terrain+2;return effect;}
static bool8 AbilityBattleEffects(unsigned kind,u8 bank,unsigned a,u16 special,unsigned b){
    if(bank!=expectedBank||kind||a||b||special!=ability||(gStatuses3[bank]&0x100))__builtin_trap();
    calls++;route=7;gStatuses3[bank]|=0x300;return effect;
}
PRODUCTION
int main(void){
    const u16 abilities[]={0,ABILITY_INTIMIDATE,ABILITY_SCARE,ABILITY_ELECTRICSURGE,
        ABILITY_GRASSYSURGE,ABILITY_MISTYSURGE,ABILITY_PSYCHICSURGE,ABILITY_DRIZZLE};
    for(unsigned bank=0;bank<4;bank++)for(currentSpecies=0;currentSpecies<4;currentSpecies++)
    for(unsigned prior=0;prior<4;prior++)for(unsigned choice=0;choice<8;choice++)
    for(alive=0;alive<2;alive++)for(effect=0;effect<2;effect++)for(unsigned done=0;done<2;done++){
        expectedBank=bank;ability=abilities[choice];calls=route=0;
        for(unsigned i=0;i<4;i++){state.secondAbilitySpecies[i]=prior;gStatuses3[i]=0x40|(done?0x100:0);}
        state.abilityPopupOverwrite=999;
        unsigned eligible=alive&&currentSpecies&&prior!=currentSpecies;
        unsigned dispatch=eligible&&choice;
        unsigned expectedRoute=dispatch?choice:0;
        if(SecondAbility_TrySwitchIn(bank)!=(dispatch&&effect))return 1;
        if(calls!=dispatch||route!=expectedRoute)return 2;
        u16 popup=!dispatch?999:effect?ability:0;
        if(state.abilityPopupOverwrite!=popup)return 3;
        for(unsigned i=0;i<4;i++){
            if(state.secondAbilitySpecies[i]!=(i==bank&&eligible?currentSpecies:prior))return 4;
            u32 expected=0x40|(done?0x100:0)|(i==bank&&dispatch&&choice==7?0x200:0);
            if(gStatuses3[i]!=expected)return 5;
        }
        calls=0;if(SecondAbility_TrySwitchIn(bank)||calls)return 6;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'switch.c';obj=path/'switch.obj';exe=path/'switch.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-I',str(ROOT),'-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        subprocess.run([str(exe)],check=True)
    print('Second ability switch: 4096 state/route cases and immediate retries pass.')
if __name__=='__main__':main()
