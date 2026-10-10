"""Execute production byte-row tutor reads, including invalid species IDs."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/item.c').read_text(encoding='utf-8')
    start = source.index('bool8 CanMonLearnTutorMove(')
    block = source[start:source.index('//Special move tutors not stored in a table', start)]
    start = source.index('u32 CanMonLearnTMHM(')
    tm_function = source[start:source.index('#ifdef EXPANDED_MOVE_TUTORS', start)]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#define NULL ((void*)0)
#define FALSE 0
#define TRUE 1
#define SPECIES_NONE 0
#define SPECIES_EGG 3
#define NUM_SPECIES 4
#define NUM_MOVE_TUTORS 152
#define MON_DATA_SPECIES 0
#define MON_DATA_SPECIES2 1
struct Pokemon {u16 species;};
u8 gTutorLearnsets[4*19];
u32 gTMHMLearnsets[4][4];
u16 GetMonData(struct Pokemon *mon,int field,void *unused) {return mon->species;}
FUNCTION
    return FALSE;
}
TM_FUNCTION
int main(void) {
    for(int species=0;species<4;species++) for(int word=0;word<4;word++)
        gTMHMLearnsets[species][word]=0xC35A970Fu ^ (species*0x391D125Bu) ^ (word*0x25AD8391u);
    for(int n=0;n<76;n++) gTutorLearnsets[n]=(n*73+19)&255;
    struct Pokemon mon;
    for(int species=0;species<65536;species++) {
        mon.species=species;
        for(int tutor=0;tutor<152;tutor++) {
            int expected=species>0 && species<4
                ? ((gTutorLearnsets[species*19+tutor/8]>>(tutor%8))&1) : 0;
            if(CanMonLearnTutorMove(&mon,tutor)!=expected) return 1;
        }
        for(int tm=0;tm<256;tm++) {
            int limit=TM_LIMIT;
            int expected=species>0 && species<3 && tm<limit
                ? ((gTMHMLearnsets[species][tm/32]>>(tm%32))&1) : 0;
            if(CanMonLearnTMHM(&mon,tm)!=expected) return 2;
        }
    }
    return 0;
}
'''.replace('TM_FUNCTION', tm_function).replace('FUNCTION', block)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('tutor' + suffix) for suffix in ('.c', '.obj', '.exe'))
        for expanded in (False, True):
            c.write_text(fixture.replace('TM_LIMIT', '128' if expanded else '64'), encoding='utf-8')
            defines = ['-DEXPANDED_TMSHMS'] if expanded else []
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                            '-fno-stack-protector', *defines, '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
            subprocess.run([str(exe)], check=True)
    print('Tutor and TM/HM table reads pass all 65536 species IDs, 152 tutor slots and 256 TM IDs in both TM layouts.')


if __name__ == '__main__':
    main()
