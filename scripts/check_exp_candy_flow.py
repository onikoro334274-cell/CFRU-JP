"""Exercise candy entry, per-level continuation and final evolution dispatch together."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/exp_candy.c').read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    names = ['ExpCandyChainClear', 'ExpCandyNextLevel', 'ExpCandyChainValid',
             'HackExpCandy_TryEvolution', 'AddCandyExperience', 'ItemUseCB_ExpCandy']
    production = '\n'.join(function(source, name) for name in names)
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef short s16;typedef unsigned int u32;
typedef u8 bool8;
typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define NUM_STATS 6
#define MAX_LEVEL 200
#define PARTY_SIZE 6
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define SE_SELECT 1
#define STR_CONV_MODE_LEFT_ALIGN 0
#define ITEM_EXP_CANDY_XS 100
#define ITEM_EXP_CANDY_XL 104
#define ITEM_RARE_CANDY 105
#define ITEM_NONE 0
#define EVO_MODE_NORMAL 0
#define SPECIES_NONE 0
#define VAR_EXP_CANDY_TARGET 1
#define VAR_EXP_CANDY_EXP_LO 2
#define VAR_EXP_CANDY_EXP_HI 3
enum{MON_DATA_SPECIES,MON_DATA_LEVEL,MON_DATA_EXP,MON_DATA_IS_EGG};
struct Pokemon{u8 level,egg;u32 exp;};static struct Pokemon gPlayerParty[6];
static struct{u8 slotId;TaskFunc exitCallback;}gPartyMenu;
static struct{TaskFunc func;}gTasks[2];static TaskFunc gCB2_AfterEvolution;
static struct{u8 growthRate;}gBaseStats[2];static u32 gExperienceTables[1][256];
static const u16 sExpCandyAmount[]={100,800,3000,10000,30000};
static struct{s16 data[16];}internal;static __typeof__(internal)*sPartyMenuInternal=&internal;
static u8 gStringVar1[8],gStringVar2[8],gStringVar4[8],gText_ExpCandyGotExp[8],gText_WontHaveEffect[8];
static u16 vars[4],gSpecialVar_ItemId;
static int gPartyMenuUseExitCallback,candies,rareCandies,bagSpace,steps,learned,evolution,freed,destroyed,queries,errors;
static void done(u8 id){(void)id;}
void HackExpCandy_TryEvolution(u8 id);
static void Task_ClosePartyMenuAfterText(u8 id){(void)id;}
/* Native UI/move-selection callback is controlled here; production continuation is real. */
static void Task_DisplayLevelUpStatsPg1(u8 id){learned++;HackExpCandy_TryEvolution(id);}
static u16 VarGet(u16 id){return vars[id];}
static void VarSet(u16 id,u16 value){vars[id]=value;}
static u32 GetMonData(struct Pokemon*m,int field,void*p){(void)p;return field==MON_DATA_SPECIES?1:field==MON_DATA_LEVEL?m->level:field==MON_DATA_IS_EGG?m->egg:m->exp;}
static void SetMonData(struct Pokemon*m,int field,const void*p){if(field!=MON_DATA_EXP)errors++;m->exp=*(const u32*)p;}
static void CalculateMonStats(struct Pokemon*m){u8 level=1;while(level<200&&gExperienceTables[0][level+1]<=m->exp)level++;m->level=level;}
static int RemoveBagItem(u16 item,int n){if(item<100||item>104||n!=1)errors++;if(!candies)return 0;candies--;return 1;}
/* Poison these APIs: neither bag capacity nor rare-candy inventory may affect a chain. */
static int CheckBagHasSpace(u16 item,int n){(void)item;(void)n;errors++;return bagSpace;}
static int AddBagItem(u16 item,int n){(void)item;(void)n;errors++;rareCandies++;return bagSpace;}
static void GetMonLevelUpWindowStats(struct Pokemon*m,s16*s){for(int i=0;i<6;i++)s[i]=m->level*10+i;}
static int ExecuteTableBasedItemEffect_(u8 slot,u16 item,u8 move){
 if(slot!=gPartyMenu.slotId||item!=105||move||gPlayerParty[slot].level>=200)errors++;
 struct Pokemon*m=&gPlayerParty[slot];m->exp=gExperienceTables[0][m->level+1];CalculateMonStats(m);steps++;return 0;
}
static void WriteQuestLog(u8 a,u8 b,u16 c,u16 d){(void)a;(void)b;(void)c;(void)d;}
static void PlayFanfareByFanfareNum(u8 n){(void)n;}
static void PlaySE(int n){(void)n;}
static void UpdateMonDisplayInfoAfterRareCandy(u8 s,struct Pokemon*m){(void)s;(void)m;}
static void GetMonNickname(struct Pokemon*m,u8*s){(void)m;(void)s;}
static void ConvertIntToDecimalStringN(u8*s,u32 n,int mode,int digits){(void)s;(void)n;(void)mode;(void)digits;}
static void StringExpandPlaceholders(u8*s,const u8*t){(void)s;(void)t;}
static void DisplayPartyMenuMessage(const u8*s,u8 b){(void)s;(void)b;}
static void ScheduleBgCopyTilemapToVram(int n){(void)n;}
static u16 GetEvolutionTargetSpecies(struct Pokemon*m,int mode,u16 item){(void)m;if(mode||item||vars[1]||vars[2]||vars[3])errors++;queries++;return evolution?7:0;}
static void FreePartyPointers(void){freed++;}
static void BeginEvolutionScene(struct Pokemon*m,u16 species,int b,u8 slot){if(m!=&gPlayerParty[slot]||species!=7||b!=1)errors++;}
static void DestroyTask(u8 id){destroyed++;gTasks[id].func=0;}
PRODUCTION
int main(void){
 for(int i=0;i<256;i++)gExperienceTables[0][i]=i*1000;
 for(int level=1;level<200;level++)for(int candy=0;candy<5;candy++)for(int offset=0;offset<3;offset++)
 for(int bag=0;bag<2;bag++)for(int rare=0;rare<2;rare++)for(int evo=0;evo<2;evo++)for(int slot=0;slot<6;slot++){
  gPartyMenu.slotId=slot;gPartyMenu.exitCallback=done;gCB2_AfterEvolution=0;
  struct Pokemon*m=&gPlayerParty[slot];m->level=level;m->egg=0;m->exp=level*1000+offset*499;
  u32 expected=m->exp+sExpCandyAmount[candy];if(expected>200000)expected=200000;
  int target=expected/1000;
  for(int i=0;i<4;i++)vars[i]=0;
  gSpecialVar_ItemId=100+candy;candies=2;rareCandies=rare*17;bagSpace=bag;evolution=evo;
  steps=learned=freed=destroyed=queries=errors=0;gTasks[1].func=done;
  ItemUseCB_ExpCandy(1,done);
  for(int guard=0;vars[1]&&guard<200;guard++){
   if(gTasks[1].func!=Task_DisplayLevelUpStatsPg1)return 1;
   gTasks[1].func(1);
  }
  if(errors||candies!=1||rareCandies!=rare*17||m->exp!=expected||m->level!=target)return 2;
  if(vars[1]||vars[2]||vars[3]||steps!=target-level||learned!=steps||gSpecialVar_ItemId!=100+candy)return 3;
  int dispatch=target>level;
  if(queries!=dispatch||freed!=dispatch*evo||destroyed!=dispatch*evo)return 4;
  if(dispatch&&!evo&&gTasks[1].func!=Task_ClosePartyMenuAfterText)return 5;
  if(dispatch&&evo&&gCB2_AfterEvolution!=done)return 6;
  if(!dispatch&&gTasks[1].func!=done)return 7;
 }
 return 0;
}
'''.replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('flow' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode

    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', '7c2f15a:src/exp_candy.c'], cwd=ROOT).decode('utf-8')
    baseline = re.sub(r'/\*.*?\*/|//[^\n]*', '', baseline, flags=re.S)
    old_production = '\n'.join(function(baseline, name) for name in names)
    baseline_fixture = fixture.replace(production, old_production)
    # Reproduce the old full-bag behavior without deliberately poisoning the old API.
    baseline_fixture = baseline_fixture.replace('errors++;return bagSpace;', 'return bagSpace;')
    legacy_step = r'''
static void ItemUseCB_RareCandyStep(u8 id,TaskFunc callback){
 (void)callback;ExecuteTableBasedItemEffect_(gPartyMenu.slotId,105,0);
 rareCandies--;gTasks[id].func=Task_DisplayLevelUpStatsPg1;
}
'''
    baseline_fixture = baseline_fixture.replace(old_production, legacy_step + old_production)
    baseline_exit = execute(baseline_fixture)
    assert baseline_exit == 3, baseline_exit  # final exp updates but intermediate steps are skipped
    report = {'result': 'passed', 'flow_cases': 199 * 5 * 3 * 2 * 2 * 2 * 6,
              'baseline': '7c2f15a reproduces missing intermediate callbacks for a full bag (exit 3).',
              'scope': 'Actual entry/level-step/chain validation/continuation/final evolution dispatch with controlled native services; all party slots, five candy sizes, levels 1..199, three experience remainders, full/available bag, zero/17 rare candies, evolution/no evolution. Checks one candy consumed, rare inventory unchanged, every intermediate callback, final experience/level/item restoration and cleared reserved vars.',
              'limitations': 'Native move UI and effect services simulated. Does not establish actual move replacement UI, friendship, save interruptions or linked GBA runtime.'}
    (ROOT / 'audit/exp_candy_flow_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"{report['flow_cases']} integrated candy-flow cases passed")


if __name__ == '__main__':
    main()
