"""Execute forward time-of-day adjustments including save-offset exhaustion."""
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
    source = (ROOT / 'src/play_clock.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/play_clock.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    defines = '\n'.join(re.findall(r'^#define (?:CLOCK_\w+|MINUTES_PER_DAY)\s+\w+.*$', source, re.M))
    table = re.search(r'static const u8 sTimeOfDayStart\[\][^;]+;', source)[0]
    production = '\n'.join(function(source, signature) for signature in
                           ('bool8 PlayClock_IsOn(void)', 'static u32 PlayClock_GameSeconds(void)',
                            'void PlayClock_SetTimeOfDay(void)'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NELEMS(a) (sizeof(a)/sizeof((a)[0]))
#include "src/Tables/hoenn_script_flags.h"
DEFINES
TABLE
static struct {u16 playTimeHours;u8 playTimeMinutes,playTimeSeconds;} save;
#define gSaveBlock2 (&save)
static u16 minutes,days,Var8004;static int writes,updates;
static u16 VarGet(u16 id){return id==HOENN_VAR_CLOCK_MIN ? minutes : days;}
static void VarSet(u16 id,u16 value){writes++;if(id==HOENN_VAR_CLOCK_MIN) minutes=value;else days=value;}
static void DirectClockUpdate(void){updates++;}
PRODUCTION
static int check(int oldMinutes,int oldDays,int target,int mode) {
    minutes=oldMinutes;days=oldDays|(mode ? 32768 : 0);Var8004=target;writes=updates=0;
    u32 base=((u32)save.playTimeHours*3600+save.playTimeMinutes*60+save.playTimeSeconds)*6/60;
    u32 offset=(u32)oldDays*1440+oldMinutes;
    int starts[4]={240,480,1020,1200};
    int delta=0,accept=mode && target<4;
    if(accept) {delta=starts[target]-(int)((base+oldMinutes)%1440);if(delta<0) delta+=1440;}
    u32 expected=offset+delta;
    if(expected/1440>32767) accept=0;
    PlayClock_SetTimeOfDay();
    if(writes!=(accept ? 2 : 0) || updates!=accept) return 1;
    u32 actual=(days&32767)*1440u+minutes;
    if(actual!=(accept ? expected : offset)) return 2;
    if((days>>15)!=mode || actual<offset) return 3;
    return 0;
}
int main(void) {
    int hourCases[4]={0,1,999,65535},dayCases[3]={0,32766,32767};
    for(int h=0;h<4;h++) for(int d=0;d<3;d++) for(int m=0;m<1440;m++) for(int t=0;t<4;t++) {
        save.playTimeHours=hourCases[h];save.playTimeMinutes=save.playTimeSeconds=0;
        int result=check(m,dayCases[d],t,1);if(result) return result;
    }
    save.playTimeHours=0;
    for(int target=0;target<65536;target++) for(int mode=0;mode<2;mode++) {
        int result=check(1439,32767,target,mode);if(result) return result;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('TABLE', table).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('adjustment' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj), '-I', str(ROOT)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Play clock adjustments: 69120 boundary/time combinations and 131072 target/mode cases never move backward.')


if __name__ == '__main__':
    main()
