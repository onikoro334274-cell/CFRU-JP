"""Verify saturated friendship arithmetic for every byte value and u16 command."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        return function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'sp013_IncreaseDecreaseHappiness')
    production = extract((ROOT / 'src/scripting.c').read_text(encoding='utf-8'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef int s32;
#define PARTY_SIZE 6
#define MON_DATA_FRIENDSHIP 32
static u8 party[6],box;static u16 Var8003,Var8004,Var8005,Var8006;static int reads,writes,errors;
static u8* target(void){
#ifdef SELECT_FROM_PC
 if(Var8003==1)return &box;
#endif
 return &party[Var8004];
}
static u32 GetMonDataFromVar8003(u8 field){reads++;if(field!=32)errors++;return *target();}
static void SetMonDataFromVar8003(u8 field){writes++;if(field!=32||Var8005>255||Var8006)errors++;*target()=Var8005;}
PRODUCTION
int main(void){
 for(Var8003=0;Var8003<2;Var8003++)for(int current=0;current<256;current++)for(u32 command=0;command<65536;command++){
  for(int i=0;i<6;i++)party[i]=17;box=19;Var8004=current%6;Var8005=command;Var8006=777;
  *target()=current;reads=writes=errors=0;
  int magnitude=command&~256u;int expected=current+((command&256)?-magnitude:magnitude);
  if(expected<0)expected=0;if(expected>255)expected=255;
  sp013_IncreaseDecreaseHappiness();
  if(errors||reads!=1||writes!=1||Var8005!=expected||Var8006||*target()!=expected)return 1;
  for(int i=0;i<6;i++)if(&party[i]!=target()&&party[i]!=17)return 2;
  if(target()!=&box&&box!=19)return 3;
 }
 for(Var8003=0;Var8003<2;Var8003++)for(u32 id=6;id<65536;id++){
  Var8004=id;Var8005=257;Var8006=777;reads=writes=errors=0;
  sp013_IncreaseDecreaseHappiness();
  if(errors||reads||writes||Var8005!=257||Var8006!=777)return 4;
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code, pc):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('friendship' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector']
            if pc:
                command.append('-DSELECT_FROM_PC')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    for pc in (False, True):
        assert execute(fixture, pc) == 0
    baseline = subprocess.check_output(['git', 'show', 'd5d4f4c:src/scripting.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline)), False) == 1
    report = {'result': 'passed', 'arithmetic_cases': 67108864, 'invalid_party_cases': 262120,
              'baseline': 'd5d4f4c underflows unsigned friendship result and fails saturation oracle.',
              'scope': 'Production friendship special for every current byte and encoded u16 delta, PC selection enabled/disabled and both selection modes. Checks saturation, target only, preserved other slots/box, returned Var8005 and cleared Var8006. All invalid u16 party indices return without state changes.',
              'limitations': 'Controlled native data services; native Pokemon encryption and linked GBA runtime unverified.'}
    (ROOT / 'audit/script_friendship_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('67108864 friendship arithmetic cases passed; old underflow reproduced')


if __name__ == '__main__':
    main()
