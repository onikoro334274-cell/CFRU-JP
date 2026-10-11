"""Verify trainer difficulty level adjustments and battle exclusions."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
 source=(ROOT/'src/hack_difficulty.c').read_text(encoding='utf-8')
 names=['static bool8 OrdinaryTrainerBattle(void)','static bool8 EasyTrainerBattle(void)','static bool8 MediumTrainerBattle(void)','static void LowerTrainerMonLevel(struct Pokemon* mon, u32 down)','static void RaiseTrainerMonLevel(struct Pokemon* mon, u32 up)','void HackDifficulty_AdjustTrainerMon(struct Pokemon* mon, u8 side)']
 production='\n'.join(function(source,n) for n in names)
 fixture=r"""
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define MAX_LEVEL 200
#define NUM_STATS 6
#define B_SIDE_OPPONENT 1
#define BATTLE_TYPE_TRAINER 1
#define BATTLE_TYPE_FRONTIER 2
#define FLAG_BATTLE_FACILITY 0
#define HOENN_VAR_DIFFICULTY 0
#define DIFFICULTY_NORMAL 0
#define DIFFICULTY_EASY 1
#define DIFFICULTY_MEDIUM 2
#define DIFFICULTY_HARD 3
#define DIFFICULTY_MOMO_ICE 4
#define EASY_LEVEL_DOWN 3
#define MEDIUM_LEVEL_DOWN 2
#define HARD_LEVEL_UP 1
#define MOMO_ICE_LEVEL_UP 5
#define ITEM_NONE 0
#define MON_DATA_SPECIES 0
#define MON_DATA_LEVEL 1
#define MON_DATA_EXP 2
#define MON_DATA_HELD_ITEM 3
#define MON_DATA_HP_EV 4
struct Pokemon {u32 species,level,exp;u16 item;u8 ev[6];};
static u32 gBattleTypeFlags;static unsigned facility,difficulty;
static const struct {u8 growthRate;} gBaseStats[2]={{0},{0}};
static u32 gExperienceTables[1][201];
static unsigned FlagGet(unsigned id){return facility;}
static unsigned VarGet(unsigned id){return difficulty;}
static u32 MathMin(u32 a,u32 b){return a<b?a:b;}
static unsigned IsMegaStone(u16 item){return item==10;}
static unsigned IsPrimalOrb(u16 item){return item==11;}
static u32 GetMonData(struct Pokemon* m,unsigned field,void* unused){return field==0?m->species:field==1?m->level:m->item;}
static void SetMonData(struct Pokemon* m,unsigned field,const void* value){if(field==1)m->level=*(const u32*)value;else if(field==2)m->exp=*(const u32*)value;else if(field==3)m->item=*(const u16*)value;else m->ev[field-4]=*(const u8*)value;}
PRODUCTION
int main(void){
for(unsigned i=0;i<=200;i++)gExperienceTables[0][i]=i*i*i;
for(difficulty=0;difficulty<5;difficulty++)for(unsigned level=1;level<=200;level++)
for(unsigned side=0;side<2;side++)for(unsigned flags=0;flags<4;flags++)for(facility=0;facility<2;facility++)for(unsigned item=9;item<=11;item++){
struct Pokemon m;m.species=1;m.level=level;m.exp=777;m.item=item;for(unsigned i=0;i<6;i++)m.ev[i]=80;
gBattleTypeFlags=flags;HackDifficulty_AdjustTrainerMon(&m,side);
unsigned active=side==1&&(flags&1)&&!(flags&2)&&!facility,target=level;
if(active){if(difficulty==1&&level>4)target=level-3;if(difficulty==2&&level>3)target=level-2;if(difficulty==3)target=level<200?level+1:200;if(difficulty==4)target=level>195?200:level+5;}
if(m.level!=target||m.exp!=(target==level?777:target*target*target))return 1;
u16 held=active&&(difficulty==1||(difficulty==2&&item==9))?0:item;if(m.item!=held)return 2;
for(unsigned i=0;i<6;i++)if(m.ev[i]!=(active&&difficulty==1?0:80))return 3;
}return 0;}
""".replace('PRODUCTION',production)
 with tempfile.TemporaryDirectory() as directory:
  path=Path(directory);c=path/'d.c';obj=path/'d.obj';exe=path/'d.exe';c.write_text(fixture,encoding='utf-8')
  subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
  subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
  subprocess.run([str(exe)],check=True)
 print('Difficulty levels: 48000 level/difficulty/side/battle/facility/item cases pass.')
if __name__=='__main__':main()
