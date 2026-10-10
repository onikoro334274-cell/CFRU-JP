"""Check friendship-max item eligibility and consumption before mutation."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_danger_sugar.c').read_text(encoding='utf-8')
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    production = function(clean, 'ItemUseCB_DangerSugar')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define FRIENDSHIP_MAX 255
#define SE_SELECT 1
enum{MON_DATA_IS_EGG,MON_DATA_FRIENDSHIP};
struct Pokemon{u8 egg,friendship;};static struct Pokemon gPlayerParty[6];
static struct{u8 slotId;}gPartyMenu;static struct{TaskFunc func;}gTasks[2];
static u16 Var800E=841;static int gPartyMenuUseExitCallback;
static u8 gStringVar1[8],gStringVar4[8],gText_DangerSugar_Used[1],gText_WontHaveEffect[1];
static int removeOK,removes,writes,names,expanded,messages,errors,success;
static void done(u8 id){(void)id;}
static void PlaySE(int n){if(n!=1)errors++;}
static u8 GetMonData(struct Pokemon*m,int f,void*p){if(p)errors++;return f==MON_DATA_IS_EGG?m->egg:m->friendship;}
static int RemoveBagItem(u16 item,int count){removes++;if(item!=841||count!=1||writes)errors++;return removeOK;}
static void SetMonData(struct Pokemon*m,int f,const void*p){writes++;if(f!=MON_DATA_FRIENDSHIP||!removeOK||removes!=1)errors++;m->friendship=*(const u8*)p;}
static void GetMonNickname(struct Pokemon*m,u8*s){(void)m;names++;if(s!=gStringVar1)errors++;}
static void StringExpandPlaceholders(u8*s,const u8*t){expanded++;if(s!=gStringVar4||t!=gText_DangerSugar_Used)errors++;}
static void DisplayPartyMenuMessage(const u8*s,int b){messages++;if(b!=1||s!=(success?gStringVar4:gText_WontHaveEffect))errors++;}
static void ScheduleBgCopyTilemapToVram(int bg){if(bg!=2)errors++;}
PRODUCTION
int main(void){
 for(int slot=0;slot<6;slot++)for(int friendship=0;friendship<256;friendship++)for(int egg=0;egg<2;egg++)for(removeOK=0;removeOK<2;removeOK++){
  for(int i=0;i<6;i++){gPlayerParty[i].friendship=17;gPlayerParty[i].egg=0;}
  gPartyMenu.slotId=slot;gPlayerParty[slot].egg=egg;gPlayerParty[slot].friendship=friendship;
  removes=writes=names=expanded=messages=errors=0;gPartyMenuUseExitCallback=1;gTasks[1].func=0;
  int eligible=!egg&&friendship<255;success=eligible&&removeOK;
  ItemUseCB_DangerSugar(1,done);
  if(errors||removes!=eligible||writes!=success||names!=success||expanded!=success||messages!=1||gTasks[1].func!=done)return 1;
  if(gPlayerParty[slot].friendship!=(success?255:friendship)||(!success&&gPartyMenuUseExitCallback))return 2;
  for(int i=0;i<6;i++)if(i!=slot&&gPlayerParty[i].friendship!=17)return 3;
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('sugar' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', '86949c7:src/hack_danger_sugar.c'], cwd=ROOT).decode('utf-8')
    baseline = re.sub(r'/\*.*?\*/|//[^\n]*', '', baseline, flags=re.S)
    assert execute(fixture.replace(production, function(baseline, 'ItemUseCB_DangerSugar'))) == 1
    report = {'result': 'passed', 'cases': 6144,
              'baseline': '86949c7 fails inventory/mutation ordering check.',
              'scope': 'Actual sugar callback for all six slots, 256 friendship values, egg/non-egg and consumption success/failure. Consumes only for eligible mons, sets friendship only after successful consumption, preserves other party slots, routes messages and task callback.',
              'limitations': 'Controlled native services; rendered party UI and linked GBA runtime unverified.'}
    (ROOT / 'audit/danger_sugar_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('6144 friendship sugar cases passed; old failure reproduced')


if __name__ == '__main__':
    main()
