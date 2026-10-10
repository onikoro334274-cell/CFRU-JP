"""Check wild profiler delegates timing to the shared stable clock."""
import json
from pathlib import Path
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/wild_shuffle.c').read_text(encoding='utf-8')
    production = function(source, 'HackWild_Shuffle').replace('(u32*) 0x0203F800', 'storage')
    fixture = r'''
typedef unsigned int u32;
struct WildPokemonInfo {int id;};static struct WildPokemonInfo input,output;
static u32 storage[374],start,duration;static int samples,innerCalls,errors;
static u32 HackProfile_Now(void){return samples++?start+duration:start;}
static const struct WildPokemonInfo* HackWild_ShuffleInner(const struct WildPokemonInfo* p){if(p!=&input||samples!=1)errors++;innerCalls++;return &output;}
PRODUCTION
int main(void){
 static const u32 starts[]={0,100,0xFFFFFFF0u},durations[]={0,1,227,228,400};
 for(int a=0;a<3;a++)for(int b=0;b<5;b++)for(int prior=0;prior<2;prior++){
  start=starts[a];duration=durations[b];samples=innerCalls=errors=0;
  for(int i=0;i<374;i++)storage[i]=0xDEADBEEF;
  storage[370]=5;storage[371]=100;storage[372]=prior?500:0;
  if(HackWild_Shuffle(&input)!=&output||errors||samples!=2||innerCalls!=1)return 1;
  if(storage[370]!=6||storage[371]!=100+duration||storage[372]!=(prior?500:duration))return 2;
  for(int i=0;i<374;i++)if((i<370||i>372)&&storage[i]!=0xDEADBEEF)return 3;
 }return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('wild_profile' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 30,
              'scope': 'Actual profiling wrapper uses shared clock around inner dispatch; unsigned timestamp wrap, count/total/max updates and adjacent storage preservation.',
              'limitations': 'Controlled clock/inner dispatch; stable clock tested separately. Real interrupts, total accumulator wrap, timer overhead and GBA execution unverified.'}
    (ROOT / 'audit/wild_profile_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('30 wild profiling timing/accumulator scenarios passed')


if __name__ == '__main__':
    main()
