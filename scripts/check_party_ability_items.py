"""Check ability and Gigantamax confirmation tasks consume before mutation."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/party_menu.c').read_text(encoding='utf-8')
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    names = ['Task_ChangeAbility', 'Task_ChangeGigantamax']
    # Select definitions after their prototypes and earlier references.
    def extract(text):
        return '\n'.join(function(text[text.index('static void ' + name + '(u8 taskId)\n{'):], name) for name in names)
    production = extract(clean)
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define SE_USE_ITEM 2
#define MON_DATA_ALT_ABILITY 1
struct Pokemon{u8 hiddenAbility,alt,gigantamax,untouched;};static struct Pokemon gPlayerParty[6];
static struct{u8 slotId;}gPartyMenu;static struct{TaskFunc func;}gTasks[2];
static u16 Var800E=55;static int gPartyMenuUseExitCallback;
static u8 gStringVar1[8],gStringVar2[8],gStringVar4[8],gText_WontHaveEffect[1],gText_AbilityCapsuleChangedAbility[1],gText_MaxPowderAllowsGigantamax[1],gText_MaxPowderRemovesGigantamax[1];
static int removeOK,type,removes,stats,writes,sounds,names,messages,errors;
static struct Pokemon before;
static void Task_ClosePartyMenuAfterText(u8 id){(void)id;}
static int ItemId_GetHoldEffectParam(u16 item){if(item!=55)errors++;return type;}
static int RemoveBagItem(u16 item,int n){
 removes++;struct Pokemon*m=&gPlayerParty[gPartyMenu.slotId];
 if(item!=55||n!=1||m->hiddenAbility!=before.hiddenAbility||m->alt!=before.alt||m->gigantamax!=before.gigantamax)errors++;
 return removeOK;
}
static void PlaySE(int n){sounds++;if(n!=2||!removeOK||removes!=1)errors++;}
static void CalculateMonStats(struct Pokemon*m){(void)m;stats++;if(!removeOK)errors++;}
static u8 GetMonData(struct Pokemon*m,int f,void*p){if(f!=1||p)errors++;return m->alt;}
static void SetMonData(struct Pokemon*m,int f,const void*p){writes++;if(f!=1||!removeOK||removes!=1)errors++;m->alt=*(const u8*)p;}
static void GetMonNickname(struct Pokemon*m,u8*s){(void)m;names++;if(s!=gStringVar1||!removeOK)errors++;}
static u16 GetMonAbility(struct Pokemon*m){return m->hiddenAbility?3:m->alt+1;}
static void CopyAbilityName(u8*s,u16 a){if(s!=gStringVar2||a!=GetMonAbility(&gPlayerParty[gPartyMenu.slotId]))errors++;}
static void StringExpandPlaceholders(u8*s,const u8*t){if(s!=gStringVar4||!removeOK)errors++;(void)t;}
static void DisplayPartyMenuMessage(const u8*s,int b){messages++;if(b!=1||s!=(removeOK?gStringVar4:gText_WontHaveEffect))errors++;}
static void ScheduleBgCopyTilemapToVram(int bg){if(bg!=2)errors++;}
PRODUCTION
static void reset(int slot,int hidden,int alt,int giant){
 for(int i=0;i<6;i++){gPlayerParty[i].hiddenAbility=0;gPlayerParty[i].alt=0;gPlayerParty[i].gigantamax=0;gPlayerParty[i].untouched=77;}
 gPartyMenu.slotId=slot;gPlayerParty[slot].hiddenAbility=hidden;gPlayerParty[slot].alt=alt;gPlayerParty[slot].gigantamax=giant;before=gPlayerParty[slot];
 removes=stats=writes=sounds=names=messages=errors=0;gTasks[1].func=0;gPartyMenuUseExitCallback=1;
}
static int common(void){
 if(errors||removes!=1||sounds!=removeOK||names!=removeOK||messages!=1||gTasks[1].func!=Task_ClosePartyMenuAfterText)return 1;
 if(!removeOK&&gPartyMenuUseExitCallback)return 2;
 for(int i=0;i<6;i++)if(gPlayerParty[i].untouched!=77||(i!=gPartyMenu.slotId&&(gPlayerParty[i].hiddenAbility||gPlayerParty[i].alt||gPlayerParty[i].gigantamax)))return 3;
 return 0;
}
int main(void){
 for(int slot=0;slot<6;slot++)for(int hidden=0;hidden<2;hidden++)for(int alt=0;alt<2;alt++)for(type=0;type<2;type++)for(removeOK=0;removeOK<2;removeOK++){
  reset(slot,hidden,alt,1);Task_ChangeAbility(1);struct Pokemon*m=&gPlayerParty[slot];
  int expectedHidden=removeOK?(hidden?0:type):hidden;
  int expectedAlt=removeOK&&!hidden&&!type?(alt^1):alt;
  if(common()||m->hiddenAbility!=expectedHidden||m->alt!=expectedAlt||m->gigantamax!=1)return 1;
  if(stats!=(removeOK&&(hidden||!type))||writes!=(removeOK&&!hidden&&!type))return 2;
 }
 for(int slot=0;slot<6;slot++)for(int giant=0;giant<2;giant++)for(removeOK=0;removeOK<2;removeOK++){
  reset(slot,1,1,giant);Task_ChangeGigantamax(1);struct Pokemon*m=&gPlayerParty[slot];
  if(common()||m->gigantamax!=(giant^removeOK)||m->hiddenAbility!=1||m->alt!=1||stats||writes)return 3;
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('items' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', '6673730:src/party_menu.c'], cwd=ROOT).decode('utf-8')
    baseline = re.sub(r'/\*.*?\*/|//[^\n]*', '', baseline, flags=re.S)
    assert execute(fixture.replace(production, extract(baseline))) != 0
    report = {'result': 'passed', 'ability_cases': 96, 'gigantamax_cases': 24,
              'baseline': '6673730 fails consumption/mutation checks.',
              'scope': 'Production confirmation tasks: all party slots, hidden/ordinary ability branches, capsule/patch parameter, both ability bits and Gigantamax states, successful/failed consumption. Checks mutation ordering, untouched fields/other slots, stats calls and failure/success UI dispatch.',
              'limitations': 'Initial eligibility/confirmation menus and linked GBA UI unverified; controlled native services.'}
    (ROOT / 'audit/party_ability_items_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('96 ability and 24 Gigantamax confirmation cases passed; old failure reproduced')


if __name__ == '__main__':
    main()
