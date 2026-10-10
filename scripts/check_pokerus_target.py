"""Check Pokerus selection termination for all empty/egg/live party shapes."""
import argparse,subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--expect-trap',action='store_true');args=parser.parse_args()
    production=function((ROOT/'src/build_pokemon.c').read_text(encoding='utf-8'),'void RandomlyGivePartyPokerus(struct Pokemon *party)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned short u16;
#define PARTY_SIZE 6
#define NULL ((void*)0)
#define MON_DATA_SPECIES 0
#define MON_DATA_IS_EGG 1
#define MON_DATA_POKERUS 2
#define TRUE 1
#define FALSE 0
typedef u8 bool8;
struct Pokemon {u8 kind,pokerus;};
static const u8 gBitTable[]={1,2,4,8,16,32};
static unsigned calls,writes;static u16 trigger;static u8 immune;
static u16 Random(void){if(calls>64)__builtin_trap();if(calls++==0)return trigger;return (calls-2)%6+6;}
static unsigned GetMonData(struct Pokemon* mon,unsigned field,void* unused){return field==0?mon->kind!=0:mon->kind==1;}
static unsigned CheckPartyHasHadPokerus(struct Pokemon* party,u8 mask){return immune;}
static void SetMonData(struct Pokemon* mon,unsigned field,const u8* value){if(field!=2||mon->kind!=2)__builtin_trap();mon->pokerus=*value;writes++;}
PRODUCTION
int main(void){
    for(unsigned shape=0;shape<729;shape++)for(immune=0;immune<2;immune++){
        struct Pokemon party[6];unsigned digits=shape,eligible=0;
        for(unsigned i=0;i<6;i++){party[i].kind=digits%3;digits/=3;party[i].pokerus=0;eligible+=party[i].kind==2;}
        trigger=0x4000;calls=writes=0;RandomlyGivePartyPokerus(party);
        if(writes!=(eligible&&!immune))return 1;
        if(!eligible&&calls!=1)return 2;
        unsigned changed=0;
        for(unsigned i=0;i<6;i++){changed+=party[i].pokerus!=0;if(party[i].kind!=2&&party[i].pokerus)return 3;}
        if(changed!=writes)return 4;
    }
    for(unsigned raw=0;raw<65536;raw++){
        struct Pokemon party[6];for(unsigned i=0;i<6;i++){party[i].kind=2;party[i].pokerus=0;}
        trigger=raw;immune=0;calls=writes=0;RandomlyGivePartyPokerus(party);
        unsigned hit=raw==0x4000||raw==0x8000||raw==0xc000;
        if(writes!=hit||(!hit&&calls!=1))return 5;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'pk.c';obj=path/'pk.obj';exe=path/'pk.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        r=subprocess.run([str(exe)])
        assert r.returncode in (0xc000001d,-1073741795) if args.expect_trap else r.returncode==0,r.returncode
    print('Baseline selection exceeds RNG limit.' if args.expect_trap else 'Pokerus: 1458 party/immunity cases and all 65536 trigger rolls pass.')
if __name__=='__main__':main()
