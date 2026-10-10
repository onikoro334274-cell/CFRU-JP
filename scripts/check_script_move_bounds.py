"""Exercise script move changes at every u16 move-slot input."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/scripting.c').read_text(encoding='utf-8')
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    production = function(clean, 'sp017_ChangePokemonAttacks')
    moves = (ROOT / 'include/constants/moves.h').read_text(encoding='utf-8')
    last_name = re.search(r'#define\s+LAST_MOVE_INDEX\s+(\w+)', moves)[1]
    last = int(re.search(r'#define\s+' + last_name + r'\s+(0x[0-9A-Fa-f]+|\d+)', moves)[1], 0)
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define PARTY_SIZE 6
#define MAX_MON_MOVES 4
#define MOVE_NONE 0
#define LAST_MOVE_INDEX LAST
struct Pokemon{u16 moves[4];};static struct Pokemon gPlayerParty[6];
static u16 Var8004,Var8005,Var8006;
static int empty,gives,sets,bonuses,deletes,errors;
static u16 GiveMoveToMon(struct Pokemon*m,u16 move){
 gives++;if(m!=&gPlayerParty[Var8004]||Var8004>=6||move!=Var8006)errors++;
 if(empty){m->moves[0]=move;return move;}return 65535;
}
static void SetMonMoveSlot(struct Pokemon*m,u16 move,u8 slot){
 sets++;if(Var8005>=4||slot!=Var8005||m!=&gPlayerParty[Var8004]||move!=Var8006){errors++;return;}
 m->moves[slot]=move;
}
static void RemoveMonPPBonus(struct Pokemon*m,u8 slot){bonuses++;if(m!=&gPlayerParty[Var8004]||Var8005>=4||slot!=Var8005||sets!=1)errors++;}
static void Special_0DD_DeleteMove(void){deletes++;if(Var8004>=6||Var8005>=4){errors++;return;}gPlayerParty[Var8004].moves[Var8005]=0;}
PRODUCTION
int main(void){
 const u16 moveCases[]={0,1,LAST_MOVE_INDEX,LAST_MOVE_INDEX+1,65535};
 for(int party=0;party<8;party++)for(u32 slot=0;slot<65536;slot++)for(int k=0;k<5;k++)for(empty=0;empty<2;empty++){
  Var8004=party;Var8005=slot;Var8006=moveCases[k];gives=sets=bonuses=deletes=errors=0;
  for(int p=0;p<6;p++)for(int s=0;s<4;s++)gPlayerParty[p].moves[s]=17;
  sp017_ChangePokemonAttacks();
  int valid=party<6&&slot<4&&Var8006<=LAST_MOVE_INDEX;
  int give=valid&&Var8006!=0,replace=give&&!empty,remove=valid&&Var8006==0;
  if(errors||gives!=give||sets!=replace||bonuses!=replace||deletes!=remove)return 1;
  for(int p=0;p<6;p++)for(int s=0;s<4;s++){
   u16 expected=17;
   if(valid&&p==party){if(remove&&s==slot)expected=0;else if(give&&s==(empty?0:slot))expected=Var8006;}
   if(gPlayerParty[p].moves[s]!=expected)return 2;
  }
 }return 0;}
'''.replace('LAST\n', str(last) + '\n').replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('moves' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', 'd908b41:src/scripting.c'], cwd=ROOT).decode('utf-8')
    baseline = re.sub(r'/\*.*?\*/|//[^\n]*', '', baseline, flags=re.S)
    assert execute(fixture.replace(production, function(baseline, 'sp017_ChangePokemonAttacks'))) == 1
    report = {'result': 'passed', 'cases': 5242880, 'last_move_index': last,
              'baseline': 'd908b41 permits invalid move slot 4 and fails bounded service checks.',
              'scope': 'Production script API for every u16 slot, six valid/two invalid party indices, delete/first/last/invalid move IDs and empty/full movesets. Invalid inputs cannot dispatch native services; valid insertion/replacement/deletion preserves unrelated slots and PP-bonus routing.',
              'limitations': 'Controlled native services; native deletion compaction, PP implementation and linked GBA runtime unverified.'}
    (ROOT / 'audit/script_move_bounds_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('5242880 script move bounds/routing cases passed; slot-4 failure reproduced')


if __name__ == '__main__':
    main()
