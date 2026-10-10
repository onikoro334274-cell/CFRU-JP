"""Check copy-special slot widths and PC-send failure propagation."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        return function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'sp01A_CopyPartyData')
    production = extract((ROOT / 'src/scripting.c').read_text(encoding='utf-8'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
struct Pokemon{u8 payload[100];};struct CompressedPokemon{u8 payload[58];};
static struct Pokemon gPlayerParty[6],gEnemyParty[6];static struct CompressedPokemon box;
static u16 Var8000=3,Var8001=7,Var8002,Var8005,gSpecialVar_LastResult;
static int sendFailed,copies,sends,gets,calculations,errors;
static void Memcpy(void*out,const void*in,u32 size){
 copies++;if(Var8005>=6){errors++;return;}
 if(Var8002==0){if(out!=&gEnemyParty[5]||in!=&gPlayerParty[Var8005]||size!=100)errors++;}
 else if(Var8002==1){if(out!=&gPlayerParty[Var8005]||in!=&gEnemyParty[5]||size!=100)errors++;}
 else if(Var8002==3){if(out!=&gEnemyParty[5]||in!=&box||size!=58)errors++;}
 else errors++;
}
static u8 SendMonToBoxPos(struct Pokemon*m,u8 b,u8 p){sends++;if(m!=&gEnemyParty[5]||b!=3||p!=7)errors++;return sendFailed;}
static void* GetBoxedMonAddr(void){gets++;return &box;}
static void CalculateMonStats(struct Pokemon*m){calculations++;if(m!=&gEnemyParty[5]||copies!=1)errors++;}
PRODUCTION
int main(void){
 for(u32 slot=0;slot<65536;slot++)for(Var8002=0;Var8002<5;Var8002++)for(sendFailed=0;sendFailed<2;sendFailed++){
  Var8005=slot;gSpecialVar_LastResult=99;copies=sends=gets=calculations=errors=0;
  sp01A_CopyPartyData();int copy=0,send=0,get=0,calc=0,result=99;
#ifdef SELECT_FROM_PC
  int valid=slot<6&&Var8002<4;result=valid?(Var8002==2?sendFailed:0):1;
  copy=valid&&(Var8002==0||Var8002==1||Var8002==3);send=valid&&Var8002==2;get=calc=valid&&Var8002==3;
#endif
  if(errors||copies!=copy||sends!=send||gets!=get||calculations!=calc||gSpecialVar_LastResult!=result||Var8005!=slot)return 1;
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code, pc):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('copy' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector']
            if pc:
                command.append('-DSELECT_FROM_PC')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    for pc in (False, True):
        assert execute(fixture, pc) == 0
    baseline = subprocess.check_output(['git', 'show', '8a307d3:src/scripting.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline)), True) == 1
    report = {'result': 'passed', 'dispatch_cases': 1310720,
              'baseline': '8a307d3 loses PC-send failure and truncates u16 slot before validation.',
              'scope': 'Actual copy-special dispatch with/without PC support, all u16 slots, four modes/invalid mode and send success/failure. Invalid slot cannot call copy/storage/stats services; PC-send result preserved. Tests existing copy directions and lengths only.',
              'limitations': 'No actual Pokemon payload copied. Compressed-to-party conversion ABI, storage coordinates, native stats and linked runtime remain unverified; valid mode-3 data conversion is not proven by this test.'}
    (ROOT / 'audit/party_copy_dispatch_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('1310720 copy-special dispatch cases passed; old failure reproduced')


if __name__ == '__main__':
    main()
