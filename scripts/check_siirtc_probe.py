"""Execute the production RTC probe across read/reset failures and register bytes."""
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
    source = (ROOT / 'src/siirtc.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/siirtc.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    header = (ROOT / 'include/siirtc.h').read_text(encoding='utf-8')
    structure = re.search(r'struct SiiRtcInfo\s*\{[^}]*\};', header)[0]
    defines = '\n'.join(re.findall(r'^#define (?:SIIRTCINFO_24HOUR|SIIRTCINFO_POWER)\s+[^\n]+', header, re.M))
    defines += '\n' + re.search(r'^#define TEST_MODE\s+[^\n]+', source, re.M)[0]
    production = function(source, 'u8 SiiRtcProbe(void)')
    fixture = r'''
typedef unsigned char u8;typedef u8 bool8;
DEFINES
STRUCTURE
static unsigned status,second,flags;
static int statusCalls,timeCalls,resetCalls;
static bool8 SiiRtcGetStatus(struct SiiRtcInfo *rtc) {
    statusCalls++;rtc->status=status;return (flags&1)!=0;
}
static bool8 SiiRtcGetTime(struct SiiRtcInfo *rtc) {
    timeCalls++;rtc->second=second;return (flags&2)!=0;
}
static bool8 SiiRtcReset(void) {
    unsigned mask=resetCalls++ ? 8 : 4;return (flags&mask)!=0;
}
PRODUCTION
int main(void) {
    for(status=0;status<256;status++) for(second=0;second<256;second++)
    for(flags=0;flags<16;flags++) {
        statusCalls=timeCalls=resetCalls=0;
        unsigned warnings=0,expected=0,reads=0,resets=0;
        if(flags&1) {
            int firstSuccess=1;
            if(!(status&0x40)) {
                resets++;firstSuccess=(flags&4)!=0;
                if(firstSuccess) warnings++;
            }
            if(firstSuccess) {
                reads++;
                if(flags&2) {
                    int finalSuccess=1;
                    if(second&0x80) {
                        unsigned mask=resets?8:4;
                        resets++;finalSuccess=(flags&mask)!=0;
                        if(finalSuccess) warnings++;
                    }
                    if(finalSuccess) expected=1;
                }
                expected|=warnings<<4;
            }
        }
        if(SiiRtcProbe()!=expected) return 1;
        if(statusCalls!=1||timeCalls!=reads||resetCalls!=resets) return 2;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('STRUCTURE', structure).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('rtc_probe' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('RTC probe: 1048576 register/read/reset combinations pass, including failed time reads and retained warnings.')


if __name__ == '__main__':
    main()
