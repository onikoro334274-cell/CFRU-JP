"""Check box-mon search bounds, direction and egg filtering."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        return function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'sub_808B884')
    production = extract((ROOT / 'src/pokemon_storage_system.c').read_text(encoding='utf-8'))
    previous = extract(subprocess.check_output(['git', 'show', '9b7a918:src/pokemon_storage_system.c'], cwd=ROOT).decode('utf-8')).replace('sub_808B884', 'previous_search')
    fixture = r'''
typedef unsigned char u8;typedef signed char s8;typedef short s16;
#define NULL ((void*)0)
#define IN_BOX_COUNT 30
#define SPECIES_NONE 0
#define MON_DATA_IS_EGG 1
struct CompressedPokemon{struct{unsigned short species;}substruct0;u8 egg;};
struct BoxPokemon{struct{unsigned short species;}substruct0;u8 egg;};
static struct CompressedPokemon mons[30];static int expansions,errors;
static void CreateBoxMonFromCompressedMon(struct BoxPokemon*out,struct CompressedPokemon*in){
 expansions++;if(in<mons||in>=mons+30){errors++;out->substruct0.species=0;out->egg=0;return;}
 out->substruct0.species=in->substruct0.species;out->egg=in->egg;
}
static int GetBoxMonData(struct BoxPokemon*m,int field,void*p){if(field!=1||p)errors++;return m->egg;}
PRODUCTION
BASELINE
int main(void){
 for(int pattern=0;pattern<3;pattern++){
  for(int i=0;i<30;i++){mons[i].substruct0.species=pattern==0?0:pattern==1?25:i%3==0?25:0;mons[i].egg=i%2;}
  for(int current=0;current<256;current++)for(int max=0;max<256;max++)for(int flags=0;flags<256;flags++){
   expansions=errors=0;int valid=max<30&&flags<4&&(current<=max||current==255);
   int expected=-1,expectedExpansions=0;
   if(valid){int delta=flags<2?1:-1;int i=(current==255?-1:current)+delta;
    for(;i>=0&&i<=max;i+=delta){
     if(!(flags&1))expectedExpansions++;
     if(mons[i].substruct0.species&&((flags&1)||!mons[i].egg)){expected=i;break;}
    }
   }
   int actual=sub_808B884(mons,current,max,flags);
   if(errors||actual!=expected||expansions!=expectedExpansions)return 1;
   if(valid&&previous_search(mons,current,max,flags)!=actual)return 2;
  }
 }
 if(sub_808B884(NULL,0,29,0)!=-1)return 3;
 return 0;}
'''.replace('PRODUCTION', production).replace('BASELINE', previous)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('search' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'search_cases': 50331648, 'null_cases': 1,
              'scope': 'Production box search across every byte current/end/direction value and three empty/dense/sparse egg patterns. Valid results match previous production and independent search oracle; start sentinel 255 preserved. Invalid range/flags/null rejected; expansion count matches visited slots.',
              'limitations': 'Compressed decoding controlled. Actual codec, summary-screen navigation and linked GBA runtime unverified.'}
    (ROOT / 'audit/pc_mon_search_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('50331648 box-mon searches passed; valid previous results preserved')


if __name__ == '__main__':
    main()
