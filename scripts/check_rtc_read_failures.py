"""Execute RTC transfer wrappers and initialization with partial/failed reads."""
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
    header = (ROOT / 'include/rtc.h').read_text(encoding='utf-8')
    constants = '\n'.join(re.findall(r'^#define RTC_\w+\s+0x\w+.*$', header, re.M))
    structure = re.search(r'struct SiiRtcInfo\s*\{[^}]*\};', (ROOT / 'include/siirtc.h').read_text(encoding='utf-8'))[0]
    dummy = re.search(r'static const struct SiiRtcInfo sRtcDummy[^;]+;', source)[0]
    production = '\n'.join(function(source, signature) for signature in (
        'void RtcGetDateTime(struct SiiRtcInfo *rtc)', 'void RtcGetStatus(struct SiiRtcInfo *rtc)',
        'void RtcGetRawInfo(struct SiiRtcInfo *rtc)', 'void RtcInit(void)',
        'void RtcGetInfo(struct SiiRtcInfo *rtc)',
    ))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef u8 bool8;
#define MONTH_JAN 1
CONSTANTS
STRUCTURE
DUMMY
static struct SiiRtcInfo sRtc;
static const struct SiiRtcInfo known={0x25,0x12,0x31,3,0x23,0x59,0x58,0x40,0,0};
static u16 sRTCErrorStatus,checkErrors;
static u8 sRTCProbeResult,probe;
static int statusOK,dateOK,disabled,sequence[32],count,invalid;
static void RtcDisableInterrupts(void){if(disabled) invalid++;disabled=1;sequence[count++]=1;}
static void RtcRestoreInterrupts(void){if(!disabled) invalid++;disabled=0;sequence[count++]=4;}
static bool8 SiiRtcGetStatus(struct SiiRtcInfo *rtc){
    if(!disabled) invalid++;sequence[count++]=2;
    rtc->status=statusOK?known.status:0xa5;return statusOK;
}
static bool8 SiiRtcGetDateTime(struct SiiRtcInfo *rtc){
    if(!disabled) invalid++;sequence[count++]=3;
    u8 *dest=(u8*)rtc;const u8 *src=(const u8*)&known;
    for(int i=0;i<7;i++) dest[i]=dateOK?src[i]:0xa5;
    return dateOK;
}
static void SiiRtcUnprotect(void){if(!disabled) invalid++;sequence[count++]=5;}
static u8 SiiRtcProbe(void){if(!disabled) invalid++;sequence[count++]=6;return probe;}
static u16 RtcCheckInfo(struct SiiRtcInfo *rtc){(void)rtc;sequence[count++]=7;return checkErrors;}
PRODUCTION
static int equal(const struct SiiRtcInfo *a,const struct SiiRtcInfo *b) {
    const u8 *x=(const u8*)a,*y=(const u8*)b;
    for(unsigned i=0;i<sizeof(*a);i++) if(x[i]!=y[i]) return 0;return 1;
}
static void reset(void){count=invalid=disabled=0;}
int main(void) {
    for(unsigned flags=0;flags<65536;flags++) for(statusOK=0;statusOK<2;statusOK++)
    for(dateOK=0;dateOK<2;dateOK++) {
        struct SiiRtcInfo rtc=known;reset();sRTCErrorStatus=flags;
        RtcGetRawInfo(&rtc);
        u16 expected=flags|((!statusOK||!dateOK)?RTC_INIT_ERROR:0);
        if(sRTCErrorStatus!=expected) return 1;
        if(!equal(&rtc,(expected&RTC_INIT_ERROR)?&sRtcDummy:&known)) return 2;
        if(count!=6||sequence[0]!=1||sequence[1]!=2||sequence[2]!=4||sequence[3]!=1||sequence[4]!=3||sequence[5]!=4||invalid||disabled) return 3;
        reset();sRTCErrorStatus=flags;rtc=known;RtcGetStatus(&rtc);
        if(sRTCErrorStatus!=(flags|(statusOK?0:RTC_INIT_ERROR))||!equal(&rtc,statusOK?&known:&sRtcDummy)||disabled||invalid||count!=3) return 4;
        reset();sRTCErrorStatus=flags;rtc=known;RtcGetDateTime(&rtc);
        if(sRTCErrorStatus!=(flags|(dateOK?0:RTC_INIT_ERROR))||!equal(&rtc,dateOK?&known:&sRtcDummy)||disabled||invalid||count!=3) return 5;
    }
    for(probe=1;probe<=17;probe+=16) for(statusOK=0;statusOK<2;statusOK++)
    for(dateOK=0;dateOK<2;dateOK++) {
        reset();checkErrors=0;sRTCErrorStatus=0xffff;sRtc=known;RtcInit();
        u16 expected=(probe==17?RTC_INIT_WARNING:0)|((!statusOK||!dateOK)?RTC_INIT_ERROR:0);
        if(sRTCErrorStatus!=expected||disabled||invalid||count!=11) return 6;
        if(!equal(&sRtc,(!statusOK||!dateOK)?&sRtcDummy:&known)) return 7;
        int oldCount=count;struct SiiRtcInfo rtc=known;RtcGetInfo(&rtc);
        if(!equal(&rtc,(!statusOK||!dateOK)?&sRtcDummy:&known)) return 8;
        if(count!=oldCount+((!statusOK||!dateOK)?0:6)) return 9;
    }
    return 0;
}
'''.replace('CONSTANTS', constants).replace('STRUCTURE', structure).replace('DUMMY', dummy).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('rtc_reads' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('RTC reads: 262144 flag/transfer combinations, interrupt order, partial data fallback and initialization retry pass.')


if __name__ == '__main__':
    main()
