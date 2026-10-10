"""Check gender item application only follows eligibility and successful consumption."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_gender_tool.c').read_text(encoding='utf-8')
    production = function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'ItemUseCB_GenderTool')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define FALSE 0
#define TRUE 1
#define MON_MALE 0
#define MON_DATA_IS_EGG 1
#define SE_SELECT 1
struct Pokemon {u32 personality;};struct BoxPokemon {u32 personality;};
static struct Pokemon gPlayerParty[6];static struct {u8 slotId;} gPartyMenu;static struct {TaskFunc func;} gTasks[2];
static u16 Var800E=123;static int gPartyMenuUseExitCallback;
static u8 gText_GenderTool_Partner[1],gText_WontHaveEffect[1],gText_GenderTool_Male[1],gText_GenderTool_Female[1];
static u8 gStringVar1[8],gStringVar4[8];
static int partner,egg,eligible,available,removed,ensures,updates,messages,searches,errors;
static void PlaySE(int n){(void)n;}
static int HackPartner_IsMon(struct Pokemon* m){(void)m;return partner;}
static int GetMonData(struct Pokemon* m,int f,void* p){(void)m;(void)f;(void)p;return egg;}
static int FindFlippedGender(struct Pokemon* m,u32* out){searches++;if(m->personality!=42)errors++;*out=84;return eligible;}
static int RemoveBagItem(u16 item,int n){removed++;if(item!=123||n!=1||gPlayerParty[2].personality!=42||ensures)errors++;return available;}
static void HackMonId_Ensure(struct BoxPokemon* m){ensures++;if(m->personality!=42||!available||removed!=1)errors++;}
static void UpdateMonDisplayInfoAfterRareCandy(u8 slot,struct Pokemon* m){updates++;if(slot!=2||m->personality!=84||ensures!=1)errors++;}
static int GetMonGender(struct Pokemon* m){(void)m;return 0;}
static void GetMonNickname(struct Pokemon* m,u8* s){(void)m;(void)s;}
static void StringExpandPlaceholders(u8* d,const u8* s){(void)d;(void)s;}
static void DisplayPartyMenuMessage(const u8* s,u8 b){(void)s;(void)b;messages++;}
static void ScheduleBgCopyTilemapToVram(int n){(void)n;}
static void done(u8 id){(void)id;}
PRODUCTION
int main(void){gPartyMenu.slotId=2;
 for(partner=0;partner<2;partner++)for(egg=0;egg<2;egg++)for(eligible=0;eligible<2;eligible++)for(available=0;available<2;available++){
  gPlayerParty[2].personality=42;removed=ensures=updates=messages=searches=errors=0;gTasks[1].func=NULL;
  ItemUseCB_GenderTool(1,done);int attempt=!partner&&!egg&&eligible,change=attempt&&available;
  if(errors||searches!=(!partner&&!egg)||removed!=attempt||ensures!=change||updates!=change||messages!=1)return 1;
  if(gPlayerParty[2].personality!=(change?84:42)||gTasks[1].func!=done)return 2;
 }return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('gender_item' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 16,
              'scope': 'Production item callback: partner/egg/eligibility/inventory combinations; no mutation on failure, consume before identity initialization, initialize identity before PID commit.',
              'limitations': 'Controlled helper and Pokemon/inventory APIs; actual search covered separately. No GBA runtime claim.'}
    (ROOT / 'audit/gender_item_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('16 gender item eligibility and consumption combinations passed')


if __name__ == '__main__':
    main()
