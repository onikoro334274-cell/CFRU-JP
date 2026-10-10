"""Check complete profile initialization and section accumulator boundaries."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/overworld_speedup.c').read_text(encoding='utf-8'),
                          'static void ProfAdd(u32 k, u32 lines)')
    fixture = r'''
typedef unsigned int u32;
#define PROF_SECTIONS 28
#define PROF_WORDS (330 + PROF_SECTIONS)
static u32 storage[PROF_WORDS+2];
#define PROF_BASE (storage+1)
static void Memset(void* dest,int value,u32 count) {
    unsigned char* p=dest;for(u32 i=0;i<count;i++) p[i]=value;
}
PRODUCTION
int main(void) {
    const u32 values[]={0,1,228,0xFFFFFFFF};
    for(u32 k=0;k<28;k++) for(u32 choice=0;choice<4;choice++) {
        for(u32 i=0;i<PROF_WORDS+2;i++) storage[i]=0xDEADBEEF;
        u32 lines=values[choice];ProfAdd(k,lines);
        if(storage[0]!=0xDEADBEEF || storage[PROF_WORDS+1]!=0xDEADBEEF) return 1;
        for(u32 i=0;i<PROF_WORDS;i++) {
            u32 expected=i==0 ? 0x464F5250 : i==3 ? 28 :
                         (i==4+2*k || i==5+2*k || i==300+k) ? lines : 0;
            if(PROF_BASE[i]!=expected) return 2;
        }
        PROF_BASE[64]=77;ProfAdd(k,1);
        if(PROF_BASE[64]!=77 || PROF_BASE[4+2*k]!=lines+1u || PROF_BASE[300+k]!=lines+1u ||
           PROF_BASE[5+2*k]!=(lines>1 ? lines : 1)) return 3;
        if(storage[0]!=0xDEADBEEF || storage[PROF_WORDS+1]!=0xDEADBEEF) return 4;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('overworld_profile' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Field profile: 112 dirty-memory/section/value cases passed with complete initialization and storage guards.')


if __name__ == '__main__':
    main()
