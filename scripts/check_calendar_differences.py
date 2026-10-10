"""Check production date/time differences against Python Gregorian dates."""
import argparse
import datetime
import calendar
import subprocess
import tempfile
import random
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    parser.add_argument('--check-saturation', action='store_true', help='Also test large time boundaries on a baseline with optimized day counting')
    args = parser.parse_args()
    source = (ROOT / 'src/dns.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/dns.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    signatures = ['static bool8 IsDate1BeforeDate2(u32 y1, u32 m1, u32 d1, u32 y2, u32 m2, u32 d2)',
                  'static bool8 IsLeapYear(u32 year)',
                  'static bool8 IsValidCalendarDate(u32 year, u8 month, u8 day)',
                  'u32 GetDayDifference(u32 startYear, u8 startMonth, u8 startDay, u32 endYear, u8 endMonth, u8 endDay)',
                  'u32 GetHourDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u32 endYear, u8 endMonth, u8 endDay, u8 endHour)',
                  'u32 GetMinuteDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u8 startMin, u32 endYear, u8 endMonth, u8 endDay, u8 endHour, u8 endMin)',
                  'u32 GetYearDifference(u32 startYear, u32 endYear)',
                  'u32 GetMonthDifference(u32 startYear, u8 startMonth, u32 endYear, u8 endMonth)']
    production = '\n'.join(function(source, s) for s in signatures if s + '\n{' in source)
    arrays = []
    for name, forward in [('toNext', True), ('fromPrev', False)]:
        values = []
        for year in (1900, 2000, 2100):
            for month in range(13):
                for day in range(32):
                    try:
                        date = datetime.date(year, month, day)
                        value = ((datetime.date(year + 1, 1, 1) - date) if forward else
                                 (date - datetime.date(year - 1, 1, 1))).days
                    except ValueError:
                        value = 0
                    values.append(value)
        arrays.append('static const u32 %s[3][13][32]={%s};' % (name, ','.join(map(str, values))))
    spans = []
    rng = random.Random(9172)
    pairs = [(0, 1), (0, 400), (1900, 2100), (1, 9999), (0, 0xFFFFFFFF),
             (0xFFFFFFFE, 0xFFFFFFFF), (0xFFFFFFFF, 0xFFFFFFFF)]
    pairs += [(rng.randrange(0x100000000), rng.randrange(0x100000000)) for _ in range(5000)]
    for start, end in pairs:
        start, end = sorted((start, end))
        def ordinal(year):
            return year * 365 + (year + 3) // 4 - (year + 99) // 100 + (year + 399) // 400
        expected = min(0xFFFFFFFF, ordinal(end) - ordinal(start))
        spans.append('{%du,%du,%du}' % (start, end, expected))
    arrays.append('static const u32 spans[][3]={' + ','.join(spans) + '};')
    boundary_dates = []
    for days in [0xFFFFFFFF // 1440 + delta for delta in (-1, 0, 1)] + [0xFFFFFFFF // 24 + delta for delta in (-1, 0, 1)]:
        target = ordinal(1) + days
        lo, hi = 1, 0xFFFFFFFF
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if ordinal(mid) <= target:
                lo = mid
            else:
                hi = mid - 1
        left, month = target - ordinal(lo), 1
        while left >= calendar.monthrange(lo, month)[1]:
            left -= calendar.monthrange(lo, month)[1]
            month += 1
        boundary_dates.append('{%du,%du,%du,%du}' % (days, lo, month, left + 1))
    arrays.append('static const u32 boundaries[][4]={' + ','.join(boundary_dates) + '};')
    # Prior loop-based implementations are tested only on the bounded calendar cases.
    large_test = '' if args.baseline else '''
    for(u32 i=0;i<sizeof(spans)/sizeof(spans[0]);i++)
        if(GetDayDifference(spans[i][0],1,1,spans[i][1],1,1)!=spans[i][2]) return 11;
'''
    if not args.baseline or args.check_saturation:
        large_test += '''
    for(u32 i=0;i<6;i++) for(u32 sh=0;sh<24;sh++) for(u32 eh=0;eh<24;eh++)
    for(u32 sm=0;sm<60;sm+=59) for(u32 em=0;em<60;em+=59) {
        u64 hours=(u64)boundaries[i][0]*24+eh-sh;
        u64 minutes=hours*60+em-sm;
        u32 expectedHours=hours>0xFFFFFFFFu ? 0xFFFFFFFFu : hours;
        u32 expectedMinutes=minutes>0xFFFFFFFFu ? 0xFFFFFFFFu : minutes;
        if(GetHourDifference(1,1,1,sh,boundaries[i][1],boundaries[i][2],boundaries[i][3],eh)!=expectedHours) return 12;
        if(GetMinuteDifference(1,1,1,sh,sm,boundaries[i][1],boundaries[i][2],boundaries[i][3],eh,em)!=expectedMinutes) return 13;
    }
'''
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef unsigned long long u64;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define MathMax(a,b) ((a)>(b) ? (a) : (b))
PRODUCTION
ORACLE
int main(void) {
    if(GetDayDifference(2000,2,30,2001,1,1)!=0) return 1;
    u32 years[3]={1900,2000,2100};
    for(int y=0;y<3;y++) for(int m=0;m<256;m++) for(int d=0;d<256;d++) {
        u32 forward=m<13 && d<32 ? toNext[y][m][d] : 0;
        u32 backward=m<13 && d<32 ? fromPrev[y][m][d] : 0;
        if(GetDayDifference(years[y],m,d,years[y]+1,1,1)!=forward) return 2;
        if(GetDayDifference(years[y]-1,1,1,years[y],m,d)!=backward) return 3;
        if(GetHourDifference(years[y],m,d,0,years[y]+1,1,1,0)!=forward*24) return 4;
        if(GetMinuteDifference(years[y],m,d,0,0,years[y]+1,1,1,0,0)!=forward*1440) return 5;
        if(GetDayDifference(years[y],1,1,years[y],m,d)!=(backward ? backward-365 : 0)) return 9;
        if(GetDayDifference(years[y],m,d,years[y],1,1)!=0) return 10;
    }
    for(int hour=0;hour<256;hour++) for(int minute=0;minute<256;minute++) {
        u32 expected=hour<24 && minute<60 ? 1440-hour*60-minute : 0;
        if(GetMinuteDifference(2000,1,1,hour,minute,2000,1,2,0,0)!=expected) return 6;
        if(GetMinuteDifference(2000,1,1,0,0,2000,1,2,hour,minute)!=(hour<24 && minute<60 ? 1440+hour*60+minute : 0)) return 7;
        if(GetHourDifference(2000,1,1,hour,2000,1,2,0)!=(hour<24 ? 24-hour : 0)) return 8;
    }
    if(GetMonthDifference(0,1,0xFFFFFFFFu,1)!=0xFFFFFFFFu) return 16;
    for(u32 sy=1999;sy<=2001;sy++) for(u32 sm=0;sm<256;sm++) for(u32 em=0;em<256;em++) {
        u32 expected=0;
        if(sm>=1 && sm<=12 && em>=1 && em<=12 && (sy<2000 || (sy==2000 && sm<=em)))
            expected=(2000-sy)*12+em-sm;
        if(GetMonthDifference(sy,sm,2000,em)!=expected) return 14;
    }
    for(u32 i=0;i<sizeof(spans)/sizeof(spans[0]);i++) for(u32 sm=1;sm<=12;sm++) for(u32 em=1;em<=12;em++) {
        u64 months=(u64)(spans[i][1]-spans[i][0])*12+em;
        u32 expected=months<sm ? 0 : months-sm>0xFFFFFFFFu ? 0xFFFFFFFFu : months-sm;
        if(GetMonthDifference(spans[i][0],sm,spans[i][1],em)!=expected) return 15;
    }
    LARGE_TEST
    return 0;
}
'''.replace('PRODUCTION', production).replace('ORACLE', '\n'.join(arrays)).replace('LARGE_TEST', large_test)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('differences' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-Wno-missing-braces', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Calendar differences: 196608 date cases, 65536 time cases and %d large year spans pass.' % (0 if args.baseline else len(spans)))
    print('Month differences: 196608 raw month cases and %d valid-month/year-span cases pass.' % (len(spans) * 144))
    if not args.baseline or args.check_saturation:
        print('Time saturation: 13824 hour/minute boundary combinations pass.')


if __name__ == '__main__':
    main()
