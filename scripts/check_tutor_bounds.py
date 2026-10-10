"""Execute production byte-row tutor reads, including invalid species IDs."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/item.c').read_text(encoding='utf-8')
    start = source.index('bool8 CanMonLearnTutorMove(')
    block = source[start:source.index('//Special move tutors not stored in a table', start)]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef u8 bool8;
#define NULL ((void*)0)
#define FALSE 0
#define TRUE 1
#define SPECIES_NONE 0
#define NUM_SPECIES 4
#define NUM_MOVE_TUTORS 152
#define MON_DATA_SPECIES 0
struct Pokemon {u16 species;};
u8 gTutorLearnsets[4*19];
u16 GetMonData(struct Pokemon *mon,int field,void *unused) {return mon->species;}
FUNCTION
    return FALSE;
}
int main(void) {
    for(int n=0;n<76;n++) gTutorLearnsets[n]=(n*73+19)&255;
    struct Pokemon mon;
    for(int species=0;species<65536;species++) {
        mon.species=species;
        for(int tutor=0;tutor<152;tutor++) {
            int expected=species>0 && species<4
                ? ((gTutorLearnsets[species*19+tutor/8]>>(tutor%8))&1) : 0;
            if(CanMonLearnTutorMove(&mon,tutor)!=expected) return 1;
        }
    }
    return 0;
}
'''.replace('FUNCTION', block)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('tutor' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Tutor byte-row reads pass all 65536 species IDs and 152 tutor slots with a four-species controlled table.')


if __name__ == '__main__':
    main()
