"""Execute daily-event timestamp writes at both u16-variable alignments."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/scripting.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/scripting.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    structure = re.search(r'^struct [^\n]*DailyEventVar\n\{[\s\S]*?\n\};', source, re.M)[0]
    production = '\n'.join(function(source, signature) for signature in
                           ('void UpdateTimeInVars(u16 eventVar)',
                            'bool8 CheckAndSetDailyEvent(u16 eventVar, bool8 setDailyEventVar)'))
    if 'GetDailyEventVarPointer' in source:
        production = function(source, 'static struct DailyEventVar *GetDailyEventVarPointer(u16 var)') + '\n' + production
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define VARS_START 1
#define VARS_END 7
#define SPECIAL_VARS_START 0x8000
#define SPECIAL_VARS_END 0x8014
STRUCTURE
_Static_assert(sizeof(struct DailyEventVar)==4,"timestamp occupies two variables");
_Static_assert(_Alignof(struct DailyEventVar)==2,"variable pairs may be halfword aligned");
static u16 vars[8];
static void* GetVarPointer(u16 id){return vars+id;}
static struct {u32 year,month,day,hour,minute;} gClock;
PRODUCTION
static u32 read(u16 id){u8* p=(u8*)(vars+id);return p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
static u32 packed(u32 year,u32 month,u32 day,u32 hour,u32 minute) {
    return minute|(hour<<6)|(day<<11)|(month<<16)|((year%100)<<20)|((year/100)<<27);
}
int main(void) {
    u32 years[4]={1999,2000,2100,2134};
    for(u16 id=1;id<=2;id++) for(int y=0;y<4;y++) for(u32 m=1;m<=12;m++)
    for(u32 d=1;d<=31;d++) for(u32 h=0;h<24;h++) for(u32 minute=0;minute<60;minute++) {
        for(int i=0;i<8;i++) vars[i]=0xA5A5;
        gClock.year=years[y];gClock.month=m;gClock.day=d;gClock.hour=h;gClock.minute=minute;
        UpdateTimeInVars(id);
        if(read(id)!=packed(years[y],m,d,h,minute)) return 1;
        for(int i=0;i<8;i++) if((i<id || i>=id+2) && vars[i]!=0xA5A5) return 2;
    }
    gClock.year=2000;gClock.month=6;gClock.day=15;gClock.hour=12;gClock.minute=34;
    for(u16 id=1;id<=2;id++) for(u32 y=1999;y<=2001;y++) for(u32 m=1;m<=12;m++)
    for(u32 d=1;d<=28;d++) for(int set=0;set<2;set++) {
        for(int i=0;i<8;i++) vars[i]=0xA5A5;
        u32 before=packed(y,m,d,23,59);u8* p=(u8*)(vars+id);
        for(int b=0;b<4;b++) p[b]=before>>(b*8);
        int older=y<2000 || (y==2000 && (m<6 || (m==6 && d<15)));
        if(CheckAndSetDailyEvent(id,set)!=older) return 3;
        u32 expected=older && set ? packed(2000,6,15,12,34) : before;
        if(read(id)!=expected) return 4;
        for(int i=0;i<8;i++) if((i<id || i>=id+2) && vars[i]!=0xA5A5) return 5;
    }
    return 0;
}
'''.replace('STRUCTURE', structure).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('daily' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Daily timestamps: 4285440 bit-layout writes and 4032 event cases pass at both variable alignments.')


if __name__ == '__main__':
    main()
