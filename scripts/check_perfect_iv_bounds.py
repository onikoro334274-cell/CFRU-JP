"""Exercise perfect-IV targets and trap nontermination on impossible requests."""
import argparse
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--expect-trap',action='store_true');args=parser.parse_args()
    production=function((ROOT/'src/build_pokemon.c').read_text(encoding='utf-8'),
        'void GiveMonXPerfectIVs(struct Pokemon* mon, u8 totalPerfectStats)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool8;
#define NUM_STATS 6
#define MON_DATA_HP_IV 0
#define TRUE 1
#define NULL ((void*)0)
struct Pokemon {u8 iv[6];u32 canary;};
static unsigned calls,writes;
static u32 MathMin(u32 a,u32 b){return a<b?a:b;}
static u32 GetMonData(struct Pokemon* mon,unsigned field,void* unused){if(field>=6)__builtin_trap();return mon->iv[field];}
static void SetMonData(struct Pokemon* mon,unsigned field,const u8* value){if(field>=6)__builtin_trap();mon->iv[field]=*value;writes++;}
static unsigned Random(void){if(calls>=64)__builtin_trap();return calls++%6;}
PRODUCTION
int main(void){
    for(unsigned mask=0;mask<64;mask++)for(unsigned target=0;target<256;target++){
        struct Pokemon mon;unsigned initial=0;
        for(unsigned i=0;i<6;i++){mon.iv[i]=(mask&(1u<<i))?31:i+7;initial+=mon.iv[i]==31;}
        mon.canary=0xa55af00d;calls=writes=0;
        GiveMonXPerfectIVs(&mon,target);
        unsigned expected=target>6?6:target;if(expected<initial)expected=initial;
        unsigned actual=0;
        for(unsigned i=0;i<6;i++){
            actual+=mon.iv[i]==31;
            if((mask&(1u<<i))&&mon.iv[i]!=31)return 1;
            if(mon.iv[i]!=31&&mon.iv[i]!=i+7)return 2;
        }
        if(actual!=expected||writes!=expected-initial||mon.canary!=0xa55af00d)return 3;
        if(target<=initial&&calls)return 4;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'iv.c';obj=path/'iv.obj';exe=path/'iv.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        result=subprocess.run([str(exe)])
        if args.expect_trap:
            assert result.returncode in (0xC000001D,-1073741795),result.returncode
            print('Baseline impossible IV request exceeds controlled RNG call limit as expected.')
        else:
            assert result.returncode==0,result.returncode
            print('Perfect IVs: all 16384 initial-perfect-mask/target cases terminate and preserve existing IVs/canary.')
if __name__=='__main__':main()
