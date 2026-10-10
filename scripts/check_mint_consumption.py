"""Verify mint confirmation never changes nature/stats without consumption."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
        return function(source[source.index('static void Task_ChangeNature(u8 taskId)\n{'):], 'Task_ChangeNature')
    production = extract((ROOT / 'src/party_menu.c').read_text(encoding='utf-8'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef void(*TaskFunc)(u8);
#define TRUE 1
#define FALSE 0
#define SE_USE_ITEM 2
struct Pokemon{u8 natureMint,unrelated;};static struct Pokemon gPlayerParty[6];
static struct{u8 slotId;}gPartyMenu;static struct{TaskFunc func;}gTasks[2];
static u16 Var800E=77;static int gPartyMenuUseExitCallback;
static u8 gStringVar1[8],gStringVar4[8],gText_MintUsed[1],gText_WontHaveEffect[1];
static int original,nature,removeOK,removes,stats,names,sounds,messages,errors;
static void Task_ClosePartyMenuAfterText(u8 id){(void)id;}
static int RemoveBagItem(u16 item,int n){removes++;if(item!=77||n!=1||gPlayerParty[gPartyMenu.slotId].natureMint!=original||stats)errors++;return removeOK;}
static void PlaySE(int n){sounds++;if(n!=2||!removeOK||removes!=1)errors++;}
static void GetMonNickname(struct Pokemon*m,u8*s){(void)m;names++;if(s!=gStringVar1||!removeOK)errors++;}
static u8 ItemId_GetMystery2(u16 item){if(item!=77)errors++;return nature;}
static void CalculateMonStats(struct Pokemon*m){stats++;if(!removeOK||removes!=1||m->natureMint!=nature+1)errors++;}
static void StringExpandPlaceholders(u8*s,const u8*t){if(s!=gStringVar4||t!=gText_MintUsed||!removeOK)errors++;}
static void DisplayPartyMenuMessage(const u8*s,int b){messages++;if(b!=1||s!=(removeOK?gStringVar4:gText_WontHaveEffect))errors++;}
static void ScheduleBgCopyTilemapToVram(int bg){if(bg!=2)errors++;}
PRODUCTION
int main(void){
 for(int slot=0;slot<6;slot++)for(original=0;original<=25;original++)for(nature=0;nature<25;nature++)for(removeOK=0;removeOK<2;removeOK++){
  for(int i=0;i<6;i++){gPlayerParty[i].natureMint=5;gPlayerParty[i].unrelated=123;}
  gPartyMenu.slotId=slot;gPlayerParty[slot].natureMint=original;
  removes=stats=names=sounds=messages=errors=0;gTasks[1].func=0;gPartyMenuUseExitCallback=1;
  Task_ChangeNature(1);
  if(errors||removes!=1||stats!=removeOK||names!=removeOK||sounds!=removeOK||messages!=1)return 1;
  if(gTasks[1].func!=Task_ClosePartyMenuAfterText||(!removeOK&&gPartyMenuUseExitCallback))return 2;
  if(gPlayerParty[slot].natureMint!=(removeOK?nature+1:original))return 3;
  for(int i=0;i<6;i++)if(gPlayerParty[i].unrelated!=123||(i!=slot&&gPlayerParty[i].natureMint!=5))return 4;
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('mint' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', '507c651:src/party_menu.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline))) != 0
    report = {'result': 'passed', 'cases': 7800, 'baseline': '507c651 fails mutation/consumption ordering.',
              'scope': 'Production mint confirmation across six slots, 26 original mint states, 25 nature parameters and consumption success/failure. Checks stats after consumption, nature mapping, unaffected other slots/fields and UI/task routing.',
              'limitations': 'Initial mint eligibility and native rendered UI unverified; controlled services.'}
    (ROOT / 'audit/mint_consumption_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('7800 mint consumption cases passed; old failure reproduced')


if __name__ == '__main__':
    main()
