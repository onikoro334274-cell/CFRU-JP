"""Execute daily swarm task with trapping shift checks and overwritten live vars."""
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
    source = (ROOT / 'src/wild_encounter.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/wild_encounter.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    config = (ROOT / 'src/config.h').read_text(encoding='utf-8')
    defines = '\n'.join(re.search(r'^#define ' + name + r'\s+[^\n]*', config, re.M)[0]
                        for name in ('VAR_SWARM_INDEX', 'VAR_SWARM_DAILY_EVENT', 'VAR_RAID_PARTNER_RANDOM_NUM'))
    stats = (ROOT / 'include/constants/game_stat.h').read_text(encoding='utf-8')
    defines += '\n' + '\n'.join(re.search(r'^#define ' + name + r'\s+[^\n]*', stats, re.M)[0]
                                  for name in ('GAME_STAT_CAUGHT_TODAY', 'GAME_STAT_EXP_EARNED_TODAY'))
    production = function(source, 'static void Task_UpdateDailyValues(u8 taskId)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
DEFINES
static struct {short data[16];} gTasks[16];
static struct {u16 year;u8 month,day;} gClock;
static u16 vars[2],gSwarmTableLength=1;
static u32 expectedBackup;
static int clocks,reads,updates,sets,days,pokerus,clears,statistics,destroys,errors;
static void DirectClockUpdate(void){clocks++;}
static u16 VarGet(u16 id){reads++;if(id<VAR_SWARM_DAILY_EVENT||id>VAR_SWARM_DAILY_EVENT+1){errors++;return 0;}return vars[id-VAR_SWARM_DAILY_EVENT];}
static bool8 CheckAndSetDailyEvent(u16 id,bool8 write){
    updates++;if(id!=VAR_SWARM_DAILY_EVENT||!write||reads!=2) errors++;
    vars[0]=0xabcd;vars[1]=0x1234;return TRUE;
}
static u16 Random(void){return 42;}
static void VarSet(u16 id,u16 value){
    sets++;
    if(id==VAR_RAID_PARTNER_RANDOM_NUM){if(value!=42) errors++;}
    else if(id==VAR_SWARM_INDEX){
#ifdef SWARM_CHANGE_HOURLY
        if(value!=0xffff) errors++;
#else
        if(value!=0) errors++;
#endif
    }else errors++;
}
static u32 GetDaysSinceTimeInValue(u32 value){days++;if(value!=expectedBackup||updates!=1) errors++;return 17;}
static void UpdatePartyPokerusTime(u32 value){pokerus++;if(value!=17) errors++;}
static void ClearDailyEventFlags(void){clears++;}
static void SetGameStat(u8 id,u32 value){statistics++;if((id!=GAME_STAT_CAUGHT_TODAY&&id!=GAME_STAT_EXP_EARNED_TODAY)||value) errors++;}
static void DestroyTask(u8 id){destroys++;if(id!=3) errors++;}
PRODUCTION
int main(void){
    const u16 lows[]={0,0xffff,0xa5a5};
    for(unsigned high=0;high<65536;high++) for(unsigned low=0;low<3;low++)
    for(int same=0;same<2;same++) for(int tick=0;tick<2;tick++){
        vars[0]=lows[low];vars[1]=high;expectedBackup=high*65536u+lows[low];
        gClock.year=2026;gClock.month=10;gClock.day=11;
        gTasks[3].data[0]=tick?1:2;gTasks[3].data[1]=same?11:10;gTasks[3].data[2]=10;gTasks[3].data[3]=2026;
        clocks=reads=updates=sets=days=pokerus=clears=statistics=destroys=errors=0;
        Task_UpdateDailyValues(3);int process=same&&tick;
        if(clocks!=tick||destroys!=tick||reads!=process*2||updates!=process||sets!=process*2||days!=process||pokerus!=process||clears!=process||statistics!=process*2||errors) return 1;
        if(!process&&(vars[0]!=lows[low]||vars[1]!=high)) return 2;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('swarm_backup' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        for hourly in (False, True):
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                       '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift']
            if hourly:
                command.append('-DSWARM_CHANGE_HOURLY')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                            '/out:' + str(exe), str(obj)], check=True)
            subprocess.run([str(exe)], check=True)
    print('Swarm backup: 1572864 timestamp/date/timer/config cases preserve old bits through live-variable replacement; shift sanitizer passes.')


if __name__ == '__main__':
    main()
