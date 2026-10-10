"""Execute production weather timer boundaries and invalid event recovery."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_abnormal_weather.c').read_text(encoding='utf-8')
    production = source[source.index('#define ABN_LOC'):]
    headers = '\n'.join('#include "' + (ROOT / name).as_posix() + '"' for name in
                        ('src/Tables/hoenn_script_flags.h', 'src/Tables/hoenn_abnormal_weather.h'))
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define NELEMS(a) (sizeof(a)/sizeof((a)[0]))
HEADERS
struct {struct {u8 mapGroup,mapNum;} location;} save,*gSaveBlock1=&save;
u16 vars[65536];u8 flags[65536];u16 randomValue;
u16 VarGet(u16 id) {return vars[id];}
void VarSet(u16 id,u16 value) {vars[id]=value;}
u8 FlagGet(u16 id) {return flags[id];}
u16 Random(void) {return randomValue;}
PRODUCTION
int main(void) {
    flags[HOENN_FLAG_IS_CHAMPION]=1;
    for(int place=0;place<3;place++) for(int moved=0;moved<2;moved++)
    for(int steps=0;steps<65536;steps++) {
        save.location.mapGroup=place==0?0:place==1?sHoennAbnRoutes[4][0]:sHoennAbnCaveMaps[0][0];
        save.location.mapNum=place==0?0:place==1?sHoennAbnRoutes[4][1]:sHoennAbnCaveMaps[0][1];
        vars[ABN_LOC]=5;vars[ABN_END]=0;vars[ABN_STEPS]=steps;
        HoennAbnormalWeatherTick(moved);
        int expired=moved && steps>=999;
        if(vars[ABN_STEPS]!=(expired?0:steps+moved)) return 1;
        if(vars[ABN_LOC]!=(expired && place==0?0:5)) return 2;
        if(vars[ABN_END]!=(expired && place!=0)) return 3;
    }
    for(int loc=17;loc<65536;loc++) {
        vars[ABN_LOC]=loc;vars[ABN_END]=1;vars[ABN_STEPS]=65535;
        HoennAbnormalWeatherTick(0);
        if(vars[ABN_LOC] || vars[ABN_END] || vars[ABN_STEPS]) return 4;
    }
    save.location.mapGroup=0;save.location.mapNum=0;
    for(int loc=1;loc<=16;loc++) for(int place=0;place<3;place++) {
        save.location.mapGroup=place==0?0:place==1?sHoennAbnRoutes[loc-1][0]:sHoennAbnCaveMaps[0][0];
        save.location.mapNum=place==0?0:place==1?sHoennAbnRoutes[loc-1][1]:sHoennAbnCaveMaps[0][1];
        vars[ABN_LOC]=loc;vars[ABN_END]=1;vars[ABN_STEPS]=999;
        HoennAbnormalWeatherTick(1);
        if(vars[ABN_LOC]!=(place==0?0:loc) || vars[ABN_END]!=(place==0?0:1)
           || vars[ABN_STEPS]!=999) return 7;
    }
    for(int map=0;map<25;map++) {
        const u8 *pair=map<16?sHoennAbnRoutes[map]:sHoennAbnCaveMaps[map-16];
        save.location.mapGroup=pair[0];save.location.mapNum=pair[1];
        vars[ABN_LOC]=0;vars[ABN_END]=0;vars[ABN_STEPS]=0;
        HoennAbnormalWeatherTick(1);
        if(vars[ABN_LOC] || vars[ABN_END] || vars[ABN_STEPS]) return 8;
    }
    flags[HOENN_FLAG_IS_CHAMPION]=0;
    vars[ABN_LOC]=65535;vars[ABN_END]=1;vars[ABN_STEPS]=65535;
    HoennAbnormalWeatherTick(1);
    if(vars[ABN_LOC]!=65535 || vars[ABN_END]!=1 || vars[ABN_STEPS]!=65535) return 9;
    flags[HOENN_FLAG_IS_CHAMPION]=1;
    save.location.mapGroup=0;save.location.mapNum=0;
    for(int defeated=0;defeated<4;defeated++) for(int r=0;r<16;r++) {
        flags[HOENN_FLAG_DEFEATED_KYOGRE]=defeated&1;
        flags[HOENN_FLAG_DEFEATED_GROUDON]=(defeated>>1)&1;
        vars[ABN_LOC]=0;vars[ABN_END]=0;vars[ABN_STEPS]=0;randomValue=r;
        HoennAbnormalWeatherTick(0);
        int loc=vars[ABN_LOC];
        if(defeated==3) {if(loc) return 5;}
        else if(loc<1 || loc>16 || (defeated==1 && loc>8) || (defeated==2 && loc<9)) return 6;
    }
    return 0;
}
'''.replace('HEADERS', headers).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('weather' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Weather timers pass 393216 step cases, invalid event recovery, all route/cave creation exclusions, end-event waits and champion gating.')


if __name__ == '__main__':
    main()
