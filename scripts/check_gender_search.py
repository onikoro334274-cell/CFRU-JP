"""Compare optimized gender search with its prior implementation."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_gender_tool.c').read_text(encoding='utf-8')
    original = subprocess.check_output(['git', 'show', 'ceab618:src/hack_gender_tool.c'], cwd=ROOT).decode('utf-8')
    functions = []
    for text, name in [(source, 'FlipGender'), (original, 'OriginalFlipGender')]:
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
        functions.append(function(text, 'FlipGender').replace('FlipGender(', name + '('))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define FALSE 0
#define TRUE 1
#define MON_MALE 0
#define MON_FEMALE 254
#define MON_GENDERLESS 255
#define NUM_NATURES 25
#define MON_DATA_SPECIES 1
struct Pokemon {u32 personality;};static struct {u8 genderRatio;} gBaseStats[2];
static u16 GetMonData(struct Pokemon* m,int field,void* p){(void)m;(void)field;(void)p;return 1;}
static u8 GetGenderFromSpeciesAndPersonality(u16 species,u32 pid){
 u8 ratio=gBaseStats[species].genderRatio;if(ratio==255)return 255;
 if(ratio==254)return 254;return (pid&255)<ratio?254:0;}
PRODUCTION
int main(void){
 for(int ratio=0;ratio<256;ratio++)for(u32 i=0;i<256;i++){
  gBaseStats[1].genderRatio=ratio;struct Pokemon a,b;a.personality=b.personality=i*0x9E3779B1u;
  u32 pid=a.personality;int x=FlipGender(&a),y=OriginalFlipGender(&b);
  if(x!=y||a.personality!=b.personality)return 1;
  if(x){if((a.personality&1)!=(pid&1)||a.personality%25!=pid%25)return 2;
   if(((a.personality>>16)^(a.personality&65535))!=((pid>>16)^(pid&65535)))return 3;
   if(GetGenderFromSpeciesAndPersonality(1,a.personality)==GetGenderFromSpeciesAndPersonality(1,pid))return 4;}
  else if(a.personality!=pid)return 5;
 }
 return 0;
}
'''.replace('PRODUCTION', '\n'.join(functions))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('gender' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 65536, 'baseline': 'ceab618',
              'scope': 'Actual gender search functions compared across all gender ratios and 256 seeded personalities; identical first chosen PID, ability bit, nature, shiny XOR and opposite gender.',
              'limitations': 'Mock gender/Pokemon APIs; not all personalities, actual species table, item consumption or GBA runtime. No runtime timing claim.'}
    (ROOT / 'audit/gender_search_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('65536 optimized/original gender search cases passed')


if __name__ == '__main__':
    main()
