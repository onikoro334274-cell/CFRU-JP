"""Verify IV script setters select the right mon and reject invalid indices."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        return function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'sp010_IVSetter')
    production = extract((ROOT / 'src/scripting.c').read_text(encoding='utf-8'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define PARTY_SIZE 6
enum{CheckIVs_HP,CheckIVs_Atk,CheckIVs_Def,CheckIVs_Spd,CheckIVs_SpAtk,CheckIVs_SpDef,SetAllIVs};
enum{MON_DATA_HP_IV=10,MON_DATA_ATK_IV,MON_DATA_DEF_IV,MON_DATA_SPEED_IV,MON_DATA_SPATK_IV,MON_DATA_SPDEF_IV};
struct Pokemon{u8 iv[6];};static struct Pokemon gPlayerParty[6],box;
static u16 Var8003,Var8004,Var8005,Var8006;static int writes,calculations,errors,selected;
static struct Pokemon* target(void){
#ifdef SELECT_FROM_PC
 if(Var8003==1)return &box;
#endif
 return &gPlayerParty[Var8004];
}
static void SetMonDataFromVar8003(u8 field){
 writes++;if(field<10||field>15||Var8005>31||(selected<6&&field!=10+selected))errors++;
 target()->iv[field-10]=Var8005;
}
static void CalculateMonStats(struct Pokemon*m){
 calculations++;if(m!=&gPlayerParty[Var8004]||writes!=(selected==6?6:1))errors++;
#ifdef SELECT_FROM_PC
 if(Var8003==1)errors++;
#endif
}
PRODUCTION
static void reset(void){writes=calculations=errors=0;for(int p=0;p<6;p++)for(int s=0;s<6;s++)gPlayerParty[p].iv[s]=17;for(int s=0;s<6;s++)box.iv[s]=19;}
int main(void){
 for(Var8003=0;Var8003<2;Var8003++)for(Var8004=0;Var8004<6;Var8004++)for(selected=0;selected<=6;selected++)for(u32 amount=0;amount<65536;amount++){
  reset();Var8005=selected;Var8006=amount;int expected=amount>31?31:amount;
  sp010_IVSetter();int shouldCalculate=1;
#ifdef SELECT_FROM_PC
  shouldCalculate=Var8003!=1;
#endif
  if(errors||writes!=(selected==6?6:1)||calculations!=shouldCalculate||Var8005!=expected)return 1;
  for(int p=0;p<6;p++)for(int s=0;s<6;s++){
   int change=target()!=&box&&p==Var8004&&(selected==6||s==selected);
   if(gPlayerParty[p].iv[s]!=(change?expected:17))return 2;
  }
  for(int s=0;s<6;s++)if(box.iv[s]!=((target()==&box&&(selected==6||s==selected))?expected:19))return 3;
 }
 for(int party=0;party<8;party++)for(u32 stat=0;stat<65536;stat++){
  if(party<6&&stat<=6)continue;
  reset();Var8003=0;Var8004=party;Var8005=stat;Var8006=65535;sp010_IVSetter();
  if(errors||writes||calculations||Var8005!=stat)return 4;
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code, pc):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('iv' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector']
            if pc:
                command.append('-DSELECT_FROM_PC')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    for pc in (False, True):
        assert execute(fixture, pc) == 0
    baseline = subprocess.check_output(['git', 'show', 'f3bd175:src/scripting.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline)), True) != 0
    report = {'result': 'passed', 'value_cases': 11010048, 'invalid_input_cases': 1048492,
              'baseline': 'f3bd175 recalculates unrelated party stats for PC selection.',
              'scope': 'Production IV setter with/without PC selection for all u16 values, six party slots, individual/all-IV modes, plus every invalid u16 stat and invalid party inputs. Checks clamp to 31, correct target, unchanged other fields, exact write count and party-only recalculation.',
              'limitations': 'Controlled native data/stats services; box coordinates, native stat calculation and linked GBA runtime unverified.'}
    (ROOT / 'audit/script_iv_updates_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('11010048 IV value and 1048492 invalid-input cases passed; old PC issue reproduced')


if __name__ == '__main__':
    main()
