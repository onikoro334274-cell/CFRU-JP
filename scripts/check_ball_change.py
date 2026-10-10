"""Exercise ball change cancellation and failed inventory consumption."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def execute(source):
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;
#define NULL ((void*)0)
#define ITEM_NONE 0
#define PARTY_SIZE 6
#define POCKET_POKEBALLS 2
#define MON_DATA_POKEBALL 1
struct Pokemon {u8 ball;};static struct Pokemon gPlayerParty[6];
static u16 Var800E,Var8004,gSpecialVar_LastResult;
static int pocket,available,removes,sets,errors;
static int GetPocketByItemId(u16 item){if(item!=Var800E)errors++;return pocket;}
static u8 ItemIdToBallId(u16 item){(void)item;return 7;}
static u8 GetMonData(struct Pokemon* mon,int field,void* p){(void)p;if(field!=1)errors++;return mon->ball;}
static int RemoveBagItem(u16 item,u16 n){if(item!=Var800E||n!=1||sets)errors++;removes++;return available;}
static void SetMonData(struct Pokemon* mon,int field,const void* p){if(field!=1||!available||removes!=1)errors++;sets++;mon->ball=*(const u8*)p;}
PRODUCTION
int main(void){
 for(Var8004=0;Var8004<8;Var8004++)for(Var800E=0;Var800E<2;Var800E++)
 for(pocket=1;pocket<3;pocket++)for(available=0;available<2;available++)for(int same=0;same<2;same++){
  for(int i=0;i<6;i++)gPlayerParty[i].ball=same?7:3;
  removes=sets=errors=0;gSpecialVar_LastResult=99;HackBallChange_Apply();
  int expected=Var800E==0||Var8004>=6?1:pocket!=2?2:same?3:!available?1:0;
  int attempt=Var800E!=0&&Var8004<6&&pocket==2&&!same;
  if(errors||gSpecialVar_LastResult!=expected||removes!=attempt||sets!=(expected==0))return 1;
  for(int i=0;i<6;i++)if(gPlayerParty[i].ball!=((expected==0&&i==Var8004)?7:same?7:3))return 2;
 }
 return 0;
}
'''.replace('PRODUCTION', function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'HackBallChange_Apply'))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('ball' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        return subprocess.run([str(exe)]).returncode


def main():
    source = (ROOT / 'src/hack_ball_change.c').read_text(encoding='utf-8')
    assert execute(source) == 0
    baseline = subprocess.check_output(['git', 'show', '3a5659f:src/hack_ball_change.c'], cwd=ROOT).decode('utf-8')
    assert execute(baseline) != 0
    report = {'result': 'passed', 'cases': 128, 'baseline_failure_reproduced': True,
              'scope': 'Production ball-change handler with controlled Pokemon/inventory APIs: slot boundaries, cancel, wrong pocket, same ball, missing item and successful consumption.',
              'limitations': 'Does not verify actual inventory implementation, event selection, encrypted Pokemon data, or GBA execution.'}
    (ROOT / 'audit/ball_change_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('128 ball-change scenarios passed')


if __name__ == '__main__':
    main()
