"""Exercise EV reset inventory failure and HP recovery boundaries."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_ev_reset.c').read_text(encoding='utf-8')
    production = function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'ItemUseCB_EVReset')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define NUM_STATS 6
#define SE_SELECT 1
#define MON_DATA_HP_EV 10
#define MON_DATA_HP 1
#define MON_DATA_MAX_HP 2
struct Pokemon {u8 ev[6];u16 hp,max;};static struct Pokemon gPlayerParty[6];
static struct {u8 slotId;} gPartyMenu;static struct {TaskFunc func;} gTasks[2];
static u16 Var800E=123;static int gPartyMenuUseExitCallback;
static u8 gStringVar1[8],gStringVar4[8],gText_EVReset_Done[8],gText_WontHaveEffect[8];
static int available,removed,writes,calcs,updates,messages,errors,mode;static u16 newMax;
static void PlaySE(int n){if(n!=1)errors++;}
static u16 GetMonData(struct Pokemon* m,int f,void* p){(void)p;return f==1?m->hp:f==2?m->max:m->ev[f-10];}
static int RemoveBagItem(u16 item,int n){if(item!=123||n!=1||writes)errors++;removed++;return available;}
static void SetMonData(struct Pokemon* m,int f,const void* p){if(!available||removed!=1)errors++;writes++;if(f==1)m->hp=*(const u16*)p;else m->ev[f-10]=*(const u8*)p;}
static void CalculateMonStats(struct Pokemon* m){calcs++;m->max=newMax;m->hp=mode==0?0:mode==1?newMax+1:newMax;}
static void UpdateMonDisplayInfoAfterRareCandy(u8 slot,struct Pokemon* m){(void)m;if(slot!=2)errors++;updates++;}
static void GetMonNickname(struct Pokemon* m,u8* s){(void)m;(void)s;}
static void StringExpandPlaceholders(u8* d,const u8* s){(void)d;(void)s;}
static void DisplayPartyMenuMessage(const u8* s,u8 b){(void)s;if(b!=1)errors++;messages++;}
static void ScheduleBgCopyTilemapToVram(int n){if(n!=2)errors++;}
static void done(u8 id){(void)id;}
PRODUCTION
int main(void){gPartyMenu.slotId=2;
 for(int nonzero=0;nonzero<2;nonzero++)for(available=0;available<2;available++)
 for(u16 old=0;old<=20;old++)for(newMax=1;newMax<=20;newMax++)for(mode=0;mode<3;mode++){
  struct Pokemon* m=&gPlayerParty[2];for(int i=0;i<6;i++)m->ev[i]=nonzero?(u8)(i+1):0;
  m->hp=old;m->max=30;removed=writes=calcs=updates=messages=errors=0;gTasks[1].func=NULL;
  ItemUseCB_EVReset(1,done);int change=nonzero&&available;
  if(errors||removed!=nonzero||calcs!=change||updates!=change||messages!=1||gTasks[1].func!=done)return 1;
  if(!change){if(writes||m->hp!=old||m->max!=30)return 2;
   for(int i=0;i<6;i++)if(m->ev[i]!=(nonzero?i+1:0))return 6;}
  else {for(int i=0;i<6;i++)if(m->ev[i])return 3;
   if(old==0&&m->hp!=0)return 4;if(old&&(!m->hp||m->hp>newMax))return 5;}
 }
 return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('ev' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 5040,
              'scope': 'Production EV reset callback with controlled inventory/stat APIs; no effect, failed consumption, successful reset, fainted HP and zero/excess/valid recalculated HP.',
              'limitations': 'Does not verify actual stat calculations, menu slot validation, encrypted Pokemon APIs, or GBA runtime.'}
    (ROOT / 'audit/ev_reset_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('5040 EV reset inventory and HP boundary cases passed')


if __name__ == '__main__':
    main()
