"""Execute production mode changes without resetting daily records on a no-op."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/play_clock.c').read_text(encoding='utf-8')
    production = function(source, 'void PlayClock_SetMode(void)')
    fixture = r'''
typedef unsigned char bool8; typedef unsigned short u16;
#define CLOCK_MODE_BIT 0x8000
#define HOENN_VAR_CLOCK_DAYS 1
#define VAR_SWARM_DAILY_EVENT 2
static u16 Var8004, vars[4]; static int writes, updates, invalid;
static u16 VarGet(u16 id) { if(id!=1) invalid++; return vars[id]; }
static void VarSet(u16 id,u16 value) { if(id<1||id>3) {invalid++;return;} vars[id]=value;writes++; }
static void DirectClockUpdate(void) {updates++;}
PRODUCTION
int main(void) {
    const u16 offsets[]={0,1,1440,32766,32767};
    for(unsigned request=0;request<65536;request++) for(unsigned mode=0;mode<2;mode++)
    for(unsigned i=0;i<5;i++) {
        u16 initial=offsets[i]|(mode?CLOCK_MODE_BIT:0);
        vars[1]=initial;vars[2]=0x1234;vars[3]=0xabcd;Var8004=request;
        writes=updates=invalid=0;PlayClock_SetMode();
        int changed=mode!=(request!=0);
        if(vars[1]!=(offsets[i]|(request?CLOCK_MODE_BIT:0))) return 1;
        if(vars[2]!=(changed?0:0x1234)||vars[3]!=(changed?0:0xabcd)) return 2;
        if(writes!=(changed?3:0)||updates!=changed||invalid) return 3;
        writes=updates=0;PlayClock_SetMode();
        if(writes||updates||invalid) return 4;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('clock_mode' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Clock mode: 655360 transitions and repeated calls passed.')


if __name__ == '__main__':
    main()
