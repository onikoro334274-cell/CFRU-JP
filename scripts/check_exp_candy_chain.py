"""Exhaustively check packed candy-chain state validity for every party slot."""
import json
from pathlib import Path
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/exp_candy.c').read_text(encoding='utf-8')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define MAX_LEVEL 200
#define PARTY_SIZE 6
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
static const u16 sExpCandyAmount[]={100,800,3000,10000,30000};
PRODUCTION
int main(void){
 for(u32 chain=0;chain<65536;chain++)for(u32 slot=0;slot<8;slot++){
  /* Independent re-encoding: valid states consist only of target, slot and five item indices. */
  int expected=0;u32 target=chain%256;
  if(target>=1&&target<=200&&slot<6)
   for(u32 candy=0;candy<5;candy++)if(chain==target+slot*256+candy*2048)expected=1;
  if(ExpCandyChainValid((u16)chain,(u8)slot)!=expected)return 1;
 }return 0;}
'''.replace('PRODUCTION', function(source, 'ExpCandyChainValid'))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('chain' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 524288,
              'scope': 'Actual packed-state validator; exhaustive halfword states and slots 0..7 against independent valid-state encoding.',
              'limitations': 'Validator only; full level-up/learn-move/evolution sequence and GBA runtime are not verified.'}
    (ROOT / 'audit/exp_candy_chain_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('524288 packed candy-chain validity cases passed')


if __name__ == '__main__':
    main()
