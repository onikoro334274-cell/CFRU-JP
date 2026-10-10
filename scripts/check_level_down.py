"""Exercise production level-down eligibility, consumption, experience and HP."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_level_down.c').read_text(encoding='utf-8')
    production = function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'ItemUseCB_LevelDown')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define FALSE 0
#define TRUE 1
#define SE_SELECT 1
#define STR_CONV_MODE_LEFT_ALIGN 0
enum {MON_DATA_SPECIES,MON_DATA_LEVEL,MON_DATA_IS_EGG,MON_DATA_HP,MON_DATA_MAX_HP,MON_DATA_EXP};
struct Pokemon {u8 level,egg;u16 hp,max;u32 exp;};static struct Pokemon gPlayerParty[6];
static struct {u8 slotId;} gPartyMenu;static struct {TaskFunc func;} gTasks[2];
static struct {u8 growthRate;} gBaseStats[2];static u32 gExperienceTables[1][256];
static u16 Var800E=123;static int gPartyMenuUseExitCallback;
static u8 gStringVar1[8],gStringVar2[8],gStringVar4[8],gText_LevelDown_Done[8],gText_WontHaveEffect[8];
static int available,removed,writes,errors,calcs,updates,messages;
static void PlaySE(int n){(void)n;}
static u32 GetMonData(struct Pokemon* m,int f,void* p){(void)p;return f==MON_DATA_SPECIES?1:f==MON_DATA_LEVEL?m->level:f==MON_DATA_IS_EGG?m->egg:f==MON_DATA_HP?m->hp:m->max;}
static int RemoveBagItem(u16 item,int n){if(item!=123||n!=1||writes)errors++;removed++;return available;}
static void SetMonData(struct Pokemon* m,int f,const void* p){if(!available||removed!=1)errors++;writes++;if(f==MON_DATA_HP)m->hp=*(const u16*)p;else if(f==MON_DATA_LEVEL)m->level=*(const u8*)p;else m->exp=*(const u32*)p;}
static void CalculateMonStats(struct Pokemon* m){calcs++;m->max=10;m->hp=65535;}
static void UpdateMonDisplayInfoAfterRareCandy(u8 slot,struct Pokemon* m){(void)m;if(slot!=2)errors++;updates++;}
static void GetMonNickname(struct Pokemon* m,u8* s){(void)m;(void)s;}
static void ConvertIntToDecimalStringN(u8* s,int n,int mode,int digits){(void)s;(void)n;(void)mode;(void)digits;}
static void StringExpandPlaceholders(u8* d,const u8* s){(void)d;(void)s;}
static void DisplayPartyMenuMessage(const u8* s,u8 b){(void)s;(void)b;messages++;}
static void ScheduleBgCopyTilemapToVram(int n){(void)n;}
static void done(u8 id){(void)id;}
PRODUCTION
int main(void){gPartyMenu.slotId=2;for(int i=0;i<256;i++)gExperienceTables[0][i]=(u32)i*100;
 for(int level=0;level<256;level++)for(int egg=0;egg<2;egg++)for(available=0;available<2;available++)for(int h=0;h<3;h++){
  u16 hp=h==0?0:h==1?5:20;struct Pokemon* m=&gPlayerParty[2];m->level=level;m->egg=egg;m->hp=hp;m->max=30;m->exp=999;
  removed=writes=errors=calcs=updates=messages=0;gTasks[1].func=NULL;ItemUseCB_LevelDown(1,done);
  int eligible=level>1&&!egg,change=eligible&&available;
  if(errors||removed!=eligible||calcs!=change||updates!=change||messages!=1||gTasks[1].func!=done)return 1;
  if(change){if(m->level!=level-1||m->exp!=(u32)(level-1)*100||m->hp!=(hp>10?10:hp))return 2;}
  else if(writes||m->level!=level||m->exp!=999||m->hp!=hp||m->max!=30)return 3;
 }return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('level' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 3072,
              'scope': 'Production callback: all byte levels, eggs, failed/successful consumption, experience lookup and fainted/low/excess HP.',
              'limitations': 'Controlled stat/inventory APIs and synthetic experience table; does not verify actual growth curves, selected-slot validation or GBA runtime.'}
    (ROOT / 'audit/level_down_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('3072 level-down cases passed')


if __name__ == '__main__':
    main()
