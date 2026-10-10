"""Verify equal accepted random preimages for every fossil species."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_moemon_fossil.c').read_text(encoding='utf-8')
    table = re.search(r'sMoemonFossilSpecies\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    count = len(re.findall(r'SPECIES_\w+', table))
    fixture = r'''
typedef unsigned int u32;typedef unsigned short u16;
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
static u16 sMoemonFossilSpecies[N_SPECIES];static u32 first,calls;
static u16 Random(void){return calls++?0:(u16)first;}
PRODUCTION
int main(void){
 u32 totals[N_SPECIES];for(u32 i=0;i<N_SPECIES;i++)totals[i]=0;
 const u32 limit=65536u-65536u%N_SPECIES;
 for(first=0;first<65536;first++){
  calls=0;u32 result=RandomFossilIndex();if(result>=N_SPECIES)return 1;
  if(first<limit){if(calls!=1||result!=first%N_SPECIES)return 2;totals[result]++;}
  else if(calls!=2||result!=0)return 3;
 }
 for(u32 i=0;i<N_SPECIES;i++)if(totals[i]!=65536u/N_SPECIES)return 4;
 return 0;
}
'''.replace('N_SPECIES', str(count)).replace('PRODUCTION', function(source, 'RandomFossilIndex'))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('fossil' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'random_inputs': 65536, 'species_count': count,
              'accepted_preimages_per_species': 65536 // count,
              'limitations': 'Equal accepted input counts assume uniform game Random output; does not verify complete revival flow or GBA execution.'}
    (ROOT / 'audit/moemon_fossil_random_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('All 65536 fossil random inputs passed equal accepted-preimage and rejection checks')


if __name__ == '__main__':
    main()
