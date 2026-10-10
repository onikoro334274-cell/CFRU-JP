"""Execute bank ability precedence, level/suppression gates and duplicate removal."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    production=function((ROOT/'src/second_ability.c').read_text(encoding='utf-8'),'u16 SecondAbility_OfBank(u8 bank)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned short u16;
#define NULL ((void*)0)
#define ABILITY_NONE 0
#define B_SIDE_PLAYER 0
#define SECOND_ABILITY_UNLOCK_LEVEL 50
struct Pokemon {u8 marker;};
static struct Pokemon party[4];
static struct {u8 level;u16 species,ability;} gBattleMons[4];
static u8 suppressed,side;
static u16 dragon,table,mom;static unsigned accesses;
#define SIDE(bank) side
#define SPECIES(bank) gBattleMons[bank].species
#define ABILITY(bank) gBattleMons[bank].ability
static unsigned IsAbilitySuppressed(u8 bank){return suppressed;}
static struct Pokemon* GetBankPartyData(u8 bank){accesses++;return &party[bank];}
static u16 HackDragonBoulder_SecondAbility(const struct Pokemon* mon,u16 species){if((side==0)!=(mon!=NULL)||species!=100)__builtin_trap();return dragon;}
static u16 SecondAbility_OfSpecies(u16 species){if(species!=100)__builtin_trap();return table;}
static u16 MomPikachuSecondAbility(u16 species,struct Pokemon* mon){if((side==0)!=(mon!=NULL)||species!=100)__builtin_trap();return mom;}
PRODUCTION
int main(void){
    for(unsigned bank=0;bank<4;bank++)for(unsigned level=0;level<256;level++)
    for(suppressed=0;suppressed<2;suppressed++)for(side=0;side<2;side++)
    for(dragon=0;dragon<3;dragon++)for(table=0;table<3;table++)for(mom=0;mom<3;mom++)
    for(unsigned base=0;base<3;base++){
        gBattleMons[bank].level=level;gBattleMons[bank].species=100;gBattleMons[bank].ability=base;accesses=0;
        u16 expected=dragon?dragon:table?table:mom;
        if(level<50||suppressed||expected==base)expected=0;
        if(SecondAbility_OfBank(bank)!=expected)return 1;
        if((level<50||suppressed||side)&&accesses)return 2;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'bank.c';obj=path/'bank.obj';exe=path/'bank.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        subprocess.run([str(exe)],check=True)
    print('Second ability bank: 331776 level/suppression/side/source/base/bank combinations pass.')
if __name__=='__main__':main()
