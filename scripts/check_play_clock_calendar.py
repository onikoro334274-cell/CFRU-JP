"""Compare production play-clock dates to Python's Gregorian calendar."""
import datetime
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/play_clock.c').read_text(encoding='utf-8')
    defines = '\n'.join(re.findall(r'^#define (?:CLOCK_\w+|MINUTES_PER_DAY)\s+\w+.*$', source, re.M))
    array = re.search(r'static const u8 sDaysInMonth\[\][^;]+;', source)[0]
    production = '\n'.join(function(source, signature) for signature in
                           ('static u32 PlayClock_GameSeconds(void)', 'static bool8 IsLeapYear(u32 y)',
                            'void PlayClock_Fill(void)'))
    max_day = ((65535 * 3600 + 255 * 60 + 255) * 6 // 60 + 65535) // 1440 + 32767
    epoch = datetime.date(2000, 1, 1)
    dates = []
    for offset in range(max_day + 1):
        date = epoch + datetime.timedelta(days=offset)
        weekday = (date.weekday() + 1) % 7
        dates.append((date.year << 16) | (date.month << 12) | (date.day << 7) | weekday)
    oracle = 'static const u32 dates[]={' + ','.join(str(date) + 'u' for date in dates) + '};'
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define CLOCK_SPEED 6
#include "src/Tables/hoenn_script_flags.h"
DEFINES
ARRAY
static struct {u16 playTimeHours;u8 playTimeMinutes,playTimeSeconds;} save;
#define gSaveBlock2 (&save)
static struct {u32 year,month,day,dayOfWeek,hour,minute,second;} gClock;
static u16 minutes,days;
static u16 VarGet(u16 id){return id==HOENN_VAR_CLOCK_MIN ? minutes : days;}
PRODUCTION
ORACLE
static int check(void) {
    u32 seconds=((u32)save.playTimeHours*3600u+save.playTimeMinutes*60u+save.playTimeSeconds)*6u;
    u32 total=seconds/60u+minutes,day=total/1440u+(days&32767);
    PlayClock_Fill();
    u32 packed=(gClock.year<<16)|(gClock.month<<12)|(gClock.day<<7)|gClock.dayOfWeek;
    if(packed!=dates[day]) return 1;
    if(gClock.hour!=(total%1440)/60 || gClock.minute!=total%60 || gClock.second!=seconds%60) return 2;
    return 0;
}
int main(void) {
    u16 hours[3]={0,999,65535},offsets[3]={0,1439,65535};
    for(int h=0;h<3;h++) for(int m=0;m<3;m++) for(int d=0;d<32768;d++) {
        save.playTimeHours=hours[h];save.playTimeMinutes=save.playTimeSeconds=0;
        minutes=offsets[m];days=d|32768;
        int result=check();if(result) return result;
    }
    save.playTimeHours=65535;minutes=65535;days=65535;
    for(int m=0;m<256;m++) for(int s=0;s<256;s++) {
        save.playTimeMinutes=m;save.playTimeSeconds=s;
        int result=check();if(result) return result;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('ARRAY', array).replace('PRODUCTION', production).replace('ORACLE', oracle)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('calendar' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Play clock: 360448 cases match Python Gregorian dates, weekdays and time, including the year 2100.')


if __name__ == '__main__':
    main()
