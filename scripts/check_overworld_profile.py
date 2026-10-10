"""Check complete profile initialization and section accumulator boundaries."""
import subprocess
import re
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/overworld_speedup.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static u32 ProfSampleTime(u32* frameBase)', 'static u32 ProfNow(void)',
        'static void ProfAdd(u32 k, u32 lines)'))
    fixture = r'''
typedef unsigned int u32;
#define NULL ((void*)0)
typedef volatile u32 vu32;
static struct {u32 vblankCounter2;} gMain;
static u32 scanline;static int retries,reads;
static u32 ReadScan(void) {
    reads++;
    if(retries>0) {retries--;gMain.vblankCounter2++;return scanline;}
    if(retries==-1) {retries=0;return 160;}
    return scanline;
}
#define PROF_VCOUNT ReadScan()
#define PROF_SECTIONS 28
#define PROF_WORDS (330 + PROF_SECTIONS)
static u32 storage[PROF_WORDS+2];
#define PROF_BASE (storage+1)
static void Memset(void* dest,int value,u32 count) {
    unsigned char* p=dest;for(u32 i=0;i<count;i++) p[i]=value;
}
PRODUCTION
int main(void) {
    for(u32 line=0;line<228;line++) {
        if(line==160) continue;
        gMain.vblankCounter2=123;scanline=line;retries=reads=0;
        if(ProfNow()!=123u*228u+(line>160 ? line-160 : line+68) || reads!=1) return 5;
    }
    for(int race=-1;race<=3;race++) {
        gMain.vblankCounter2=123;scanline=161;retries=race;reads=0;
        u32 finalFrame=123+(race>0 ? race : 0);
        int wantedReads=race==-1 ? 2 : race+1;
        if(ProfNow()!=finalFrame*228u+1 || reads!=wantedReads) return 6;
    }
    gMain.vblankCounter2=0xFFFFFFFF;scanline=159;retries=reads=0;
    if(ProfNow()!=0xFFFFFFFFu*228u+227u) return 7;
    for(u32 frame=0;frame<4;frame++) for(u32 line=0;line<228;line++) {
        if(line==160) continue;
        gMain.vblankCounter2=frame==3 ? 0xFFFFFFFF : frame;
        scanline=line;retries=2;reads=0;u32 base=77;
        u32 stamp=ProfSampleTime(&base);
        u32 wantedBase=gMain.vblankCounter2*228u;
        u32 phase=line>160 ? line-160 : line+68;
        if(base!=wantedBase || stamp-base!=phase || reads!=3) return 8;
    }
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
    words_definition = re.search(r'^#define PROF_WORDS[^\n]*', source, re.M)[0]
    fixture = fixture.replace('#define PROF_WORDS (330 + PROF_SECTIONS)', words_definition)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('overworld_profile' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Field profile: 112 storage, 233 scanline/race/wrap and 908 coherent frame-base cases passed.')


if __name__ == '__main__':
    main()
