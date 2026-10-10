"""Verify inventory-free candy steps preserve native stats/UI continuation."""
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
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef short s16;typedef unsigned int u32;
typedef void(*TaskFunc)(u8);
#define TRUE 1
#define NULL ((void*)0)
#define NUM_STATS 6
#define ITEM_RARE_CANDY 105
#define MON_DATA_LEVEL 1
#define STR_CONV_MODE_LEFT_ALIGN 0
struct Pokemon {u8 level;};static struct Pokemon gPlayerParty[6];
static struct {u8 slotId;} gPartyMenu;
static struct {s16 data[16];} internal;static __typeof__(internal)*sPartyMenuInternal=&internal;
static struct {TaskFunc func;} gTasks[2];static u16 gSpecialVar_ItemId;
static int gPartyMenuUseExitCallback,errors,stage;static u8 gStringVar1[8],gStringVar2[8],gStringVar4[8];
static void Task_DisplayLevelUpStatsPg1(u8 id){(void)id;}
static void tick(int expected){if(stage++!=expected)errors++;}
static void GetMonLevelUpWindowStats(struct Pokemon*m,s16*stats){
 if(m!=&gPlayerParty[2]||stats!=internal.data+(stage==0?0:6))errors++;
 tick(stage==0?0:2);for(int i=0;i<6;i++)stats[i]=m->level*10+i;
}
static int ExecuteTableBasedItemEffect_(u8 slot,u16 item,u8 move){
 tick(1);if(slot!=2||item!=105||move||gSpecialVar_ItemId!=105)errors++;gPlayerParty[slot].level++;return 0;
}
static void WriteQuestLog(u8 event,u8 b,u16 item,u16 extra){tick(3);if(!gPartyMenuUseExitCallback||event!=4||b||item!=105||extra!=65535)errors++;}
static void PlayFanfareByFanfareNum(u8 n){tick(4);if(n)errors++;}
static void UpdateMonDisplayInfoAfterRareCandy(u8 slot,struct Pokemon*m){tick(5);if(slot!=2||m!=&gPlayerParty[2])errors++;}
static void GetMonNickname(struct Pokemon*m,u8*s){tick(6);if(m!=&gPlayerParty[2]||s!=gStringVar1)errors++;}
static u32 GetMonData(struct Pokemon*m,int field,void*p){tick(7);if(field!=1||p)errors++;return m->level;}
static void ConvertIntToDecimalStringN(u8*s,u32 n,int mode,int digits){tick(8);if(s!=gStringVar2||n!=gPlayerParty[2].level||mode||digits!=3)errors++;}
static void StringExpandPlaceholders(u8*s,const u8*text){tick(9);if(s!=gStringVar4||text!=(const u8*)0x083DE267)errors++;}
static void DisplayPartyMenuMessage(const u8*s,u8 b){tick(10);if(s!=gStringVar4||b!=1)errors++;}
static void ScheduleBgCopyTilemapToVram(int bg){tick(11);if(bg!=2)errors++;}
PRODUCTION
int main(void){gPartyMenu.slotId=2;
 for(int level=1;level<200;level++){
  gPlayerParty[2].level=level;stage=errors=0;gSpecialVar_ItemId=100;gPartyMenuUseExitCallback=0;
  gTasks[1].func=0;for(int i=0;i<16;i++)internal.data[i]=-123;
  ExpCandyNextLevel(1);
  if(errors||stage!=12||gPlayerParty[2].level!=level+1||gTasks[1].func!=Task_DisplayLevelUpStatsPg1)return 1;
  for(int i=0;i<6;i++)if(internal.data[i]!=level*10+i||internal.data[i+6]!=(level+1)*10+i)return 2;
  for(int i=12;i<16;i++)if(internal.data[i]!=-123)return 3;
 }return 0;}
'''.replace('PRODUCTION', function(source, 'ExpCandyNextLevel'))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('step' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'level_steps': 199,
              'scope': 'Production ExpCandyNextLevel ordering, before/after stat snapshots, untouched remaining buffer, UI calls and native learning task dispatch. Fixture offers no bag API, so step cannot consume or insert inventory.',
              'limitations': 'Native item effect simulated. Actual move selection, evolution, RTC/ROM execution and final chain integration remain unverified.'}
    (ROOT / 'audit/exp_candy_step_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('199 inventory-free level step cases passed')


if __name__ == '__main__':
    main()
