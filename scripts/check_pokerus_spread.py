"""Check empty-slot protection and original adjacent-only Pokerus propagation."""
import argparse,subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--expect-failure',action='store_true');args=parser.parse_args()
    production=function((ROOT/'src/build_pokemon.c').read_text(encoding='utf-8'),'void PartySpreadPokerus(struct Pokemon *party)')
    fixture=r'''
typedef unsigned char u8;
#define PARTY_SIZE 6
#define MON_DATA_SPECIES 0
#define MON_DATA_POKERUS 1
#define NULL ((void*)0)
struct Pokemon {u8 occupied,pokerus;};
static struct Pokemon* base;static unsigned calls;static unsigned roll;
static unsigned Random(void){calls++;return roll;}
static unsigned GetMonData(struct Pokemon* mon,unsigned field,void* unused){if(mon<base||mon>=base+6)__builtin_trap();return field==0?mon->occupied:mon->pokerus;}
static void SetMonData(struct Pokemon* mon,unsigned field,const u8* value){if(mon<base||mon>=base+6||field!=1)__builtin_trap();mon->pokerus=*value;}
PRODUCTION
int main(void){
    for(unsigned shape=0;shape<4096;shape++)for(roll=0;roll<3;roll++){
        struct Pokemon party[6],expected[6];unsigned digits=shape;
        for(unsigned i=0;i<6;i++){
            unsigned state=digits%4;digits/=4;
            party[i].occupied=state!=0;party[i].pokerus=state==2?0x23:state==3?0x20:0;
            expected[i]=party[i];
        }
        if(roll==0)for(unsigned i=0;i<6;i++){
            if(!expected[i].occupied||!(expected[i].pokerus%16))continue;
            u8 strain=expected[i].pokerus;
            if(i>0&&expected[i-1].occupied&&expected[i-1].pokerus<16)expected[i-1].pokerus=strain;
            if(i+1<6&&expected[i+1].occupied&&expected[i+1].pokerus<16){expected[i+1].pokerus=strain;i++;}
        }
        base=party;calls=0;PartySpreadPokerus(party);
        if(calls!=1)return 2;
        for(unsigned i=0;i<6;i++)if(party[i].occupied!=expected[i].occupied||party[i].pokerus!=expected[i].pokerus)return 1;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'spread.c';obj=path/'spread.obj';exe=path/'spread.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        r=subprocess.run([str(exe)]);assert r.returncode==(1 if args.expect_failure else 0),r.returncode
    print('Baseline writes to an empty party slot.' if args.expect_failure else 'Pokerus spread: 12288 party-shape/roll cases pass adjacent-only and empty/cured protections.')
if __name__=='__main__':main()
