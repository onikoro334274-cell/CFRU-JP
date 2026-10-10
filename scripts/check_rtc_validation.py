"""Execute RTC validation on calendar boundaries and raw BCD field values."""
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
    source = (ROOT / 'src/rtc.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/rtc.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    constants = '\n'.join(re.findall(r'^#define (?:RTC_ERR_\w+|SIIRTCINFO_\w+)\s+0x\w+.*$',
                                    '\n'.join((ROOT / p).read_text(encoding='utf-8')
                                              for p in ('include/rtc.h', 'include/siirtc.h')), re.M))
    header = (ROOT / 'include/siirtc.h').read_text(encoding='utf-8')
    start = header.index('struct SiiRtcInfo\n{')
    structure = header[start:header.index('\n};', start) + 3]
    array = re.search(r'static const s32 sNumDaysInMonths\[12\][\s\S]*?\n\};', source)[0]
    production = '\n'.join(function(source, signature) for signature in
                           ('u32 ConvertBcdToBinary(u8 bcd)', 'static bool8 IsLeapYear(u32 year)',
                            'u16 RtcCheckInfo(struct SiiRtcInfo *rtc)'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
typedef int s32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define MONTH_FEB 2
CONSTANTS
STRUCTURE
ARRAY
PRODUCTION
static int decimal(int raw) {return raw/16<10 && raw%16<10 ? raw/16*10+raw%16 : -1;}
static u8 bcd(int n) {return (n/10)*16+n%10;}
static int expected(struct SiiRtcInfo* r) {
    int result=0,y=decimal(r->year),m=decimal(r->month),d=decimal(r->day);
    if(r->status&0x80) result|=RTC_ERR_POWER_FAILURE;
    if(!(r->status&0x40)) result|=RTC_ERR_12HOUR_CLOCK;
    if(y<0) result|=RTC_ERR_INVALID_YEAR;
    if(m<1 || m>12) result|=RTC_ERR_INVALID_MONTH;
    if(d<=0) result|=RTC_ERR_INVALID_DAY;
    if(m>=1 && m<=12) {
        int days=m==2 ? 28 : (m==4 || m==6 || m==9 || m==11 ? 30 : 31);
        if(m==2 && y>=0 && y%4==0) days++;
        if(d>days) result|=RTC_ERR_INVALID_DAY;
    }
    int h=decimal(r->hour),minute=decimal(r->minute),second=decimal(r->second);
    if(h<0 || h>=24) result|=RTC_ERR_INVALID_HOUR;
    if(minute<0 || minute>=60) result|=RTC_ERR_INVALID_MINUTE;
    if(second<0 || second>=60) result|=RTC_ERR_INVALID_SECOND;
    return result;
}
int main(void) {
    struct SiiRtcInfo r={0};r.month=1;r.day=0;r.status=0x40;
    if(RtcCheckInfo(&r)!=RTC_ERR_INVALID_DAY) return 1;
    r.day=1;
    for(int raw=0;raw<256;raw++) {
        if(ConvertBcdToBinary(raw)!=(u32)(decimal(raw)<0 ? 255 : decimal(raw))) return 2;
        r.year=raw;if(RtcCheckInfo(&r)!=expected(&r)) return 3;r.year=0;
        r.hour=raw;if(RtcCheckInfo(&r)!=expected(&r)) return 4;r.hour=0;
        r.minute=raw;if(RtcCheckInfo(&r)!=expected(&r)) return 5;r.minute=0;
        r.second=raw;if(RtcCheckInfo(&r)!=expected(&r)) return 6;r.second=0;
        r.status=raw;if(RtcCheckInfo(&r)!=expected(&r)) return 7;r.status=0x40;
    }
    for(int y=0;y<100;y++) for(int m=0;m<256;m++) for(int d=0;d<256;d++) {
        r.year=bcd(y);r.month=m;r.day=d;
        if(RtcCheckInfo(&r)!=expected(&r)) return 8;
    }
    return 0;
}
'''.replace('CONSTANTS', constants).replace('STRUCTURE', structure).replace('ARRAY', array).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('rtc' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('RTC validation: 6553600 year/raw-month/raw-day cases, all BCD bytes and 1280 field/status cases pass.')


if __name__ == '__main__':
    main()
