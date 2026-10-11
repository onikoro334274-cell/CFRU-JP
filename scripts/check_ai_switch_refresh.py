"""Verify cached switch options refresh and reservation/dead-slot rejection."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
 source=function((ROOT/'src/Battle_AI/ai_master.c').read_text(encoding='utf-8'),'u8 GetMostSuitableMonToSwitchInto(void)')
 fixture=r"""
typedef unsigned char u8;typedef unsigned short u16;
#define NULL ((void*)0)
#define TRUE 1
#define PARTY_SIZE 6
#define MON_DATA_HP 0
#define PARTNER(bank) ((bank)^2)
struct Pokemon {u16 hp;};static struct Pokemon party[6];
static struct {struct {u8 bestMonIdToSwitchInto[4][2],calculatedAISwitchings[4];}ai;} state;
#define gNewBS (&state)
static struct {u8 monToSwitchIntoId[4];} battle;
#define gBattleStruct (&battle)
static u8 gActiveBattler=1,fresh,alternate;static unsigned recalcs;
static void LoadBattlersAndFoes(u8*a,u8*b,u8*c,u8*d){*a=1;*b=3;*c=0;*d=2;}
static struct Pokemon* LoadPartyRange(u8 bank,u8*first,u8*last){*first=0;*last=6;return party;}
static void CalcMostSuitableMonSwitchIfNecessary(void){}
static void CalcMostSuitableMonToSwitchInto(void){recalcs++;state.ai.bestMonIdToSwitchInto[1][0]=fresh;state.ai.bestMonIdToSwitchInto[1][1]=alternate;}
static unsigned GetMonData(struct Pokemon*m,unsigned field,void*unused){if(m<party||m>=party+6)__builtin_trap();return m->hp;}
static unsigned BankSideHasTwoTrainers(u8 bank){return 0;}
PRODUCTION
int main(void){
for(unsigned old=0;old<8;old++)for(fresh=0;fresh<8;fresh++)for(alternate=0;alternate<8;alternate++)
for(unsigned reserve=0;reserve<8;reserve++)for(unsigned mask=0;mask<64;mask++){
 for(unsigned i=0;i<6;i++)party[i].hp=(mask&(1u<<i))?10:0;
 state.ai.bestMonIdToSwitchInto[1][0]=old;state.ai.bestMonIdToSwitchInto[1][1]=alternate;
 battle.monToSwitchIntoId[1]=reserve;battle.monToSwitchIntoId[3]=6;recalcs=0;
 unsigned invalid=old>=6||!(mask&(1u<<old));unsigned expected=invalid?fresh:old;
 if(expected==reserve||expected==6)expected=alternate;
 if(expected>=6||!(mask&(1u<<expected))||expected==reserve)expected=6;
 if(GetMostSuitableMonToSwitchInto()!=expected||recalcs!=invalid)return 1;
}return 0;}
""".replace('PRODUCTION',source)
 with tempfile.TemporaryDirectory() as directory:
  path=Path(directory);c=path/'s.c';obj=path/'s.obj';exe=path/'s.exe';c.write_text(fixture,encoding='utf-8')
  subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
  subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
  subprocess.run([str(exe)],check=True)
 print('AI switch refresh: 262144 cached/new/alternate/reserved/alive-mask combinations pass.')
if __name__=='__main__':main()
