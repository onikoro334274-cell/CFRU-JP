"""Exercise legacy tutor eligibility dispatch with actual compatibility tables."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_vanilla_tutor.c').read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    defines = '\n'.join(re.findall(r'^#define (?:HOENN_TUTOR_BASE|IS_HOENN_TUTOR|NUM_VANILLA_TUTORS)\b[^\n]*', source, re.M))
    table = re.search(r'static const u16 sVanillaTutorMoves\[[^]]+\]\s*=\s*\{.*?\};', source, re.S)[0]
    production = '\n'.join(function(source, name) for name in ['FindExpandedTutorId', 'HackVanillaTutor_GetMove', 'HackVanillaTutor_CanMonLearnTMTutor'])
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define NUM_MOVE_TUTORS 152
#define POCKET_TM_HM 4
#define MON_DATA_IS_EGG 1
#define MON_DATA_SPECIES 2
enum {CAN_LEARN_MOVE,CANNOT_LEARN_MOVE,ALREADY_KNOWS_MOVE,CANNOT_LEARN_MOVE_IS_EGG};
#include "include/constants/moves.h"
#include "src/Tables/hack_vanilla_tutor_compat.h"
#include "src/Tables/hack_emerald_tutor_compat.h"
DEFINES
TABLE
struct Pokemon {u8 egg;};static u16 gTutorMoves[152];
static int tm,eligible,knows,delegates,compatCalls,errors;static u16 nat,expectedMove;
static int GetPocketByItemId(u16 item){(void)item;return tm?4:0;}
static u8 CanMonLearnTMTutor(struct Pokemon* m,u16 item,u8 tutor){(void)m;(void)item;(void)tutor;delegates++;return 9;}
static u32 GetMonData(struct Pokemon* m,int field,void* p){(void)p;return field==1?m->egg:1;}
static u16 SpeciesToNationalPokedexNum(u16 species){if(species!=1)errors++;return nat;}
static int CanMonLearnTutorMove(struct Pokemon* m,u8 tutor){(void)m;if(tutor!=7)errors++;compatCalls++;return eligible;}
static int MonKnowsMove(struct Pokemon* m,u16 move){(void)m;if(move!=expectedMove)errors++;return knows;}
static u16 GetExpandedTutorMove(u8 tutor){return 4000+tutor;}
PRODUCTION
int main(void){u16 dex[]={0,1,386,65535};
 for(int tutor=0;tutor<256;tutor++)for(tm=0;tm<2;tm++)for(int egg=0;egg<2;egg++)
 for(int found=0;found<2;found++)for(eligible=0;eligible<2;eligible++)for(knows=0;knows<2;knows++)for(int n=0;n<4;n++){
  struct Pokemon mon;mon.egg=egg;nat=dex[n];expectedMove=HackVanillaTutor_GetMove(tutor);
  for(int i=0;i<152;i++)gTutorMoves[i]=65535;if(found)gTutorMoves[7]=expectedMove;
  delegates=compatCalls=errors=0;u8 actual=HackVanillaTutor_CanMonLearnTMTutor(&mon,0,tutor);
  int legacy=tutor<18||(tutor>=64&&tutor<94),expected;
  if(tm||!legacy)expected=9;
  else if(egg)expected=3;
  else {int can=eligible;
   if(!found){if(tutor<18)can=nat>0&&nat<ARRAY_COUNT(sHackVanillaTutorCompat)&&(sHackVanillaTutorCompat[nat]&(1u<<tutor));
    else can=nat>0&&nat<ARRAY_COUNT(sHackEmeraldTutorCompat)&&(sHackEmeraldTutorCompat[nat]&(1u<<(tutor-64)));}
   expected=!can?1:knows?2:0;}
  if(errors||actual!=expected||delegates!=(tm||!legacy)||compatCalls!=(!tm&&legacy&&!egg&&found))return 1;
 }return 0;}
'''.replace('DEFINES', defines).replace('TABLE', table).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('eligibility' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-I', str(ROOT), '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 32768,
              'scope': 'Actual wrapper and compatibility tables: all tutor inputs, TM delegation, egg rejection, expanded lookup vs legacy fallback, learnability, known moves and national dex boundaries.',
              'limitations': 'Controlled Pokemon and expanded-tutor APIs; does not independently certify learnsets, species-to-dex conversion, hook insertion or GBA runtime.'}
    (ROOT / 'audit/tutor_eligibility_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('32768 tutor eligibility dispatch scenarios passed')


if __name__ == '__main__':
    main()
