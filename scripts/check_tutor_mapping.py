"""Check tutor namespace dispatch using the actual generated move tables."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_vanilla_tutor.c').read_text(encoding='utf-8')
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    table = re.search(r'static const u16 sVanillaTutorMoves\[[^]]+\]\s*=\s*\{.*?\};', clean, re.S)[0]
    defines = '\n'.join(re.findall(r'^#define (?:HOENN_TUTOR_BASE|IS_HOENN_TUTOR|NUM_VANILLA_TUTORS)\b[^\n]*', clean, re.M))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#include "include/constants/moves.h"
#include "src/Tables/hack_emerald_tutor_compat.h"
DEFINES
TABLE
static int calls,last;
static u16 GetExpandedTutorMove(u8 tutor){calls++;last=tutor;return 4000+tutor;}
PRODUCTION
int main(void){
 for(int tutor=0;tutor<256;tutor++){
  calls=0;last=-1;u16 actual=HackVanillaTutor_GetMove((u8)tutor),expected;
  if(tutor<18)expected=sVanillaTutorMoves[tutor];
  else if(tutor>=64&&tutor<94)expected=sHackEmeraldTutorMoves[tutor-64];
  else expected=4000+tutor;
  if(actual!=expected)return 1;
  int delegated=!(tutor<18||(tutor>=64&&tutor<94));
  if(calls!=delegated||(delegated&&last!=tutor))return 2;
 }return 0;}
'''.replace('DEFINES', defines).replace('TABLE', table).replace('PRODUCTION', function(clean, 'HackVanillaTutor_GetMove'))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('tutor' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-I', str(ROOT), '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'tutor_inputs': 256,
              'scope': 'Actual GetMove function and move tables; vanilla 0..17, Emerald 64..93, expanded fallback 18..63 and 94..255, original fallback argument preservation.',
              'limitations': 'Does not independently certify generated move content, compatibility learnsets, underlying expanded fallback, ROM hook insertion, or GBA runtime.'}
    (ROOT / 'audit/tutor_mapping_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('All 256 tutor namespace mappings passed')


if __name__ == '__main__':
    main()
