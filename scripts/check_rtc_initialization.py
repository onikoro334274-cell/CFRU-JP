"""Execute RTC initialization and retain probe warnings alongside date errors."""
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
    if args.baseline:
        header = subprocess.run(['git', 'show', args.baseline + ':include/rtc.h'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    constants = '\n'.join(re.findall(r'^#define RTC_\w+\s+0x\w+.*$', header, re.M))
    production = function(source, 'void RtcInit(void)') + '\n' + function(source, 'void RtcGetInfo(struct SiiRtcInfo *rtc)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;
CONSTANTS
static u16 sRTCErrorStatus,flags;static u8 sRTCProbeResult,probe;
static struct SiiRtcInfo {int opaque;} sRtc;
static const struct SiiRtcInfo sRtcDummy={123};
static int order[6],count;
static void RtcDisableInterrupts(void){order[count++]=1;}
static void SiiRtcUnprotect(void){order[count++]=2;}
static u8 SiiRtcProbe(void){order[count++]=3;return probe;}
static void RtcRestoreInterrupts(void){order[count++]=4;}
static void RtcGetRawInfo(struct SiiRtcInfo* rtc){order[count++]=5;rtc->opaque=567;}
static u16 RtcCheckInfo(struct SiiRtcInfo* rtc){order[count++]=6;return flags;}
PRODUCTION
int main(void) {
    for(int p=0;p<256;p++) for(int bits=0;bits<256;bits++) {
        probe=p;flags=bits<<4;sRTCErrorStatus=0xFFFF;sRTCProbeResult=0;count=0;
        RtcInit();
        int success=(p&15)==1;
        u16 expected=success ? flags|((p&240) ? RTC_INIT_WARNING : 0) : RTC_INIT_ERROR;
        if(sRTCErrorStatus!=expected || sRTCProbeResult!=p) return 1;
        if(count!=(success ? 6 : 4)) return 2;
        for(int i=0;i<count;i++) if(order[i]!=i+1) return 3;
        struct SiiRtcInfo result={0};count=0;
        RtcGetInfo(&result);
        int error=!success || flags!=0;
        if(result.opaque!=(error ? 123 : 567) || count!=(error ? 0 : 1)) return 4;
    }
    return 0;
}
'''.replace('CONSTANTS', constants).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('rtc_init' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('RTC initialization: 65536 probe/error combinations preserve warnings, interrupt ordering and fallback selection.')


if __name__ == '__main__':
    main()
