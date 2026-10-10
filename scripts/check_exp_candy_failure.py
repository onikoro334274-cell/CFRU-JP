"""Ensure candy application cannot mutate Pokemon or chain state without an item."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/exp_candy.c').read_text(encoding='utf-8')
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    production = function(clean, 'AddCandyExperience') + '\n' + function(clean, 'ItemUseCB_ExpCandy')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define MAX_LEVEL 200
#define SE_SELECT 1
#define STR_CONV_MODE_LEFT_ALIGN 0
#define ITEM_EXP_CANDY_XS 100
#define ITEM_EXP_CANDY_XL 104
#define ITEM_RARE_CANDY 105
#define VAR_EXP_CANDY_TARGET 1
#define VAR_EXP_CANDY_EXP_LO 2
#define VAR_EXP_CANDY_EXP_HI 3
enum {MON_DATA_SPECIES,MON_DATA_LEVEL,MON_DATA_EXP,MON_DATA_IS_EGG};
struct Pokemon {u8 level,egg;};static struct Pokemon gPlayerParty[6];
static struct {u8 slotId;} gPartyMenu;static struct {TaskFunc func;} gTasks[2];
static struct {u8 growthRate;} gBaseStats[2];static u32 gExperienceTables[1][256];
static const u16 sExpCandyAmount[]={100,800,3000,10000,30000};
static u16 gSpecialVar_ItemId;static int gPartyMenuUseExitCallback;
static u8 gStringVar1[8],gStringVar2[8],gStringVar4[8],gText_ExpCandyGotExp[8],gText_WontHaveEffect[8];
static int removed,effects,messages,errors;
static void PlaySE(int n){(void)n;}
static u32 GetMonData(struct Pokemon* m,int f,void* p){(void)p;return f==MON_DATA_SPECIES?1:f==MON_DATA_LEVEL?m->level:f==MON_DATA_IS_EGG?m->egg:0;}
static int RemoveBagItem(u16 item,int n){if(item!=gSpecialVar_ItemId||n!=1)errors++;removed++;return 0;}
static void SetMonData(struct Pokemon* m,int f,const void* p){(void)m;(void)f;(void)p;effects++;}
static int CheckBagHasSpace(u16 item,int n){(void)item;(void)n;effects++;return 1;}
static void CalculateMonStats(struct Pokemon* m){(void)m;effects++;}
static void UpdateMonDisplayInfoAfterRareCandy(u8 s,struct Pokemon* m){(void)s;(void)m;effects++;}
static void GetMonNickname(struct Pokemon* m,u8* s){(void)m;(void)s;effects++;}
static void ConvertIntToDecimalStringN(u8* s,u32 n,int mode,int digits){(void)s;(void)n;(void)mode;(void)digits;effects++;}
static void StringExpandPlaceholders(u8* d,const u8* s){(void)d;(void)s;effects++;}
static void VarSet(u16 id,u16 value){(void)id;(void)value;effects++;}
static void ExpCandyNextLevel(u8 id){(void)id;effects++;}
static void DisplayPartyMenuMessage(const u8* s,u8 b){if(s!=gText_WontHaveEffect||b!=1)errors++;messages++;}
static void ScheduleBgCopyTilemapToVram(int n){if(n!=2)errors++;}
static void done(u8 id){(void)id;}
PRODUCTION
int main(void){gPartyMenu.slotId=2;
 for(gSpecialVar_ItemId=99;gSpecialVar_ItemId<=105;gSpecialVar_ItemId++)for(int level=0;level<256;level++)for(int egg=0;egg<2;egg++){
  gPlayerParty[2].level=level;gPlayerParty[2].egg=egg;removed=effects=messages=errors=0;gTasks[1].func=NULL;
  ItemUseCB_ExpCandy(1,done);int eligible=gSpecialVar_ItemId>=100&&gSpecialVar_ItemId<=104&&level<200&&!egg;
  if(errors||effects||removed!=eligible||messages!=1||gTasks[1].func!=done)return 1;
 }return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('candy_fail' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 3584,
              'scope': 'Production candy callback with controlled APIs: invalid items, levels, eggs and failed consumption leave Pokemon/chain state untouched.',
              'limitations': 'Fixture level cap is 200; does not verify active build cap, successful application, actual tables, level chain or GBA runtime.'}
    (ROOT / 'audit/exp_candy_failure_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('3584 candy failure/eligibility cases passed')


if __name__ == '__main__':
    main()
