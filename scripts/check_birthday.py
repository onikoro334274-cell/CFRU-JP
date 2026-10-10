"""Execute production player-birthday registration, date and save validation."""
import calendar
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/birthday.c').read_text(encoding='utf-8')
    production = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
    global_header = (ROOT / 'include/global.h').read_text(encoding='utf-8')
    saved_type = re.search(r'struct PlayerBirthdaySaveData\s*\{[^}]*\};', global_header)[0]
    # Python's calendar is the independent Gregorian-date oracle.
    month_lengths = ','.join(str(calendar.monthrange(2000, m)[1]) for m in range(1, 13))
    leaps = ','.join(str(int(calendar.isleap(y))) for y in range(65536))
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef u8 bool8;
#define TRUE 1
#define FALSE 0
SAVED_TYPE
struct SaveBlock2 {u8 prefix[0xf18];struct PlayerBirthdaySaveData playerBirthday;u32 encryptionKey;};
struct Clock {u16 year;u8 month,day;};
static struct SaveBlock2 storage,*gSaveBlock2=&storage;
static struct Clock gClock;
static u16 Var8004,Var8005,gSpecialVar_LastResult;
PRODUCTION
static const u8 expectedLengths[]={MONTH_LENGTHS};
static const u8 expectedLeap[]={LEAPS};
static int same(const struct PlayerBirthdaySaveData *a,const struct PlayerBirthdaySaveData *b) {
    return a->signature==b->signature && a->monthDay==b->monthDay && a->monthDayComplement==b->monthDayComplement;
}
int main(void) {
    u8 m=91,d=92;
    for(unsigned i=0;i<sizeof(storage.prefix);i++) storage.prefix[i]=(u8)(i*37);
    storage.encryptionKey=0x12345678;
    if(Birthday_Get(&m,&d)||m!=91||d!=92||Birthday_IsToday()) return 1;
    for(unsigned month=0;month<256;month++) for(unsigned day=0;day<256;day++) {
        struct PlayerBirthdaySaveData before=storage.playerBirthday;
        int valid=month>=1&&month<=12&&day>=1&&day<=expectedLengths[month-1];
        if(Birthday_Set(month,day)!=valid) return 2;
        if(!valid) {if(!same(&before,&storage.playerBirthday)) return 3;}
        else {
            if(!Birthday_Get(&m,&d)||m!=month||d!=day||!Birthday_Get(0,0)) return 4;
            if(!Birthday_IsOnDate(2000,month,day)) return 5;
            if(Birthday_IsOnDate(2000,month,day+1)||Birthday_IsOnDate(2000,0,day)) return 6;
            gClock.year=2000;gClock.month=month;gClock.day=day;
            Birthday_CheckTodayFromScript();if(gSpecialVar_LastResult!=1) return 7;
        }
    }
    Birthday_Set(6,15);
    for(unsigned n=256;n<65536;n++) {
        struct PlayerBirthdaySaveData before=storage.playerBirthday;
        if(Birthday_Set(n,15)||Birthday_Set(6,n)||!same(&before,&storage.playerBirthday)) return 8;
    }
    Birthday_Set(2,29);
    for(unsigned year=0;year<65536;year++) {
        if(Birthday_IsOnDate(year,2,29)!=expectedLeap[year]) return 9;
        if(Birthday_IsOnDate(year,2,28)||Birthday_IsOnDate(year,3,1)) return 10;
    }
    struct PlayerBirthdaySaveData good=storage.playerBirthday;
    for(unsigned bit=0;bit<64;bit++) {
        storage.playerBirthday=good;
        ((u8*)&storage.playerBirthday)[bit/8]^=1u<<(bit%8);
        m=91;d=92;
        if(Birthday_Get(&m,&d)||m!=91||d!=92||Birthday_IsOnDate(2000,2,29)) return 11;
    }
    for(unsigned packed=0;packed<65536;packed++) {
        unsigned month=packed>>8,day=packed&255;
        int valid=month>=1&&month<=12&&day>=1&&day<=expectedLengths[month-1];
        storage.playerBirthday=good;storage.playerBirthday.monthDay=packed;
        storage.playerBirthday.monthDayComplement=(u16)~packed;
        if(Birthday_Get(0,0)!=valid) return 12;
    }
    storage.playerBirthday=good;
    Var8004=256;Var8005=29;Birthday_SetFromScript();
    if(gSpecialVar_LastResult||!same(&good,&storage.playerBirthday)) return 13;
    Var8004=12;Var8005=31;Birthday_SetFromScript();if(!gSpecialVar_LastResult) return 14;
    Var8004=Var8005=0;Birthday_GetForScript();
    if(gSpecialVar_LastResult!=1||Var8004!=12||Var8005!=31) return 15;
    Birthday_ClearFromScript();if(!gSpecialVar_LastResult||Birthday_Get(0,0)) return 16;
    Var8004=Var8005=123;Birthday_GetForScript();
    if(gSpecialVar_LastResult||Var8004||Var8005) return 17;
    for(unsigned i=0;i<sizeof(storage.prefix);i++) if(storage.prefix[i]!=(u8)(i*37)) return 18;
    if(storage.encryptionKey!=0x12345678) return 19;
    gSaveBlock2=0;m=91;d=92;
    if(Birthday_Set(1,1)||Birthday_Get(&m,&d)||m!=91||d!=92||Birthday_IsToday()) return 20;
    Birthday_ClearFromScript();if(gSpecialVar_LastResult) return 21;
    return 0;
}
'''.replace('SAVED_TYPE', saved_type).replace('PRODUCTION', production).replace(
        'MONTH_LENGTHS', month_lengths).replace('LEAPS', leaps)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('birthday' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Birthday: registration bounds, 65536 Gregorian years, all packed dates, 64 corruptions, script calls and save canaries pass.')


if __name__ == '__main__':
    main()
