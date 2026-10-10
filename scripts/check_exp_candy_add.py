"""Check saturating candy experience arithmetic against a 64-bit oracle."""
from pathlib import Path
import json
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/exp_candy.c').read_text(encoding='utf-8')
    fixture = r'''
typedef unsigned int u32;typedef unsigned long long u64;
PRODUCTION
static int check(u32 exp,u32 amount,u32 max){
 u64 sum=(u64)exp+amount;u32 expected=sum>max?max:(u32)sum;
 return AddCandyExperience(exp,amount,max)!=expected;
}
int main(void){
 static const u32 values[]={0,1,7,100,800,3000,10000,30000,10000000,0x7FFFFFFF,0xFFFFFFFE,0xFFFFFFFF};
 for(int a=0;a<12;a++)for(int b=0;b<12;b++)for(int c=0;c<12;c++)if(check(values[a],values[b],values[c]))return 1;
 u32 rng=0xDEADBEEFu;
 for(int i=0;i<1000000;i++){
  rng=rng*1664525u+1013904223u;u32 exp=rng;
  rng=rng*1664525u+1013904223u;u32 amount=rng;
  rng=rng*1664525u+1013904223u;u32 max=rng;
  if(check(exp,amount,max))return 2;
 }
 /* The old expression wrapped this addition to 99. */
 if(AddCandyExperience(0xFFFFFFFFu,100,10000000)!=10000000)return 3;
 return 0;}
'''.replace('PRODUCTION', function(source, 'AddCandyExperience'))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('exp' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 1001728,
              'scope': 'Production saturating add against independent 64-bit sum/min oracle; boundary grid and deterministic randomized inputs.',
              'limitations': 'Arithmetic only; actual growth tables, item consumption, level-up/learn-move chain and GBA runtime remain unverified.'}
    (ROOT / 'audit/exp_candy_add_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('1001728 candy experience arithmetic cases passed')


if __name__ == '__main__':
    main()
