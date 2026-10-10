"""Verify EV script updates clamp correctly and recalculate only the selected party mon."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/scripting.c').read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    production = function(source, 'sp00F_EVAdderSubtracter')
    config = (ROOT / 'src/config.h').read_text(encoding='utf-8')
    cap = int(re.search(r'#define\s+EV_CAP\s+(\d+)', config)[1])
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef short s16;typedef unsigned int u32;typedef int s32;
#define PARTY_SIZE 6
#define EV_CAP CAP
enum{CheckEVs_HP,CheckEVs_Atk,CheckEVs_Def,CheckEVs_Spd,CheckEVs_SpAtk,CheckEVs_SpDef};
enum{MON_DATA_HP_EV=10,MON_DATA_ATK_EV,MON_DATA_DEF_EV,MON_DATA_SPEED_EV,MON_DATA_SPATK_EV,MON_DATA_SPDEF_EV};
struct Pokemon{u8 ev[6];};static struct Pokemon gPlayerParty[6],box;
static u16 Var8003,Var8004,Var8005,Var8006;static int reads,writes,calculations,errors,selected;
static struct Pokemon* target(void){
#ifdef SELECT_FROM_PC
 if(Var8003==1)return &box;
#endif
 return &gPlayerParty[Var8004];
}
static u8 sp007_PokemonEVContestStatsChecker(void){reads++;if(Var8005>=6)errors++;return target()->ev[Var8005];}
static void SetMonDataFromVar8003(u8 field){writes++;if(field!=10+selected||Var8005>EV_CAP)errors++;target()->ev[field-10]=Var8005;}
static void CalculateMonStats(struct Pokemon*m){calculations++;if(m!=&gPlayerParty[Var8004]||writes!=1)errors++;
#ifdef SELECT_FROM_PC
 if(Var8003==1)errors++;
#endif
}
PRODUCTION
static void reset(void){reads=writes=calculations=errors=0;for(int p=0;p<6;p++)for(int s=0;s<6;s++)gPlayerParty[p].ev[s]=17;for(int s=0;s<6;s++)box.ev[s]=19;}
int main(void){
 const int initial[]={0,1,EV_CAP-1,EV_CAP};
 for(Var8003=0;Var8003<2;Var8003++)for(selected=0;selected<6;selected++)for(int k=0;k<4;k++)for(u32 amount=0;amount<65536;amount++){
  reset();Var8004=2;Var8005=selected;Var8006=amount;target()->ev[selected]=initial[k];
  int subtract=(amount&256)!=0;int magnitude=amount&~256u;
  if(magnitude>EV_CAP)magnitude=EV_CAP;
  int expected=initial[k]+(subtract?-magnitude:magnitude);if(expected<0)expected=0;if(expected>EV_CAP)expected=EV_CAP;
  sp00F_EVAdderSubtracter();
  int shouldCalculate=1;
#ifdef SELECT_FROM_PC
  shouldCalculate=Var8003!=1;
#endif
  if(errors||reads!=1||writes!=1||calculations!=shouldCalculate||Var8005!=expected)return 1;
  for(int p=0;p<6;p++)for(int s=0;s<6;s++){
   int selectedParty=target()!=&box&&p==2&&s==selected;
   if(gPlayerParty[p].ev[s]!=(selectedParty?expected:17))return 2;
  }
  for(int s=0;s<6;s++)if(box.ev[s]!=((target()==&box&&s==selected)?expected:19))return 3;
 }
 for(int party=0;party<8;party++)for(u32 stat=0;stat<65536;stat++){
  if(party<6&&stat<6)continue;
  reset();Var8003=0;Var8004=party;Var8005=stat;Var8006=1;sp00F_EVAdderSubtracter();
  if(errors||reads||writes||calculations||Var8005!=stat)return 4;
 }return 0;}
'''.replace('CAP\n', str(cap) + '\n').replace('PRODUCTION', production)
    def execute(code, pc):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('ev' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector']
            if pc:
                command.append('-DSELECT_FROM_PC')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    for pc in (False, True):
        assert execute(fixture, pc) == 0
    baseline = subprocess.check_output(['git', 'show', '2272921:src/scripting.c'], cwd=ROOT).decode('utf-8')
    baseline = re.sub(r'/\*.*?\*/|//[^\n]*', '', baseline, flags=re.S)
    assert execute(fixture.replace(production, function(baseline, 'sp00F_EVAdderSubtracter')), False) == 1
    report = {'result': 'passed', 'arithmetic_cases': 6291456, 'invalid_input_cases': 1048504,
              'ev_cap': cap, 'baseline': '2272921 fails expected party stat recalculation.',
              'scope': 'Production EV special with/without PC selection, all encoded u16 deltas, six stats, four valid EV boundaries; checks clamping, exactly one data write, party-only stat recalculation and unrelated party/box values. Every u16 invalid stat plus invalid party inputs rejected without native calls.',
              'limitations': 'Controlled data and stats services. Native HP adjustment, NPC scripts, save interruptions and linked GBA runtime unverified.'}
    (ROOT / 'audit/script_ev_updates_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('6291456 EV arithmetic and 1048504 invalid-input cases passed; missing recalc reproduced')


if __name__ == '__main__':
    main()
