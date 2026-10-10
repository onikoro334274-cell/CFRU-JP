"""Check ordinary rare-candy cap routing and inventory failure handling."""
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
    production = function(clean, 'ItemUseCB_RareCandy')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef u8 bool8;typedef void(*TaskFunc)(u8);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define SE_SELECT 1
#define MON_DATA_LEVEL 1
#define EVO_MODE_NORMAL 0
#define SPECIES_NONE 0
struct Pokemon{u8 level;};static struct Pokemon gPlayerParty[6];
static struct{u8 slotId;}gPartyMenu;static struct{TaskFunc func;}gTasks[2];
static u16 gSpecialVar_ItemId=105;static int gPartyMenuUseExitCallback;
static u8 gText_WontHaveEffect[1];
static int hasItem,removeOK,canEvolve,itemNoEffect,hardFlag,checks,removes,evolves,steps,messages,errors,probes;
static void done(u8 id){(void)id;}
static void PlaySE(int n){if(n!=1)errors++;}
static u8 GetMonData(struct Pokemon*m,int f,void*p){if(f!=1||p)errors++;return m->level;}
static int CheckBagHasItem(u16 item,int count){checks++;if(item!=105||count!=1)errors++;return hasItem;}
static int RemoveBagItem(u16 item,int count){removes++;if(item!=105||count!=1)errors++;return removeOK;}
static int GetEvolutionTargetSpecies(struct Pokemon*m,int mode,int item){(void)m;if(mode||item)errors++;return canEvolve;}
static void PartyMenuTryEvolution(u8 id){if(id!=1)errors++;evolves++;}
static int PokemonItemUseNoEffect(struct Pokemon*m,u16 item,u8 slot,int n){(void)m;probes++;if(item!=105||slot!=2||n)errors++;return itemNoEffect;}
static void DisplayPartyMenuMessage(const u8*s,int b){messages++;if(s!=gText_WontHaveEffect||b!=1)errors++;}
static void ScheduleBgCopyTilemapToVram(int n){if(n!=2)errors++;}
static void ItemUseCB_RareCandyStep(u8 id,TaskFunc f){steps++;if(id!=1||f!=done||!hasItem)errors++;}
static int FlagGet(int flag){if(flag!=9)errors++;return hardFlag;}
static u8 GetCurrentLevelCap(void){return 80;}
PRODUCTION
int main(void){gPartyMenu.slotId=2;
 for(int level=0;level<256;level++)for(hasItem=0;hasItem<2;hasItem++)for(removeOK=0;removeOK<2;removeOK++)
 for(canEvolve=0;canEvolve<2;canEvolve++)for(itemNoEffect=0;itemNoEffect<2;itemNoEffect++)for(hardFlag=0;hardFlag<2;hardFlag++){
  checks=removes=evolves=steps=messages=errors=probes=0;gTasks[1].func=0;gPartyMenuUseExitCallback=1;
  gPlayerParty[2].level=level;ItemUseCB_RareCandy(1,done);
  int capped=level>=100;
#ifdef FLAG_HARD_LEVEL_CAP
  capped|=hardFlag&&level>=80;
#endif
  int expectedEvolve=hasItem&&capped&&canEvolve&&removeOK;
  int expectedStep=hasItem&&!capped&&!itemNoEffect;
  int failure=!expectedEvolve&&!expectedStep;
  if(errors||checks!=1||evolves!=expectedEvolve||steps!=expectedStep||messages!=failure)return 1;
  if(removes!=(hasItem&&capped&&canEvolve)||probes!=(hasItem&&!capped))return 2;
  if(failure&&(gTasks[1].func!=done||gPartyMenuUseExitCallback))return 3;
 }return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        for hard in (False, True):
            c, obj, exe = (Path(temp) / ('rare' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(fixture, encoding='utf-8')
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector']
            if hard:
                command.append('-DFLAG_HARD_LEVEL_CAP=9')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'entry_cases': 16384,
              'scope': 'Production ordinary rare-candy entry with/without hard-cap compile option, all byte levels, inventory existence/removal failures, eligibility and evolution branches. Cap remains 100; optional lower cap routes to evolution only. Missing item never dispatches effect/evolution; evolution requires successful consumption.',
              'limitations': 'Native RareCandyStep and evolution scenes simulated. Native level-up inventory removal remains synchronous after the entry inventory check.'}
    (ROOT / 'audit/rare_candy_entry_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('16384 ordinary rare-candy entry cases passed')


if __name__ == '__main__':
    main()
