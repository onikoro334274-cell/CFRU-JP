"""Exercise all variable IDs and every timestamp API with production bounds checks."""
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/scripting.c').read_text(encoding='utf-8')
    structure = re.search(r'^struct [^\n]*DailyEventVar\n\{[\s\S]*?\n\};', source, re.M)[0]
    signatures = (
        'static struct DailyEventVar *GetDailyEventVarPointer(u16 var)',
        'bool8 IsTimeInVarInFuture(u16 var)', 'bool8 IsDayInVarInFuture(u16 var)',
        'void BufferYearMonthDayFromVar(u16 var)',
        'bool8 CheckAndSetDailyEvent(u16 eventVar, bool8 setDailyEventVar)',
        'void UpdateTimeInVars(u16 eventVar)', 'u16 sp0A2_GetTimeDifference(void)',
    )
    production = '\n'.join(function(source, signature) for signature in signatures)
    fixture = r'''
#include <stddef.h>
#include "include/constants/vars.h"
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define EOS 255
#define STR_CONV_MODE_LEFT_ALIGN 0
STRUCTURE
static u16 base[258], expanded[514], special[23], other;
static int resolverCalls, badResolver, fault, differenceCalls, formatCalls;
static u16 firstId,Var8000,Var8001;
static u8 gStringVar1[8],gStringVar2[8],gStringVar3[8];
static struct {u32 year,month,day,hour,minute;} gClock;
static u16 *GetVarPointer(u16 id) {
    resolverCalls++;
    if(fault==1 && id==firstId) return 0;
    if(fault==2 && id==firstId+1) return 0;
    if(fault==3 && id==firstId+1) return &other;
    if(id>=0x4000&&id<=0x40ff) return base+1+id-0x4000;
    if(id>=0x5000&&id<=0x51ff) return expanded+1+id-0x5000;
    if(id>=0x8000&&id<=0x8014) return special+1+id-0x8000;
    badResolver++;return 0;
}
static void ConvertIntToDecimalStringN(u8 *dest,u32 value,int mode,int digits) {
    (void)value;(void)mode;(void)digits;formatCalls++;dest[0]=1;dest[1]=EOS;
}
static u32 GetMinuteDifference(u32 y,u8 m,u8 d,u8 h,u8 min,u32 ey,u8 em,u8 ed,u8 eh,u8 emin) {
    (void)y;(void)m;(void)d;(void)h;(void)min;(void)ey;(void)em;(void)ed;(void)eh;(void)emin;
    differenceCalls++;return 720;
}
static u32 GetMonthDifference(u32 y,u8 m,u32 ey,u8 em) {
    (void)y;(void)m;(void)ey;(void)em;differenceCalls++;return 3;
}
static u32 GetYearDifference(u32 y,u32 ey) {
    (void)y;(void)ey;differenceCalls++;return 1;
}
PRODUCTION
static u16 *oraclePointer(unsigned id) {
    if(id>=0x4000&&id<0x40ff) return base+1+id-0x4000;
#ifdef SAVE_BLOCK_EXPANSION
    if(id>=0x5000&&id<0x51ff) return expanded+1+id-0x5000;
#endif
    if(id>=0x8000&&id<0x8014) return special+1+id-0x8000;
    return 0;
}
int main(void) {
    base[0]=base[257]=expanded[0]=expanded[513]=special[0]=special[22]=0x5a5a;
    gClock.year=2000;gClock.month=6;gClock.day=15;gClock.hour=12;gClock.minute=34;
    for(unsigned id=0;id<65536;id++) for(fault=0;fault<4;fault++) {
        firstId=id;resolverCalls=badResolver=0;
        u16 *expected=oraclePointer(id);
        if(GetDailyEventVarPointer(id)!=(fault?0:(void*)expected)||badResolver) return 1;
        if(!expected && resolverCalls) return 2;
        u16 *guarded=expected;
        if(guarded) guarded[0]=guarded[1]=0;
        other=0x1234;
        int valid=expected && !fault;
        if(CheckAndSetDailyEvent(id,1)!=valid) return 3;
        if(valid) {
            struct DailyEventVar *date=(void*)expected;
            if(date->year!=0||date->century!=20||date->month!=6||date->day!=15||date->hour!=12||date->minute!=34) return 4;
            if(CheckAndSetDailyEvent(id,1)) return 5;
        } else if(guarded && (guarded[0]||guarded[1])) return 6;
        UpdateTimeInVars(id);
        if(!valid && guarded && (guarded[0]||guarded[1])) return 7;
        if(IsTimeInVarInFuture(id)||IsDayInVarInFuture(id)) return 8;
        formatCalls=0;gStringVar1[0]=gStringVar2[0]=gStringVar3[0]=99;
        BufferYearMonthDayFromVar(id);
        if(formatCalls!=(valid?3:0)) return 9;
        if(!valid && (gStringVar1[0]!=EOS||gStringVar2[0]!=EOS||gStringVar3[0]!=EOS)) return 10;
        Var8000=id;
        for(Var8001=0;Var8001<6;Var8001++) {
            differenceCalls=0;
            u16 result=sp0A2_GetTimeDifference();
            if((!valid || Var8001==5) && (result||differenceCalls)) return 11;
            if(valid && Var8001<5 && differenceCalls!=1) return 12;
        }
        if(badResolver||other!=0x1234) return 13;
        if(base[0]!=0x5a5a||base[257]!=0x5a5a||expanded[0]!=0x5a5a||expanded[513]!=0x5a5a||special[0]!=0x5a5a||special[22]!=0x5a5a) return 14;
    }
    return 0;
}
'''.replace('STRUCTURE', structure).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('daily_bounds' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        for expansion in (False, True):
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                       '-fno-stack-protector', '-I', str(ROOT)]
            if expansion:
                command.append('-DSAVE_BLOCK_EXPANSION')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                            '/out:' + str(exe), str(obj)], check=True)
            subprocess.run([str(exe)], check=True)
    print('Daily timestamp bounds: all 65536 IDs, four resolver states, six APIs and both expansion configurations pass.')


if __name__ == '__main__':
    main()
