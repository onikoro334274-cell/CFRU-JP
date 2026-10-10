"""Verify only the documented five-character species truncation expands."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/hack_wonder_trade.c').read_text(encoding='utf-8'),
                          'static void GetMonDisplayName(u8* dest, struct Pokemon* mon)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;
#define NULL ((void*)0)
#define POKEMON_NAME_LENGTH 10
#define MON_DATA_SPECIES 1
#define MON_DATA_NICKNAME 2
struct Pokemon {u16 species;u8 nickname[11];};
static u8 gSpeciesNames[1][11];
static u16 GetMonData(struct Pokemon* mon,int field,void* dest) {
    if(field==1) return mon->species;
    for(int i=0;i<11;i++) ((u8*)dest)[i]=mon->nickname[i];return 0;
}
static void StringCopy(u8* dest,const u8* source) {
    do {*dest++=*source;} while(*source++!=0xFF);
}
PRODUCTION
int main(void) {
    for(int speciesLength=0;speciesLength<=10;speciesLength++)
    for(int nameLength=0;nameLength<=10;nameLength++)
    for(int mismatch=-1;mismatch<nameLength;mismatch++) {
        struct Pokemon mon={0,{0}};u8 output[13];
        for(int i=0;i<11;i++) {
            gSpeciesNames[0][i]=i<speciesLength ? 10+i : 0xFF;
            mon.nickname[i]=i<nameLength ? 10+i : 0xFF;
        }
        if(mismatch>=0) mon.nickname[mismatch]=100;
        for(int i=0;i<13;i++) output[i]=0xCC;
        GetMonDisplayName(output+1,&mon);
        const u8* expected=nameLength==5 && mismatch<0 && speciesLength>5 ? gSpeciesNames[0] : mon.nickname;
        int length=expected==mon.nickname ? nameLength : speciesLength;
        for(int i=0;i<=length;i++) if(output[i+1]!=expected[i]) return 1;
        if(output[0]!=0xCC || output[length+2]!=0xCC) return 2;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('wonder_names' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Wonder names: 726 length/prefix/mismatch cases passed with output guards.')


if __name__ == '__main__':
    main()
