"""Check Eterna clock state and cell updates independently of omitted art."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/sinnoh_eterna_gym.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static u8 EternaClockState(void)', 'static void PlaceClock(bool8 draw)',
        'void SinnohEternaGym_FountainOpened(void)'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define ETERNA_CLOCK_STATES 5
#define ETERNA_CLOCK_CELLS 53
#define ETERNA_MAP_OFFSET 7
#define SINNOH_FLAG_BADGE2 123
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
static const u16 sEternaClockTrainers[]={100,101,102};
static u8 sEternaClockCellXY[53][2];static u16 sEternaClockTiles[5][53];
static int badge,won,expectedState,sets,draws,errors;
static u16 gSpecialVar_LastResult;
static int FlagGet(u16 id) {if(id!=123) errors++;return badge;}
static bool8 HoennHasTrainerBeenFought(u16 id) {return (won>>(id-100))&1;}
static void MapGridSetMetatileIdAt(int x,int y,u16 tile) {
    int i=sets++;if(i>=53) {errors++;return;}
    if(x!=sEternaClockCellXY[i][0]+7 || y!=sEternaClockCellXY[i][1]+7 || tile!=sEternaClockTiles[expectedState][i]) errors++;
}
static void CurrentMapDrawMetatileAt(int x,int y) {
    int i=draws++;if(i>=53) {errors++;return;}
    if(sets!=draws || x!=sEternaClockCellXY[i][0]+7 || y!=sEternaClockCellXY[i][1]+7) errors++;
}
PRODUCTION
int main(void) {
    for(int i=0;i<53;i++) {
        sEternaClockCellXY[i][0]=i%23;sEternaClockCellXY[i][1]=i/23;
        for(int state=0;state<5;state++) sEternaClockTiles[state][i]=state*100+i;
    }
    for(badge=0;badge<2;badge++) for(won=0;won<8;won++) {
        expectedState=badge ? 4 : ((won&1)!=0)+((won&2)!=0)+((won&4)!=0);
        errors=0;
        if(EternaClockState()!=expectedState) return 1;
        gSpecialVar_LastResult=77;SinnohEternaGym_FountainOpened();
        if(gSpecialVar_LastResult!=(expectedState==2 || expectedState==3)) return 2;
        for(int draw=0;draw<2;draw++) {
            sets=draws=0;PlaceClock(draw);
            if(errors || sets!=53 || draws!=(draw ? 53 : 0)) return 3;
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('eterna_clock' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Eterna clock: 16 badge/trainer states and 1696 controlled cell updates passed.')


if __name__ == '__main__':
    main()
