"""Execute every encoded Pokerus byte against every u16 elapsed day count."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
    production=function((ROOT/'src/build_pokemon.c').read_text(encoding='utf-8'),'void UpdatePartyPokerusTime(u16 days)')
    fixture=r'''
typedef unsigned char u8;typedef unsigned short u16;
#define PARTY_SIZE 6
#define MON_DATA_SPECIES 0
#define MON_DATA_POKERUS 1
#define NULL ((void*)0)
static struct {u8 occupied,pokerus;} gPlayerParty[6];
static unsigned writes;
static unsigned GetMonData(void* mon,unsigned field,void* unused){
    for(unsigned i=0;i<6;i++)if(mon==&gPlayerParty[i])return field==0?gPlayerParty[i].occupied:gPlayerParty[i].pokerus;
    __builtin_trap();
}
static void SetMonData(void* mon,unsigned field,const u8* value){
    if(field!=1)__builtin_trap();
    for(unsigned i=0;i<6;i++)if(mon==&gPlayerParty[i]){gPlayerParty[i].pokerus=*value;writes++;return;}
    __builtin_trap();
}
PRODUCTION
int main(void){
    for(unsigned encoded=0;encoded<256;encoded++)for(unsigned days=0;days<65536;days++){
        for(unsigned i=0;i<6;i++){gPlayerParty[i].occupied=i%2;gPlayerParty[i].pokerus=encoded;}
        writes=0;UpdatePartyPokerusTime(days);
        unsigned duration=encoded%16,strain=encoded/16;
        unsigned remaining=days>4||days>=duration?0:duration-days;
        unsigned expected=encoded;
        if(duration){expected=strain*16+remaining;if(!expected)expected=16;}
        for(unsigned i=0;i<6;i++){
            if(gPlayerParty[i].occupied!=i%2)return 1;
            if(gPlayerParty[i].pokerus!=(i%2?expected:encoded))return 2;
        }
        if(writes!=(duration?3:0))return 3;
    }
    return 0;
}
'''.replace('PRODUCTION',production)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'time.c';obj=path/'time.obj';exe=path/'time.exe';c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
        subprocess.run([str(exe)],check=True)
    print('Pokerus time: 16777216 byte/day pairs preserve strains, cured state and empty slots.')
if __name__=='__main__':main()
